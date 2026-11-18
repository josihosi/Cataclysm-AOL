#!/usr/bin/env python3
"""Exercise macOS packaging search paths and noninteractive failure."""

import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[2] / "build-data/osx/bundle_portable_dependencies.sh"


class BundleSearchPathTest(unittest.TestCase):
    def test_search_paths_and_missing_dependency_are_isolated_from_file_list(self):
        with tempfile.TemporaryDirectory(prefix="caol bundle test ") as directory:
            root = Path(directory)
            tools = root / "tools"
            tools.mkdir()
            app = root / "App bundle.app"
            resources = app / "Contents/Resources"
            resources.mkdir(parents=True)
            (app / "Contents/MacOS").mkdir()
            for name in ("probe", "probe two", "ordinary text"):
                (resources / name).write_text("fixture")
            search1 = root / "SDL libs with spaces"
            search2 = root / "codec libs with spaces"
            search1.mkdir()
            search2.mkdir()
            scripts = {
                "file": '''#!/usr/bin/env python3
import pathlib,sys
print(sys.argv[1] + (': Mach-O executable' if pathlib.Path(sys.argv[1]).name.startswith('probe') else ': ASCII text'))
''',
                "otool": '''#!/usr/bin/env python3
import sys
print(sys.argv[-1] + ':\\n /usr/lib/libSystem.B.dylib (compatibility version 1.0.0)')
''',
                "dylibbundler": '''#!/usr/bin/env python3
import json,os,sys
with open(os.environ['CAOL_TEST_BUNDLE_LOG'], 'a', encoding='utf-8') as stream:
    stream.write(json.dumps({'args': sys.argv[1:], 'stdin': sys.stdin.read()}) + '\\n')
sys.exit(1 if os.environ.get('CAOL_TEST_MISSING') else 0)
''',
            }
            for name, content in scripts.items():
                path = tools / name
                path.write_text(content)
                path.chmod(0o755)
            log = root / "calls.jsonl"
            env = os.environ.copy()
            env.update(PATH=str(tools) + os.pathsep + env["PATH"],
                       DYLIBBUNDLER_SEARCH_PATHS=str(search1) + ":" + str(search2),
                       CAOL_TEST_BUNDLE_LOG=str(log))
            command = ["/bin/bash", str(SCRIPT), str(app), str(resources / "probe")]
            result = subprocess.run(command, env=env, capture_output=True, text=True, timeout=15)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            calls = [json.loads(line) for line in log.read_text().splitlines()]
            self.assertEqual(len(calls), 1)
            for call in calls:
                self.assertEqual(call["stdin"], "quit\n")
                args = call["args"]
                self.assertEqual({args[i + 1] for i, item in enumerate(args) if item == "-x"},
                                 {str(resources / "probe"), str(resources / "probe two")})
                self.assertEqual([args[i + 1] for i, item in enumerate(args) if item == "-s"],
                                 [str(search1), str(search2)])
            log.unlink()
            env.pop("DYLIBBUNDLER_SEARCH_PATHS")
            result = subprocess.run(command, env=env, capture_output=True, text=True, timeout=15)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            calls = [json.loads(line) for line in log.read_text().splitlines()]
            self.assertEqual(len(calls), 1)
            self.assertTrue(all("-s" not in call["args"] for call in calls))
            log.unlink()
            env["DYLIBBUNDLER_SEARCH_PATHS"] = str(search1) + ":" + str(search2)
            env["CAOL_TEST_MISSING"] = "1"
            result = subprocess.run(command, env=env, capture_output=True, text=True, timeout=15)
            self.assertNotEqual(result.returncode, 0)
            calls = [json.loads(line) for line in log.read_text().splitlines()]
            self.assertEqual(len(calls), 1)
            self.assertEqual(calls[0]["stdin"], "quit\n")


@unittest.skipUnless(sys.platform == "darwin" and all(shutil.which(x) for x in ("clang", "otool", "dylibbundler")),
                     "requires macOS compiler and dylibbundler")
class NativeBundleTest(unittest.TestCase):
    def test_shared_nested_dependencies_for_multiple_roots(self):
        with tempfile.TemporaryDirectory(prefix="caol multi root ") as directory:
            root = Path(directory)
            libs = root / "source libs"
            libs.mkdir()
            app = root / "App bundle.app"
            resources = app / "Contents/Resources"
            resources.mkdir(parents=True)
            (app / "Contents/MacOS").mkdir()
            (root / "inner.c").write_text("int inner(void) { return 42; }\n")
            (root / "outer.c").write_text("int inner(void); int outer(void) { return inner(); }\n")
            (root / "main.c").write_text("int outer(void); int main(void) { return outer() != 42; }\n")
            def compile_binary(*args):
                args = list(map(str, args))
                output = Path(args[-1])
                thin = []
                for arch in ("arm64", "x86_64"):
                    part = root / (output.name + "." + arch)
                    subprocess.run(["clang", "-arch", arch, *args[:-1], str(part)],
                                   check=True, capture_output=True)
                    thin.append(part)
                subprocess.run(["lipo", "-create", *map(str, thin), "-output", str(output)],
                               check=True, capture_output=True)
            compile_binary("-dynamiclib", root / "inner.c", "-Wl,-install_name,@rpath/libinner.dylib",
                           "-o", libs / "libinner.dylib")
            compile_binary("-dynamiclib", root / "outer.c", "-L" + str(libs), "-linner",
                           "-Wl,-install_name,@rpath/libouter.dylib", "-Wl,-rpath," + str(libs),
                           "-o", libs / "libouter.dylib")
            for name in ("probe", "utility"):
                compile_binary(root / "main.c", "-L" + str(libs), "-louter", "-Wl,-rpath," + str(libs),
                               "-o", resources / name)
            env = os.environ.copy()
            env["DYLIBBUNDLER_SEARCH_PATHS"] = str(libs)
            result = subprocess.run(["/bin/bash", str(SCRIPT), str(app), str(resources / "probe")],
                                    env=env, capture_output=True, text=True, timeout=60)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertNotIn("no LC_RPATH load command", result.stdout + result.stderr)
            for name in ("probe", "utility", "libinner.dylib", "libouter.dylib"):
                binary = resources / name
                dependencies = subprocess.run(["otool", "-L", str(binary)], check=True,
                                              capture_output=True, text=True).stdout
                self.assertNotIn(str(libs), dependencies)
                if name in ("probe", "utility"):
                    self.assertIn("@executable_path/libouter.dylib", dependencies)
                if name == "libouter.dylib":
                    self.assertIn("@executable_path/libinner.dylib", dependencies)
            shutil.rmtree(libs)
            for name in ("probe", "utility"):
                self.assertEqual(subprocess.run([str(resources / name)], capture_output=True).returncode, 0)

    def test_real_rpath_resolution_and_unresolved_failure(self):
        with tempfile.TemporaryDirectory(prefix="caol real dylib ") as directory:
            root = Path(directory)
            libs = root / "source libs"
            libs.mkdir()
            (root / "lib.c").write_text("int caol_probe(void) { return 42; }\n")
            (root / "main.c").write_text("int caol_probe(void); int main(void) { return caol_probe() != 42; }\n")
            lib = libs / "libcaol_release_probe.dylib"
            subprocess.run(["clang", "-dynamiclib", str(root / "lib.c"),
                            "-Wl,-install_name,@rpath/libcaol_release_probe.dylib", "-o", str(lib)],
                           check=True, capture_output=True)
            env = os.environ.copy()
            env["DYLIBBUNDLER_SEARCH_PATHS"] = str(libs)
            for missing in (False, True):
                app = root / ("missing.app" if missing else "App bundle.app")
                resources = app / "Contents/Resources"
                resources.mkdir(parents=True)
                (app / "Contents/MacOS").mkdir()
                binary = resources / "probe"
                subprocess.run(["clang", str(root / "main.c"), "-L" + str(libs), "-lcaol_release_probe",
                                "-Wl,-rpath,@executable_path", "-o", str(binary)],
                               check=True, capture_output=True)
                if missing:
                    lib.unlink()
                result = subprocess.run(["/bin/bash", str(SCRIPT), str(app), str(binary)],
                                        env=env, capture_output=True, text=True, timeout=30)
                if missing:
                    self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
                else:
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    self.assertTrue((resources / lib.name).is_file())
                    dependencies = subprocess.run(["otool", "-L", str(binary)], check=True,
                                                  capture_output=True, text=True).stdout
                    self.assertIn("@executable_path/" + lib.name, dependencies)
                    self.assertEqual(subprocess.run([str(binary)], capture_output=True).returncode, 0)


if __name__ == "__main__":
    unittest.main()
