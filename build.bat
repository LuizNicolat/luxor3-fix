@echo off
cd /d "%~dp0"
rem Compila o D3D8.dll (32 bits) usando o Zig como compilador C.
rem Requisitos: Python 3 e "python -m pip install ziglang"
python -m ziglang cc -target x86-windows-gnu -O2 -shared -o dist\D3D8.dll src\luxorfix.c src\d3d8.def -luser32
if errorlevel 1 exit /b 1
echo Gerado: dist\D3D8.dll
