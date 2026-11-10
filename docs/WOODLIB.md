# PIPIwood and `.woodlib`

PIPIwood is the standard library packer and installer. It is a standalone C89 executable and does not need `zip`, `unzip`, Python, Java or a network connection.

## Commands

```sh
pipiwood --version
pipiwood pack files.lst -o library.woodlib
pipiwood verify library.woodlib
pipiwood list library.woodlib
pipiwood install library.woodlib
pipiwood install library.woodlib --root ./vendor
pipiwood remove acme/greeting
```

The default install directory is `PIPI_HOME`, or `$HOME/.pipi` on POSIX and `%USERPROFILE%\.pipi` on Windows. Compiler and VM search this location automatically.

## Package contents

A package contains `.woodlib/manifest`, module manifests, native binaries and optional source/docs files. A native package typically looks like:

```text
.woodlib/manifest
modules/acme/greeting/greeting.pmod
modules/acme/greeting/native/linux-aarch64/libpipi_greeting.so
sources/acme/greeting.c
```

`WOODLIB 1` package manifest:

```text
WOODLIB 1
name acme/greeting
version 1.0.0
```

Module manifest:

```text
PIPI-MODULE 1
name acme/greeting
kind native-plugin
abi 1
exports greet doubleValue
```

## File-list format

Each non-comment line has either one path (archive path equals source path) or two whitespace-separated fields (archive path, then source path):

```text
modules/acme/greeting/greeting.pmod
modules/acme/greeting/native/linux-x86_64/libpipi_greeting.so build/libpipi_greeting.so
sources/acme/greeting.c src/greeting.c
```

Paths in the archive must be relative and cannot contain `..`, absolute roots, drive prefixes, empty path segments or unsupported path characters. Keep source paths free of spaces in this simple file-list format.

## WOODLIB1 binary format

The format is intentionally small and uncompressed:

- 8 bytes: ASCII magic `WOODLIB1`.
- 4 bytes: big-endian file count.
- Per entry: 2-byte big-endian path length, 4-byte big-endian data size, 4-byte big-endian CRC32, relative path bytes, then file data.

Limits: 4096 files, 511 path bytes per entry, 64 MiB per file and 256 MiB total package data. `verify` checks every entry's CRC32, rejects unsafe paths and rejects trailing bytes. Installation verifies the whole archive before extraction and records installed paths under `.installed/` so `remove` can remove that package's files.

CRC32 detects accidental corruption; it does not authenticate the publisher. Do not install untrusted native code: plugins run with the same privileges as PIPI.
