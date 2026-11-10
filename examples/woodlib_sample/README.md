# Creating a third-party .woodlib

1. Implement the `PipiNativeModuleV1` contract from `include/pipi_native.h` in C89.
2. Add a `PIPI-MODULE 1` manifest with `kind native-plugin`, `abi 1`, and exact exports.
3. Build `libpipi_<module-leaf>.so` or `pipi_<module-leaf>.dll` under `modules/<module-name>/native/<platform>/`.
4. Create a `.woodlib/manifest` package file and a two-column Pipiwood file list.
5. Run `pipiwood pack files.lst -o library.woodlib`, then `pipiwood verify library.woodlib`.
6. Users install it with `pipiwood install library.woodlib` and import it normally.

See the root `build.sh` and this sample's matching build section for a full working package.
