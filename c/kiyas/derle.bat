@echo off
rem =====================================================================
rem KIYAS - tamsayi kodek ailesi karsilastirmasi (Windows / MSVC)
rem ---------------------------------------------------------------------
rem ONEMLI: ElBari ve rakipler AYNI derleyici ve AYNI bayraklarla
rem derlenir. Karsilastirmanin adil olmasi buna baglidir.
rem =====================================================================
cd /d "%~dp0"
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

set ELBARI=..\src\elbari.c ..\src\elbari_kanal.c ..\src\elbari_cerceve.c ..\src\elbari_float.c ..\src\elbari_float_xor.c
set RAKIP=onisleme.c kodekler.c sprintz.c
set BAYRAK=/nologo /std:c17 /W4 /O2

echo [1/2] kiyas.exe
cl %BAYRAK% /Fe:kiyas.exe kiyas.c %RAKIP% %ELBARI% >nul || goto :hata
echo [2/2] oztest.exe
cl %BAYRAK% /Fe:oztest.exe oztest.c %RAKIP% >nul || goto :hata

echo Derleme basarili.
exit /b 0

:hata
echo DERLEME HATASI!
exit /b 1
