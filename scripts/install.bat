@echo off
setlocal EnableExtensions DisableDelayedExpansion
cd /d "%~dp0\.."
set "VERSION=unknown"
if exist PIPI_VERSION set /p VERSION=<PIPI_VERSION

echo.
echo PIPI v%VERSION% - choose the OS for this installation
echo   1^) Windows x64 - install on this Windows machine
echo   2^) Linux       - run ./install.sh on Linux
echo   3^) Termux      - run bash install.sh inside Termux
set /p "CHOICE=Select [1-3]: "
if "%CHOICE%"=="2" (
  echo Run ./install.sh on the Linux machine itself. Cross-OS binaries cannot be installed into Linux PATH from Windows.
  exit /b 0
)
if "%CHOICE%"=="3" (
  echo Copy this source folder to Termux, then run: bash install.sh
  exit /b 0
)
if not "%CHOICE%"=="1" (
  echo Invalid selection.
  exit /b 2
)

where gcc >nul 2>nul
if errorlevel 1 (
  echo [PIPI] MinGW-w64 GCC was not found. Install MinGW-w64 and add gcc/g++ to PATH.
  exit /b 1
)
where g++ >nul 2>nul
if errorlevel 1 (
  echo [PIPI] MinGW-w64 G++ was not found. Install MinGW-w64 and add gcc/g++ to PATH.
  exit /b 1
)

set "TOOL_HOME=%LOCALAPPDATA%\PIPI"
if "%LOCALAPPDATA%"=="" set "TOOL_HOME=%USERPROFILE%\.local\PIPI"
set "TOOL_BIN=%TOOL_HOME%\bin"
if "%PIPI_HOME%"=="" set "PIPI_HOME=%USERPROFILE%\.pipi"
if not exist "%TOOL_BIN%" mkdir "%TOOL_BIN%"
if errorlevel 1 exit /b 1

echo.
echo [PIPI] Building native Windows tools and modules...
call build.bat
if errorlevel 1 goto :failed

for %%T in (pipi-c.exe pipi-vm.exe pipiwood.exe) do (
  if not exist "bin\%%T" (
    echo [PIPI] Missing bin\%%T after build.
    goto :failed
  )
  copy /Y "bin\%%T" "%TOOL_BIN%\%%T" >nul
  if errorlevel 1 goto :failed
)

set "GUI_LIBRARY="
if exist "native\windows-x86_64\pipi_gui.dll" (
  if not exist "%TOOL_HOME%\native\windows-x86_64" mkdir "%TOOL_HOME%\native\windows-x86_64"
  copy /Y "native\windows-x86_64\pipi_gui.dll" "%TOOL_HOME%\native\windows-x86_64\pipi_gui.dll" >nul
  if errorlevel 1 goto :failed
  set "GUI_LIBRARY=%TOOL_HOME%\native\windows-x86_64\pipi_gui.dll"
)

echo.
echo [PIPI] Installing standard libraries into "%PIPI_HOME%" without overwriting existing module folders...
if not exist "%PIPI_HOME%" mkdir "%PIPI_HOME%"
for %%F in (dist\woodlibs\pipi_*.woodlib) do if exist "%%F" call :install_package "%%F"
if errorlevel 1 goto :failed
if exist "dist\woodlibs\pdx.woodlib" call :install_pdx
if errorlevel 1 goto :failed

echo.
echo [PIPI] Adding the private tool directory to the current-user PATH (no administrator rights)...
if not "%GUI_LIBRARY%"=="" (
  powershell -NoProfile -ExecutionPolicy Bypass -File "scripts\add-user-path.ps1" -Directory "%TOOL_BIN%" -GuiLibrary "%GUI_LIBRARY%"
) else (
  powershell -NoProfile -ExecutionPolicy Bypass -File "scripts\add-user-path.ps1" -Directory "%TOOL_BIN%"
)
if errorlevel 1 (
  echo [PIPI] Could not update User PATH automatically. Add this directory manually:
  echo        %TOOL_BIN%
) else (
  set "PATH=%PATH%;%TOOL_BIN%"
  if not "%GUI_LIBRARY%"=="" if "%PIPI_GUI_LIBRARY%"=="" set "PIPI_GUI_LIBRARY=%GUI_LIBRARY%"
)

"%TOOL_BIN%\pipi-c.exe" --version
"%TOOL_BIN%\pipi-vm.exe" --version
"%TOOL_BIN%\pipiwood.exe" --version
echo.
echo [PIPI] Installation complete.
echo   Tools:   %TOOL_BIN%
echo   Modules: %PIPI_HOME%
echo Existing module folders were not removed or overwritten.
echo Open a new terminal to use pipi-c, pipi-vm and pipiwood.
exit /b 0

:install_package
set "ARCHIVE=%~1"
for %%A in ("%ARCHIVE%") do set "ID=%%~nA"
set "MOD=%ID:pipi_=%"
if exist "%PIPI_HOME%\.installed\%ID%.files" (
  echo [PIPI] Preserving installed package %ID%.
  exit /b 0
)
if exist "%PIPI_HOME%\modules\pipi\%MOD%" (
  echo [PIPI] Preserving existing module folder pipi/%MOD%.
  exit /b 0
)
"%TOOL_BIN%\pipiwood.exe" verify "%ARCHIVE%" >nul
if errorlevel 1 exit /b 1
"%TOOL_BIN%\pipiwood.exe" install "%ARCHIVE%" --root "%PIPI_HOME%"
if errorlevel 1 exit /b 1
exit /b 0

:install_pdx
if exist "%PIPI_HOME%\.installed\pdx.files" (
  echo [PIPI] Preserving installed package pdx.
  exit /b 0
)
if exist "%PIPI_HOME%\modules\pdx" (
  echo [PIPI] Preserving existing module folder pdx.
  exit /b 0
)
"%TOOL_BIN%\pipiwood.exe" verify "dist\woodlibs\pdx.woodlib" >nul
if errorlevel 1 exit /b 1
"%TOOL_BIN%\pipiwood.exe" install "dist\woodlibs\pdx.woodlib" --root "%PIPI_HOME%"
if errorlevel 1 exit /b 1
exit /b 0

:failed
echo [PIPI] Installation failed. Existing modules and user PATH entries were left untouched.
exit /b 1
