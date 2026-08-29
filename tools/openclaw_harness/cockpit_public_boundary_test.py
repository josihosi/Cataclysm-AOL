#!/usr/bin/env python3
"""Public call/result transcripts for the stateless cockpit boundary."""

from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

import cockpit  # noqa: E402


class CockpitPublicBoundaryTest(unittest.TestCase):
    def test_frontier_and_unsupported_fields_are_compact_public_transcripts(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            service = cockpit.CockpitService(str(Path(directory) / "registry.sqlite3"))
            frontier = service.call({"action": "frontier"})
            unsupported = service.call({"action": "frontier", "pid": 42})
        self.assertEqual(frontier["ok"], True)
        self.assertEqual(frontier["result"]["kind"], "frontier")
        self.assertEqual(set(frontier["result"]), {"kind", "count", "scenarios"})
        self.assertEqual(unsupported, {"ok": False, "error": "unsupported fields", "fields": ["pid"]})
        self._record("frontier", frontier)
        self._record("unsupported", unsupported)

    def test_capability_and_scenario_search_detail_hide_internal_fields_recursively(self) -> None:
        candidate = SimpleNamespace(
            scenario_id="demo.scenario", lifecycle_state="run-verified",
            facts={"capability": {"value": True, "token": "secret", "nested": {"offset": 9}},
                   "physical_key": "secret-key"},
            explanation={"summary": "visible", "token_id": "secret", "details": {"pid": 7}},
        )
        evaluation = SimpleNamespace(candidates=[candidate])
        execution = SimpleNamespace(evaluation=evaluation)
        fake_open = __import__("contextlib").nullcontext(object())
        transcripts = {}
        with patch.object(cockpit, "open_registry", return_value=fake_open), \
                patch.object(cockpit, "execute_registry_query", return_value=execution):
            for label, request in {
                "capability": {"action": "capability_search", "detail": True},
                "scenario": {"action": "scenario_search", "detail": True},
            }.items():
                transcripts[label] = cockpit.CockpitService().call(request)
                result = transcripts[label]["result"]
                self.assertTrue(result["matches"])
                self.assertIn("detail", result)
                encoded = json.dumps(result, sort_keys=True).lower()
                for forbidden in ("token", "offset", "pid", "physical_key", "ocr", "raw_logs", "executable_path", "manifest_sha256"):
                    self.assertNotIn(forbidden, encoded)
        self._record("search_detail", transcripts)

    def test_recursive_public_projection_and_protected_llm_boundary(self) -> None:
        value = {"safe": [{"token": "x", "candidate_offsets": [[1, 2]], "key": "x", "subprocess": "x",
                            "inner": {"logs": "x", "name": "ok", "candidate_offsets": [3], "key": "x", "subprocess": "x"}}],
                 "source_path": "/private", "manual": "full manual", "raw_logs": "raw"}
        public = cockpit._public(value)
        self.assertEqual(public, {"safe": [{"inner": {"name": "ok"}}]})
        source = (HARNESS_DIR / "cockpit.py").read_text(encoding="utf-8")
        self.assertNotIn("llm_intent", source)
        self._record("projection", public)

    def _record(self, name: str, value: object) -> None:
        target = HARNESS_DIR.parents[1] / ".userdata" / "openclaw_harness" / "cockpit_public_boundary_transcript.jsonl"
        target.parent.mkdir(parents=True, exist_ok=True)
        with target.open("a", encoding="utf-8") as stream:
            stream.write(json.dumps({"case": name, "result": value}, sort_keys=True) + "\n")


if __name__ == "__main__":
    unittest.main()
