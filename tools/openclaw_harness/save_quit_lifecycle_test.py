"""Native save completion is the only admission proof for saved-world reentry."""

from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from startup_harness import native_save_quit_completion


def completion(*, saved: bool = True, artifact: bool = True) -> dict:
    return {
        "event": "native_save_completion", "schema": "caol-native-save-completion-v1",
        "run_id": "run-1", "request_id": "save-request-1", "requested_run_id": "run-1",
        "requested_surface_id": "world-1", "requested_frame_id": "frame-1",
        "action_id": "world.save_quit", "serializer_result": "saved" if saved else "failed_io_exception",
        "save_succeeded": saved, "world_name": "McWilliams", "player_save_id": "Ada",
        "artifact_identity": {
            "kind": "harness_run_directory", "value": "/run-1" if artifact else "",
        },
    }


class SaveQuitLifecycleTest(unittest.TestCase):
    def read(self, *events: dict) -> dict:
        with tempfile.TemporaryDirectory() as directory:
            trace = Path(directory) / "semantic.native.log"
            trace.write_text("".join(
                "openclaw_harness_semantic_step: " + json.dumps(event) + "\n"
                for event in events
            ), encoding="utf-8")
            return native_save_quit_completion(
                trace, 0, run_id="run-1", expected_world="McWilliams",
            )

    def test_only_exact_native_serializer_success_admits_reentry(self) -> None:
        result = self.read(completion())
        self.assertEqual(result["status"], "matched")
        self.assertEqual(result["completion"]["request_id"], "save-request-1")

    def test_failed_save_and_missing_artifact_cannot_be_promoted_by_exit(self) -> None:
        self.assertEqual(self.read(completion(saved=False))["status"], "failed")
        self.assertEqual(self.read(completion(artifact=False))["status"], "malformed")

    def test_native_handoff_is_cross_scope_single_use_and_cleared_before_prompt(self) -> None:
        source = (Path(__file__).resolve().parents[2] / "src" / "handle_action.cpp").read_text(
            encoding="utf-8"
        )
        handoff = "openclaw_harness_pending_save_quit_request"
        self.assertLess(source.index("static std::optional<semantic_action_request> " + handoff),
                        source.index("bool game::do_regular_action"))
        save_case = source[source.index("case ACTION_SAVE:"):source.index("case ACTION_QUICKSAVE:")]
        self.assertIn("std::move( " + handoff + " )", save_case)
        self.assertLess(save_case.index(handoff + ".reset()"), save_case.index('query_yn( _( "Save and quit?" )'))
        self.assertIn("const bool saved = save();", save_case)
        self.assertIn("openclaw_harness_semantic_save_quit_completion", save_case)


if __name__ == "__main__":
    unittest.main()
