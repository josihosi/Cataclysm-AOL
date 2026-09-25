from __future__ import annotations

import contextlib
import io
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch


SCRIPTS = Path(__file__).resolve().parents[1] / "scripts"
sys.path.insert(0, str(SCRIPTS))

from blocker_adapter import (  # noqa: E402
    BlockerAdapterError,
    SubprocessBlockerAdapter,
    parse_adapter_command,
    safe_wait_for_reply,
)


class BlockerAdapterTests(unittest.TestCase):
    def test_core_adapter_seam_has_no_openclaw_or_discord_dependency(self) -> None:
        core = (SCRIPTS / "blocker_adapter.py").read_text(encoding="utf-8").lower()
        supervisor = (SCRIPTS / "coordinator_supervisor.py").read_text(
            encoding="utf-8"
        ).lower()
        self.assertNotIn("openclaw", core + supervisor)
        self.assertNotIn("discord", core + supervisor)

    def test_command_is_a_shell_independent_json_array(self) -> None:
        self.assertEqual(
            parse_adapter_command('["python3", "adapter.py", "--flag"]'),
            ("python3", "adapter.py", "--flag"),
        )
        for encoded in ("no", "{}", "[]", '["python3", ""]'):
            with self.subTest(encoded=encoded):
                with self.assertRaises(BlockerAdapterError):
                    parse_adapter_command(encoded)

    def test_subprocess_receives_exact_workspace_and_lineage_arguments(self) -> None:
        reply = {
            "blocker_digest": "digest",
            "notification_message_id": "notice",
            "reply_message_id": "reply",
            "reply_text": "Continue.",
            "author_id": "owner",
        }
        completed = subprocess.CompletedProcess([], 0, json.dumps(reply), "")
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            with patch("blocker_adapter.subprocess.run", return_value=completed) as run:
                result = SubprocessBlockerAdapter(("python3", "adapter.py")).wait_for_reply(
                    workspace=workspace,
                    lineage_id="project",
                )
        self.assertEqual(result.reply_message_id if result else None, "reply")
        self.assertEqual(
            run.call_args.args[0],
            [
                "python3", "adapter.py", "wait", "--workspace", str(workspace),
                "--lineage", "project",
            ],
        )
        self.assertNotIn("shell", run.call_args.kwargs)

    def test_missing_or_malformed_optional_adapter_returns_no_reply(self) -> None:
        adapter = SubprocessBlockerAdapter(("missing-adapter",))
        with tempfile.TemporaryDirectory() as directory:
            for error in (
                FileNotFoundError("missing"),
                subprocess.CalledProcessError(
                    2, ["adapter"], stderr="adapter transport unavailable\n"
                ),
            ):
                with self.subTest(error=type(error).__name__):
                    with patch("blocker_adapter.subprocess.run", side_effect=error):
                        with contextlib.redirect_stderr(io.StringIO()) as stderr:
                            self.assertIsNone(
                                safe_wait_for_reply(
                                    adapter,
                                    workspace=Path(directory),
                                    lineage_id="project",
                                )
                            )
                    self.assertIn("optional blocker adapter unavailable", stderr.getvalue())
                    if isinstance(error, subprocess.CalledProcessError):
                        self.assertIn("adapter transport unavailable", stderr.getvalue())

        completed = subprocess.CompletedProcess([], 0, "not-json", "")
        with tempfile.TemporaryDirectory() as directory:
            with patch("blocker_adapter.subprocess.run", return_value=completed):
                self.assertIsNone(
                    safe_wait_for_reply(
                        adapter,
                        workspace=Path(directory),
                        lineage_id="project",
                    )
                )


if __name__ == "__main__":
    unittest.main()
