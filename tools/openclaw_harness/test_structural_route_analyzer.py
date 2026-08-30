#!/usr/bin/env python3
"""Focused parser/matcher checks for the production route analyzer artifact."""

from __future__ import annotations

import tempfile
import unittest
from pathlib import Path
import sys
import json

HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

from startup_harness import (  # noqa: E402
    audit_structural_route_analyzer,
    parse_structural_route_analyzer_corridor,
    parse_structural_route_analyzer_line,
)
from scenario_registry import validate_manifest  # noqa: E402


class StructuralRouteAnalyzerTests(unittest.TestCase):
    def test_parser_keeps_selected_and_rejected_identity(self) -> None:
        selected = parse_structural_route_analyzer_line(
            "INFO : bandit_live_world structural_route_analyzer site=camp-a "
            "lead=lead-a target=(164,30,0) selector=non_frontier outcome=selected watch=(161,30,0) route_cost=14 "
            "summary=live structural route solve accepted"
        )
        rejected = parse_structural_route_analyzer_line(
            "INFO : bandit_live_world structural_route_analyzer site=camp-a "
            "lead=frontier:0 target=(164,30,0) selector=frontier outcome=rejected "
            "summary=live structural route abandoned: no bounded safe watch geography"
        )
        self.assertEqual(selected["outcome"], "selected")
        self.assertEqual(selected["site"], "camp-a")
        self.assertEqual(selected["lead"], "lead-a")
        self.assertEqual(selected["target"], "(164,30,0)")
        self.assertEqual(selected["selector"], "non_frontier")
        self.assertEqual(rejected["outcome"], "rejected")
        self.assertEqual(rejected["lead"], "frontier:0")
        self.assertIsNone(parse_structural_route_analyzer_line(
            "INFO : bandit_live_world structural_route_analyzer site=camp-a "
            "target=(164,30,0) outcome=selected"
        ))

    def test_parser_accepts_normalized_scheduler_rows_and_rejects_incomplete_selected(self) -> None:
        scheduler_row = parse_structural_route_analyzer_line(
            "INFO : bandit_live_world structural_route_analyzer site=camp-a "
            "lead=lead-a target=(164,30,0) selector=non_frontier outcome=selected "
            "watch=(161,30,0) route_cost=14 "
            "summary=live structural route solve accepted; watch geography selected"
        )
        self.assertEqual(scheduler_row["selector"], "non_frontier")
        self.assertEqual(scheduler_row["watch"], "(161,30,0)")
        self.assertEqual(scheduler_row["route_cost"], "14")
        self.assertIsNone(parse_structural_route_analyzer_line(
            "INFO : bandit_live_world structural_route_analyzer site=camp-a "
            "lead=lead-a target=(164,30,0) selector=non_frontier outcome=selected "
            "summary=live structural route solve accepted"
        ))

    def test_audit_matches_both_outcomes_after_log_normalization(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            log = root / "debug.log"
            log.write_text(
                "INFO : ignored startup line\n"
                "INFO : bandit_live_world structural_route_analyzer site=camp-a "
                "lead=lead-a target=(164,30,0) selector=non_frontier outcome=selected watch=(161,30,0) route_cost=14 "
                "summary=live structural route solve accepted\n"
                "INFO : bandit_live_world structural_route_analyzer site=camp-a "
                "lead=frontier:0 target=(164,30,0) selector=frontier outcome=rejected "
                "summary=live structural route abandoned: no bounded safe watch geography\n",
                encoding="utf-8",
            )
            metadata = audit_structural_route_analyzer(
                root,
                "audit",
                artifact_log=log,
                artifact_baseline=0,
                required_outcomes=["selected", "rejected"],
                required_site="camp-a",
                required_target="(164,30,0)",
            )
            self.assertEqual(metadata["status"], "required_state_present")
            self.assertEqual({entry["outcome"] for entry in metadata["matches"]},
                             {"selected", "rejected"})

    def test_audit_rejects_former_fragmented_multi_record_output(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            log = root / "debug.log"
            log.write_text(
                "INFO : bandit_live_world structural_route_analyzer site=camp-a "
                "lead=lead-a target=(164,30,0) selector=non_frontier outcome=selected\n"
                "INFO : watch=(161,30,0) route_cost=14\n"
                "INFO : summary=live structural route solve accepted\n"
                "INFO : bandit_live_world structural_route_analyzer site=camp-a "
                "lead=frontier:0 target=(164,30,0) selector=frontier outcome=rejected\n"
                "INFO : summary=live structural route abandoned: no bounded safe watch geography\n",
                encoding="utf-8",
            )
            metadata = audit_structural_route_analyzer(
                root,
                "fragmented",
                artifact_log=log,
                artifact_baseline=0,
                required_outcomes=["selected", "rejected"],
                required_site="camp-a",
                required_target="(164,30,0)",
            )
            self.assertEqual(metadata["status"], "required_state_missing")
            self.assertEqual(metadata["missing_required_outcomes"], ["selected", "rejected"])
            self.assertEqual(metadata["matches"], [])

    def test_audit_requires_complete_owner_derived_corridor_without_expected_coordinates(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            log = root / "debug.log"
            log.write_text(
                "INFO : bandit_live_world structural_route_analyzer site=camp-a "
                "lead=lead-a target=(164,30,0) selector=non_frontier outcome=selected "
                "watch=(161,30,0) route_cost=14 "
                "corridor=(160,39,0)>(161,37,0)>(161,30,0)>(161,37,0)>(160,39,0) "
                "summary=live structural route solve accepted\n",
                encoding="utf-8",
            )
            metadata = audit_structural_route_analyzer(
                root,
                "current_world",
                artifact_log=log,
                artifact_baseline=0,
                required_outcomes=["selected"],
                require_complete_corridor=True,
            )
        self.assertEqual(metadata["status"], "required_state_present")
        self.assertEqual(
            metadata["derived_corridors"][0]["corridor"],
            [[160, 39, 0], [161, 37, 0], [161, 30, 0], [161, 37, 0], [160, 39, 0]],
        )
        self.assertIsNone(parse_structural_route_analyzer_corridor("(160,39,0)>(161,30,0)"))

    def test_r005_current_world_observation_declares_source_binding_without_route_coordinates(self) -> None:
        path = HARNESS_DIR / "scenarios" / "bandit.r005_current_world_corridor_observation.json"
        manifest = json.loads(path.read_text(encoding="utf-8"))
        validated = validate_manifest(manifest, path=path)
        serialized = json.dumps(manifest, sort_keys=True)

        self.assertEqual(
            manifest["runtime_contract"]["requirements"]["source"], "current-worktree"
        )
        self.assertFalse(manifest["runtime_contract"]["grants_gameplay_proof"])
        self.assertNotIn("origin_omt", serialized)
        self.assertNotIn("destination_omt", serialized)
        self.assertNotIn("cursor_keys", serialized)
        self.assertEqual(validated["validation"]["status"], "valid")
        self.assertTrue(validated["source"]["sha256"])


if __name__ == "__main__":
    unittest.main()
