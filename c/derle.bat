@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
cl /nologo /std:c17 /W4 /O2 /Fe:dogrulama.exe test\dogrulama.c src\elbari.c src\elbari_kanal.c src\elbari_cerceve.c
