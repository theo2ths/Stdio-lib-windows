build: so_stdio.dll

so_stdio.dll: so_stdio.obj
	link /nologo /dll /out:so_stdio.dll /implib:so_stdio.lib so_stdio.obj

so_stdio.obj: so_stdio.c
	cl /D_CRT_SECURE_NO_DEPRECATE /W3 /nologo /MD /DDLL_EXPORTS /c so_stdio.c

.PHONY: clean

clean:
	del *.obj *.dll *.lib *.exe *.exp