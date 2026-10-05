#!/usr/bin/env python3
"""Read-only, multi-error declaration lint through the registry CLI."""

from __future__ import annotations

import hashlib
import gzip
import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


CLI = Path(__file__).with_name("scenario_registry_cli.py")


def valid_manifest() -> dict:
    return {
        "manifest_version": 1,
        "name": "lint.fixture",
        "steps": [{"label": label, "kind": "press"} for label in (
            "setup", "action", "save", "artifact", "shortcut",
        )],
        "capabilities": {"player.ready": True},
        "runtime_contract": {
            "permitted_input": ["press:x"],
            "forbidden_input": [],
            "setup_only_debug": False,
            "disposable_copy": True,
            "helpers": [],
            "permissions": [],
            "platform": ["macos"],
            "profile": "test",
            "fixture": "test",
            "requirements": {key: None for key in (
                "os", "source", "executable", "profile", "fixture", "helper",
                "peekaboo", "input", "ocr", "cleanup",
            )},
            "grants_gameplay_proof": False,
        },
        "proof_route": {
            "precondition": ["setup"],
            "production_behavior": ["action"],
            "terminal_persistence": ["save"],
            "artifact_verdict": ["artifact"],
            "disallowed_shortcuts": ["shortcut"],
        },
    }


def write_manifest(path: Path, manifest: dict) -> None:
    path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


class ScenarioRegistryLintTest(unittest.TestCase):
    def run_lint(self, registry: Path, *paths: Path) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [sys.executable, str(CLI), "--json", "--registry", str(registry),
             "lint-declarations", *(str(path) for path in paths)],
            text=True, capture_output=True, check=False,
        )

    def test_multiple_independent_errors_then_corrected_fixture_without_mutation(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            path = root / "scenario.json"
            registry = root / "registry.sqlite3"
            registry.write_bytes(b"registry sentinel: must not be opened or rewritten")
            registry_before = sha256(registry)
            manifest = valid_manifest()
            manifest["capabilities"] = {"unscoped": True}
            manifest["runtime_contract"]["requirements"].pop("os")
            manifest["proof_route"]["production_behavior"] = ["missing_action"]
            manifest["r027_isolated_launch"] = {"name": "incomplete"}
            write_manifest(path, manifest)
            before = sha256(path)

            failed = self.run_lint(registry, path)

            self.assertEqual(failed.returncode, 1, failed.stderr)
            self.assertEqual(failed.stderr, "")
            result = json.loads(failed.stdout)
            self.assertFalse(result["ok"])
            self.assertEqual(result["counts"], {"valid": 0, "review_required": 0, "invalid": 1})
            self.assertEqual(result["results"][0]["status"], "invalid")
            diagnostics = result["results"][0]["diagnostics"]
            self.assertGreaterEqual(len(diagnostics), 4)
            self.assertTrue({"capabilities", "runtime_contract", "proof_route",
                             "r027_isolated_launch"}.issubset({item["field"].split(".")[0]
                                                                for item in diagnostics}))
            self.assertTrue(all(item["file"] == str(path.resolve()) and item["line"] is not None
                                and item["severity"] == "error" and item["rule"]
                                for item in diagnostics))
            self.assertEqual(sha256(path), before)
            self.assertEqual(sha256(registry), registry_before)
            plain = subprocess.run(
                [sys.executable, str(CLI), "--registry", str(registry),
                 "lint-declarations", str(path)],
                text=True, capture_output=True, check=False,
            )
            self.assertEqual(plain.returncode, 1, plain.stderr)
            self.assertIn("[capability_declaration] capabilities:", plain.stdout)
            self.assertIn("[runtime_contract] runtime_contract.requirements.os:", plain.stdout)
            self.assertIn("invalid=1", plain.stdout)

            write_manifest(path, valid_manifest())
            corrected = sha256(path)
            passed = self.run_lint(registry, path)
            self.assertEqual(passed.returncode, 0, passed.stderr)
            self.assertEqual(json.loads(passed.stdout)["results"][0]["status"], "valid")
            self.assertEqual(json.loads(passed.stdout)["results"][0]["diagnostics"], [])
            self.assertEqual(sha256(path), corrected)
            self.assertEqual(sha256(registry), registry_before)

    def test_selected_immutable_product_build_binding_has_a_typed_fail_closed_schema(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            path = root / "scenario.json"
            registry = root / "registry.sqlite3"
            registry.write_bytes(b"registry remains untouched")
            manifest = valid_manifest()
            manifest["runtime_contract"]["selected_product_build"] = {
                "schema": "caol-selected-product-build-v1",
                "receipt_path": "build_logs/immutable-receipt.json",
                "receipt_sha256": "a" * 64,
                "executable_sha256": "b" * 64,
                "product_source_sha256": "c" * 64,
            }
            manifest["runtime_contract"]["requirements"]["executable"] = (
                "build_logs/immutable/cataclysm-tiles"
            )
            write_manifest(path, manifest)

            valid = self.run_lint(registry, path)

            self.assertEqual(valid.returncode, 0, valid.stdout + valid.stderr)
            result = json.loads(valid.stdout)
            self.assertEqual(result["counts"], {"valid": 1, "review_required": 0, "invalid": 0})

            manifest["runtime_contract"]["selected_product_build"]["product_source_sha256"] = "C" * 64
            write_manifest(path, manifest)
            invalid = self.run_lint(registry, path)

            self.assertEqual(invalid.returncode, 1)
            invalid_result = json.loads(invalid.stdout)
            self.assertEqual(invalid_result["counts"]["invalid"], 1)
            self.assertIn("runtime_contract", {
                item["field"].split(".")[0]
                for item in invalid_result["results"][0]["diagnostics"]
            })
            self.assertEqual(registry.read_bytes(), b"registry remains untouched")

    def test_legacy_review_and_json_syntax_are_distinct(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            registry = root / "registry.sqlite3"
            legacy = root / "legacy.json"
            syntax = root / "syntax.json"
            manifest = valid_manifest()
            manifest.pop("manifest_version")
            write_manifest(legacy, manifest)
            syntax.write_text('{\n  "manifest_version": 1,\n  "steps": [\n}\n', encoding="utf-8")

            result = self.run_lint(registry, legacy, syntax)

            self.assertEqual(result.returncode, 1)
            records = json.loads(result.stdout)["results"]
            self.assertEqual([item["status"] for item in records], ["review_required", "invalid"])
            self.assertEqual(records[0]["diagnostics"][0]["rule"], "legacy_review_required")
            self.assertEqual(records[0]["diagnostics"][0]["severity"], "warning")
            self.assertEqual(records[1]["diagnostics"][0]["rule"], "json_syntax")
            self.assertEqual(records[1]["diagnostics"][0]["line"], 4)
            self.assertFalse(registry.exists())

    def test_checkpoint_declaration_reports_independent_field_shapes(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            path = root / "checkpoint.json"
            manifest = valid_manifest()
            manifest.update({
                "manifest_version": 2,
                "run_class": [],
                "observer_character": "yes",
                "installed_save_player": "",
                "proof_gates": [],
                "proof_route": [],
            })
            write_manifest(path, manifest)

            result = self.run_lint(root / "absent.sqlite3", path)

            self.assertEqual(result.returncode, 1, result.stderr)
            fields = {item["field"] for item in json.loads(result.stdout)["results"][0]["diagnostics"]}
            self.assertTrue({"run_class", "observer_character", "installed_save_player",
                             "proof_gates", "proof_route"}.issubset(fields))

    def test_actual_r12_relaunch_only_reference_fails_before_selection_without_mutation(self):
        import scenario_registry as registry
        original = gzip.decompress((Path(__file__).parent /
            "fixtures/controls/r067_original_r12_relaunch_reference.json.gz").read_bytes())
        self.assertEqual(hashlib.sha256(original).hexdigest(),
            "96b635c035ce34eef8a722a0af76616801a6c7fc93c6dc15db99aba91b6518ac")
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); path = root / "r12.json"; path.write_bytes(original)
            manifest = json.loads(original); before = copy.deepcopy(manifest)
            result = self.run_lint(root / "absent.sqlite3", path)
            self.assertEqual(result.returncode, 1, result.stderr)
            row = json.loads(result.stdout)["results"][0]
            diagnostic = next(d for d in row["diagnostics"] if
                              d["rule"] == "post_relaunch_initial_step_reference")
            self.assertEqual(diagnostic["field"], "post_relaunch.terminal_save_step_label")
            self.assertIn("available initial labels", diagnostic["message"])
            self.assertIn(manifest["steps"][-1]["label"], diagnostic["message"])
            with self.assertRaisesRegex(registry.ManifestValidationError, "must name an initial"):
                registry.validate_manifest(manifest, path=path)
            self.assertEqual(manifest, before); self.assertEqual(path.read_bytes(), original)
            self.assertFalse((root / "absent.sqlite3").exists())

    def test_actual_saved_night_start_reuses_exact_declared_steps_without_synthetic_contract(self):
        import scenario_registry as registry
        import startup_harness
        original = gzip.decompress((Path(__file__).parent /
            "fixtures/controls/r067_original_saved_night_no_relaunch.json.gz").read_bytes())
        self.assertEqual(hashlib.sha256(original).hexdigest(),
            "4c83ae07c60aeb0faa468d654314facac9eca7f76f79688fb1236969856a7d63")
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "night.json"; path.write_bytes(original)
            manifest = json.loads(original); before = copy.deepcopy(manifest)
            self.assertEqual(registry.lint_manifest(path)["status"], "valid")
            registry.validate_manifest(manifest, path=path)
            steps = startup_harness.normalize_scenario_steps(manifest["steps"], 0, 0)
            self.assertIsNone(startup_harness.normalize_post_relaunch_contract(None, steps))
            continuation = registry.saved_world_continuation_steps(steps, None)
            self.assertEqual(continuation, steps)
            self.assertEqual([step["label"] for step in continuation],
                             [step["label"] for step in manifest["steps"]])
            continuation[1]["objective"] = "changed returned copy"
            self.assertNotEqual(continuation[1]["objective"], steps[1]["objective"])
            self.assertEqual(manifest, before); self.assertEqual(path.read_bytes(), original)
            self.assertNotIn("post_relaunch", manifest)

    def test_saved_world_start_never_guesses_setup_or_ambiguous_routes(self):
        import scenario_registry as registry
        safe = [{"kind": "native_semantic_bootstrap", "label": "saved_world"},
                {"kind": "cockpit_live_session", "label": "actual_player"}]
        for steps in ([safe[1]], safe[::-1], [safe[0], {"kind": "press", "label": "setup"}, safe[1]],
                      [*safe, safe[1]], [None, safe[1]]):
            with self.subTest(steps=steps), tempfile.TemporaryDirectory() as temp:
                path = Path(temp) / "legacy.json"
                manifest = {"name": "ambiguous", "saved_world_snapshot": "/exact/save", "steps": steps}
                write_manifest(path, manifest); raw = path.read_bytes()
                row = registry.lint_manifest(path)
                diagnostic = next(d for d in row["diagnostics"] if
                                  d["rule"] == "saved_world_continuation_steps")
                self.assertEqual(diagnostic["field"], "saved_world_snapshot")
                self.assertIn("declare post_relaunch.steps explicitly", diagnostic["message"])
                with self.assertRaises(registry.ManifestValidationError):
                    registry.validate_manifest(manifest, path=path)
                self.assertEqual(path.read_bytes(), raw)
        # No saved start requested: legacy declarations remain optional.
        self.assertEqual(registry.saved_world_continuation_steps(safe, None), safe)
        post = {"steps": [{"kind": "cockpit_live_session", "label": "bound_reentry"}]}
        self.assertEqual(registry.saved_world_continuation_steps([safe[0], {"kind": "press"}], post),
                         [safe[0], post["steps"][0]])

    def test_post_relaunch_valid_initial_reference_and_omitted_contract(self):
        import scenario_registry as registry
        for declared in (False, True):
            with self.subTest(declared=declared), tempfile.TemporaryDirectory() as temp:
                path = Path(temp) / "valid.json"; manifest = valid_manifest()
                if declared:
                    manifest["post_relaunch"] = {"terminal_save_step_label": "save",
                        "terminal_exit_timeout_seconds": 30,
                        "steps": [{"kind": "wait", "label": "reentry", "seconds": 1}]}
                write_manifest(path, manifest)
                self.assertEqual(registry.lint_manifest(path)["status"], "valid")
                self.assertEqual(registry.validate_manifest(manifest, path=path)["validation"]["status"], "valid")

    def test_post_relaunch_malformed_reference_is_not_coerced(self):
        import scenario_registry as registry
        for contract in ([], {}, {"terminal_save_step_label": 7},
                         {"terminal_save_step_label": True}, {"terminal_save_step_label": ""}):
            with self.subTest(contract=contract), tempfile.TemporaryDirectory() as temp:
                path = Path(temp) / "invalid.json"; manifest = valid_manifest()
                manifest["post_relaunch"] = contract; write_manifest(path, manifest)
                self.assertEqual(registry.lint_manifest(path)["status"], "invalid")
                with self.assertRaises(registry.ManifestValidationError):
                    registry.validate_manifest(manifest, path=path)

    def test_post_relaunch_legacy_optional_and_implicit_initial_labels(self):
        import scenario_registry as registry
        for contract in (None, "", {"terminal_save_step_label": "step_01_wait"}):
            with self.subTest(contract=contract), tempfile.TemporaryDirectory() as temp:
                path = Path(temp) / "legacy.json"
                manifest = {"name": "legacy", "steps": [{"kind": "wait", "seconds": 1}]}
                if contract is not None: manifest["post_relaunch"] = contract
                write_manifest(path, manifest)
                self.assertEqual(registry.lint_manifest(path)["status"], "review_required")
                self.assertEqual(registry.validate_manifest(manifest, path=path)["validation"]["status"], "review_required")
        # An adopted legacy contract still cannot name a relaunch-only step.
        with self.assertRaisesRegex(ValueError, "must name an initial"):
            registry.post_relaunch_terminal_label({"terminal_save_step_label": "reentry"},
                                                  [{"label": "save", "kind": "wait"}])

    def test_post_relaunch_lint_has_no_startup_import(self):
        code = ("import scenario_registry,sys; "
                "assert scenario_registry.post_relaunch_terminal_label(None,[]) is None; "
                "assert 'startup_harness' not in sys.modules")
        result = subprocess.run([sys.executable, "-c", code], cwd=CLI.parent,
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
