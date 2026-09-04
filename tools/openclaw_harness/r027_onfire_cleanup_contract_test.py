#!/usr/bin/env python3
"""Fail-closed contract coverage for the R-027 bootstrap-only cleanup."""

import json
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert( 0, str( ROOT / "tools/openclaw_harness" ) )
from r027_onfire_cleanup import verify


SCENARIO = ROOT / "tools/openclaw_harness/scenarios/bandit.r027_avatar_onfire_cleanup_bootstrap_v001_mcw.json"
SOURCE = ROOT / "src/do_turn.cpp"
GAME_IO = ROOT / "src/game_io.cpp"


class R027OnfireCleanupContractTest(unittest.TestCase):
    def test_manifest_is_explicitly_zero_credit(self) -> None:
        manifest = json.loads(SCENARIO.read_text(encoding="utf-8"))
        self.assertTrue(manifest["r027_onfire_cleanup_bootstrap"])
        self.assertFalse(manifest["runtime_contract"]["grants_gameplay_proof"])
        self.assertTrue(manifest["runtime_contract"]["setup_only_debug"])

    def test_native_operation_is_exactly_scoped_and_non_mutating(self) -> None:
        source = SOURCE.read_text(encoding="utf-8")
        start = source.index("bool openclaw_harness_r027_onfire_cleanup_is_authorized()")
        end = source.index("} // namespace", start)
        operation = source[start:end]
        self.assertIn('"remove_onfire"', operation)
        self.assertIn('"bandit.r027_avatar_onfire_cleanup_bootstrap_v001_mcw"', operation)
        self.assertIn("openclaw_harness_r027_snapshot_request_is_authorized()", operation)
        self.assertIn("u.remove_effect( effect_onfire )", operation)
        for forbidden in ("heal", "mod_part_hp", "setpos", "add_field", "bandit_live_world"):
            self.assertNotIn(forbidden, operation)

    def test_native_quicksave_forces_no_turn_cleanup_to_disk(self) -> None:
        source = GAME_IO.read_text(encoding="utf-8")
        start = source.index("void game::quicksave()")
        end = source.index("void game::quickload()", start)
        operation = source[start:end]
        self.assertIn('"remove_onfire"', operation)
        self.assertIn('"bandit.r027_avatar_onfire_cleanup_bootstrap_v001_mcw"', operation)
        self.assertIn("u.remove_effect( efftype_id( \"onfire\" ) )", operation)
        self.assertIn("!r027_onfire_cleanup", operation)
        for forbidden in ("heal", "mod_part_hp", "setpos", "add_field", "bandit_live_world"):
            self.assertNotIn(forbidden, operation)

    def test_verdict_rejects_any_protected_change(self) -> None:
        before = {
            "avatar": {"effects": [{"id": "onfire", "intensity": 3}], "abs_ms": "(1,1,0)",
                       "abs_omt": "(0,0,0)", "fire_intensity": 0, "body_parts": []},
            "nearby_entities": [], "damaging_fields": [], "saved_source_south_of_avatar": {},
            "fixed_saved_source": {}, "bandit_live_world": {"camps": []},
        }
        after = {**before, "avatar": {**before["avatar"], "effects": []}}
        self.assertTrue(verify(before, after)["accepted"])
        changed = {**after, "bandit_live_world": {"camps": [{"id": "changed"}]}}
        self.assertFalse(verify(before, changed)["accepted"])


if __name__ == "__main__":
    unittest.main()
