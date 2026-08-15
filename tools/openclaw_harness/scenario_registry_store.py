#!/usr/bin/env python3
"""SQLite storage foundation for the authoritative scenario registry.

This module owns database opening, schema migration, and transaction semantics.
Projection rebuild, report ingestion, resolution, tokens, and CLI behavior are
deliberately separate later slices.
"""

from __future__ import annotations

from contextlib import contextmanager
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import sqlite3
from typing import Any, Callable, Dict, Iterator, List, Mapping, Optional, Sequence, Tuple

from scenario_registry import CAPABILITY_NAMESPACE_PREFIXES, ManifestValidationError, validate_manifest


SCHEMA_VERSION = 2
Migration = Tuple[int, str, Callable[[sqlite3.Connection], None]]


class ScenarioRegistryStoreError(RuntimeError):
    """The registry database could not be opened or migrated safely."""


class ScenarioRegistryQueryError(ValueError):
    """A registry query request or explicit candidate fact is not well-typed."""


@dataclass(frozen=True)
class RegistryQueryPredicate:
    """One typed hard requirement or ordered soft preference."""

    key: str
    op: str
    value: Any = None
    minimum: Optional[float] = None
    maximum: Optional[float] = None
    minimum_evidence: str = "run-verified"


@dataclass(frozen=True)
class RegistryQueryRequest:
    """The parsed query contract; no registry or runtime state is resolved here."""

    requirements: Tuple[RegistryQueryPredicate, ...]
    preferences: Tuple[RegistryQueryPredicate, ...]


@dataclass(frozen=True)
class RegistryQueryPredicateResult:
    """A complete predicate observation, including fail-closed evidence details."""

    key: str
    op: str
    expected: Any
    observed: Any
    evidence_state: str
    passed: bool
    reason: str


@dataclass(frozen=True)
class RegistryQueryCandidateResult:
    """All hard observations precede any preference observations for one candidate."""

    scenario_id: str
    hard_results: Tuple[RegistryQueryPredicateResult, ...]
    preference_results: Tuple[RegistryQueryPredicateResult, ...]

    @property
    def hard_valid(self) -> bool:
        return all(result.passed for result in self.hard_results)

    @property
    def preference_vector(self) -> Tuple[bool, ...]:
        return tuple(result.passed for result in self.preference_results)


@dataclass(frozen=True)
class RegistryQueryEvaluation:
    """Pure deterministic selection output for later lifecycle/evidence owners."""

    candidates: Tuple[RegistryQueryCandidateResult, ...]
    ranked_scenario_ids: Tuple[str, ...]


@dataclass(frozen=True)
class RegistryQueryCandidateSnapshot:
    """One fixed registry-authority candidate prepared for pure evaluation."""

    scenario_id: str
    facts: Mapping[str, Mapping[str, Any]]
    lifecycle_state: str
    token_eligible: bool
    explanation: Mapping[str, Any]


@dataclass(frozen=True)
class RegistryStoredQueryEvaluation:
    """Registry snapshots plus their fixed typed-predicate evaluation."""

    candidates: Tuple[RegistryQueryCandidateSnapshot, ...]
    evaluation: RegistryQueryEvaluation


@dataclass(frozen=True)
class RegistryQueryExecution:
    """The append-only audit result of a non-executing registry query."""

    query_id: str
    query_sha256: str
    evaluation: RegistryStoredQueryEvaluation
    token_id: Optional[str]
    draft_path: Optional[str]


_QUERY_OPERATORS = frozenset({"eq", "contains", "present", "absent", "range"})
# Query floors use the WEC's public authority labels.  They deliberately do
# not reuse registry resolution labels such as the internal hard_proven state.
_PUBLIC_EVIDENCE_MINIMUM_RANKS = {
    "declared": 0,
    "inspected": 1,
    "run-verified": 2,
}
_KNOWN_EVIDENCE_STATES = frozenset({
    "declared",
    "inspected",
    "run-verified",
    "unknown",
    "stale",
    "contradicted",
})
_KNOWN_PROOF_DEPTHS = frozenset({
    "startup",
    "interaction",
    "terminal",
    "persistence",
    "replay",
})


def _is_number(value: Any) -> bool:
    return type(value) in {int, float} and value == value and value not in {float("inf"), float("-inf")}


def _is_scalar(value: Any) -> bool:
    return (
        value is None
        or type(value) in {bool, int, str}
        or (type(value) is float and _is_number(value))
    )


def _is_scalar_list(value: Any) -> bool:
    return isinstance(value, list) and all(_is_scalar(item) for item in value)


def _is_bounded_value(value: Any) -> bool:
    if _is_scalar(value) or _is_scalar_list(value):
        return True
    return isinstance(value, dict) and all(
        isinstance(key, str) and key and (_is_scalar(item) or _is_scalar_list(item))
        for key, item in value.items()
    )


def _typed_equal(left: Any, right: Any) -> bool:
    """Compare JSON-shaped values without Python's bool/int coercion."""
    if type(left) is not type(right):
        return False
    if isinstance(left, list):
        return len(left) == len(right) and all(_typed_equal(a, b) for a, b in zip(left, right))
    if isinstance(left, dict):
        return set(left) == set(right) and all(_typed_equal(left[key], right[key]) for key in left)
    return left == right


def _query_error(field: str, message: str) -> ScenarioRegistryQueryError:
    return ScenarioRegistryQueryError(f"Invalid registry query {field}: {message}")


def _parse_predicate(raw: Any, *, field: str) -> RegistryQueryPredicate:
    if not isinstance(raw, Mapping):
        raise _query_error(field, "predicate must be an object")
    key = raw.get("key")
    if not isinstance(key, str) or not key.strip():
        raise _query_error(field, "key must be a non-empty string")
    if not any(key.startswith(prefix) for prefix in CAPABILITY_NAMESPACE_PREFIXES):
        raise _query_error(field, f"key {key!r} is outside the stable capability namespaces")
    op = raw.get("op")
    if not isinstance(op, str) or op not in _QUERY_OPERATORS:
        raise _query_error(field, "op must be one of eq, contains, present, absent, range")
    minimum_evidence = raw.get("minimum_evidence", "run-verified")
    if not isinstance(minimum_evidence, str) or minimum_evidence not in _PUBLIC_EVIDENCE_MINIMUM_RANKS:
        raise _query_error(field, "minimum_evidence must be declared, inspected, or run-verified")

    common_fields = {"key", "op", "minimum_evidence"}
    if op in {"eq", "contains"}:
        allowed_fields = common_fields | {"value"}
        if set(raw) - allowed_fields:
            raise _query_error(field, f"{op} contains unsupported fields")
        if "value" not in raw:
            raise _query_error(field, "value is required")
        value = raw["value"]
        if op == "eq" and not _is_bounded_value(value):
            raise _query_error(field, "eq value must be a JSON scalar, scalar list, or bounded object")
        if op == "contains" and not _is_scalar(value):
            raise _query_error(field, "contains value must be a JSON scalar")
        return RegistryQueryPredicate(
            key=key,
            op=op,
            value=value,
            minimum_evidence=minimum_evidence,
        )

    if op in {"present", "absent"}:
        if set(raw) - common_fields:
            raise _query_error(field, f"{op} does not accept value or range bounds")
        return RegistryQueryPredicate(key=key, op=op, minimum_evidence=minimum_evidence)

    allowed_fields = common_fields | {"minimum", "maximum"}
    if set(raw) - allowed_fields:
        raise _query_error(field, "range does not accept value or unsupported fields")
    has_minimum = "minimum" in raw
    has_maximum = "maximum" in raw
    if not has_minimum and not has_maximum:
        raise _query_error(field, "range requires minimum, maximum, or both")
    minimum = raw.get("minimum")
    maximum = raw.get("maximum")
    if has_minimum and not _is_number(minimum):
        raise _query_error(field, "minimum must be a finite number and not a boolean")
    if has_maximum and not _is_number(maximum):
        raise _query_error(field, "maximum must be a finite number and not a boolean")
    if has_minimum and has_maximum and float(minimum) > float(maximum):
        raise _query_error(field, "minimum cannot exceed maximum")
    return RegistryQueryPredicate(
        key=key,
        op=op,
        minimum=float(minimum) if has_minimum else None,
        maximum=float(maximum) if has_maximum else None,
        minimum_evidence=minimum_evidence,
    )


def parse_registry_query_request(raw: Any) -> RegistryQueryRequest:
    """Parse the public requirements/preferences shape without touching SQLite.

    Candidate facts are intentionally supplied separately by later lifecycle and
    evidence owners.  This function never accepts prose fields and it never
    constructs SQL from user-provided data.
    """
    if not isinstance(raw, Mapping):
        raise _query_error("", "top level must be an object")
    allowed_fields = {"requirements", "preferences"}
    if set(raw) - allowed_fields:
        raise _query_error("", "top level contains unsupported fields")
    for name in ("requirements", "preferences"):
        if name not in raw:
            raise _query_error("", f"{name} is required")
        if not isinstance(raw[name], list):
            raise _query_error(name, "must be a list")
    return RegistryQueryRequest(
        requirements=tuple(
            _parse_predicate(predicate, field=f"requirements[{index}]")
            for index, predicate in enumerate(raw["requirements"])
        ),
        preferences=tuple(
            _parse_predicate(predicate, field=f"preferences[{index}]")
            for index, predicate in enumerate(raw["preferences"])
        ),
    )


def _fact_for_key(facts: Any, key: str) -> Tuple[Optional[Mapping[str, Any]], str]:
    if not isinstance(facts, Mapping):
        return None, "malformed_facts"
    if key not in facts:
        return None, "unknown_fact"
    fact = facts[key]
    if not isinstance(fact, Mapping):
        return None, "malformed_fact"
    return fact, ""


def _predicate_expected(predicate: RegistryQueryPredicate) -> Any:
    if predicate.op == "range":
        return {"minimum": predicate.minimum, "maximum": predicate.maximum}
    if predicate.op in {"present", "absent"}:
        return predicate.op
    return predicate.value


def _evaluate_registry_predicate(
    predicate: RegistryQueryPredicate,
    facts: Any,
) -> RegistryQueryPredicateResult:
    expected = _predicate_expected(predicate)
    fact, fact_problem = _fact_for_key(facts, predicate.key)
    if fact is None:
        return RegistryQueryPredicateResult(
            key=predicate.key,
            op=predicate.op,
            expected=expected,
            observed=None,
            evidence_state="unknown",
            passed=False,
            reason=fact_problem,
        )
    evidence_state = fact.get("evidence_state")
    if not isinstance(evidence_state, str) or evidence_state not in _KNOWN_EVIDENCE_STATES:
        return RegistryQueryPredicateResult(
            key=predicate.key,
            op=predicate.op,
            expected=expected,
            observed=None,
            evidence_state="unknown",
            passed=False,
            reason="malformed_evidence_state",
        )
    proof_depth = fact.get("proof_depth")
    if proof_depth is not None and (
        not isinstance(proof_depth, str) or proof_depth not in _KNOWN_PROOF_DEPTHS
    ):
        return RegistryQueryPredicateResult(
            key=predicate.key,
            op=predicate.op,
            expected=expected,
            observed=None,
            evidence_state=evidence_state,
            passed=False,
            reason="malformed_proof_depth",
        )
    present = fact.get("present")
    if type(present) is not bool:
        return RegistryQueryPredicateResult(
            key=predicate.key,
            op=predicate.op,
            expected=expected,
            observed=None,
            evidence_state=evidence_state,
            passed=False,
            reason="malformed_presence",
        )
    if evidence_state in {"unknown", "stale", "contradicted"}:
        return RegistryQueryPredicateResult(
            key=predicate.key,
            op=predicate.op,
            expected=expected,
            observed=fact.get("value") if present else None,
            evidence_state=evidence_state,
            passed=False,
            reason=evidence_state,
        )
    if _PUBLIC_EVIDENCE_MINIMUM_RANKS[evidence_state] < _PUBLIC_EVIDENCE_MINIMUM_RANKS[predicate.minimum_evidence]:
        return RegistryQueryPredicateResult(
            key=predicate.key,
            op=predicate.op,
            expected=expected,
            observed=fact.get("value") if present else None,
            evidence_state=evidence_state,
            passed=False,
            reason="below_minimum_evidence",
        )
    if not present:
        return RegistryQueryPredicateResult(
            key=predicate.key,
            op=predicate.op,
            expected=expected,
            observed=None,
            evidence_state=evidence_state,
            passed=predicate.op == "absent",
            reason="matched" if predicate.op == "absent" else "fact_absent",
        )
    if "value" not in fact or not _is_bounded_value(fact["value"]):
        return RegistryQueryPredicateResult(
            key=predicate.key,
            op=predicate.op,
            expected=expected,
            observed=None,
            evidence_state=evidence_state,
            passed=False,
            reason="malformed_value",
        )
    observed = fact["value"]
    if predicate.op == "present":
        return RegistryQueryPredicateResult(
            key=predicate.key,
            op=predicate.op,
            expected=expected,
            observed=observed,
            evidence_state=evidence_state,
            passed=True,
            reason="matched",
        )
    if predicate.op == "absent":
        return RegistryQueryPredicateResult(
            key=predicate.key,
            op=predicate.op,
            expected=expected,
            observed=observed,
            evidence_state=evidence_state,
            passed=False,
            reason="fact_present",
        )
    if predicate.op == "eq":
        matched = _typed_equal(observed, predicate.value)
        return RegistryQueryPredicateResult(
            key=predicate.key,
            op=predicate.op,
            expected=expected,
            observed=observed,
            evidence_state=evidence_state,
            passed=matched,
            reason="matched" if matched else "equality_mismatch",
        )
    if predicate.op == "contains":
        if not isinstance(observed, list):
            return RegistryQueryPredicateResult(
                key=predicate.key,
                op=predicate.op,
                expected=expected,
                observed=observed,
                evidence_state=evidence_state,
                passed=False,
                reason="non_container_value",
            )
        matched = any(_typed_equal(item, predicate.value) for item in observed)
        return RegistryQueryPredicateResult(
            key=predicate.key,
            op=predicate.op,
            expected=expected,
            observed=observed,
            evidence_state=evidence_state,
            passed=matched,
            reason="matched" if matched else "containment_mismatch",
        )
    if not _is_number(observed):
        return RegistryQueryPredicateResult(
            key=predicate.key,
            op=predicate.op,
            expected=expected,
            observed=observed,
            evidence_state=evidence_state,
            passed=False,
            reason="non_numeric_value",
        )
    number = float(observed)
    matched = (
        (predicate.minimum is None or number >= predicate.minimum)
        and (predicate.maximum is None or number <= predicate.maximum)
    )
    return RegistryQueryPredicateResult(
        key=predicate.key,
        op=predicate.op,
        expected=expected,
        observed=observed,
        evidence_state=evidence_state,
        passed=matched,
        reason="matched" if matched else "outside_range",
    )


def evaluate_registry_query(
    request: RegistryQueryRequest,
    candidates: Sequence[Mapping[str, Any]],
) -> RegistryQueryEvaluation:
    """Evaluate explicit facts only; lifecycle, bindings, tokens, and SQL stay elsewhere."""
    candidate_results: List[RegistryQueryCandidateResult] = []
    seen_scenario_ids = set()
    for index, candidate in enumerate(candidates):
        if not isinstance(candidate, Mapping):
            raise ScenarioRegistryQueryError(f"Invalid registry candidate {index}: must be an object")
        scenario_id = candidate.get("scenario_id")
        if not isinstance(scenario_id, str) or not scenario_id.strip():
            raise ScenarioRegistryQueryError(f"Invalid registry candidate {index}: scenario_id must be a non-empty string")
        if scenario_id in seen_scenario_ids:
            raise ScenarioRegistryQueryError(f"Invalid registry candidate {index}: duplicate scenario_id {scenario_id!r}")
        seen_scenario_ids.add(scenario_id)
        facts = candidate.get("facts")
        hard_results = tuple(
            _evaluate_registry_predicate(predicate, facts)
            for predicate in request.requirements
        )
        preference_results: Tuple[RegistryQueryPredicateResult, ...] = ()
        if all(result.passed for result in hard_results):
            preference_results = tuple(
                _evaluate_registry_predicate(predicate, facts)
                for predicate in request.preferences
            )
        candidate_results.append(RegistryQueryCandidateResult(
            scenario_id=scenario_id,
            hard_results=hard_results,
            preference_results=preference_results,
        ))
    hard_valid = [result for result in candidate_results if result.hard_valid]
    ranked = sorted(
        hard_valid,
        key=lambda result: (
            tuple(0 if preference else 1 for preference in result.preference_vector),
            result.scenario_id,
        ),
    )
    return RegistryQueryEvaluation(
        candidates=tuple(candidate_results),
        ranked_scenario_ids=tuple(result.scenario_id for result in ranked),
    )


def _json_object(value: str, field: str) -> Mapping[str, Any]:
    try:
        parsed = json.loads(value)
    except json.JSONDecodeError as exc:
        raise ScenarioRegistryStoreError(f"Registry {field} is not valid JSON") from exc
    if not isinstance(parsed, Mapping):
        raise ScenarioRegistryStoreError(f"Registry {field} must be an object")
    return parsed


def _public_evidence_state(state: str) -> str:
    """Translate retained resolution terminology only at the public query boundary."""
    if state == "hard_proven":
        return "run-verified"
    if state in _KNOWN_EVIDENCE_STATES:
        return state
    return "unknown"


def _current_route_evidence(connection: sqlite3.Connection, manifest_id: str) -> Tuple[Mapping[str, Any], ...]:
    manifest = connection.execute(
        "SELECT present, current_sha256 FROM manifest_current WHERE manifest_id = ?",
        (manifest_id,),
    ).fetchone()
    if manifest is None:
        raise ScenarioRegistryStoreError("Route evidence references a missing manifest")
    current_manifest_sha256 = manifest["current_sha256"]
    rows = connection.execute(
        "SELECT capability_evidence_id, evidence_state, details_json, value_json FROM capability_evidence_history "
        "WHERE manifest_id = ? AND capability_key = '_registry.proof_route' "
        "AND evidence_kind = 'route_resolution' ORDER BY capability_evidence_id",
        (manifest_id,),
    ).fetchall()
    latest: Dict[str, Mapping[str, Any]] = {}
    for row in rows:
        details = _json_object(str(row["details_json"]), "route evidence details")
        value = _json_object(str(row["value_json"]), "route evidence value")
        route_key = str(details.get("route_key") or value.get("route_key") or "")
        if not route_key:
            raise ScenarioRegistryStoreError("Registry route evidence is missing route_key")
        bindings = []
        resolutions = connection.execute(
            "SELECT verification.verification_id, resolution.resolution_kind, resolution.binding_fingerprint, "
            "resolution.details_json FROM verification_history AS verification "
            "JOIN verification_resolution_history AS resolution "
            "ON resolution.verification_id = verification.verification_id "
            "WHERE verification.manifest_id = ? AND verification.route_key = ? "
            "AND resolution.resolution_event_id = ( SELECT MAX( latest.resolution_event_id ) "
            "FROM verification_resolution_history AS latest "
            "WHERE latest.verification_id = verification.verification_id ) "
            "ORDER BY verification.verification_id",
            (manifest_id, route_key),
        ).fetchall()
        for resolution in resolutions:
            manifest_binding_current = False
            for binding_row in connection.execute(
                "SELECT payload_json FROM binding_history WHERE manifest_id = ? AND binding_kind = 'manifest' "
                "ORDER BY binding_event_id DESC",
                (manifest_id,),
            ):
                payload = _json_object(str(binding_row["payload_json"]), "manifest binding payload")
                if payload.get("verification_id") != str(resolution["verification_id"]):
                    continue
                expected = payload.get("expected")
                manifest_binding_current = (
                    bool(manifest["present"])
                    and isinstance(expected, Mapping)
                    and expected.get("source_sha256") == current_manifest_sha256
                )
                break
            resolution_state = str(resolution["resolution_kind"])
            if resolution_state == "compatible" and not manifest_binding_current:
                resolution_state = "stale"
            freshness = dict(_json_object(str(resolution["details_json"]), "binding resolution details"))
            freshness["manifest_current"] = "compatible" if manifest_binding_current else "stale"
            bindings.append({
                "verification_id": str(resolution["verification_id"]),
                "resolution": resolution_state,
                "binding_fingerprint": str(resolution["binding_fingerprint"]),
                "freshness": freshness,
            })
        evidence_state = _public_evidence_state(str(row["evidence_state"]))
        if evidence_state == "run-verified" and not any(
            binding["resolution"] == "compatible" for binding in bindings
        ):
            evidence_state = "stale"
        latest[route_key] = {
            "route_key": route_key,
            "evidence_state": evidence_state,
            "internal_resolution_state": str(row["evidence_state"]),
            "details": dict(details),
            "bindings": tuple(bindings),
        }
    return tuple(latest[key] for key in sorted(latest))


def _current_lifecycle_state(
    connection: sqlite3.Connection,
    *,
    manifest_id: str,
    present: bool,
    route_evidence: Sequence[Mapping[str, Any]],
) -> Tuple[str, str]:
    if connection.execute(
        "SELECT retirement_event_id FROM retirement_history WHERE manifest_id = ? "
        "ORDER BY retirement_event_id DESC LIMIT 1",
        (manifest_id,),
    ).fetchone() is not None:
        return "retired", "retirement_history"
    if not present:
        return "retired", "source_absent"
    states = {str(item["evidence_state"]) for item in route_evidence}
    if "contradicted" in states:
        return "quarantined", "route_contradicted"
    if "stale" in states:
        return "quarantined", "route_stale"
    latest_quarantine: Dict[str, str] = {}
    for row in connection.execute(
        "SELECT route_key, quarantine_kind FROM quarantine_history WHERE manifest_id = ? "
        "ORDER BY quarantine_event_id",
        (manifest_id,),
    ):
        latest_quarantine[str(row["route_key"])] = str(row["quarantine_kind"])
    if any(not kind.startswith("released_") for kind in latest_quarantine.values()):
        return "quarantined", "quarantine_history"
    return "active", "current_manifest"


def _fact_state_from_current_authority(
    connection: sqlite3.Connection,
    *,
    manifest_id: str,
    capability_key: str,
    declared_state: str,
    route_evidence: Sequence[Mapping[str, Any]],
) -> str:
    route_states = {str(item["evidence_state"]) for item in route_evidence}
    if "contradicted" in route_states:
        return "contradicted"
    if "stale" in route_states:
        return "stale"
    rows = connection.execute(
        "SELECT evidence_state FROM capability_evidence_history WHERE manifest_id = ? "
        "AND capability_key = ? AND evidence_kind != 'declaration' ORDER BY capability_evidence_id",
        (manifest_id, capability_key),
    ).fetchall()
    states = {_public_evidence_state(str(row["evidence_state"])) for row in rows}
    if "contradicted" in states:
        return "contradicted"
    if "stale" in states:
        return "stale"
    if "run-verified" in states:
        return "run-verified"
    if "inspected" in states:
        return "inspected"
    return _public_evidence_state(declared_state)


def build_registry_query_candidate_snapshot(
    connection: sqlite3.Connection,
    *,
    include_lifecycle_states: Sequence[str] = (),
) -> Tuple[RegistryQueryCandidateSnapshot, ...]:
    """Read current projection/history into fixed, explained, non-executing candidates."""
    requested_states = set(include_lifecycle_states)
    if not requested_states <= {"quarantined", "retired"}:
        raise ScenarioRegistryQueryError("include_lifecycle_states may contain only quarantined or retired")
    allowed_states = {"active"} | requested_states
    snapshots: List[RegistryQueryCandidateSnapshot] = []
    manifests = connection.execute(
        "SELECT manifest_id, source_path, present, revision, current_sha256, validation_json, declaration_json "
        "FROM manifest_current ORDER BY manifest_id"
    ).fetchall()
    for manifest in manifests:
        manifest_id = str(manifest["manifest_id"])
        declaration = _json_object(str(manifest["declaration_json"]), "manifest declaration")
        validation = _json_object(str(manifest["validation_json"]), "manifest validation")
        route_evidence = _current_route_evidence(connection, manifest_id)
        known_footing: Dict[str, Any] = {}
        for field in ("fixture", "fixture_profile", "profile", "profile_snapshot", "profile_snapshot_profile", "world", "required_helpers"):
            value = declaration.get(field)
            if value not in (None, "", [], {}):
                known_footing[field] = value
        runtime_contract = declaration.get("runtime_contract")
        if isinstance(runtime_contract, Mapping):
            for field in ("helpers", "profile", "fixture"):
                value = runtime_contract.get(field)
                if value not in (None, "", [], {}):
                    known_footing.setdefault(field if field != "helpers" else "required_helpers", value)
        lifecycle_state, lifecycle_reason = _current_lifecycle_state(
            connection,
            manifest_id=manifest_id,
            present=bool(manifest["present"]),
            route_evidence=route_evidence,
        )
        if lifecycle_state not in allowed_states:
            continue
        facts: Dict[str, Mapping[str, Any]] = {}
        fact_explanations: Dict[str, Mapping[str, Any]] = {}
        capabilities = connection.execute(
            "SELECT capability_key, value_json, declared_state, review_required "
            "FROM manifest_capability_current WHERE manifest_id = ? ORDER BY capability_key",
            (manifest_id,),
        ).fetchall()
        for capability in capabilities:
            key = str(capability["capability_key"])
            value = json.loads(str(capability["value_json"]))
            evidence_state = _fact_state_from_current_authority(
                connection,
                manifest_id=manifest_id,
                capability_key=key,
                declared_state=str(capability["declared_state"]),
                route_evidence=route_evidence,
            )
            facts[key] = {
                "present": True,
                "value": value,
                "evidence_state": evidence_state,
                "proof_depth": None,
            }
            fact_explanations[key] = {
                "declared_state": str(capability["declared_state"]),
                "review_required": bool(capability["review_required"]),
                "evidence_state": evidence_state,
                "source": "manifest_capability_current",
            }
        snapshots.append(RegistryQueryCandidateSnapshot(
            scenario_id=manifest_id,
            facts=facts,
            lifecycle_state=lifecycle_state,
            token_eligible=lifecycle_state == "active",
            explanation={
                "manifest": {
                    "manifest_id": manifest_id,
                    "name": declaration.get("name"),
                    "source_path": str(manifest["source_path"]),
                    "present": bool(manifest["present"]),
                    "revision": int(manifest["revision"]),
                    "sha256": manifest["current_sha256"],
                    "validation": dict(validation),
                    "known_footing": known_footing,
                },
                "lifecycle": {"state": lifecycle_state, "reason": lifecycle_reason},
                "route_evidence": route_evidence,
                "facts": fact_explanations,
            },
        ))
    return tuple(snapshots)


def evaluate_registry_query_from_store(
    connection: sqlite3.Connection,
    request: RegistryQueryRequest,
    *,
    include_lifecycle_states: Sequence[str] = (),
) -> RegistryStoredQueryEvaluation:
    """Evaluate fixed current-authority snapshots; this owner never launches or mints tokens."""
    candidates = build_registry_query_candidate_snapshot(
        connection,
        include_lifecycle_states=include_lifecycle_states,
    )
    evaluation = evaluate_registry_query(
        request,
        tuple({"scenario_id": candidate.scenario_id, "facts": candidate.facts} for candidate in candidates),
    )
    return RegistryStoredQueryEvaluation(candidates=candidates, evaluation=evaluation)


def _query_predicate_json(predicate: RegistryQueryPredicate) -> Dict[str, Any]:
    result: Dict[str, Any] = {
        "key": predicate.key,
        "op": predicate.op,
        "minimum_evidence": predicate.minimum_evidence,
    }
    if predicate.op in {"eq", "contains"}:
        result["value"] = predicate.value
    elif predicate.op == "range":
        if predicate.minimum is not None:
            result["minimum"] = predicate.minimum
        if predicate.maximum is not None:
            result["maximum"] = predicate.maximum
    return result


def _query_request_json(request: RegistryQueryRequest) -> str:
    return _json_text({
        "requirements": [_query_predicate_json(predicate) for predicate in request.requirements],
        "preferences": [_query_predicate_json(predicate) for predicate in request.preferences],
    })


def _current_verified_route(snapshot: RegistryQueryCandidateSnapshot) -> Optional[Mapping[str, Any]]:
    routes = snapshot.explanation.get("route_evidence", ())
    if not isinstance(routes, Sequence):
        return None
    for route in routes:
        if not isinstance(route, Mapping) or route.get("evidence_state") != "run-verified":
            continue
        bindings = route.get("bindings", ())
        if isinstance(bindings, Sequence) and bindings and all(
            isinstance(binding, Mapping) and binding.get("resolution") == "compatible"
            for binding in bindings
        ):
            return route
    return None


def _query_unmet_capabilities(evaluation: RegistryStoredQueryEvaluation) -> List[Dict[str, Any]]:
    explanations = {candidate.scenario_id: candidate.explanation for candidate in evaluation.candidates}
    unmet: List[Dict[str, Any]] = []
    for candidate in evaluation.evaluation.candidates:
        failed = [
            {
                "key": result.key,
                "expected": result.expected,
                "observed": result.observed,
                "evidence_state": result.evidence_state,
                "reason": result.reason,
            }
            for result in candidate.hard_results
            if not result.passed
        ]
        if failed:
            unmet.append({
                "scenario_id": candidate.scenario_id,
                "manifest": explanations[candidate.scenario_id]["manifest"],
                "unmet": failed,
            })
    return unmet


def _known_draft_footing(candidates: Sequence[RegistryQueryCandidateSnapshot]) -> Dict[str, Any]:
    known: Dict[str, Any] = {}
    for candidate in candidates:
        manifest = candidate.explanation.get("manifest", {})
        footing = manifest.get("known_footing") if isinstance(manifest, Mapping) else None
        if not isinstance(footing, Mapping):
            continue
        for field, value in footing.items():
            if value not in (None, "", [], {}):
                known[field] = value
    return known


def _write_inert_draft(
    *,
    request_json: str,
    query_sha256: str,
    evaluation: RegistryStoredQueryEvaluation,
    drafts_root: Path,
) -> str:
    drafts_root.mkdir(parents=True, exist_ok=True)
    path = drafts_root / f"{query_sha256}.json"
    payload = {
        "review_status": "pending",
        "executable": False,
        "query": json.loads(request_json),
        "unmet_capabilities": _query_unmet_capabilities(evaluation),
        "candidate_manifest": _known_draft_footing(evaluation.candidates),
    }
    path.write_text(json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return str(path)


def _append_query_audit(
    connection: sqlite3.Connection,
    *,
    query_id: str,
    request_json: str,
    result: Mapping[str, Any],
) -> None:
    connection.execute(
        "INSERT INTO query_history( query_id, query_kind, request_json, result_sha256 ) VALUES( ?, ?, ?, ? )",
        (query_id, "registry_query", request_json, hashlib.sha256(_json_text(result).encode("utf-8")).hexdigest()),
    )


def execute_registry_query(
    connection: sqlite3.Connection,
    request: RegistryQueryRequest,
    *,
    include_lifecycle_states: Sequence[str] = (),
    drafts_root: Optional[Path] = None,
) -> RegistryQueryExecution:
    """Audit a fixed query and issue one bound token or a deterministic inert draft."""
    request_json = _query_request_json(request)
    query_sha256 = hashlib.sha256(request_json.encode("utf-8")).hexdigest()
    query_id = _identity("caol-scenario-query-v1", query_sha256)
    evaluation = evaluate_registry_query_from_store(
        connection,
        request,
        include_lifecycle_states=include_lifecycle_states,
    )
    selected_id = evaluation.evaluation.ranked_scenario_ids[0] if evaluation.evaluation.ranked_scenario_ids else None
    selected = next((candidate for candidate in evaluation.candidates if candidate.scenario_id == selected_id), None)
    route = _current_verified_route(selected) if selected is not None and selected.token_eligible else None
    if selected is None or route is None:
        root = drafts_root or repository_root() / ".userdata" / "openclaw_harness" / "drafts"
        draft_path = _write_inert_draft(
            request_json=request_json,
            query_sha256=query_sha256,
            evaluation=evaluation,
            drafts_root=root,
        )
        with immediate_transaction(connection):
            _append_query_audit(
                connection,
                query_id=query_id,
                request_json=request_json,
                result={"kind": "draft", "draft_path": draft_path, "selected_scenario_id": selected_id},
            )
        return RegistryQueryExecution(query_id, query_sha256, evaluation, None, draft_path)

    manifest = selected.explanation["manifest"]
    route_key = str(route["route_key"])
    bindings = route["bindings"]
    verification_ids = tuple(str(binding["verification_id"]) for binding in bindings)
    token_details = {
        "query_id": query_id,
        "query_sha256": query_sha256,
        "manifest_id": manifest["manifest_id"],
        "manifest_revision": manifest["revision"],
        "manifest_sha256": manifest["sha256"],
        "lifecycle": selected.explanation["lifecycle"],
        "selected_values": selected.facts,
        "route_evidence": route,
    }
    token_id = _identity(
        "caol-scenario-selection-token-v1",
        query_sha256,
        str(manifest["manifest_id"]),
        str(manifest["revision"]),
        str(manifest["sha256"]),
        route_key,
        _json_text(token_details),
    )
    with immediate_transaction(connection):
        _append_query_audit(
            connection,
            query_id=query_id,
            request_json=request_json,
            result={"kind": "selection", "token_id": token_id, "selected_scenario_id": selected.scenario_id},
        )
        connection.execute(
            "INSERT OR IGNORE INTO token_history( token_id, manifest_id, verification_id, route_key, event_kind, reason, details_json ) "
            "VALUES( ?, ?, ?, ?, 'issued', 'query_selection', ? )",
            (
                token_id,
                str(manifest["manifest_id"]),
                verification_ids[0],
                route_key,
                _json_text(token_details),
            ),
        )
    return RegistryQueryExecution(query_id, query_sha256, evaluation, token_id, None)


def _invalidate_manifest_tokens(
    connection: sqlite3.Connection,
    *,
    manifest_id: str,
    reason: str,
    details: Mapping[str, Any],
) -> int:
    routes = connection.execute(
        "SELECT DISTINCT route_key FROM token_history AS issued WHERE manifest_id = ? "
        "AND event_kind = 'issued' AND NOT EXISTS( SELECT 1 FROM token_history AS invalidated "
        "WHERE invalidated.token_id = issued.token_id AND invalidated.event_kind = 'invalidated' )",
        (manifest_id,),
    ).fetchall()
    return sum(
        _invalidate_outstanding_tokens(
            connection,
            manifest_id=manifest_id,
            route_key=str(row["route_key"]),
            reason=reason,
            details=details,
        )
        for row in routes
    )


@dataclass(frozen=True)
class StagedManifest:
    manifest_id: str
    source_path: str
    source_sha256: str
    declaration_json: str
    normalized_json: str
    validation_json: str
    capabilities: Tuple[Tuple[str, str, str, int], ...]
    proof_routes: Tuple[Tuple[str, str], ...]
    relations: Tuple[Tuple[str, str, str, str], ...]


@dataclass(frozen=True)
class BindingAdapters:
    """Current-owner callbacks; fixture/profile facts must be hash-backed."""

    runtime: Callable[[Mapping[str, Any]], Mapping[str, Any]]
    fixture: Callable[[Mapping[str, Any]], Mapping[str, Any]]
    profile: Callable[[Mapping[str, Any]], Mapping[str, Any]]


def repository_root() -> Path:
    return Path(__file__).resolve().parents[2]


def resolve_registry_path(override: Optional[str] = None) -> Path:
    """Return the default shared harness DB path or an explicit override."""
    if override:
        return Path(override).expanduser().resolve()
    return repository_root() / ".userdata" / "openclaw_harness" / "scenario_registry.sqlite3"


@contextmanager
def immediate_transaction(connection: sqlite3.Connection) -> Iterator[sqlite3.Connection]:
    """Run one all-or-nothing registry mutation transaction."""
    if connection.in_transaction:
        raise ScenarioRegistryStoreError("registry transaction cannot be nested")
    connection.execute("BEGIN IMMEDIATE")
    try:
        yield connection
    except BaseException:
        connection.rollback()
        raise
    else:
        connection.commit()


def _create_migration_history(connection: sqlite3.Connection) -> None:
    connection.executescript(
        """
        CREATE TABLE IF NOT EXISTS schema_migration_history (
            version INTEGER PRIMARY KEY,
            migration_name TEXT NOT NULL UNIQUE,
            applied_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        );

        CREATE TRIGGER IF NOT EXISTS schema_migration_history_no_update
        BEFORE UPDATE ON schema_migration_history
        BEGIN
            SELECT RAISE(ABORT, 'schema migration history is append-only');
        END;

        CREATE TRIGGER IF NOT EXISTS schema_migration_history_no_delete
        BEFORE DELETE ON schema_migration_history
        BEGIN
            SELECT RAISE(ABORT, 'schema migration history is append-only');
        END;
        """
    )


def _create_history_append_only_triggers(connection: sqlite3.Connection, table: str) -> None:
    connection.executescript(
        f"""
        CREATE TRIGGER IF NOT EXISTS {table}_no_update
        BEFORE UPDATE ON {table}
        BEGIN
            SELECT RAISE(ABORT, '{table} is append-only');
        END;

        CREATE TRIGGER IF NOT EXISTS {table}_no_delete
        BEFORE DELETE ON {table}
        BEGIN
            SELECT RAISE(ABORT, '{table} is append-only');
        END;
        """
    )


def _migration_001_initial(connection: sqlite3.Connection) -> None:
    connection.executescript(
        """
        CREATE TABLE manifest_current (
            manifest_id TEXT PRIMARY KEY,
            source_path TEXT NOT NULL UNIQUE,
            present INTEGER NOT NULL CHECK ( present IN ( 0, 1 ) ),
            revision INTEGER NOT NULL CHECK ( revision >= 0 ),
            current_sha256 TEXT,
            last_content_sha256 TEXT,
            declaration_json TEXT NOT NULL,
            normalized_json TEXT NOT NULL,
            validation_json TEXT NOT NULL,
            last_seen_at TEXT,
            absent_at TEXT,
            CHECK ( present = 1 OR absent_at IS NOT NULL )
        );

        CREATE TABLE manifest_capability_current (
            manifest_id TEXT NOT NULL REFERENCES manifest_current( manifest_id ) ON DELETE RESTRICT,
            capability_key TEXT NOT NULL,
            value_json TEXT,
            declared_state TEXT NOT NULL,
            review_required INTEGER NOT NULL CHECK ( review_required IN ( 0, 1 ) ),
            PRIMARY KEY ( manifest_id, capability_key )
        );

        CREATE TABLE manifest_proof_route_current (
            manifest_id TEXT NOT NULL REFERENCES manifest_current( manifest_id ) ON DELETE RESTRICT,
            route_role TEXT NOT NULL,
            step_label TEXT NOT NULL,
            PRIMARY KEY ( manifest_id, route_role, step_label )
        );

        CREATE TABLE manifest_relation_current (
            manifest_id TEXT NOT NULL REFERENCES manifest_current( manifest_id ) ON DELETE RESTRICT,
            relation_kind TEXT NOT NULL,
            target_kind TEXT NOT NULL,
            target_key TEXT NOT NULL,
            route_role TEXT NOT NULL DEFAULT '',
            PRIMARY KEY ( manifest_id, relation_kind, target_kind, target_key, route_role )
        );

        CREATE TABLE lifecycle_history (
            lifecycle_event_id INTEGER PRIMARY KEY,
            manifest_id TEXT NOT NULL REFERENCES manifest_current( manifest_id ) ON DELETE RESTRICT,
            event_kind TEXT NOT NULL,
            revision INTEGER NOT NULL,
            cause_sha256 TEXT,
            details_json TEXT NOT NULL,
            recorded_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        );

        CREATE TABLE manifest_relation_history (
            relation_event_id INTEGER PRIMARY KEY,
            manifest_id TEXT NOT NULL REFERENCES manifest_current( manifest_id ) ON DELETE RESTRICT,
            relation_kind TEXT NOT NULL,
            target_kind TEXT NOT NULL,
            target_key TEXT NOT NULL,
            route_role TEXT NOT NULL DEFAULT '',
            event_kind TEXT NOT NULL,
            revision INTEGER NOT NULL,
            details_json TEXT NOT NULL,
            recorded_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            UNIQUE ( manifest_id, relation_kind, target_kind, target_key, route_role, event_kind, revision )
        );

        CREATE TABLE binding_history (
            binding_event_id INTEGER PRIMARY KEY,
            manifest_id TEXT NOT NULL REFERENCES manifest_current( manifest_id ) ON DELETE RESTRICT,
            binding_kind TEXT NOT NULL,
            binding_fingerprint TEXT NOT NULL,
            binding_status TEXT NOT NULL,
            payload_json TEXT NOT NULL,
            recorded_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            UNIQUE ( manifest_id, binding_kind, binding_fingerprint, binding_status )
        );

        CREATE TABLE report_ingestion_history (
            report_id TEXT PRIMARY KEY,
            manifest_id TEXT REFERENCES manifest_current( manifest_id ) ON DELETE RESTRICT,
            report_path TEXT NOT NULL,
            report_sha256 TEXT NOT NULL,
            report_kind TEXT NOT NULL,
            ingestion_status TEXT NOT NULL,
            error_text TEXT NOT NULL DEFAULT '',
            recorded_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            UNIQUE ( report_path, report_sha256 )
        );

        CREATE TABLE verification_history (
            verification_id TEXT PRIMARY KEY,
            manifest_id TEXT NOT NULL REFERENCES manifest_current( manifest_id ) ON DELETE RESTRICT,
            report_id TEXT NOT NULL REFERENCES report_ingestion_history( report_id ) ON DELETE RESTRICT,
            route_key TEXT NOT NULL,
            binding_fingerprint TEXT NOT NULL,
            outcome_kind TEXT NOT NULL,
            proof_status TEXT NOT NULL,
            report_timestamp TEXT NOT NULL DEFAULT '',
            supersedes_verification_id TEXT REFERENCES verification_history( verification_id ) ON DELETE RESTRICT,
            details_json TEXT NOT NULL,
            recorded_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            UNIQUE ( report_id, route_key, binding_fingerprint, outcome_kind )
        );

        CREATE TABLE capability_evidence_history (
            capability_evidence_id INTEGER PRIMARY KEY,
            manifest_id TEXT NOT NULL REFERENCES manifest_current( manifest_id ) ON DELETE RESTRICT,
            verification_id TEXT REFERENCES verification_history( verification_id ) ON DELETE RESTRICT,
            capability_key TEXT NOT NULL,
            evidence_kind TEXT NOT NULL,
            evidence_state TEXT NOT NULL,
            value_json TEXT,
            value_sha256 TEXT NOT NULL,
            details_json TEXT NOT NULL,
            recorded_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            UNIQUE ( manifest_id, verification_id, capability_key, evidence_kind, value_sha256 )
        );

        CREATE TABLE verification_resolution_history (
            resolution_event_id INTEGER PRIMARY KEY,
            verification_id TEXT NOT NULL REFERENCES verification_history( verification_id ) ON DELETE RESTRICT,
            manifest_id TEXT NOT NULL REFERENCES manifest_current( manifest_id ) ON DELETE RESTRICT,
            route_key TEXT NOT NULL,
            resolution_kind TEXT NOT NULL,
            binding_fingerprint TEXT NOT NULL,
            details_json TEXT NOT NULL,
            recorded_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        );

        CREATE TABLE quarantine_history (
            quarantine_event_id INTEGER PRIMARY KEY,
            manifest_id TEXT NOT NULL REFERENCES manifest_current( manifest_id ) ON DELETE RESTRICT,
            route_key TEXT NOT NULL,
            verification_id TEXT REFERENCES verification_history( verification_id ) ON DELETE RESTRICT,
            quarantine_kind TEXT NOT NULL,
            details_json TEXT NOT NULL,
            recorded_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        );

        CREATE TABLE token_history (
            token_event_id INTEGER PRIMARY KEY,
            token_id TEXT NOT NULL,
            manifest_id TEXT NOT NULL REFERENCES manifest_current( manifest_id ) ON DELETE RESTRICT,
            verification_id TEXT REFERENCES verification_history( verification_id ) ON DELETE RESTRICT,
            route_key TEXT NOT NULL,
            event_kind TEXT NOT NULL,
            reason TEXT NOT NULL,
            details_json TEXT NOT NULL,
            recorded_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            UNIQUE ( token_id, event_kind, reason )
        );

        CREATE TABLE query_history (
            query_event_id INTEGER PRIMARY KEY,
            query_id TEXT NOT NULL,
            query_kind TEXT NOT NULL,
            request_json TEXT NOT NULL,
            result_sha256 TEXT NOT NULL DEFAULT '',
            recorded_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        );

        CREATE TABLE retirement_history (
            retirement_event_id INTEGER PRIMARY KEY,
            manifest_id TEXT NOT NULL REFERENCES manifest_current( manifest_id ) ON DELETE RESTRICT,
            retirement_kind TEXT NOT NULL,
            authority TEXT NOT NULL,
            reason TEXT NOT NULL,
            details_json TEXT NOT NULL,
            recorded_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        );

        CREATE INDEX idx_manifest_current_present ON manifest_current( present, source_path );
        CREATE INDEX idx_lifecycle_manifest ON lifecycle_history( manifest_id, lifecycle_event_id );
        CREATE INDEX idx_relation_history_manifest ON manifest_relation_history( manifest_id, relation_event_id );
        CREATE INDEX idx_binding_manifest ON binding_history( manifest_id, binding_kind, binding_event_id );
        CREATE INDEX idx_report_manifest ON report_ingestion_history( manifest_id, recorded_at );
        CREATE INDEX idx_verification_route ON verification_history( manifest_id, route_key, recorded_at );
        CREATE INDEX idx_capability_evidence ON capability_evidence_history( manifest_id, capability_key, capability_evidence_id );
        CREATE INDEX idx_resolution_route ON verification_resolution_history( manifest_id, route_key, resolution_event_id );
        CREATE INDEX idx_quarantine_route ON quarantine_history( manifest_id, route_key, quarantine_event_id );
        CREATE INDEX idx_token_route ON token_history( manifest_id, route_key, token_event_id );
        CREATE INDEX idx_query_id ON query_history( query_id, query_event_id );
        CREATE INDEX idx_retirement_manifest ON retirement_history( manifest_id, retirement_event_id );
        """
    )
    for table in (
        "lifecycle_history",
        "manifest_relation_history",
        "binding_history",
        "report_ingestion_history",
        "verification_history",
        "capability_evidence_history",
        "verification_resolution_history",
        "quarantine_history",
        "token_history",
        "query_history",
        "retirement_history",
    ):
        _create_history_append_only_triggers(connection, table)


def _migration_002_inventory_migration_history(connection: sqlite3.Connection) -> None:
    """Add immutable ownership records for resumable scenario inventory work."""
    connection.executescript(
        """
        CREATE TABLE migration_run (
            migration_run_id TEXT PRIMARY KEY,
            run_identity TEXT NOT NULL UNIQUE,
            launch_status TEXT NOT NULL,
            launch_reason TEXT NOT NULL,
            launcher_identity TEXT NOT NULL,
            details_json TEXT NOT NULL,
            launched_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        );

        CREATE TABLE migration_item (
            migration_item_event_id INTEGER PRIMARY KEY,
            migration_run_id TEXT NOT NULL REFERENCES migration_run( migration_run_id ) ON DELETE RESTRICT,
            manifest_id TEXT REFERENCES manifest_current( manifest_id ) ON DELETE RESTRICT,
            attempt_identity TEXT NOT NULL,
            source_path TEXT NOT NULL,
            source_sha256 TEXT NOT NULL,
            event_kind TEXT NOT NULL,
            completion_status TEXT NOT NULL,
            disposition TEXT NOT NULL,
            reason TEXT NOT NULL,
            details_json TEXT NOT NULL,
            recorded_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            UNIQUE (
                migration_run_id, attempt_identity, source_path, source_sha256,
                event_kind, completion_status, disposition, reason
            )
        );

        CREATE INDEX idx_migration_item_attempt
        ON migration_item( migration_run_id, attempt_identity, migration_item_event_id );

        CREATE INDEX idx_migration_item_source
        ON migration_item( source_path, source_sha256, migration_item_event_id );
        """
    )
    _create_history_append_only_triggers(connection, "migration_run")
    _create_history_append_only_triggers(connection, "migration_item")


SCHEMA_MIGRATIONS: Sequence[Migration] = (
    (1, "initial_registry_surface", _migration_001_initial),
    (2, "inventory_migration_history", _migration_002_inventory_migration_history),
)


def apply_migrations(
    connection: sqlite3.Connection,
    migrations: Sequence[Migration] = SCHEMA_MIGRATIONS,
) -> None:
    """Apply contiguous migrations atomically, recording each exactly once."""
    with immediate_transaction(connection):
        _create_migration_history(connection)

    current_version = int(connection.execute("PRAGMA user_version").fetchone()[0])
    recorded = {
        int(row[0]): str(row[1])
        for row in connection.execute("SELECT version, migration_name FROM schema_migration_history")
    }
    if any(version > current_version for version in recorded):
        raise ScenarioRegistryStoreError("migration history is ahead of PRAGMA user_version")

    for version, name, migration in sorted(migrations, key=lambda item: item[0]):
        if version <= current_version:
            if recorded.get(version) != name:
                raise ScenarioRegistryStoreError(f"migration history mismatch at version {version}")
            continue
        if version != current_version + 1:
            raise ScenarioRegistryStoreError(
                f"migration {version} is not contiguous after schema version {current_version}"
            )
        with immediate_transaction(connection):
            migration(connection)
            connection.execute(
                "INSERT INTO schema_migration_history( version, migration_name ) VALUES( ?, ? )",
                (version, name),
            )
            connection.execute(f"PRAGMA user_version = {version}")
        current_version = version
        recorded[version] = name


def open_registry(
    override: Optional[str] = None,
    *,
    writable: bool = True,
) -> sqlite3.Connection:
    """Open the registry, creating parent directories only for writable use."""
    path = resolve_registry_path(override)
    if writable:
        path.parent.mkdir(parents=True, exist_ok=True)
        connection = sqlite3.connect(path, isolation_level=None)
        connection.row_factory = sqlite3.Row
        connection.execute("PRAGMA foreign_keys = ON")
        try:
            apply_migrations(connection)
        except BaseException:
            connection.close()
            raise
        return connection

    connection = sqlite3.connect(f"file:{path}?mode=ro", uri=True, isolation_level=None)
    connection.row_factory = sqlite3.Row
    connection.execute("PRAGMA foreign_keys = ON")
    return connection


def manifest_identity(source_path: Path) -> str:
    canonical_path = str(source_path.resolve())
    source = b"caol-scenario-manifest-path-v1\0" + canonical_path.encode("utf-8")
    return hashlib.sha256(source).hexdigest()


def _json_text(value: Any) -> str:
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":"))


def _stage_manifest(path: Path) -> StagedManifest:
    try:
        source_bytes = path.read_bytes()
        source_text = source_bytes.decode("utf-8")
        declaration = json.loads(source_text)
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ScenarioRegistryStoreError(f"Could not stage scenario source {path}: {exc}") from exc
    if not isinstance(declaration, dict):
        raise ScenarioRegistryStoreError(f"Could not stage scenario source {path}: top level must be an object")
    try:
        validated = validate_manifest(declaration, path=path)
    except ManifestValidationError as exc:
        raise ScenarioRegistryStoreError(str(exc)) from exc
    try:
        final_bytes = path.read_bytes()
    except OSError as exc:
        raise ScenarioRegistryStoreError(f"Scenario source changed while staging {path}: {exc}") from exc
    if final_bytes != source_bytes:
        raise ScenarioRegistryStoreError(f"Scenario source changed while staging {path}")

    source = validated["source"]
    source_path = str(path.resolve())
    source_sha256 = hashlib.sha256(source_bytes).hexdigest()
    if source.get("path") != source_path or source.get("sha256") != source_sha256:
        raise ScenarioRegistryStoreError(f"Scenario validator did not bind staged source {path}")

    normalized = validated["normalized"]
    capabilities_field = normalized.get("capabilities", {})
    capability_values = capabilities_field.get("value") if isinstance(capabilities_field, dict) else None
    capability_state = str(capabilities_field.get("state", "unknown")) if isinstance(capabilities_field, dict) else "unknown"
    capability_review = int(bool(capabilities_field.get("review_required", True))) if isinstance(capabilities_field, dict) else 1
    capabilities: List[Tuple[str, str, str, int]] = []
    if isinstance(capability_values, dict):
        for key, value in sorted(capability_values.items()):
            capabilities.append((str(key), _json_text(value), capability_state, capability_review))

    proof_routes_field = normalized.get("proof_route", {})
    proof_route_values = proof_routes_field.get("value") if isinstance(proof_routes_field, dict) else None
    proof_routes: List[Tuple[str, str]] = []
    if isinstance(proof_route_values, dict):
        for role, labels in sorted(proof_route_values.items()):
            if not isinstance(labels, list):
                continue
            for label in sorted(str(item) for item in labels):
                proof_routes.append((str(role), label))

    relations = tuple(
        ("proof_route_step", "step", label, role)
        for role, label in proof_routes
    )
    return StagedManifest(
        manifest_id=manifest_identity(path),
        source_path=source_path,
        source_sha256=source_sha256,
        declaration_json=source_text,
        normalized_json=_json_text(normalized),
        validation_json=_json_text(validated["validation"]),
        capabilities=tuple(capabilities),
        proof_routes=tuple(proof_routes),
        relations=relations,
    )


def stage_manifest_projection(scenarios_root: Path) -> Tuple[StagedManifest, ...]:
    """Validate every source before any registry transaction begins."""
    root = scenarios_root.resolve()
    if not root.is_dir():
        raise ScenarioRegistryStoreError(f"Scenario root does not exist: {root}")
    staged = [_stage_manifest(path) for path in sorted(root.glob("*.json"), key=lambda item: item.name.lower())]
    paths = [entry.source_path for entry in staged]
    if len(set(paths)) != len(paths):
        raise ScenarioRegistryStoreError("Scenario root contains duplicate canonical source paths")
    return tuple(staged)


def _replace_current_materializations(connection: sqlite3.Connection, staged: StagedManifest) -> None:
    connection.execute("DELETE FROM manifest_capability_current WHERE manifest_id = ?", (staged.manifest_id,))
    connection.execute("DELETE FROM manifest_proof_route_current WHERE manifest_id = ?", (staged.manifest_id,))
    connection.execute("DELETE FROM manifest_relation_current WHERE manifest_id = ?", (staged.manifest_id,))
    connection.executemany(
        "INSERT INTO manifest_capability_current( manifest_id, capability_key, value_json, declared_state, review_required ) "
        "VALUES( ?, ?, ?, ?, ? )",
        [(staged.manifest_id, key, value, state, review) for key, value, state, review in staged.capabilities],
    )
    connection.executemany(
        "INSERT INTO manifest_proof_route_current( manifest_id, route_role, step_label ) VALUES( ?, ?, ? )",
        [(staged.manifest_id, role, label) for role, label in staged.proof_routes],
    )
    connection.executemany(
        "INSERT INTO manifest_relation_current( manifest_id, relation_kind, target_kind, target_key, route_role ) "
        "VALUES( ?, ?, ?, ?, ? )",
        [(staged.manifest_id, kind, target_kind, target_key, route_role)
         for kind, target_kind, target_key, route_role in staged.relations],
    )
    for capability_key, value_json, declared_state, _review_required in staged.capabilities:
        value_sha256 = _identity(
            "caol-scenario-declaration-evidence-v1",
            staged.source_sha256,
            capability_key,
            declared_state,
            value_json,
        )
        existing = connection.execute(
            "SELECT capability_evidence_id FROM capability_evidence_history "
            "WHERE manifest_id = ? AND verification_id IS NULL AND capability_key = ? "
            "AND evidence_kind = 'declaration' AND value_sha256 = ?",
            (staged.manifest_id, capability_key, value_sha256),
        ).fetchone()
        if existing is None:
            connection.execute(
                "INSERT INTO capability_evidence_history( "
                "manifest_id, capability_key, evidence_kind, evidence_state, value_json, value_sha256, details_json "
                ") VALUES( ?, ?, 'declaration', ?, ?, ?, ? )",
                (
                    staged.manifest_id,
                    capability_key,
                    declared_state,
                    value_json,
                    value_sha256,
                    _json_text({"source_sha256": staged.source_sha256}),
                ),
            )


def _append_relation_events(
    connection: sqlite3.Connection,
    staged: StagedManifest,
    *,
    event_kind: str,
    revision: int,
) -> None:
    if event_kind not in {"discovery", "change"}:
        return
    connection.executemany(
        "INSERT OR IGNORE INTO manifest_relation_history( "
        "manifest_id, relation_kind, target_kind, target_key, route_role, event_kind, revision, details_json "
        ") VALUES( ?, ?, ?, ?, ?, ?, ?, ? )",
        [
            (
                staged.manifest_id,
                kind,
                target_kind,
                target_key,
                route_role,
                event_kind,
                revision,
                _json_text({"source_sha256": staged.source_sha256}),
            )
            for kind, target_kind, target_key, route_role in staged.relations
        ],
    )


def rebuild_manifest_projection(
    connection: sqlite3.Connection,
    scenarios_root: Path,
) -> Dict[str, int]:
    """Transactionally project staged scenario sources without rewriting them."""
    staged = stage_manifest_projection(scenarios_root)
    discovered = 0
    changed = 0
    absent = 0

    with immediate_transaction(connection):
        existing_rows = {
            str(row["source_path"]): row
            for row in connection.execute(
                "SELECT manifest_id, source_path, present, revision, current_sha256, last_content_sha256 "
                "FROM manifest_current"
            )
        }
        staged_paths = {entry.source_path for entry in staged}
        for entry in staged:
            previous = existing_rows.get(entry.source_path)
            if previous is None:
                revision = 1
                event_kind = "discovery"
                connection.execute(
                    "INSERT INTO manifest_current( "
                    "manifest_id, source_path, present, revision, current_sha256, last_content_sha256, "
                    "declaration_json, normalized_json, validation_json, last_seen_at, absent_at "
                    ") VALUES( ?, ?, 1, ?, ?, ?, ?, ?, ?, CURRENT_TIMESTAMP, NULL )",
                    (
                        entry.manifest_id,
                        entry.source_path,
                        revision,
                        entry.source_sha256,
                        entry.source_sha256,
                        entry.declaration_json,
                        entry.normalized_json,
                        entry.validation_json,
                    ),
                )
                discovered += 1
            else:
                if str(previous["manifest_id"]) != entry.manifest_id:
                    raise ScenarioRegistryStoreError(
                        f"Manifest identity mismatch for canonical source path {entry.source_path}"
                    )
                previous_hash = str(previous["last_content_sha256"] or "")
                content_changed = previous_hash != entry.source_sha256
                was_absent = int(previous["present"]) == 0
                revision = int(previous["revision"]) + (1 if content_changed else 0)
                event_kind = "change" if content_changed else ("discovery" if was_absent else "")
                connection.execute(
                    "UPDATE manifest_current SET present = 1, revision = ?, current_sha256 = ?, "
                    "last_content_sha256 = ?, declaration_json = ?, normalized_json = ?, validation_json = ?, "
                    "last_seen_at = CURRENT_TIMESTAMP, absent_at = NULL WHERE manifest_id = ?",
                    (
                        revision,
                        entry.source_sha256,
                        entry.source_sha256,
                        entry.declaration_json,
                        entry.normalized_json,
                        entry.validation_json,
                        entry.manifest_id,
                    ),
                )
                if content_changed:
                    changed += 1
                    _invalidate_manifest_tokens(
                        connection,
                        manifest_id=entry.manifest_id,
                        reason="manifest_changed",
                        details={"source_path": entry.source_path, "source_sha256": entry.source_sha256},
                    )
                elif was_absent:
                    discovered += 1

            if event_kind:
                connection.execute(
                    "INSERT INTO lifecycle_history( manifest_id, event_kind, revision, cause_sha256, details_json ) "
                    "VALUES( ?, ?, ?, ?, ? )",
                    (
                        entry.manifest_id,
                        event_kind,
                        revision,
                        entry.source_sha256,
                        _json_text({"source_path": entry.source_path}),
                    ),
                )
                _append_relation_events(connection, entry, event_kind=event_kind, revision=revision)
            _replace_current_materializations(connection, entry)

        for source_path, previous in existing_rows.items():
            if source_path in staged_paths or int(previous["present"]) == 0:
                continue
            manifest_id = str(previous["manifest_id"])
            revision = int(previous["revision"])
            last_hash = str(previous["last_content_sha256"] or "")
            current_relations = connection.execute(
                "SELECT relation_kind, target_kind, target_key, route_role FROM manifest_relation_current "
                "WHERE manifest_id = ?",
                (manifest_id,),
            ).fetchall()
            connection.execute(
                "UPDATE manifest_current SET present = 0, current_sha256 = NULL, absent_at = CURRENT_TIMESTAMP "
                "WHERE manifest_id = ?",
                (manifest_id,),
            )
            connection.execute(
                "DELETE FROM manifest_capability_current WHERE manifest_id = ?",
                (manifest_id,),
            )
            connection.execute(
                "DELETE FROM manifest_proof_route_current WHERE manifest_id = ?",
                (manifest_id,),
            )
            connection.execute(
                "DELETE FROM manifest_relation_current WHERE manifest_id = ?",
                (manifest_id,),
            )
            connection.execute(
                "INSERT INTO lifecycle_history( manifest_id, event_kind, revision, cause_sha256, details_json ) "
                "VALUES( ?, 'absence', ?, ?, ? )",
                (manifest_id, revision, last_hash, _json_text({"source_path": source_path})),
            )
            connection.executemany(
                "INSERT INTO manifest_relation_history( "
                "manifest_id, relation_kind, target_kind, target_key, route_role, event_kind, revision, details_json "
                ") VALUES( ?, ?, ?, ?, ?, 'absence', ?, ? )",
                [
                    (
                        manifest_id,
                        str(row["relation_kind"]),
                        str(row["target_kind"]),
                        str(row["target_key"]),
                        str(row["route_role"]),
                        revision,
                        _json_text({"source_path": source_path}),
                    )
                    for row in current_relations
                ],
            )
            _invalidate_manifest_tokens(
                connection,
                manifest_id=manifest_id,
                reason="manifest_absent",
                details={"source_path": source_path, "source_sha256": last_hash},
            )
            absent += 1

    return {
        "staged": len(staged),
        "discovered": discovered,
        "changed": changed,
        "absent": absent,
    }


def _identity(prefix: str, *parts: str) -> str:
    source = prefix.encode("utf-8") + b"\0" + b"\0".join(part.encode("utf-8") for part in parts)
    return hashlib.sha256(source).hexdigest()


def _report_path_and_bytes(report_path: Path) -> Tuple[str, bytes]:
    canonical_path = str(report_path.resolve())
    try:
        return canonical_path, report_path.read_bytes()
    except OSError as exc:
        raise ScenarioRegistryStoreError(f"Could not read report reference {canonical_path}: {exc}") from exc


def _object(value: Any, field: str) -> Mapping[str, Any]:
    if not isinstance(value, dict):
        raise ScenarioRegistryStoreError(f"Report field {field} must be an object")
    return value


def _string(value: Any, field: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ScenarioRegistryStoreError(f"Report field {field} must be a non-empty string")
    return value.strip()


def _selected_mapping(value: Any, keys: Sequence[str]) -> Dict[str, Any]:
    if not isinstance(value, dict):
        return {}
    selected: Dict[str, Any] = {}
    for key in keys:
        item = value.get(key)
        if isinstance(item, (bool, int, float, str)) or item is None:
            selected[key] = item
        elif isinstance(item, list) and all(isinstance(entry, str) for entry in item):
            selected[key] = list(item)
    return selected


def _extract_report_facts(report: Mapping[str, Any]) -> Dict[str, Any]:
    scenario_manifest = _object(report.get("scenario_manifest"), "scenario_manifest")
    source = _object(scenario_manifest.get("source"), "scenario_manifest.source")
    source_path = str(Path(_string(source.get("path"), "scenario_manifest.source.path")).resolve())
    source_sha256 = _string(source.get("sha256"), "scenario_manifest.source.sha256").lower()
    contract = report.get("contract") if isinstance(report.get("contract"), dict) else {}
    startup = report.get("startup") if isinstance(report.get("startup"), dict) else {}
    screen = startup.get("screen") if isinstance(startup.get("screen"), dict) else {}
    proof_source = report.get("proof_classification")
    if not isinstance(proof_source, dict):
        proof_source = startup.get("proof_classification")
    proof = _selected_mapping(
        proof_source,
        ("status", "verdict", "evidence_class", "feature_proof"),
    )
    proof["route_verdict"] = str(report.get("verdict", "")).strip()
    proof["route_evidence_class"] = str(report.get("evidence_class", "")).strip()
    proof["route_feature_proof"] = bool(report.get("feature_proof", False))
    supersedes_verification_id = report.get("supersedes_verification_id")
    if supersedes_verification_id is not None:
        supersedes_verification_id = _string(
            supersedes_verification_id,
            "supersedes_verification_id",
        )
    runtime = {
        "runtime_binding_status": screen.get("runtime_binding_status"),
        "runtime_binding_observed": _selected_mapping(
            screen.get("runtime_binding_observed"),
            ("status", "executable_path", "executable_sha256", "runtime_source_sha256"),
        ),
    }
    fixture = {
        "fixture": contract.get("fixture"),
        "fixture_profile": contract.get("fixture_profile"),
        "installed": _selected_mapping(
            startup.get("fixture_install"),
            (
                "fixture", "fixture_profile", "resolved_fixture", "resolved_fixture_profile",
                "source_path", "source_sha256", "destination",
            ),
        ),
    }
    profile = {
        "profile": contract.get("profile") or report.get("profile"),
        "config_profile": contract.get("config_profile") or report.get("config_profile"),
        "profile_snapshot": contract.get("profile_snapshot"),
        "profile_snapshot_profile": contract.get("profile_snapshot_profile"),
        "snapshot_install": _selected_mapping(
            startup.get("profile_snapshot"),
            (
                "profile", "snapshot", "snapshot_profile", "resolved_snapshot",
                "resolved_snapshot_profile", "source_path", "source_sha256",
            ),
        ),
    }
    return {
        "scenario": str(report.get("scenario", "")).strip(),
        "mode": str(report.get("mode", "")).strip(),
        "manifest": {
            "source_path": source_path,
            "source_sha256": source_sha256,
        },
        "runtime": runtime,
        "fixture": fixture,
        "profile": profile,
        "proof": proof,
        "supersedes_verification_id": supersedes_verification_id,
    }


def _adapter_result(kind: str, adapter: Callable[[Mapping[str, Any]], Mapping[str, Any]], expected: Mapping[str, Any]) -> Dict[str, Any]:
    result = adapter(expected)
    if not isinstance(result, Mapping):
        raise ScenarioRegistryStoreError(f"{kind} binding adapter must return an object")
    status = str(result.get("status", "")).strip().lower()
    if status not in {"compatible", "stale"}:
        raise ScenarioRegistryStoreError(f"{kind} binding adapter must return compatible or stale")
    facts = result.get("facts", {})
    if not isinstance(facts, Mapping):
        raise ScenarioRegistryStoreError(f"{kind} binding adapter facts must be an object")
    source_sha256 = facts.get("source_sha256")
    if kind in {"fixture", "profile"} and (
        not isinstance(source_sha256, str)
        or len(source_sha256) != 64
        or any(character not in "0123456789abcdef" for character in source_sha256.lower())
    ):
        raise ScenarioRegistryStoreError(f"{kind} binding adapter must provide a SHA-256 source_sha256 fact")
    return {
        "status": status,
        "facts": dict(facts),
    }


def _binding_fingerprint(
    report_id: str,
    kind: str,
    expected: Mapping[str, Any],
    facts: Mapping[str, Any],
) -> str:
    """Bind the report declaration to facts recomputed by the current owner."""
    return _identity(
        "caol-scenario-binding-v1",
        report_id,
        kind,
        _json_text(expected),
        _json_text(facts),
    )


def _append_binding(
    connection: sqlite3.Connection,
    *,
    manifest_id: str,
    report_id: str,
    verification_id: str,
    kind: str,
    expected: Mapping[str, Any],
    status: str,
    facts: Mapping[str, Any],
) -> str:
    fingerprint = _binding_fingerprint(report_id, kind, expected, facts)
    payload = {
        "report_id": report_id,
        "verification_id": verification_id,
        "expected": dict(expected),
        "facts": dict(facts),
    }
    connection.execute(
        "INSERT OR IGNORE INTO binding_history( "
        "manifest_id, binding_kind, binding_fingerprint, binding_status, payload_json "
        ") VALUES( ?, ?, ?, ?, ? )",
        (manifest_id, kind, fingerprint, status, _json_text(payload)),
    )
    return fingerprint


def _append_resolution_if_changed(
    connection: sqlite3.Connection,
    *,
    verification_id: str,
    manifest_id: str,
    route_key: str,
    resolution_kind: str,
    binding_fingerprint: str,
    details: Mapping[str, Any],
) -> None:
    latest = connection.execute(
        "SELECT resolution_kind, binding_fingerprint FROM verification_resolution_history "
        "WHERE verification_id = ? ORDER BY resolution_event_id DESC LIMIT 1",
        (verification_id,),
    ).fetchone()
    if latest is not None and str(latest["resolution_kind"]) == resolution_kind and str(latest["binding_fingerprint"]) == binding_fingerprint:
        return
    connection.execute(
        "INSERT INTO verification_resolution_history( "
        "verification_id, manifest_id, route_key, resolution_kind, binding_fingerprint, details_json "
        ") VALUES( ?, ?, ?, ?, ?, ? )",
        (verification_id, manifest_id, route_key, resolution_kind, binding_fingerprint, _json_text(details)),
    )


def _verification_evidence_state(row: sqlite3.Row) -> str:
    """Classify report proof without allowing declarations or unknowns to prove."""
    details = json.loads(str(row["details_json"]))
    proof = details.get("proof", {}) if isinstance(details, dict) else {}
    status = str(proof.get("status", "")).strip().lower()
    if status == "red":
        return "contradicted"
    if (
        status == "green"
        and bool(proof.get("feature_proof", False))
        and str(proof.get("evidence_class", "")).strip() == "feature-path"
    ):
        return "hard_proven"
    return "unknown"


def _append_route_evidence_if_changed(
    connection: sqlite3.Connection,
    *,
    manifest_id: str,
    route_key: str,
    evidence_state: str,
    details: Mapping[str, Any],
) -> None:
    value_json = _json_text({"route_key": route_key, "state": evidence_state})
    value_sha256 = _identity(
        "caol-scenario-route-evidence-v1",
        manifest_id,
        route_key,
        evidence_state,
        _json_text(details),
    )
    existing = connection.execute(
        "SELECT capability_evidence_id FROM capability_evidence_history "
        "WHERE manifest_id = ? AND verification_id IS NULL AND capability_key = '_registry.proof_route' "
        "AND evidence_kind = 'route_resolution' AND value_sha256 = ?",
        (manifest_id, value_sha256),
    ).fetchone()
    if existing is None:
        connection.execute(
            "INSERT INTO capability_evidence_history( "
            "manifest_id, capability_key, evidence_kind, evidence_state, value_json, value_sha256, details_json "
            ") VALUES( ?, '_registry.proof_route', 'route_resolution', ?, ?, ?, ? )",
            (manifest_id, evidence_state, value_json, value_sha256, _json_text(details)),
        )


def _append_lifecycle_if_changed(
    connection: sqlite3.Connection,
    *,
    manifest_id: str,
    event_kind: str,
    details: Mapping[str, Any],
) -> None:
    details_json = _json_text(details)
    existing = connection.execute(
        "SELECT lifecycle_event_id FROM lifecycle_history "
        "WHERE manifest_id = ? AND event_kind = ? AND details_json = ?",
        (manifest_id, event_kind, details_json),
    ).fetchone()
    if existing is not None:
        return
    manifest = connection.execute(
        "SELECT revision, last_content_sha256 FROM manifest_current WHERE manifest_id = ?",
        (manifest_id,),
    ).fetchone()
    if manifest is None:
        raise ScenarioRegistryStoreError("route resolution references a missing manifest")
    connection.execute(
        "INSERT INTO lifecycle_history( manifest_id, event_kind, revision, cause_sha256, details_json ) "
        "VALUES( ?, ?, ?, ?, ? )",
        (
            manifest_id,
            event_kind,
            int(manifest["revision"]),
            manifest["last_content_sha256"],
            details_json,
        ),
    )


def _append_quarantine_if_changed(
    connection: sqlite3.Connection,
    *,
    manifest_id: str,
    route_key: str,
    quarantine_kind: str,
    details: Mapping[str, Any],
) -> None:
    latest = connection.execute(
        "SELECT quarantine_kind FROM quarantine_history WHERE manifest_id = ? AND route_key = ? "
        "ORDER BY quarantine_event_id DESC LIMIT 1",
        (manifest_id, route_key),
    ).fetchone()
    if latest is not None and str(latest["quarantine_kind"]) == quarantine_kind:
        return
    connection.execute(
        "INSERT INTO quarantine_history( manifest_id, route_key, quarantine_kind, details_json ) "
        "VALUES( ?, ?, ?, ? )",
        (manifest_id, route_key, quarantine_kind, _json_text(details)),
    )


def _invalidate_outstanding_tokens(
    connection: sqlite3.Connection,
    *,
    manifest_id: str,
    route_key: str,
    reason: str,
    details: Mapping[str, Any],
) -> int:
    outstanding = connection.execute(
        "SELECT issued.token_id, issued.verification_id FROM token_history AS issued "
        "WHERE issued.manifest_id = ? AND issued.route_key = ? AND issued.event_kind = 'issued' "
        "AND NOT EXISTS( SELECT 1 FROM token_history AS invalidated "
        "WHERE invalidated.token_id = issued.token_id AND invalidated.event_kind = 'invalidated' )",
        (manifest_id, route_key),
    ).fetchall()
    for token in outstanding:
        connection.execute(
            "INSERT OR IGNORE INTO token_history( "
            "token_id, manifest_id, verification_id, route_key, event_kind, reason, details_json "
            ") VALUES( ?, ?, ?, ?, 'invalidated', ?, ? )",
            (
                str(token["token_id"]),
                manifest_id,
                token["verification_id"],
                route_key,
                reason,
                _json_text(details),
            ),
        )
    return len(outstanding)


def _resolve_route_evidence(
    connection: sqlite3.Connection,
    *,
    manifest_id: str,
    route_key: str,
) -> str:
    """Append the current route decision without rewriting report or declaration history."""
    rows = connection.execute(
        "SELECT verification_id, proof_status, supersedes_verification_id, details_json FROM verification_history "
        "WHERE manifest_id = ? AND route_key = ? AND EXISTS( "
        "SELECT 1 FROM verification_resolution_history AS resolution "
        "WHERE resolution.verification_id = verification_history.verification_id "
        "AND resolution.resolution_event_id = ( SELECT MAX( latest.resolution_event_id ) "
        "FROM verification_resolution_history AS latest WHERE latest.verification_id = verification_history.verification_id ) "
        "AND resolution.resolution_kind = 'compatible' )",
        (manifest_id, route_key),
    ).fetchall()
    by_id = {str(row["verification_id"]): row for row in rows}
    hard_proven = {
        verification_id
        for verification_id, row in by_id.items()
        if _verification_evidence_state(row) == "hard_proven"
    }
    contradicted = {
        verification_id
        for verification_id, row in by_id.items()
        if _verification_evidence_state(row) == "contradicted"
    }
    superseded = {
        str(row["supersedes_verification_id"])
        for row in by_id.values()
        if (
            _verification_evidence_state(row) == "hard_proven"
            and row["supersedes_verification_id"] is not None
            and str(row["supersedes_verification_id"]) in contradicted
        )
    }
    unresolved_contradictions = sorted(contradicted - superseded)
    if not rows:
        evidence_state = "stale"
    elif unresolved_contradictions:
        evidence_state = "contradicted"
    elif hard_proven:
        evidence_state = "hard_proven"
    else:
        evidence_state = "unknown"
    details = {
        "route_key": route_key,
        "compatible_verification_ids": sorted(by_id),
        "hard_proven_verification_ids": sorted(hard_proven),
        "unresolved_contradiction_ids": unresolved_contradictions,
        "superseded_contradiction_ids": sorted(superseded),
    }
    _append_route_evidence_if_changed(
        connection,
        manifest_id=manifest_id,
        route_key=route_key,
        evidence_state=evidence_state,
        details=details,
    )
    _append_lifecycle_if_changed(
        connection,
        manifest_id=manifest_id,
        event_kind=f"proof_route_{evidence_state}",
        details=details,
    )
    if not rows:
        _append_quarantine_if_changed(
            connection,
            manifest_id=manifest_id,
            route_key=route_key,
            quarantine_kind="quarantined_no_compatible_verification",
            details=details,
        )
    elif connection.execute(
        "SELECT quarantine_event_id FROM quarantine_history WHERE manifest_id = ? AND route_key = ?",
        (manifest_id, route_key),
    ).fetchone() is not None:
        _append_quarantine_if_changed(
            connection,
            manifest_id=manifest_id,
            route_key=route_key,
            quarantine_kind="released_compatible_verification",
            details=details,
        )
    if evidence_state != "hard_proven":
        _invalidate_outstanding_tokens(
            connection,
            manifest_id=manifest_id,
            route_key=route_key,
            reason=f"proof_route_{evidence_state}",
            details=details,
        )
    return evidence_state


def _evaluation_for_facts(
    connection: sqlite3.Connection,
    *,
    report_id: str,
    verification_id: str,
    facts: Mapping[str, Any],
    adapters: BindingAdapters,
) -> Tuple[str, str, Dict[str, str]]:
    manifest_source = _object(facts["manifest"], "manifest")
    manifest_row = connection.execute(
        "SELECT manifest_id FROM manifest_current WHERE source_path = ? AND present = 1 AND current_sha256 = ?",
        (manifest_source["source_path"], manifest_source["source_sha256"]),
    ).fetchone()
    if manifest_row is None:
        raise ScenarioRegistryStoreError("Report scenario manifest is not a current present registry manifest")
    manifest_id = str(manifest_row["manifest_id"])
    component_results: List[Tuple[str, Mapping[str, Any], str, Mapping[str, Any]]] = [
        (
            "manifest",
            manifest_source,
            "compatible",
            {"source_path": manifest_source["source_path"], "source_sha256": manifest_source["source_sha256"]},
        ),
    ]
    for kind, expected, adapter in (
        ("runtime", _object(facts["runtime"], "runtime"), adapters.runtime),
        ("fixture", _object(facts["fixture"], "fixture"), adapters.fixture),
        ("profile", _object(facts["profile"], "profile"), adapters.profile),
    ):
        result = _adapter_result(kind, adapter, expected)
        component_results.append((kind, expected, result["status"], result["facts"]))

    fingerprints: Dict[str, str] = {}
    statuses: Dict[str, str] = {}
    for kind, expected, status, observed in component_results:
        fingerprints[kind] = _append_binding(
            connection,
            manifest_id=manifest_id,
            report_id=report_id,
            verification_id=verification_id,
            kind=kind,
            expected=expected,
            status=status,
            facts=observed,
        )
        statuses[kind] = status
    aggregate = _identity(
        "caol-scenario-binding-set-v1",
        *[f"{kind}:{fingerprints[kind]}" for kind in sorted(fingerprints)],
    )
    return manifest_id, aggregate, statuses


def ingest_report_reference(
    connection: sqlite3.Connection,
    report_path: Path,
    *,
    adapters: BindingAdapters,
) -> Dict[str, Any]:
    """Ingest a full report by reference/hash, never by copying its body."""
    canonical_path, report_bytes = _report_path_and_bytes(report_path)
    report_sha256 = hashlib.sha256(report_bytes).hexdigest()
    report_id = _identity("caol-scenario-report-v1", canonical_path, report_sha256)
    existing = connection.execute(
        "SELECT report_id, ingestion_status, error_text FROM report_ingestion_history "
        "WHERE report_path = ? AND report_sha256 = ?",
        (canonical_path, report_sha256),
    ).fetchone()
    if existing is not None:
        return {
            "report_id": str(existing["report_id"]),
            "status": str(existing["ingestion_status"]),
            "error": str(existing["error_text"]),
            "idempotent": True,
        }
    try:
        report = json.loads(report_bytes.decode("utf-8"))
        if not isinstance(report, dict):
            raise ScenarioRegistryStoreError("Report top level must be an object")
        facts = _extract_report_facts(report)
    except (UnicodeDecodeError, json.JSONDecodeError, ScenarioRegistryStoreError) as exc:
        with immediate_transaction(connection):
            connection.execute(
                "INSERT INTO report_ingestion_history( report_id, report_path, report_sha256, report_kind, ingestion_status, error_text ) "
                "VALUES( ?, ?, ?, 'unknown', 'invalid_report', ? )",
                (report_id, canonical_path, report_sha256, str(exc)),
            )
        return {"report_id": report_id, "status": "invalid_report", "error": str(exc), "idempotent": False}

    route_key = _identity(
        "caol-scenario-proof-route-v2",
        facts["manifest"]["source_path"],
        facts["scenario"],
    )
    verification_id = _identity("caol-scenario-verification-v1", report_id, route_key)
    with immediate_transaction(connection):
        try:
            manifest_id, aggregate_binding, statuses = _evaluation_for_facts(
                connection,
                report_id=report_id,
                verification_id=verification_id,
                facts=facts,
                adapters=adapters,
            )
        except ScenarioRegistryStoreError as exc:
            connection.execute(
                "INSERT INTO report_ingestion_history( report_id, report_path, report_sha256, report_kind, ingestion_status, error_text ) "
                "VALUES( ?, ?, ?, ?, 'rejected_manifest', ? )",
                (report_id, canonical_path, report_sha256, facts["mode"] or "report", str(exc)),
            )
            return {"report_id": report_id, "status": "rejected_manifest", "error": str(exc), "idempotent": False}

        report_binding = _append_binding(
            connection,
            manifest_id=manifest_id,
            report_id=report_id,
            verification_id=verification_id,
            kind="report",
            expected={"path": canonical_path, "sha256": report_sha256},
            status="compatible",
            facts={"path": canonical_path, "sha256": report_sha256},
        )
        statuses["report"] = "compatible"
        aggregate_binding = _identity(
            "caol-scenario-binding-set-v1",
            aggregate_binding,
            f"report:{report_binding}",
        )
        supersedes_verification_id = facts["supersedes_verification_id"]
        if supersedes_verification_id is not None:
            superseded = connection.execute(
                "SELECT manifest_id, route_key FROM verification_history WHERE verification_id = ?",
                (supersedes_verification_id,),
            ).fetchone()
            if (
                superseded is None
                or str(superseded["manifest_id"]) != manifest_id
                or str(superseded["route_key"]) != route_key
            ):
                supersedes_verification_id = None

        connection.execute(
            "INSERT INTO report_ingestion_history( "
            "report_id, manifest_id, report_path, report_sha256, report_kind, ingestion_status "
            ") VALUES( ?, ?, ?, ?, ?, 'ingested' )",
            (report_id, manifest_id, canonical_path, report_sha256, facts["mode"] or "report"),
        )
        proof = _object(facts["proof"], "proof")
        connection.execute(
            "INSERT INTO verification_history( "
            "verification_id, manifest_id, report_id, route_key, binding_fingerprint, outcome_kind, proof_status, "
            "supersedes_verification_id, details_json ) VALUES( ?, ?, ?, ?, ?, ?, ?, ?, ? )",
            (
                verification_id,
                manifest_id,
                report_id,
                route_key,
                aggregate_binding,
                str(proof.get("verdict", "unknown")),
                str(proof.get("status", "unknown")),
                supersedes_verification_id,
                _json_text({
                    "scenario": facts["scenario"],
                    "proof": proof,
                    "manifest": facts["manifest"],
                    "requested_supersedes_verification_id": facts["supersedes_verification_id"],
                }),
            ),
        )
        resolution = "compatible" if all(status == "compatible" for status in statuses.values()) else "stale"
        _append_resolution_if_changed(
            connection,
            verification_id=verification_id,
            manifest_id=manifest_id,
            route_key=route_key,
            resolution_kind=resolution,
            binding_fingerprint=aggregate_binding,
            details={"statuses": statuses},
        )
        eligibility = _resolve_route_evidence(
            connection,
            manifest_id=manifest_id,
            route_key=route_key,
        )
    return {
        "report_id": report_id,
        "verification_id": verification_id,
        "status": "ingested",
        "resolution": resolution,
        "eligibility": eligibility,
        "idempotent": False,
    }


def reconcile_report_bindings(connection: sqlite3.Connection, *, adapters: BindingAdapters) -> Dict[str, int]:
    """Recompute report/manifest/runtime/fixture/profile compatibility by reference."""
    reconciled = 0
    stale = 0
    references = connection.execute(
        "SELECT report_id, report_path, report_sha256 FROM report_ingestion_history WHERE ingestion_status = 'ingested'"
    ).fetchall()
    for reference in references:
        report_id = str(reference["report_id"])
        verification = connection.execute(
            "SELECT verification_id, manifest_id, route_key FROM verification_history WHERE report_id = ?",
            (report_id,),
        ).fetchone()
        if verification is None:
            continue
        reason = ""
        facts: Optional[Dict[str, Any]] = None
        try:
            canonical_path, report_bytes = _report_path_and_bytes(Path(str(reference["report_path"])))
            if canonical_path != str(reference["report_path"]) or hashlib.sha256(report_bytes).hexdigest() != str(reference["report_sha256"]):
                raise ScenarioRegistryStoreError("report reference is missing or its content hash changed")
            report = json.loads(report_bytes.decode("utf-8"))
            if not isinstance(report, dict):
                raise ScenarioRegistryStoreError("Report top level must be an object")
            facts = _extract_report_facts(report)
        except (OSError, UnicodeDecodeError, json.JSONDecodeError, ScenarioRegistryStoreError) as exc:
            reason = str(exc)

        with immediate_transaction(connection):
            verification_id = str(verification["verification_id"])
            manifest_id = str(verification["manifest_id"])
            route_key = str(verification["route_key"])
            if facts is None:
                fingerprint = _append_binding(
                    connection,
                    manifest_id=manifest_id,
                    report_id=report_id,
                    verification_id=verification_id,
                    kind="report",
                    expected={"path": str(reference["report_path"]), "sha256": str(reference["report_sha256"])},
                    status="stale",
                    facts={"reason": reason},
                )
                _append_resolution_if_changed(
                    connection,
                    verification_id=verification_id,
                    manifest_id=manifest_id,
                    route_key=route_key,
                    resolution_kind="stale",
                    binding_fingerprint=fingerprint,
                    details={"reason": reason},
                )
                _resolve_route_evidence(
                    connection,
                    manifest_id=manifest_id,
                    route_key=route_key,
                )
                reconciled += 1
                stale += 1
                continue
            try:
                observed_manifest_id, aggregate_binding, statuses = _evaluation_for_facts(
                    connection,
                    report_id=report_id,
                    verification_id=verification_id,
                    facts=facts,
                    adapters=adapters,
                )
                if observed_manifest_id != manifest_id:
                    raise ScenarioRegistryStoreError("report manifest identity no longer matches verification")
                report_binding = _append_binding(
                    connection,
                    manifest_id=manifest_id,
                    report_id=report_id,
                    verification_id=verification_id,
                    kind="report",
                    expected={"path": str(reference["report_path"]), "sha256": str(reference["report_sha256"])},
                    status="compatible",
                    facts={"path": str(reference["report_path"]), "sha256": str(reference["report_sha256"])},
                )
                statuses["report"] = "compatible"
                aggregate_binding = _identity(
                    "caol-scenario-binding-set-v1",
                    aggregate_binding,
                    f"report:{report_binding}",
                )
                resolution = "compatible" if all(status == "compatible" for status in statuses.values()) else "stale"
                details: Mapping[str, Any] = {"statuses": statuses}
            except ScenarioRegistryStoreError as exc:
                _append_binding(
                    connection,
                    manifest_id=manifest_id,
                    report_id=report_id,
                    verification_id=verification_id,
                    kind="manifest",
                    expected=_object(facts["manifest"], "manifest"),
                    status="stale",
                    facts={"reason": str(exc)},
                )
                aggregate_binding = _identity("caol-scenario-binding-set-v1", report_id, "manifest", str(exc))
                resolution = "stale"
                details = {"reason": str(exc)}
            _append_resolution_if_changed(
                connection,
                verification_id=verification_id,
                manifest_id=manifest_id,
                route_key=route_key,
                resolution_kind=resolution,
                binding_fingerprint=aggregate_binding,
                details=details,
            )
            _resolve_route_evidence(
                connection,
                manifest_id=manifest_id,
                route_key=route_key,
            )
            reconciled += 1
            stale += int(resolution == "stale")
    return {"reconciled": reconciled, "stale": stale}
