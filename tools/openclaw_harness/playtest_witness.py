#!/usr/bin/env python3
"""Mechanical integrity boundary for LLM-authored playtest witnesses.

The journal owns facts.  The witness explains their causal meaning.  This
module validates identity, citations, contradictions, and evidence ceilings;
it deliberately does not encode a claim-specific interaction matrix.
"""

from __future__ import annotations

import hashlib
import json
from typing import Any, Mapping, Sequence


VERDICTS = frozenset({"proved", "contradicted", "inconclusive"})
DISPOSITIONS = frozenset({"accept", "continue", "repair", "change-strategy"})
FINDING_DISPOSITIONS = frozenset({"open", "repair", "revalidate", "closed"})
EVIDENCE_CEILINGS = {
    "zero-credit": 0,
    "setup-only": 1,
    "diagnostic": 2,
    "focused": 3,
    "certification": 4,
}


class WitnessError(ValueError):
    """The proposed witness is not grounded in its immutable journal."""


def _canonical(value: Any) -> str:
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":"))


def _digest(label: str, value: Any) -> str:
    return hashlib.sha256((label + ":" + _canonical(value)).encode("utf-8")).hexdigest()


def normalize_witness_charter(value: Mapping[str, Any]) -> dict[str, Any]:
    """Normalize the compact proof question without prescribing a trajectory."""
    if not isinstance(value, Mapping):
        raise WitnessError("witness_charter_must_be_an_object")
    required_text = ("claim", "material_proof", "current_uncertainty")
    normalized: dict[str, Any] = {"schema": "caol-playtest-witness-charter-v1"}
    for key in required_text:
        text = str(value.get(key, "")).strip()
        if not text:
            raise WitnessError("witness_charter_missing_" + key)
        normalized[key] = text
    for key in ("material_contradiction", "already_accepted_evidence",
                "forbidden_shortcuts", "honest_stop_conditions"):
        items = value.get(key, [])
        if isinstance(items, (str, bytes)) or not isinstance(items, list) or \
                any(not str(item).strip() for item in items):
            raise WitnessError("witness_charter_invalid_" + key)
        normalized[key] = [str(item).strip() for item in items]
    requested = str(value.get("requested_evidence_ceiling", "focused")).strip()
    if requested not in EVIDENCE_CEILINGS:
        raise WitnessError("witness_charter_invalid_evidence_ceiling")
    normalized["requested_evidence_ceiling"] = requested
    def normalize_predecessors(
        key: str, required_keys: tuple[str, ...], error_prefix: str,
    ) -> list[dict[str, str]]:
        predecessors = value.get(key)
        if predecessors is None:
            return []
        if isinstance(predecessors, (str, bytes)) or not isinstance(predecessors, list):
            raise WitnessError(error_prefix + "s")
        normalized_predecessors: list[dict[str, str]] = []
        predecessor_keys: set[tuple[str, ...]] = set()
        for predecessor in predecessors:
            if not isinstance(predecessor, Mapping):
                raise WitnessError(error_prefix)
            normalized_predecessor = {
                key: str(predecessor.get(key, "")).strip()
                for key in required_keys
            }
            if any(len(item) != 64 or any(char not in "0123456789abcdef" for char in item)
                   for item in normalized_predecessor.values()):
                raise WitnessError(error_prefix + "_identity")
            predecessor_key = tuple(normalized_predecessor.values())
            if predecessor_key in predecessor_keys:
                raise WitnessError("witness_charter_duplicate_" + key)
            predecessor_keys.add(predecessor_key)
            normalized_predecessors.append(normalized_predecessor)
        return normalized_predecessors

    predecessor_witnesses = normalize_predecessors(
        "accepted_predecessor_witnesses",
        ("witness_id", "charter_id", "journal_sha256"),
        "witness_charter_invalid_accepted_predecessor_witness",
    )
    if predecessor_witnesses:
        normalized["accepted_predecessor_witnesses"] = predecessor_witnesses
    predecessor_journals = normalize_predecessors(
        "cited_predecessor_journals",
        ("charter_id", "journal_sha256"),
        "witness_charter_invalid_cited_predecessor_journal",
    )
    if predecessor_journals:
        normalized["cited_predecessor_journals"] = predecessor_journals
    normalized["charter_id"] = _digest("caol-playtest-witness-charter-v1", normalized)
    return normalized


def _compact_observation(value: Mapping[str, Any]) -> dict[str, Any]:
    log = value.get("compact_log")
    log = log if isinstance(log, Mapping) else {}
    result = {
        "observation_id": str(value.get("observation_id", "")),
        "run_id": str(value.get("run_id", "")),
        "game_minutes": value.get("game_minutes"),
        "visible_entities": [
            {key: item.get(key) for key in (
                "handle", "kind", "name", "attitude", "dx", "dy",
                "fixture_actor_id", "typeid", "faction", "friendly",
                "aggro_character",
            )}
            for item in value.get("visible_entities", []) if isinstance(item, Mapping)
        ],
        "advertised_actions": list(value.get("advertised_actions", [])),
        "delta": value.get("delta", {}),
        "active_interruption": value.get("active_interruption"),
        "receipt_count": log.get("receipt_count", 0),
        "latest_receipt": log.get("latest_receipt"),
        "latest_transition": log.get("latest_transition"),
        "actor_owners": log.get("actor_owners", []),
        "persistence": log.get("persistence", "unavailable"),
        "contradictory_evidence": log.get("contradictory_evidence", []),
        "production_channel_observation": log.get(
            "production_channel_observation", {
                "status": "unavailable", "eligible": False,
                "records": [], "channels": [],
                "issues": ["channel_observation_unavailable"],
            }
        ),
        "child_resources": log.get("child_resources", {}),
    }
    return result


def _compact_transcript_event(event: Mapping[str, Any]) -> dict[str, Any]:
    kind = str(event.get("kind", "event"))
    if kind == "observation" and isinstance(event.get("value"), Mapping):
        return {"kind": kind, "value": _compact_observation(event["value"])}
    if kind == "action":
        result = event.get("result")
        result = result if isinstance(result, Mapping) else {}
        receipt = result.get("receipt")
        receipt = receipt if isinstance(receipt, Mapping) else {}
        native = receipt.get("native_receipt")
        native = native if isinstance(native, Mapping) else receipt
        return {
            "kind": kind,
            "action_id": str(event.get("action_id", "")),
            "observation_id": str(event.get("observation_id", "")),
            "native_receipt": {
                key: native.get(key) for key in (
                    "run_id", "frame_id", "action_id", "accepted", "reason"
                )
            },
            "recovery": "recovery_receipt" in result,
        }
    return {key: value for key, value in event.items() if key not in {"minimap", "visible_local"}}


def build_evidence_journal(
    *, charter: Mapping[str, Any], identity: Mapping[str, Any],
    transcript: Sequence[Mapping[str, Any]], terminal: Mapping[str, Any],
    evidence_ceiling: str,
) -> dict[str, Any]:
    """Seal compact cited facts while retaining run-local identities."""
    normalized_charter = normalize_witness_charter(charter)
    if evidence_ceiling not in EVIDENCE_CEILINGS:
        raise WitnessError("journal_invalid_evidence_ceiling")
    required_identity = ("scenario_id", "source_identity", "executable_identity",
                         "run_id", "binding_id")
    if any(not str(identity.get(key, "")).strip() for key in required_identity):
        raise WitnessError("journal_incomplete_identity")
    entries: list[dict[str, Any]] = []

    def append(kind: str, value: Mapping[str, Any]) -> None:
        sequence = len(entries) + 1
        payload = dict(value)
        entries.append({
            "citation_id": f"J{sequence:04d}",
            "sequence": sequence,
            "kind": kind,
            "value": payload,
            "value_sha256": _digest("caol-playtest-journal-value-v1", payload),
        })

    append("identity", dict(identity))
    for event in transcript:
        if isinstance(event, Mapping):
            compact = _compact_transcript_event(event)
            event_run_id = ""
            if compact.get("kind") == "observation" and isinstance(compact.get("value"), Mapping):
                candidate_run_id = compact["value"].get("run_id", "")
                event_run_id = candidate_run_id if isinstance(candidate_run_id, str) else ""
            elif compact.get("kind") == "action" and isinstance(compact.get("native_receipt"), Mapping):
                candidate_run_id = compact["native_receipt"].get("run_id", "")
                event_run_id = candidate_run_id if isinstance(candidate_run_id, str) else ""
            if event_run_id and event_run_id != str(identity["run_id"]):
                raise WitnessError("journal_event_run_identity_mismatch")
            append(str(compact.get("kind", "event")), compact)
            if compact.get("kind") == "observation":
                observation = compact.get("value")
                contradictions = observation.get("contradictory_evidence", []) \
                    if isinstance(observation, Mapping) else []
                for contradiction in contradictions:
                    if isinstance(contradiction, Mapping):
                        append("contradiction", dict(contradiction))
    terminal_run_id = str(terminal.get("run_id", ""))
    if terminal_run_id and terminal_run_id != str(identity["run_id"]):
        raise WitnessError("journal_terminal_run_identity_mismatch")
    append("terminal", dict(terminal))
    journal = {
        "schema": "caol-playtest-evidence-journal-v1",
        "charter_id": normalized_charter["charter_id"],
        "identity": dict(identity),
        "evidence_ceiling": evidence_ceiling,
        "entries": entries,
    }
    journal["journal_sha256"] = _digest("caol-playtest-evidence-journal-v1", journal)
    return journal


def _validate_journal_integrity(
    normalized_charter: Mapping[str, Any], journal: Mapping[str, Any],
) -> str:
    schema = str(journal.get("schema", ""))
    labels = {
        "caol-playtest-evidence-journal-v1": "caol-playtest-evidence-journal-v1",
        "caol-playtest-evidence-journal-set-v1": "caol-playtest-evidence-journal-set-v1",
    }
    if schema not in labels or journal.get("charter_id") != normalized_charter["charter_id"]:
        raise WitnessError("witness_journal_charter_mismatch")
    expected_digest = _digest(
        labels[schema],
        {key: value for key, value in journal.items() if key != "journal_sha256"},
    )
    if journal.get("journal_sha256") != expected_digest:
        raise WitnessError("witness_journal_digest_mismatch")
    return schema


def _validate_source_journal_integrity(
    normalized_charter: Mapping[str, Any], journal: Mapping[str, Any],
) -> str:
    """Accept the current charter or one exact coordinator-cited predecessor."""
    schema = _validate_journal_integrity(normalized_charter, journal) \
        if journal.get("charter_id") == normalized_charter["charter_id"] else ""
    if schema:
        return schema
    source_charter_id = str(journal.get("charter_id", ""))
    source_digest = str(journal.get("journal_sha256", ""))
    cited_predecessors = list(normalized_charter.get(
        "accepted_predecessor_witnesses", []
    )) + list(normalized_charter.get("cited_predecessor_journals", []))
    for predecessor in cited_predecessors:
        if not isinstance(predecessor, Mapping):
            continue
        if source_charter_id == predecessor.get("charter_id") and \
                source_digest == predecessor.get("journal_sha256"):
            labels = {
                "caol-playtest-evidence-journal-v1": "caol-playtest-evidence-journal-v1",
                "caol-playtest-evidence-journal-set-v1": "caol-playtest-evidence-journal-set-v1",
            }
            schema = str(journal.get("schema", ""))
            if schema not in labels:
                raise WitnessError("witness_journal_charter_mismatch")
            expected_digest = _digest(
                labels[schema],
                {key: value for key, value in journal.items() if key != "journal_sha256"},
            )
            if source_digest != expected_digest:
                raise WitnessError("witness_journal_digest_mismatch")
            return schema
    raise WitnessError("witness_journal_charter_mismatch")


def compose_evidence_journals(
    *, charter: Mapping[str, Any], journals: Sequence[Mapping[str, Any]],
) -> dict[str, Any]:
    """Seal several independently bound runs into one citable comparison journal."""
    normalized_charter = normalize_witness_charter(charter)
    if len(journals) < 2:
        raise WitnessError("journal_set_requires_multiple_runs")
    entries: list[dict[str, Any]] = []
    identities: list[dict[str, Any]] = []
    source_digests: list[str] = []
    ceilings: list[str] = []
    source_charter_ids: list[str] = []
    run_keys: set[tuple[str, str]] = set()
    for run_index, journal in enumerate(journals, start=1):
        if _validate_source_journal_integrity(normalized_charter, journal) != \
                "caol-playtest-evidence-journal-v1":
            raise WitnessError("journal_set_cannot_nest")
        identity = journal.get("identity")
        if not isinstance(identity, Mapping):
            raise WitnessError("journal_set_identity_missing")
        run_key = (str(identity.get("run_id", "")), str(identity.get("binding_id", "")))
        if not all(run_key) or run_key in run_keys:
            raise WitnessError("journal_set_run_identity_missing_or_duplicate")
        run_keys.add(run_key)
        identities.append(dict(identity))
        source_digests.append(str(journal["journal_sha256"]))
        source_charter_ids.append(str(journal["charter_id"]))
        ceiling = str(journal.get("evidence_ceiling", "zero-credit"))
        if ceiling not in EVIDENCE_CEILINGS:
            raise WitnessError("journal_invalid_evidence_ceiling")
        ceilings.append(ceiling)
        for entry in journal.get("entries", []):
            if not isinstance(entry, Mapping):
                raise WitnessError("witness_journal_entries_missing")
            copied = dict(entry)
            copied["citation_id"] = f"R{run_index}:{entry.get('citation_id', '')}"
            copied["run_index"] = run_index
            copied["source_journal_sha256"] = journal["journal_sha256"]
            entries.append(copied)
    ceiling = min(ceilings, key=lambda value: EVIDENCE_CEILINGS[value])
    result: dict[str, Any] = {
        "schema": "caol-playtest-evidence-journal-set-v1",
        "charter_id": normalized_charter["charter_id"],
        "identities": identities,
        "source_journal_sha256s": source_digests,
        "source_charter_ids": source_charter_ids,
        "evidence_ceiling": ceiling,
        "entries": entries,
    }
    result["journal_sha256"] = _digest("caol-playtest-evidence-journal-set-v1", result)
    return result


def _path_value(value: Any, path: str) -> Any:
    current = value
    for part in path.split(".") if path else ():
        if isinstance(current, Mapping) and part in current:
            current = current[part]
        elif isinstance(current, list) and part.isdecimal() and int(part) < len(current):
            current = current[int(part)]
        else:
            raise WitnessError("witness_citation_path_missing:" + path)
    return current


def validate_witness_statement(
    *, charter: Mapping[str, Any], journal: Mapping[str, Any],
    statement: Mapping[str, Any],
) -> dict[str, Any]:
    """Validate facts without deciding whether the causal argument is persuasive."""
    normalized_charter = normalize_witness_charter(charter)
    _validate_journal_integrity(normalized_charter, journal)
    if not isinstance(statement, Mapping):
        raise WitnessError("witness_statement_must_be_an_object")
    verdict = str(statement.get("verdict", "")).strip()
    disposition = str(statement.get("recommended_disposition", "")).strip()
    if verdict not in VERDICTS:
        raise WitnessError("witness_invalid_verdict")
    if disposition not in DISPOSITIONS:
        raise WitnessError("witness_invalid_disposition")
    supported_claim = str(statement.get("smallest_supported_claim", "")).strip()
    causal_account = str(statement.get("causal_account", "")).strip()
    if not supported_claim or not causal_account:
        raise WitnessError("witness_missing_claim_or_causal_account")
    entries = journal.get("entries")
    if not isinstance(entries, list):
        raise WitnessError("witness_journal_entries_missing")
    by_id = {
        str(entry.get("citation_id", "")): entry
        for entry in entries if isinstance(entry, Mapping)
    }
    citations = statement.get("citations")
    if not isinstance(citations, list) or not citations:
        raise WitnessError("witness_requires_citations")
    cited: set[str] = set()
    normalized_citations: list[dict[str, Any]] = []
    for citation in citations:
        if not isinstance(citation, Mapping):
            raise WitnessError("witness_citation_must_be_an_object")
        citation_id = str(citation.get("citation_id", "")).strip()
        meaning = str(citation.get("meaning", "")).strip()
        if citation_id not in by_id or not meaning:
            raise WitnessError("witness_citation_missing_or_unknown:" + citation_id)
        checks = citation.get("checks", {})
        if not isinstance(checks, Mapping):
            raise WitnessError("witness_citation_checks_must_be_an_object")
        for path, expected in checks.items():
            actual = _path_value(by_id[citation_id].get("value"), str(path))
            if actual != expected:
                raise WitnessError("witness_citation_value_mismatch:" + citation_id + ":" + str(path))
        cited.add(citation_id)
        normalized_citations.append({
            "citation_id": citation_id, "meaning": meaning, "checks": dict(checks),
        })
    contradictions = {
        citation_id for citation_id, entry in by_id.items()
        if entry.get("kind") == "contradiction"
    }
    disclosed = statement.get("contradictions", [])
    if isinstance(disclosed, (str, bytes)) or not isinstance(disclosed, list):
        raise WitnessError("witness_contradictions_must_be_a_list")
    disclosed_ids = {
        str(item.get("citation_id", "")).strip()
        for item in disclosed if isinstance(item, Mapping)
    }
    if contradictions - disclosed_ids:
        raise WitnessError("witness_omits_supplied_contradiction")
    claimed_ceiling = str(statement.get(
        "evidence_ceiling", journal.get("evidence_ceiling", "zero-credit")
    )).strip()
    journal_ceiling = str(journal.get("evidence_ceiling", "zero-credit"))
    requested_ceiling = str(normalized_charter["requested_evidence_ceiling"])
    if claimed_ceiling not in EVIDENCE_CEILINGS or \
            EVIDENCE_CEILINGS[claimed_ceiling] > EVIDENCE_CEILINGS[journal_ceiling] or \
            EVIDENCE_CEILINGS[claimed_ceiling] > EVIDENCE_CEILINGS[requested_ceiling]:
        raise WitnessError("witness_evidence_promotion_rejected")
    normalized = {
        "schema": "caol-playtest-witness-statement-v1",
        "verdict": verdict,
        "smallest_supported_claim": supported_claim,
        "causal_account": causal_account,
        "citations": normalized_citations,
        "material_deviations": list(statement.get("material_deviations", [])),
        "contradictions": list(disclosed),
        "remaining_unknowns": list(statement.get("remaining_unknowns", [])),
        "recommended_disposition": disposition,
        "evidence_ceiling": claimed_ceiling,
    }
    normalized["witness_sha256"] = _digest("caol-playtest-witness-statement-v1", normalized)
    return {
        "status": "mechanically_valid",
        "charter": normalized_charter,
        "journal_sha256": journal["journal_sha256"],
        "witness": normalized,
        "cited_entry_count": len(cited),
        "contradiction_count": len(contradictions),
        "causal_sufficiency_owner": "coordinator_llm",
        "proof_promotion_authority": False,
    }


def validate_witness_bundle(
    *, charter: Mapping[str, Any], journal: Mapping[str, Any],
    bundle: Mapping[str, Any],
) -> dict[str, Any]:
    """Validate independent claim conclusions and routed defects from one run.

    A feature defect may contradict one claim without erasing facts that still
    answer another. Findings remain coordinator input; this validator only
    binds them to the immutable journal and identifies their affected scope.
    """
    if not isinstance(bundle, Mapping) or bundle.get("schema") != \
            "caol-playtest-witness-bundle-v1":
        raise WitnessError("witness_bundle_schema_missing_or_unknown")
    claims = bundle.get("claims")
    if isinstance(claims, (str, bytes)) or not isinstance(claims, list) or not claims:
        raise WitnessError("witness_bundle_requires_claims")
    normalized_claims: list[dict[str, Any]] = []
    claim_ids: set[str] = set()
    for claim in claims:
        if not isinstance(claim, Mapping):
            raise WitnessError("witness_bundle_claim_must_be_an_object")
        claim_id = str(claim.get("claim_id", "")).strip()
        statement = claim.get("statement")
        if not claim_id or claim_id in claim_ids or not isinstance(statement, Mapping):
            raise WitnessError("witness_bundle_claim_identity_missing_or_duplicate")
        claim_ids.add(claim_id)
        normalized_claims.append({
            "claim_id": claim_id,
            "validation": validate_witness_statement(
                charter=charter, journal=journal, statement=statement,
            ),
        })

    entries = journal.get("entries")
    if not isinstance(entries, list):
        raise WitnessError("witness_journal_entries_missing")
    citation_ids = {
        str(entry.get("citation_id", ""))
        for entry in entries if isinstance(entry, Mapping)
    }
    findings = bundle.get("findings", [])
    if isinstance(findings, (str, bytes)) or not isinstance(findings, list):
        raise WitnessError("witness_bundle_findings_must_be_a_list")
    normalized_findings: list[dict[str, Any]] = []
    finding_ids: set[str] = set()
    for finding in findings:
        if not isinstance(finding, Mapping):
            raise WitnessError("witness_bundle_finding_must_be_an_object")
        finding_id = str(finding.get("finding_id", "")).strip()
        observed_defect = str(finding.get("observed_defect", "")).strip()
        disposition = str(finding.get("disposition", "")).strip()
        next_action = str(finding.get("next_action", "")).strip()
        citations = finding.get("citations")
        affected = finding.get("affected_claims")
        unaffected = finding.get("unaffected_claims", [])
        lists = (citations, affected, unaffected)
        if not finding_id or finding_id in finding_ids or not observed_defect or \
                disposition not in FINDING_DISPOSITIONS or not next_action:
            raise WitnessError("witness_bundle_finding_incomplete_or_duplicate")
        if any(isinstance(items, (str, bytes)) or not isinstance(items, list) or
               any(not isinstance(item, str) or not item.strip() for item in items)
               for items in lists) or not citations or not affected:
            raise WitnessError("witness_bundle_finding_scope_or_citations_invalid")
        if any(citation not in citation_ids for citation in citations):
            raise WitnessError("witness_bundle_finding_citation_unknown")
        affected_set = set(affected)
        unaffected_set = set(unaffected)
        if not affected_set.issubset(claim_ids) or not unaffected_set.issubset(claim_ids) or \
                affected_set & unaffected_set:
            raise WitnessError("witness_bundle_finding_claim_scope_invalid")
        finding_ids.add(finding_id)
        normalized_findings.append({
            "finding_id": finding_id,
            "observed_defect": observed_defect,
            "citations": list(citations),
            "affected_claims": list(affected),
            "unaffected_claims": list(unaffected),
            "disposition": disposition,
            "next_action": next_action,
            "run_id": str(journal.get("identity", {}).get("run_id", "")),
            "journal_sha256": str(journal.get("journal_sha256", "")),
        })
    normalized = {
        "schema": "caol-playtest-witness-bundle-v1",
        "claims": normalized_claims,
        "findings": normalized_findings,
    }
    normalized["bundle_sha256"] = _digest("caol-playtest-witness-bundle-v1", normalized)
    return {
        "status": "mechanically_valid_bundle",
        "journal_sha256": journal["journal_sha256"],
        "claim_count": len(normalized_claims),
        "finding_count": len(normalized_findings),
        "bundle": normalized,
        "causal_sufficiency_owner": "coordinator_llm",
        "finding_record_owner": "coordinator_llm",
        "proof_promotion_authority": False,
    }


def review_witness(
    validation: Mapping[str, Any], *, decision: str, rationale: str,
    concrete_risk: str = "",
) -> dict[str, Any]:
    """Record intellectual scrutiny while repairing clerical omissions locally."""
    if validation.get("status") != "mechanically_valid":
        raise WitnessError("coordinator_requires_valid_witness")
    if decision not in DISPOSITIONS:
        raise WitnessError("coordinator_invalid_witness_decision")
    reason = str(rationale).strip()
    if not reason:
        raise WitnessError("coordinator_witness_rationale_required")
    if decision != "accept" and not str(concrete_risk).strip():
        raise WitnessError("coordinator_rejection_requires_concrete_causal_risk")
    return {
        "schema": "caol-playtest-witness-review-v1",
        "decision": decision,
        "rationale": reason,
        "concrete_risk": str(concrete_risk).strip(),
        "clerical_normalization": "derived_without_gameplay_replay",
        "witness_sha256": validation.get("witness", {}).get("witness_sha256"),
    }


__all__ = [
    "WitnessError", "build_evidence_journal", "compose_evidence_journals",
    "normalize_witness_charter",
    "review_witness", "validate_witness_statement",
]
