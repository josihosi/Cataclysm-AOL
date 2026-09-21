from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import build_source_bound_macos as builder


class BuildSourceBoundMacOSTest(unittest.TestCase):
    def invoke(self, responses: list[object], log_dir: Path, build_prefix: str = "") -> tuple[int, str, str]:
        argv = ["build_source_bound_macos.py", "--log-dir", str(log_dir)]
        if build_prefix:
            argv += ["--build-prefix", build_prefix]
        with patch.object(sys, "argv", argv), patch.object(
            builder.subprocess, "run", side_effect=responses
        ), patch.object(builder, "ROOT", log_dir), patch.object(
            builder.startup_harness, "product_source_binding", return_value={"ok": True, "sha256": "source"}
        ), patch.object(builder.startup_harness, "sha256_file", return_value=("binary", "")), patch.object(
            builder.startup_harness, "current_head_short", return_value="head"
        ), patch.object(builder.startup_harness, "product_build_receipt_path", return_value=log_dir / "receipt.json"
        ), patch("sys.stdout") as stdout, patch("sys.stderr") as stderr:
            status = builder.main()
            output = "".join(call.args[0] for call in stdout.write.call_args_list)
            error = "".join(call.args[0] for call in stderr.write.call_args_list)
            return status, output, error

    def test_version_failure_retains_diagnostic_and_full_log(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            log_dir = Path(raw) / "version"
            result = type("Result", (), {"returncode": 7, "stdout": b"version out\n", "stderr": b"version failed\n"})()
            status, _, error = self.invoke([result], log_dir)
            self.assertEqual(status, 7)
            payload = json.loads(error)
            self.assertEqual(payload["phase"], "version")
            self.assertEqual(Path(payload["logs"]["full_log"]).read_bytes(), b"===== stdout =====\nversion out\n\n===== stderr =====\nversion failed\n")

    def test_build_failure_retains_diagnostic_and_exit_status(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            log_dir = Path(raw) / "build"
            version = type("Result", (), {"returncode": 0, "stdout": b"version\n", "stderr": b""})()
            build = type("Result", (), {"returncode": 9, "stdout": b"compile out\n", "stderr": b"linker failed\n"})()
            status, _, error = self.invoke([version, build], log_dir)
            self.assertEqual(status, 9)
            payload = json.loads(error)
            self.assertEqual(payload["phase"], "build")
            self.assertEqual(payload["diagnostic"], "linker failed")
            self.assertTrue(Path(payload["logs"]["stdout_log"]).exists())

    def test_success_receipt_contains_both_log_handles(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            log_dir = Path(raw) / "success"
            result = type("Result", (), {"returncode": 0, "stdout": b"ok\n", "stderr": b""})()
            status, output, error = self.invoke([result, result], log_dir)
            self.assertEqual(status, 0)
            self.assertEqual(error, "")
            receipt = json.loads(output)["receipt"]
            self.assertEqual(set(receipt["logs"]), {"version", "build"})
            for phase in receipt["logs"].values():
                self.assertTrue(Path(phase["full_log"]).exists())
            self.assertEqual(receipt['build_configuration']['renderer'], 'tiles')
            self.assertIn('TILES=1', receipt['build_configuration']['make_variables'])

    def test_incompatible_owned_pch_is_refreshed_once_before_the_same_build_retries(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            log_dir = Path(raw) / 'refresh'
            stale_pch = log_dir / 'obj/tiles/pch/main-pch.hpp.pch'
            stale_pch.parent.mkdir(parents=True)
            stale_pch.write_bytes(b'stale pch')
            stale_dependency = stale_pch.with_suffix('.pch.d')
            stale_dependency.write_text('stale dependency')
            version = type('Result', (), {'returncode': 0, 'stdout': b'version\n', 'stderr': b''})()
            mismatch = type('Result', (), {
                'returncode': 1, 'stdout': b'',
                'stderr': b"error: definition of macro '__OPTIMIZE_SIZE__' differs between the precompiled header ('1') and the command line ('0')\n",
            })()
            retry = type('Result', (), {'returncode': 0, 'stdout': b'rebuilt\n', 'stderr': b''})()
            status, output, error = self.invoke([version, mismatch, retry], log_dir)
            self.assertEqual(status, 0)
            self.assertEqual(error, '')
            receipt = json.loads(output)['receipt']
            recovery = receipt['build_configuration']['pch_recovery']
            self.assertEqual(len(recovery['removed']), 2)
            self.assertEqual(recovery['retry']['exit_status'], 0)
            self.assertFalse(stale_pch.exists())
            self.assertFalse(stale_dependency.exists())

    def test_concatenated_nonempty_prefix_owns_its_exact_pch_and_target(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            log_dir = Path(raw) / 'prefixed-refresh'
            prefix = 'r033-headless-'
            stale_pch = log_dir / 'r033-headless-obj/tiles/pch/main-pch.hpp.pch'
            stale_pch.parent.mkdir(parents=True)
            stale_pch.write_bytes(b'stale prefixed pch')
            stale_dependency = stale_pch.with_suffix('.pch.d')
            stale_dependency.write_text('stale prefixed dependency')
            version = type('Result', (), {'returncode': 0, 'stdout': b'version\n', 'stderr': b''})()
            mismatch = type('Result', (), {
                'returncode': 1, 'stdout': b'',
                'stderr': b'error: incompatible precompiled header\n',
            })()
            retry = type('Result', (), {'returncode': 0, 'stdout': b'rebuilt\n', 'stderr': b''})()
            status, output, error = self.invoke([version, mismatch, retry], log_dir, prefix)
            self.assertEqual(status, 0)
            self.assertEqual(error, '')
            receipt = json.loads(output)['receipt']
            self.assertIn('BUILD_PREFIX=r033-headless-', receipt['command'])
            self.assertEqual(receipt['command'][-1], 'r033-headless-cataclysm-tiles')
            removed = {item['path'] for item in receipt['build_configuration']['pch_recovery']['removed']}
            self.assertEqual(removed, {str(stale_pch), str(stale_dependency)})
            self.assertFalse(stale_pch.exists())
            self.assertFalse(stale_dependency.exists())

    def test_non_pch_build_failure_does_not_delete_the_cache_or_retry(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            log_dir = Path(raw) / 'ordinary-failure'
            pch = log_dir / 'obj/tiles/pch/main-pch.hpp.pch'
            pch.parent.mkdir(parents=True)
            pch.write_bytes(b'compatible cache')
            version = type('Result', (), {'returncode': 0, 'stdout': b'version\n', 'stderr': b''})()
            failed = type('Result', (), {'returncode': 9, 'stdout': b'', 'stderr': b'undefined symbol\n'})()
            status, _, error = self.invoke([version, failed], log_dir)
            self.assertEqual(status, 9)
            self.assertTrue(pch.exists())
            self.assertEqual(json.loads(error)['phase'], 'build')

    def test_receipt_archive_name_is_identity_bound(self) -> None:
        executable = Path("/tmp/cataclysm-tiles")
        first = builder.startup_harness.product_build_receipt_archive_path(
            executable, "a" * 64, "b" * 64, "e" * 64
        )
        second = builder.startup_harness.product_build_receipt_archive_path(
            executable, "c" * 64, "d" * 64, "f" * 64
        )
        self.assertNotEqual(first, second)
        self.assertIn("a" * 64 + "-" + "b" * 64 + "-" + "e" * 64, first.name)
        self.assertIn("c" * 64 + "-" + "d" * 64 + "-" + "f" * 64, second.name)

    def test_lookup_selects_exact_identity_over_newer_unrelated_archive(self) -> None:
        import startup_harness as harness
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            executable = root / "cataclysm-tiles"
            with patch.object(harness, "repo_root", return_value=root):
                old_path = harness.product_build_receipt_archive_path(executable, "a" * 64, "b" * 64, "1" * 64)
                new_path = harness.product_build_receipt_archive_path(executable, "c" * 64, "d" * 64, "2" * 64)
                old_path.parent.mkdir(parents=True)
                common = {"schema": harness.PRODUCT_BUILD_RECEIPT_SCHEMA, "executable_path": str(executable.resolve())}
                old_path.write_text(json.dumps({**common, "executable_sha256": "a" * 64, "product_source_sha256": "b" * 64}), encoding="utf-8")
                new_path.write_text(json.dumps({**common, "executable_sha256": "c" * 64, "product_source_sha256": "d" * 64}), encoding="utf-8")
                found, error = harness._current_product_build_receipt(executable, expected_executable_sha256="a" * 64, expected_product_source_sha256="b" * 64)
                self.assertFalse(error)
                self.assertEqual(found["executable_sha256"], "a" * 64)


if __name__ == "__main__":
    unittest.main()
