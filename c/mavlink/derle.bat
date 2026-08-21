@echo off
rem =====================================================================
rem Iki kademeli MAVLink vekili - olcum (Windows / MSVC)
rem =====================================================================
cd /d "%~dp0"
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

set ELBARI=..\src\elbari.c ..\src\elbari_kanal.c ..\src\elbari_cerceve.c ..\src\elbari_float.c ..\src\elbari_float_xor.c
set VEKIL=mav_bicim.c mav_sema.c mav_kademe.c mav_uretici.c
set BAYRAK=/nologo /std:c17 /W4 /O2

echo Derleniyor: mav_olcum.exe
cl %BAYRAK% /Fe:mav_olcum.exe mav_olcum.c %VEKIL% %ELBARI% >nul || goto :hata

echo Derleme basarili.
exit /b 0

:hata
echo DERLEME HATASI!
exit /b 1
