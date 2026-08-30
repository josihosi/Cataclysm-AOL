#!/usr/bin/env python3
"""Focused WEC class, authority, and final-gate controls."""

import copy
import sys
import unittest
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from startup_harness import seal_wec_authority  # noqa: E402
from wec_evidence import WEC_CLASSES, derive_final_gate_eligibility, validate_authority_fact  # noqa: E402
from scenario_registry_store import issue_wec_authority, open_registry  # noqa: E402


class WecEvidenceTest(unittest.TestCase):
    def test_each_class_requires_a_sealed_start_fact(self) -> None:
        for index, evidence_class in enumerate(WEC_CLASSES):
            fact = seal_wec_authority(
                evidence_class=evidence_class, authority="ordinary", run_id=f"run-{index}",
                binding_id=f"binding-{index}", source_sha256="source-hash",
            )
            self.assertEqual(validate_authority_fact(fact)["status"], "sealed")
            forged = copy.deepcopy(fact)
            forged["authority"] = "certification"
            self.assertEqual(validate_authority_fact(forged)["reason"], "authority_commitment_mismatch")

    def test_public_commitments_cannot_satisfy_final_gates(self) -> None:
        certification = seal_wec_authority(
            evidence_class="automated continuous-round certification", authority="certification",
            run_id="cert", binding_id="sealed", source_sha256="hash",
        )
        feel = seal_wec_authority(
            evidence_class="Windows feel evidence", authority="windows-josef", owner="Josef",
            run_id="feel", binding_id="sealed", source_sha256="hash",
        )
        self.assertFalse(derive_final_gate_eligibility(certification, proof_status="green", resolution="compatible")["automated_certification"])
        self.assertFalse(derive_final_gate_eligibility(feel, proof_status="green", resolution="compatible")["windows_feel"])
        self.assertFalse(derive_final_gate_eligibility(certification, proof_status="green", resolution="stale")["automated_certification"])
        self.assertFalse(derive_final_gate_eligibility({"evidence_class": "automated continuous-round certification"}, proof_status="green", resolution="compatible")["automated_certification"])

    def test_public_authority_api_cannot_issue_a_final_gate(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            connection = open_registry(directory + "/registry.sqlite3")
            with self.assertRaisesRegex(Exception, "cannot be caller-issued"):
                issue_wec_authority(
                    connection, evidence_class="automated continuous-round certification",
                    authority="certification", run_id="owned-run", binding_id="owned-binding",
                    source_sha256="a" * 64,
                )
            self.assertEqual(
                connection.execute("SELECT COUNT(*) FROM wec_authority_history").fetchone()[0], 0
            )
            connection.close()

    def test_even_registry_shaped_josef_labels_cannot_authenticate_windows_feel(self) -> None:
        forged_registry_fact = {
            "authority_id": "registry-shaped-but-not-human-authentication",
            "evidence_class": "Windows feel evidence",
            "authority": "windows-josef",
            "run_id": "feel-run",
            "binding_id": "feel-binding",
            "source_sha256": "a" * 64,
            "owner": "Josef",
            "commitment": "",
        }
        eligibility = derive_final_gate_eligibility(
            forged_registry_fact, proof_status="green", resolution="compatible", registry_owned=True,
        )
        self.assertFalse(eligibility["windows_feel"])
        self.assertEqual(eligibility["reason"], "external_windows_feel_not_machine_verifiable")


if __name__ == "__main__":
    unittest.main()
