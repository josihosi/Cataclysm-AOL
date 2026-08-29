#!/usr/bin/env python3
"""Inverse controls for production-origin fixture capture."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import production_capture


class ProductionCaptureTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.scenario = self.root / "natural.json"
        self.scenario.write_text('{"name":"natural"}\n', encoding="utf-8")
        self.executable_hash = "a" * 64
        self.runtime_source_hash = "b" * 64
        self.run_id = "normal-production-run"
        self.runtime = {
            "ok": True,
            "executable_sha256": self.executable_hash,
            "runtime_source_sha256": self.runtime_source_hash,
        }
        self.report = self.root / "probe.report.json"
        self.write_report()

    def owner_change(self, before_owner: str = "abstract", after_owner: str = "local",
                     before_epoch: int = 0, after_epoch: int = 1,
                     actor_ids: list[str] | None = None) -> dict:
        actors = ["actor-a", "actor-b"] if actor_ids is None else actor_ids

        def snapshot(owner: str, epoch: int, prior: str, next_owner: str) -> dict:
            receipt = {
                "actor_ids": list(actors), "run_id": self.run_id, "activity_id": "activity-7",
                "generation": 1, "prior_owner": prior, "next_owner": next_owner,
                "handoff_epoch": epoch, "cursor_minutes": 120, "cursor_waypoint": epoch,
                "outcome": "committed", "persistence_acknowledged": True,
            }
            return {
                "world_id": "world-1", "run_id": self.run_id,
                "identity": {
                    "world_id": "world-1", "run_id": self.run_id,
                    "activity_id": "activity-7", "generation": 1,
                    "actor_ids": list(actors), "simulation_owner": owner,
                    "handoff_epoch": epoch,
                },
                "crossing_receipt": receipt,
            }

        return {
            "before": snapshot(before_owner, before_epoch, after_owner, before_owner),
            "after": snapshot(after_owner, after_epoch, before_owner, after_owner),
        }

    def tearDown(self) -> None:
        self.temp.cleanup()

    def write_report(self, *, transforms: object = None, mutate: dict | None = None) -> None:
        value = {
            "mode": "probe",
            "proof_classification": {
                "status": "green",
                "feature_proof": True,
                "evidence_class": "feature-path",
            },
            "scenario_manifest": {"source": {
                "path": str(self.scenario.resolve()),
                "sha256": hashlib.sha256(self.scenario.read_bytes()).hexdigest(),
            }},
            "startup": {
                "fixture_install": {
                    "source_chain": ["live-debug:raw-natural-world"],
                    "applied_save_transforms": [] if transforms is None else transforms,
                    "binding": {"sha256": "c" * 64},
                },
                "profile_snapshot": {
                    "source_chain": ["live-debug:base-profile"],
                    "binding": {"sha256": "d" * 64},
                },
                "screen": {"runtime_binding_observed": {
                    "status": "matched",
                    "executable_path": "/tmp/cataclysm-tiles",
                    "executable_sha256": self.executable_hash,
                    "runtime_source_sha256": self.runtime_source_hash,
                }},
            },
            "structured_transition_events": {
                "run_id": self.run_id,
                "gates": [{
                    "id": "natural_dispatch",
                    "status": "green",
                    "expectations": [{"predicate": {"outcome": "committed"}}],
                }],
            },
            "sole_owner_audit": {
                "actor_ids": ["actor-a", "actor-b"],
                "receipts": [self.owner_change()],
            },
        }
        if mutate:
            value.update(mutate)
        self.report.write_text(json.dumps(value), encoding="utf-8")

    def prepare(self, runtime: dict | None = None) -> dict:
        return production_capture.prepare_production_capture(
            report_path=self.report,
            scenario_path=self.scenario,
            runtime_binding=self.runtime if runtime is None else runtime,
        )

    def observation(self) -> tuple[Path, Path, Path]:
        before = self.root / "fixture" / "save" / "McWilliams"
        after = self.root / "profile" / "save" / "McWilliams"
        before.mkdir(parents=True, exist_ok=True)
        after.mkdir(parents=True, exist_ok=True)
        (before / "world.dat").write_bytes(b"before")
        (after / "world.dat").write_bytes(b"after")
        source = self.root / "production.source.json"
        production_capture.write_observation_source_report(
            report_path=self.report, output_path=source, before_world=before, after_world=after,
        )
        return source, before, after

    def test_normal_capture_binds_all_production_inputs(self) -> None:
        provenance = self.prepare()
        source_world = self.root / "save" / "McWilliams"
        source_world.mkdir(parents=True)
        (source_world / "world.dat").write_bytes(b"normal production world")
        manifest = production_capture.capture_production_fixture(
            source_world=source_world,
            fixture_dir=self.root / "fixtures" / "natural",
            production_origin=provenance,
            overwrite=False,
        )
        origin = manifest["production_origin"]
        self.assertEqual(origin["scenario"]["sha256"], hashlib.sha256(self.scenario.read_bytes()).hexdigest())
        self.assertEqual(origin["runtime"]["executable_sha256"], self.executable_hash)
        self.assertEqual(origin["normal_transition_receipts"]["run_id"], self.run_id)
        self.assertEqual(origin["saved_world"]["source_tree_sha256"], origin["saved_world"]["fixture_tree_sha256"])

    def test_stale_runtime_fails_closed(self) -> None:
        stale = dict(self.runtime, runtime_source_sha256="c" * 64)
        with self.assertRaisesRegex(production_capture.ProductionCaptureError, "runtime source differs"):
            self.prepare(runtime=stale)

    def test_malformed_report_fails_closed(self) -> None:
        self.report.write_text("{", encoding="utf-8")
        with self.assertRaisesRegex(production_capture.ProductionCaptureError, "unreadable"):
            self.prepare()

    def test_partial_receipts_fail_closed(self) -> None:
        self.write_report(mutate={"structured_transition_events": {"run_id": self.run_id, "gates": []}})
        with self.assertRaisesRegex(production_capture.ProductionCaptureError, "no normal transition receipts"):
            self.prepare()

    def test_duplicate_capture_fails_closed_without_overwrite(self) -> None:
        source_world = self.root / "save" / "McWilliams"
        source_world.mkdir(parents=True)
        (source_world / "world.dat").write_bytes(b"world")
        fixture = self.root / "fixtures" / "natural"
        production_capture.capture_production_fixture(
            source_world=source_world, fixture_dir=fixture, production_origin=self.prepare(), overwrite=False,
        )
        with self.assertRaisesRegex(production_capture.ProductionCaptureError, "already exists"):
            production_capture.capture_production_fixture(
                source_world=source_world, fixture_dir=fixture, production_origin=self.prepare(), overwrite=False,
            )

    def test_self_promoted_authority_fails_closed(self) -> None:
        self.write_report(mutate={"wec_authority": {"authority_id": "caller-issued"}})
        with self.assertRaisesRegex(production_capture.ProductionCaptureError, "self-promoted"):
            self.prepare()

    def test_synthetic_transform_fails_closed(self) -> None:
        self.write_report(transforms=[{"kind": "bandit_active_sortie_clock"}])
        with self.assertRaisesRegex(production_capture.ProductionCaptureError, "used save transforms"):
            self.prepare()

    def test_source_substitution_fails_closed(self) -> None:
        self.scenario.write_text('{"name":"substituted"}\n', encoding="utf-8")
        with self.assertRaisesRegex(production_capture.ProductionCaptureError, "scenario bytes changed"):
            self.prepare()

    def test_sole_owner_audit_rejects_duplicate_stale_partial_ambiguous_and_replacement_claims(self) -> None:
        valid = self.owner_change()
        reverse = self.owner_change(before_owner="local", after_owner="abstract", before_epoch=1, after_epoch=2)
        self.assertEqual(
            production_capture.audit_sole_owner_receipts(
                {"actor_ids": ["actor-a", "actor-b"], "receipts": [valid, reverse]},
                expected_run_id=self.run_id,
            )["status"], "green"
        )
        cases = {
            "duplicate": [valid, valid],
            "stale": [self.owner_change(after_epoch=3)],
            "partial": [{"before": valid["before"]}],
            "ambiguous": [valid, self.owner_change(before_owner="abstract", after_owner="local", before_epoch=0, after_epoch=1)],
            "replacement": [self.owner_change(actor_ids=["actor-a", "replacement"])],
        }
        for label, rows in cases.items():
            with self.subTest(label=label):
                result = production_capture.audit_sole_owner_receipts(
                    {"actor_ids": ["actor-a", "actor-b"], "receipts": rows},
                    expected_run_id=self.run_id,
                )
                self.assertEqual(result["status"], "red")

    def test_capture_requires_route_independent_sole_owner_audit(self) -> None:
        self.write_report(mutate={"sole_owner_audit": {"receipts": []}})
        with self.assertRaisesRegex(production_capture.ProductionCaptureError, "sole-owner audit"):
            self.prepare()

    def test_observation_source_is_explicitly_uncredited_and_validates(self) -> None:
        source, _, _ = self.observation()
        observed = production_capture.validate_observation_source_report(
            source_path=source, runtime_binding=self.runtime,
        )
        self.assertEqual(observed["credit"], production_capture.OBSERVATION_CREDIT)

    def test_observation_source_rejects_replacement_and_continuity_abuse(self) -> None:
        source, _, after = self.observation()
        after.joinpath("world.dat").write_bytes(b"replacement")
        with self.assertRaisesRegex(production_capture.ProductionCaptureError, "saved world after"):
            production_capture.validate_observation_source_report(source_path=source, runtime_binding=self.runtime)

        # Each source is independent so the first rejected divergence remains precise.
        for key in ("resumed", "rolled_back", "joined_segments", "replacement_identity"):
            source, _, _ = self.observation()
            payload = json.loads(source.read_text(encoding="utf-8"))
            payload["continuity"][key] = True
            source.write_text(json.dumps(payload), encoding="utf-8")
            with self.assertRaisesRegex(production_capture.ProductionCaptureError, key):
                production_capture.validate_observation_source_report(source_path=source, runtime_binding=self.runtime)

    def test_observation_source_rejects_self_promotion_and_partial_or_malformed_inputs(self) -> None:
        source, _, _ = self.observation()
        payload = json.loads(source.read_text(encoding="utf-8"))
        payload["credit"]["route_authority"] = True
        source.write_text(json.dumps(payload), encoding="utf-8")
        with self.assertRaisesRegex(production_capture.ProductionCaptureError, "grant credit"):
            production_capture.validate_observation_source_report(source_path=source, runtime_binding=self.runtime)

        self.write_report(mutate={"structured_transition_events": {"run_id": self.run_id, "gates": []}})
        with self.assertRaisesRegex(production_capture.ProductionCaptureError, "transition receipts"):
            self.observation()
        source.write_text("{", encoding="utf-8")
        with self.assertRaisesRegex(production_capture.ProductionCaptureError, "unreadable"):
            production_capture.validate_observation_source_report(source_path=source, runtime_binding=self.runtime)

    def test_red_observation_source_preserves_first_divergence_without_credit(self) -> None:
        self.write_report(mutate={"structured_transition_events": {"run_id": self.run_id, "gates": []}})
        source = self.root / "production.source.json"
        failed = production_capture.write_observation_failure_report(
            report_path=self.report, output_path=source, reason="source report has no normal transition receipts",
        )
        self.assertEqual(failed["credit"], production_capture.OBSERVATION_CREDIT)
        with self.assertRaisesRegex(production_capture.ProductionCaptureError, "incomplete"):
            production_capture.validate_observation_source_report(source_path=source, runtime_binding=self.runtime)

    def relaunch_receipt(self, *, world_id: str = "world-1", run_id: str = "lineage-1",
                         actor_ids: list[str] | None = None, owner: str = "local",
                         generation: int = 7, handoff_epoch: int = 3) -> dict:
        actors = ["actor-a", "actor-b"] if actor_ids is None else actor_ids
        crossing = {
            "actor_ids": list(actors), "run_id": run_id, "activity_id": "activity-7",
            "generation": generation, "prior_owner": "abstract", "next_owner": owner,
            "handoff_epoch": handoff_epoch, "cursor_minutes": 120,
            "cursor_waypoint": 2, "outcome": "committed", "persistence_acknowledged": True,
        }
        return {
            "world_id": world_id, "run_id": run_id,
            "identity": {
                "world_id": world_id, "run_id": run_id, "activity_id": "activity-7",
                "generation": generation, "actor_ids": list(actors),
                "simulation_owner": owner, "handoff_epoch": handoff_epoch,
            },
            "crossing_receipt": crossing,
        }

    def test_normal_save_quit_new_process_relaunch_normalizes_one_transition(self) -> None:
        before = self.relaunch_receipt()
        after = self.relaunch_receipt()
        crossing = dict(after["crossing_receipt"])
        normalized = production_capture.normalize_relaunch_receipt(
            before_save=before, after_load=after, transition=crossing,
            expected_world_id="world-1", expected_run_id="lineage-1",
        )
        self.assertEqual(normalized["status"], "green")
        self.assertEqual(normalized["normalization"], "matched_single_committed_transition")
        self.assertEqual(normalized["identity"]["actor_ids"], ["actor-a", "actor-b"])

    def test_relaunch_receipt_rejects_forbidden_or_replacement_paths(self) -> None:
        baseline = self.relaunch_receipt()
        crossing = dict(baseline["crossing_receipt"])
        cases = (
            (dict(baseline, route="checkpoint_rollback"), baseline, crossing, "forbidden"),
            (dict(baseline, stale=True), baseline, crossing, "forbidden"),
            (baseline, self.relaunch_receipt(world_id="replacement"), crossing, "post-load"),
            (baseline, baseline, [crossing, crossing], "exactly one"),
            (baseline, baseline, dict(crossing, outcome="rejected"), "committed"),
        )
        for before, after, transition, message in cases:
            with self.subTest(message=message):
                with self.assertRaisesRegex(production_capture.RelaunchReceiptError, message):
                    production_capture.normalize_relaunch_receipt(
                        before_save=before, after_load=after, transition=transition,
                    )

    def test_relaunch_receipt_rejects_stale_duplicate_partial_and_malformed_identity(self) -> None:
        baseline = self.relaunch_receipt()
        crossing = dict(baseline["crossing_receipt"])
        cases = (
            dict(baseline, identity={**baseline["identity"], "run_id": "stale"}),
            dict(baseline, identity={**baseline["identity"], "actor_ids": ["actor-a", "actor-a"]}),
            dict(baseline, identity={key: value for key, value in baseline["identity"].items() if key != "handoff_epoch"}),
            dict(baseline, crossing_receipt={**crossing, "next_owner": "abstract"}),
        )
        for after in cases:
            with self.assertRaises(production_capture.RelaunchReceiptError):
                production_capture.normalize_relaunch_receipt(
                    before_save=baseline, after_load=after, transition=crossing,
                )


if __name__ == "__main__":
    unittest.main()
