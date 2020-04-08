#include "so_stdio.h"

#define OK 0
#define BUF_SIZE 4096
#define WRITE 1
#define EOF_FLAG 1
#define BASH "/bin/sh"


struct _so_file {
	HANDLE fd;
	unsigned char *buff;
	int bufcnt;
	int bufpoz;
	int bufsize;
	int eof;
	int err;
	int write;
	int pid;
};
static size_t xwrite(SO_FILE *stream)
{
	size_t bytes_written = 0;
	size_t bytes_written_now;
	while (bytes_written < (size_t)stream->bufcnt) {
		WriteFile(stream->fd,
				 stream->buff + bytes_written,
				 stream->bufcnt - bytes_written,
				 &bytes_written_now,
				 NULL);
		if (bytes_written_now <= 0)
			return SO_EOF;
		bytes_written += bytes_written_now;
	}
	return bytes_written;
}

static void file_mode(const char *mode, int *ok,
	int *access, int *sharing, int *existing)
{
	if (strncmp(mode, "r", 1) == 0) {
		*ok = 1;
		*existing = OPEN_EXISTING;
	}

	if (strncmp(mode, "r+", 2) == 0) {
		*ok = 1;
		*existing = OPEN_EXISTING;
	}

	if (strncmp(mode, "w", 1) == 0) {
		*ok = 1;
		*existing = CREATE_ALWAYS;
	}

	if (strncmp(mode, "w+", 2) == 0) {
		*ok = 1;
		*existing = CREATE_ALWAYS;
	}
	*access = GENERIC_READ | GENERIC_WRITE;
	if (strncmp(mode, "a", 1) == 0) {
		*ok = 1;
		*access = FILE_APPEND_DATA;
		*existing = OPEN_ALWAYS;
	}

	if (strncmp(mode, "a+", 2) == 0) {
		*ok = 1;
		*access = FILE_APPEND_DATA | FILE_GENERIC_READ;
		*existing = OPEN_ALWAYS;
	}
	*sharing = FILE_SHARE_READ | FILE_SHARE_WRITE;
}

SO_FILE *so_fopen(const char *pathname, const char *mode)
{
	HANDLE fd;
	int ok = 0;
	int access, sharing, existing;
	SO_FILE *file;

	file = (SO_FILE *) calloc(1, sizeof(SO_FILE));
	file_mode(mode, &ok, &access, &sharing, &existing);

	if (ok == 0) {
		free(file);
		return NULL;
	}
	fd = CreateFile(
				pathname,
				access,
				sharing,
				NULL,
				existing,
				FILE_ATTRIBUTE_NORMAL,
				NULL
				);

	if (fd == INVALID_HANDLE_VALUE) {
		free(file);
		return NULL;
	}

	file->buff = (unsigned char *)calloc(BUF_SIZE, sizeof(unsigned char));

	if (!file->buff) {
		free(file);
		return NULL;
	}
	file->fd = fd;
	file->bufsize = BUF_SIZE;
	file->pid = -1;
	return file;
}

int so_fclose(SO_FILE *stream)
{
	int ret = OK;

	ret = so_fflush(stream);
	if (ret == -1) {
		free(stream->buff);
		free(stream);
		return ret;
	}
	free(stream->buff);
	ret = CloseHandle(stream->fd);
	free(stream);
	return ret == FALSE;
}

int so_fflush(SO_FILE *stream)
{
	int rc = 0;

	if (stream->write && stream->bufcnt) {
		rc = xwrite(stream);
		stream->write = 0;
		if (rc < 0) {
			stream->err = -1;
			return SO_EOF;
		}
	}
	stream->bufpoz = 0;
	stream->bufcnt = 0;
	memset(stream->buff, 0, stream->bufsize);

	return 0;
}

int so_fseek(SO_FILE *stream, long offset, int whence)
{
	if (stream->write) {
		so_fflush(stream);
	} else {
		stream->bufpoz = 0;
		stream->bufcnt = 0;
		memset(stream->buff, 0, stream->bufsize);
	}

	if (SetFilePointer(stream->fd, offset, 0, whence) !=
				INVALID_SET_FILE_POINTER)
		return 0;
	return -1;
}

long so_ftell(SO_FILE *stream)
{
	int file_pos = SetFilePointer(stream->fd,
								0,
								NULL,
								FILE_CURRENT
								);
	if (stream->write)
		return stream->bufcnt + file_pos;
	return file_pos - stream->bufcnt + stream->bufpoz;
}

size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	int bytesToRead = nmemb * size;
	int bytesRead = 0, i;

	for (i = 0; i < bytesToRead; i++) {
		int charRead = so_fgetc(stream);

		if (so_feof(stream) == 0) {
			((char *)ptr)[i] = charRead;
			bytesRead++;
		} else
			break;
	}

	return bytesRead / size;
}

size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{

	int bytesToRead = nmemb * size;
	int i, bytesRead = 0;

	for (i = 0; i < bytesToRead; i++) {
		so_fputc(((char *)ptr)[i], stream);
		bytesRead++;
	}
	return bytesRead / size;
}

int so_fgetc(SO_FILE *stream)
{
	int res, readBytes = 0;

	if (stream->bufpoz - stream->bufcnt >= 0) {
		ReadFile(
					stream->fd,
					stream->buff,
					stream->bufsize,
					&readBytes,
					NULL
					);
		if (readBytes) {
			stream->bufcnt = readBytes;
			stream->bufpoz = 0;
		} else {
			stream->err = -1;
			stream->eof = EOF_FLAG;
			return SO_EOF;
		}
	}
	res = (int)stream->buff[stream->bufpoz];

	stream->bufpoz++;

	return res;
}

int so_fputc(int c, SO_FILE *stream)
{
	int rc = 0;

	stream->bufpoz = 0;

	if (stream->bufsize - stream->bufcnt == 0) {

		rc = xwrite(stream);
		memset(stream->buff, 0, stream->bufsize);
		stream->bufcnt = 0;
		if (rc == 0) {
			stream->eof = EOF_FLAG;
			return SO_EOF;
		} else if (rc < 0) {
			stream->err = -1;
			return SO_EOF;
		}
	}
	stream->buff[stream->bufcnt] = c;
	stream->bufcnt++;
	stream->write = WRITE;
	return c;
}

HANDLE so_fileno(SO_FILE *stream)
{
	return stream->fd;
}

int so_feof(SO_FILE *stream)
{
	if (stream->eof)
		return SO_EOF;
	else
		return OK;
}

int so_ferror(SO_FILE *stream)
{
	if (stream->err)
		return SO_EOF;
	else
		return OK;

}

SO_FILE *so_popen(const char *command, const char *type)
{
	return NULL;
}
int so_pclose(SO_FILE *stream)
{
	return 0;
}
