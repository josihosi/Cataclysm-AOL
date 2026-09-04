import hashlib
import sys
import tempfile
import unittest
from unittest import mock
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from identity_binding import canonical_digest, component_identity
from scenario_registry_store import (
    ScenarioRegistryStoreError,
    append_certification_lease_event,
    append_certification_lifecycle_event,
    _certification_round_check,
    certification_round_authority_facts,
    certification_round_facts,
    invalidate_certification_round,
    _issue_registry_certification_authority,
    open_registry,
    register_certification_round,
)


class CertificationRoundRegistryTest(unittest.TestCase):
    def manifest(self, round_id="round-1"):
        names = ("worktree", "executable", "data_config", "harness", "scenario", "fixture", "profile", "world_save", "player", "actors")
        authoritative = {name: {"identity": name} for name in names}
        authoritative["scenario"] = {"identity": "scenario", "content_sha256": "a" * 64}
        authoritative["executable"] = {"identity": "executable", "content_sha256": "b" * 64}
        components = {name: component_identity(name, authoritative[name]) for name in names}
        binding = {"schema": 1, "components": components, "authoritative_components": authoritative}
        binding["sha256"] = canonical_digest({key: value["sha256"] for key, value in components.items()}, domain="caol-complete-binding:v1")
        result = {"schema": 1, "version": 1, "round_id": round_id, "scenario_lineage_id": "lineage",
                  "authority_id": "authority", "authority_kind": "automated-certification", "event_stream_id": "stream",
                  "event_stream_schema": 1, "binding_id": binding["sha256"], "binding": binding}
        result["manifest_sha256"] = canonical_digest(result, domain="caol-round-manifest:v1")
        return result

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.db = open_registry(str(Path(self.temp.name) / "registry.sqlite3"))
        manifest = self.manifest()
        authority = _issue_registry_certification_authority(
            self.db, round_id=manifest["round_id"], binding_id=manifest["binding_id"],
            source_sha256="a" * 64, launch_token="test-token",
        )
        manifest["authority_id"] = authority["authority_id"]
        manifest["manifest_sha256"] = canonical_digest(
            {key: value for key, value in manifest.items() if key != "manifest_sha256"},
            domain="caol-round-manifest:v1",
        )
        self.valid_manifest = manifest
        register_certification_round(self.db, manifest)

    def tearDown(self):
        self.db.close(); self.temp.cleanup()

    def test_registration_and_immutable_components(self):
        self.assertTrue(register_certification_round(self.db, self.valid_manifest)["idempotent"])
        changed = self.valid_manifest.copy(); changed["binding"] = dict(changed["binding"]); changed["binding"]["authoritative_components"] = dict(changed["binding"]["authoritative_components"]); changed["binding"]["authoritative_components"]["world_save"] = {"identity": "changed"}
        with self.assertRaises(ScenarioRegistryStoreError): register_certification_round(self.db, changed)
        before = self.db.execute("SELECT fact_sha256, fact_json FROM certification_round_component ORDER BY component_sequence").fetchall()
        with self.assertRaises(Exception): self.db.execute("UPDATE certification_round_component SET fact_json='x'")
        self.assertEqual(before, self.db.execute("SELECT fact_sha256, fact_json FROM certification_round_component ORDER BY component_sequence").fetchall())

    def test_forged_manifest_cannot_become_registry_authority(self):
        forged = self.valid_manifest.copy()
        forged["round_id"] = "caller-forged-round"
        forged["manifest_sha256"] = canonical_digest(
            {key: value for key, value in forged.items() if key != "manifest_sha256"},
            domain="caol-round-manifest:v1",
        )
        with self.assertRaisesRegex(ScenarioRegistryStoreError, "authority"):
            register_certification_round(self.db, forged)

    def test_ordered_lifecycle_and_first_invalidation(self):
        append_certification_lifecycle_event(self.db, round_id="round-1", event_sequence=1, event_kind="started")
        with self.assertRaises(ScenarioRegistryStoreError): append_certification_lifecycle_event(self.db, round_id="round-1", event_sequence=3, event_kind="skipped")
        append_certification_lifecycle_event(self.db, round_id="round-1", event_sequence=2, event_kind="complete")
        self.assertFalse(invalidate_certification_round(self.db, round_id="round-1", reason="drift", component_name="world")["preserved"])
        first = invalidate_certification_round(self.db, round_id="round-1", reason="later", component_name="player")
        self.assertEqual((first["first_reason"], first["first_component"]), ("drift", "world"))

    def test_successful_recheck_is_current_lifecycle_evidence_for_final_gate(self):
        append_certification_lifecycle_event(self.db, round_id="round-1", event_sequence=1, event_kind="started")
        append_certification_lifecycle_event(
            self.db, round_id="round-1", event_sequence=2, event_kind="segment_rechecked",
            details={"segment": "post_relaunch_evidence_segment", "recheck": {"ok": True}},
        )
        report_facts = {
            "manifest": {"source_sha256": "a" * 64},
            "runtime": {"runtime_binding_observed": {"executable_sha256": "b" * 64}},
            "certification_lifecycle": {},
        }
        with mock.patch("scenario_registry_store.evaluate_continuous_certification", return_value={"status": "green"}):
            accepted = _certification_round_check(
                self.db, authority=certification_round_authority_facts(self.db, "round-1"),
                round_facts=certification_round_facts(self.db, "round-1"), report_facts=report_facts,
            )
        self.assertTrue(accepted["eligible"], accepted)

        append_certification_lifecycle_event(
            self.db, round_id="round-1", event_sequence=3, event_kind="segment_rechecked",
            details={"segment": "later_evidence_segment", "recheck": {"ok": False}},
        )
        rejected = _certification_round_check(
            self.db, authority=certification_round_authority_facts(self.db, "round-1"),
            round_facts=certification_round_facts(self.db, "round-1"), report_facts=report_facts,
        )
        self.assertEqual(rejected["reason"], "certification_round_lifecycle_not_active")

    def test_lease_history_is_append_only_and_ordered(self):
        append_certification_lease_event(self.db, round_id="round-1", lease_id="lease", event_sequence=1, event_kind="declared", process_identity="pid", world_identity="world")
        self.assertTrue(append_certification_lease_event(self.db, round_id="round-1", lease_id="lease", event_sequence=1, event_kind="declared", process_identity="pid", world_identity="world")["idempotent"])
        with self.assertRaises(ScenarioRegistryStoreError): append_certification_lease_event(self.db, round_id="round-1", lease_id="lease", event_sequence=3, event_kind="released", process_identity="pid", world_identity="world")


if __name__ == "__main__":
    unittest.main()
