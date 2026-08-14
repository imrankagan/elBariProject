@echo off
cd /d "%~dp0"
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
echo === MSVC statik analiz (/Wall /analyze) ===
cl /nologo /std:c17 /Wall /analyze /analyze:only /c ^
   /wd4820 /wd4710 /wd4711 /wd5045 /wd4996 ^
   src\elbari.c src\elbari_kanal.c src\elbari_cerceve.c
echo === analiz bitti (exit=%errorlevel%) ===
