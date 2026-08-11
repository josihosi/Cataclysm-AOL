#!/usr/bin/env python3
"""Focused parser/matcher checks for the production route analyzer artifact."""

from __future__ import annotations

import tempfile
import unittest
from pathlib import Path
import sys

HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

from startup_harness import (  # noqa: E402
    audit_structural_route_analyzer,
    parse_structural_route_analyzer_line,
)


class StructuralRouteAnalyzerTests(unittest.TestCase):
    def test_parser_keeps_selected_and_rejected_identity(self) -> None:
        selected = parse_structural_route_analyzer_line(
            "INFO : bandit_live_world structural_route_analyzer site=camp-a "
            "target=(164,30,0) selector=non_frontier outcome=selected watch=(161,30,0) route_cost=14 "
            "summary=live structural route solve accepted"
        )
        rejected = parse_structural_route_analyzer_line(
            "INFO : bandit_live_world structural_route_analyzer site=camp-a "
            "target=(164,30,0) selector=frontier outcome=rejected "
            "summary=live structural route abandoned: no bounded safe watch geography"
        )
        self.assertEqual(selected["outcome"], "selected")
        self.assertEqual(selected["site"], "camp-a")
        self.assertEqual(selected["target"], "(164,30,0)")
        self.assertEqual(selected["selector"], "non_frontier")
        self.assertEqual(rejected["outcome"], "rejected")
        self.assertIsNone(parse_structural_route_analyzer_line(
            "INFO : bandit_live_world structural_route_analyzer site=camp-a "
            "target=(164,30,0) outcome=selected"
        ))

    def test_audit_matches_both_outcomes_after_log_normalization(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            log = root / "debug.log"
            log.write_text(
                "INFO : ignored startup line\n"
                "INFO : bandit_live_world structural_route_analyzer site=camp-a "
                "target=(164,30,0) selector=non_frontier outcome=selected watch=(161,30,0) route_cost=14 "
                "summary=live structural route solve accepted\n"
                "INFO : bandit_live_world structural_route_analyzer site=camp-a "
                "target=(164,30,0) selector=frontier outcome=rejected "
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
                "target=(164,30,0) selector=non_frontier outcome=selected\n"
                "INFO : watch=(161,30,0) route_cost=14\n"
                "INFO : summary=live structural route solve accepted\n"
                "INFO : bandit_live_world structural_route_analyzer site=camp-a "
                "target=(164,30,0) selector=frontier outcome=rejected\n"
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


if __name__ == "__main__":
    unittest.main()
