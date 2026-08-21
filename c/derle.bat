@echo off
rem Betigin bulundugu dizine gec (nereden cagrilirsa cagrilsin calissin)
cd /d "%~dp0"
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
set KAYNAK=src\elbari.c src\elbari_kanal.c src\elbari_cerceve.c src\elbari_float.c src\elbari_float_xor.c
set BAYRAK=/nologo /std:c17 /W4 /O2

echo [1/6] dogrulama.exe
cl %BAYRAK% /Fe:dogrulama.exe test\dogrulama.c %KAYNAK% >nul || goto :hata
echo [2/6] olcum.exe
cl %BAYRAK% /Fe:olcum.exe test\olcum.c %KAYNAK% >nul || goto :hata
echo [3/6] fuzz.exe
cl %BAYRAK% /Fe:fuzz.exe test\fuzz.c %KAYNAK% >nul || goto :hata
echo [4/6] uygunluk.exe
cl %BAYRAK% /Fe:uygunluk.exe test\uygunluk.c %KAYNAK% >nul || goto :hata

echo [5/6] kapsamli.exe
cl %BAYRAK% /Fe:kapsamli.exe test\kapsamli.c %KAYNAK% >nul || goto :hata
echo [6/6] deger_fuzz.exe
cl %BAYRAK% /Fe:deger_fuzz.exe test\deger_fuzz.c %KAYNAK% >nul || goto :hata

echo Derleme basarili.
exit /b 0

:hata
echo DERLEME HATASI!
exit /b 1
