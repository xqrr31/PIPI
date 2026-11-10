@echo off
setlocal EnableExtensions
cd /d "%~dp0"
if "%CC%"=="" set CC=gcc
if "%CXX%"=="" set CXX=g++
if not exist dist mkdir dist
if not exist dist\woodlibs mkdir dist\woodlibs
if not exist dist\woodlib-lists mkdir dist\woodlib-lists
if not exist dist\woodlib-manifests mkdir dist\woodlib-manifests
if not exist bin mkdir bin

echo [PIPI] v1.0.0 - C89 compiler, own VM, pipiwood and native standard libraries
%CC% -std=c89 -pedantic -Wall -Wextra -Werror -O2 bin\pipi.c -o bin\pipi-c.exe
if errorlevel 1 goto :fail
%CC% -std=c89 -pedantic -Wall -Wextra -Werror -O2 bin\pipi-vm.c -o bin\pipi-vm.exe -lm
if errorlevel 1 goto :fail
%CC% -std=c89 -pedantic -Wall -Wextra -Werror -O2 bin\pipiwood.c -o bin\pipiwood.exe
if errorlevel 1 goto :fail

rem Build the existing extra example plugin.
if not exist modules\pipi\extra\native\windows-x86_64 mkdir modules\pipi\extra\native\windows-x86_64
%CC% -std=c89 -pedantic -Wall -Wextra -Werror -O2 -shared -DPIPI_NATIVE_BUILD -Iinclude modules\pipi\extra\pipi_extra.c -o modules\pipi\extra\native\windows-x86_64\pipi_extra.dll
if errorlevel 1 goto :fail

rem Build all standard native modules with the stable C ABI.
for %%M in (net fs time string encoding http system crypto path json random terminal) do if not exist modules\pipi\%%M\native\windows-x86_64 mkdir modules\pipi\%%M\native\windows-x86_64
%CC% -std=c89 -pedantic -Wall -Wextra -Werror -O2 -shared -DPIPI_NATIVE_BUILD -Iinclude stdlib\pipi_net.c -o modules\pipi\net\native\windows-x86_64\pipi_net.dll -lws2_32
if errorlevel 1 goto :fail
%CC% -std=c89 -pedantic -Wall -Wextra -Werror -O2 -shared -DPIPI_NATIVE_BUILD -Iinclude stdlib\pipi_fs.c -o modules\pipi\fs\native\windows-x86_64\pipi_fs.dll
if errorlevel 1 goto :fail
%CC% -std=c89 -pedantic -Wall -Wextra -Werror -O2 -shared -DPIPI_NATIVE_BUILD -Iinclude stdlib\pipi_time.c -o modules\pipi\time\native\windows-x86_64\pipi_time.dll
if errorlevel 1 goto :fail
%CC% -std=c89 -pedantic -Wall -Wextra -Werror -O2 -shared -DPIPI_NATIVE_BUILD -Iinclude stdlib\pipi_string.c -o modules\pipi\string\native\windows-x86_64\pipi_string.dll
if errorlevel 1 goto :fail
%CC% -std=c89 -pedantic -Wall -Wextra -Werror -O2 -shared -DPIPI_NATIVE_BUILD -Iinclude stdlib\pipi_encoding.c -o modules\pipi\encoding\native\windows-x86_64\pipi_encoding.dll
if errorlevel 1 goto :fail
%CC% -std=c89 -pedantic -Wall -Wextra -Werror -O2 -shared -DPIPI_NATIVE_BUILD -Iinclude stdlib\pipi_http.c -o modules\pipi\http\native\windows-x86_64\pipi_http.dll -lws2_32
if errorlevel 1 goto :fail
%CC% -std=c89 -pedantic -Wall -Wextra -Werror -O2 -shared -DPIPI_NATIVE_BUILD -Iinclude stdlib\pipi_system.c -o modules\pipi\system\native\windows-x86_64\pipi_system.dll
if errorlevel 1 goto :fail
%CC% -std=c89 -pedantic -Wall -Wextra -Werror -O2 -shared -DPIPI_NATIVE_BUILD -Iinclude stdlib\pipi_crypto.c -o modules\pipi\crypto\native\windows-x86_64\pipi_crypto.dll
if errorlevel 1 goto :fail
%CC% -std=c89 -pedantic -Wall -Wextra -Werror -O2 -shared -DPIPI_NATIVE_BUILD -Iinclude stdlib\pipi_path.c -o modules\pipi\path\native\windows-x86_64\pipi_path.dll
if errorlevel 1 goto :fail
%CC% -std=c89 -pedantic -Wall -Wextra -Werror -O2 -shared -DPIPI_NATIVE_BUILD -Iinclude stdlib\pipi_json.c -o modules\pipi\json\native\windows-x86_64\pipi_json.dll
if errorlevel 1 goto :fail
%CC% -std=c89 -pedantic -Wall -Wextra -Werror -O2 -shared -DPIPI_NATIVE_BUILD -Iinclude stdlib\pipi_random.c -o modules\pipi\random\native\windows-x86_64\pipi_random.dll
if errorlevel 1 goto :fail
%CC% -std=c89 -pedantic -Wall -Wextra -Werror -O2 -shared -DPIPI_NATIVE_BUILD -Iinclude stdlib\pipi_terminal.c -o modules\pipi\terminal\native\windows-x86_64\pipi_terminal.dll
if errorlevel 1 goto :fail

rem Legacy GUI API (kept separate from the standard native modules).
if not exist native\windows-x86_64 mkdir native\windows-x86_64
%CC% -std=c89 -Wall -Wextra -Werror -O2 -shared -DPIPI_GUI_BUILD -Ilib -Inative\common lib\pipi_gui.c native\windows-x86_64\pipi_gui_backend.c -o native\windows-x86_64\pipi_gui.dll -luser32 -lgdi32
if errorlevel 1 goto :fail

rem Compile PIPI programs.
bin\pipi-c.exe check examples\hello.pipi
if errorlevel 1 goto :fail
bin\pipi-c.exe compile examples\hello.pipi -o dist\PIPI_Hello.pbc
if errorlevel 1 goto :fail
bin\pipi-c.exe compile examples\control_flow.pipi -o dist\PIPI_ControlFlow.pbc
if errorlevel 1 goto :fail
bin\pipi-c.exe compile examples\alias_import.pipi -o dist\PIPI_AliasImport.pbc
if errorlevel 1 goto :fail
bin\pipi-c.exe compile examples\native_module.pipi -o dist\PIPI_NativeModule.pbc
if errorlevel 1 goto :fail
bin\pipi-c.exe compile frameworks\PDX\examples\pdx_demo.pipi -o dist\PIPI_PDX_Demo.pbc
if errorlevel 1 goto :fail
bin\pipi-c.exe compile frameworks\PDX\examples\texture_demo.pipi -o dist\PIPI_PDX_TextureDemo.pbc
if errorlevel 1 goto :fail
bin\pipi-c.exe compile examples\stdlib_demo.pipi -o dist\PIPI_StdlibDemo.pbc
if errorlevel 1 goto :fail
bin\pipi-c.exe compile examples\tcp_echo_client.pipi -o dist\PIPI_TcpEchoClient.pbc
if errorlevel 1 goto :fail
bin\pipi-c.exe compile examples\tcp_echo_server.pipi -o dist\PIPI_TcpEchoServer.pbc
if errorlevel 1 goto :fail
bin\pipi-c.exe compile examples\http_get.pipi -o dist\PIPI_HttpGet.pbc
if errorlevel 1 goto :fail
if not exist modules\acme\greeting\native\windows-x86_64 mkdir modules\acme\greeting\native\windows-x86_64
copy /Y examples\woodlib_sample\greeting.pmod modules\acme\greeting\greeting.pmod >nul
%CC% -std=c89 -pedantic -Wall -Wextra -Werror -O2 -shared -DPIPI_NATIVE_BUILD -Iinclude examples\woodlib_sample\greeting.c -o modules\acme\greeting\native\windows-x86_64\pipi_greeting.dll
if errorlevel 1 goto :fail
bin\pipi-c.exe compile examples\woodlib_sample\main.pipi -o dist\PIPI_WoodlibSample.pbc
if errorlevel 1 goto :fail

rem Build the DirectX 9 framework module, then package its DLL.
call frameworks\PDX\build.bat
if errorlevel 1 goto :fail

rem Package the portable built-in core and math declarations.
call :package_builtin core
if errorlevel 1 goto :fail
call :package_builtin math
if errorlevel 1 goto :fail

rem Package the sample external module.
call :package_native extra
if errorlevel 1 goto :fail

rem Package all native standard libraries.
for %%M in (net fs time string encoding http system crypto path json random terminal) do (
  call :package_native %%M
  if errorlevel 1 goto :fail
)

rem Package a third-party ABI-v1 example to prove independent module development.
if not exist dist\woodlib-manifests\acme_greeting mkdir dist\woodlib-manifests\acme_greeting
> dist\woodlib-manifests\acme_greeting\manifest echo WOODLIB 1
>>dist\woodlib-manifests\acme_greeting\manifest echo name acme/greeting
>>dist\woodlib-manifests\acme_greeting\manifest echo version 1.0.0
> dist\woodlib-lists\acme_greeting.lst echo .woodlib/manifest dist\woodlib-manifests\acme_greeting\manifest
>>dist\woodlib-lists\acme_greeting.lst echo modules/acme/greeting/greeting.pmod
>>dist\woodlib-lists\acme_greeting.lst echo modules/acme/greeting/native/windows-x86_64/pipi_greeting.dll
>>dist\woodlib-lists\acme_greeting.lst echo sources/acme/greeting.c examples/woodlib_sample/greeting.c
bin\pipiwood.exe pack dist\woodlib-lists\acme_greeting.lst -o dist\woodlibs\acme_greeting.woodlib
if errorlevel 1 goto :fail
bin\pipiwood.exe verify dist\woodlibs\acme_greeting.woodlib
if errorlevel 1 goto :fail

rem Package PDX separately as a native framework.
if not exist dist\woodlib-manifests\pdx mkdir dist\woodlib-manifests\pdx
> dist\woodlib-manifests\pdx\manifest echo WOODLIB 1
>>dist\woodlib-manifests\pdx\manifest echo name pdx
>>dist\woodlib-manifests\pdx\manifest echo version 0.8.0
> dist\woodlib-lists\pdx.lst echo .woodlib/manifest dist\woodlib-manifests\pdx\manifest
>>dist\woodlib-lists\pdx.lst echo modules/pdx/pdx.pmod
>>dist\woodlib-lists\pdx.lst echo modules/pdx/native/windows-x86_64/pipi_pdx.dll
bin\pipiwood.exe pack dist\woodlib-lists\pdx.lst -o dist\woodlibs\pdx.woodlib
if errorlevel 1 goto :fail
bin\pipiwood.exe verify dist\woodlibs\pdx.woodlib
if errorlevel 1 goto :fail

echo [PIPI] smoke tests
bin\pipi-vm.exe dist\PIPI_Hello.pbc
if errorlevel 1 goto :fail
bin\pipi-vm.exe dist\PIPI_NativeModule.pbc
if errorlevel 1 goto :fail
bin\pipi-vm.exe dist\PIPI_StdlibDemo.pbc
if errorlevel 1 goto :fail
bin\pipiwood.exe install dist\woodlibs\acme_greeting.woodlib --root dist\woodlib-test
if errorlevel 1 goto :fail
set PIPI_HOME=%CD%\dist\woodlib-test
bin\pipi-vm.exe dist\PIPI_WoodlibSample.pbc
if errorlevel 1 goto :fail
set PIPI_HOME=
echo [PIPI] v1.0.0 build complete. WOODLIB packages are in dist\woodlibs\
exit /b 0

:package_builtin
set MOD=%1
if not exist dist\woodlib-manifests\pipi_%MOD% mkdir dist\woodlib-manifests\pipi_%MOD%
> dist\woodlib-manifests\pipi_%MOD%\manifest echo WOODLIB 1
>>dist\woodlib-manifests\pipi_%MOD%\manifest echo name pipi/%MOD%
>>dist\woodlib-manifests\pipi_%MOD%\manifest echo version 1.0.0
> dist\woodlib-lists\pipi_%MOD%.lst echo .woodlib/manifest dist\woodlib-manifests\pipi_%MOD%\manifest
>>dist\woodlib-lists\pipi_%MOD%.lst echo modules/pipi/%MOD%/%MOD%.pmod
bin\pipiwood.exe pack dist\woodlib-lists\pipi_%MOD%.lst -o dist\woodlibs\pipi_%MOD%.woodlib
if errorlevel 1 exit /b 1
bin\pipiwood.exe verify dist\woodlibs\pipi_%MOD%.woodlib
exit /b %errorlevel%

:package_native
set MOD=%1
if not exist dist\woodlib-manifests\pipi_%MOD% mkdir dist\woodlib-manifests\pipi_%MOD%
> dist\woodlib-manifests\pipi_%MOD%\manifest echo WOODLIB 1
>>dist\woodlib-manifests\pipi_%MOD%\manifest echo name pipi/%MOD%
>>dist\woodlib-manifests\pipi_%MOD%\manifest echo version 1.0.0
> dist\woodlib-lists\pipi_%MOD%.lst echo .woodlib/manifest dist\woodlib-manifests\pipi_%MOD%\manifest
>>dist\woodlib-lists\pipi_%MOD%.lst echo modules/pipi/%MOD%/%MOD%.pmod
>>dist\woodlib-lists\pipi_%MOD%.lst echo modules/pipi/%MOD%/native/windows-x86_64/pipi_%MOD%.dll
bin\pipiwood.exe pack dist\woodlib-lists\pipi_%MOD%.lst -o dist\woodlibs\pipi_%MOD%.woodlib
if errorlevel 1 exit /b 1
bin\pipiwood.exe verify dist\woodlibs\pipi_%MOD%.woodlib
exit /b %errorlevel%

:fail
echo [PIPI] BUILD FAILED (errorlevel %errorlevel%)
exit /b 1
