@echo off
rem Betigin bulundugu dizine gec (nereden cagrilirsa cagrilsin calissin)
cd /d "%~dp0"
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
echo [1/2] dogrulama.exe
cl /nologo /std:c17 /W4 /O2 /Fe:dogrulama.exe test\dogrulama.c src\elbari.c src\elbari_kanal.c src\elbari_cerceve.c >nul
echo [2/2] olcum.exe
cl /nologo /std:c17 /W4 /O2 /Fe:olcum.exe test\olcum.c src\elbari.c src\elbari_kanal.c src\elbari_cerceve.c >nul
echo Derleme bitti (exit=%errorlevel%)
