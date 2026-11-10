#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$ROOT"
CC=${CC:-cc}
CFLAGS=${CFLAGS:--std=c89 -pedantic -Wall -Wextra -Werror -O2}
OS=$(uname -s)
ARCH=$(uname -m)
case "$OS:$ARCH" in
  Linux:x86_64|Linux:amd64) PLATFORM=linux-x86_64 ;;
  Linux:aarch64|Linux:arm64) PLATFORM=linux-aarch64 ;;
  Linux:armv7l|Linux:armv7*) PLATFORM=linux-armv7 ;;
  Linux:i?86) PLATFORM=linux-x86 ;;
  *) echo "[PIPI] supported targets are Linux x86/aarch64/ARMv7; got $OS/$ARCH" >&2; exit 2 ;;
esac
mkdir -p bin dist dist/woodlibs dist/woodlib-lists dist/woodlib-manifests
printf '[PIPI] v1.0.0 C89 compiler, VM and PIPIwood (target %s)\n' "$PLATFORM"
$CC $CFLAGS bin/pipi.c -o bin/pipi-c
$CC $CFLAGS bin/pipi-vm.c -o bin/pipi-vm -ldl -lm
$CC $CFLAGS bin/pipiwood.c -o bin/pipiwood

# Existing sample third-party ABI-v1 native module.
mkdir -p "modules/pipi/extra/native/$PLATFORM"
$CC $CFLAGS -fPIC -shared -DPIPI_NATIVE_BUILD -Iinclude \
  modules/pipi/extra/pipi_extra.c -o "modules/pipi/extra/native/$PLATFORM/libpipi_extra.so"

# C89 standard-library modules: each module has its own ABI descriptor and binary.
for MOD in net fs time string encoding http system crypto path json random terminal; do
  mkdir -p "modules/pipi/$MOD/native/$PLATFORM"
  cp "lib/pipi/$MOD.pmod" "modules/pipi/$MOD/$MOD.pmod"
  $CC $CFLAGS -fPIC -shared -DPIPI_NATIVE_BUILD -Iinclude \
    "stdlib/pipi_$MOD.c" -o "modules/pipi/$MOD/native/$PLATFORM/libpipi_$MOD.so"
done

# Keep the existing X11 bridge where development headers are present.
X11_HEADER=""
if [ -f /usr/include/X11/Xlib.h ]; then X11_HEADER=/usr/include/X11/Xlib.h; fi
if [ -n "${PREFIX:-}" ] && [ -f "$PREFIX/include/X11/Xlib.h" ]; then X11_HEADER="$PREFIX/include/X11/Xlib.h"; fi
if [ -n "$X11_HEADER" ]; then
  mkdir -p "native/$PLATFORM"
  X11_INCLUDE="$(dirname "$(dirname "$X11_HEADER")")"
  $CC -std=c89 -pedantic -Wall -Wextra -Werror -O2 -fPIC -shared -DPIPI_GUI_BUILD \
    -I"$X11_INCLUDE" -Ilib -Inative/common lib/pipi_gui.c native/linux-x86_64/pipi_gui_backend.c \
    -o "native/$PLATFORM/libpipi_gui.so" -lX11
fi

# Make the third-party sample manifest discoverable at compile time.
mkdir -p modules/acme/greeting
cp examples/woodlib_sample/greeting.pmod modules/acme/greeting/greeting.pmod

# Compile the language, native-module, stdlib and framework examples.
./bin/pipi-c check examples/hello.pipi
./bin/pipi-c compile examples/hello.pipi -o dist/PIPI_Hello.pbc
./bin/pipi-c compile examples/control_flow.pipi -o dist/PIPI_ControlFlow.pbc
./bin/pipi-c compile examples/alias_import.pipi -o dist/PIPI_AliasImport.pbc
./bin/pipi-c compile examples/native_module.pipi -o dist/PIPI_NativeModule.pbc
./bin/pipi-c compile frameworks/PDX/examples/pdx_demo.pipi -o dist/PIPI_PDX_Demo.pbc
./bin/pipi-c compile frameworks/PDX/examples/texture_demo.pipi -o dist/PIPI_PDX_TextureDemo.pbc
./bin/pipi-c compile examples/stdlib_demo.pipi -o dist/PIPI_StdlibDemo.pbc
./bin/pipi-c compile examples/tcp_echo_client.pipi -o dist/PIPI_TcpEchoClient.pbc
./bin/pipi-c compile examples/tcp_echo_server.pipi -o dist/PIPI_TcpEchoServer.pbc
./bin/pipi-c compile examples/http_get.pipi -o dist/PIPI_HttpGet.pbc
./bin/pipi-c compile examples/woodlib_sample/main.pipi -o dist/PIPI_WoodlibSample.pbc
if [ -f "native/$PLATFORM/libpipi_gui.so" ]; then
  ./bin/pipi-c compile examples/gui_app.pipi -o dist/PIPI_GUI.pbc
fi

# Pure built-in modules are also distributable .woodlib packages.
for MOD in core math; do
  PKG="pipi/$MOD"; ID="pipi_$MOD"; META="dist/woodlib-manifests/$ID"; LIST="dist/woodlib-lists/$ID.lst"
  mkdir -p "$META"
  printf 'WOODLIB 1\nname %s\nversion 1.0.0\n' "$PKG" > "$META/manifest"
  printf '.woodlib/manifest %s/manifest\nmodules/pipi/%s/%s.pmod\n' "$META" "$MOD" "$MOD" > "$LIST"
  ./bin/pipiwood pack "$LIST" -o "dist/woodlibs/$ID.woodlib"
  ./bin/pipiwood verify "dist/woodlibs/$ID.woodlib"
done

# The example third-party native plugin is available as its own installable package.
PKG="pipi/extra"; ID="pipi_extra"; META="dist/woodlib-manifests/$ID"; LIST="dist/woodlib-lists/$ID.lst"
mkdir -p "$META"
printf 'WOODLIB 1\nname %s\nversion 1.0.0\n' "$PKG" > "$META/manifest"
printf '.woodlib/manifest %s/manifest\nmodules/pipi/extra/extra.pmod\nmodules/pipi/extra/native/%s/libpipi_extra.so\n' "$META" "$PLATFORM" > "$LIST"
./bin/pipiwood pack "$LIST" -o "dist/woodlibs/$ID.woodlib"
./bin/pipiwood verify "dist/woodlibs/$ID.woodlib"

# Native .woodlib packages contain their manifest and host-platform binary.
for MOD in net fs time string encoding http system crypto path json random terminal; do
  PKG="pipi/$MOD"; ID="pipi_$MOD"; META="dist/woodlib-manifests/$ID"; LIST="dist/woodlib-lists/$ID.lst"
  mkdir -p "$META"
  printf 'WOODLIB 1\nname %s\nversion 1.0.0\n' "$PKG" > "$META/manifest"
  printf '.woodlib/manifest %s/manifest\nmodules/pipi/%s/%s.pmod\nmodules/pipi/%s/native/%s/libpipi_%s.so\n' \
    "$META" "$MOD" "$MOD" "$MOD" "$PLATFORM" "$MOD" > "$LIST"
  ./bin/pipiwood pack "$LIST" -o "dist/woodlibs/$ID.woodlib"
  ./bin/pipiwood verify "dist/woodlibs/$ID.woodlib"
done

# Runtime smoke tests run with the same C89 VM that users receive.
./bin/pipi-vm dist/PIPI_Hello.pbc
./bin/pipi-vm dist/PIPI_NativeModule.pbc
./bin/pipi-vm dist/PIPI_StdlibDemo.pbc
# Build and package a fully external third-party module without changing compiler/VM sources.
mkdir -p "modules/acme/greeting/native/$PLATFORM"
cp examples/woodlib_sample/greeting.pmod modules/acme/greeting/greeting.pmod
$CC $CFLAGS -fPIC -shared -DPIPI_NATIVE_BUILD -Iinclude \
  examples/woodlib_sample/greeting.c -o "modules/acme/greeting/native/$PLATFORM/libpipi_greeting.so"
PKG="acme/greeting"; ID="acme_greeting"; META="dist/woodlib-manifests/$ID"; LIST="dist/woodlib-lists/$ID.lst"
mkdir -p "$META"
printf 'WOODLIB 1\nname %s\nversion 1.0.0\n' "$PKG" > "$META/manifest"
printf '.woodlib/manifest %s/manifest\nmodules/acme/greeting/greeting.pmod\nmodules/acme/greeting/native/%s/libpipi_greeting.so\nsources/acme/greeting.c examples/woodlib_sample/greeting.c\n' "$META" "$PLATFORM" > "$LIST"
./bin/pipiwood pack "$LIST" -o "dist/woodlibs/$ID.woodlib"
./bin/pipiwood verify "dist/woodlibs/$ID.woodlib"

# Prove that an installed third-party package is discoverable by both compiler and VM.
WOOD_HOME=$(mktemp -d "${TMPDIR:-/tmp}/pipiwood-home.XXXXXX")
./bin/pipiwood install "dist/woodlibs/$ID.woodlib" --root "$WOOD_HOME"
PIPI_HOME="$WOOD_HOME" ./bin/pipi-vm dist/PIPI_WoodlibSample.pbc
./bin/pipiwood remove "$PKG" --root "$WOOD_HOME"
rm -rf "$WOOD_HOME"

printf '[PIPI] v1.0.0 build complete. platform=%s; 16 .woodlib packages in dist/woodlibs/\n' "$PLATFORM"
