#!/usr/bin/env python3
"""Corpus accounting for registry-backed harness scenario declarations."""

from __future__ import annotations

import hashlib
import json
import sys
import unittest
from pathlib import Path


HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

from startup_harness import list_scenarios, scenarios_root  # noqa: E402


class ScenarioRegistryCorpusContractTest(unittest.TestCase):
    def test_talker_selector_declaration_binds_only_its_named_structured_gates(self) -> None:
        path = scenarios_root() / "basecamp.talker_selector_metadata_probe_mcw.json"
        raw = json.loads(path.read_text(encoding="utf-8"))
        labels = {str(step["label"]) for step in raw["steps"]}
        proof_route = raw["proof_route"]

        self.assertEqual(raw["manifest_version"], 1)
        self.assertEqual(raw["runtime_contract"]["fixture"], raw["fixture"])
        self.assertEqual(raw["runtime_contract"]["profile"], raw["profile"])
        self.assertEqual(
            proof_route["capability_gates"]["actors.katharina_leach.nearby"],
            {"startup": ["wait_for_gameplay_settle"]},
        )
        self.assertEqual(
            proof_route["capability_gates"]["capabilities.dialogue.talker_selector.visible"],
            {"interaction": ["open_talker_selector"]},
        )
        self.assertEqual(
            proof_route["capability_gates"]["capabilities.dialogue.talker_selector.confirmed_response"],
            {"terminal": ["confirm_highlighted_talker"]},
        )
        terminal_step = next(step for step in raw["steps"] if step["label"] == "confirm_highlighted_talker")
        self.assertEqual(terminal_step["expected_screen_text_after_contains"], ["Your response"])
        self.assertEqual(
            terminal_step["capture_after_crop"],
            {"height": 1650, "width": 1650, "offset_y": 640, "offset_x": 470},
        )
        for role in (
            "precondition",
            "production_behavior",
            "terminal_persistence",
            "artifact_verdict",
            "disallowed_shortcuts",
        ):
            self.assertTrue(set(proof_route[role]) <= labels)

    def test_current_scenario_paths_are_bound_and_legacy_unknowns_are_explicit(self) -> None:
        paths = sorted(scenarios_root().glob("*.json"), key=lambda path: path.name.lower())
        source_before = {path.resolve(): path.read_bytes() for path in paths}

        listed = list_scenarios()

        source_after = {path.resolve(): path.read_bytes() for path in paths}
        self.assertEqual(source_after, source_before, "listing/validation must not rewrite declarations")

        expected_paths = set(source_before)
        entries_by_path = {
            Path(str(entry["scenario_manifest"]["source"]["path"])).resolve(): entry
            for entry in listed
        }
        self.assertEqual(set(entries_by_path), expected_paths)

        for path, source_bytes in source_before.items():
            with self.subTest(path=path.name):
                entry = entries_by_path[path]
                binding = entry["scenario_manifest"]
                raw = json.loads(source_bytes.decode("utf-8"))

                self.assertEqual(binding["source"]["path"], str(path))
                self.assertEqual(binding["source"]["sha256"], hashlib.sha256(source_bytes).hexdigest())

                normalized = binding["normalized"]
                validation = binding["validation"]
                if "manifest_version" in raw:
                    self.assertEqual(validation["status"], "valid")
                    self.assertFalse(validation["review_required"])
                    for field in ("manifest_version", "capabilities", "runtime_contract", "proof_route"):
                        self.assertEqual(normalized[field]["state"], "declared")
                        self.assertEqual(normalized[field]["value"], raw[field])
                    continue

                self.assertEqual(validation["status"], "review_required")
                self.assertTrue(validation["review_required"])
                for field in ("manifest_version", "capabilities", "runtime_contract", "proof_route"):
                    field_binding = normalized[field]
                    self.assertTrue(field_binding["review_required"])
                    if field in raw:
                        self.assertEqual(field_binding["state"], "declared_unversioned")
                        self.assertEqual(field_binding["value"], raw[field])
                    else:
                        self.assertEqual(field_binding["state"], "unknown")
                        self.assertIsNone(field_binding["value"])

    def test_cannibal_dispatch_fixture_has_idle_observed_footing(self) -> None:
        cannibal = json.loads((scenarios_root() / "cannibal.live_world_night_local_contact_pack_mcw.json").read_text())
        cannibal_fixture = json.loads((
            HARNESS_DIR / "fixtures" / "saves" / "live-debug"
            / "cannibal_live_world_night_local_contact_pack_v0_2026-04-28" / "manifest.json"
        ).read_text())

        self.assertEqual(cannibal["manifest_version"], 2)
        self.assertEqual(cannibal["run_class"], "combat")
        self.assertFalse(cannibal["observer_character"])
        self.assertNotIn("status", cannibal)
        self.assertNotIn("blocked_reason", cannibal)
        cannibal_preflight = next(
            step for step in cannibal["steps"]
            if step["label"] == "preflight_observed_idle_cannibal_dispatch_footing"
        )
        self.assertEqual(cannibal_preflight["required_profile"], "cannibal_camp")
        self.assertEqual(cannibal_preflight["required_member_count"], 2)
        self.assertEqual(cannibal_preflight["required_active_outside_count"], 0)
        self.assertEqual(
            [(transform["kind"], transform.get("site_id", transform.get("new_site_id", "")))
             for transform in cannibal_fixture["save_transforms"]],
            [
                ("game_turn", ""),
                ("seed_overmap_special_near_player", ""),
                ("map_fields_near_player", ""),
                ("bandit_clone_site", "overmap_special:cannibal_camp@140,51,0"),
                ("bandit_site_roster_shape", "overmap_special:bandit_camp@140,51,0"),
                ("bandit_site_roster_shape", "overmap_special:cannibal_camp@140,51,0"),
                ("bandit_clear_site_evidence", "overmap_special:cannibal_camp@140,51,0"),
                ("bandit_camp_map_lead", "overmap_special:cannibal_camp@140,51,0"),
                ("player_mutations", ""),
            ],
        )
        disturbance = cannibal_fixture["save_transforms"][2]
        cannibal_clone = cannibal_fixture["save_transforms"][3]
        cannibal_roster = cannibal_fixture["save_transforms"][5]
        self.assertEqual(
            [field["field_id"] for field in disturbance["fields"]],
            ["fd_fire", "fd_smoke"],
        )
        self.assertEqual(cannibal_clone["new_hostile_profile"], "cannibal_camp")
        self.assertEqual(cannibal_clone["new_anchor"], [140, 51, 0])
        self.assertEqual(cannibal_roster["living_member_count"], 2)
        self.assertEqual(cannibal_roster["active_outside_member_count"], 0)

    def test_r002_m040_post_abort_recenter_continuation_is_noncredit_and_source_bound(self) -> None:
        path = scenarios_root() / "bandit.r002_m040_post_abort_recenter_return_mcw.json"
        raw = json.loads(path.read_text(encoding="utf-8"))
        labels = [step["label"] for step in raw["steps"]]

        self.assertEqual(raw["manifest_version"], 2)
        self.assertEqual(
            raw["fixture"],
            "bandit_r002_m040_post_abort_recenter_return_v0_2026-08-22",
        )
        self.assertFalse(raw["runtime_contract"]["grants_gameplay_proof"])
        self.assertEqual(
            raw["runtime_contract"]["forbidden_input"],
            ["debug:ecology_edit", "debug:inject_report", "debug:inject_decision",
             "debug:inject_operation", "debug:spawn_npc", "raw-save-rewrite",
             "fixture-save-transform-after-install"],
        )
        self.assertLess(labels.index("preflight_exact_post_abort_pair"),
                        labels.index("load_zero_slot_abort_resume_omt"))
        self.assertLess(labels.index("accept_zero_slot_abort_resume_route"),
                        labels.index("audit_recenter_gate_and_local_rebind"))
        self.assertLess(labels.index("audit_recenter_gate_and_local_rebind"),
                        labels.index("audit_ordered_physical_return_receipts"))
        self.assertLess(labels.index("audit_ordered_physical_return_receipts"),
                        labels.index("audit_saved_pair_home_after_recenter"))

        preflight = next(step for step in raw["steps"]
                         if step["label"] == "preflight_exact_post_abort_pair")
        self.assertEqual(preflight["required_active_outing_handoff_epoch"], 2)
        self.assertTrue(preflight["required_local_handoff_cohesion_abort_return"])
        self.assertFalse(preflight["required_local_handoff_cohesion_assembled"])
        self.assertEqual(preflight["required_local_handoff_route_position"], [164, 36, 0])
        route = next(step for step in raw["steps"]
                     if step["label"] == "load_zero_slot_abort_resume_omt")
        self.assertEqual(route["origin_omt"], [161, 36, 0])
        self.assertEqual(route["destination_omt"], [164, 36, 0])
        self.assertEqual(route["cursor_keys"], ["right", "right", "right"])


if __name__ == "__main__":
    unittest.main()
