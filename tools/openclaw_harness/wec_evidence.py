"""WEC evidence classes and fail-closed authority facts."""

from __future__ import annotations

import hashlib
import json
from typing import Any, Mapping

WEC_CLASSES = (
    "setup support",
    "build proof",
    "synthetic proof",
    "focused feature proof",
    "diagnostic replay",
    "automated continuous-round certification",
    "Windows feel evidence",
)
WEC_CLASS_SET = frozenset(WEC_CLASSES)
FINAL_GATE_CLASSES = {
    "automated_certification": "automated continuous-round certification",
    "windows_feel": "Windows feel evidence",
}


def authority_commitment(*, evidence_class: str, authority: str, run_id: str,
                        binding_id: str, source_sha256: str, owner: str = "") -> str:
    payload = {
        "evidence_class": evidence_class,
        "authority": authority,
        "run_id": run_id,
        "binding_id": binding_id,
        "source_sha256": source_sha256,
        "owner": owner,
    }
    return hashlib.sha256(json.dumps(payload, sort_keys=True, separators=(",", ":")).encode()).hexdigest()


def validate_authority_fact(value: Any) -> dict[str, Any]:
    if not isinstance(value, Mapping):
        return {"status": "unsealed", "reason": "missing_authority_fact"}
    fact = {key: value.get(key, "") for key in (
        "authority_id", "evidence_class", "authority", "run_id", "binding_id", "source_sha256", "owner", "commitment",
    )}
    if fact["evidence_class"] not in WEC_CLASS_SET:
        return {"status": "ineligible", "reason": "unknown_evidence_class", "fact": fact}
    if fact.get("authority_id") and not fact.get("commitment"):
        if not fact["run_id"] or not fact["binding_id"]:
            return {"status": "ineligible", "reason": "authority_identity_incomplete", "fact": fact}
        return {"status": "sealed", "fact": fact}
    expected = authority_commitment(**{key: str(fact[key]) for key in (
        "evidence_class", "authority", "run_id", "binding_id", "source_sha256", "owner",
    )})
    if not fact["run_id"] or not fact["binding_id"] or str(fact["commitment"]) != expected:
        return {"status": "ineligible", "reason": "authority_commitment_mismatch", "fact": fact}
    return {"status": "sealed", "fact": fact}


def derive_final_gate_eligibility(
    authority: Any, *, proof_status: str, resolution: str, registry_owned: bool = False,
    certification_round_valid: bool = True,
) -> dict[str, Any]:
    checked = validate_authority_fact(authority)
    result = {"automated_certification": False, "windows_feel": False, "reason": checked.get("reason", "not_final_class")}
    if checked.get("status") != "sealed" or proof_status != "green" or resolution != "compatible":
        return result
    fact = checked["fact"]
    # A public commitment can preserve focused history, but only an immutable
    # registry-issued authority may receive final-gate credit.
    if not registry_owned or not str(fact.get("authority_id", "")).strip():
        result["reason"] = "missing_registry_authority"
        return result
    if fact["evidence_class"] == FINAL_GATE_CLASSES["automated_certification"] and fact["authority"] == "certification":
        if not certification_round_valid:
            result["reason"] = "missing_current_certification_round"
            return result
        result["automated_certification"] = True
        result["reason"] = "sealed_certification"
    if fact["evidence_class"] == FINAL_GATE_CLASSES["windows_feel"]:
        # The local registry can preserve a labelled external attestation, but
        # it cannot authenticate the human who supplied it.  Keep that record
        # visible without converting metadata labels into a machine gate.
        result["reason"] = "external_windows_feel_not_machine_verifiable"
    return result
