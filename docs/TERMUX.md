# Build and install PIPI on Termux (Android ARM64)

The source release is portable, but this download's prebuilt Linux executables and `.woodlib` native binaries are **x86-64**. Do not try to execute/install those on the typical ARM64 Android phone. Build them on the device so the `.so` files match Android/Termux ARM64.

## 1. Build locally

```sh
pkg update
pkg install clang unzip
unzip PIPI_General_Language_C89_v1.0.zip
cd PIPI_General_Language_v1.0
CC=clang ./build.sh
```

`build.sh` detects `aarch64` and writes native modules under `native/linux-aarch64/` and `modules/**/native/linux-aarch64/`. It then builds ARM64 `.woodlib` packages in `dist/woodlibs/`.

## 2. Put PIPI tools on PATH

```sh
install -m 755 bin/pipi-c bin/pipi-vm bin/pipiwood "$PREFIX/bin/"
hash -r
pipi-c --version
pipi-vm --version
pipiwood --version
```

## 3. Install the standard libraries

Install every generated standard package into `$HOME/.pipi`:

```sh
for package in dist/woodlibs/pipi_*.woodlib; do
  pipiwood install "$package"
done
```

The compiler and VM automatically search `$HOME/.pipi`; no `PIPI_MODULE_PATH` export is required. Packages contain native ARM64 shared libraries created by the local build.

## 4. Compile and run an app

```sh
pipi-c compile examples/stdlib_demo.pipi -o dist/my-demo.pbc
pipi-vm dist/my-demo.pbc
```

A small socket app can import `pipi/net`:

```ts
import { print } from "pipi/core";
import { socketCreate, socketConnect, socketSend, socketRecv, socketClose } from "pipi/net";

let sock = socketCreate("tcp");
if (socketConnect(sock, "127.0.0.1", 7000)) {
  socketSend(sock, "hello from Termux");
  print(socketRecv(sock, 1024));
}
socketClose(sock);
```

Start the sample echo server on another shell with `pipi-vm dist/PIPI_TcpEchoServer.pbc`, then run the client program. The sample server listens on port 7000.

## Android notes

- `pipi/net` and `pipi/http` use Android's POSIX socket resolver and are built locally as `.so` libraries.
- `pipi/http` is plain HTTP only; no HTTPS/TLS.
- The X11 GUI bridge is built only when X11 development headers are installed. PDX is a Windows/Direct3D 9 framework and is not available on Android.
- If the build fails because of an optional GUI header, install the relevant Termux X11 development package or build the core/runtime modules without that optional bridge; socket, filesystem, JSON and other standard modules do not depend on X11.
