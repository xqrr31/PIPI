@echo off
setlocal
cd /d "%~dp0\..\.."
if "%CC%"=="" set CC=gcc
if not exist native\windows-x86_64 mkdir native\windows-x86_64
%CC% -std=c89 -Wall -Wextra -Werror -O2 -shared -DPIPI_GUI_BUILD -Ilib -Inative\common lib\pipi_gui.c native\windows-x86_64\pipi_gui_backend.c -o native\windows-x86_64\pipi_gui.dll -luser32 -lgdi32
if errorlevel 1 exit /b 1
echo [PIPI] native Windows GUI DLL built without JNI.
