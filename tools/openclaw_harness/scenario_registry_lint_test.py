#!/usr/bin/env python3
"""Read-only, multi-error declaration lint through the registry CLI."""

from __future__ import annotations

import hashlib
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


if __name__ == "__main__":
    unittest.main()
