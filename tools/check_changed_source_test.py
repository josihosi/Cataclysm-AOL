"""Focused integration checks for tools/check_changed_source.py."""

import contextlib
import hashlib
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

import tools.check_changed_source as lint

ROOT = Path(__file__).resolve().parent.parent
ENTRY = ROOT / "tools/check_changed_source.py"
SCENARIOS = ROOT / "tools/openclaw_harness/scenarios"
REGISTRY_SOURCE = ROOT / "tools/openclaw_harness/scenario_registry.py"


class ChangedSourceEntryPointTest(unittest.TestCase):
    def test_changed_cpp_line_filter_uses_diff_hunks_and_full_untracked_files(self):
        with tempfile.TemporaryDirectory(prefix="changed-source-line-filter-") as temp:
            root = Path(temp).resolve()
            (root / "src").mkdir()
            (root / "src/changed.cpp").write_text("one\ntwo\nthree\nfour\n", encoding="utf-8")
            (root / "src/new.cpp").write_text("new\nfile\n", encoding="utf-8")
            tracked = subprocess.CompletedProcess([], 0, "src/changed.cpp\n", "")
            diff = subprocess.CompletedProcess(
                [], 0,
                "@@ -1 +1 @@\n-one\n+ONE\n@@ -4,0 +5 @@\n+five\n",
                "",
            )
            untracked = subprocess.CompletedProcess([], 1, "", "")
            with mock.patch.object(lint, "run", side_effect=[tracked, diff, untracked]):
                result, error = lint.changed_cpp_line_filter(
                    "HEAD", ["src/changed.cpp", "src/new.cpp"], root=root
                )
            self.assertIsNone(error)
            self.assertEqual(result, [
                {"name": str(root / "src/changed.cpp"), "lines": [[1, 1], [5, 5]]},
                {"name": str(root / "src/new.cpp"), "lines": [[1, 2]]},
            ])

    def test_entrypoint_keeps_diff_failure_incomplete_after_full_fallback_passes(self):
        with tempfile.TemporaryDirectory(prefix="changed-source-no-line-diff-") as temp:
            root = Path(temp).resolve()
            (root / "src").mkdir()
            (root / "src/alpha.cpp").write_text("int alpha() { return 0; }\n", encoding="utf-8")
            output = io.StringIO()
            with mock.patch.object(lint, "ROOT", root), \
                    mock.patch.object(lint, "changed_from_git", return_value=["src/alpha.cpp"]), \
                    mock.patch.object(lint, "run_astyle", return_value=0), \
                    mock.patch.object(lint, "changed_cpp_line_filter", return_value=(None, "diff unavailable")), \
                    mock.patch.object(lint, "run_cpp_analysis", return_value=0) as analyze, \
                    contextlib.redirect_stdout(output):
                status = lint.main([])
            self.assertEqual(status, 2)
            self.assertIn("INCOMPLETE: changed-line filter unavailable: diff unavailable", output.getvalue())
            self.assertIn("RESULT: incomplete coverage", output.getvalue())
            analyze.assert_called_once_with(["src/alpha.cpp"], line_filter=None, compact=True)

    def test_compact_clang_tidy_output_deduplicates_and_keeps_full_log(self):
        with tempfile.TemporaryDirectory(prefix="changed-source-compact-") as temp:
            log = Path(temp) / "tidy.log"
            output = io.StringIO()
            diagnostic = "src/alpha.cpp:8:3: error: avoid auto [cata-almost-never-auto,-warnings-as-errors]\n"
            with contextlib.redirect_stdout(output):
                lint.compact_clang_tidy_output([
                    ("src/alpha.cpp", 1, diagnostic),
                    ("tests/smoke.cpp", 1, "../src/alpha.cpp:8:3: error: avoid auto [cata-almost-never-auto,-warnings-as-errors]\n"),
                ], log)
            self.assertEqual(log.read_text(encoding="utf-8").count("avoid auto"), 2)
            self.assertIn("2 occurrence(s), 1 unique", output.getvalue())
            self.assertIn("src/alpha.cpp:8 [cata-almost-never-auto] avoid auto (2 TU reports)", output.getvalue())

    def test_git_changed_path_traversal_uses_nul_delimiters(self):
        results = [
            subprocess.CompletedProcess([], 0, b"tools/changed.py\0tools/line\nbreak.py\0", b""),
            subprocess.CompletedProcess([], 0, b"tools/new_module.py\0.build/generated.py\0", b""),
        ]
        with mock.patch.object(lint, "run", side_effect=results) as mocked:
            paths = lint.changed_from_git("HEAD")
        self.assertEqual(paths, [".build/generated.py", "tools/changed.py", "tools/line\nbreak.py", "tools/new_module.py"])
        self.assertEqual(mocked.call_count, 2)
        self.assertIn("-z", mocked.call_args_list[0].args[0])
        self.assertIn("-z", mocked.call_args_list[1].args[0])
        self.assertFalse(mocked.call_args_list[0].kwargs["text"])

    def test_real_git_changed_python_traversal_and_generated_exclusion(self):
        with tempfile.TemporaryDirectory(prefix="changed-source-git-") as temp:
            root = Path(temp).resolve()
            (root / "tools").mkdir()
            tracked = root / "tools/tracked.py"
            unusual = root / "tools/line\nbreak.py"
            tracked.write_text("value = 1\n", encoding="utf-8")
            unusual.write_text("value = 1\n", encoding="utf-8")

            def git(*args):
                result = subprocess.run(
                    ["git"] + list(args), cwd=str(root), stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT, text=True,
                )
                self.assertEqual(result.returncode, 0, result.stdout)

            git("init", "-q")
            git("config", "user.name", "Lint Test")
            git("config", "user.email", "lint-test@example.invalid")
            git("add", "--", "tools")
            git("commit", "-qm", "baseline")
            tracked.write_text("value = 2\n", encoding="utf-8")
            unusual.write_text("value = 2\n", encoding="utf-8")
            (root / "tools/new.py").write_text("value = 3\n", encoding="utf-8")
            (root / ".build").mkdir()
            (root / ".build/generated.py").write_text("value = 4\n", encoding="utf-8")

            def local_git(command, cwd=lint.ROOT, env=None, text=True):
                return subprocess.run(
                    command, cwd=str(root), env=env, text=text,
                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                )

            with mock.patch.object(lint, "ROOT", root), mock.patch.object(lint, "run", side_effect=local_git):
                selected_raw = lint.changed_from_git("HEAD")
                selected, incomplete = lint.normalize_paths(selected_raw, root=root)

            self.assertFalse(incomplete)
            self.assertEqual(selected, ["tools/line\nbreak.py", "tools/new.py", "tools/tracked.py"])

    def test_python_classification_and_runtime_exclusions(self):
        selected, incomplete = lint.normalize_paths([
            "tools/openclaw_harness/scenario_registry_cli.py",
            "build/generated.py",
            ".userdata/private.py",
            ".build/generated.py",
            "tools/openclaw_harness/build_logs/run.py",
            ".env/private.py",
            "src/generated/cache.py",
        ])
        self.assertFalse(incomplete)
        self.assertEqual(selected, ["tools/openclaw_harness/scenario_registry_cli.py"])
        self.assertEqual(lint.source_classifier(selected[0]), "python")

    def test_affected_header_selects_source_and_test_tus_and_flags_stale_scan(self):
        with tempfile.TemporaryDirectory(prefix="changed-source-deps-") as temp:
            root = Path(temp)
            (root / "src").mkdir()
            (root / "tests").mkdir()
            (root / "obj").mkdir()
            (root / "tests/obj").mkdir()
            header = root / "src/alpha.h"
            new_header = root / "src/new_header.h"
            header.write_text("// header fixture\n", encoding="utf-8")
            new_header.write_text("// new header fixture\n", encoding="utf-8")
            src_scan = root / "obj/alpha.inc"
            test_scan = root / "tests/obj/alpha_test.inc"
            src_scan.write_text(". ./src/alpha.h\n", encoding="utf-8")
            test_scan.write_text(".. ../src/alpha.h\n", encoding="utf-8")

            tus, reason = lint.select_cpp_tus(["src/alpha.h", "src/new_header.h"], root=root)
            self.assertEqual(tus, ["src/alpha.cpp", "tests/alpha_test.cpp"])
            self.assertIn("src/new_header.h", reason)
            self.assertIn("absent from existing include scan", reason)

            output = io.StringIO()
            with mock.patch.object(lint, "ROOT", root), mock.patch.object(lint, "run_astyle", return_value=0), \
                    contextlib.redirect_stdout(output):
                status = lint.main(["--files", "src/alpha.h", "src/new_header.h"])
            self.assertEqual(status, 2)
            self.assertIn("src/new_header.h", output.getvalue())
            self.assertIn("RESULT: incomplete coverage", output.getvalue())

            src_scan.write_text(". ./src/alpha.h\n. ./src/new_header.h\n", encoding="utf-8")
            fixed_tus, fixed_reason = lint.select_cpp_tus(
                ["src/alpha.h", "src/new_header.h"], root=root
            )
            self.assertIsNone(fixed_reason)
            self.assertEqual(fixed_tus, tus)
            fixed_output = io.StringIO()
            with mock.patch.object(lint, "ROOT", root), mock.patch.object(lint, "run_astyle", return_value=0), \
                    contextlib.redirect_stdout(fixed_output):
                fixed_status = lint.main(["--files", "src/alpha.h", "src/new_header.h"])
            self.assertEqual(fixed_status, 2)
            self.assertNotIn("absent from existing include scan", fixed_output.getvalue())
            self.assertIn("src/alpha.cpp, tests/alpha_test.cpp", fixed_output.getvalue())

            tus, reason = lint.select_cpp_tus(["src/alpha.h"], root=root)
            self.assertIsNone(reason)
            self.assertEqual(tus, fixed_tus)

            future = max(src_scan.stat().st_mtime, test_scan.stat().st_mtime) + 10
            os.utime(header, (future, future))
            stale_tus, stale_reason = lint.select_cpp_tus(["src/alpha.h"], root=root)
            self.assertEqual(stale_tus, tus)
            self.assertIn("scan predates src/alpha.h", stale_reason)
            newline_tus, newline_reason = lint.select_cpp_tus(["src/line\nbreak.h"], root=root)
            self.assertEqual(newline_tus, [])
            self.assertIn("cannot represent newline paths", newline_reason)

    def test_tiles_only_scan_freshness_and_mixed_layout_ambiguity(self):
        with tempfile.TemporaryDirectory(prefix="changed-source-tiles-") as temp:
            root = Path(temp).resolve()
            (root / "src").mkdir()
            (root / "tests").mkdir()
            (root / "obj/tiles").mkdir(parents=True)
            (root / "tests/obj").mkdir(parents=True)
            header = root / "src/alpha.h"
            header.write_text("// header fixture\n", encoding="utf-8")
            tiles_scan = root / "obj/tiles/alpha.inc"
            test_scan = root / "tests/obj/alpha_test.inc"
            tiles_scan.write_text(". ./src/alpha.h\n", encoding="utf-8")
            test_scan.write_text(".. ../src/alpha.h\n", encoding="utf-8")

            tus, reason = lint.select_cpp_tus(["src/alpha.h"], root=root)
            self.assertIsNone(reason)
            self.assertEqual(tus, ["src/alpha.cpp", "tests/alpha_test.cpp"])

            future = max(tiles_scan.stat().st_mtime, test_scan.stat().st_mtime) + 10
            os.utime(header, (future, future))
            os.utime(test_scan, (future + 10, future + 10))
            _, stale_reason = lint.select_cpp_tus(["src/alpha.h"], root=root)
            self.assertIn("obj/tiles/alpha.inc", stale_reason)
            self.assertIn("scan predates src/alpha.h", stale_reason)

            tiles_scan.write_text(". ./src/other.h\n", encoding="utf-8")
            normal_scan = root / "obj/alpha.inc"
            normal_scan.write_text(". ./src/alpha.h\n", encoding="utf-8")
            os.utime(normal_scan, (future + 10, future + 10))
            _, mixed_reason = lint.select_cpp_tus(["src/alpha.h"], root=root)
            self.assertIn("obj/tiles/alpha.inc", mixed_reason)
            self.assertIn("scan predates src/alpha.h", mixed_reason)

            tiles_scan.write_text(". ./src/alpha.h\n", encoding="utf-8")
            os.utime(tiles_scan, (future + 10, future + 10))
            mixed_tus, mixed_fixed_reason = lint.select_cpp_tus(["src/alpha.h"], root=root)
            self.assertIsNone(mixed_fixed_reason)
            self.assertEqual(mixed_tus, tus)

    def test_changed_header_requires_scans_for_active_compile_database_tus(self):
        with tempfile.TemporaryDirectory(prefix="changed-source-active-tus-") as temp:
            root = Path(temp).resolve()
            for part in ("src", "tests", "obj", "tests/obj", "build"):
                (root / part).mkdir(parents=True)
            header = root / "src/alpha.h"
            header.write_text("// shared header\n", encoding="utf-8")
            for name in ("alpha.cpp", "beta.cpp"):
                (root / "src" / name).write_text("int value() { return 1; }\n", encoding="utf-8")
            (root / "tests/smoke.cpp").write_text("int test() { return 0; }\n", encoding="utf-8")
            alpha_scan = root / "obj/alpha.inc"
            alpha_scan.write_text(". ./src/alpha.h\n", encoding="utf-8")
            test_scan = root / "tests/obj/smoke.inc"
            test_scan.write_text(".. ../tests/smoke.cpp\n", encoding="utf-8")
            database = [
                {"directory": str(root), "file": str(root / source), "command": "clang++ -c"}
                for source in ("src/alpha.cpp", "src/beta.cpp", "tests/smoke.cpp")
            ]
            (root / "build/compile_commands.json").write_text(
                json.dumps(database), encoding="utf-8"
            )

            with mock.patch.dict(os.environ, {"CATA_BUILD_DIR": "build"}):
                partial_tus, partial_reason = lint.select_cpp_tus(["src/alpha.h"], root=root)
            self.assertEqual(partial_tus, ["src/alpha.cpp"])
            self.assertIn("src/beta.cpp", partial_reason)
            self.assertIn("lack parser-visible include-scan provenance", partial_reason)

            output = io.StringIO()
            with mock.patch.object(lint, "ROOT", root), \
                    mock.patch.object(lint, "run_astyle", return_value=0), \
                    mock.patch.dict(os.environ, {
                        "CATA_BUILD_DIR": "build",
                        "CATA_CLANG_TIDY": "missing-clang-tidy-22",
                        "CATA_CLANG_TIDY_PLUGIN": str(root / "missing-plugin.so"),
                    }), \
                    contextlib.redirect_stdout(output):
                status = lint.main(["--files", "src/alpha.h"])
            self.assertEqual(status, 2)
            self.assertIn("src/beta.cpp", output.getvalue())
            self.assertIn("RESULT: incomplete coverage", output.getvalue())

            beta_scan = root / "obj/beta.inc"
            beta_scan.write_text(". ./src/alpha.h\n", encoding="utf-8")
            with mock.patch.dict(os.environ, {"CATA_BUILD_DIR": "build"}):
                populated_tus, populated_reason = lint.select_cpp_tus(["src/alpha.h"], root=root)
            self.assertIsNone(populated_reason)
            self.assertEqual(populated_tus, ["src/alpha.cpp", "src/beta.cpp"])

    def test_c_compile_database_map_is_resolved_to_the_c_translation_unit(self):
        with tempfile.TemporaryDirectory(prefix="changed-source-c-scan-") as temp:
            root = Path(temp).resolve()
            for part in (
                "src", "tests", "obj", "tests/obj", "build", "build-scripts",
            ):
                (root / part).mkdir(parents=True)
            (root / "src/alpha.h").write_text("// shared header\n", encoding="utf-8")
            (root / "src/alpha.cpp").write_text('#include "alpha.h"\n', encoding="utf-8")
            (root / "src/allocator.c").write_text('#include "alpha.h"\n', encoding="utf-8")
            (root / "tests/smoke.cpp").write_text("int test() { return 0; }\n", encoding="utf-8")
            (root / "obj/alpha.inc").write_text(". ./src/alpha.h\n", encoding="utf-8")
            (root / "obj/allocator.inc").write_text(". ./src/alpha.h\n", encoding="utf-8")
            (root / "tests/obj/smoke.inc").write_text(".. ../tests/smoke.cpp\n", encoding="utf-8")
            database = [
                {"directory": str(root), "file": str(root / source), "command": "clang -c"}
                for source in ("src/alpha.cpp", "src/allocator.c", "tests/smoke.cpp")
            ]
            (root / "build/compile_commands.json").write_text(
                json.dumps(database), encoding="utf-8"
            )

            with mock.patch.dict(os.environ, {"CATA_BUILD_DIR": "build"}):
                tus, reason = lint.select_cpp_tus(["src/alpha.h"], root=root)

            self.assertEqual(tus, ["src/allocator.c", "src/alpha.cpp"])
            self.assertIsNone(reason)
            active, mapped, omissions, error = lint.cpp_database_coverage(
                root / "build/compile_commands.json", root
            )
            self.assertIsNone(error)
            self.assertIn("src/allocator.c", active)
            self.assertIn("src/allocator.c", mapped)

            plugin = root / "plugin.so"
            plugin.touch()
            (root / ".clang-tidy").touch()
            wrapper = root / "build-scripts/clang-tidy-wrapper.sh"
            wrapper.write_text("#!/bin/sh\nexit 0\n", encoding="utf-8")
            wrapper.chmod(0o755)
            observed = []
            run_real = lint.run

            def fake_run(command, cwd=None, env=None, **kwargs):
                if command[0] == "bash":
                    observed.append(command)
                    return subprocess.CompletedProcess(command, 0, "", "")
                return run_real(command, cwd=cwd, env=env, **kwargs)

            output = io.StringIO()
            with mock.patch.object(lint, "ROOT", root), \
                    mock.patch.object(lint, "run_astyle", return_value=0), \
                    mock.patch.object(lint.shutil, "which", return_value="/isolated/clang-tidy"), \
                    mock.patch.object(lint, "version_text", return_value="LLVM version 22.1.0"), \
                    mock.patch.object(lint, "run", side_effect=fake_run), \
                    mock.patch.dict(os.environ, {
                        "CATA_BUILD_DIR": "build",
                        "CATA_CLANG_TIDY": "/isolated/clang-tidy",
                        "CATA_CLANG_TIDY_PLUGIN": str(plugin),
                    }), \
                    contextlib.redirect_stdout(output):
                status = lint.main(["--files", "src/allocator.c"])

            self.assertEqual(status, 0)
            self.assertEqual(len(observed), 1)
            self.assertTrue(observed[0][-1].endswith("/src/allocator.c"))
            self.assertIn("Affected translation units from existing include scan: src/allocator.c", output.getvalue())
            self.assertIn("PASS: clang-tidy-22 on selected affected translation units", output.getvalue())

    def test_header_analysis_runs_known_tus_but_reports_utility_and_plugin_gaps(self):
        with tempfile.TemporaryDirectory(prefix="changed-source-cdb-gaps-") as temp:
            root = Path(temp).resolve()
            for part in (
                "src", "tests", "obj", "tests/obj", "build", "build-scripts",
                "tools/format", "src/chkjson", "tools/save", "tools/clang-tidy-plugin",
            ):
                (root / part).mkdir(parents=True, exist_ok=True)
            header = root / "src/alpha.h"
            source = root / "src/alpha.cpp"
            header.write_text("// shared header\n", encoding="utf-8")
            source.write_text('#include "alpha.h"\nint alpha() { return 1; }\n', encoding="utf-8")
            (root / "obj/alpha.inc").write_text(". ./src/alpha.h\n", encoding="utf-8")
            (root / "tests/smoke.cpp").write_text("int smoke() { return 0; }\n", encoding="utf-8")
            (root / "tests/obj/smoke.inc").write_text(".. ../tests/smoke.cpp\n", encoding="utf-8")
            for path in (
                "tools/format/format.cpp", "src/chkjson/chkjson.cpp",
                "tools/save/zzip_main.cpp", "tools/clang-tidy-plugin/Check.cpp",
            ):
                (root / path).write_text("int omitted() { return 0; }\n", encoding="utf-8")
            plugin = root / "plugin.so"
            plugin.touch()
            (root / ".clang-tidy").touch()
            wrapper = root / "build-scripts/clang-tidy-wrapper.sh"
            wrapper.write_text("#!/bin/sh\nexit 0\n", encoding="utf-8")
            wrapper.chmod(0o755)
            database = [{
                "directory": str(root),
                "file": str(source),
                "arguments": ["clang++", "-c", str(source)],
            }]
            (root / "build/compile_commands.json").write_text(
                json.dumps(database), encoding="utf-8"
            )

            observed = {}

            run_real = lint.run

            def fake_run(command, cwd=None, env=None, **kwargs):
                if command[0] == "bash":
                    observed["command"] = command
                    observed["cwd"] = cwd
                    return subprocess.CompletedProcess(command, 0, "", "")
                return run_real(command, cwd=cwd, env=env, **kwargs)

            output = io.StringIO()
            with mock.patch.object(lint, "ROOT", root), \
                    mock.patch.object(lint.shutil, "which", return_value="/isolated/clang-tidy"), \
                    mock.patch.object(lint, "version_text", return_value="LLVM version 22.1.0"), \
                    mock.patch.object(lint, "run", side_effect=fake_run), \
                    mock.patch.dict(os.environ, {
                        "CATA_BUILD_DIR": "build",
                        "CATA_CLANG_TIDY": "/isolated/clang-tidy",
                        "CATA_CLANG_TIDY_PLUGIN": str(plugin),
                    }), \
                    contextlib.redirect_stdout(output):
                status = lint.run_cpp_analysis(
                    ["src/alpha.h"], line_filter=[{"name": str(source), "lines": [[1, 2]]}]
                )

            self.assertEqual(status, 2)
            self.assertIn("src/alpha.cpp", observed["command"][-1])
            filter_arg = next(arg for arg in observed["command"] if arg.startswith("--line-filter="))
            self.assertEqual(json.loads(filter_arg.split("=", 1)[1])[0]["lines"], [[1, 2]])
            self.assertIn("Makefile direct-link utilities (3 TUs)", output.getvalue())
            self.assertIn("standalone clang-tidy plugin (1 TU)", output.getvalue())
            self.assertIn("Affected-header coverage remains incomplete", output.getvalue())
            self.assertIn("INCOMPLETE: changed-header coverage incomplete", output.getvalue())

    def test_existing_clang_tidy_wrapper_forwards_build_dir_and_translation_unit(self):
        with tempfile.TemporaryDirectory(prefix="changed-source-wrapper-") as temp:
            root = Path(temp)
            stub = root / "clang-tidy-22-stub"
            stub.write_text("#!/bin/sh\nprintf '%s\\n' \"$@\"\n", encoding="utf-8")
            stub.chmod(0o755)
            build_dir = root / "analysis-build"
            env = os.environ.copy()
            env["CATA_BUILD_DIR"] = str(build_dir)
            env["CATA_CLANG_TIDY"] = str(stub)
            plugin = root / "isolated/libCataAnalyzerPlugin.so"
            plugin.parent.mkdir(parents=True)
            plugin.touch()
            env["CATA_CLANG_TIDY_PLUGIN"] = str(plugin)
            result = subprocess.run(
                ["bash", str(ROOT / "build-scripts/clang-tidy-wrapper.sh"),
                 "-p", str(build_dir), str(root / "src/alpha.cpp")],
                cwd=str(root), env=env, stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT, text=True,
            )
            self.assertEqual(result.returncode, 0, result.stdout)
            self.assertIn("--load={}".format(plugin), result.stdout)
            self.assertIn(str(build_dir), result.stdout)
            self.assertIn(str(root / "src/alpha.cpp"), result.stdout)

    def test_cached_tidy_uses_configured_build_dir_and_wrapper_arguments(self):
        with tempfile.TemporaryDirectory(prefix="changed-source-tidy-") as temp:
            root = Path(temp)
            build_dir = root / "analysis-build"
            plugin = build_dir / "tools/clang-tidy-plugin/libCataAnalyzerPlugin.so"
            plugin.parent.mkdir(parents=True)
            plugin.touch()
            (build_dir / "compile_commands.json").write_text("[]\n", encoding="utf-8")
            (root / ".clang-tidy").touch()
            observed = {}
            fake_run = subprocess.CompletedProcess([], 0, "", "")

            def capture(command, cwd=None, env=None, **kwargs):
                observed["command"] = command
                observed["cwd"] = cwd
                observed["env"] = env
                trace = Path(cwd) / "clang-tidy-trace"
                trace.mkdir()
                (trace / "analysis.json").write_text("{}\n", encoding="utf-8")
                return fake_run

            output = io.StringIO()
            with mock.patch.object(lint, "ROOT", root), \
                    mock.patch.object(lint, "select_cpp_tus", return_value=(["src/alpha.cpp"], None)), \
                    mock.patch.object(lint.shutil, "which", return_value="/isolated/clang-tidy") as which_tool, \
                    mock.patch.object(lint, "version_text", return_value="Debian LLVM version 22.1.0"), \
                    mock.patch.object(lint, "run", side_effect=capture), \
                    mock.patch.dict(os.environ, {
                        "CATA_BUILD_DIR": "analysis-build",
                        "CATA_CLANG_TIDY": "/isolated/clang-tidy",
                        "CATA_CLANG_TIDY_PLUGIN": str(plugin),
                    }), \
                    contextlib.redirect_stdout(output):
                status = lint.run_cpp_analysis(["src/alpha.cpp"])

            self.assertEqual(status, 0)
            which_tool.assert_called_once_with("/isolated/clang-tidy")
            self.assertEqual(observed["command"][0:2], ["bash", str(root / "build-scripts/clang-tidy-wrapper.sh")])
            self.assertEqual(observed["command"][2:4], ["-p", str(build_dir)])
            self.assertEqual(observed["command"][4], str((root / "src/alpha.cpp").resolve()))
            self.assertNotEqual(Path(observed["cwd"]), root)
            self.assertFalse(Path(observed["cwd"]).exists())
            self.assertEqual(observed["env"]["CATA_BUILD_DIR"], str(build_dir))
            self.assertEqual(observed["env"]["CATA_CLANG_TIDY"], "/isolated/clang-tidy")
            self.assertEqual(
                observed["env"]["CATA_CLANG_TIDY_PLUGIN"],
                str(plugin.resolve()),
            )

    def test_custom_clang_tidy_still_requires_llvm_22(self):
        output = io.StringIO()
        with mock.patch.object(lint, "ROOT", Path("/repo")), \
                mock.patch.object(lint, "select_cpp_tus", return_value=([], "include maps missing")), \
                mock.patch.object(lint.shutil, "which", return_value="/isolated/clang-tidy"), \
                mock.patch.object(lint, "version_text", return_value="Debian LLVM version 18.1.8"), \
                mock.patch.dict(os.environ, {"CATA_CLANG_TIDY": "/isolated/clang-tidy"}), \
                contextlib.redirect_stdout(output):
            status = lint.run_cpp_analysis(["src/alpha.cpp"])
        self.assertEqual(status, 2)
        self.assertIn("does not match CI LLVM 22", output.getvalue())

    def test_unclassified_selected_source_is_incomplete(self):
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            status = lint.main(["--files", "README.md"])
        self.assertEqual(status, 2)
        self.assertIn("no lint route selected", output.getvalue())
        self.assertIn("RESULT: incomplete coverage", output.getvalue())

    def test_missing_astyle_configuration_is_incomplete(self):
        with tempfile.TemporaryDirectory(prefix="check-changed-source-config-") as temp:
            output = io.StringIO()
            with mock.patch.object(lint, "ROOT", Path(temp)):
                with contextlib.redirect_stdout(output):
                    status = lint.run_astyle(["src/example.cpp"])
        self.assertEqual(status, 2)
        self.assertIn(".astylerc is missing", output.getvalue())

    def test_missing_flake8_configuration_is_incomplete(self):
        with tempfile.TemporaryDirectory(prefix="check-changed-source-config-") as temp:
            output = io.StringIO()
            with mock.patch.object(lint, "ROOT", Path(temp)):
                with contextlib.redirect_stdout(output):
                    status = lint.run_flake8(["tools/example.py"])
        self.assertEqual(status, 2)
        self.assertIn(".flake8 is missing", output.getvalue())

    def test_missing_flake8_is_incomplete(self):
        with mock.patch.object(lint.shutil, "which", return_value=None):
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                status = lint.run_flake8(["tools/example.py"])
        self.assertEqual(status, 2)
        self.assertIn("flake8 is missing", output.getvalue())

    def test_wrong_flake8_version_is_incomplete(self):
        with tempfile.TemporaryDirectory(prefix="changed-source-flake8-") as temp:
            root = Path(temp)
            (root / ".flake8").touch()
            (root / "requirements-lint.txt").write_text("flake8==7.3.0\n", encoding="utf-8")
            output = io.StringIO()
            with mock.patch.object(lint, "ROOT", root), \
                    mock.patch.object(lint.shutil, "which", return_value="/test/flake8"), \
                    mock.patch.object(lint, "version_text", return_value="7.0.0"), \
                    mock.patch.object(lint, "run") as run_check, contextlib.redirect_stdout(output):
                status = lint.run_flake8(["tools/example.py"])
            self.assertEqual(status, 2)
            self.assertIn("Flake8 must match CI version 7.3.0", output.getvalue())
            run_check.assert_not_called()

    def invoke(self, relative):
        return subprocess.run(
            [sys.executable, str(ENTRY), "--files", relative], cwd=str(ROOT),
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
        )

    def test_declaration_invalid_and_corrected_through_entry_point(self):
        with tempfile.TemporaryDirectory(prefix="check-changed-source-", dir=str(SCENARIOS)) as temp:
            directory = Path(temp)
            invalid = directory / "bad.json"
            valid = directory / "fixed.json"
            legacy = directory / "legacy.json"
            registry_before = hashlib.sha256(REGISTRY_SOURCE.read_bytes()).hexdigest()
            invalid.write_text("[]\n", encoding="utf-8")
            runtime = {
                "permitted_input": [], "forbidden_input": [], "setup_only_debug": False,
                "disposable_copy": True, "helpers": [], "permissions": [], "platform": [],
                "profile": "test", "fixture": "test", "grants_gameplay_proof": False,
                "requirements": {key: None for key in (
                    "os", "source", "executable", "profile", "fixture", "helper",
                    "peekaboo", "input", "ocr", "cleanup",
                )},
            }
            roles = ("precondition", "production_behavior", "terminal_persistence",
                     "artifact_verdict", "disallowed_shortcuts")
            manifest = {
                "manifest_version": 1, "name": "lint.fixture",
                "steps": [{"label": "step", "kind": "press"}],
                "capabilities": {}, "runtime_contract": runtime,
                "proof_route": {role: ["step"] for role in roles},
            }
            valid.write_text(json.dumps(manifest), encoding="utf-8")
            legacy.write_text("{}\n", encoding="utf-8")
            invalid_before = hashlib.sha256(invalid.read_bytes()).hexdigest()
            bad_result = self.invoke(invalid.relative_to(ROOT).as_posix())
            self.assertEqual(bad_result.returncode, 1, bad_result.stdout)
            self.assertIn("[top_level_object]", bad_result.stdout)
            self.assertEqual(hashlib.sha256(invalid.read_bytes()).hexdigest(), invalid_before)
            valid_before = hashlib.sha256(valid.read_bytes()).hexdigest()
            fixed_result = self.invoke(valid.relative_to(ROOT).as_posix())
            self.assertEqual(fixed_result.returncode, 0, fixed_result.stdout)
            self.assertIn("RESULT: full selected coverage passed", fixed_result.stdout)
            self.assertIn("declarations: valid=1 review_required=0 invalid=0", fixed_result.stdout)
            self.assertEqual(hashlib.sha256(valid.read_bytes()).hexdigest(), valid_before)
            legacy_before = hashlib.sha256(legacy.read_bytes()).hexdigest()
            legacy_result = self.invoke(legacy.relative_to(ROOT).as_posix())
            self.assertEqual(legacy_result.returncode, 0, legacy_result.stdout)
            self.assertIn("declarations: valid=0 review_required=1 invalid=0", legacy_result.stdout)
            self.assertEqual(hashlib.sha256(legacy.read_bytes()).hexdigest(), legacy_before)
            self.assertEqual(hashlib.sha256(REGISTRY_SOURCE.read_bytes()).hexdigest(), registry_before)

    @unittest.skipUnless(shutil.which("astyle"), "AStyle is not installed")
    def test_astyle_mismatch_skips_analysis_and_preserves_source(self):
        with tempfile.TemporaryDirectory(prefix="check-changed-source-", dir=str(ROOT / "tools")) as temp:
            fixture = Path(temp) / "format_fixture.cpp"
            fixture.write_text("int main(){return 0;}\n", encoding="utf-8")
            before = hashlib.sha256(fixture.read_bytes()).hexdigest()
            result = self.invoke_external(fixture)
            version = lint.version_text("astyle", ["--version"])
            if version != "Artistic Style Version 3.1":
                self.assertEqual(result.returncode, 2, result.stdout)
                self.assertIn("formatting analysis skipped", result.stdout)
                self.assertNotIn("formatting changes are required", result.stdout)
            else:
                self.assertEqual(result.returncode, 1, result.stdout)
                self.assertIn("formatting changes are required", result.stdout)
            self.assertEqual(hashlib.sha256(fixture.read_bytes()).hexdigest(), before)

    def invoke_external(self, path):
        return subprocess.run(
            [sys.executable, str(ENTRY), "--files", path.relative_to(ROOT).as_posix()], cwd=str(ROOT),
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
        )


if __name__ == "__main__":
    unittest.main()
