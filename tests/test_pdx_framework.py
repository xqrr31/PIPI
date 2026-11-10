import os
import re
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
COMPILER = ROOT / "bin" / ("pipi-c.exe" if os.name == "nt" else "pipi-c")
def run(args, *, cwd=ROOT, env=None, timeout=20):
    return subprocess.run([str(a) for a in args], cwd=cwd, env=env,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                          text=True, timeout=timeout)


class PDXFrameworkTests(unittest.TestCase):
    def test_public_native_abi_remains_stable_and_compiler_checks_calls(self):
        import hashlib
        digest = hashlib.sha256((ROOT / "include" / "pipi_native.h").read_bytes()).hexdigest()
        self.assertEqual(digest, "69bd36110895160e5aac6d00134585a3221e9b29c39b93d0d90c3bea43e75a5a")
        compiler_source = (ROOT / "bin" / "pipi.c").read_text(encoding="utf-8")
        self.assertIn("unknown function '%s'; import it from a module or define it with fn", compiler_source)

    def test_manifest_exports_have_native_dispatch(self):
        manifest = (ROOT / "lib" / "pdx.pmod").read_text(encoding="utf-8")
        native = (ROOT / "frameworks" / "PDX" / "pdx_native.cpp").read_text(encoding="utf-8")
        exports_line = next(line for line in manifest.splitlines() if line.startswith("exports "))
        exports = exports_line.split()[1:]
        self.assertGreaterEqual(len(exports), 30)
        start = native.index("static int has_function")
        finish = native.index("static int call_function", start)
        has_function_body = native[start:finish]
        for name in exports:
            self.assertIn(f'"{name}"', has_function_body, f"has_function missing {name}")
            self.assertIn(f'"{name}"', native, f"no native dispatch evidence for {name}")

    def test_manifest_copies_are_identical_and_abi_v1(self):
        a = (ROOT / "lib" / "pdx.pmod").read_bytes()
        b = (ROOT / "modules" / "pdx" / "pdx.pmod").read_bytes()
        self.assertEqual(a, b)
        self.assertIn(b"kind native-plugin", a)
        self.assertIn(b"abi 1", a)
        self.assertIn(b"platform windows-x86_64", a)

    def test_gameplay_and_texture_examples_compile_to_pipibc06(self):
        examples = [
            (ROOT / "frameworks" / "PDX" / "examples" / "pdx_demo.pipi", ROOT / "dist" / "PIPI_PDX_Demo.pbc"),
            (ROOT / "frameworks" / "PDX" / "examples" / "texture_demo.pipi", ROOT / "dist" / "PIPI_PDX_TextureDemo.pbc"),
        ]
        for source, output in examples:
            with self.subTest(source=source.name):
                result = run([COMPILER, "compile", source, "-o", output])
                self.assertEqual(result.returncode, 0, result.stderr.decode(errors="replace") if isinstance(result.stderr, bytes) else result.stderr)
                self.assertEqual(output.read_bytes()[:8], b"PIPIBC06")

    def test_missing_pdx_import_is_a_compile_error(self):
        with tempfile.TemporaryDirectory(prefix="pipi_missing_pdx_import_") as temp:
            source = Path(temp) / "missing_drawline.pipi"
            output = Path(temp) / "missing_drawline.pbc"
            source.write_text(
                'import { open } from "pdx";\nopen("test", 640, 480);\ndrawLine(0, 0, 10, 10, 255, 255, 255);\n',
                encoding="utf-8",
            )
            result = run([COMPILER, "compile", source, "-o", output])
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("unknown function 'drawLine'", result.stderr)
            self.assertIn("line 3", result.stderr)

    def test_docs_cover_all_native_exports(self):
        manifest = (ROOT / "lib" / "pdx.pmod").read_text(encoding="utf-8")
        exports_line = next(line for line in manifest.splitlines() if line.startswith("exports "))
        docs = (ROOT / "docs" / "PDX_API.md").read_text(encoding="utf-8")
        html = (ROOT / "docs" / "pdx" / "index.html").read_text(encoding="utf-8")
        for name in exports_line.split()[1:]:
            self.assertTrue(name in docs or name in html, f"docs don't mention API function {name}")

    def test_build_files_compile_both_pdx_examples_and_build_dll(self):
        sh = (ROOT / "build.sh").read_text(encoding="utf-8")
        bat = (ROOT / "build.bat").read_text(encoding="utf-8")
        for sample in ("pdx_demo.pipi", "texture_demo.pipi"):
            self.assertIn(sample, sh)
            self.assertIn(sample, bat)
        self.assertIn("-lwinmm", (ROOT / "frameworks" / "PDX" / "build.bat").read_text(encoding="utf-8"))
        self.assertIn("call frameworks\\PDX\\build.bat", bat)


if __name__ == "__main__":
    unittest.main(verbosity=2)
