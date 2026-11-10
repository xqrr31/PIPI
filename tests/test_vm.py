import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
COMPILER = ROOT / "bin" / ("pipi-c.exe" if os.name == "nt" else "pipi-c")
VM = ROOT / "bin" / ("pipi-vm.exe" if os.name == "nt" else "pipi-vm")


def run(args, *, timeout=10, env=None):
    merged_env = os.environ.copy()
    if env:
        merged_env.update(env)
    return subprocess.run(
        [str(x) for x in args], cwd=ROOT, text=True, env=merged_env,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=timeout
    )


class PIPIVMTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not COMPILER.exists() or not VM.exists():
            build = "build.bat" if os.name == "nt" else "./build.sh"
            subprocess.run(build, cwd=ROOT, shell=(os.name == "nt"), check=True, timeout=120)

    def compile_source(self, source):
        handle = tempfile.NamedTemporaryFile(
            mode="w", encoding="utf-8", suffix=".pipi", prefix="test_",
            dir=ROOT / "tests", delete=False
        )
        try:
            handle.write(source)
            handle.close()
            output = Path(handle.name).with_suffix(".pbc")
            compiled = run([COMPILER, "compile", handle.name, "-o", output])
            return handle.name, output, compiled
        except Exception:
            handle.close()
            raise

    def test_compiler_and_vm_versions_are_100(self):
        self.assertIn("1.0.0", run([COMPILER, "--version"]).stdout)
        self.assertIn("1.0.0", run([VM, "--version"]).stdout)

    def test_hello_and_user_function(self):
        result = run([VM, ROOT / "dist" / "PIPI_Hello.pbc"])
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("PIPI is alive — version 2", result.stdout)
        self.assertIn("square(9) = 81", result.stdout)

    def test_custom_bytecode_magic_and_not_java_class(self):
        program = (ROOT / "dist" / "PIPI_Hello.pbc").read_bytes()
        self.assertEqual(program[:8], b"PIPIBC06")
        self.assertNotEqual(program[:4], b"\xca\xfe\xba\xbe")

    def test_control_flow_and_arithmetic(self):
        result = run([VM, ROOT / "dist" / "PIPI_ControlFlow.pbc"])
        self.assertEqual(result.returncode, 0, result.stderr)
        for expected in ("fizz", "buzz", "fizzbuzz", "17"):
            self.assertIn(expected, result.stdout)

    def test_import_aliases(self):
        result = run([VM, ROOT / "dist" / "PIPI_AliasImport.pbc"])
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("PIPI aliases work", result.stdout)
        self.assertIn("root(144) = 12", result.stdout)

    def test_native_module_and_callback_bridge(self):
        result = run([VM, ROOT / "dist" / "PIPI_NativeModule.pbc"])
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("pipi/extra 1.0.0 (native ABI v1)", result.stdout)
        self.assertIn("clamp(27, 0, 10) = 10", result.stdout)
        self.assertIn("plugin called back into PIPI with 8", result.stdout)
        self.assertIn("callback result = 9", result.stdout)

    def test_vm_keeps_reading_pipibc05(self):
        legacy = ROOT / "tests" / "fixtures" / "hello_pipibc05.pbc"
        self.assertEqual(legacy.read_bytes()[:8], b"PIPIBC05")
        result = run([VM, legacy])
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("square(9) = 81", result.stdout)

    def test_pdx_examples_compile_with_resolved_function_names(self):
        result = run([COMPILER, "compile", ROOT / "frameworks" / "PDX" / "examples" / "pdx_demo.pipi", "-o", ROOT / "tests" / "pdx_test.pbc"])
        try:
            self.assertEqual(result.returncode, 0, result.stderr)
            bytecode = (ROOT / "tests" / "pdx_test.pbc").read_bytes()
            self.assertEqual(bytecode[:8], b"PIPIBC06")
        finally:
            (ROOT / "tests" / "pdx_test.pbc").unlink(missing_ok=True)

    @unittest.skipUnless(sys.platform.startswith("linux") and shutil.which("cc"), "external shared-module build is tested on Linux")
    def test_external_module_installs_without_changing_core(self):
        with tempfile.TemporaryDirectory(prefix="pipi_external_modules_") as temp_root:
            package = Path(temp_root) / "acme" / "gizmo"
            native = package / "native" / "linux-x86_64"
            native.mkdir(parents=True)
            (package / "gizmo.pmod").write_text(
                "PIPI-MODULE 1\nname acme/gizmo\nkind native-plugin\nabi 1\nexports twice\n",
                encoding="utf-8",
            )
            plugin_source = package / "gizmo.c"
            plugin_source.write_text(r'''#include <string.h>
#include "pipi_native.h"
static int has_function(const char *name) { return strcmp(name, "twice") == 0; }
static int call_function(const PipiHostAPI *host, const char *name, int argc,
    const PipiValue *argv, PipiValue *result, char *error, size_t error_size) {
    (void)host; (void)error; (void)error_size;
    if (strcmp(name, "twice") != 0 || argc != 1 || argv[0].type != PIPI_VALUE_NUMBER) return 0;
    result->type = PIPI_VALUE_NUMBER; result->number = argv[0].number * 2.0;
    result->string = ""; result->boolean = 0; return 1;
}
static const PipiNativeModuleV1 api = { PIPI_NATIVE_ABI_VERSION, "acme/gizmo", "1.0", 0, has_function, call_function, 0 };
PIPI_NATIVE_EXPORT const PipiNativeModuleV1 *pipi_native_module_v1(void) { return &api; }
''', encoding="utf-8")
            library = native / "libpipi_gizmo.so"
            built = subprocess.run(
                ["cc", "-std=c89", "-pedantic", "-Wall", "-Wextra", "-Werror", "-fPIC", "-shared",
                 "-I", str(ROOT / "include"), str(plugin_source), "-o", str(library)],
                text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30,
            )
            self.assertEqual(built.returncode, 0, built.stderr)
            source = ROOT / "tests" / "external_module_test.pipi"
            output = ROOT / "tests" / "external_module_test.pbc"
            source.write_text('import { twice } from "acme/gizmo";\nimport { print } from "pipi/core";\nprint(twice(21));\n', encoding="utf-8")
            try:
                env = {"PIPI_MODULE_PATH": temp_root}
                compiled = run([COMPILER, "compile", source, "-o", output], env=env)
                self.assertEqual(compiled.returncode, 0, compiled.stderr)
                runtime = run([VM, output], env=env)
                self.assertEqual(runtime.returncode, 0, runtime.stderr)
                self.assertIn("42", runtime.stdout)
            finally:
                source.unlink(missing_ok=True)
                output.unlink(missing_ok=True)

    def test_missing_native_plugin_has_clear_runtime_error(self):
        manifest = ROOT / "lib" / "pipi" / "ghost.pmod"
        source = ROOT / "tests" / "native_missing_test.pipi"
        output = ROOT / "tests" / "native_missing_test.pbc"
        manifest.write_text("PIPI-MODULE 1\nname pipi/ghost\nkind native-plugin\nabi 1\nexports ghost\n", encoding="utf-8")
        source.write_text('import { ghost } from "pipi/ghost";\nghost();\n', encoding="utf-8")
        try:
            compiled = run([COMPILER, "compile", source, "-o", output])
            self.assertEqual(compiled.returncode, 0, compiled.stderr)
            runtime = run([VM, output], env={"PIPI_MODULE_PATH": str(ROOT / "no_such_module_root")})
            self.assertNotEqual(runtime.returncode, 0)
            self.assertIn("native module 'pipi/ghost' not found", runtime.stderr)
        finally:
            manifest.unlink(missing_ok=True)
            source.unlink(missing_ok=True)
            output.unlink(missing_ok=True)

    def test_unknown_function_fails_at_compile_time(self):
        src = 'import { print } from "pipi/core";\nprint("before");\nnotARealFunction();\n'
        path, output, compiled = self.compile_source(src)
        try:
            self.assertNotEqual(compiled.returncode, 0)
            self.assertIn("unknown function 'notARealFunction'", compiled.stderr)
            self.assertIn("import it from a module or define it with fn", compiled.stderr)
        finally:
            Path(path).unlink(missing_ok=True)
            output.unlink(missing_ok=True)

    def test_check_command_also_resolves_function_names(self):
        src = 'import { print } from "pipi/core";\nprint("ok");\nnotImportedYet();\n'
        path, output, _ = self.compile_source(src)
        try:
            checked = run([COMPILER, "check", path])
            self.assertNotEqual(checked.returncode, 0)
            self.assertIn("unknown function 'notImportedYet'", checked.stderr)
        finally:
            Path(path).unlink(missing_ok=True)
            output.unlink(missing_ok=True)

    def test_missing_export_fails_at_compile_time(self):
        src = 'import { definitelyMissing } from "pipi/core";\n'
        path, output, compiled = self.compile_source(src)
        try:
            self.assertNotEqual(compiled.returncode, 0)
            self.assertIn("does not export", compiled.stderr)
        finally:
            Path(path).unlink(missing_ok=True)
            output.unlink(missing_ok=True)

    def test_runtime_division_by_zero_is_reported(self):
        src = 'import { print } from "pipi/core";\nprint(3 / 0);\n'
        path, output, compiled = self.compile_source(src)
        try:
            self.assertEqual(compiled.returncode, 0, compiled.stderr)
            runtime = run([VM, output])
            self.assertNotEqual(runtime.returncode, 0)
            self.assertIn("division by zero", runtime.stderr)
        finally:
            Path(path).unlink(missing_ok=True)
            output.unlink(missing_ok=True)

    def test_custom_string_and_branch(self):
        src = '''import { print } from "pipi/core";
let score: number = 9;
if (score >= 9) { print("skibidi " + score); } else { print("nope"); }
'''
        path, output, compiled = self.compile_source(src)
        try:
            self.assertEqual(compiled.returncode, 0, compiled.stderr)
            runtime = run([VM, output])
            self.assertEqual(runtime.returncode, 0, runtime.stderr)
            self.assertIn("skibidi 9", runtime.stdout)
        finally:
            Path(path).unlink(missing_ok=True)
            output.unlink(missing_ok=True)

    @unittest.skipUnless(
        sys.platform.startswith("linux") and shutil.which("xvfb-run") and shutil.which("timeout"),
        "GUI smoke test requires Linux, Xvfb and timeout"
    )
    def test_native_gui_loads_without_jni(self):
        proc = run(["xvfb-run", "-a", "timeout", "2s", VM, ROOT / "dist" / "PIPI_GUI.pbc"], timeout=6)
        self.assertEqual(proc.returncode, 124, proc.stderr + proc.stdout)
        self.assertIn("Window opened. Close it to exit.", proc.stdout)
        self.assertNotIn("UnsatisfiedLinkError", proc.stderr)
        self.assertNotIn("PIPI runtime error", proc.stderr)

    @unittest.skipUnless(
        sys.platform.startswith("linux") and shutil.which("xvfb-run") and shutil.which("timeout"),
        "GUI smoke test requires Linux, Xvfb and timeout"
    )
    def test_native_gui_loads_when_started_from_dist_directory(self):
        proc = subprocess.run(
            ["xvfb-run", "-a", "timeout", "2s", str(VM), "PIPI_GUI.pbc"],
            cwd=ROOT / "dist", text=True, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, timeout=6
        )
        self.assertEqual(proc.returncode, 124, proc.stderr + proc.stdout)
        self.assertIn("Window opened. Close it to exit.", proc.stdout)
        self.assertNotIn("PIPI runtime error", proc.stderr)

    def test_linux_shared_library_exports_plain_c_api(self):
        lib = ROOT / "native" / "linux-x86_64" / "libpipi_gui.so"
        if not lib.exists() or not shutil.which("nm"):
            self.skipTest("native library or nm unavailable")
        symbols = subprocess.run(["nm", "-D", str(lib)], text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.assertIn("pipi_gui_window_create", symbols.stdout)
        self.assertNotIn("Java_", symbols.stdout)

    @unittest.skipUnless(sys.platform.startswith("linux") and shutil.which("nm"), "requires Linux nm")
    def test_sample_native_plugin_exports_only_stable_entry(self):
        lib = ROOT / "modules" / "pipi" / "extra" / "native" / "linux-x86_64" / "libpipi_extra.so"
        self.assertTrue(lib.exists(), "native module should be built by build.sh")
        symbols = subprocess.run(["nm", "-D", str(lib)], text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.assertIn("pipi_native_module_v1", symbols.stdout)
        self.assertNotIn("Java_", symbols.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
