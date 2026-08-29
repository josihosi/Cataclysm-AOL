#!/usr/bin/env python3
"""Focused controls for R-008 source-bound capability validation."""

from __future__ import annotations

import hashlib
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

from scenario_registry import ManifestValidationError, validate_manifest  # noqa: E402
from scenario_registry_store import StagedManifest, _source_binding_validation_records  # noqa: E402


CAPABILITIES = (
    "world.r008_closure_046.fixed_native_footing",
    "capabilities.shakedown.transform_free_m095",
)

STABILIZER_TRANSFORM = {
    "kind": "player_mutations",
    "player_save": "#R2xvcnkgVHJlam8=.sav.zzip",
    "mutations": [
        "DEBUG_LS", "DEBUG_NOTEMP", "DEBUG_STAMINA", "DEBUG_CARDIO",
        "DEBUG_CLAIRVOYANCE", "DEBUG_NIGHTVISION",
    ],
}


class SourceBindingValidationTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temp_dir = tempfile.TemporaryDirectory()
        self.root = Path(self.temp_dir.name)
        artifact = self.root / ".userdata/r008-closure-045/r008-closure-045-footing-bootstrap.json"
        artifact.parent.mkdir(parents=True)
        artifact.write_text(json.dumps({
            "source_chain": {"raw_seed": 830205385, "world_name": "R008Closure045Seed830205385"},
            "footing": {
                "native_camp": {"site_id": "overmap_special:bandit_camp@177,13,0"},
                "hostile_origin": {"origin_omt": [177, 9, 0]},
                "candidate_lane": {"watch_omt": [174, 13, 0], "watch_route_cost": 10},
            },
        }), encoding="utf-8")
        fixture = self.root / "tools/openclaw_harness/fixtures/saves/dev-harness/r008_closure_046_source_bound_m095/manifest.json"
        fixture.parent.mkdir(parents=True)
        fixture.write_text(json.dumps({
            "save_transforms": [STABILIZER_TRANSFORM],
            "source_binding": {
                "raw_seed": 830205385,
                "world_name": "R008Closure045Seed830205385",
                "native_camp": "overmap_special:bandit_camp@177,13,0",
                "hostile_origin_omt": [177, 9, 0],
                "watch_omt": [174, 13, 0],
                "watch_route_cost": 10,
            },
        }), encoding="utf-8")
        self.declaration = {
            "fixture": "r008_closure_046_source_bound_m095",
            "fixture_profile": "dev-harness",
            "capabilities": {
                CAPABILITIES[0]: {
                    "raw_seed": "830205385", "world": "R008Closure045Seed830205385",
                    "camp_omt": "177,13,0", "hostile_origin_omt": "177,9,0",
                    "watch_omt": "174,13,0", "watch_route_cost": 10,
                    "save_transforms": ["player_mutations"],
                    "stabilizer_traits": STABILIZER_TRANSFORM["mutations"],
                },
                CAPABILITIES[1]: "native_wait_save_quit_relaunch",
            },
            "source_binding": {"bootstrap_artifact": ".userdata/r008-closure-045/r008-closure-045-footing-bootstrap.json"},
            "source_binding_validation": {
                "validator": "r008_closure_046_source_binding",
                "bootstrap_artifact": ".userdata/r008-closure-045/r008-closure-045-footing-bootstrap.json",
                "capabilities": list(CAPABILITIES),
            },
            "runtime_contract": {"forbidden_input": ["fixture-save-transform"]},
        }

    def tearDown(self) -> None:
        self.temp_dir.cleanup()

    def records(self, declaration: dict | None = None) -> tuple:
        payload = self.declaration if declaration is None else declaration
        source = json.dumps(payload, sort_keys=True)
        staged = StagedManifest(
            manifest_id="m095", source_path="scenario.json",
            source_sha256=hashlib.sha256(source.encode("utf-8")).hexdigest(),
            declaration_json=source, normalized_json="{}", validation_json="{}",
            capabilities=(), proof_routes=(), relations=(),
        )
        with mock.patch("scenario_registry_store.repository_root", return_value=self.root):
            return _source_binding_validation_records(staged)

    def test_derives_exact_declared_values_only_from_matching_source_owners(self) -> None:
        records = self.records()
        self.assertEqual([record[0] for record in records], list(CAPABILITIES))
        self.assertTrue(all(record[1] == "inspected" for record in records))

    def test_rejects_missing_false_stale_mismatched_and_transformed_sources(self) -> None:
        cases = {
            "missing": lambda value: value["source_binding_validation"].update({"bootstrap_artifact": ".userdata/missing.json"}),
            "false": lambda value: value["capabilities"].update({CAPABILITIES[1]: "not_native"}),
            "stale": lambda value: value["source_binding"].update({"bootstrap_artifact": ".userdata/other.json"}),
            "mismatched_world": lambda value: value["capabilities"][CAPABILITIES[0]].update({"world": "OtherWorld"}),
        }
        for name, mutate in cases.items():
            with self.subTest(name=name):
                value = json.loads(json.dumps(self.declaration))
                mutate(value)
                self.assertTrue(all(record[1] == "contradicted" for record in self.records(value)))
        fixture = self.root / "tools/openclaw_harness/fixtures/saves/dev-harness/r008_closure_046_source_bound_m095/manifest.json"
        payload = json.loads(fixture.read_text(encoding="utf-8"))
        payload["save_transforms"] = [{"kind": "fixture-save-transform"}]
        fixture.write_text(json.dumps(payload), encoding="utf-8")
        self.assertTrue(all(record[1] == "contradicted" for record in self.records()))

    def test_natural_source_binding_requires_the_post_install_transform_boundary(self) -> None:
        for name in (
            "bandit.r008_natural_safe_watch_validation_mcw",
            "bandit.r008_natural_return_validation_mcw",
            "bandit.r008_natural_wait_progress_observation_mcw",
        ):
            with self.subTest(name=name):
                source_path = HARNESS_DIR / "scenarios" / f"{name}.json"
                declaration = json.loads(source_path.read_text(encoding="utf-8"))

                def natural_records(value: dict) -> tuple:
                    source = json.dumps(value, sort_keys=True)
                    staged = StagedManifest(
                        manifest_id=name, source_path=str(source_path),
                        source_sha256=hashlib.sha256(source.encode("utf-8")).hexdigest(),
                        declaration_json=source, normalized_json="{}", validation_json="{}",
                        capabilities=(), proof_routes=(), relations=(),
                    )
                    return _source_binding_validation_records(staged)

                self.assertTrue(all(record[1] == "inspected" for record in natural_records(declaration)))
                declaration["runtime_contract"]["forbidden_input"] = ["fixture-save-transform"]
                self.assertTrue(all(record[1] == "contradicted" for record in natural_records(declaration)))

    def test_without_validator_no_capability_is_promoted(self) -> None:
        value = json.loads(json.dumps(self.declaration))
        value.pop("source_binding_validation")
        self.assertEqual(self.records(value), ())

    def test_packaged_m095_rejects_unvalidated_declarations(self) -> None:
        source = HARNESS_DIR / "scenarios/bandit.r008_closure_046_source_bound_m095_mcw.json"
        declaration = json.loads(source.read_text(encoding="utf-8"))
        declaration.pop("source_binding_validation")
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / source.name
            path.write_text(json.dumps(declaration), encoding="utf-8")
            with self.assertRaisesRegex(ManifestValidationError, "requires source_binding_validation"):
                validate_manifest(declaration, path=path)


if __name__ == "__main__":
    unittest.main()
