from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import build_source_bound_macos as builder


class BuildSourceBoundMacOSTest(unittest.TestCase):
    def invoke(self, responses: list[object], log_dir: Path) -> tuple[int, str, str]:
        argv = ["build_source_bound_macos.py", "--log-dir", str(log_dir)]
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


if __name__ == "__main__":
    unittest.main()
