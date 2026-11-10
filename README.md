# PIPI General Language — v1.0.0

**P**layful · **I**ntense · **P**owerful · **I**nsane

PIPI is a general-purpose language with a C89 compiler, custom `PIPIBC06` bytecode, a C89 VM and Native Module ABI v1. Native standard-library modules are written in C89 and can be distributed independently as `.woodlib` packages through **PIPIwood**.

## v1.0.0 highlights

- Added a C89 TCP/UDP socket module (`pipi/net`) for client/server networking.
- Added plain HTTP/1.0 GET helper (`pipi/http`), filesystem (`pipi/fs`), time (`pipi/time`), string (`pipi/string`), URL/hex encoding (`pipi/encoding`), system information (`pipi/system`), SHA-256/CRC32 (`pipi/crypto`), path helpers (`pipi/path`), JSON scalar getters/escaping (`pipi/json`), pseudo-random functions (`pipi/random`) and terminal I/O (`pipi/terminal`).
- Added `pipiwood`, a standalone C89 `.woodlib` packer, verifier, installer and remover.
- `.woodlib` uses a simple uncompressed `WOODLIB1` container with per-file CRC32, safe relative paths, install records and a package manifest.
- Compiler and VM automatically search `PIPI_HOME` or `$HOME/.pipi` (Windows: `%USERPROFILE%/.pipi`) after installation.
- Includes individual `.woodlib` packages for each standard library and a complete independently authored `acme/greeting` module example.
- Preserved the compiler, VM and public ABI model: the bytecode magic remains `PIPIBC06`, native ABI remains v1. There is no Java/JNI/JAR runtime dependency.
- Added a one-time GitHub repository setup script, an idempotent version publisher, immutable version tags and optional GitHub Release creation.
- Added GitHub Actions CI for strict C89 builds and automated tests.

## Standard library overview

| Import | Purpose | Main exports |
|---|---|---|
| `pipi/core` | Built-in language helpers | `print` |
| `pipi/math` | Built-in math helpers | compiler/VM-provided math exports |
| `pipi/net` | TCP/UDP sockets | `socketCreate`, `socketConnect`, `socketBind`, `socketListen`, `socketAccept`, `socketSend`, `socketRecv`, `socketClose`, `socketLastError` |
| `pipi/http` | Plain HTTP GET | `httpGet` |
| `pipi/fs` | Files and directories | `fileRead`, `fileWrite`, `fileAppend`, `fileCopy`, `fileMove`, `fileDelete`, `fileSize`, `fileExists`, `directoryCreate` |
| `pipi/time` | Unix time, milliseconds, sleep, formatting | `timeUnix`, `timeMillis`, `sleepMs`, `timeFormat` |
| `pipi/string` | Common string functions | `strContains`, `strIndexOf`, `strUpper`, `strLower`, `strTrim`, `strReplace`, `strStartsWith`, `strEndsWith` |
| `pipi/encoding` | URL and hex text encoding | `urlEncode`, `urlDecode`, `hexEncode`, `hexDecode` |
| `pipi/system` | OS and process details | `osName`, `architecture`, `envGet`, `currentDirectory`, `processId`, `pathSeparator` |
| `pipi/crypto` | Hash/checksum helpers | `sha256`, `crc32` |
| `pipi/path` | Path manipulation | `pathJoin`, `pathBasename`, `pathDirname`, `pathExtension` |
| `pipi/json` | JSON object scalar lookup and escaping | `jsonHasKey`, `jsonGetString`, `jsonGetNumber`, `jsonGetBoolean`, `jsonType`, `jsonEscape` |
| `pipi/random` | Non-cryptographic PRNG | `randomSeed`, `randomInt`, `randomFloat`, `randomBool` |
| `pipi/terminal` | Terminal read/write and ANSI colors | `terminalWrite`, `terminalWriteLine`, `terminalReadLine`, `terminalClear`, `terminalColor`, `terminalResetColor`, `terminalIsTTY` |

### Important scope notes

- `pipi/http` currently implements plain HTTP only; it does **not** provide HTTPS/TLS.
- `pipi/net` uses blocking IPv4 sockets. `socketRecv` returns text, not arbitrary binary buffers. UDP sockets can be connected and used with `socketSend`/`socketRecv`; there is no `sendTo`/`recvFrom` API yet.
- `pipi/fs` text reads are capped at 1 MiB.
- `pipi/json` currently reads scalar values from JSON objects. It skips nested JSON values safely, but does not create PIPI objects/arrays because the language does not yet have first-class object/array values.
- `pipi/random` is a deterministic/pseudo-random utility and is **not cryptographically secure**.
- `pipi/encoding`'s URL/hex helpers operate on text strings; they are not a binary buffer API.
- Native modules execute with the host process's privileges. ABI compatibility and CRC verification are not a sandbox.

## Build

### Linux / Termux

Requires a C89-capable compiler. On Termux, install Clang first:

```sh
pkg install clang
CC=clang ./build.sh
```

The build script detects `x86_64`, `aarch64` and ARMv7 Linux targets, compiles the C89 compiler/VM/PIPIwood, builds all native modules for the current architecture, compiles examples and creates `.woodlib` archives under `dist/woodlibs/`. **Do not copy the included x86-64 ELF executables or install the x86-64 `.woodlib` files on Termux ARM64**; build locally with `CC=clang` so all native plugins match the phone CPU.

To install the tools into Termux's command path:

```sh
install -m 755 bin/pipi-c bin/pipi-vm bin/pipiwood "$PREFIX/bin/"
hash -r
```

See [`docs/TERMUX.md`](docs/TERMUX.md) for the complete Android/Termux ARM64 build and install procedure.

### Windows

Use MinGW-w64 GCC. The Windows build compiles the C89 modules as DLLs and creates Windows-targeted `.woodlib` packages:

```bat
build.bat
```

PDX (Direct3D 9) still needs its Windows graphics SDK/import library as described in `frameworks/PDX/README.md`.

## PIPIwood quick start

List and verify packages:

```sh
pipiwood list dist/woodlibs/pipi_net.woodlib
pipiwood verify dist/woodlibs/pipi_net.woodlib
```

Install to the default user location (`~/.pipi`, or `%USERPROFILE%/.pipi`):

```sh
pipiwood install dist/woodlibs/pipi_net.woodlib
```

Or install into a custom root:

```sh
pipiwood install dist/woodlibs/pipi_net.woodlib --root ./pipi-packages
PIPI_HOME="$PWD/pipi-packages" pipi-c compile app.pipi -o app.pbc
PIPI_HOME="$PWD/pipi-packages" pipi-vm app.pbc
```

Remove an installed package:

```sh
pipiwood remove pipi/net
```

## Use sockets

```ts
import { print } from "pipi/core";
import { socketCreate, socketConnect, socketSend, socketRecv, socketClose } from "pipi/net";

let sock = socketCreate("tcp");
if (socketConnect(sock, "127.0.0.1", 7000)) {
  socketSend(sock, "hello from PIPI");
  print(socketRecv(sock, 1024));
}
socketClose(sock);
```

See `examples/tcp_echo_client.pipi` for a complete client example. `examples/http_get.pipi` demonstrates a plain HTTP request.

## Create a third-party `.woodlib`

1. Implement the C89 `PipiNativeModuleV1` contract in `include/pipi_native.h`.
2. Create a `PIPI-MODULE 1` module manifest with the exact exported symbol list.
3. Build `libpipi_<leaf>.so` (Linux) or `pipi_<leaf>.dll` (Windows) under `modules/<module-path>/native/<platform>/`.
4. Create a text file list containing archive paths and optional source-file paths.
5. Run `pipiwood pack files.lst -o my-library.woodlib`, then `pipiwood verify my-library.woodlib`.
6. Users run `pipiwood install my-library.woodlib` and import it from PIPI normally.

A complete working example lives in `examples/woodlib_sample/`. `dist/woodlibs/acme_greeting.woodlib` is built and installed during the Linux build smoke test; the sample is then loaded through `PIPI_HOME` and executed by the VM.

## Documentation

- `docs/index.html` — language and architecture overview.
- `docs/stdlib/index.html` — standard-library catalogue.
- `docs/WOODLIB.md` — package format and PIPIwood commands.
- `docs/GITHUB.md` — one-time repository setup and repeatable publishing.
- `docs/TERMUX.md` — Android/Termux ARM64 setup.
- `docs/pdx/index.html` and `docs/PDX_API.md` — PDX game framework.

## Tests

```sh
python3 -m unittest discover -s tests -v
```

Python is used only by the test harness. The compiler, VM, native modules and PIPIwood are C89 and do not require Python at runtime.

## Publish this repository to GitHub

The repository is created **once**; each later version only pushes commits and a
new immutable version tag. GitHub CLI is required only for the initial remote
creation and optional GitHub Releases.

```sh
pkg install git gh       # Termux; on Linux install git + gh normally
gh auth login
./scripts/github-init.sh OWNER/REPOSITORY --public   # one time only

# For each later version, update PIPI_VERSION + CHANGELOG.md, then:
CC=clang ./build.sh
python3 -m unittest discover -s tests -v
./scripts/github-publish.sh
```

Use `./scripts/github-publish.sh --release` to also create a GitHub Release and
attach current-host `.woodlib` packages. The publisher never creates a repo.
See [docs/GITHUB.md](docs/GITHUB.md) for details and safety behavior.
