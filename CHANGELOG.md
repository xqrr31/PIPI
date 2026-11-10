# Changelog

## 1.0.0 — Stable library ecosystem and GitHub publishing

- Promoted the PIPI distribution to 1.0.0 while preserving the `PIPIBC06` bytecode container and Native Module ABI v1.
- Added one-time GitHub repository setup via `scripts/github-init.sh`; it creates the remote only if it does not already exist and refuses to replace a different configured `origin`.
- Added repeatable `scripts/github-publish.sh`; later publishes commit and push to the existing remote, create a version tag once, and optionally create a GitHub Release without ever invoking repository creation.
- Added release packaging support for host-built `.woodlib` files when publishing a GitHub Release.
- Added GitHub Actions CI for strict C89 build and the full Python regression suite.
- Added GitHub publishing documentation for Termux, Linux, macOS and Git Bash.

Validation: strict C89 builds pass with GCC and Clang; 42/42 automated tests pass. Git publishing scripts were integration-tested against a local bare remote and a mocked GitHub CLI, including one-time creation, repeated setup, repeat pushes, release idempotency, and immutable-tag protection.

## 0.9.0 — C89 standard library and PIPIwood

- Added portable C89 native standard libraries: `pipi/net` (TCP/UDP sockets), `pipi/http` (plain HTTP GET), `pipi/fs`, `pipi/time`, `pipi/string`, `pipi/encoding`, `pipi/system`, `pipi/crypto` (SHA-256/CRC32), `pipi/path`, `pipi/json`, `pipi/random` and `pipi/terminal`.
- Added PIPIwood, a standalone C89 `.woodlib` packer, CRC32 verifier, path-safe installer and package remover.
- Added per-library `.woodlib` packages, automatic `PIPI_HOME` / `~/.pipi` lookup, Termux/Linux architecture detection and Windows DLL build steps.
- Added third-party `acme/greeting` package example; build smoke test packs, verifies, installs, loads and removes it without changing the compiler or VM.
- Added standard-library and package-format HTML/Markdown documentation plus live socket, local HTTP, JSON and PRNG regression tests.
- Kept the `PIPIBC06` bytecode container and Native Module ABI v1 compatible.

Validation: strict C89 native modules and tools build successfully; full Linux build and 40/40 automated tests pass. Windows sources and batch build are included but cannot be executed in this Linux environment. Termux/Android must build ARM64 `.woodlib` binaries locally rather than installing the Linux x86-64 packages.


## 0.8.0 — PDX import and function-resolution fix

- Fixed the concrete Neon Arena crash: `drawLine()` was called but missing from the `pdx` import list.
- Added compile-time callable resolution. A call now must target a built-in, an imported export, or a user function declared with `fn`; unknown calls report the source line before bytecode is emitted.
- The main Windows `build.bat` now invokes the PDX native build, so successful full builds leave `modules/pdx/native/windows-x86_64/pipi_pdx.dll` where the VM expects it.
- Bumped the distribution and PDX module to 0.8.0. The bytecode container stays `PIPIBC06`; Native Module ABI v1 remains compatible.
- Added regression tests for unknown-call diagnostics, the formerly missing `drawLine` import, and both PDX examples.

Validation: strict C89 compiler/VM build succeeded; 26/26 automated tests passed on Linux, including PIPIBC06 compilation and runtime/module tests. The Direct3D 9 Windows DLL cannot be executed in this Linux environment, so final graphics/input validation remains a Windows-side test.

## 0.6.0 — Native Module ABI v1

- Added third-party native module support through `.pmod` manifests and the `pipi_native_module_v1` C ABI.
- Added `PIPIBC06` bytecode label and external `pipi/extra` C89 module proof.
- Added an external PDX Direct3D 9/Win32 plugin skeleton.

## 0.5.0 — Custom bytecode VM

- Added the `PIPIBC05` bytecode format and C89 VM.
- Replaced the previous Java/JNI execution model with the custom PIPI runtime.
