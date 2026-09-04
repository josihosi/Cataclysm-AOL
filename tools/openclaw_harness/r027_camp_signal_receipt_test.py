#!/usr/bin/env python3
"""Focused rejection tests for R-027's per-read observation receipt."""

from __future__ import annotations

import copy
import json
import tempfile
import unittest
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from r027_camp_signal_receipt import (normalize_aging_receipt, normalize_receipt,
                                      normalize_run_bound_receipts, validate_changed_observation_capture,
                                      validate_unchanged_deduplication)
from startup_harness import (apply_r027_changed_observation_finalization,
                             r027_changed_observation_capture_artifact)


class R027CampSignalReceiptTest(unittest.TestCase):

    def test_capture_waits_for_cleanup_then_projects_green_claim_specific_gate(self):
        scenario = {
            "capabilities": {
                "capabilities.r027.changed_observation_validation":
                "same_saved_lead_same_source_changed_fd_fire_payload",
            },
            "assertions": {
                "stable_source_omt": [140, 50, 0], "stable_lead_id": "lead-a",
            },
        }
        with tempfile.TemporaryDirectory() as temp:
            run_dir = Path(temp)
            (run_dir / "process.json").write_text("{}", encoding="utf-8")
            before_cleanup = r027_changed_observation_capture_artifact(
                scenario=scenario, run_dir=run_dir, runtime_binding={}, events=[], profile="missing",
                world="missing", binding_id="binding-a",
            )
        self.assertEqual(before_cleanup["status"], "red")
        self.assertEqual(before_cleanup["reason"], "r027_cleanup_missing")

        report = {"feature_proof": False, "verdict": "inconclusive"}
        gate = apply_r027_changed_observation_finalization(
            report,
            capture={"status": "green", "artifact": "r027.changed_observation.capture.json",
                     "validation": {"accepted": True},
                     "cleanup": {"status": "terminated"}},
            run_id="run-a", binding_id="binding-a",
        )
        self.assertEqual(gate["status"], "green")
        self.assertEqual(report["structured_gate_evidence"], gate)
        self.assertEqual(report["proof_classification"]["structured_gate_status"], "green")
        self.assertTrue(report["feature_proof"])
        self.assertEqual(report["verdict"], "r027_changed_observation_capture_matched")
        self.assertEqual(report["certification_capture_artifacts"]["artifact"],
                         "r027.changed_observation.capture.json")
    def setUp(self) -> None:
        self.event = {"run_id": "run-1", "sequence": 4, "actor_ids": [4],
                      "transition": "staffed_camp_signal_read",
                      "staffed_camp_signal_read": {
                          "identity": {"run_id": "run-1", "scenario": "r027", "source_sha256": "src",
                                       "executable_sha256": "exe", "binding_id": "binding"},
                          "observer": {"id": "4", "home": "[1,2,0]", "at_home": True,
                                       "eligible": True, "capability": {"sight": 3}},
                          "camp": {"id": "camp-a", "omt": [1, 2, 0]},
                          "channel": {"kind": "smoke"},
                          "source": {"omt": [3, 2, 0], "intensity": 3}, "range": {"actual": 2, "cap": 6},
                          "line_of_sight": {"result": True}, "elevation": {"delta": 0},
                          "weather": {"summary": "clear"},
                          "visibility": {"inputs": {"sight_points": 3}, "result": True},
                          "observation_clocks": {"game_minutes": 220, "emitted_minutes": -1},
                          "lead": {"id": "lead-a"}, "outcome": {"kind": "created"},
                          "work": {"reads": 1, "callbacks": 1},
                          "persistence": {"site_id": "camp-a", "lead_id": "lead-a",
                                          "lead_count_before": 0, "lead_count_after": 1,
                                          "lead_hash_before": "1", "lead_hash_after": "2"},
                          "drive_response": {"before": "idle", "after": "idle"},
                      }}
        self.lease = {"lease_id": "lease-1", "run_id": "run-1", "executable_sha256": "exe",
                      "pid": 44, "profile": "dev-harness", "world": "McWilliams",
                      "cleanup_token": "r027-cleanup-token"}
        self.cleanup = {"lease_id": "lease-1", "pid": 44, "status": "pending",
                        "cleanup_token": "r027-cleanup-token"}

    def receipt(self, event=None, **kwargs):
        return normalize_receipt(event or self.event, run_id="run-1", executable_sha256="exe", pid=44,
                                 profile="dev-harness", world="McWilliams", lease=self.lease,
                                 cleanup=self.cleanup, **kwargs)

    def test_accepts_complete_per_read_receipt(self):
        self.assertTrue(self.receipt()["accepted"])

    def test_accepts_only_complete_run_bound_aging_receipt(self):
        event = {
            "run_id": "run-1", "sequence": 6, "transition": "staffed_camp_signal_aging",
            "staffed_camp_signal_aging": {
                "identity": {"run_id": "run-1", "scenario": "r027", "source_sha256": "src",
                             "executable_sha256": "exe", "binding_id": "binding"},
                "camp": {"id": "camp-a", "omt": "(1,2,0)"},
                "lead": {"id": "lead-a", "channel": "smoke_signal", "source_omt": "(3,2,0)",
                         "source_key": "camp-signal:fire"},
                "previous": {"last_seen_minutes": 0, "age_minutes": 360,
                             "status": "scout_confirmed"},
                "result": {"last_seen_minutes": 0, "age_minutes": 360, "status": "stale",
                           "expired_removed": False},
                "persistence": {"lead_set_hash_before": "before", "lead_set_hash_after": "after"},
                "drive_response": {"before": "idle", "after": "idle"},
            },
        }
        accepted = normalize_aging_receipt(event, run_id="run-1", executable_sha256="exe", pid=44,
                                            profile="dev-harness", world="McWilliams",
                                            lease=self.lease, cleanup=self.cleanup)
        self.assertTrue(accepted["accepted"])
        self.assertEqual(accepted["aging"]["lead"]["channel"], "smoke_signal")
        incomplete = copy.deepcopy(event)
        incomplete["staffed_camp_signal_aging"]["persistence"]["lead_set_hash_after"] = ""
        self.assertEqual(normalize_aging_receipt(
                             incomplete, run_id="run-1", executable_sha256="exe", pid=44,
                             profile="dev-harness", world="McWilliams", lease=self.lease,
                             cleanup=self.cleanup)["reason"], "incomplete_aging_transition")

    def test_rejects_missing_fields_wrong_run_stale_lease_profile_world_and_aggregate(self):
        missing = copy.deepcopy(self.event); del missing["staffed_camp_signal_read"]["lead"]
        self.assertEqual(self.receipt(missing)["reason"], "missing_required_fields")
        wrong_run = copy.deepcopy(self.event); wrong_run["run_id"] = "old"
        self.assertEqual(self.receipt(wrong_run)["reason"], "wrong_or_missing_run")
        lease = dict(self.lease); lease["pid"] = 99
        self.assertEqual(normalize_receipt(self.event, run_id="run-1", executable_sha256="exe", pid=44,
                         profile="dev-harness", world="McWilliams", lease=lease,
                         cleanup=self.cleanup)["reason"], "stale_or_wrong_lease")
        aggregate = {"run_id": "run-1", "transition": "staffed_camp_signal_observation"}
        self.assertEqual(self.receipt(aggregate)["reason"], "aggregate_or_wrong_transition")
        incomplete = copy.deepcopy(self.event); incomplete["staffed_camp_signal_read"]["identity"]["binding_id"] = ""
        self.assertEqual(self.receipt(incomplete)["reason"], "incomplete_production_identity")
        mismatched = copy.deepcopy(self.event); mismatched["staffed_camp_signal_read"]["identity"]["executable_sha256"] = "old"
        self.assertEqual(self.receipt(mismatched)["reason"], "receipt_runtime_identity_mismatch")
        for key, value in (("profile", "other"), ("world", "Other")):
            lease = dict(self.lease); lease[key] = value
            self.assertEqual(normalize_receipt(self.event, run_id="run-1", executable_sha256="exe", pid=44,
                             profile="dev-harness", world="McWilliams", lease=lease,
                             cleanup=self.cleanup)["reason"], "stale_or_wrong_lease")

    def test_rejects_missing_or_mismatched_actor_binding(self):
        missing = copy.deepcopy(self.event); del missing["actor_ids"]
        self.assertEqual(self.receipt(missing)["reason"], "missing_or_mismatched_actor_binding")
        mismatched = copy.deepcopy(self.event); mismatched["actor_ids"] = [99]
        self.assertEqual(self.receipt(mismatched)["reason"], "missing_or_mismatched_actor_binding")

    def test_rejects_incomplete_cleanup(self):
        cleanup = {"lease_id": "lease-1", "pid": 99}
        self.assertEqual(normalize_receipt(self.event, run_id="run-1", executable_sha256="exe", pid=44,
                         profile="dev-harness", world="McWilliams", lease=self.lease,
                         cleanup=cleanup)["reason"], "incomplete_cleanup_binding")
        cleanup = dict(self.cleanup, cleanup_token="wrong")
        self.assertEqual(normalize_receipt(self.event, run_id="run-1", executable_sha256="exe", pid=44,
                         profile="dev-harness", world="McWilliams", lease=self.lease,
                         cleanup=cleanup)["reason"], "incomplete_cleanup_binding")

    def test_reads_existing_sidecars_and_rejects_missing_or_aggregate_substitution(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "events.jsonl").write_text(json.dumps(self.event) + "\n", encoding="utf-8")
            for name, value in {
                "transition.events.binding.json": {"run_id": "run-1", "event_path": "events.jsonl"},
                "runtime.binding.json": {"executable_sha256": "exe"},
                "process.json": {"pid": 44, "certification_lease": self.lease,
                                 "cleanup": self.cleanup},
                "plan.json": {"profile": "dev-harness", "target_world": "McWilliams"},
            }.items():
                (root / name).write_text(json.dumps(value), encoding="utf-8")
            self.assertTrue(normalize_run_bound_receipts(root)[0]["accepted"])
            (root / "process.json").write_text(json.dumps({"pid": 44}), encoding="utf-8")
            self.assertEqual(normalize_run_bound_receipts(root)[0]["reason"], "stale_or_wrong_lease")
            (root / "events.jsonl").write_text(json.dumps({
                "run_id": "run-1", "transition": "staffed_camp_signal_observation"
            }) + "\n", encoding="utf-8")
            self.assertEqual(normalize_run_bound_receipts(root)[0]["reason"],
                             "aggregate_or_wrong_transition")

    def test_unchanged_deduplication_requires_one_matching_stable_lead(self):
        created = self.receipt()
        unchanged_event = copy.deepcopy(self.event)
        unchanged_event["sequence"] = 5
        unchanged_event["staffed_camp_signal_read"]["outcome"]["kind"] = "unchanged"
        persistence = unchanged_event["staffed_camp_signal_read"]["persistence"]
        persistence["lead_count_before"] = 1
        persistence["lead_count_after"] = 1
        persistence["lead_hash_before"] = "2"
        persistence["lead_hash_after"] = "2"
        unchanged = self.receipt(unchanged_event)
        verdict = validate_unchanged_deduplication([created, unchanged])
        self.assertTrue(verdict["accepted"])
        self.assertEqual(verdict["lead_count"], 1)
        self.assertEqual(verdict["lead_hash"], "2")

        persistence["lead_count_after"] = 2
        self.assertEqual(validate_unchanged_deduplication([created, self.receipt(unchanged_event)])["reason"],
                         "lead_count_changed_or_duplicated")
        persistence["lead_count_after"] = 1
        persistence["lead_hash_after"] = "other"
        self.assertEqual(validate_unchanged_deduplication([created, self.receipt(unchanged_event)])["reason"],
                         "lead_payload_changed")

    def test_changed_observation_capture_requires_one_bound_channel_and_saved_refresh(self):
        records = [{"run_id": "run-1", "channel": "smoke", "observed": True,
                    "source_omt": "(140,50,0)", "signal_origin": "local_field",
                    "binding": {"runtime_source_sha256": "src", "executable_sha256": "exe",
                                "binding_id": "binding"}}]
        persisted = {"tile": {"fields": [{"field_id": "fd_fire", "intensity": 2}]},
                     "lead_count": 3, "leads": [{"lead_id": "lead-a", "source_key": "source-a",
                     "first_seen_minutes": 9245, "last_seen_minutes": 9250,
                     "last_checked_minutes": 9250}]}
        verdict = validate_changed_observation_capture(
            records, persisted=persisted, run_id="run-1", source_sha256="src",
            executable_sha256="exe", binding_id="binding", source_omt=(140, 50, 0),
            lead_id="lead-a", source_key="source-a")
        self.assertTrue(verdict["accepted"])
        records[0]["binding"]["binding_id"] = "other"
        self.assertEqual(validate_changed_observation_capture(
            records, persisted=persisted, run_id="run-1", source_sha256="src",
            executable_sha256="exe", binding_id="binding", source_omt=(140, 50, 0),
            lead_id="lead-a", source_key="source-a")["reason"], "sidecar_identity_mismatch")


if __name__ == "__main__":
    unittest.main()
