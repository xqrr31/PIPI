import os
import socket
import socketserver
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
COMPILER = ROOT / "bin" / ("pipi-c.exe" if os.name == "nt" else "pipi-c")
VM = ROOT / "bin" / ("pipi-vm.exe" if os.name == "nt" else "pipi-vm")
WOOD = ROOT / "bin" / ("pipiwood.exe" if os.name == "nt" else "pipiwood")


def run(args, *, env=None, cwd=ROOT, timeout=15):
    merged = os.environ.copy()
    if env:
        merged.update(env)
    return subprocess.run(
        [str(x) for x in args], cwd=cwd, env=merged, text=True,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=timeout,
    )


class EchoHandler(socketserver.BaseRequestHandler):
    def handle(self):
        data = self.request.recv(4096)
        self.request.sendall(b"ACK:" + data)


class HTTPHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        body = b"PIPI_HTTP_OK"
        self.send_response(200)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *_args):
        pass


class PIPIStandardLibraryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not COMPILER.exists() or not VM.exists() or not WOOD.exists():
            subprocess.run("./build.sh" if os.name != "nt" else "build.bat", cwd=ROOT,
                           shell=(os.name == "nt"), check=True, timeout=180)

    def compile_and_run(self, source, env=None, cwd=ROOT):
        with tempfile.TemporaryDirectory(prefix="pipi_stdlib_") as temp:
            src = Path(temp) / "app.pipi"
            pbc = Path(temp) / "app.pbc"
            src.write_text(source, encoding="utf-8")
            compiled = run([COMPILER, "compile", src, "-o", pbc], env=env, cwd=cwd)
            self.assertEqual(compiled.returncode, 0, compiled.stderr)
            return run([VM, pbc], env=env, cwd=cwd)


    def test_each_standard_manifest_matches_its_c89_native_module(self):
        modules = ("net", "fs", "time", "string", "encoding", "http", "system", "crypto", "path", "json", "random", "terminal")
        for module in modules:
            with self.subTest(module=module):
                manifest = (ROOT / "lib" / "pipi" / f"{module}.pmod").read_text(encoding="utf-8")
                source = (ROOT / "stdlib" / f"pipi_{module}.c").read_text(encoding="utf-8")
                exports = next(line for line in manifest.splitlines() if line.startswith("exports ")).split()[1:]
                self.assertIn(f'"pipi/{module}"', source)
                for export in exports:
                    self.assertIn(f'"{export}"', source, f"{module} source missing {export}")
                installed_manifest = (ROOT / "modules" / "pipi" / module / f"{module}.pmod").read_bytes()
                self.assertEqual(installed_manifest, (ROOT / "lib" / "pipi" / f"{module}.pmod").read_bytes())

    def test_stdlib_demo_runs_every_native_module(self):
        result = run([VM, ROOT / "dist" / "PIPI_StdlibDemo.pbc"])
        self.assertEqual(result.returncode, 0, result.stderr)
        for expected in (
            "TCP socket handle:", "File contents: PIPI v1.0.0 woodlib test",
            "Uppercase: PIPI IS SO BACK", "URL decoded: hello pipi world!",
            "SHA-256 abc: ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
            "Basename: hero.bmp", "OS: linux / x86_64",
            "JSON name: PIPI", "JSON version: 1", "JSON ready: true",
            "JSON has name: true", "JSON type: string", "JSON escaped: hello\\n\\\"wood\\\"",
            "PIPIwood standard libraries loaded.",
        ):
            self.assertIn(expected, result.stdout)

    @unittest.skipUnless(sys.platform.startswith("linux"), "POSIX socket smoke test")
    def test_tcp_socket_connect_send_receive(self):
        server = socketserver.ThreadingTCPServer(("127.0.0.1", 0), EchoHandler)
        server.daemon_threads = True
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            port = server.server_address[1]
            source = f'''import {{ print }} from "pipi/core";
import {{ socketCreate, socketConnect, socketSend, socketRecv, socketClose }} from "pipi/net";
let sock = socketCreate("tcp");
print("connected=" + socketConnect(sock, "127.0.0.1", {port}));
socketSend(sock, "ping");
print(socketRecv(sock, 64));
socketClose(sock);
'''
            result = self.compile_and_run(source, {"PIPI_MODULE_PATH": str(ROOT)})
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("connected=true", result.stdout)
            self.assertIn("ACK:ping", result.stdout)
        finally:
            server.shutdown()
            server.server_close()
            thread.join(timeout=2)


    @unittest.skipUnless(sys.platform.startswith("linux"), "POSIX TCP server test")
    def test_tcp_bind_listen_accept_echo_server(self):
        with tempfile.TemporaryDirectory(prefix="pipi_tcp_server_") as temp:
            probe = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            probe.bind(("127.0.0.1", 0))
            port = probe.getsockname()[1]
            probe.close()
            source = '''import { print } from "pipi/core";
import { socketCreate, socketSetReuse, socketBind, socketListen, socketAccept, socketRecv, socketSend, socketClose } from "pipi/net";
let server = socketCreate("tcp");
socketSetReuse(server, true);
if (socketBind(server, "127.0.0.1", PORT_PLACEHOLDER)) {
  if (socketListen(server, 4)) {
    while (true) {
      let client = socketAccept(server);
      if (client > 0) {
        let message = socketRecv(client, 128);
        socketSend(client, "echo:" + message);
        socketClose(client);
      }
    }
  }
}
socketClose(server);
'''.replace("PORT_PLACEHOLDER", str(port))
            source_file = Path(temp) / "server.pipi"
            bytecode = Path(temp) / "server.pbc"
            source_file.write_text(source, encoding="utf-8")
            compiled = run([COMPILER, "compile", source_file, "-o", bytecode])
            self.assertEqual(compiled.returncode, 0, compiled.stderr)
            process = subprocess.Popen([str(VM), str(bytecode)], cwd=ROOT,
                                       stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
            try:
                client = None
                for _ in range(60):
                    try:
                        client = socket.create_connection(("127.0.0.1", port), timeout=0.25)
                        break
                    except OSError:
                        if process.poll() is not None:
                            break
                        time.sleep(0.05)
                self.assertIsNotNone(client, "PIPI TCP server did not accept connections")
                with client:
                    client.sendall(b"from-python")
                    client.shutdown(socket.SHUT_WR)
                    self.assertEqual(client.recv(128), b"echo:from-python")
            finally:
                process.terminate()
                try:
                    process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=2)
                if process.stderr:
                    process.stderr.close()

    @unittest.skipUnless(sys.platform.startswith("linux"), "plain HTTP local-server test")
    def test_http_get_against_local_server(self):
        server = HTTPServer(("127.0.0.1", 0), HTTPHandler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            port = server.server_address[1]
            source = f'''import {{ print }} from "pipi/core";
import {{ httpGet }} from "pipi/http";
print(httpGet("127.0.0.1", "/", {port}));
'''
            result = self.compile_and_run(source, {"PIPI_MODULE_PATH": str(ROOT)})
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("200 OK", result.stdout)
            self.assertIn("PIPI_HTTP_OK", result.stdout)
        finally:
            server.shutdown()
            server.server_close()
            thread.join(timeout=2)


    def test_all_standard_woodlibs_install_and_run_from_pipi_home(self):
        packages = sorted((ROOT / "dist" / "woodlibs").glob("pipi_*.woodlib"))
        with tempfile.TemporaryDirectory(prefix="pipiwood_all_stdlib_") as home:
            for package in packages:
                installed = run([WOOD, "install", package, "--root", home])
                self.assertEqual(installed.returncode, 0, f"{package.name}: {installed.stderr}")
            result = run([VM, ROOT / "dist" / "PIPI_StdlibDemo.pbc"],
                         env={"PIPI_HOME": home, "PIPI_MODULE_PATH": ""})
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("SHA-256 abc:", result.stdout)
            self.assertIn("JSON name: PIPI", result.stdout)
            self.assertIn("PIPIwood standard libraries loaded.", result.stdout)

    def test_woodlib_install_auto_configures_compiler_and_vm_search(self):
        archive = ROOT / "dist" / "woodlibs" / "pipi_net.woodlib"
        with tempfile.TemporaryDirectory(prefix="pipiwood_home_") as home:
            installed = run([WOOD, "install", archive, "--root", home])
            self.assertEqual(installed.returncode, 0, installed.stderr)
            source = '''import { socketCreate, socketClose } from "pipi/net";
let sock = socketCreate("tcp");
socketClose(sock);
'''
            result = self.compile_and_run(source, {"PIPI_HOME": home, "PIPI_MODULE_PATH": ""}, cwd=home)
            self.assertEqual(result.returncode, 0, result.stderr)
            removed = run([WOOD, "remove", "pipi/net", "--root", home])
            self.assertEqual(removed.returncode, 0, removed.stderr)
            self.assertFalse((Path(home) / "modules/pipi/net/net.pmod").exists())


    def test_json_getters_skip_nested_values_and_handle_escapes(self):
        source = r'''import { print } from "pipi/core";
import { jsonGetString, jsonGetNumber, jsonGetBoolean, jsonHasKey, jsonType, jsonEscape } from "pipi/json";
let data = "{\"nested\":{\"items\":[1,2]},\"title\":\"wood\\\"lib\",\"count\":42,\"ok\":false}";
print(jsonGetString(data, "title"));
print(jsonGetNumber(data, "count"));
print(jsonGetBoolean(data, "ok"));
print(jsonHasKey(data, "nested"));
print(jsonType(data, "nested"));
print(jsonEscape("line\nquote\""));
'''
        result = self.compile_and_run(source, {"PIPI_MODULE_PATH": str(ROOT)})
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('wood"lib', result.stdout)
        self.assertIn('42', result.stdout)
        self.assertIn('false', result.stdout)
        self.assertIn('object', result.stdout)
        self.assertIn(r'line\nquote\"', result.stdout)

    def test_random_seed_is_repeatable_and_values_stay_in_range(self):
        source = '''import { print } from "pipi/core";
import { randomSeed, randomInt, randomFloat } from "pipi/random";
randomSeed(1337);
let first = randomInt(1, 6);
randomSeed(1337);
let second = randomInt(1, 6);
let fraction = randomFloat();
print("same=" + (first == second));
print("range=" + (first >= 1 && first <= 6));
print("float=" + (fraction >= 0 && fraction < 1));
'''
        result = self.compile_and_run(source, {"PIPI_MODULE_PATH": str(ROOT)})
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("same=true", result.stdout)
        self.assertIn("range=true", result.stdout)
        self.assertIn("float=true", result.stdout)

    def test_third_party_woodlib_installs_and_loads_native_module(self):
        archive = ROOT / "dist" / "woodlibs" / "acme_greeting.woodlib"
        program = ROOT / "dist" / "PIPI_WoodlibSample.pbc"
        with tempfile.TemporaryDirectory(prefix="pipiwood_external_") as home:
            installed = run([WOOD, "install", archive, "--root", home])
            self.assertEqual(installed.returncode, 0, installed.stderr)
            result = run([VM, program], env={"PIPI_HOME": home, "PIPI_MODULE_PATH": ""})
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("Hello, bestie! Welcome to PIPIwood.", result.stdout)
            self.assertIn("doubleValue(21) = 42", result.stdout)
            removed = run([WOOD, "remove", "acme/greeting", "--root", home])
            self.assertEqual(removed.returncode, 0, removed.stderr)
            self.assertFalse((Path(home) / "modules/acme/greeting/greeting.pmod").exists())

    def test_woodlib_verify_detects_corruption(self):
        source = ROOT / "dist" / "woodlibs" / "pipi_fs.woodlib"
        with tempfile.TemporaryDirectory(prefix="pipiwood_corrupt_") as temp:
            corrupt = Path(temp) / "corrupt.woodlib"
            data = bytearray(source.read_bytes())
            data[-1] ^= 0x5A
            corrupt.write_bytes(data)
            result = run([WOOD, "verify", corrupt])
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("checksum mismatch", result.stderr)

    def test_woodlib_pack_rejects_path_traversal(self):
        with tempfile.TemporaryDirectory(prefix="pipiwood_traversal_") as temp:
            base = Path(temp)
            payload = base / "payload.txt"
            payload.write_text("do not escape", encoding="utf-8")
            file_list = base / "files.lst"
            file_list.write_text(f"../escaped.txt {payload}\n", encoding="utf-8")
            output = base / "bad.woodlib"
            result = run([WOOD, "pack", file_list, "-o", output])
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("unsafe archive path", result.stderr)
            self.assertFalse(output.exists())


    def test_woodlib_pack_rejects_reserved_installer_paths(self):
        with tempfile.TemporaryDirectory(prefix="pipiwood_reserved_") as temp:
            base = Path(temp)
            payload = base / "payload.txt"
            payload.write_text("bad", encoding="utf-8")
            file_list = base / "files.lst"
            file_list.write_text(f".installed/pipi_net.files {payload}\n", encoding="utf-8")
            output = base / "bad.woodlib"
            result = run([WOOD, "pack", file_list, "-o", output])
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("unsafe archive path", result.stderr)
            self.assertFalse(output.exists())

    def test_all_standard_library_packages_verify(self):
        packages = sorted((ROOT / "dist" / "woodlibs").glob("*.woodlib"))
        self.assertGreaterEqual(len(packages), 16)
        for package in packages:
            result = run([WOOD, "verify", package])
            self.assertEqual(result.returncode, 0, f"{package.name}: {result.stderr}")


if __name__ == "__main__":
    unittest.main()
