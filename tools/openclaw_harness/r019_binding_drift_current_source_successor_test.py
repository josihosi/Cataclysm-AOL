#!/usr/bin/env python3
"""Keep the quarantined R-019 drift route separate from its fresh successor."""

from __future__ import annotations

import json
import unittest
from pathlib import Path


HARNESS_DIR = Path(__file__).resolve().parent


class R019BindingDriftCurrentSourceSuccessorTest(unittest.TestCase):
    def test_stale_old_and_current_new_controls_have_distinct_authority(self) -> None:
        old = json.loads((HARNESS_DIR / "scenarios" / "r019.keep_watch_off_binding_drift_control_mcw.json").read_text())
        current = json.loads((HARNESS_DIR / "scenarios" / "r019.keep_watch_off_binding_drift_current_source_control_mcw.json").read_text())

        self.assertNotEqual(old["name"], current["name"])
        self.assertEqual(
            current["capabilities"]["capabilities.r019.off_binding_drift_current_source_control"],
            "primitive_continuation_fail_closed_on_runtime_binding_drift",
        )
        self.assertEqual(current["runtime_contract"]["grants_gameplay_proof"], False)
        self.assertIn("cockpit:run.controlled_binding_drift", current["runtime_contract"]["permitted_input"])
        session = current["steps"][-1]
        self.assertEqual(session["label"], "r019_off_binding_drift_current_source_live_session")
        self.assertIn("no dispatch after binding drift", session["invariants"])


if __name__ == "__main__":
    unittest.main()
