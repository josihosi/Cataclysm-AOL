import hashlib
import json
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
    certification_round_manifest,
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

    def test_compact_future_storage_reconstructs_the_exact_full_manifest(self):
        reconstructed = certification_round_manifest(self.db, "round-1")
        self.assertEqual(reconstructed, self.valid_manifest)
        self.assertEqual(
            json.dumps(reconstructed, sort_keys=True, separators=(",", ":")),
            json.dumps(self.valid_manifest, sort_keys=True, separators=(",", ":")),
        )
        stored = json.loads(self.db.execute(
            "SELECT manifest_json FROM certification_round WHERE round_id = 'round-1'"
        ).fetchone()[0])
        self.assertEqual(stored["storage_schema"], "caol-certification-round-storage-v2")
        self.assertNotIn("binding", stored["round_manifest"])

        large = self.manifest("round-large")
        payload = "x" * (1024 * 1024)
        authoritative = large["binding"]["authoritative_components"]
        authoritative["world_save"] = {"identity": "world_save", "payload": payload}
        large["binding"]["components"]["world_save"] = component_identity(
            "world_save", authoritative["world_save"],
        )
        large["binding"]["sha256"] = canonical_digest(
            {key: value["sha256"] for key, value in large["binding"]["components"].items()},
            domain="caol-complete-binding:v1",
        )
        large["binding_id"] = large["binding"]["sha256"]
        authority = _issue_registry_certification_authority(
            self.db, round_id="round-large", binding_id=large["binding_id"],
            source_sha256="a" * 64, launch_token="large-test-token",
        )
        large["authority_id"] = authority["authority_id"]
        large["manifest_sha256"] = canonical_digest(
            {key: value for key, value in large.items() if key != "manifest_sha256"},
            domain="caol-round-manifest:v1",
        )
        register_certification_round(self.db, large)
        row = self.db.execute(
            "SELECT manifest_json FROM certification_round WHERE round_id = 'round-large'"
        ).fetchone()
        component_bytes = self.db.execute(
            "SELECT COALESCE(SUM(LENGTH(fact_json)), 0) FROM certification_round_component WHERE round_id = 'round-large'"
        ).fetchone()[0]
        compact_bytes = len(str(row["manifest_json"])) + int(component_bytes)
        legacy_bytes = len(json.dumps(large, sort_keys=True, separators=(",", ":"))) + int(component_bytes)
        self.assertLess(compact_bytes, legacy_bytes // 2)
        reconstructed_large = certification_round_manifest(self.db, "round-large")
        self.assertEqual(reconstructed_large, large)
        self.assertEqual(
            json.dumps(reconstructed_large, sort_keys=True, separators=(",", ":")),
            json.dumps(large, sort_keys=True, separators=(",", ":")),
        )

    def test_compact_component_tampering_fails_closed_on_full_retrieval(self):
        self.db.execute("DROP TRIGGER certification_round_component_no_update")
        self.db.execute(
            "UPDATE certification_round_component SET fact_json = '{}' "
            "WHERE round_id = 'round-1' AND component_name = 'world_save'"
        )
        with self.assertRaisesRegex(ScenarioRegistryStoreError, "component digest mismatch"):
            certification_round_manifest(self.db, "round-1")

    def test_compact_component_rehashing_still_breaks_the_round_seal(self):
        self.db.execute("DROP TRIGGER certification_round_component_no_update")
        replacement = json.dumps({"identity": "substituted-world"}, sort_keys=True, separators=(",", ":"))
        self.db.execute(
            "UPDATE certification_round_component SET fact_json = ?, fact_sha256 = ? "
            "WHERE round_id = 'round-1' AND component_name = 'world_save'",
            (replacement, hashlib.sha256(replacement.encode("utf-8")).hexdigest()),
        )
        with self.assertRaisesRegex(ScenarioRegistryStoreError, "stored certification manifest is invalid"):
            certification_round_manifest(self.db, "round-1")

    def test_legacy_full_round_rows_remain_retrievable_and_idempotent(self):
        legacy = self.manifest("legacy-round")
        authority = _issue_registry_certification_authority(
            self.db, round_id="legacy-round", binding_id=legacy["binding_id"],
            source_sha256="a" * 64, launch_token="legacy-test-token",
        )
        legacy["authority_id"] = authority["authority_id"]
        legacy["manifest_sha256"] = canonical_digest(
            {key: value for key, value in legacy.items() if key != "manifest_sha256"},
            domain="caol-round-manifest:v1",
        )
        manifest_json = json.dumps(legacy, sort_keys=True, separators=(",", ":"))
        self.db.execute(
            "INSERT INTO certification_round( round_id, scenario_lineage_id, authority_id, authority_kind, "
            "event_stream_id, binding_id, manifest_sha256, manifest_json ) VALUES(?,?,?,?,?,?,?,?)",
            (
                legacy["round_id"], legacy["scenario_lineage_id"], legacy["authority_id"],
                legacy["authority_kind"], legacy["event_stream_id"], legacy["binding_id"],
                legacy["manifest_sha256"], manifest_json,
            ),
        )
        for sequence, name in enumerate(legacy["binding"]["authoritative_components"], start=1):
            fact_json = json.dumps(
                legacy["binding"]["authoritative_components"][name],
                sort_keys=True, separators=(",", ":"),
            )
            self.db.execute(
                "INSERT INTO certification_round_component( round_id, component_sequence, component_name, fact_sha256, fact_json ) "
                "VALUES(?,?,?,?,?)",
                (legacy["round_id"], sequence, name,
                 hashlib.sha256(fact_json.encode("utf-8")).hexdigest(), fact_json),
            )
        self.assertEqual(certification_round_manifest(self.db, "legacy-round"), legacy)
        self.assertTrue(register_certification_round(self.db, legacy)["idempotent"])

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
