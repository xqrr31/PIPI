# Windows native GUI backend

`pipi_gui_backend.c` implements the PIPI C GUI API using Win32 windows, STATIC labels, BUTTON controls and message boxes. It is compiled into `pipi_gui.dll` and exports `pipi_gui_*` symbols through `lib/pipi_gui.h`.

This is a direct C ABI. It does **not** include `jni.h`, `JNIEnv`, Java classes, or JNI symbol mangling.

Build from the project root with MinGW-w64:

```bat
native\windows-x86_64\build.bat
```

Or run the full `build.bat` in the project root to build the compiler, VM, DLL and example bytecode.
