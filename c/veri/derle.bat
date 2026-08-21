@echo off
rem =====================================================================
rem ArduPilot DataFlash log okuyucusu ve donusturucu (Windows / MSVC)
rem =====================================================================
cd /d "%~dp0"
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

set BAYRAK=/nologo /std:c17 /W4 /O2

echo Derleniyor: donustur.exe
cl %BAYRAK% /Fe:donustur.exe donustur.c dataflash.c >nul || goto :hata

echo Derleme basarili.
exit /b 0

:hata
echo DERLEME HATASI!
exit /b 1
