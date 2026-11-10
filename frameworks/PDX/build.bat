@echo off
setlocal
cd /d "%~dp0\..\.."
if "%CXX%"=="" set CXX=g++
if not exist modules\pdx\native\windows-x86_64 mkdir modules\pdx\native\windows-x86_64
echo [PDX] Building PDX 0.8.0 Direct3D 9 game module; PIPI core remains unchanged.
%CXX% -std=c++98 -Wall -Wextra -O2 -shared -DPIPI_NATIVE_BUILD -Iinclude frameworks\PDX\pdx_native.cpp -o modules\pdx\native\windows-x86_64\pipi_pdx.dll -ld3d9 -luser32 -lgdi32 -lwinmm -static-libgcc -static-libstdc++
if errorlevel 1 goto :fail
echo [PDX] Built modules\pdx\native\windows-x86_64\pipi_pdx.dll (PDX 0.8.0)
exit /b 0
:fail
echo [PDX] Build failed. Check that g++ and d3d9.h / libd3d9 are installed.
exit /b 1
