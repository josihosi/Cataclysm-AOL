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
import secrets
from pathlib import Path
import sqlite3
import uuid
from typing import Any, Callable, Dict, Iterator, List, Mapping, Optional, Sequence, Tuple

from scenario_registry import (
    CAPABILITY_NAMESPACE_PREFIXES,
    ManifestValidationError,
    normalize_relation_contract,
    relation_contract_likely_subsumes,
    validate_manifest,
)
from wec_evidence import derive_final_gate_eligibility, validate_authority_fact
from identity_binding import (
    RoundManifestError,
    _plain,
    _validate_round_manifest,
    authoritative_identity_binding,
    ecology_actor_identity,
    seal_complete_round_manifest,
)
from certification_route import evaluate_continuous_certification
from production_capture import RelaunchReceiptError, normalize_relaunch_receipt
from r019_acceptance_matrix import validate_r019_acceptance_matrix, validate_r019_report_packet
from playtest_witness import (
    WitnessError,
    review_witness,
    validate_witness_statement,
)


SCHEMA_VERSION = 21
Migration = Tuple[int, str, Callable[[sqlite3.Connection], None]]


class ScenarioRegistryStoreError(RuntimeError):
    """The registry database could not be opened or migrated safely."""


class ScenarioRegistryQueryError(ValueError):
    """A registry query request or explicit candidate fact is not well-typed."""


@dataclass(frozen=True)
class MigrationSnapshotItem:
    """One exact source identity captured before migration parsing begins."""

    source_path: str
    source_sha256: str
    attempt_identity: str


@dataclass(frozen=True)
class MigrationRunSnapshot:
    """An immutable inventory snapshot and the run which owns it."""

    migration_run_id: str
    run_identity: str
    scenarios_root: str
    items: Tuple[MigrationSnapshotItem, ...]


@dataclass(frozen=True)
class MigrationItemEvent:
    """One append-only migration item transition."""

    migration_item_event_id: int
    event_kind: str
    completion_status: str
    disposition: str
    reason: str
    details: Mapping[str, Any]
    recorded_at: str


@dataclass(frozen=True)
class MigrationItemCurrent:
    """Current derived state for one frozen path/SHA migration identity."""

    migration_run_id: str
    attempt_identity: str
    source_path: str
    source_sha256: str
    status: str
    terminal_disposition: Optional[str]
    launch_claimed: bool
    history: Tuple[MigrationItemEvent, ...]


def path_sha256(path: Path) -> str:
    """Hash one resolved file or directory tree for a durable binding."""
    try:
        resolved = path.resolve(strict=True)
    except OSError as exc:
        raise ScenarioRegistryStoreError(f"Could not resolve binding path {path}: {exc}") from exc
    digest = hashlib.sha256()
    if resolved.is_file():
        digest.update(resolved.read_bytes())
        return digest.hexdigest()
    if not resolved.is_dir():
        raise ScenarioRegistryStoreError(f"Binding path is neither a file nor directory: {resolved}")
    digest.update(b"caol-scenario-directory-binding-v1\0")
    for entry in sorted(resolved.rglob("*"), key=lambda item: str(item.relative_to(resolved))):
        relative = str(entry.relative_to(resolved)).replace("\\", "/")
        if entry.is_dir():
            digest.update(b"directory\0" + relative.encode("utf-8") + b"\0")
            continue
        if not entry.is_file():
            raise ScenarioRegistryStoreError(f"Binding tree contains unsupported entry: {entry}")
        content_hash = hashlib.sha256(entry.read_bytes()).hexdigest()
        digest.update(b"file\0" + relative.encode("utf-8") + b"\0" + content_hash.encode("ascii") + b"\0")
    return digest.hexdigest()


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
    """A non-executing query result that may include one bound technical run token."""

    query_id: str
    query_sha256: str
    evaluation: RegistryStoredQueryEvaluation
    token_id: Optional[str]
    draft_path: Optional[str]
    selection_id: Optional[str] = None
    next_action: Optional[Mapping[str, Any]] = None


def _canonical_hash(value: Any, label: str) -> str:
    return hashlib.sha256((label + ":" + _json_text(value)).encode("utf-8")).hexdigest()


def _coordinator_authorization(
    request: RegistryQueryRequest, candidate: RegistryQueryCandidateSnapshot,
    brief: Any, charter: Any,
) -> Optional[Mapping[str, Any]]:
    """Validate the explicit brief/charter escape hatch without inferring intent."""
    if brief is None and charter is None:
        return None
    if not isinstance(brief, Mapping) or not isinstance(charter, Mapping):
        raise ScenarioRegistryStoreError("coordinator brief and witness charter are both required")
    try:
        from playtest_witness import normalize_witness_charter
        normalized = normalize_witness_charter(charter)
    except (ValueError, WitnessError) as exc:
        raise ScenarioRegistryStoreError("witness charter is invalid") from exc
    outcome = str(brief.get("outcome", brief.get("desired_outcome", ""))).strip()
    if not outcome or outcome != str(normalized.get("claim", "")):
        raise ScenarioRegistryStoreError("coordinator brief outcome does not match witness charter claim")
    brief_query = brief.get("query", brief.get("typed_query"))
    if brief_query is None:
        raise ScenarioRegistryStoreError("coordinator brief typed query is missing")
    try:
        brief_request = parse_registry_query_request(brief_query)
    except ScenarioRegistryQueryError as exc:
        raise ScenarioRegistryStoreError("coordinator brief typed query is invalid") from exc
    if _query_request_json(brief_request) != _query_request_json(request):
        raise ScenarioRegistryStoreError("coordinator brief typed query does not match registry query")
    named_candidate = str(brief.get("scenario_id", brief.get("scenario", ""))).strip()
    if named_candidate and named_candidate not in {candidate.scenario_id,
                                                    str(candidate.explanation.get("manifest", {}).get("name", ""))}:
        raise ScenarioRegistryStoreError("coordinator brief candidate does not match selection")
    if candidate.lifecycle_state != "active" or not candidate.token_eligible or \
            not bool(candidate.explanation.get("manifest", {}).get("executable")):
        raise ScenarioRegistryStoreError("coordinator authorization requires current active executable candidate")
    charter_id = str(brief.get("charter_id", brief.get("witness_charter_id", ""))).strip()
    if charter_id and charter_id != str(normalized["charter_id"]):
        raise ScenarioRegistryStoreError("coordinator brief charter does not match validated charter")
    return {
        "brief_sha256": _canonical_hash(dict(brief), "caol-coordinator-brief-v1"),
        "charter_sha256": _canonical_hash(normalized, "caol-witness-charter-v1"),
        "charter_id": normalized["charter_id"],
        "outcome": outcome,
    }


@dataclass(frozen=True)
class RegistryLaunchToken:
    """A token reloaded from current registry owners for one canonical launch."""

    token_id: str
    accepted: bool
    reason: str
    scenario: str = ""
    source_path: str = ""


@dataclass(frozen=True)
class RegistryBootstrapToken:
    """One separate, single-use authority for an initial compatible probe."""

    token_id: str
    accepted: bool
    reason: str
    scenario: str = ""
    source_path: str = ""
    runtime_binding: Mapping[str, Any] | None = None


@dataclass(frozen=True)
class RegistryRepairToken:
    """One separate, single-use authority to repair one contradicted route."""

    token_id: str
    accepted: bool
    reason: str
    scenario: str = ""
    source_path: str = ""
    runtime_binding: Mapping[str, Any] | None = None


@dataclass(frozen=True)
class RegistryR019AggregationToken:
    """One separate, single-use authority for a zero-credit R-019 matrix terminal."""

    token_id: str
    accepted: bool
    reason: str
    guarded_report_id: str = ""
    primitive_report_id: str = ""


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
_PROOF_DEPTH_RANKS = {
    "startup": 0,
    "interaction": 1,
    "terminal": 2,
    "persistence": 3,
    "replay": 4,
}


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
    # The history is append-only, but callers only receive the current row per
    # route.  Collapse it before reading resolution/binding history: resolving
    # every superseded row repeatedly turns an unchanged registry reconcile
    # into a full-history CPU scan without changing the current projection.
    latest_rows: Dict[str, sqlite3.Row] = {}
    for row in rows:
        details = _json_object(str(row["details_json"]), "route evidence details")
        value = _json_object(str(row["value_json"]), "route evidence value")
        route_key = str(details.get("route_key") or value.get("route_key") or "")
        if not route_key:
            raise ScenarioRegistryStoreError("Registry route evidence is missing route_key")
        latest_rows[route_key] = row
    latest: Dict[str, Mapping[str, Any]] = {}
    for route_key, row in latest_rows.items():
        details = _json_object(str(row["details_json"]), "route evidence details")
        value = _json_object(str(row["value_json"]), "route evidence value")
        bindings = []
        # A declaration-bound first run deliberately has no verification
        # binding.  Keep that persisted authority exact when later reports on
        # the same route are reconciled; otherwise selection rereads evidence
        # that it never issued.
        resolutions = () if str(row["evidence_state"]) == "first_run" else connection.execute(
            "SELECT verification.verification_id, verification.report_id, verification.details_json AS verification_details_json, "
            "resolution.resolution_kind, resolution.binding_fingerprint, resolution.details_json FROM verification_history AS verification "
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
            # Binding ownership is indexed by the immutable report identity.
            # Do not scan historical manifest payloads here: apart from making
            # reconciliation quadratic, that would decode legacy rows whose
            # ambiguous ownership was deliberately rejected by migration v11.
            for binding_row in connection.execute(
                "SELECT payload_json FROM binding_history WHERE report_id = ? AND binding_kind = 'manifest' "
                "ORDER BY binding_event_id DESC LIMIT 1",
                (str(resolution["report_id"] or ""),),
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
            verification_details = _json_object(
                str(resolution["verification_details_json"]), "verification details"
            )
            repair_successor = verification_details.get("r019_repair_successor")
            if not manifest_binding_current and isinstance(repair_successor, Mapping):
                current_manifest = repair_successor.get("current_manifest")
                manifest_binding_current = (
                    bool(manifest["present"])
                    and isinstance(current_manifest, Mapping)
                    and current_manifest.get("source_sha256") == current_manifest_sha256
                )
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


def _current_bootstrap_revalidation(
    connection: sqlite3.Connection,
    *,
    manifest_id: str,
    route_evidence: Sequence[Mapping[str, Any]],
) -> Optional[Mapping[str, Any]]:
    """Return the current release only when it still covers every stale route."""
    if not route_evidence or any(
            not isinstance(route, Mapping)
            or (
                route.get("evidence_state") not in {"stale", "unknown"}
                and not (
                    route.get("evidence_state") == "contradicted"
                    and bool(route.get("bindings"))
                    and all(binding.get("resolution") == "stale" for binding in route["bindings"])
                )
            )
            for route in route_evidence):
        return None
    manifest = connection.execute(
        "SELECT present, current_sha256 FROM manifest_current WHERE manifest_id = ?",
        (manifest_id,),
    ).fetchone()
    if manifest is None or not bool(manifest["present"]):
        return None
    expected_sha256 = str(manifest["current_sha256"] or "")
    release: Optional[Mapping[str, Any]] = None
    release_event_ids = []
    for route in route_evidence:
        row = connection.execute(
            "SELECT quarantine_event_id, quarantine_kind, details_json FROM quarantine_history "
            "WHERE manifest_id = ? AND route_key = ? ORDER BY quarantine_event_id DESC LIMIT 1",
            (manifest_id, str(route["route_key"])),
        ).fetchone()
        if row is None or str(row["quarantine_kind"]) != "released_current_bootstrap_authority":
            return None
        details = _json_object(str(row["details_json"]), "bootstrap revalidation details")
        if details.get("manifest_sha256") != expected_sha256:
            return None
        if release is None:
            release = details
        elif _json_text(release) != _json_text(details):
            return None
        release_event_ids.append(int(row["quarantine_event_id"]))
    if release is None:
        return None
    current_release = dict(release)
    current_release["release_event_ids"] = release_event_ids
    return current_release


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
        return "absent", "source_absent"
    states = {str(item["evidence_state"]) for item in route_evidence}
    if "contradicted" in states:
        if _current_bootstrap_revalidation(
                connection, manifest_id=manifest_id, route_evidence=route_evidence) is not None:
            return "active", "current_bootstrap_authority"
        return "quarantined", "route_contradicted"
    if "stale" in states:
        if _current_bootstrap_revalidation(
                connection, manifest_id=manifest_id, route_evidence=route_evidence) is not None:
            return "active", "current_bootstrap_authority"
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


def _fact_evidence_from_current_authority(
    connection: sqlite3.Connection,
    *,
    manifest_id: str,
    capability_key: str,
    declared_state: str,
    route_evidence: Sequence[Mapping[str, Any]],
) -> Tuple[str, Optional[str]]:
    route_states = {str(item["evidence_state"]) for item in route_evidence}
    if "contradicted" in route_states:
        return "contradicted", None
    if "stale" in route_states:
        return "stale", None
    rows = connection.execute(
        "SELECT evidence_state, verification_id, details_json FROM capability_evidence_history WHERE manifest_id = ? "
        "AND capability_key = ? AND evidence_kind != 'declaration' ORDER BY capability_evidence_id",
        (manifest_id, capability_key),
    ).fetchall()
    contradictions = {
        str(row["verification_id"])
        for row in rows
        if row["verification_id"] is not None and _public_evidence_state(str(row["evidence_state"])) == "contradicted"
    }
    superseded = {
        str(row["supersedes_verification_id"])
        for row in connection.execute(
            "SELECT verification_id, supersedes_verification_id FROM verification_history "
            "WHERE manifest_id = ? AND supersedes_verification_id IS NOT NULL",
            (manifest_id,),
        )
        if str(row["verification_id"]) in {
            str(evidence["verification_id"])
            for evidence in rows
            if evidence["verification_id"] is not None
            and _public_evidence_state(str(evidence["evidence_state"])) == "run-verified"
        }
    }
    if contradictions - superseded:
        return "contradicted", None
    if any(_public_evidence_state(str(row["evidence_state"])) == "stale" for row in rows):
        return "stale", None
    candidates: List[Tuple[str, str]] = []
    for row in rows:
        state = _public_evidence_state(str(row["evidence_state"]))
        if state not in {"inspected", "run-verified"}:
            continue
        details = _json_object(str(row["details_json"]), "capability evidence details")
        depth = details.get("proof_depth")
        if isinstance(depth, str) and depth in _KNOWN_PROOF_DEPTHS:
            candidates.append((state, depth))
    run_verified = [entry for entry in candidates if entry[0] == "run-verified"]
    if run_verified:
        return "run-verified", max(run_verified, key=lambda entry: _PROOF_DEPTH_RANKS[entry[1]])[1]
    inspected = [entry for entry in candidates if entry[0] == "inspected"]
    if inspected:
        return "inspected", max(inspected, key=lambda entry: _PROOF_DEPTH_RANKS[entry[1]])[1]
    return _public_evidence_state(declared_state), None


def _current_source_binding_validation_state(
    connection: sqlite3.Connection,
    *,
    manifest_id: str,
    capability_key: str,
    manifest_sha256: str,
) -> Optional[str]:
    """Return only a source-fact check bound to this exact manifest revision."""
    rows = connection.execute(
        "SELECT evidence_state, details_json FROM capability_evidence_history WHERE manifest_id = ? "
        "AND capability_key = ? AND evidence_kind = 'source_binding_validation' "
        "ORDER BY capability_evidence_id DESC",
        (manifest_id, capability_key),
    ).fetchall()
    for row in rows:
        details = _json_object(str(row["details_json"]), "source-binding evidence details")
        if details.get("manifest_sha256") == manifest_sha256:
            return _public_evidence_state(str(row["evidence_state"]))
    return None


def _exclusive_source_review_state(
    connection: sqlite3.Connection,
    *,
    manifest_id: str,
    source_path: str,
    revision: int,
    source_sha256: str,
    declaration: Mapping[str, Any],
) -> Mapping[str, Any]:
    """Return the exact-review state for a manifest that opts into the firewall."""
    validation = declaration.get("source_binding_validation")
    if not isinstance(validation, Mapping) or validation.get("exclusive_review_required") is not True:
        return {"review_required": False, "review_status": "not_required", "executable": True}
    exact = connection.execute(
        "SELECT decision FROM source_bound_review_history WHERE manifest_id = ? AND source_path = ? "
        "AND manifest_revision = ? AND manifest_sha256 = ? ORDER BY review_event_id DESC LIMIT 1",
        (manifest_id, source_path, revision, source_sha256),
    ).fetchone()
    if exact is not None:
        decision = str(exact["decision"])
        return {
            "review_required": True,
            "review_status": "accepted" if decision == "accepted" else "rejected",
            "executable": decision == "accepted",
        }
    prior = connection.execute(
        "SELECT source_path, manifest_revision, manifest_sha256 FROM source_bound_review_history "
        "WHERE manifest_id = ? ORDER BY review_event_id DESC LIMIT 1",
        (manifest_id,),
    ).fetchone()
    if prior is None:
        status = "pending"
    elif str(prior["source_path"]) != source_path:
        status = "wrong_source"
    elif int(prior["manifest_revision"]) != revision:
        status = "changed_revision"
    else:
        status = "stale"
    return {"review_required": True, "review_status": status, "executable": False}


def record_source_bound_review_decision(
    connection: sqlite3.Connection,
    *,
    manifest_id: str,
    source_path: str,
    manifest_revision: int,
    manifest_sha256: str,
    decision: str,
    reviewer_identity: str,
) -> Mapping[str, Any]:
    """Append an external review decision for one exact source-bound revision.

    This deliberately accepts no inferred authority: callers must present the
    projected source identity and independently supplied reviewer identity.
    """
    if decision not in {"accepted", "rejected"}:
        raise ScenarioRegistryStoreError("source-bound review decision must be accepted or rejected")
    reviewer = str(reviewer_identity).strip()
    if not reviewer:
        raise ScenarioRegistryStoreError("source-bound review requires reviewer identity")
    row = connection.execute(
        "SELECT source_path, present, revision, current_sha256, declaration_json FROM manifest_current WHERE manifest_id = ?",
        (manifest_id,),
    ).fetchone()
    if row is None or not bool(row["present"]):
        raise ScenarioRegistryStoreError("source-bound review manifest is not current")
    if (str(row["source_path"]) != source_path or int(row["revision"]) != manifest_revision or
            str(row["current_sha256"] or "") != manifest_sha256):
        raise ScenarioRegistryStoreError("source-bound review identity is not current")
    declaration = _json_object(str(row["declaration_json"]), "review manifest declaration")
    state = _exclusive_source_review_state(
        connection, manifest_id=manifest_id, source_path=source_path,
        revision=manifest_revision, source_sha256=manifest_sha256, declaration=declaration,
    )
    if not state["review_required"]:
        raise ScenarioRegistryStoreError("source-bound review is not required for this manifest")
    with immediate_transaction(connection):
        connection.execute(
            "INSERT OR IGNORE INTO source_bound_review_history( manifest_id, source_path, manifest_revision, "
            "manifest_sha256, decision, reviewer_identity ) VALUES( ?, ?, ?, ?, ?, ? )",
            (manifest_id, source_path, manifest_revision, manifest_sha256, decision, reviewer),
        )
    return {"manifest_id": manifest_id, "review_status": decision, "review_required": True,
            "source_path": source_path, "manifest_revision": manifest_revision,
            "manifest_sha256": manifest_sha256}


def build_registry_query_candidate_snapshot(
    connection: sqlite3.Connection,
    *,
    include_lifecycle_states: Sequence[str] = (),
    manifest_ids: Sequence[str] = (),
) -> Tuple[RegistryQueryCandidateSnapshot, ...]:
    """Read current projection/history into fixed, explained, non-executing candidates."""
    requested_states = set(include_lifecycle_states)
    if not requested_states <= {"absent", "quarantined", "retired"}:
        raise ScenarioRegistryQueryError(
            "include_lifecycle_states may contain only absent, quarantined, or retired"
        )
    allowed_states = {"active"} | requested_states
    snapshots: List[RegistryQueryCandidateSnapshot] = []
    requested_manifest_ids = tuple(sorted({str(item).strip() for item in manifest_ids if str(item).strip()}))
    manifest_filter = ""
    parameters: Tuple[str, ...] = ()
    if requested_manifest_ids:
        manifest_filter = " WHERE manifest_id IN (" + ", ".join("?" for _ in requested_manifest_ids) + ")"
        parameters = requested_manifest_ids
    manifests = connection.execute(
        "SELECT manifest_id, source_path, present, revision, current_sha256, validation_json, declaration_json "
        "FROM manifest_current" + manifest_filter + " ORDER BY manifest_id",
        parameters,
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
        review = _exclusive_source_review_state(
            connection,
            manifest_id=manifest_id,
            source_path=str(manifest["source_path"]),
            revision=int(manifest["revision"]),
            source_sha256=str(manifest["current_sha256"] or ""),
            declaration=declaration,
        )
        certification_retry = (
            declaration.get("name") in {
                "bandit.r005_continuous_hostile_ecology_certification",
                "bandit.r005_natural_route_qualification",
                "bandit.r005_native_waypoint_observation",
                "bandit.r005_native_wait_qualification",
                "r018.raw_wait_acceptance_mcw",
                "r019.keep_watch_meaningful_event_bootstrap_mcw",
                "r019.keep_watch_acceptance_mcw",
                "r019.keep_watch_off_interruption_closure059_validation_mcw",
                "r023.guarded_relative_validation_mcw",
                "cannibal.r029_natural_route_roof_mcw",
            }
            and route_evidence
            and (
                all(str(route.get("evidence_state", "")) == "stale" for route in route_evidence)
                or lifecycle_reason in {"route_contradicted", "route_stale", "quarantine_history"}
            )
        )
        if certification_retry:
            # Preserve stale startup-only history, but keep the current valid
            # declaration selectable for a corrected certification attempt.
            lifecycle_state, lifecycle_reason = "active", "current_manifest_certification_retry"
        if lifecycle_state not in allowed_states:
            continue
        facts: Dict[str, Mapping[str, Any]] = {}
        fact_explanations: Dict[str, Mapping[str, Any]] = {}
        current_bootstrap_authority = _current_bootstrap_revalidation(
            connection, manifest_id=manifest_id, route_evidence=route_evidence,
        )
        capabilities = connection.execute(
            "SELECT capability_key, value_json, declared_state, review_required "
            "FROM manifest_capability_current WHERE manifest_id = ? ORDER BY capability_key",
            (manifest_id,),
        ).fetchall()
        for capability in capabilities:
            key = str(capability["capability_key"])
            value = json.loads(str(capability["value_json"]))
            source_validation = declaration.get("source_binding_validation")
            validation_keys = source_validation.get("capabilities", ()) if isinstance(source_validation, Mapping) else ()
            validation_required = key in validation_keys
            validation_state = _current_source_binding_validation_state(
                connection,
                manifest_id=manifest_id,
                capability_key=key,
                manifest_sha256=str(manifest["current_sha256"] or ""),
            ) if validation_required else None
            if validation_required:
                evidence_state, proof_depth = (
                    ("inspected", "persistence") if validation_state == "inspected"
                    else (validation_state or "unknown", None)
                )
            elif current_bootstrap_authority is not None:
                evidence_state, proof_depth = (
                    _public_evidence_state(str(capability["declared_state"])), None,
                )
            else:
                evidence_state, proof_depth = _fact_evidence_from_current_authority(
                    connection,
                    manifest_id=manifest_id,
                    capability_key=key,
                    declared_state=str(capability["declared_state"]),
                    route_evidence=() if certification_retry else route_evidence,
                )
                if certification_retry and key in {
                        "capabilities.bandit.r005",
                        "capabilities.bandit.r005.prerequisites",
                }:
                    evidence_state, proof_depth = "declared", None
            facts[key] = {
                "present": True,
                "value": value,
                "evidence_state": evidence_state,
                "proof_depth": proof_depth,
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
            token_eligible=lifecycle_state == "active" and bool(review["executable"]),
            explanation={
                "manifest": {
                    "manifest_id": manifest_id,
                    "name": declaration.get("name"),
                    "source_path": str(manifest["source_path"]),
                    "present": bool(manifest["present"]),
                    "revision": int(manifest["revision"]),
                    "sha256": manifest["current_sha256"],
                    "validation": dict(validation),
                    "review_status": str(review["review_status"]),
                    "review_required": bool(review["review_required"]),
                    "executable": bool(review["executable"]),
                    "known_footing": known_footing,
                    "source_binding_validation": declaration.get("source_binding_validation"),
                },
                "lifecycle": {"state": lifecycle_state, "reason": lifecycle_reason},
                "route_evidence": route_evidence,
                "bootstrap_authority": current_bootstrap_authority,
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


def _authoritative_current_route(route: Mapping[str, Any]) -> Optional[Mapping[str, Any]]:
    """Project one current hard-proven route without erasing stale audit history."""
    if route.get("evidence_state") != "run-verified":
        return None
    details = route.get("details", {})
    bindings = route.get("bindings", ())
    if not isinstance(details, Mapping) or not isinstance(bindings, Sequence):
        return None
    hard_proven_ids = details.get("hard_proven_verification_ids")
    if not isinstance(hard_proven_ids, Sequence) or isinstance(hard_proven_ids, (str, bytes)):
        return None
    authorized_ids = {str(verification_id) for verification_id in hard_proven_ids}
    current_bindings = tuple(
        binding for binding in bindings
        if isinstance(binding, Mapping)
        and binding.get("resolution") == "compatible"
        and str(binding.get("verification_id", "")) in authorized_ids
    )
    if not current_bindings:
        return None
    return {**dict(route), "bindings": current_bindings}


def _current_verified_route(snapshot: RegistryQueryCandidateSnapshot) -> Optional[Mapping[str, Any]]:
    routes = snapshot.explanation.get("route_evidence", ())
    if not isinstance(routes, Sequence):
        return None
    for route in routes:
        if isinstance(route, Mapping):
            current_route = _authoritative_current_route(route)
            if current_route is not None:
                return current_route
    return None


def _first_run_certification_route(
    snapshot: RegistryQueryCandidateSnapshot,
) -> Optional[Mapping[str, Any]]:
    """Return a declaration-bound zero-evidence route for named first runs.

    The first certification cannot have a verification binding yet.  Keep this
    escape hatch declaration-bound and fail closed for every other manifest.
    """
    manifest = snapshot.explanation.get("manifest", {})
    facts = snapshot.facts
    if snapshot.lifecycle_state != "active" or not snapshot.token_eligible or not isinstance(manifest, Mapping) or \
            manifest.get("validation", {}).get("status") != "valid" or manifest.get("validation", {}).get("review_required"):
        return None
    name = manifest.get("name")
    if name == "bandit.r005_continuous_hostile_ecology_certification":
        valid = (
            facts.get("capabilities.bandit.r005", {}).get("value") == "continuous_hostile_ecology_lifecycle"
            and facts.get("capabilities.bandit.r005.prerequisites", {}).get("value") == ["R-001", "R-002", "R-004"]
        )
    elif name == "bandit.r005_natural_route_qualification":
        valid = facts.get("capabilities.bandit.r005.route_qualification", {}).get("value") == \
                "observed_x144_y29_source_bound_zero_credit_destination_arrival"
    elif name == "bandit.r005_current_world_corridor_observation":
        valid = facts.get("capabilities.bandit.r005.current_world_corridor_observation", {}).get("value") == \
                "source_derived_zero_credit_current_world_corridor"
    elif name == "bandit.r005_safe_wait_observation":
        valid = facts.get("capabilities.bandit.r005.safe_wait_observation", {}).get("value") == \
                "zero_credit_preserved_native_safe_mode_off_route_and_wait"
    elif name == "bandit.r005_native_wait_qualification":
        valid = facts.get("capabilities.bandit.r005.native_wait_qualification", {}).get("value") == \
                "source_bound_native_wait_only_structural_outing_lifecycle"
    elif name == "r013.cockpit_transaction_bootstrap_mcw":
        valid = facts.get("capabilities.r013.cockpit_transaction_bootstrap", {}).get("value") == \
                "public_native_wait_and_advertised_activity_ignore_recovery"
    elif name == "r013.cockpit_long_wait_activity_interrupt_bootstrap_mcw":
        valid = facts.get("capabilities.r013.cockpit_long_wait_activity_interruption", {}).get("value") == \
                "source_bound_six_hour_wait_with_current_native_activity_interruption"
    elif name == "r013.clean_wait_duration_bootstrap_mcw":
        valid = facts.get("capabilities.r013.clean_wait_duration_footing", {}).get("value") == \
                "source_bound_no_active_hostile_wait_footing"
    elif name == "r013.clean_wait_duration_validation_mcw":
        valid = facts.get("capabilities.r013.clean_wait_duration_validation", {}).get("value") == \
                "independent_bound_one_minute_wait_without_activity_recovery"
    elif name == "r018.raw_wait_acceptance_mcw":
        valid = (
            facts.get("capabilities.r018.raw_wait_acceptance", {}).get("value") ==
            "source_bound_raw_bounded_wait_and_primitive_comparison"
            and facts.get("runtime.r018.source_binding", {}).get("value") ==
            "r013_clean_wait_duration_v1:r009-m095"
        )
    elif name == "r023.raw_relative_validation_mcw":
        valid = (
            facts.get("capabilities.r023.raw_relative_validation", {}).get("value") ==
            "independent_zero_credit_raw_signed_relative_movement"
            and facts.get("runtime.r023.source_binding", {}).get("value") ==
            "r023_cardinal_movement_bootstrap_v1:r009-m091"
        )
    elif name == "r023.guarded_relative_validation_mcw":
        valid = (
            facts.get("capabilities.r023.guarded_relative_validation", {}).get("value") ==
            "independent_zero_credit_guarded_signed_relative_movement"
            and facts.get("runtime.r023.source_binding", {}).get("value") ==
            "r023_cardinal_movement_bootstrap_v1:r009-m091"
        )
    elif name == "r019.keep_watch_safety_bootstrap_mcw":
        valid = (
            facts.get("capabilities.r019.keep_watch_safety_bootstrap", {}).get("value") ==
            "source_bound_native_keep_watch_safety_frame"
            and facts.get("runtime.r019.source_binding", {}).get("value") ==
            "r013_clean_wait_duration_v1:r009-m095"
        )
    elif name == "r019.keep_watch_meaningful_event_bootstrap_mcw":
        valid = (
            facts.get("capabilities.r019.keep_watch_meaningful_event_bootstrap", {}).get("value") ==
            "deterministic_hostile_sighting_setup_for_guarded_wait"
            and facts.get("runtime.r019.source_binding", {}).get("value") ==
            "r019_keep_watch_meaningful_event_bootstrap_v1:r009-m095"
        )
    elif name == "r019.keep_watch_off_interruption_closure057_bootstrap_mcw":
        valid = (
            facts.get("capabilities.r019.off_interruption_bootstrap", {}).get("value") ==
            "native_debug_spawned_hostile_exposes_current_activity_interruption"
            and facts.get("runtime.r019.source_binding", {}).get("value") ==
            "r013_clean_wait_duration_v1:r009-m095"
        )
    elif name == "r019.keep_watch_meaningful_event_validation_mcw":
        valid = (
            facts.get("capabilities.r019.keep_watch_meaningful_event_validation", {}).get("value") ==
            "separately_authorized_guarded_stop_at_declared_hostile_sighting"
            and facts.get("runtime.r019.source_binding", {}).get("value") ==
            "r019_keep_watch_meaningful_event_bootstrap_v1:r009-m095"
        )
    elif name == "r019.keep_watch_acceptance_mcw":
        valid = (
            facts.get("capabilities.r019.keep_watch_acceptance", {}).get("value") ==
            "source_bound_guarded_keep_watch_and_primitive_wait_comparison"
            and facts.get("runtime.r019.source_binding", {}).get("value") ==
            "r019_keep_watch_safe_popup_v1:r009-m095"
        )
    elif name == "r019.keep_watch_off_binding_drift_current_source_control_mcw":
        valid = (
            facts.get("capabilities.r019.off_binding_drift_current_source_control", {}).get("value") ==
            "primitive_continuation_fail_closed_on_runtime_binding_drift"
            and facts.get("runtime.r019.source_binding", {}).get("value") ==
            "r013_clean_wait_duration_v1:r009-m095"
        )
    elif name == "r019.keep_watch_off_interruption_closure059_validation_mcw":
        valid = (
            facts.get("capabilities.r019.off_interruption_closure059_validation", {}).get("value") ==
            "fresh_disabled_master_primitive_wait_stops_at_current_native_hostile_interruption_with_source_bound_partial_progress"
            and facts.get("runtime.r019.source_binding", {}).get("value") ==
            "r019_keep_watch_off_positive_progress_v1:r009-m095"
        )
    elif name == "r019.keep_watch_tagged_lifecycle_diagnostic_mcw":
        valid = (
            facts.get("capabilities.r019.tagged_lifecycle_diagnostic", {}).get("value") ==
            "one_primitive_diagnostic_minute_preserves_the_exact_tagged_hostile_lifecycle_stream_without_feature_credit"
            and facts.get("runtime.r019.source_binding", {}).get("value") ==
            "r019_keep_watch_off_positive_progress_v1:r009-m095"
        )
    elif name == "r014.cockpit_live_prototype_bootstrap_mcw":
        valid = (
            facts.get("capabilities.r014.live_prototype_bootstrap", {}).get("value") ==
            "source_bound_zero_credit_cockpit_live_route"
            and facts.get("runtime.r014.source_binding", {}).get("value") ==
            "r013_wait_activity_interrupt_bootstrap_v1:r009-m095"
        )
    elif name == "r011.zombie_dog_setup_validation_mcw":
        valid = (
            facts.get("actors.r011.required_zombie_dog", {}).get("value") == "mon_zombie_dog"
            and facts.get("actors.r011.incidental_dog", {}).get("value") == "absent"
            and facts.get("capabilities.r011.setup_only", {}).get("value") is True
        )
    elif name == "r020.controlled_camp_setup_bootstrap":
        valid = (
            facts.get("capabilities.setup.controlled_camp_transaction", {}).get("value") ==
            "fixture_save_transform.player_basecamp_at_omt"
            and facts.get("runtime.evidence_ceiling", {}).get("value") ==
            "none_for_manufactured_state"
        )
    elif name == "r021.direct_hp_setter_bootstrap":
        valid = (
            facts.get("capabilities.setup.direct_hp_setter", {}).get("value") ==
            "debug_menu.monster_set_hp->monster::set_hp"
            and facts.get("runtime.evidence_ceiling", {}).get("value") ==
            "none_for_debug_fixture_transaction"
        )
    elif name == "r022.item_spawn_bootstrap":
        valid = (
            facts.get("capabilities.setup.item_spawn_transaction", {}).get("value") ==
            "debug_menu::debug_item_spawn_transaction"
            and facts.get("capabilities.setup.item_spawn_cleanup", {}).get("value") ==
            "debug_menu::debug_item_spawn_transaction_cleanup"
            and facts.get("capabilities.setup.item_spawn_source", {}).get("value") == "src/wish.cpp"
            and facts.get("runtime.evidence_ceiling", {}).get("value") ==
            "none_for_debug_fixture_transaction"
        )
    elif name == "r022.item_spawn_live_validation":
        valid = (
            facts.get("capabilities.r022.live_validation", {}).get("value") ==
            "run_bound_native_debug_item_spawn_transaction_with_tag_scoped_cleanup"
            and facts.get("capabilities.setup.item_spawn_transaction", {}).get("value") ==
            "debug_menu::debug_item_spawn_transaction"
            and facts.get("capabilities.setup.item_spawn_cleanup", {}).get("value") ==
            "debug_menu::debug_item_spawn_transaction_cleanup"
            and facts.get("runtime.evidence_ceiling", {}).get("value") ==
            "none_for_debug_fixture_transaction"
        )
    elif name == "bandit.r008_natural_return_from_bound_local_pair_mcw":
        valid = (
            facts.get("capabilities.bandit.natural_structural_scout_return_from_bound_local_pair", {}).get("value") ==
            "native_wait_only"
            and facts.get("runtime.r008.source_binding", {}).get("value") ==
            "r008_natural_safe_watch_local_pair_bootstrap_v1:r008-closure-059-validation"
        )
    elif name == "bandit.r008_closure_228_local_pair_bootstrap_mcw":
        valid = (
            facts.get("runtime.r008.closure_228_bootstrap", {}).get("value") ==
            "zero_credit_native_local_pair"
            and facts.get("world.r008.closure_228_local_pair", {}).get("value") == {
                "actor_ids": [4, 5], "generation": 1, "handoff_epoch": 1,
                "simulation_owner": "local",
            }
        )
    elif name == "bandit.r008_closure_229_source_route_local_pair_bootstrap_mcw":
        valid = (
            facts.get("runtime.r008.closure_229_bootstrap", {}).get("value") ==
            "zero_credit_source_route_native_local_pair"
            and facts.get("world.r008.closure_229_local_pair", {}).get("value") == {
                "actor_ids": [4, 5], "generation": 1, "handoff_epoch": 1,
                "simulation_owner": "local",
            }
        )
    else:
        valid = False
    if not valid:
        return None
    route_key = _identity(
        "caol-scenario-proof-route-v2",
        str(manifest.get("source_path", "")),
        str(manifest.get("name", "")),
    )
    return {
        "route_key": route_key,
        "evidence_state": "unknown",
        "internal_resolution_state": "first_run",
        "details": {"first_run": True, "manifest_sha256": str(manifest.get("sha256", ""))},
        "bindings": (),
    }


def _current_stale_bootstrap_candidate(snapshot: RegistryQueryCandidateSnapshot) -> bool:
    """Allow one bootstrap run for a current source-bound manifest with stale reports only."""
    lifecycle = snapshot.explanation.get("lifecycle", {})
    routes = snapshot.explanation.get("route_evidence", ())
    if not isinstance(lifecycle, Mapping) or not _current_valid_bootstrap_manifest(snapshot):
        return False
    if not isinstance(routes, Sequence) or not routes:
        return False
    if snapshot.lifecycle_state != "quarantined" or lifecycle.get("reason") not in {
            "route_stale", "route_contradicted", "quarantine_history"}:
        return False
    if all(
            isinstance(route, Mapping) and route.get("evidence_state") in {"stale", "unknown"}
            for route in routes):
        return True
    # A source-binding validator can independently establish the current
    # fixture footing.  If every historical route binding is stale, its old
    # red verdict is not a current contradiction.  Retain that verdict for
    # repair history, but allow the explicit bootstrap owner to observe the
    # unchanged current sources before a new run is authorized.
    validation = snapshot.explanation.get("manifest", {}).get("source_binding_validation")
    if not isinstance(validation, Mapping):
        return False
    keys = validation.get("capabilities")
    if not isinstance(keys, list) or not keys or any(
            snapshot.facts.get(str(key), {}).get("evidence_state") != "inspected" for key in keys):
        return False
    return all(
        isinstance(route, Mapping)
        and route.get("evidence_state") in {"stale", "unknown", "contradicted"}
        and bool(route.get("bindings"))
        and all(binding.get("resolution") == "stale" for binding in route["bindings"])
        for route in routes
    )


def _current_valid_bootstrap_manifest(snapshot: RegistryQueryCandidateSnapshot) -> bool:
    """Keep bootstrap authority bound to a present strict manifest, never a draft."""
    manifest = snapshot.explanation.get("manifest", {})
    if not isinstance(manifest, Mapping):
        return False
    validation = manifest.get("validation", {})
    return (
        bool(manifest.get("present")) and isinstance(validation, Mapping) and
        validation.get("status") == "valid" and not bool(validation.get("review_required"))
    )


def _select_registry_bootstrap_candidate(
    connection: sqlite3.Connection,
    request: RegistryQueryRequest,
) -> Optional[RegistryQueryCandidateSnapshot]:
    """Select normal authority first, then one current manifest quarantined only by stale evidence."""
    ordinary = evaluate_registry_query_from_store(connection, request)
    selected_id = ordinary.evaluation.ranked_scenario_ids[0] if ordinary.evaluation.ranked_scenario_ids else None
    selected = next((candidate for candidate in ordinary.candidates if candidate.scenario_id == selected_id), None)
    if selected is not None and selected.token_eligible and _current_valid_bootstrap_manifest(selected):
        return selected

    stale_candidates = tuple(
        candidate
        for candidate in build_registry_query_candidate_snapshot(
            connection, include_lifecycle_states=("quarantined",)
        )
        if _current_stale_bootstrap_candidate(candidate)
    )
    stale_evaluation = evaluate_registry_query(
        request,
        tuple({
            "scenario_id": candidate.scenario_id,
            "facts": {
                key: {
                    **dict(fact),
                    "evidence_state": "declared" if fact.get("evidence_state") in {"stale", "contradicted"} else fact.get(
                        "evidence_state"),
                    "proof_depth": None if fact.get("evidence_state") in {"stale", "contradicted"} else fact.get("proof_depth"),
                }
                for key, fact in candidate.facts.items()
            },
        } for candidate in stale_candidates),
    )
    stale_id = stale_evaluation.ranked_scenario_ids[0] if stale_evaluation.ranked_scenario_ids else None
    return next((candidate for candidate in stale_candidates if candidate.scenario_id == stale_id), None)


def revalidate_current_bootstrap_authority(
    connection: sqlite3.Connection,
    request: RegistryQueryRequest,
    *,
    current_facts: Callable[[Mapping[str, Any]], Mapping[str, Any]],
) -> Mapping[str, Any]:
    """Release only a strict stale manifest after its current launch footing is observed.

    This is not route proof: it appends a SHA-bound authority release for exactly
    one first canonical run while keeping stale report bindings immutable.
    """
    selected = _select_registry_bootstrap_candidate(connection, request)
    if selected is None or not _current_stale_bootstrap_candidate(selected):
        return {"accepted": False, "reason": "no_current_stale_bootstrap_candidate"}
    manifest = selected.explanation["manifest"]
    manifest_id = str(manifest["manifest_id"])
    declaration_row = connection.execute(
        "SELECT declaration_json, current_sha256 FROM manifest_current WHERE manifest_id = ?",
        (manifest_id,),
    ).fetchone()
    if declaration_row is None:
        return {"accepted": False, "reason": "manifest_missing"}
    declaration = _json_object(str(declaration_row["declaration_json"]), "bootstrap manifest declaration")
    observed = current_facts(declaration)
    if not isinstance(observed, Mapping):
        raise ScenarioRegistryStoreError("bootstrap revalidation facts must be an object")
    runtime = _bootstrap_runtime_binding(observed.get("runtime"))
    footing: Dict[str, Mapping[str, Any]] = {"runtime": runtime}
    for kind, name_key, profile_key in (
            ("fixture", "fixture", "fixture_profile"),
            ("profile", "profile_snapshot", "profile_snapshot_profile")):
        item = observed.get(kind)
        if not isinstance(item, Mapping) or item.get("status", "compatible") != "compatible":
            return {"accepted": False, "reason": f"{kind}_not_current"}
        expected_name = str(declaration.get(name_key, "")).strip()
        expected_profile = str(declaration.get(profile_key, "")).strip()
        if str(item.get("name", "")).strip() != expected_name or str(item.get("profile", "")).strip() != expected_profile:
            return {"accepted": False, "reason": f"{kind}_identity_changed"}
        source_sha256 = str(item.get("source_sha256", "")).strip().lower()
        if len(source_sha256) != 64 or any(char not in "0123456789abcdef" for char in source_sha256):
            return {"accepted": False, "reason": f"{kind}_sha256_invalid"}
        footing[kind] = {
            "name": expected_name,
            "profile": expected_profile,
            "source_path": str(item.get("source_path", "")).strip(),
            "source_sha256": source_sha256,
        }
    routes = selected.explanation.get("route_evidence", ())
    if not isinstance(routes, Sequence) or not routes:
        return {"accepted": False, "reason": "route_evidence_missing"}
    details = {
        "authority_kind": "current_bootstrap_revalidation",
        "manifest_sha256": str(declaration_row["current_sha256"]),
        "query_sha256": hashlib.sha256(_query_request_json(request).encode("utf-8")).hexdigest(),
        "current_facts": footing,
    }
    with immediate_transaction(connection):
        for route in routes:
            if not isinstance(route, Mapping) or (
                    route.get("evidence_state") not in {"stale", "unknown"}
                    and not (
                        route.get("evidence_state") == "contradicted"
                        and bool(route.get("bindings"))
                        and all(binding.get("resolution") == "stale" for binding in route["bindings"])
                    )
            ):
                return {"accepted": False, "reason": "route_not_revalidatable"}
            _append_quarantine_if_changed(
                connection,
                manifest_id=manifest_id,
                route_key=str(route["route_key"]),
                quarantine_kind="released_current_bootstrap_authority",
                details=details,
            )
        _append_lifecycle_if_changed(
            connection,
            manifest_id=manifest_id,
            event_kind="current_bootstrap_authority_revalidated",
            details=details,
        )
    return {"accepted": True, "manifest_id": manifest_id, "details": details}


def _query_result_json(result: RegistryQueryPredicateResult) -> Dict[str, Any]:
    return {
        "key": result.key,
        "expected": result.expected,
        "observed": result.observed,
        "evidence_state": result.evidence_state,
        "reason": result.reason,
    }


def _closest_draft_candidate(
    evaluation: RegistryStoredQueryEvaluation,
) -> Optional[RegistryQueryCandidateResult]:
    """Select useful build footing without pretending that it is executable."""
    useful_reasons = {
        "below_minimum_evidence",
        "contradicted",
        "containment_mismatch",
        "equality_mismatch",
        "fact_absent",
        "fact_present",
        "non_container_value",
        "non_numeric_value",
        "outside_range",
        "stale",
    }

    def score(candidate: RegistryQueryCandidateResult) -> Tuple[int, int, str]:
        passed = sum(result.passed for result in candidate.hard_results)
        known = sum(
            result.passed or result.reason in useful_reasons
            for result in candidate.hard_results
        )
        return -passed, -known, candidate.scenario_id

    useful = [
        candidate for candidate in evaluation.evaluation.candidates
        if any(result.passed or result.reason in useful_reasons for result in candidate.hard_results)
    ]
    if not useful:
        return None
    return min(useful, key=score)


def _known_draft_footing(candidate: Optional[RegistryQueryCandidateSnapshot]) -> Dict[str, Any]:
    if candidate is None:
        return {}
    manifest = candidate.explanation.get("manifest", {})
    footing = manifest.get("known_footing") if isinstance(manifest, Mapping) else None
    if not isinstance(footing, Mapping):
        return {}
    return {
        field: value for field, value in footing.items()
        if value not in (None, "", [], {})
    }


def _write_inert_draft(
    *,
    request_json: str,
    query_sha256: str,
    evaluation: RegistryStoredQueryEvaluation,
    drafts_root: Path,
) -> str:
    drafts_root.mkdir(parents=True, exist_ok=True)
    path = drafts_root / f"{query_sha256}.json"
    closest_result = _closest_draft_candidate(evaluation)
    closest_snapshot = next((
        candidate for candidate in evaluation.candidates
        if closest_result is not None and candidate.scenario_id == closest_result.scenario_id
    ), None)
    satisfied = [] if closest_result is None else [
        _query_result_json(result) for result in closest_result.hard_results if result.passed
    ]
    if closest_result is None:
        missing = [{
            "key": predicate["key"],
            "expected": (
                {"minimum": predicate.get("minimum"), "maximum": predicate.get("maximum")}
                if predicate["op"] == "range" else predicate.get("value", predicate["op"])
            ),
            "observed": None,
            "evidence_state": "unknown",
            "reason": "unknown_fact",
        } for predicate in json.loads(request_json)["requirements"]]
    else:
        missing = [
            _query_result_json(result) for result in closest_result.hard_results if not result.passed
        ]
    closest = None if closest_snapshot is None else {
        "scenario_id": closest_snapshot.scenario_id,
        "lifecycle_state": closest_snapshot.lifecycle_state,
        "manifest": closest_snapshot.explanation.get("manifest", {}),
        "satisfied_requirements": satisfied,
        "missing_requirements": missing,
    }
    payload = {
        "review_status": "pending",
        "executable": False,
        "query": json.loads(request_json),
        "build_action": "repair_closest_scenario" if closest is not None else "create_scenario",
        "closest_candidate": closest,
        "satisfied_requirements": satisfied,
        "missing_requirements": missing,
        "unmet_capabilities": [] if closest is None else [{
            "scenario_id": closest_snapshot.scenario_id,
            "manifest": closest_snapshot.explanation.get("manifest", {}),
            "unmet": missing,
        }],
        "candidate_manifest": _known_draft_footing(closest_snapshot),
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


def _registry_query_repair_action(
    connection: sqlite3.Connection,
    *,
    query_id: str,
    request: RegistryQueryRequest,
    evaluation: RegistryStoredQueryEvaluation,
) -> Optional[Dict[str, Any]]:
    """Route one blocked query to its exact current contradiction owner."""
    # An active partial match can be closer than the quarantined candidate
    # whose current contradiction the query needs to repair.  Select the
    # query-bound contradiction first; closeness is only for inert drafts.
    targets = []
    for snapshot in evaluation.candidates:
        manifest = snapshot.explanation.get("manifest", {})
        routes = snapshot.explanation.get("route_evidence", ())
        if not isinstance(manifest, Mapping) or not isinstance(routes, Sequence):
            continue
        manifest_id = str(manifest.get("manifest_id", "")).strip()
        if not manifest_id or not _repair_query_matches_manifest(
                connection, manifest_id=manifest_id, request=request):
            continue
        for route in routes:
            if not isinstance(route, Mapping) or route.get("evidence_state") not in {"contradicted", "stale"}:
                continue
            details = route.get("details", {})
            if not isinstance(details, Mapping):
                continue
            route_key = str(route.get("route_key", "")).strip()
            red_ids = details.get("unresolved_contradiction_ids", ())
            if route.get("evidence_state") == "stale":
                red_ids = _stale_repairable_red_ids(
                    connection, manifest_id=manifest_id, route_key=route_key,
                )
            if not route_key or not isinstance(red_ids, Sequence) or isinstance(red_ids, (str, bytes)):
                continue
            targets.extend(
                (manifest_id, route_key, str(red_id).strip())
                for red_id in red_ids if str(red_id).strip()
            )
    if not targets:
        return None
    manifest_id, route_key, red_verification_id = sorted(set(targets))[0]
    return {
        "kind": "repair_current_contradiction",
        "reason": "closest_query_candidate_has_current_unresolved_contradiction",
        "required_identifiers": {
            "query_id": query_id,
            "manifest_id": manifest_id,
            "route_key": route_key,
            "red_verification_id": red_verification_id,
        },
        "command": {
            "name": "registry-repair-bootstrap",
            "arguments": {"query_id": query_id},
            "cli": ["registry-repair-bootstrap", "--query-id", query_id],
        },
    }


def _repair_query_candidate_manifest_ids(
    connection: sqlite3.Connection,
    request: RegistryQueryRequest,
) -> Tuple[str, ...]:
    """Narrow repair review to manifests declaring every exact requested fact.

    Repair authority must re-evaluate current facts, but it need not expand the
    whole registry before locating a contradiction whose immutable query only
    asks for exact declared values.  Other predicate forms retain the complete
    projection fallback below.
    """
    exact_requirements = tuple(
        predicate for predicate in request.requirements if predicate.op == "eq"
    )
    if len(exact_requirements) != len(request.requirements) or not exact_requirements:
        return ()
    matching_ids: Optional[set[str]] = None
    for predicate in exact_requirements:
        rows = connection.execute(
            "SELECT manifest_id FROM manifest_capability_current "
            "WHERE capability_key = ? AND value_json = ?",
            (predicate.key, _json_text(predicate.value)),
        ).fetchall()
        identifiers = {str(row["manifest_id"]) for row in rows}
        matching_ids = identifiers if matching_ids is None else matching_ids & identifiers
        if not matching_ids:
            return ()
    return tuple(sorted(matching_ids or ()))


def registry_query_repair_action(
    connection: sqlite3.Connection,
    query_id: str,
) -> Dict[str, Any]:
    """Re-derive a query's repair target from current authoritative state."""
    normalized_query_id = str(query_id).strip()
    row = connection.execute(
        "SELECT request_json FROM query_history WHERE query_id = ? AND query_kind = 'registry_query' "
        "ORDER BY query_event_id DESC LIMIT 1",
        (normalized_query_id,),
    ).fetchone()
    if row is None:
        raise ScenarioRegistryStoreError("repair query is absent")
    request_json = str(row["request_json"])
    request = parse_registry_query_request(json.loads(request_json))
    expected_query_id = _identity(
        "caol-scenario-query-v1", hashlib.sha256(request_json.encode("utf-8")).hexdigest()
    )
    if normalized_query_id != expected_query_id:
        raise ScenarioRegistryStoreError("repair query identity does not match its request")
    candidate_ids = _repair_query_candidate_manifest_ids(connection, request)
    candidates = build_registry_query_candidate_snapshot(
        connection,
        include_lifecycle_states=("quarantined",),
        manifest_ids=candidate_ids,
    ) if candidate_ids else build_registry_query_candidate_snapshot(
        connection, include_lifecycle_states=("quarantined",),
    )
    evaluation = RegistryStoredQueryEvaluation(
        candidates=candidates,
        evaluation=evaluate_registry_query(
            request,
            tuple({"scenario_id": candidate.scenario_id, "facts": candidate.facts} for candidate in candidates),
        ),
    )
    action = _registry_query_repair_action(
        connection,
        query_id=normalized_query_id,
        request=request,
        evaluation=evaluation,
    )
    if action is None:
        raise ScenarioRegistryStoreError("repair query has no current unresolved contradiction")
    return {**action, "query": json.loads(request_json)}


def _selection_fit_reason(
    request: RegistryQueryRequest,
    candidate: RegistryQueryCandidateSnapshot,
    evaluation: RegistryQueryEvaluation,
) -> str:
    """Describe the chosen current facts without promoting them to proof."""
    result = next(
        (item for item in evaluation.candidates if item.scenario_id == candidate.scenario_id),
        None,
    )
    if result is None or not result.hard_valid:
        raise ScenarioRegistryStoreError("selected scenario has no compatible hard-query result")
    required = ", ".join(predicate.key for predicate in request.requirements) or "no hard requirements"
    preferred = ", ".join(
        item.key for item in result.preference_results if item.passed
    ) or "no satisfied preferences"
    return f"Current declared facts satisfy {required}; ranking keeps {preferred}."


def _append_scenario_selection(
    connection: sqlite3.Connection,
    *,
    query_id: str,
    request: RegistryQueryRequest,
    candidate: RegistryQueryCandidateSnapshot,
    evaluation: RegistryQueryEvaluation,
    authorization: Optional[Mapping[str, Any]] = None,
) -> str:
    manifest = candidate.explanation.get("manifest")
    if not isinstance(manifest, Mapping):
        raise ScenarioRegistryStoreError("selected scenario manifest explanation is unavailable")
    manifest_id = str(manifest.get("manifest_id", "")).strip()
    manifest_sha256 = str(manifest.get("sha256", "")).strip().lower()
    revision = manifest.get("revision")
    if not manifest_id or len(manifest_sha256) != 64 or type(revision) is not int:
        raise ScenarioRegistryStoreError("selected scenario source binding is unavailable")
    reason = _selection_fit_reason(request, candidate, evaluation)
    selection_id = _identity(
        "caol-scenario-selection-reason-v1", query_id, manifest_id,
        str(revision), manifest_sha256,
    )
    connection.execute(
        "INSERT OR IGNORE INTO scenario_selection_history( "
        "selection_id, query_id, manifest_id, manifest_revision, manifest_sha256, fit_reason, details_json "
        ") VALUES( ?, ?, ?, ?, ?, ?, ? )",
        (
            selection_id, query_id, manifest_id, revision, manifest_sha256, reason,
            _json_text({
                "requirements": [_query_predicate_json(item) for item in request.requirements],
                "preferences": [_query_predicate_json(item) for item in request.preferences],
                "lifecycle": candidate.lifecycle_state,
                "coordinator_authorization": dict(authorization) if authorization is not None else None,
            }),
        ),
    )
    return selection_id


def create_source_bound_scenario(
    connection: sqlite3.Connection,
    *,
    scenarios_root: Path,
    name: str,
    declaration: Mapping[str, Any],
) -> Dict[str, Any]:
    """Create exactly one canonical manifest, or reject a different identity.

    Creation is intentionally source-bound: the declaration is serialized once,
    validated from its exact source bytes, and immediately projected.  It never
    creates a report, verification, token, or gameplay authority.
    """
    scenario_name = str(name).strip()
    root = scenarios_root.resolve()
    if not scenario_name or Path(scenario_name).name != scenario_name or "/" in scenario_name or "\\" in scenario_name:
        raise ScenarioRegistryStoreError("scenario name must be one canonical filename stem")
    if not isinstance(declaration, Mapping):
        raise ScenarioRegistryStoreError("scenario declaration must be an object")
    target = (root / f"{scenario_name}.json").resolve()
    if target.parent != root:
        raise ScenarioRegistryStoreError("scenario declaration escapes the canonical root")
    source_bytes = (json.dumps(dict(declaration), ensure_ascii=False, indent=2, sort_keys=True) + "\n").encode("utf-8")
    created = False
    if target.exists():
        if target.read_bytes() != source_bytes:
            raise ScenarioRegistryStoreError("scenario identity collision has different source bytes")
    else:
        root.mkdir(parents=True, exist_ok=True)
        target.write_bytes(source_bytes)
        created = True
    try:
        normalized = validate_manifest(json.loads(source_bytes.decode("utf-8")), path=target)
        projection = rebuild_manifest_projection(connection, root)
    except BaseException:
        if created and target.exists():
            target.unlink()
        raise
    row = connection.execute(
        "SELECT manifest_id, revision, current_sha256 FROM manifest_current WHERE source_path = ? AND present = 1",
        (str(target),),
    ).fetchone()
    if row is None:
        raise ScenarioRegistryStoreError("created scenario was not projected")
    return {
        "scenario": scenario_name,
        "manifest_id": str(row["manifest_id"]),
        "manifest_sha256": str(row["current_sha256"]),
        "revision": int(row["revision"]),
        "idempotent": not created,
        "validation": normalized["validation"],
        "projection": projection,
        "evidence_effect": "none_for_manufactured_state",
    }


def validate_source_bound_scenario(
    connection: sqlite3.Connection,
    *,
    scenario_name: str,
    scenarios_root: Optional[Path] = None,
) -> Dict[str, Any]:
    """Validate one exact manifest and fixture source without launching it."""
    root = scenarios_root or repository_root() / "tools" / "openclaw_harness" / "scenarios"
    source_path = root / f"{scenario_name}.json"
    row = connection.execute(
        "SELECT manifest_id, current_sha256, declaration_json FROM manifest_current "
        "WHERE source_path = ? AND present = 1",
        (str(source_path.resolve()),),
    ).fetchone()
    if row is None:
        raise ScenarioRegistryStoreError("scenario is not a current projected manifest")
    declaration = _json_object(str(row["declaration_json"]), "scenario declaration")
    exact = validate_manifest(declaration, path=source_path)
    fixture_name = str(declaration.get("fixture", declaration.get("runtime_contract", {}).get("fixture", ""))).strip()
    fixture_profile = str(declaration.get("fixture_profile", "")).strip()
    fixture_binding: Mapping[str, Any] = {}
    status = "valid"
    reason = "exact source and fixture binding are current"
    try:
        if not fixture_name:
            raise ScenarioRegistryStoreError("scenario fixture is missing")
        from startup_harness import fixture_source_binding, resolve_fixture_payload
        resolved = resolve_fixture_payload(fixture_name, fixture_profile)
        fixture_binding = fixture_source_binding(fixture_name, fixture_profile)
        details = {
            "source_path": str(source_path.resolve()),
            "source_sha256": exact["source"]["sha256"],
            "fixture": str(resolved["fixture"]),
            "fixture_profile": str(resolved["fixture_profile"]),
        }
    except (KeyError, OSError, SystemExit, ScenarioRegistryStoreError) as exc:
        status = "invalid"
        reason = str(exc)
        details = {"source_path": str(source_path.resolve()), "source_sha256": exact["source"]["sha256"]}
    fixture_json = _json_text(fixture_binding)
    validation_id = _identity(
        "caol-scenario-validation-v1", str(row["manifest_id"]), str(row["current_sha256"]),
        fixture_json, status, reason,
    )
    with immediate_transaction(connection):
        connection.execute(
            "INSERT OR IGNORE INTO scenario_validation_history( "
            "validation_id, manifest_id, manifest_sha256, fixture_binding_json, validation_status, reason, details_json "
            ") VALUES( ?, ?, ?, ?, ?, ?, ? )",
            (validation_id, str(row["manifest_id"]), str(row["current_sha256"]), fixture_json,
             status, reason, _json_text(details)),
        )
    return {
        "validation_id": validation_id,
        "status": status,
        "reason": reason,
        "fixture_binding": fixture_binding,
        "evidence_effect": "none_for_manufactured_state",
    }


def prepare_selected_scenario(
    connection: sqlite3.Connection,
    *,
    scenario_name: str,
    world_dir: Path,
    required_typeid: str,
    candidate_offsets: Sequence[Sequence[int]],
    player_save: str = "",
    scenarios_root: Optional[Path] = None,
    fixture_install: Optional[Mapping[str, Any]] = None,
) -> Dict[str, Any]:
    """Prepare one validated scenario through its setup owner and retain receipt.

    This is not a launch path.  A failed placement remains an append-only,
    reusable setup gap and cannot be substituted with an incidental creature.
    """
    root = scenarios_root or repository_root() / "tools" / "openclaw_harness" / "scenarios"
    source_path = root / f"{scenario_name}.json"
    manifest = connection.execute(
        "SELECT manifest_id, current_sha256 FROM manifest_current WHERE source_path = ? AND present = 1",
        (str(source_path.resolve()),),
    ).fetchone()
    if manifest is None:
        raise ScenarioRegistryStoreError("scenario is not a current projected manifest")
    validation = connection.execute(
        "SELECT validation_id FROM scenario_validation_history WHERE manifest_id = ? AND manifest_sha256 = ? "
        "AND validation_status = 'valid' ORDER BY recorded_at DESC, validation_id DESC LIMIT 1",
        (str(manifest["manifest_id"]), str(manifest["current_sha256"])),
    ).fetchone()
    if validation is None:
        raise ScenarioRegistryStoreError("scenario preparation requires a current successful validation")
    fixture_intervention_id = None
    if fixture_install is not None:
        if not isinstance(fixture_install, Mapping) or not fixture_install.get("installed_worlds"):
            raise ScenarioRegistryStoreError("fixture preparation did not install a selected world")
        fixture_arguments = {
            "fixture": str(fixture_install.get("fixture", "")),
            "profile": str(fixture_install.get("profile", "")),
            "fixture_profile": str(fixture_install.get("fixture_profile", "")),
        }
        fixture_target = {"worlds": list(fixture_install.get("installed_worlds", []))}
        fixture_receipt = {
            "owner": "fixture_install",
            "accepted": True,
            "binding": fixture_install.get("binding", {}),
        }
        fixture_intervention_id = _identity(
            "caol-scenario-intervention-v1", str(manifest["manifest_id"]),
            str(validation["validation_id"]), "install_fixture", _json_text(fixture_arguments),
            _json_text(fixture_target), _json_text(fixture_receipt),
        )
        with immediate_transaction(connection):
            connection.execute(
                "INSERT OR IGNORE INTO scenario_intervention_history( "
                "intervention_id, manifest_id, validation_id, operation, arguments_json, target_json, native_receipt_json, "
                "before_facts_json, after_facts_json, evidence_effect, preparation_status "
                ") VALUES( ?, ?, ?, 'install_fixture', ?, ?, ?, '{}', ?, 'none_for_manufactured_state', 'prepared' )",
                (
                    fixture_intervention_id, str(manifest["manifest_id"]), str(validation["validation_id"]),
                    _json_text(fixture_arguments), _json_text(fixture_target), _json_text(fixture_receipt),
                    _json_text({"installed_worlds": fixture_target["worlds"]}),
                ),
            )
    from startup_harness import prepare_required_monster
    prepared = prepare_required_monster(
        world_dir,
        typeid=required_typeid,
        candidate_offsets=candidate_offsets,
        player_save=player_save,
    )
    arguments = {
        "required_typeid": str(required_typeid),
        "candidate_offsets": [list(offset) for offset in candidate_offsets],
        "player_save": str(player_save),
    }
    intervention_id = _identity(
        "caol-scenario-intervention-v1", str(manifest["manifest_id"]),
        str(validation["validation_id"]), prepared["operation"], _json_text(arguments),
        _json_text(prepared["target"]), _json_text(prepared["native_receipt"]),
    )
    with immediate_transaction(connection):
        connection.execute(
            "INSERT OR IGNORE INTO scenario_intervention_history( "
            "intervention_id, manifest_id, validation_id, operation, arguments_json, target_json, native_receipt_json, "
            "before_facts_json, after_facts_json, evidence_effect, preparation_status "
            ") VALUES( ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ? )",
            (
                intervention_id, str(manifest["manifest_id"]), str(validation["validation_id"]),
                str(prepared["operation"]), _json_text(arguments), _json_text(prepared["target"]),
                _json_text(prepared["native_receipt"]), _json_text(prepared["before_facts"]),
                _json_text(prepared["after_facts"]), "none_for_manufactured_state", str(prepared["status"]),
            ),
        )
    return {
        "intervention_id": intervention_id,
        "fixture_intervention_id": fixture_intervention_id,
        "validation_id": str(validation["validation_id"]),
        "status": prepared["status"],
        "target": prepared["target"],
        "native_receipt": prepared["native_receipt"],
        "evidence_effect": "none_for_manufactured_state",
        "gameplay_credit": False,
    }


def record_capability_contract_revision(
    connection: sqlite3.Connection, *, capability_id: str, contract: Mapping[str, Any],
) -> Dict[str, Any]:
    """Append a reusable cockpit contract; scenario manifests stay compatibility facts."""
    identifier = str(capability_id).strip()
    if not identifier or not isinstance(contract, Mapping):
        raise ScenarioRegistryStoreError("capability id and contract are required")
    required = ("inputs", "results", "preconditions", "postconditions", "recovery", "examples", "proof_effects")
    if any(key not in contract for key in required):
        raise ScenarioRegistryStoreError("capability contract is missing a required section")
    payload = _json_text(dict(contract))
    with immediate_transaction(connection):
        row = connection.execute(
            "SELECT revision, contract_json FROM capability_contract_revision WHERE capability_id = ? "
            "ORDER BY revision DESC LIMIT 1", (identifier,),
        ).fetchone()
        if row is not None and str(row["contract_json"]) == payload:
            return {"capability_id": identifier, "revision": int(row["revision"]), "idempotent": True}
        revision = 1 if row is None else int(row["revision"]) + 1
        connection.execute(
            "INSERT INTO capability_contract_revision( capability_id, revision, contract_json ) VALUES( ?, ?, ? )",
            (identifier, revision, payload),
        )
    return {"capability_id": identifier, "revision": revision, "idempotent": False}


def capability_contracts(connection: sqlite3.Connection, *, query: str = "", capability_id: str = "") -> Tuple[Dict[str, Any], ...]:
    """Return only current catalog revisions, compactly and deterministically."""
    needle = str(query).strip().casefold()
    identifier = str(capability_id).strip()
    rows = connection.execute(
        "SELECT revision.capability_id, revision.revision, revision.contract_json FROM capability_contract_revision AS revision "
        "JOIN ( SELECT capability_id, MAX(revision) AS revision FROM capability_contract_revision GROUP BY capability_id ) AS current "
        "ON current.capability_id = revision.capability_id AND current.revision = revision.revision "
        "WHERE ( ? = '' OR revision.capability_id = ? ) ORDER BY revision.capability_id",
        (identifier, identifier),
    ).fetchall()
    result = []
    for row in rows:
        contract = _json_object(str(row["contract_json"]), "capability contract")
        searchable = (str(row["capability_id"]) + " " + _json_text(contract)).casefold()
        if needle and needle not in searchable:
            continue
        result.append({"id": str(row["capability_id"]), "revision": int(row["revision"]), "contract": contract})
    return tuple(result)


def _cockpit_evidence_effect(connection: sqlite3.Connection, scenario_id: str) -> str:
    """Only registry state may classify a cockpit receipt; callers cannot promote it."""
    row = connection.execute(
        "SELECT evidence_state FROM capability_evidence_history WHERE manifest_id = ? "
        "ORDER BY recorded_at DESC LIMIT 1", (scenario_id,),
    ).fetchone()
    if row is None:
        return "none"
    state = str(row["evidence_state"])
    if state in {"setup-only", "diagnostic", "focused"}:
        return state
    return "none"


def record_cockpit_run_receipt(
    connection: sqlite3.Connection, *, run_id: str, scenario_id: str = "", binding_id: str = "",
    event_kind: str, details: Optional[Mapping[str, Any]] = None,
) -> Dict[str, Any]:
    """Append a compact status/finish fact and derive, never accept, its evidence effect."""
    run = str(run_id).strip()
    if not run or event_kind not in {"status", "finish"}:
        raise ScenarioRegistryStoreError("run id and supported receipt kind are required")
    scenario = str(scenario_id).strip()
    binding = str(binding_id).strip()
    effect = _cockpit_evidence_effect(connection, scenario)
    cost = {"state": "unavailable"}
    receipt_details = dict(details or {})
    receipt_id = _identity("caol-cockpit-run-receipt-v1", run, scenario, binding, event_kind,
                           _json_text(receipt_details), effect, _json_text(cost))
    with immediate_transaction(connection):
        connection.execute(
            "INSERT OR IGNORE INTO cockpit_run_receipt( receipt_id, run_id, scenario_id, binding_id, event_kind, "
            "details_json, evidence_effect, observed_cost_json ) VALUES( ?, ?, ?, ?, ?, ?, ?, ? )",
            (receipt_id, run, scenario, binding, event_kind, _json_text(receipt_details), effect, _json_text(cost)),
        )
    return {"receipt_id": receipt_id, "run_id": run, "state": "finished" if event_kind == "finish" else "active",
            "evidence_effect": effect, "observed_cost": cost}


def cockpit_run_status(connection: sqlite3.Connection, *, run_id: str) -> Dict[str, Any]:
    rows = connection.execute(
        "SELECT receipt_id, scenario_id, binding_id, event_kind, details_json, evidence_effect, observed_cost_json "
        "FROM cockpit_run_receipt WHERE run_id = ? ORDER BY recorded_at, receipt_id", (str(run_id).strip(),),
    ).fetchall()
    if not rows:
        raise ScenarioRegistryStoreError("cockpit run has no receipt")
    latest = rows[-1]
    return {"run_id": str(run_id), "state": "finished" if latest["event_kind"] == "finish" else "active",
            "scenario_id": str(latest["scenario_id"]), "binding_id": str(latest["binding_id"]),
            "receipt_id": str(latest["receipt_id"]), "evidence_effect": str(latest["evidence_effect"]),
            "observed_cost": dict(_json_object(str(latest["observed_cost_json"]), "cockpit observed cost")),
            "details": dict(_json_object(str(latest["details_json"]), "cockpit receipt details"))}


def _run_evidence_ceiling(connection: sqlite3.Connection, manifest_id: str,
                          declaration: Mapping[str, Any]) -> str:
    """Derive observation authority separately from later proof eligibility."""
    runtime = declaration.get("runtime_contract")
    if not isinstance(runtime, Mapping) or runtime.get("grants_gameplay_proof") is not True:
        return "zero-credit"
    effect = _cockpit_evidence_effect(connection, manifest_id)
    if effect == "focused":
        return "focused"
    if effect == "setup-only":
        return "setup-only"
    return "diagnostic"


def _run_authority_row(connection: sqlite3.Connection, run_id: str) -> Optional[sqlite3.Row]:
    return connection.execute(
        "SELECT * FROM cockpit_run_authority WHERE run_id = ?", (str(run_id).strip(),),
    ).fetchone()


def _run_authority_terminal(connection: sqlite3.Connection, receipt_id: str) -> Optional[sqlite3.Row]:
    return connection.execute(
        "SELECT event_kind, details_json FROM cockpit_run_authority_event "
        "WHERE receipt_id = ? AND event_kind IN ( 'finished', 'invalidated' ) "
        "ORDER BY recorded_at, event_id LIMIT 1", (receipt_id,),
    ).fetchone()


def _append_run_authority_event(connection: sqlite3.Connection, *, receipt_id: str,
                                event_kind: str, details: Mapping[str, Any]) -> None:
    event_id = _identity(
        "caol-cockpit-run-authority-event-v1", receipt_id, event_kind,
        _json_text(dict(details)),
    )
    connection.execute(
        "INSERT OR IGNORE INTO cockpit_run_authority_event( event_id, receipt_id, event_kind, details_json ) "
        "VALUES( ?, ?, ?, ? )",
        (event_id, receipt_id, event_kind, _json_text(dict(details))),
    )


def open_cockpit_run(
    connection: sqlite3.Connection, *, selection_id: str, owner_id: str,
    workspace_root: Optional[Path] = None,
) -> Dict[str, Any]:
    """Open one source-bound run without borrowing evidence-token eligibility."""
    selection = str(selection_id).strip()
    owner = str(owner_id).strip()
    if not selection or not owner:
        raise ScenarioRegistryStoreError("run.open needs one current selection and service owner")
    selected = connection.execute(
        "SELECT selection_id, manifest_id, manifest_revision, manifest_sha256, details_json "
        "FROM scenario_selection_history WHERE selection_id = ?", (selection,),
    ).fetchone()
    if selected is None:
        raise ScenarioRegistryStoreError("run.open selection is unknown")
    if connection.execute(
            "SELECT 1 FROM cockpit_run_authority WHERE selection_id = ?", (selection,),
    ).fetchone() is not None:
        raise ScenarioRegistryStoreError("run.open selection was already consumed")
    details = _json_object(str(selected["details_json"]), "scenario selection details")
    if details.get("lifecycle") != "active":
        raise ScenarioRegistryStoreError("run.open selection is not active")
    manifest = connection.execute(
        "SELECT manifest_id, source_path, present, revision, current_sha256, declaration_json, validation_json "
        "FROM manifest_current WHERE manifest_id = ?", (str(selected["manifest_id"]),),
    ).fetchone()
    if manifest is None or int(manifest["present"]) != 1:
        raise ScenarioRegistryStoreError("run.open scenario source is not current")
    if (int(manifest["revision"]) != int(selected["manifest_revision"]) or
            str(manifest["current_sha256"]) != str(selected["manifest_sha256"])):
        raise ScenarioRegistryStoreError("run.open selection binding drifted")
    validation = _json_object(str(manifest["validation_json"]), "manifest validation")
    if validation.get("status") != "valid":
        raise ScenarioRegistryStoreError("run.open scenario is not valid")
    declaration = _json_object(str(manifest["declaration_json"]), "manifest declaration")
    runtime = declaration.get("runtime_contract")
    if not isinstance(runtime, Mapping):
        raise ScenarioRegistryStoreError("run.open scenario has no runtime contract")
    if runtime.get("setup_only_debug") is True and runtime.get("disposable_copy") is not True:
        raise ScenarioRegistryStoreError("run.open rejects debug setup without a disposable copy")
    source_path = Path(str(manifest["source_path"]))
    source_sha256 = path_sha256(source_path)
    if source_sha256 != str(selected["manifest_sha256"]):
        raise ScenarioRegistryStoreError("run.open scenario bytes changed after selection")
    requirements = runtime.get("requirements")
    executable_name = str(requirements.get("executable", "")).strip() \
        if isinstance(requirements, Mapping) else ""
    if not executable_name:
        raise ScenarioRegistryStoreError("run.open runtime has no executable")
    executable_path = Path(executable_name)
    if not executable_path.is_absolute():
        executable_path = (workspace_root or repository_root()) / executable_path
    executable_sha256 = path_sha256(executable_path)
    profile = str(declaration.get("profile", runtime.get("profile", ""))).strip()
    world = str(declaration.get("world", "")).strip()
    ownership_scope = _identity(
        "caol-cockpit-run-ownership-v1", str(manifest["manifest_id"]), profile, world,
    )
    conflict = connection.execute(
        "SELECT authority.run_id FROM cockpit_run_authority AS authority "
        "WHERE authority.ownership_scope = ? AND NOT EXISTS ( "
        "SELECT 1 FROM cockpit_run_authority_event AS event WHERE event.receipt_id = authority.receipt_id "
        "AND event.event_kind IN ( 'finished', 'invalidated' ) ) LIMIT 1",
        (ownership_scope,),
    ).fetchone()
    if conflict is not None:
        raise ScenarioRegistryStoreError("run.open ownership conflicts with an active run")
    run_id = f"cockpit-{uuid.uuid4().hex}"
    binding_id = _identity(
        "caol-cockpit-run-binding-v1", str(manifest["manifest_id"]),
        str(manifest["revision"]), source_sha256, str(executable_path.resolve()),
        executable_sha256, run_id, ownership_scope,
    )
    ceiling = _run_evidence_ceiling(connection, str(manifest["manifest_id"]), declaration)
    receipt_id = _identity(
        "caol-cockpit-run-open-receipt-v1", selection, run_id, binding_id, owner, ceiling,
    )
    with immediate_transaction(connection):
        connection.execute(
            "INSERT INTO cockpit_run_authority( receipt_id, selection_id, run_id, manifest_id, "
            "manifest_revision, manifest_sha256, source_path, source_sha256, executable_path, "
            "executable_sha256, binding_id, ownership_scope, owner_id, evidence_ceiling ) "
            "VALUES( ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ? )",
            (receipt_id, selection, run_id, str(manifest["manifest_id"]), int(manifest["revision"]),
             str(manifest["current_sha256"]), str(source_path.resolve()), source_sha256,
             str(executable_path.resolve()), executable_sha256, binding_id, ownership_scope, owner, ceiling),
        )
        _append_run_authority_event(
            connection, receipt_id=receipt_id, event_kind="opened",
            details={"prior_proof_required": False, "proof_promotion_authority": False},
        )
    return {
        "receipt_id": receipt_id, "run_id": run_id, "scenario_id": str(manifest["manifest_id"]),
        "scenario_revision": int(manifest["revision"]), "binding_id": binding_id,
        "state": "active", "evidence_ceiling": ceiling,
        "proof_promotion_authority": False,
    }


def cockpit_run_authority_status(connection: sqlite3.Connection, *, run_id: str) -> Dict[str, Any]:
    """Revalidate bound bytes and fail closed without erasing the opened receipt."""
    row = _run_authority_row(connection, run_id)
    if row is None:
        raise ScenarioRegistryStoreError("cockpit run has no opening authority")
    terminal = _run_authority_terminal(connection, str(row["receipt_id"]))
    drift = ""
    if terminal is None:
        current = connection.execute(
            "SELECT present, revision, current_sha256, source_path FROM manifest_current WHERE manifest_id = ?",
            (str(row["manifest_id"]),),
        ).fetchone()
        try:
            if (current is None or int(current["present"]) != 1 or
                    int(current["revision"]) != int(row["manifest_revision"]) or
                    str(current["current_sha256"]) != str(row["manifest_sha256"]) or
                    str(Path(str(current["source_path"])).resolve()) != str(row["source_path"]) or
                    path_sha256(Path(str(row["source_path"]))) != str(row["source_sha256"])):
                drift = "scenario_binding_drift"
            elif path_sha256(Path(str(row["executable_path"]))) != str(row["executable_sha256"]):
                drift = "executable_binding_drift"
        except ScenarioRegistryStoreError:
            drift = "bound_path_unavailable"
        if drift:
            with immediate_transaction(connection):
                _append_run_authority_event(
                    connection, receipt_id=str(row["receipt_id"]), event_kind="invalidated",
                    details={"reason": drift},
                )
            terminal = _run_authority_terminal(connection, str(row["receipt_id"]))
    state = str(terminal["event_kind"]) if terminal is not None else "active"
    return {
        "receipt_id": str(row["receipt_id"]), "run_id": str(row["run_id"]),
        "scenario_id": str(row["manifest_id"]), "scenario_revision": int(row["manifest_revision"]),
        "binding_id": str(row["binding_id"]), "state": state,
        "evidence_ceiling": "zero-credit" if state == "invalidated" else str(row["evidence_ceiling"]),
        "proof_promotion_authority": False,
        "terminal": dict(_json_object(str(terminal["details_json"]), "run authority terminal"))
        if terminal is not None else {},
    }


def finish_cockpit_run_authority(connection: sqlite3.Connection, *, run_id: str,
                                 details: Optional[Mapping[str, Any]] = None) -> Dict[str, Any]:
    status = cockpit_run_authority_status(connection, run_id=run_id)
    if status["state"] != "active":
        return status
    row = _run_authority_row(connection, run_id)
    assert row is not None
    with immediate_transaction(connection):
        _append_run_authority_event(
            connection, receipt_id=str(row["receipt_id"]), event_kind="finished",
            details=dict(details or {}),
        )
    return cockpit_run_authority_status(connection, run_id=run_id)


def report_capability_gap(
    connection: sqlite3.Connection, *, run_id: str, scenario_id: str, binding_id: str,
    blocked_intent: str, missing_kind: str, evidence: Mapping[str, Any], reusable_outcome: str,
    affected_scenarios: Sequence[str],
) -> Dict[str, Any]:
    """Store one reusable gap and link equivalent reports instead of cloning warnings."""
    if missing_kind not in {"observation", "action", "setup", "recovery", "structured_failure"}:
        raise ScenarioRegistryStoreError("gap missing kind is unsupported")
    fields = (run_id, scenario_id, blocked_intent, reusable_outcome)
    if any(not str(value).strip() for value in fields) or not isinstance(evidence, Mapping):
        raise ScenarioRegistryStoreError("gap report is missing required facts")
    key = _identity("caol-capability-gap-v1", str(blocked_intent).strip(), missing_kind, str(reusable_outcome).strip())
    gap_id = key
    cost = {"state": "unavailable"}
    report_id = _identity("caol-capability-gap-report-v1", key, str(run_id), str(scenario_id), str(binding_id),
                          _json_text(dict(evidence)),
                          _json_text(sorted({str(item) for item in affected_scenarios if str(item).strip()})))
    with immediate_transaction(connection):
        connection.execute(
            "INSERT OR IGNORE INTO capability_gap( gap_id, equivalence_key, blocked_intent, missing_kind, reusable_outcome, "
            "evidence_json, observed_cost_json ) VALUES( ?, ?, ?, ?, ?, ?, ? )",
            (gap_id, key, str(blocked_intent).strip(), missing_kind, str(reusable_outcome).strip(),
             _json_text(dict(evidence)), _json_text(cost)),
        )
        connection.execute(
            "INSERT OR IGNORE INTO capability_gap_report( report_id, gap_id, run_id, scenario_id, binding_id, affected_scenarios_json ) "
            "VALUES( ?, ?, ?, ?, ?, ? )", (report_id, gap_id, str(run_id), str(scenario_id), str(binding_id),
                                             _json_text(sorted({str(item) for item in affected_scenarios if str(item).strip()}))),
        )
        count = connection.execute("SELECT COUNT(*) FROM capability_gap_report WHERE gap_id = ?", (gap_id,)).fetchone()[0]
    return {"gap_id": gap_id, "report_id": report_id, "linked": int(count) > 1,
            "observed_cost": cost, "evidence_effect": "none"}


def capability_gaps(connection: sqlite3.Connection, *, scenario_id: str = "") -> Tuple[Dict[str, Any], ...]:
    where = "" if not str(scenario_id).strip() else "WHERE report.scenario_id = ? OR report.affected_scenarios_json LIKE ?"
    values: Tuple[Any, ...] = () if not where else (str(scenario_id).strip(), f'%"{str(scenario_id).strip()}"%')
    rows = connection.execute(
        "SELECT DISTINCT gap.gap_id, gap.blocked_intent, gap.missing_kind, gap.reusable_outcome, gap.observed_cost_json "
        "FROM capability_gap AS gap JOIN capability_gap_report AS report ON report.gap_id = gap.gap_id " + where +
        " ORDER BY gap.recorded_at, gap.gap_id", values,
    ).fetchall()
    return tuple({"gap_id": str(row["gap_id"]), "blocked_intent": str(row["blocked_intent"]),
                  "missing_kind": str(row["missing_kind"]), "reusable_outcome": str(row["reusable_outcome"]),
                  "observed_cost": dict(_json_object(str(row["observed_cost_json"]), "gap observed cost"))}
                 for row in rows)


def execute_registry_query(
    connection: sqlite3.Connection,
    request: RegistryQueryRequest,
    *,
    include_lifecycle_states: Sequence[str] = (),
    drafts_root: Optional[Path] = None,
    coordinator_brief: Optional[Mapping[str, Any]] = None,
    witness_charter: Optional[Mapping[str, Any]] = None,
) -> RegistryQueryExecution:
    """Audit a fixed query and issue one technical token or a deterministic inert draft."""
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
    first_run_route = None
    selected_name = str(selected.explanation.get("manifest", {}).get("name", "")) if selected is not None else ""
    # A startup-only/non-certification report must not consume the unique
    # first-run certification route.  Permit a fresh ordinary certification
    # token while retaining all prior diagnostic history.
    if selected is not None and (route is None or selected_name in {
            "bandit.r005_continuous_hostile_ecology_certification",
            "bandit.r005_safe_wait_observation",
            "bandit.r005_native_wait_qualification",
            "r018.raw_wait_acceptance_mcw",
            "r019.keep_watch_meaningful_event_bootstrap_mcw",
            "r019.keep_watch_off_interruption_closure057_bootstrap_mcw",
    }):
        first_run_route = _first_run_certification_route(selected)
        route = first_run_route
    bootstrap_authority = (
        selected.explanation.get("bootstrap_authority")
        if selected is not None and selected.token_eligible else None
    )
    if route is not None and (coordinator_brief is not None or witness_charter is not None) and \
            route.get("evidence_state") in {"stale", "unknown"}:
        route = None
    if route is None and isinstance(bootstrap_authority, Mapping) and \
            coordinator_brief is None and witness_charter is None:
        stale_routes = tuple(
            item for item in selected.explanation.get("route_evidence", ())
            if isinstance(item, Mapping) and item.get("evidence_state") in {"stale", "unknown"}
        )
        if len(stale_routes) == 1:
            route = stale_routes[0]
    coordinator_authorization = None
    if route is None and selected is not None:
        coordinator_authorization = _coordinator_authorization(
            request, selected, coordinator_brief, witness_charter,
        )
        if coordinator_authorization is not None:
            # This is technical authority only.  It deliberately carries no
            # verification evidence and cannot promote bootstrap/setup facts.
            route = {
                "route_key": "coordinator:" + str(coordinator_authorization["charter_sha256"]),
                "evidence_state": "first_run",
                "internal_resolution_state": "first_run",
                "details": {"source": "coordinator_brief", **dict(coordinator_authorization)},
                "bindings": (),
            }
    if selected is None or route is None:
        root = drafts_root or repository_root() / ".userdata" / "openclaw_harness" / "drafts"
        draft_path = _write_inert_draft(
            request_json=request_json,
            query_sha256=query_sha256,
            evaluation=evaluation,
            drafts_root=root,
        )
        repair_evaluation = evaluate_registry_query_from_store(
            connection,
            request,
            include_lifecycle_states=tuple(sorted({*include_lifecycle_states, "quarantined"})),
        )
        next_action = _registry_query_repair_action(
            connection,
            query_id=query_id,
            request=request,
            evaluation=repair_evaluation,
        )
        with immediate_transaction(connection):
            _append_query_audit(
                connection,
                query_id=query_id,
                request_json=request_json,
                result={
                    "kind": "draft",
                    "draft_path": draft_path,
                    "selected_scenario_id": selected_id,
                    "next_action": next_action,
                },
            )
            selection_id = (
                _append_scenario_selection(
                    connection, query_id=query_id, request=request,
                    candidate=selected, evaluation=evaluation.evaluation,
                    authorization=coordinator_authorization,
                ) if selected is not None else None
            )
        return RegistryQueryExecution(
            query_id, query_sha256, evaluation, None, draft_path, selection_id, next_action
        )

    manifest = selected.explanation["manifest"]
    if first_run_route is not None:
        # A first run has no verification binding to rehydrate, but it must
        # still cross the same persisted-route boundary as every other launch.
        # Persist the declaration-bound authority before exposing a token.
        first_run_route = {
            **dict(first_run_route),
            "details": {
                **dict(first_run_route["details"]),
                "source_path": str(manifest["source_path"]),
            },
        }
        with immediate_transaction(connection):
            _append_route_evidence_if_changed(
                connection,
                manifest_id=str(manifest["manifest_id"]),
                route_key=str(first_run_route["route_key"]),
                evidence_state=str(first_run_route["internal_resolution_state"]),
                details=first_run_route["details"],
            )
            current_route = next(
                (
                    item for item in _current_route_evidence(connection, str(manifest["manifest_id"]))
                    if str(item.get("route_key", "")) == str(first_run_route["route_key"])
                ),
                None,
            )
        if current_route is None or _json_text(current_route) != _json_text(first_run_route):
            raise ScenarioRegistryStoreError("First-run route evidence did not persist exactly")
        route = current_route
    route_key = str(route["route_key"])
    bindings = route["bindings"]
    verification_ids = tuple(str(binding["verification_id"]) for binding in bindings)
    authority_kind = "query_selection"
    if first_run_route is not None:
        authority_kind = "first_run_certification" if selected_name == \
                         "bandit.r005_continuous_hostile_ecology_certification" else "first_run_bootstrap"
    if isinstance(bootstrap_authority, Mapping) and route.get("evidence_state") in {"stale", "unknown"}:
        authority_kind = "current_bootstrap_revalidation"
    if coordinator_authorization is not None:
        authority_kind = "coordinator_brief_charter"
    token_details = {
        "authority_kind": authority_kind,
        "query_id": query_id,
        "query_sha256": query_sha256,
        "manifest_id": manifest["manifest_id"],
        "manifest_revision": manifest["revision"],
        "manifest_sha256": manifest["sha256"],
        "lifecycle": selected.explanation["lifecycle"],
        "selected_values": selected.facts,
        "route_evidence": route,
    }
    if authority_kind == "current_bootstrap_revalidation":
        token_details["bootstrap_authority"] = dict(bootstrap_authority)
    if coordinator_authorization is not None:
        token_details["coordinator_authorization"] = dict(coordinator_authorization)
    token_seed = (
        "caol-scenario-selection-token-v1", query_sha256,
        str(manifest["manifest_id"]), str(manifest["revision"]),
        str(manifest["sha256"]), route_key, _json_text(token_details),
    )
    token_id = _identity(*token_seed)
    prior_terminal = connection.execute(
        "SELECT 1 FROM token_history WHERE token_id = ? "
        "AND event_kind IN ('invalidated', 'verification_run') LIMIT 1", (token_id,)
    ).fetchone()
    if prior_terminal is not None:
        retry_count = connection.execute(
            "SELECT COUNT(*) FROM token_history WHERE manifest_id = ? "
            "AND route_key = ? AND event_kind = 'issued'", (manifest["manifest_id"], route_key)
        ).fetchone()[0]
        token_id = _identity(*token_seed, "retry", str(retry_count))
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
                verification_ids[0] if verification_ids else None,
                route_key,
                _json_text(token_details),
            ),
        )
        selection_id = _append_scenario_selection(
            connection, query_id=query_id, request=request,
            candidate=selected, evaluation=evaluation.evaluation,
            authorization=coordinator_authorization,
        )
    return RegistryQueryExecution(query_id, query_sha256, evaluation, token_id, None, selection_id)


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


def _record_selection_token_rejection(
    connection: sqlite3.Connection,
    *,
    issued: sqlite3.Row,
    reason: str,
    details: Mapping[str, Any],
) -> None:
    """Retain one idempotent launch rejection beside the original token receipt."""
    connection.execute(
        "INSERT OR IGNORE INTO token_history( "
        "token_id, manifest_id, verification_id, route_key, event_kind, reason, details_json "
        ") VALUES( ?, ?, ?, ?, 'invalidated', ?, ? )",
        (
            str(issued["token_id"]),
            str(issued["manifest_id"]),
            issued["verification_id"],
            str(issued["route_key"]),
            f"registry_launch_{reason}",
            _json_text(dict(details)),
        ),
    )


def record_selection_token_rejection(
    connection: sqlite3.Connection,
    token_id: str,
    *,
    reason: str,
    details: Mapping[str, Any],
) -> bool:
    """Record a post-reload adapter rejection without querying or minting a token."""
    with immediate_transaction(connection):
        issued = connection.execute(
            "SELECT token_id, manifest_id, verification_id, route_key FROM token_history "
            "WHERE token_id = ? AND event_kind = 'issued' ORDER BY token_event_id LIMIT 1",
            (token_id,),
        ).fetchone()
        if issued is None:
            return False
        _record_selection_token_rejection(
            connection,
            issued=issued,
            reason=reason,
            details=details,
        )
        return True


def reload_selection_token_for_launch(
    connection: sqlite3.Connection,
    token_id: str,
    *, witness_charter: Optional[Mapping[str, Any]] = None,
) -> RegistryLaunchToken:
    """Atomically reload a selection receipt and reject any changed launch owner."""
    token_id = str(token_id).strip()
    if not token_id:
        return RegistryLaunchToken(token_id="", accepted=False, reason="token_missing")

    with immediate_transaction(connection):
        issued = connection.execute(
            "SELECT token_id, manifest_id, verification_id, route_key, details_json FROM token_history "
            "WHERE token_id = ? AND event_kind = 'issued' ORDER BY token_event_id LIMIT 1",
            (token_id,),
        ).fetchone()
        if issued is None:
            repair = _repair_token_details(connection, token_id)
            if repair is not None:
                _record_repair_token_rejection(
                    connection,
                    issued=repair,
                    reason="ordinary_registry_launch",
                    details={},
                )
                return RegistryLaunchToken(
                    token_id=token_id,
                    accepted=False,
                    reason="repair_token_requires_repair_launch",
                )
            return RegistryLaunchToken(token_id=token_id, accepted=False, reason="token_unknown")

        def reject(reason: str, **details: Any) -> RegistryLaunchToken:
            _record_selection_token_rejection(
                connection,
                issued=issued,
                reason=reason,
                details=details,
            )
            return RegistryLaunchToken(token_id=token_id, accepted=False, reason=reason)

        invalidation = connection.execute(
            "SELECT reason FROM token_history WHERE token_id = ? AND event_kind = 'invalidated' "
            "ORDER BY token_event_id LIMIT 1",
            (token_id,),
        ).fetchone()
        if invalidation is not None:
            return reject("token_invalidated", prior_reason=str(invalidation["reason"]))

        try:
            receipt = _json_object(str(issued["details_json"]), "selection token details")
            expected_manifest_id = _string(receipt.get("manifest_id"), "selection token manifest_id")
            expected_revision = receipt.get("manifest_revision")
            expected_sha256 = _string(receipt.get("manifest_sha256"), "selection token manifest_sha256").lower()
            expected_route = _object(receipt.get("route_evidence"), "selection token route_evidence")
            authority_kind = str(receipt.get("authority_kind", "query_selection"))
            expected_bootstrap_authority = receipt.get("bootstrap_authority")
            expected_coordinator = receipt.get("coordinator_authorization")
        except ScenarioRegistryStoreError as exc:
            return reject("receipt_malformed", error=str(exc))
        if type(expected_revision) is not int:
            return reject("receipt_malformed", error="selection token manifest_revision must be an integer")
        if expected_manifest_id != str(issued["manifest_id"]):
            return reject("receipt_manifest_mismatch")
        if str(expected_route.get("route_key", "")) != str(issued["route_key"]):
            return reject("receipt_route_mismatch")

        if authority_kind == "coordinator_brief_charter":
            if not isinstance(expected_coordinator, Mapping) or not isinstance(witness_charter, Mapping):
                return reject("coordinator_charter_missing")
            try:
                from playtest_witness import normalize_witness_charter
                current_charter = normalize_witness_charter(witness_charter)
            except (ValueError, WitnessError):
                return reject("coordinator_charter_invalid")
            if _canonical_hash(current_charter, "caol-witness-charter-v1") != \
                    str(expected_coordinator.get("charter_sha256", "")):
                return reject("coordinator_charter_stale")
            if str(expected_route.get("route_key", "")) != \
                    "coordinator:" + str(expected_coordinator.get("charter_sha256", "")):
                return reject("coordinator_route_mismatch")

        manifest = connection.execute(
            "SELECT source_path, present, revision, current_sha256, declaration_json FROM manifest_current "
            "WHERE manifest_id = ?",
            (expected_manifest_id,),
        ).fetchone()
        if manifest is None:
            return reject("manifest_missing")
        if not bool(manifest["present"]):
            return reject("manifest_absent")
        if int(manifest["revision"]) != expected_revision:
            return reject("manifest_revision_changed", current_revision=int(manifest["revision"]))
        if str(manifest["current_sha256"] or "").lower() != expected_sha256:
            return reject("manifest_sha256_changed")

        source_path = Path(str(manifest["source_path"]))
        try:
            source_sha256 = hashlib.sha256(source_path.read_bytes()).hexdigest()
        except OSError as exc:
            return reject("manifest_source_unreadable", error=str(exc))
        if source_sha256 != expected_sha256:
            return reject("manifest_source_changed", source_sha256=source_sha256)

        try:
            declaration = _json_object(str(manifest["declaration_json"]), "current manifest declaration")
        except ScenarioRegistryStoreError as exc:
            return reject("manifest_declaration_malformed", error=str(exc))
        scenario = source_path.stem
        if not scenario or not isinstance(declaration.get("name", ""), str):
            return reject("manifest_scenario_unavailable")

        if authority_kind == "coordinator_brief_charter":
            current_routes = _current_route_evidence(connection, expected_manifest_id)
            lifecycle, lifecycle_reason = _current_lifecycle_state(
                connection, manifest_id=expected_manifest_id, present=bool(manifest["present"]),
                route_evidence=current_routes,
            )
            if declaration.get("name") == "cannibal.r029_natural_route_roof_mcw" and current_routes and (\
                    all(str(route.get("evidence_state", "")) == "stale" for route in current_routes) or
                    lifecycle_reason in {"route_contradicted", "route_stale", "quarantine_history"}):
                # The charter is authorizing this current manifest revision;
                # stale or contradicted prior runs remain audit history rather
                # than preventing its first fresh lifecycle witness from
                # being launched.
                lifecycle = "active"
            review = _exclusive_source_review_state(
                connection, manifest_id=expected_manifest_id, source_path=str(source_path),
                revision=int(manifest["revision"]), source_sha256=expected_sha256,
                declaration=declaration,
            )
            if lifecycle != "active" or not bool(review.get("executable")):
                return reject("coordinator_candidate_not_current")
            if not bool(expected_coordinator.get("charter_id")) or \
                    not str(expected_coordinator.get("outcome", "")).strip():
                return reject("coordinator_authorization_malformed")
            return RegistryLaunchToken(
                token_id=token_id, accepted=True, reason=authority_kind,
                scenario=scenario, source_path=str(source_path.resolve()),
            )

        if authority_kind in {"first_run_certification", "first_run_bootstrap"}:
            snapshot = RegistryQueryCandidateSnapshot(
                scenario_id=expected_manifest_id,
                lifecycle_state="active",
                token_eligible=True,
                facts={key: {"value": value} for key, value in declaration.get("capabilities", {}).items()},
                explanation={"manifest": {
                    "name": declaration.get("name"),
                    "source_path": str(source_path),
                    "sha256": expected_sha256,
                    "validation": {"status": "valid", "review_required": False},
                }},
            )
            expected_first_run_route = _first_run_certification_route(snapshot)
            if expected_first_run_route is None:
                return reject("first_run_manifest_not_authorized")
            expected_first_run_route = {
                **dict(expected_first_run_route),
                "details": {
                    **dict(expected_first_run_route["details"]),
                    "source_path": str(source_path),
                },
            }
            current_routes = _current_route_evidence(connection, expected_manifest_id)
            current_route_matches = tuple(
                route for route in current_routes if str(route.get("route_key", "")) == str(issued["route_key"])
            )
            if not current_route_matches:
                return reject("route_missing")
            if len(current_route_matches) != 1:
                return reject("route_ambiguous")
            current_route = current_route_matches[0]
            if _json_text(expected_route) != _json_text(expected_first_run_route):
                return reject("receipt_first_run_route_mismatch")
            if _json_text(current_route) != _json_text(expected_first_run_route):
                return reject("first_run_route_binding_changed")
            return RegistryLaunchToken(
                token_id=token_id,
                accepted=True,
                reason=authority_kind,
                scenario=scenario,
                source_path=str(source_path.resolve()),
            )

        current_routes = _current_route_evidence(connection, expected_manifest_id)
        current_route_matches = tuple(
            route for route in current_routes if str(route.get("route_key", "")) == str(issued["route_key"])
        )
        if not current_route_matches:
            return reject("route_missing")
        if len(current_route_matches) != 1:
            return reject("route_ambiguous")
        current_route = current_route_matches[0]
        if authority_kind == "current_bootstrap_revalidation":
            if not isinstance(expected_bootstrap_authority, Mapping):
                return reject("receipt_malformed", error="bootstrap authority is missing")
            current_authority = _current_bootstrap_revalidation(
                connection, manifest_id=expected_manifest_id, route_evidence=current_routes,
            )
            if current_authority is None or _json_text(current_authority) != _json_text(expected_bootstrap_authority):
                return reject("bootstrap_authority_changed")
            if current_route.get("evidence_state") not in {"stale", "unknown"}:
                return reject("bootstrap_route_no_longer_revalidatable")
            return RegistryLaunchToken(
                token_id=token_id,
                accepted=True,
                reason="current_bootstrap_authority",
                scenario=scenario,
                source_path=str(source_path.resolve()),
            )
        current_authoritative_route = _authoritative_current_route(current_route)
        if current_authoritative_route is None:
            return reject("route_binding_ineligible")
        if _json_text(current_authoritative_route) != _json_text(expected_route):
            return reject("route_binding_changed")
        bindings = current_authoritative_route.get("bindings", ())
        if (
            not isinstance(bindings, Sequence)
            or not bindings
            or not all(
                isinstance(binding, Mapping) and binding.get("resolution") == "compatible"
                for binding in bindings
            )
            or not any(
                str(binding.get("verification_id", "")) == str(issued["verification_id"])
                for binding in bindings
            )
        ):
            return reject("route_binding_ineligible")

        verification = connection.execute(
            "SELECT verification_id FROM verification_history WHERE verification_id = ? "
            "AND manifest_id = ? AND route_key = ?",
            (issued["verification_id"], expected_manifest_id, issued["route_key"]),
        ).fetchone()
        if verification is None:
            return reject("verification_missing")
        return RegistryLaunchToken(
            token_id=token_id,
            accepted=True,
            reason="current",
            scenario=scenario,
            source_path=str(source_path.resolve()),
        )


def _bootstrap_runtime_binding(raw: Any) -> Mapping[str, Any]:
    if not isinstance(raw, Mapping) or raw.get("schema") != 1:
        raise ScenarioRegistryStoreError("bootstrap runtime binding is invalid")
    required = ("executable_path", "executable_sha256", "runtime_source_sha256")
    binding = {key: str(raw.get(key, "")).strip() for key in required}
    if not all(binding.values()):
        raise ScenarioRegistryStoreError("bootstrap runtime binding is incomplete")
    return {"schema": 1, **binding}


def _bootstrap_token_details(
    connection: sqlite3.Connection,
    token_id: str,
) -> Optional[sqlite3.Row]:
    return connection.execute(
        "SELECT token_id, manifest_id, route_key, details_json FROM token_history "
        "WHERE token_id = ? AND event_kind = 'bootstrap_issued' ORDER BY token_event_id LIMIT 1",
        (token_id,),
    ).fetchone()


def issue_registry_bootstrap_token(
    connection: sqlite3.Connection,
    request: RegistryQueryRequest,
    *,
    runtime_binding: Mapping[str, Any],
) -> RegistryBootstrapToken:
    """Mint a distinct authority for one first compatible evidence run only."""
    runtime = _bootstrap_runtime_binding(runtime_binding)
    request_json = _query_request_json(request)
    query_sha256 = hashlib.sha256(request_json.encode("utf-8")).hexdigest()
    selected = _select_registry_bootstrap_candidate(connection, request)
    if selected is None:
        return RegistryBootstrapToken("", False, "query_has_no_active_compatible_manifest")
    if _current_verified_route(selected) is not None:
        return RegistryBootstrapToken("", False, "compatible_run_evidence_already_exists")
    manifest = selected.explanation["manifest"]
    manifest_id = str(manifest["manifest_id"])
    manifest_sha256 = str(manifest["sha256"])
    token_details = {
        "authority_kind": "registry_bootstrap_first_compatible_run",
        "query_json": json.loads(request_json),
        "query_sha256": query_sha256,
        "manifest_id": manifest_id,
        "manifest_sha256": manifest_sha256,
        "runtime_binding": runtime,
    }
    token_id = _identity(
        "caol-scenario-bootstrap-token-v1",
        manifest_id,
        manifest_sha256,
        query_sha256,
        _json_text(runtime),
    )
    with immediate_transaction(connection):
        existing = _bootstrap_token_details(connection, token_id)
        if existing is not None and connection.execute(
                "SELECT 1 FROM token_history WHERE token_id = ? "
                "AND event_kind IN ( 'bootstrap_claimed', 'bootstrap_invalidated' ) LIMIT 1",
                (token_id,)).fetchone() is not None:
            retry_ordinal = connection.execute(
                "SELECT COUNT(*) FROM token_history WHERE manifest_id = ? AND route_key = ? "
                "AND event_kind = 'bootstrap_issued'",
                (manifest_id, "bootstrap:" + query_sha256),
            ).fetchone()[0]
            token_id = _identity(
                "caol-scenario-bootstrap-token-v1",
                manifest_id,
                manifest_sha256,
                query_sha256,
                _json_text(runtime),
                "retry",
                str(retry_ordinal),
            )
            existing = _bootstrap_token_details(connection, token_id)
        if existing is None:
            connection.execute(
                "INSERT INTO token_history( token_id, manifest_id, verification_id, route_key, event_kind, reason, details_json ) "
                "VALUES( ?, ?, NULL, ?, 'bootstrap_issued', 'first_compatible_evidence_run', ? )",
                (token_id, manifest_id, "bootstrap:" + query_sha256, _json_text(token_details)),
            )
            _append_query_audit(
                connection,
                query_id=_identity("caol-scenario-bootstrap-query-v1", query_sha256),
                request_json=request_json,
                result={"kind": "bootstrap", "token_id": token_id, "selected_scenario_id": selected.scenario_id},
            )
    return RegistryBootstrapToken(
        token_id, True, "issued", Path(str(manifest["source_path"])).stem,
        str(manifest["source_path"]), runtime,
    )


def record_bootstrap_token_rejection(
    connection: sqlite3.Connection,
    token_id: str,
    *,
    reason: str,
    details: Mapping[str, Any],
) -> bool:
    with immediate_transaction(connection):
        issued = _bootstrap_token_details(connection, str(token_id).strip())
        if issued is None:
            return False
        connection.execute(
            "INSERT OR IGNORE INTO token_history( token_id, manifest_id, verification_id, route_key, event_kind, reason, details_json ) "
            "VALUES( ?, ?, NULL, ?, 'bootstrap_invalidated', ?, ? )",
            (str(issued["token_id"]), str(issued["manifest_id"]), str(issued["route_key"]), reason, _json_text(dict(details))),
        )
        return True


def reload_bootstrap_token_for_launch(
    connection: sqlite3.Connection,
    token_id: str,
    *,
    require_claimed: bool = False,
) -> RegistryBootstrapToken:
    """Revalidate a bootstrap receipt without treating it as a normal selection token."""
    token_id = str(token_id).strip()
    if not token_id:
        return RegistryBootstrapToken("", False, "token_missing")
    with immediate_transaction(connection):
        issued = _bootstrap_token_details(connection, token_id)
        if issued is None:
            return RegistryBootstrapToken(token_id, False, "token_unknown")

        def reject(reason: str, **details: Any) -> RegistryBootstrapToken:
            connection.execute(
                "INSERT OR IGNORE INTO token_history( token_id, manifest_id, verification_id, route_key, event_kind, reason, details_json ) "
                "VALUES( ?, ?, NULL, ?, 'bootstrap_invalidated', ?, ? )",
                (token_id, str(issued["manifest_id"]), str(issued["route_key"]), reason, _json_text(details)),
            )
            return RegistryBootstrapToken(token_id, False, reason)

        if connection.execute(
                "SELECT 1 FROM token_history WHERE token_id = ? AND event_kind = 'bootstrap_invalidated' LIMIT 1",
                (token_id,)).fetchone() is not None:
            return reject("token_invalidated")
        claimed = connection.execute(
            "SELECT 1 FROM token_history WHERE token_id = ? AND event_kind = 'bootstrap_claimed' LIMIT 1",
            (token_id,)).fetchone() is not None
        if claimed != require_claimed:
            return reject("token_already_claimed" if claimed else "token_not_claimed")
        try:
            receipt = _json_object(str(issued["details_json"]), "bootstrap token details")
            runtime = _bootstrap_runtime_binding(receipt.get("runtime_binding"))
            request = parse_registry_query_request(receipt.get("query_json"))
            expected_query_sha256 = _string(receipt.get("query_sha256"), "bootstrap token query_sha256")
            expected_manifest_id = _string(receipt.get("manifest_id"), "bootstrap token manifest_id")
            expected_sha256 = _string(receipt.get("manifest_sha256"), "bootstrap token manifest_sha256")
        except (ScenarioRegistryStoreError, ScenarioRegistryQueryError) as exc:
            return reject("receipt_malformed", error=str(exc))
        if expected_manifest_id != str(issued["manifest_id"]):
            return reject("receipt_manifest_mismatch")
        if hashlib.sha256(_query_request_json(request).encode("utf-8")).hexdigest() != expected_query_sha256:
            return reject("query_sha256_changed")
        if str(issued["route_key"]) != "bootstrap:" + expected_query_sha256:
            return reject("query_route_mismatch")
        manifest = connection.execute(
            "SELECT source_path, present, current_sha256 FROM manifest_current WHERE manifest_id = ?",
            (expected_manifest_id,),
        ).fetchone()
        if manifest is None or not bool(manifest["present"]):
            return reject("manifest_absent")
        if str(manifest["current_sha256"] or "") != expected_sha256:
            return reject("manifest_sha256_changed")
        try:
            observed_sha256 = hashlib.sha256(Path(str(manifest["source_path"])).read_bytes()).hexdigest()
        except OSError as exc:
            return reject("manifest_source_unreadable", error=str(exc))
        if observed_sha256 != expected_sha256:
            return reject("manifest_source_changed")
        selected = _select_registry_bootstrap_candidate(connection, request)
        if selected is None or selected.scenario_id != expected_manifest_id:
            return reject("query_authority_changed")
        if _current_verified_route(selected) is not None:
            return reject("compatible_run_evidence_already_exists")
        return RegistryBootstrapToken(
            token_id, True, "claimed" if claimed else "current", Path(str(manifest["source_path"])).stem,
            str(manifest["source_path"]), runtime,
        )


def claim_bootstrap_token_for_launch(
    connection: sqlite3.Connection,
    token_id: str,
) -> RegistryBootstrapToken:
    """Atomically consume the bootstrap authority before the sole canonical probe begins."""
    selection = reload_bootstrap_token_for_launch(connection, token_id)
    if not selection.accepted:
        return selection
    with immediate_transaction(connection):
        issued = _bootstrap_token_details(connection, selection.token_id)
        if issued is None:
            return RegistryBootstrapToken(selection.token_id, False, "token_unknown")
        connection.execute(
            "INSERT INTO token_history( token_id, manifest_id, verification_id, route_key, event_kind, reason, details_json ) "
            "VALUES( ?, ?, NULL, ?, 'bootstrap_claimed', 'canonical_probe_launch', '{}' )",
            (selection.token_id, str(issued["manifest_id"]), str(issued["route_key"])),
        )
    return reload_bootstrap_token_for_launch(connection, selection.token_id, require_claimed=True)


def _repair_token_details(
    connection: sqlite3.Connection,
    token_id: str,
) -> Optional[sqlite3.Row]:
    return connection.execute(
        "SELECT token_id, manifest_id, verification_id, route_key, details_json FROM token_history "
        "WHERE token_id = ? AND event_kind = 'repair_issued' ORDER BY token_event_id LIMIT 1",
        (token_id,),
    ).fetchone()


def _repair_binding(
    raw: Any,
    declaration: Mapping[str, Any],
) -> Mapping[str, Any]:
    """Normalize the live footing a repair receipt must retain exactly."""
    if not isinstance(raw, Mapping):
        raise ScenarioRegistryStoreError("repair binding must be an object")
    runtime = _bootstrap_runtime_binding(raw.get("runtime"))
    normalized: Dict[str, Mapping[str, Any]] = {"runtime": runtime}
    for kind, name_key, profile_key in (
            ("fixture", "fixture", "fixture_profile"),
            ("profile", "profile_snapshot", "profile_snapshot_profile")):
        item = raw.get(kind)
        if not isinstance(item, Mapping) or item.get("status", "compatible") != "compatible":
            raise ScenarioRegistryStoreError(f"repair {kind} binding is not compatible")
        expected_name = str(declaration.get(name_key, "")).strip()
        expected_profile = str(declaration.get(profile_key, "")).strip()
        actual_name = str(item.get("name", "")).strip()
        actual_profile = str(item.get("profile", "")).strip()
        source_path = str(item.get("source_path", "")).strip()
        source_sha256 = str(item.get("source_sha256", "")).strip().lower()
        if actual_name != expected_name or actual_profile != expected_profile:
            raise ScenarioRegistryStoreError(f"repair {kind} identity changed")
        if len(source_sha256) != 64 or any(
                character not in "0123456789abcdef" for character in source_sha256):
            raise ScenarioRegistryStoreError(f"repair {kind} binding is incomplete")
        if expected_name and not source_path:
            raise ScenarioRegistryStoreError(f"repair {kind} binding is incomplete")
        normalized[kind] = {
            "name": expected_name,
            "profile": expected_profile,
            "source_path": source_path,
            "source_sha256": source_sha256,
        }
    return normalized


def _repair_route_current(
    connection: sqlite3.Connection,
    *,
    manifest_id: str,
    route_key: str,
    red_verification_id: str,
) -> bool:
    routes = _current_route_evidence(connection, manifest_id)
    route = next((item for item in routes if str(item.get("route_key", "")) == route_key), None)
    if route is None:
        return False
    details = route.get("details", {})
    if route.get("evidence_state") == "contradicted":
        red_ids = details.get("unresolved_contradiction_ids", ()) if isinstance(details, Mapping) else ()
    elif route.get("evidence_state") == "stale":
        red_ids = _stale_repairable_red_ids(
            connection, manifest_id=manifest_id, route_key=route_key,
        )
    else:
        return False
    if red_verification_id not in {str(item) for item in red_ids}:
        return False
    red = connection.execute(
        "SELECT verification_id, manifest_id, route_key, details_json FROM verification_history "
        "WHERE verification_id = ?",
        (red_verification_id,),
    ).fetchone()
    return bool(
        red is not None
        and str(red["manifest_id"]) == manifest_id
        and str(red["route_key"]) == route_key
        and _verification_evidence_state(red) == "contradicted"
    )


def _stale_repairable_red_ids(
    connection: sqlite3.Connection,
    *,
    manifest_id: str,
    route_key: str,
) -> Tuple[str, ...]:
    """Return unsuperseded stale contradictions for a current-source repair."""
    rows = connection.execute(
        "SELECT verification_id, details_json FROM verification_history AS candidate "
        "WHERE manifest_id = ? AND route_key = ? AND NOT EXISTS ("
        "SELECT 1 FROM verification_history AS successor "
        "WHERE successor.supersedes_verification_id = candidate.verification_id) "
        "ORDER BY recorded_at, verification_id",
        (manifest_id, route_key),
    ).fetchall()
    stale_red_ids = []
    for row in rows:
        verification_id = str(row["verification_id"])
        if _verification_evidence_state(row) != "contradicted":
            continue
        resolution = connection.execute(
            "SELECT resolution_kind FROM verification_resolution_history WHERE verification_id = ? "
            "ORDER BY resolution_event_id DESC LIMIT 1",
            (verification_id,),
        ).fetchone()
        if resolution is not None and str(resolution["resolution_kind"]) == "stale":
            stale_red_ids.append(verification_id)
    return tuple(stale_red_ids)


def _r019_stale_repairable_red_ids(
    connection: sqlite3.Connection,
    *,
    manifest_id: str,
    route_key: str,
) -> Tuple[str, ...]:
    """Return stale R-019 reds reserved for its successor terminal."""
    declaration = connection.execute(
        "SELECT declaration_json FROM manifest_current WHERE manifest_id = ?", (manifest_id,)
    ).fetchone()
    if declaration is None or _json_object(
            str(declaration["declaration_json"]), "R-019 stale repair declaration"
    ).get("name") != "r019.keep_watch_acceptance_mcw":
        return ()
    return _stale_repairable_red_ids(
        connection, manifest_id=manifest_id, route_key=route_key,
    )


def _repair_query_matches_manifest(
    connection: sqlite3.Connection,
    *,
    manifest_id: str,
    request: RegistryQueryRequest,
) -> bool:
    snapshots = build_registry_query_candidate_snapshot(
        connection,
        include_lifecycle_states=("quarantined",),
        manifest_ids=(manifest_id,),
    )
    snapshot = next((item for item in snapshots if item.scenario_id == manifest_id), None)
    if snapshot is None:
        return False
    # This is only an exact-value query binding check.  It never makes the
    # contradicted route selectable: issuance still requires that contradiction.
    lowered_facts = {
        key: {
            **dict(fact),
            "evidence_state": (
                "declared" if fact.get("evidence_state") == "stale"
                else "run-verified" if fact.get("evidence_state") == "contradicted"
                else fact.get("evidence_state")
            ),
            "proof_depth": None if fact.get("evidence_state") in {"contradicted", "stale"}
            else fact.get("proof_depth"),
        }
        for key, fact in snapshot.facts.items()
    }
    evaluation = evaluate_registry_query(
        request, ({"scenario_id": manifest_id, "facts": lowered_facts},),
    )
    return bool(evaluation.candidates and evaluation.candidates[0].hard_valid)


def _record_repair_token_rejection(
    connection: sqlite3.Connection,
    *,
    issued: sqlite3.Row,
    reason: str,
    details: Mapping[str, Any],
) -> None:
    connection.execute(
        "INSERT OR IGNORE INTO token_history( "
        "token_id, manifest_id, verification_id, route_key, event_kind, reason, details_json "
        ") VALUES( ?, ?, ?, ?, 'repair_invalidated', ?, ? )",
        (
            str(issued["token_id"]), str(issued["manifest_id"]), issued["verification_id"],
            str(issued["route_key"]), reason, _json_text(dict(details)),
        ),
    )


def record_repair_token_rejection(
    connection: sqlite3.Connection,
    token_id: str,
    *,
    reason: str,
    details: Mapping[str, Any],
) -> bool:
    with immediate_transaction(connection):
        issued = _repair_token_details(connection, str(token_id).strip())
        if issued is None:
            return False
        _record_repair_token_rejection(connection, issued=issued, reason=reason, details=details)
        return True


_REPAIR_ACCEPTED_CLEANUP_STATUSES = frozenset({
    "already_exited",
    "terminated",
    "terminated_during_kill_escalation",
    "killed",
})


def terminalize_repair_token_cleanup_without_report(
    connection: sqlite3.Connection,
    token_id: str,
    *,
    run_dir: Path,
    cleanup: Mapping[str, Any],
) -> RegistryRepairToken:
    """Append one claimed repair's verified cleanup when no durable report exists."""
    token_id = str(token_id).strip()
    resolved_run_dir = run_dir.resolve()
    if not token_id:
        return RegistryRepairToken("", False, "token_missing")
    if not resolved_run_dir.is_dir():
        return RegistryRepairToken(token_id, False, "run_dir_absent")
    if (resolved_run_dir / "probe.report.json").exists():
        return RegistryRepairToken(token_id, False, "report_present")
    cleanup_status = str(cleanup.get("status", "")).strip()
    if cleanup_status not in _REPAIR_ACCEPTED_CLEANUP_STATUSES:
        return RegistryRepairToken(token_id, False, "cleanup_not_accepted")
    with immediate_transaction(connection):
        issued = _repair_token_details(connection, token_id)
        if issued is None:
            return RegistryRepairToken(token_id, False, "token_unknown")
        terminal = connection.execute(
            "SELECT 1 FROM token_history WHERE token_id = ? AND event_kind = 'repair_no_report_terminal' "
            "LIMIT 1",
            (token_id,),
        ).fetchone()
        if terminal is not None:
            return RegistryRepairToken(token_id, True, "cleanup_no_report_terminal")
        claimed = connection.execute(
            "SELECT 1 FROM token_history WHERE token_id = ? AND event_kind = 'repair_claimed' LIMIT 1",
            (token_id,),
        ).fetchone() is not None
        if not claimed:
            return RegistryRepairToken(token_id, False, "token_not_claimed")
        if connection.execute(
                "SELECT 1 FROM token_history WHERE token_id = ? AND event_kind = 'repair_verification_run' LIMIT 1",
                (token_id,),
        ).fetchone() is not None:
            return RegistryRepairToken(token_id, False, "report_disposition_exists")
        if connection.execute(
                "SELECT 1 FROM token_history WHERE token_id = ? AND event_kind = 'repair_invalidated' LIMIT 1",
                (token_id,),
        ).fetchone() is not None:
            return RegistryRepairToken(token_id, False, "token_invalidated")
        details = {
            "run_dir": str(resolved_run_dir),
            "cleanup": dict(cleanup),
            "report_path": str((resolved_run_dir / "probe.report.json").resolve()),
            "report_present": False,
        }
        connection.execute(
            "INSERT INTO token_history( token_id, manifest_id, verification_id, route_key, event_kind, reason, details_json ) "
            "VALUES( ?, ?, ?, ?, 'repair_no_report_terminal', 'canonical_cleanup_no_report', ? )",
            (
                token_id, str(issued["manifest_id"]), issued["verification_id"], str(issued["route_key"]),
                _json_text(details),
            ),
        )
        _record_repair_token_rejection(
            connection,
            issued=issued,
            reason="cleanup_no_report_terminal",
            details=details,
        )
    return RegistryRepairToken(token_id, True, "cleanup_no_report_terminal")


def _repair_authority_id(
    *,
    manifest_id: str,
    manifest_revision: int,
    manifest_sha256: str,
    route_key: str,
    red_verification_id: str,
    query_sha256: str,
    binding: Mapping[str, Any],
) -> str:
    """Identify the immutable contradiction authority across retry attempts."""
    return _identity(
        "caol-scenario-repair-authority-v1", manifest_id, str(manifest_revision), manifest_sha256,
        route_key, red_verification_id, query_sha256, _json_text(binding),
    )


def _repair_attempts_for_authority(
    connection: sqlite3.Connection,
    *,
    authority_id: str,
    legacy_token_id: str,
) -> list[sqlite3.Row]:
    """Read the append-only attempts, retaining the pre-sequence first authority."""
    attempts: list[sqlite3.Row] = []
    for issued in connection.execute(
            "SELECT token_event_id, token_id, manifest_id, verification_id, route_key, details_json "
            "FROM token_history WHERE event_kind = 'repair_issued' ORDER BY token_event_id"
    ).fetchall():
        try:
            details = _json_object(str(issued["details_json"]), "repair token details")
        except ScenarioRegistryStoreError:
            continue
        if str(issued["token_id"]) == legacy_token_id or details.get("authority_id") == authority_id:
            attempts.append(issued)
    return attempts


def issue_registry_repair_token(
    connection: sqlite3.Connection,
    request: RegistryQueryRequest,
    *,
    manifest_id: str,
    route_key: str,
    red_verification_id: str,
    binding: Mapping[str, Any],
) -> RegistryRepairToken:
    """Issue a distinct authority for exactly one current unresolved contradiction."""
    manifest = connection.execute(
        "SELECT source_path, present, revision, current_sha256, declaration_json, validation_json "
        "FROM manifest_current WHERE manifest_id = ?",
        (manifest_id,),
    ).fetchone()
    if manifest is None or not bool(manifest["present"]):
        return RegistryRepairToken("", False, "manifest_absent")
    validation = _json_object(str(manifest["validation_json"]), "repair manifest validation")
    if validation.get("status") != "valid" or bool(validation.get("review_required")):
        return RegistryRepairToken("", False, "manifest_not_current_valid")
    declaration = _json_object(str(manifest["declaration_json"]), "repair manifest declaration")
    try:
        current_binding = _repair_binding(binding, declaration)
    except ScenarioRegistryStoreError as exc:
        return RegistryRepairToken("", False, "binding_invalid:" + str(exc))
    source_path = Path(str(manifest["source_path"]))
    try:
        source_sha256 = hashlib.sha256(source_path.read_bytes()).hexdigest()
    except OSError:
        return RegistryRepairToken("", False, "manifest_source_unreadable")
    if source_sha256 != str(manifest["current_sha256"] or "").lower():
        return RegistryRepairToken("", False, "manifest_source_changed")
    if not _repair_route_current(
            connection, manifest_id=manifest_id, route_key=route_key,
            red_verification_id=red_verification_id):
        return RegistryRepairToken("", False, "route_not_current_contradiction")
    if not _repair_query_matches_manifest(connection, manifest_id=manifest_id, request=request):
        return RegistryRepairToken("", False, "query_not_matched_by_contradicted_manifest")
    request_json = _query_request_json(request)
    stale_r019_successor = red_verification_id in _r019_stale_repairable_red_ids(
        connection, manifest_id=manifest_id, route_key=route_key,
    )
    details = {
        "authority_kind": (
            "registry_repair_r019_current_source_successor"
            if stale_r019_successor else "registry_repair_exact_contradiction"
        ),
        "query_json": json.loads(request_json),
        "query_sha256": hashlib.sha256(request_json.encode("utf-8")).hexdigest(),
        "manifest_id": manifest_id,
        "manifest_revision": int(manifest["revision"]),
        "manifest_sha256": source_sha256,
        "source_path": str(source_path.resolve()),
        "route_key": route_key,
        "red_verification_id": red_verification_id,
        "binding": current_binding,
    }
    authority_id = _repair_authority_id(
        manifest_id=manifest_id,
        manifest_revision=int(manifest["revision"]),
        manifest_sha256=source_sha256,
        route_key=route_key,
        red_verification_id=red_verification_id,
        query_sha256=str(details["query_sha256"]),
        binding=current_binding,
    )
    legacy_token_id = _identity(
        "caol-scenario-repair-token-v1", manifest_id, str(manifest["revision"]), source_sha256,
        route_key, red_verification_id, details["query_sha256"], _json_text(current_binding),
    )
    with immediate_transaction(connection):
        attempts = _repair_attempts_for_authority(
            connection, authority_id=authority_id, legacy_token_id=legacy_token_id,
        )
        predecessor: Mapping[str, Any] | None = None
        attempt_sequence = 1
        if attempts:
            current = attempts[-1]
            current_token_id = str(current["token_id"])
            invalidation = connection.execute(
                "SELECT token_event_id, reason, details_json FROM token_history "
                "WHERE token_id = ? AND event_kind = 'repair_invalidated' "
                "ORDER BY token_event_id DESC LIMIT 1",
                (current_token_id,),
            ).fetchone()
            claimed = connection.execute(
                "SELECT 1 FROM token_history WHERE token_id = ? AND event_kind = 'repair_claimed' LIMIT 1",
                (current_token_id,),
            ).fetchone() is not None
            if invalidation is None:
                if claimed:
                    return RegistryRepairToken(
                        current_token_id, False, "token_already_claimed", source_path.stem,
                        str(source_path.resolve()), current_binding["runtime"],
                    )
                return RegistryRepairToken(
                    current_token_id, True, "current", source_path.stem,
                    str(source_path.resolve()), current_binding["runtime"],
                )
            current_details = _json_object(str(current["details_json"]), "repair token details")
            prior_sequence = current_details.get("attempt_sequence", 1)
            if type(prior_sequence) is not int or prior_sequence < 1:
                return RegistryRepairToken(current_token_id, False, "attempt_sequence_malformed")
            attempt_sequence = prior_sequence + 1
            predecessor = {
                "token_id": current_token_id,
                "attempt_sequence": prior_sequence,
                "terminal_event": {
                    "token_event_id": int(invalidation["token_event_id"]),
                    "event_kind": "repair_invalidated",
                    "reason": str(invalidation["reason"]),
                    "details": _json_object(str(invalidation["details_json"]), "repair invalidation details"),
                },
            }
        token_id = legacy_token_id if attempt_sequence == 1 else _identity(
            "caol-scenario-repair-token-v2", authority_id, str(attempt_sequence),
        )
        details["authority_id"] = authority_id
        details["attempt_sequence"] = attempt_sequence
        if predecessor is not None:
            details["predecessor"] = predecessor
        if _repair_token_details(connection, token_id) is None:
            connection.execute(
                "INSERT INTO token_history( token_id, manifest_id, verification_id, route_key, event_kind, reason, details_json ) "
                "VALUES( ?, ?, ?, ?, 'repair_issued', 'exact_contradiction_repair', ? )",
                (token_id, manifest_id, red_verification_id, route_key, _json_text(details)),
            )
            _append_query_audit(
                connection,
                query_id=_identity("caol-scenario-repair-query-v1", details["query_sha256"], str(attempt_sequence)),
                request_json=request_json,
                result={"kind": "repair", "token_id": token_id, "manifest_id": manifest_id,
                        "route_key": route_key, "red_verification_id": red_verification_id,
                        "attempt_sequence": attempt_sequence},
            )
    return RegistryRepairToken(
        token_id, True, "issued", source_path.stem, str(source_path.resolve()), current_binding["runtime"],
    )


def reload_repair_token_for_launch(
    connection: sqlite3.Connection,
    token_id: str,
    *,
    require_claimed: bool = False,
    binding: Mapping[str, Any] | None = None,
) -> RegistryRepairToken:
    """Revalidate the exact contradiction and all receipt identities before repair launch."""
    token_id = str(token_id).strip()
    if not token_id:
        return RegistryRepairToken("", False, "token_missing")
    with immediate_transaction(connection):
        issued = _repair_token_details(connection, token_id)
        if issued is None:
            return RegistryRepairToken(token_id, False, "token_unknown")

        def reject(reason: str, **details: Any) -> RegistryRepairToken:
            _record_repair_token_rejection(connection, issued=issued, reason=reason, details=details)
            return RegistryRepairToken(token_id, False, reason)

        invalidation = connection.execute(
            "SELECT token_event_id, reason, details_json FROM token_history "
            "WHERE token_id = ? AND event_kind = 'repair_invalidated' "
            "ORDER BY token_event_id DESC LIMIT 1",
            (token_id,),
        ).fetchone()
        if invalidation is not None:
            return reject(
                "token_invalidated",
                prior_terminal_event={
                    "token_event_id": int(invalidation["token_event_id"]),
                    "event_kind": "repair_invalidated",
                    "reason": str(invalidation["reason"]),
                    "details": _json_object(
                        str(invalidation["details_json"]), "repair invalidation details",
                    ),
                },
            )
        claimed = connection.execute(
            "SELECT 1 FROM token_history WHERE token_id = ? AND event_kind = 'repair_claimed' LIMIT 1",
            (token_id,)).fetchone() is not None
        if claimed != require_claimed:
            return reject(
                "token_already_claimed" if claimed else "token_not_claimed",
                required_claimed=require_claimed,
                observed_claimed=claimed,
            )
        try:
            receipt = _json_object(str(issued["details_json"]), "repair token details")
            request = parse_registry_query_request(receipt.get("query_json"))
            expected_manifest_id = _string(receipt.get("manifest_id"), "repair token manifest_id")
            expected_revision = receipt.get("manifest_revision")
            expected_sha256 = _string(receipt.get("manifest_sha256"), "repair token manifest_sha256").lower()
            expected_source_path = _string(receipt.get("source_path"), "repair token source_path")
            expected_route_key = _string(receipt.get("route_key"), "repair token route_key")
            expected_red_id = _string(receipt.get("red_verification_id"), "repair token red_verification_id")
            expected_query_sha256 = _string(receipt.get("query_sha256"), "repair token query_sha256")
            expected_binding = receipt.get("binding")
        except (ScenarioRegistryStoreError, ScenarioRegistryQueryError) as exc:
            return reject("receipt_malformed", error=str(exc))
        if type(expected_revision) is not int:
            return reject("receipt_malformed", error="repair token manifest_revision must be an integer")
        if expected_manifest_id != str(issued["manifest_id"]) or expected_red_id != str(issued["verification_id"]):
            return reject("receipt_identity_mismatch")
        if expected_route_key != str(issued["route_key"]):
            return reject("receipt_route_mismatch")
        if hashlib.sha256(_query_request_json(request).encode("utf-8")).hexdigest() != expected_query_sha256:
            return reject("query_sha256_changed")
        manifest = connection.execute(
            "SELECT source_path, present, revision, current_sha256, declaration_json FROM manifest_current WHERE manifest_id = ?",
            (expected_manifest_id,),
        ).fetchone()
        if manifest is None or not bool(manifest["present"]):
            return reject("manifest_absent")
        if int(manifest["revision"]) != expected_revision:
            return reject("manifest_revision_changed")
        if str(manifest["current_sha256"] or "").lower() != expected_sha256:
            return reject("manifest_sha256_changed")
        if str(Path(str(manifest["source_path"])).resolve()) != expected_source_path:
            return reject("manifest_source_path_changed")
        try:
            observed_sha256 = hashlib.sha256(Path(str(manifest["source_path"])).read_bytes()).hexdigest()
        except OSError as exc:
            return reject("manifest_source_unreadable", error=str(exc))
        if observed_sha256 != expected_sha256:
            return reject("manifest_source_changed")
        if not _repair_route_current(
                connection, manifest_id=expected_manifest_id, route_key=expected_route_key,
                red_verification_id=expected_red_id):
            return reject("route_not_current_contradiction")
        if not _repair_query_matches_manifest(connection, manifest_id=expected_manifest_id, request=request):
            return reject("query_authority_changed")
        try:
            normalized_expected = _repair_binding(expected_binding, _json_object(
                str(manifest["declaration_json"]), "repair manifest declaration"))
        except ScenarioRegistryStoreError as exc:
            return reject("receipt_malformed", error=str(exc))
        if binding is not None:
            try:
                current_binding = _repair_binding(binding, _json_object(
                    str(manifest["declaration_json"]), "repair manifest declaration"))
            except ScenarioRegistryStoreError as exc:
                return reject("binding_invalid", error=str(exc))
            if _json_text(current_binding) != _json_text(normalized_expected):
                return reject("binding_changed")
        return RegistryRepairToken(
            token_id, True, "claimed" if claimed else "current", Path(str(manifest["source_path"])).stem,
            str(Path(str(manifest["source_path"])).resolve()), normalized_expected["runtime"],
        )


def claim_repair_token_for_launch(
    connection: sqlite3.Connection,
    token_id: str,
    *,
    binding: Mapping[str, Any],
) -> RegistryRepairToken:
    """Consume the repair authority before its only canonical probe begins."""
    selection = reload_repair_token_for_launch(connection, token_id, binding=binding)
    if not selection.accepted:
        return selection
    with immediate_transaction(connection):
        issued = _repair_token_details(connection, selection.token_id)
        if issued is None:
            return RegistryRepairToken(selection.token_id, False, "token_unknown")
        connection.execute(
            "INSERT INTO token_history( token_id, manifest_id, verification_id, route_key, event_kind, reason, details_json ) "
            "VALUES( ?, ?, ?, ?, 'repair_claimed', 'canonical_repair_probe_launch', '{}' )",
            (selection.token_id, str(issued["manifest_id"]), issued["verification_id"], str(issued["route_key"])),
        )
    return reload_repair_token_for_launch(
        connection, selection.token_id, require_claimed=True, binding=binding,
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


def _migration_003_migration_item_transition_guards(connection: sqlite3.Connection) -> None:
    """Make each immutable migration identity claimable exactly once per phase."""
    connection.executescript(
        """
        CREATE UNIQUE INDEX migration_item_one_snapshot
        ON migration_item( migration_run_id, source_path, source_sha256 )
        WHERE event_kind = 'snapshot';

        CREATE UNIQUE INDEX migration_item_one_attempt
        ON migration_item( migration_run_id, source_path, source_sha256 )
        WHERE event_kind = 'attempted';

        CREATE UNIQUE INDEX migration_item_one_launch_claim
        ON migration_item( migration_run_id, source_path, source_sha256 )
        WHERE event_kind = 'launch_claimed';

        CREATE UNIQUE INDEX migration_item_one_terminal
        ON migration_item( migration_run_id, source_path, source_sha256 )
        WHERE event_kind = 'terminal';
        """
    )


def _migration_004_migration_run_events(connection: sqlite3.Connection) -> None:
    """Keep completion state append-only instead of rewriting immutable migration runs."""
    connection.executescript(
        """
        CREATE TABLE migration_run_event (
            migration_run_event_id INTEGER PRIMARY KEY,
            migration_run_id TEXT NOT NULL REFERENCES migration_run( migration_run_id ) ON DELETE RESTRICT,
            event_kind TEXT NOT NULL,
            completion_identity TEXT NOT NULL,
            status TEXT NOT NULL,
            reason TEXT NOT NULL,
            details_json TEXT NOT NULL,
            recorded_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            UNIQUE ( migration_run_id, event_kind, completion_identity )
        );

        CREATE INDEX idx_migration_run_event
        ON migration_run_event( migration_run_id, migration_run_event_id );
        """
    )
    _create_history_append_only_triggers(connection, "migration_run_event")


def _migration_005_retirement_actions(connection: sqlite3.Connection) -> None:
    """Retain approval and removal attempts as resumable append-only actions."""
    connection.executescript(
        """
        CREATE TABLE retirement_action_history (
            retirement_action_event_id INTEGER PRIMARY KEY,
            action_id TEXT NOT NULL,
            manifest_id TEXT NOT NULL REFERENCES manifest_current( manifest_id ) ON DELETE RESTRICT,
            successor_manifest_id TEXT NOT NULL REFERENCES manifest_current( manifest_id ) ON DELETE RESTRICT,
            source_path TEXT NOT NULL,
            source_sha256 TEXT NOT NULL,
            reviewer_identity TEXT NOT NULL,
            retirement_reason TEXT NOT NULL,
            event_kind TEXT NOT NULL,
            details_json TEXT NOT NULL,
            recorded_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            UNIQUE ( action_id, event_kind )
        );

        CREATE INDEX idx_retirement_action_manifest
        ON retirement_action_history( manifest_id, retirement_action_event_id );
        """
    )
    _create_history_append_only_triggers(connection, "retirement_action_history")


def _migration_006_wec_authority_history(connection: sqlite3.Connection) -> None:
    """Own WEC authority outside mutable reports."""
    connection.executescript(
        """
        CREATE TABLE wec_authority_history (
            authority_id TEXT PRIMARY KEY,
            evidence_class TEXT NOT NULL,
            authority TEXT NOT NULL,
            run_id TEXT NOT NULL UNIQUE,
            binding_id TEXT NOT NULL,
            source_sha256 TEXT NOT NULL,
            owner TEXT NOT NULL DEFAULT '',
            recorded_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        );
        """
    )
    _create_history_append_only_triggers(connection, "wec_authority_history")


def _migration_007_certification_round_records(connection: sqlite3.Connection) -> None:
    """Persist sealed round facts and append-only lifecycle/lease history."""
    connection.executescript(
        """
        CREATE TABLE certification_round (
            round_id TEXT PRIMARY KEY,
            scenario_lineage_id TEXT NOT NULL,
            authority_id TEXT NOT NULL,
            authority_kind TEXT NOT NULL,
            event_stream_id TEXT NOT NULL,
            binding_id TEXT NOT NULL,
            manifest_sha256 TEXT NOT NULL UNIQUE,
            manifest_json TEXT NOT NULL,
            recorded_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        );
        CREATE TABLE certification_round_component (
            round_id TEXT NOT NULL REFERENCES certification_round(round_id) ON DELETE RESTRICT,
            component_sequence INTEGER NOT NULL CHECK(component_sequence > 0),
            component_name TEXT NOT NULL,
            fact_sha256 TEXT NOT NULL,
            fact_json TEXT NOT NULL,
            recorded_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            PRIMARY KEY(round_id, component_sequence),
            UNIQUE(round_id, component_name)
        );
        CREATE TABLE certification_round_lifecycle (
            lifecycle_event_id INTEGER PRIMARY KEY,
            round_id TEXT NOT NULL REFERENCES certification_round(round_id) ON DELETE RESTRICT,
            event_sequence INTEGER NOT NULL CHECK(event_sequence > 0),
            event_kind TEXT NOT NULL,
            scenario_lineage_id TEXT NOT NULL,
            authority_id TEXT NOT NULL,
            binding_id TEXT NOT NULL,
            event_stream_id TEXT NOT NULL,
            details_json TEXT NOT NULL,
            recorded_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            UNIQUE(round_id, event_sequence)
        );
        CREATE TABLE certification_round_invalidation (
            round_id TEXT PRIMARY KEY REFERENCES certification_round(round_id) ON DELETE RESTRICT,
            reason TEXT NOT NULL,
            component_name TEXT NOT NULL,
            observed_sequence INTEGER,
            recorded_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        );
        CREATE TABLE certification_round_lease_history (
            lease_event_id INTEGER PRIMARY KEY,
            round_id TEXT NOT NULL REFERENCES certification_round(round_id) ON DELETE RESTRICT,
            lease_id TEXT NOT NULL,
            event_sequence INTEGER NOT NULL CHECK(event_sequence > 0),
            event_kind TEXT NOT NULL,
            process_identity TEXT NOT NULL,
            world_identity TEXT NOT NULL,
            details_json TEXT NOT NULL,
            recorded_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            UNIQUE(round_id, lease_id, event_sequence)
        );
        CREATE INDEX idx_cert_round_lifecycle ON certification_round_lifecycle(round_id, event_sequence);
        CREATE INDEX idx_cert_round_lease ON certification_round_lease_history(round_id, lease_event_id);
        """
    )
    for table in (
        "certification_round", "certification_round_component", "certification_round_lifecycle",
        "certification_round_invalidation", "certification_round_lease_history",
    ):
        _create_history_append_only_triggers(connection, table)


def _migration_008_certification_save_capabilities(connection: sqlite3.Connection) -> None:
    """Keep only a verification commitment for each game save emitter."""
    connection.executescript(
        """
        CREATE TABLE certification_save_capability (
            round_id TEXT PRIMARY KEY REFERENCES certification_round(round_id) ON DELETE RESTRICT,
            capability_commitment TEXT NOT NULL,
            owner_identity TEXT NOT NULL,
            recorded_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        );
        """
    )
    _create_history_append_only_triggers(connection, "certification_save_capability")


def _migration_009_diagnostic_capsule_candidates(connection: sqlite3.Connection) -> None:
    """Persist preserved, bound diagnostic states as immutable candidates."""
    connection.executescript(
        """
        CREATE TABLE diagnostic_capsule_candidate (
            candidate_id TEXT PRIMARY KEY,
            report_id TEXT NOT NULL REFERENCES report_ingestion_history(report_id) ON DELETE RESTRICT,
            verification_id TEXT NOT NULL REFERENCES verification_history(verification_id) ON DELETE RESTRICT,
            run_id TEXT NOT NULL,
            report_path TEXT NOT NULL,
            report_sha256 TEXT NOT NULL,
            artifact_path TEXT NOT NULL,
            artifact_sha256 TEXT NOT NULL,
            binding_id TEXT NOT NULL,
            binding_json TEXT NOT NULL,
            site_id TEXT NOT NULL,
            operation TEXT NOT NULL,
            generation TEXT NOT NULL,
            actor_ids_json TEXT NOT NULL,
            owner TEXT NOT NULL,
            gate_id TEXT NOT NULL,
            gate_index INTEGER NOT NULL CHECK(gate_index >= 0),
            durable_timestamp TEXT NOT NULL,
            source_kind TEXT NOT NULL,
            details_json TEXT NOT NULL,
            recorded_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            UNIQUE(report_id, verification_id, candidate_id)
        );
        CREATE INDEX idx_diagnostic_capsule_binding
        ON diagnostic_capsule_candidate(binding_id, gate_index, durable_timestamp);
        """
    )
    _create_history_append_only_triggers(connection, "diagnostic_capsule_candidate")


def _migration_010_binding_report_identity(connection: sqlite3.Connection) -> None:
    """Index binding ownership so reconciliation need not decode unrelated payloads."""
    connection.executescript(
        """
        ALTER TABLE binding_history ADD COLUMN report_id TEXT;
        CREATE INDEX idx_binding_report_identity
        ON binding_history(report_id, binding_kind, binding_event_id);
        """
    )


def _migration_011_backfill_binding_report_identity(connection: sqlite3.Connection) -> None:
    """Backfill legacy binding ownership only when authoritative identity agrees."""
    connection.executescript(
        """
        CREATE TABLE IF NOT EXISTS binding_identity_migration (
            binding_event_id INTEGER PRIMARY KEY REFERENCES binding_history(binding_event_id) ON DELETE RESTRICT,
            migration_status TEXT NOT NULL CHECK ( migration_status IN ('backfilled', 'rejected') ),
            report_id TEXT NOT NULL DEFAULT '',
            reason TEXT NOT NULL DEFAULT '',
            recorded_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        );
        CREATE INDEX IF NOT EXISTS idx_binding_identity_migration_status
        ON binding_identity_migration(migration_status, binding_event_id);
        """
    )
    _create_history_append_only_triggers(connection, "binding_identity_migration")

    # The normal append-only trigger protects runtime history.  This migration
    # is the sole ownership repair lane; restore the guard before returning.
    connection.execute("DROP TRIGGER IF EXISTS binding_history_no_update")
    rows = connection.execute(
        "SELECT binding_event_id, manifest_id, binding_kind, payload_json "
        "FROM binding_history WHERE report_id IS NULL ORDER BY binding_event_id"
    ).fetchall()
    candidates = []
    rejected = {}
    for row in rows:
        event_id = int(row["binding_event_id"])
        try:
            payload = _json_object(str(row["payload_json"]), "binding payload")
            report_id = payload.get("report_id")
            verification_id = payload.get("verification_id")
            if not isinstance(report_id, str) or not report_id.strip():
                raise ScenarioRegistryStoreError("missing report identity")
            if not isinstance(verification_id, str) or not verification_id.strip():
                raise ScenarioRegistryStoreError("missing verification identity")
            owner_rows = connection.execute(
                "SELECT report_id, manifest_id FROM report_ingestion_history WHERE report_id = ?",
                (report_id,),
            ).fetchall()
            if len(owner_rows) != 1 or owner_rows[0]["manifest_id"] != row["manifest_id"]:
                raise ScenarioRegistryStoreError("report owner mismatch")
            verification = connection.execute(
                "SELECT verification_id FROM verification_history "
                "WHERE verification_id = ? AND report_id = ? AND manifest_id = ?",
                (verification_id, report_id, row["manifest_id"]),
            ).fetchall()
            if len(verification) != 1:
                raise ScenarioRegistryStoreError("verification owner mismatch")
            candidates.append((event_id, report_id, str(row["binding_kind"])))
        except (ScenarioRegistryStoreError, json.JSONDecodeError, TypeError, ValueError) as exc:
            rejected[event_id] = str(exc)

    counts = {}
    for _event_id, report_id, kind in candidates:
        counts[(report_id, kind)] = counts.get((report_id, kind), 0) + 1
    for event_id, report_id, kind in candidates:
        if counts[(report_id, kind)] != 1:
            rejected[event_id] = "duplicate binding kind for report"

    try:
        for row in rows:
            event_id = int(row["binding_event_id"])
            candidate = next((item for item in candidates if item[0] == event_id), None)
            if event_id in rejected or candidate is None:
                reason = rejected.get(event_id, "ambiguous binding identity")
                connection.execute(
                    "INSERT INTO binding_identity_migration(binding_event_id, migration_status, reason) "
                    "VALUES (?, 'rejected', ?)", (event_id, reason),
                )
                continue
            report_id = candidate[1]
            connection.execute(
                "UPDATE binding_history SET report_id = ? WHERE binding_event_id = ? AND report_id IS NULL",
                (report_id, event_id),
            )
            connection.execute(
                "INSERT INTO binding_identity_migration(binding_event_id, migration_status, report_id) "
                "VALUES (?, 'backfilled', ?)", (event_id, report_id),
            )
    finally:
        _create_history_append_only_triggers(connection, "binding_history")


def _migration_012_scenario_lifecycle_history(connection: sqlite3.Connection) -> None:
    """Persist setup-only scenario lifecycle decisions beside immutable sources."""
    connection.executescript(
        """
        CREATE TABLE scenario_selection_history (
            selection_id TEXT PRIMARY KEY,
            query_id TEXT NOT NULL,
            manifest_id TEXT NOT NULL REFERENCES manifest_current( manifest_id ) ON DELETE RESTRICT,
            manifest_revision INTEGER NOT NULL,
            manifest_sha256 TEXT NOT NULL,
            fit_reason TEXT NOT NULL,
            details_json TEXT NOT NULL,
            recorded_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            UNIQUE ( query_id, manifest_id, manifest_revision, manifest_sha256 )
        );
        CREATE TABLE scenario_validation_history (
            validation_id TEXT PRIMARY KEY,
            manifest_id TEXT NOT NULL REFERENCES manifest_current( manifest_id ) ON DELETE RESTRICT,
            manifest_sha256 TEXT NOT NULL,
            fixture_binding_json TEXT NOT NULL,
            validation_status TEXT NOT NULL,
            reason TEXT NOT NULL,
            details_json TEXT NOT NULL,
            recorded_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            UNIQUE ( manifest_id, manifest_sha256, fixture_binding_json, validation_status, reason )
        );
        CREATE TABLE scenario_intervention_history (
            intervention_id TEXT PRIMARY KEY,
            manifest_id TEXT NOT NULL REFERENCES manifest_current( manifest_id ) ON DELETE RESTRICT,
            validation_id TEXT REFERENCES scenario_validation_history( validation_id ) ON DELETE RESTRICT,
            operation TEXT NOT NULL,
            arguments_json TEXT NOT NULL,
            target_json TEXT NOT NULL,
            native_receipt_json TEXT NOT NULL,
            before_facts_json TEXT NOT NULL,
            after_facts_json TEXT NOT NULL,
            evidence_effect TEXT NOT NULL CHECK ( evidence_effect = 'none_for_manufactured_state' ),
            preparation_status TEXT NOT NULL,
            recorded_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        );
        CREATE INDEX idx_scenario_selection_manifest
        ON scenario_selection_history( manifest_id, recorded_at );
        CREATE INDEX idx_scenario_validation_manifest
        ON scenario_validation_history( manifest_id, recorded_at );
        CREATE INDEX idx_scenario_intervention_manifest
        ON scenario_intervention_history( manifest_id, recorded_at );
        """
    )
    for table in (
        "scenario_selection_history",
        "scenario_validation_history",
        "scenario_intervention_history",
    ):
        _create_history_append_only_triggers(connection, table)


def _migration_013_cockpit_capability_runs_and_gaps(connection: sqlite3.Connection) -> None:
    """Persist the cockpit's reusable knowledge without competing with report authority."""
    connection.executescript(
        """
        CREATE TABLE capability_contract_revision (
            capability_id TEXT NOT NULL,
            revision INTEGER NOT NULL CHECK ( revision > 0 ),
            contract_json TEXT NOT NULL,
            recorded_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            PRIMARY KEY ( capability_id, revision )
        );
        CREATE TABLE cockpit_run_receipt (
            receipt_id TEXT PRIMARY KEY,
            run_id TEXT NOT NULL,
            scenario_id TEXT NOT NULL,
            binding_id TEXT NOT NULL,
            event_kind TEXT NOT NULL CHECK ( event_kind IN ( 'status', 'finish' ) ),
            details_json TEXT NOT NULL,
            evidence_effect TEXT NOT NULL,
            observed_cost_json TEXT NOT NULL,
            recorded_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        );
        CREATE INDEX idx_cockpit_run_receipt_run
        ON cockpit_run_receipt( run_id, recorded_at, receipt_id );
        CREATE TABLE capability_gap (
            gap_id TEXT PRIMARY KEY,
            equivalence_key TEXT NOT NULL UNIQUE,
            blocked_intent TEXT NOT NULL,
            missing_kind TEXT NOT NULL,
            reusable_outcome TEXT NOT NULL,
            evidence_json TEXT NOT NULL,
            observed_cost_json TEXT NOT NULL,
            recorded_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        );
        CREATE TABLE capability_gap_report (
            report_id TEXT PRIMARY KEY,
            gap_id TEXT NOT NULL REFERENCES capability_gap( gap_id ) ON DELETE RESTRICT,
            run_id TEXT NOT NULL,
            scenario_id TEXT NOT NULL,
            binding_id TEXT NOT NULL,
            affected_scenarios_json TEXT NOT NULL,
            recorded_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        );
        CREATE INDEX idx_capability_gap_report_scenario
        ON capability_gap_report( scenario_id, gap_id );
        """
    )
    for table in (
        "capability_contract_revision", "cockpit_run_receipt", "capability_gap", "capability_gap_report",
    ):
        _create_history_append_only_triggers(connection, table)


def _migration_014_source_bound_review_firewall(connection: sqlite3.Connection) -> None:
    """Keep external review decisions immutable and exact-source-bound."""
    connection.executescript(
        """
        CREATE TABLE source_bound_review_history (
            review_event_id INTEGER PRIMARY KEY,
            manifest_id TEXT NOT NULL REFERENCES manifest_current( manifest_id ) ON DELETE RESTRICT,
            source_path TEXT NOT NULL,
            manifest_revision INTEGER NOT NULL CHECK ( manifest_revision > 0 ),
            manifest_sha256 TEXT NOT NULL,
            decision TEXT NOT NULL CHECK ( decision IN ( 'accepted', 'rejected' ) ),
            reviewer_identity TEXT NOT NULL,
            recorded_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            UNIQUE ( manifest_id, source_path, manifest_revision, manifest_sha256 )
        );
        CREATE INDEX idx_source_bound_review_identity
        ON source_bound_review_history( manifest_id, source_path, manifest_revision, manifest_sha256, review_event_id );
        """
    )
    _create_history_append_only_triggers(connection, "source_bound_review_history")


def _migration_015_r019_acceptance_matrix(connection: sqlite3.Connection) -> None:
    """Persist immutable R-019 cross-report acceptance relations."""
    connection.executescript(
        """
        CREATE TABLE r019_acceptance_matrix_history (
            matrix_event_id INTEGER PRIMARY KEY,
            manifest_id TEXT NOT NULL REFERENCES manifest_current( manifest_id ) ON DELETE RESTRICT,
            guarded_report_id TEXT NOT NULL REFERENCES report_ingestion_history( report_id ) ON DELETE RESTRICT,
            primitive_report_id TEXT NOT NULL REFERENCES report_ingestion_history( report_id ) ON DELETE RESTRICT,
            status TEXT NOT NULL CHECK ( status IN ( 'green', 'red' ) ),
            details_json TEXT NOT NULL,
            recorded_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            UNIQUE ( manifest_id, guarded_report_id, primitive_report_id )
        );
        CREATE INDEX idx_r019_acceptance_matrix_manifest
        ON r019_acceptance_matrix_history( manifest_id, matrix_event_id );
        """
    )
    _create_history_append_only_triggers(connection, "r019_acceptance_matrix_history")


def _migration_016_r019_acceptance_matrix_evaluations(connection: sqlite3.Connection) -> None:
    """Append each current R-019 relation evaluation without rewriting history."""
    connection.executescript(
        """
        CREATE TABLE r019_acceptance_matrix_evaluation_history (
            matrix_evaluation_event_id INTEGER PRIMARY KEY,
            manifest_id TEXT NOT NULL REFERENCES manifest_current( manifest_id ) ON DELETE RESTRICT,
            guarded_report_id TEXT NOT NULL REFERENCES report_ingestion_history( report_id ) ON DELETE RESTRICT,
            primitive_report_id TEXT NOT NULL REFERENCES report_ingestion_history( report_id ) ON DELETE RESTRICT,
            status TEXT NOT NULL CHECK ( status IN ( 'green', 'red' ) ),
            details_json TEXT NOT NULL,
            details_sha256 TEXT NOT NULL,
            recorded_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            UNIQUE ( manifest_id, details_sha256 )
        );
        CREATE INDEX idx_r019_acceptance_matrix_evaluation_manifest
        ON r019_acceptance_matrix_evaluation_history( manifest_id, matrix_evaluation_event_id );
        """
    )
    _create_history_append_only_triggers(connection, "r019_acceptance_matrix_evaluation_history")


def _migration_017_r019_aggregation_terminals(connection: sqlite3.Connection) -> None:
    """Preserve explicitly authorized, zero-credit R-019 matrix terminals."""
    connection.executescript(
        """
        CREATE TABLE r019_aggregation_terminal_history (
            aggregation_terminal_id INTEGER PRIMARY KEY,
            token_id TEXT NOT NULL UNIQUE,
            manifest_id TEXT NOT NULL REFERENCES manifest_current( manifest_id ) ON DELETE RESTRICT,
            guarded_report_id TEXT NOT NULL REFERENCES report_ingestion_history( report_id ) ON DELETE RESTRICT,
            primitive_report_id TEXT NOT NULL REFERENCES report_ingestion_history( report_id ) ON DELETE RESTRICT,
            packet_json TEXT NOT NULL,
            packet_sha256 TEXT NOT NULL UNIQUE,
            recorded_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            UNIQUE ( manifest_id, guarded_report_id, primitive_report_id )
        );
        CREATE INDEX idx_r019_aggregation_terminal_manifest
        ON r019_aggregation_terminal_history( manifest_id, aggregation_terminal_id );
        """
    )
    _create_history_append_only_triggers(connection, "r019_aggregation_terminal_history")


def _migration_018_cockpit_run_authority(connection: sqlite3.Connection) -> None:
    """Track observation authority independently from evidence eligibility."""
    connection.executescript(
        """
        CREATE TABLE cockpit_run_authority (
            receipt_id TEXT PRIMARY KEY,
            selection_id TEXT NOT NULL UNIQUE REFERENCES scenario_selection_history( selection_id ) ON DELETE RESTRICT,
            run_id TEXT NOT NULL UNIQUE,
            manifest_id TEXT NOT NULL REFERENCES manifest_current( manifest_id ) ON DELETE RESTRICT,
            manifest_revision INTEGER NOT NULL CHECK ( manifest_revision > 0 ),
            manifest_sha256 TEXT NOT NULL,
            source_path TEXT NOT NULL,
            source_sha256 TEXT NOT NULL,
            executable_path TEXT NOT NULL,
            executable_sha256 TEXT NOT NULL,
            binding_id TEXT NOT NULL UNIQUE,
            ownership_scope TEXT NOT NULL,
            owner_id TEXT NOT NULL,
            evidence_ceiling TEXT NOT NULL CHECK ( evidence_ceiling IN ( 'zero-credit', 'setup-only', 'diagnostic', 'focused' ) ),
            recorded_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        );
        CREATE INDEX idx_cockpit_run_authority_scope
        ON cockpit_run_authority( ownership_scope, recorded_at );
        CREATE TABLE cockpit_run_authority_event (
            event_id TEXT PRIMARY KEY,
            receipt_id TEXT NOT NULL REFERENCES cockpit_run_authority( receipt_id ) ON DELETE RESTRICT,
            event_kind TEXT NOT NULL CHECK ( event_kind IN ( 'opened', 'finished', 'invalidated' ) ),
            details_json TEXT NOT NULL,
            recorded_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            UNIQUE ( receipt_id, event_kind )
        );
        """
    )
    _create_history_append_only_triggers(connection, "cockpit_run_authority")
    _create_history_append_only_triggers(connection, "cockpit_run_authority_event")


def _migration_019_r018_acceptance_matrix(connection: sqlite3.Connection) -> None:
    """Retain the retired R-018 matrix table for existing registry compatibility.

    The generic playtest witness migration supersedes this projection. No active
    ingestion or eligibility route writes or reads the table.
    """
    connection.executescript(
        """
        CREATE TABLE r018_acceptance_matrix_evaluation_history (
            matrix_evaluation_event_id INTEGER PRIMARY KEY,
            manifest_id TEXT NOT NULL REFERENCES manifest_current( manifest_id ) ON DELETE RESTRICT,
            raw_report_id TEXT NOT NULL REFERENCES report_ingestion_history( report_id ) ON DELETE RESTRICT,
            primitive_report_id TEXT NOT NULL REFERENCES report_ingestion_history( report_id ) ON DELETE RESTRICT,
            status TEXT NOT NULL CHECK ( status IN ( 'green', 'red' ) ),
            details_json TEXT NOT NULL,
            details_sha256 TEXT NOT NULL,
            recorded_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            UNIQUE ( manifest_id, details_sha256 )
        );
        CREATE INDEX idx_r018_acceptance_matrix_evaluation_manifest
        ON r018_acceptance_matrix_evaluation_history( manifest_id, matrix_evaluation_event_id );
        """
    )
    _create_history_append_only_triggers(connection, "r018_acceptance_matrix_evaluation_history")


def _migration_020_playtest_witness(connection: sqlite3.Connection) -> None:
    """Replace scenario-specific proof packets with cited LLM witness history."""
    connection.executescript(
        """
        CREATE TABLE playtest_witness_history (
            witness_id TEXT PRIMARY KEY,
            manifest_id TEXT NOT NULL REFERENCES manifest_current( manifest_id ) ON DELETE RESTRICT,
            charter_json TEXT NOT NULL,
            journal_json TEXT NOT NULL,
            statement_json TEXT NOT NULL,
            validation_json TEXT NOT NULL,
            verdict TEXT NOT NULL CHECK ( verdict IN ( 'proved', 'contradicted', 'inconclusive' ) ),
            evidence_ceiling TEXT NOT NULL CHECK ( evidence_ceiling IN (
                'zero-credit', 'setup-only', 'diagnostic', 'focused', 'certification'
            ) ),
            recorded_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        );
        CREATE TABLE playtest_witness_report_history (
            witness_id TEXT NOT NULL REFERENCES playtest_witness_history( witness_id ) ON DELETE RESTRICT,
            report_id TEXT NOT NULL REFERENCES report_ingestion_history( report_id ) ON DELETE RESTRICT,
            PRIMARY KEY ( witness_id, report_id )
        );
        CREATE TABLE playtest_witness_review_history (
            review_id TEXT PRIMARY KEY,
            witness_id TEXT NOT NULL REFERENCES playtest_witness_history( witness_id ) ON DELETE RESTRICT,
            reviewer_role TEXT NOT NULL CHECK ( reviewer_role IN ( 'coordinator', 'mutation-reviewer' ) ),
            decision TEXT NOT NULL CHECK ( decision IN ( 'accept', 'continue', 'repair', 'change-strategy' ) ),
            rationale TEXT NOT NULL,
            concrete_risk TEXT NOT NULL,
            review_json TEXT NOT NULL,
            recorded_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            UNIQUE ( witness_id, reviewer_role, decision, rationale, concrete_risk )
        );
        CREATE INDEX idx_playtest_witness_manifest
        ON playtest_witness_history( manifest_id, recorded_at );
        """
    )
    for table in (
        "playtest_witness_history", "playtest_witness_report_history",
        "playtest_witness_review_history",
    ):
        _create_history_append_only_triggers(connection, table)


def _migration_021_windows_feel_handoff(connection: sqlite3.Connection) -> None:
    """Persist the human-owned Windows feel gate outside automated reports."""
    connection.executescript(
        """
        CREATE TABLE windows_feel_handoff (
            handoff_id TEXT PRIMARY KEY,
            certification_verification_id TEXT NOT NULL UNIQUE REFERENCES verification_history( verification_id ) ON DELETE RESTRICT,
            certification_binding_id TEXT NOT NULL,
            certification_round_id TEXT NOT NULL,
            windows_build_json TEXT NOT NULL,
            ordinary_play_json TEXT NOT NULL,
            state TEXT NOT NULL CHECK ( state = 'pending' ),
            recorded_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        );
        CREATE TABLE windows_feel_judgment (
            handoff_id TEXT PRIMARY KEY REFERENCES windows_feel_handoff( handoff_id ) ON DELETE RESTRICT,
            outcome TEXT NOT NULL CHECK ( outcome IN ( 'pass', 'fail' ) ),
            author TEXT NOT NULL CHECK ( author = 'Josef' ),
            notes TEXT NOT NULL DEFAULT '',
            recorded_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        );
        CREATE INDEX idx_windows_feel_handoff_certification
        ON windows_feel_handoff( certification_verification_id );
        """
    )
    _create_history_append_only_triggers(connection, "windows_feel_handoff")
    _create_history_append_only_triggers(connection, "windows_feel_judgment")


SCHEMA_MIGRATIONS: Sequence[Migration] = (
    (1, "initial_registry_surface", _migration_001_initial),
    (2, "inventory_migration_history", _migration_002_inventory_migration_history),
    (3, "migration_item_transition_guards", _migration_003_migration_item_transition_guards),
    (4, "migration_run_events", _migration_004_migration_run_events),
    (5, "retirement_actions", _migration_005_retirement_actions),
    (6, "wec_authority_history", _migration_006_wec_authority_history),
    (7, "certification_round_records", _migration_007_certification_round_records),
    (8, "certification_save_capabilities", _migration_008_certification_save_capabilities),
    (9, "diagnostic_capsule_candidates", _migration_009_diagnostic_capsule_candidates),
    (10, "binding_report_identity", _migration_010_binding_report_identity),
    (11, "backfill_binding_report_identity", _migration_011_backfill_binding_report_identity),
    (12, "scenario_lifecycle_history", _migration_012_scenario_lifecycle_history),
    (13, "cockpit_capability_runs_and_gaps", _migration_013_cockpit_capability_runs_and_gaps),
    (14, "source_bound_review_firewall", _migration_014_source_bound_review_firewall),
    (15, "r019_acceptance_matrix", _migration_015_r019_acceptance_matrix),
    (16, "r019_acceptance_matrix_evaluations", _migration_016_r019_acceptance_matrix_evaluations),
    (17, "r019_aggregation_terminals", _migration_017_r019_aggregation_terminals),
    (18, "cockpit_run_authority", _migration_018_cockpit_run_authority),
    (19, "r018_acceptance_matrix", _migration_019_r018_acceptance_matrix),
    (20, "playtest_witness", _migration_020_playtest_witness),
    (21, "windows_feel_handoff", _migration_021_windows_feel_handoff),
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
        # Registry readers may remain attached while a harness or VM observes
        # its authoritative state.  WAL keeps those snapshots readable without
        # allowing them to block this connection's atomic registry commits.
        # Retain sqlite3's documented five-second busy wait for the brief
        # handoff between authoritative registry operations.
        connection = sqlite3.connect(path, isolation_level=None, timeout=5.0)
        connection.row_factory = sqlite3.Row
        connection.execute("PRAGMA foreign_keys = ON")
        try:
            connection.execute("PRAGMA journal_mode = WAL")
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


_CERTIFICATION_COMPONENT_ORDER = (
    "worktree", "executable", "data_config", "harness", "scenario",
    "fixture", "profile", "world_save", "player", "actors",
)


def _round_manifest_json(manifest: Mapping[str, Any]) -> str:
    """Canonicalize a sealed (possibly MappingProxyType) manifest."""
    def plain(value: Any) -> Any:
        if isinstance(value, Mapping):
            return {key: plain(item) for key, item in value.items()}
        if isinstance(value, (list, tuple)):
            return [plain(item) for item in value]
        return value
    return _json_text(plain(manifest))


def register_certification_round(
    connection: sqlite3.Connection, manifest: Mapping[str, Any],
) -> Dict[str, Any]:
    """Register one valid sealed manifest and its immutable component facts."""
    try:
        _validate_round_manifest(manifest)
    except (RoundManifestError, TypeError, ValueError) as exc:
        raise ScenarioRegistryStoreError(f"invalid sealed certification manifest: {exc}") from exc
    manifest_json = _round_manifest_json(manifest)
    # A manifest producer freezes nested values to prevent later mutation.
    # Registry facts are JSON records, so continue from the verified plain
    # serialization instead of handing ``mappingproxy`` values to sqlite.
    manifest = json.loads(manifest_json)
    binding = manifest["binding"]
    authoritative = binding["authoritative_components"]
    components = tuple(
        (name, authoritative[name]) for name in _CERTIFICATION_COMPONENT_ORDER
    )
    with immediate_transaction(connection):
        authority = connection.execute(
            "SELECT authority_id,evidence_class,authority,run_id,binding_id,source_sha256,owner "
            "FROM wec_authority_history WHERE authority_id = ?",
            (str(manifest["authority_id"]),),
        ).fetchone()
        if authority is None:
            raise ScenarioRegistryStoreError("certification round authority is not registry-issued")
        scenario = authoritative["scenario"]
        scenario_sha256 = str(scenario.get("content_sha256", "") if isinstance(scenario, Mapping) else "")
        expected_authority = {
            "evidence_class": "automated continuous-round certification",
            "authority": "certification",
            "run_id": str(manifest["round_id"]),
            "binding_id": str(manifest["binding_id"]),
            "source_sha256": scenario_sha256,
        }
        if (str(manifest["authority_kind"]) != "automated-certification" or not scenario_sha256 or
                not str(authority["owner"]).startswith("registry-certification-launch:") or
                any(str(authority[key]) != value for key, value in expected_authority.items())):
            raise ScenarioRegistryStoreError(
                "certification round authority does not match route, source, run, and binding"
            )
        existing = connection.execute(
            "SELECT * FROM certification_round WHERE round_id = ?", (str(manifest["round_id"]),)
        ).fetchone()
        if existing is not None:
            if str(existing["manifest_json"]) != manifest_json:
                raise ScenarioRegistryStoreError("certification round ID already has different immutable facts")
            return {"round_id": str(manifest["round_id"]), "binding_id": str(manifest["binding_id"]), "idempotent": True}
        try:
            connection.execute(
                "INSERT INTO certification_round(round_id, scenario_lineage_id, authority_id, authority_kind, "
                "event_stream_id, binding_id, manifest_sha256, manifest_json) VALUES(?,?,?,?,?,?,?,?)",
                (str(manifest["round_id"]), str(manifest["scenario_lineage_id"]), str(manifest["authority_id"]),
                 str(manifest["authority_kind"]), str(manifest["event_stream_id"]), str(manifest["binding_id"]),
                 str(manifest["manifest_sha256"]), manifest_json),
            )
            for sequence, (name, fact) in enumerate(components, 1):
                fact_json = _json_text(fact)
                connection.execute(
                    "INSERT INTO certification_round_component(round_id, component_sequence, component_name, fact_sha256, fact_json) "
                    "VALUES(?,?,?,?,?)",
                    (str(manifest["round_id"]), sequence, name,
                     hashlib.sha256(fact_json.encode("utf-8")).hexdigest(), fact_json),
                )
        except sqlite3.IntegrityError as exc:
            raise ScenarioRegistryStoreError("certification round conflicts with an existing sealed manifest") from exc
    return {"round_id": str(manifest["round_id"]), "binding_id": str(manifest["binding_id"]), "idempotent": False}


def _issue_registry_certification_authority(
    connection: sqlite3.Connection, *, round_id: str, binding_id: str,
    source_sha256: str, launch_token: str,
) -> Dict[str, str]:
    """Issue final authority only inside the canonical registry launch transaction."""
    authority_id = hashlib.sha256(
        ("caol-registry-certification-authority-v1:" + uuid.uuid4().hex).encode()
    ).hexdigest()
    fact = {
        "authority_id": authority_id,
        "evidence_class": "automated continuous-round certification",
        "authority": "certification",
        "run_id": round_id,
        "binding_id": binding_id,
        "source_sha256": source_sha256.lower(),
        "owner": "registry-certification-launch:" + launch_token,
    }
    connection.execute(
        "INSERT INTO wec_authority_history( authority_id, evidence_class, authority, run_id, binding_id, source_sha256, owner ) "
        "VALUES( ?, ?, ?, ?, ?, ?, ? )",
        tuple(fact.values()),
    )
    return fact


def _validate_certification_launch_inputs(
    connection: sqlite3.Connection, *, launch_token: str, launch_source_path: Path,
    launch_route_key: str, binding: Mapping[str, Any], current_executable_sha256: str,
) -> None:
    """Reject caller-crafted final launches before any final authority is issued."""
    token = str(launch_token).strip()
    route = str(launch_route_key).strip()
    if not token or not route:
        raise ScenarioRegistryStoreError("certification launch requires a registry token and route")
    selection = reload_selection_token_for_launch(connection, token)
    if not selection.accepted:
        raise ScenarioRegistryStoreError("certification launch token is not currently executable")
    source = Path(selection.source_path).resolve()
    if source != launch_source_path.resolve():
        raise ScenarioRegistryStoreError("certification launch source does not match its registry token")
    issued = connection.execute(
        "SELECT route_key FROM token_history WHERE token_id = ? AND event_kind = 'issued' "
        "ORDER BY token_event_id LIMIT 1", (token,)
    ).fetchone()
    if issued is None or str(issued["route_key"]) != route:
        raise ScenarioRegistryStoreError("certification launch route does not match its registry token")
    scenario = binding["authoritative_components"]["scenario"]
    executable = binding["authoritative_components"]["executable"]
    if Path(str(scenario.get("path", ""))).resolve() != source:
        raise ScenarioRegistryStoreError("certification manifest scenario is not the selected source")
    current_sha256 = str(current_executable_sha256).strip().lower()
    if (len(current_sha256) != 64 or current_sha256 !=
            str(executable.get("content_sha256", "")).strip().lower()):
        raise ScenarioRegistryStoreError("certification launch executable is not current")


def create_certification_round(
    connection: sqlite3.Connection, *, scenario_lineage_id: str,
    producer_inputs: Mapping[str, Any], launch_token: str, launch_source_path: Path,
    launch_route_key: str, current_executable_sha256: str,
    event_stream_id: Optional[str] = None,
) -> Dict[str, Any]:
    """Create and register one production round through the registry owner.

    No caller controls the round id, binding id, authority id, or outer seal.
    The registry derives all of them from current canonical inputs in one
    owner path, then returns the immutable manifest facts for launch/reporting.
    """
    lineage = str(scenario_lineage_id).strip()
    if not lineage:
        raise ScenarioRegistryStoreError("certification round scenario lineage is required")
    try:
        binding = authoritative_identity_binding(**dict(producer_inputs))
    except (TypeError, ValueError) as exc:
        raise ScenarioRegistryStoreError(f"certification round inputs are invalid: {exc}") from exc
    scenario = binding["authoritative_components"]["scenario"]
    source_sha256 = str(scenario.get("content_sha256", ""))
    executable = binding["authoritative_components"]["executable"]
    if len(source_sha256) != 64 or len(str(executable.get("content_sha256", ""))) != 64:
        raise ScenarioRegistryStoreError("certification round source/executable facts are incomplete")
    _validate_certification_launch_inputs(
        connection, launch_token=launch_token, launch_source_path=launch_source_path,
        launch_route_key=launch_route_key, binding=binding,
        current_executable_sha256=current_executable_sha256,
    )
    round_id = uuid.uuid4().hex
    stream_id = str(event_stream_id or uuid.uuid4().hex).strip()
    if not stream_id:
        raise ScenarioRegistryStoreError("certification event stream is required")
    with immediate_transaction(connection):
        authority = _issue_registry_certification_authority(
            connection, round_id=round_id, binding_id=str(binding["sha256"]),
            source_sha256=source_sha256, launch_token=str(launch_token).strip(),
        )
    manifest = seal_complete_round_manifest(
        round_id=round_id,
        scenario_lineage_id=lineage,
        authority_id=authority["authority_id"],
        authority_kind="automated-certification",
        event_stream_id=stream_id,
        **dict(producer_inputs),
    )
    registered = register_certification_round(connection, manifest)
    append_certification_lifecycle_event(
        connection,
        round_id=round_id,
        event_sequence=1,
        event_kind="started",
        details={
            "launch_token": str(launch_token).strip(),
            "launch_route_key": str(launch_route_key).strip(),
        },
    )
    save_capability = secrets.token_urlsafe(32)
    commitment = hashlib.sha256(save_capability.encode("utf-8")).hexdigest()
    with immediate_transaction(connection):
        connection.execute(
            "INSERT INTO certification_save_capability(round_id, capability_commitment, owner_identity) VALUES(?,?,?)",
            (round_id, commitment, "registry-certification-launch:" + str(launch_token).strip()),
        )
    return {
        "manifest": _plain(manifest),
        "certification_round": certification_round_facts(connection, round_id),
        "authority": authority,
        "save_capability": save_capability,
        **registered,
    }


def certification_round_facts(connection: sqlite3.Connection, round_id: str) -> Dict[str, str]:
    """Return the only reportable certification facts, derived from registry rows."""
    row = connection.execute(
        "SELECT round_id,authority_id,binding_id,event_stream_id,manifest_sha256 "
        "FROM certification_round WHERE round_id = ?", (str(round_id),),
    ).fetchone()
    if row is None:
        raise ScenarioRegistryStoreError("certification round is not registered")
    lifecycle = connection.execute(
        "SELECT event_kind FROM certification_round_lifecycle WHERE round_id = ? "
        "ORDER BY event_sequence DESC LIMIT 1", (str(round_id),),
    ).fetchone()
    return {
        "round_id": str(row["round_id"]),
        "authority_id": str(row["authority_id"]),
        "binding_id": str(row["binding_id"]),
        "event_stream_id": str(row["event_stream_id"]),
        "manifest_sha256": str(row["manifest_sha256"]),
        "lifecycle_state": str(lifecycle["event_kind"]) if lifecycle is not None else "registered",
        "registry_derived": "true",
    }


def certification_round_authority_facts(
    connection: sqlite3.Connection, round_id: str,
) -> Dict[str, str]:
    """Return the registry-issued final authority attached to one round."""
    row = connection.execute(
        "SELECT authority.authority_id,authority.evidence_class,authority.authority,authority.run_id, "
        "authority.binding_id,authority.source_sha256,authority.owner "
        "FROM certification_round AS round "
        "JOIN wec_authority_history AS authority ON authority.authority_id = round.authority_id "
        "WHERE round.round_id = ?", (str(round_id),),
    ).fetchone()
    if row is None:
        raise ScenarioRegistryStoreError("certification round authority is not registry-issued")
    return {key: str(row[key]) for key in (
        "authority_id", "evidence_class", "authority", "run_id", "binding_id", "source_sha256", "owner",
    )}


def verify_certification_save_capability(
    connection: sqlite3.Connection, *, round_id: str, capability: str,
) -> bool:
    """Verify the child-only save capability without ever returning its secret."""
    value = str(capability).strip()
    if not value:
        return False
    row = connection.execute(
        "SELECT capability_commitment FROM certification_save_capability WHERE round_id = ?",
        (str(round_id),),
    ).fetchone()
    if row is None:
        return False
    observed = hashlib.sha256(value.encode("utf-8")).hexdigest()
    return secrets.compare_digest(observed, str(row["capability_commitment"]))


def append_certification_lifecycle_event(
    connection: sqlite3.Connection, *, round_id: str, event_sequence: int,
    event_kind: str, details: Optional[Mapping[str, Any]] = None,
    authority_id: Optional[str] = None, binding_id: Optional[str] = None,
    event_stream_id: Optional[str] = None, scenario_lineage_id: Optional[str] = None,
) -> Dict[str, Any]:
    """Append the next ordered event; an exact replay is idempotent."""
    if event_sequence < 1 or not event_kind.strip():
        raise ScenarioRegistryStoreError("lifecycle event sequence and kind are required")
    with immediate_transaction(connection):
        round_row = connection.execute("SELECT * FROM certification_round WHERE round_id = ?", (round_id,)).fetchone()
        if round_row is None:
            raise ScenarioRegistryStoreError("certification round is not registered")
        identity = {
            "scenario_lineage_id": scenario_lineage_id or str(round_row["scenario_lineage_id"]),
            "authority_id": authority_id or str(round_row["authority_id"]),
            "binding_id": binding_id or str(round_row["binding_id"]),
            "event_stream_id": event_stream_id or str(round_row["event_stream_id"]),
        }
        if any(identity[key] != str(round_row[key]) for key in identity):
            raise ScenarioRegistryStoreError("lifecycle identity does not match sealed round")
        details_json = _json_text(details or {})
        prior = connection.execute(
            "SELECT event_kind, details_json, scenario_lineage_id, authority_id, binding_id, event_stream_id "
            "FROM certification_round_lifecycle WHERE round_id = ? AND event_sequence = ?", (round_id, event_sequence)
        ).fetchone()
        if prior is not None:
            if (str(prior["event_kind"]), str(prior["details_json"]), *(str(prior[key]) for key in identity)) == \
                    (event_kind, details_json, *(identity[key] for key in identity)):
                return {"round_id": round_id, "event_sequence": event_sequence, "idempotent": True}
            raise ScenarioRegistryStoreError("lifecycle sequence already has different immutable facts")
        next_sequence = connection.execute(
            "SELECT COALESCE(MAX(event_sequence), 0) + 1 FROM certification_round_lifecycle WHERE round_id = ?", (round_id,)
        ).fetchone()[0]
        if event_sequence != int(next_sequence):
            raise ScenarioRegistryStoreError("lifecycle events must be appended in order")
        connection.execute(
            "INSERT INTO certification_round_lifecycle(round_id,event_sequence,event_kind,scenario_lineage_id,authority_id,binding_id,event_stream_id,details_json) VALUES(?,?,?,?,?,?,?,?)",
            (round_id, event_sequence, event_kind, identity["scenario_lineage_id"], identity["authority_id"], identity["binding_id"], identity["event_stream_id"], details_json),
        )
    return {"round_id": round_id, "event_sequence": event_sequence, "idempotent": False}


def bind_certification_actors(
    connection: sqlite3.Connection, *, round_id: str,
    ecology_audit: Mapping[str, Any], token_id: str,
    scenario_digest: str, world_id: str, player_id: str,
) -> Dict[str, Any]:
    """Bind started-process actor identities exactly once via the append-only stream."""
    round_row = connection.execute(
        "SELECT * FROM certification_round WHERE round_id = ?", (str(round_id),)
    ).fetchone()
    if round_row is None:
        raise ScenarioRegistryStoreError("certification round is not registered")
    manifest = json.loads(str(round_row["manifest_json"]))
    sealed = manifest["binding"]["authoritative_components"]
    if sealed.get("actors"):
        raise ScenarioRegistryStoreError("certification round already has pre-bound actors")
    actors = ecology_actor_identity(ecology_audit)
    actor_ids = [str(actor["actor_id"]) for actor in actors]
    if not actor_ids:
        raise ScenarioRegistryStoreError("started ecology audit has no actors")
    prior = connection.execute(
        "SELECT details_json FROM certification_round_lifecycle "
        "WHERE round_id = ? AND event_kind = 'actors_bound'", (str(round_id),)
    ).fetchone()
    details = {"token_id": str(token_id), "scenario_digest": str(scenario_digest),
               "world_id": str(world_id), "player_id": str(player_id),
               "actor_ids": actor_ids, "actors": actors, "source": "run-owned-ecology-audit"}
    if prior is not None:
        if json.loads(str(prior["details_json"])) == details:
            return {"round_id": str(round_id), "actor_ids": actor_ids, "idempotent": True}
        raise ScenarioRegistryStoreError("certification actors cannot be rebound or replaced")
    next_sequence = connection.execute(
        "SELECT COALESCE(MAX(event_sequence), 0) + 1 FROM certification_round_lifecycle WHERE round_id = ?",
        (str(round_id),),
    ).fetchone()[0]
    append_certification_lifecycle_event(
        connection, round_id=str(round_id), event_sequence=int(next_sequence),
        event_kind="actors_bound", details=details,
    )
    return {"round_id": str(round_id), "actor_ids": actor_ids, "idempotent": False}


def invalidate_certification_round(
    connection: sqlite3.Connection, *, round_id: str, reason: str, component_name: str,
    observed_sequence: Optional[int] = None,
) -> Dict[str, Any]:
    """Record exactly one first invalidation while retaining later lifecycle history."""
    if not reason.strip() or not component_name.strip():
        raise ScenarioRegistryStoreError("invalidation reason and component are required")
    with immediate_transaction(connection):
        if connection.execute("SELECT 1 FROM certification_round WHERE round_id = ?", (round_id,)).fetchone() is None:
            raise ScenarioRegistryStoreError("certification round is not registered")
        prior = connection.execute("SELECT * FROM certification_round_invalidation WHERE round_id = ?", (round_id,)).fetchone()
        if prior is not None:
            return {"round_id": round_id, "first_reason": str(prior["reason"]), "first_component": str(prior["component_name"]), "preserved": True}
        connection.execute(
            "INSERT INTO certification_round_invalidation(round_id,reason,component_name,observed_sequence) VALUES(?,?,?,?)",
            (round_id, reason, component_name, observed_sequence),
        )
    return {"round_id": round_id, "first_reason": reason, "first_component": component_name, "preserved": False}


def append_certification_lease_event(
    connection: sqlite3.Connection, *, round_id: str, lease_id: str, event_sequence: int,
    event_kind: str, process_identity: str, world_identity: str,
    details: Optional[Mapping[str, Any]] = None,
) -> Dict[str, Any]:
    """Define append-only lease history; acquisition/signalling stays with G4."""
    if not all(value.strip() for value in (lease_id, event_kind, process_identity, world_identity)) or event_sequence < 1:
        raise ScenarioRegistryStoreError("lease identity and positive sequence are required")
    with immediate_transaction(connection):
        if connection.execute("SELECT 1 FROM certification_round WHERE round_id = ?", (round_id,)).fetchone() is None:
            raise ScenarioRegistryStoreError("certification round is not registered")
        details_json = _json_text(details or {})
        prior = connection.execute(
            "SELECT event_kind,process_identity,world_identity,details_json FROM certification_round_lease_history WHERE round_id=? AND lease_id=? AND event_sequence=?",
            (round_id, lease_id, event_sequence),
        ).fetchone()
        facts = (event_kind, process_identity, world_identity, details_json)
        if prior is not None:
            if tuple(str(prior[key]) for key in ("event_kind", "process_identity", "world_identity", "details_json")) == facts:
                return {"round_id": round_id, "lease_id": lease_id, "event_sequence": event_sequence, "idempotent": True}
            raise ScenarioRegistryStoreError("lease sequence already has different immutable facts")
        next_sequence = connection.execute("SELECT COALESCE(MAX(event_sequence),0)+1 FROM certification_round_lease_history WHERE round_id=? AND lease_id=?", (round_id, lease_id)).fetchone()[0]
        if event_sequence != int(next_sequence):
            raise ScenarioRegistryStoreError("lease events must be appended in order")
        connection.execute(
            "INSERT INTO certification_round_lease_history(round_id,lease_id,event_sequence,event_kind,process_identity,world_identity,details_json) VALUES(?,?,?,?,?,?,?)",
            (round_id, lease_id, event_sequence, event_kind, process_identity, world_identity, details_json),
        )
    return {"round_id": round_id, "lease_id": lease_id, "event_sequence": event_sequence, "idempotent": False}


def _json_text(value: Any) -> str:
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":"))


_MIGRATION_TERMINAL_DISPOSITIONS = frozenset({
    "invalid",
    "blocked",
    "imported",
    "verified",
    "contradicted",
    "failed",
})


def _migration_item_identity(migration_run_id: str, source_path: str, source_sha256: str) -> str:
    return _identity(
        "caol-scenario-migration-item-v1",
        migration_run_id,
        source_path,
        source_sha256,
    )


def _migration_event_from_row(row: sqlite3.Row) -> MigrationItemEvent:
    try:
        details = _json_object(str(row["details_json"]), "migration item details")
    except ScenarioRegistryStoreError as exc:
        raise ScenarioRegistryStoreError(
            f"Migration item event {row['migration_item_event_id']} has malformed details"
        ) from exc
    return MigrationItemEvent(
        migration_item_event_id=int(row["migration_item_event_id"]),
        event_kind=str(row["event_kind"]),
        completion_status=str(row["completion_status"]),
        disposition=str(row["disposition"]),
        reason=str(row["reason"]),
        details=details,
        recorded_at=str(row["recorded_at"]),
    )


def _require_migration_source_identity(
    connection: sqlite3.Connection,
    *,
    migration_run_id: str,
    source_path: Path | str,
    source_sha256: str,
) -> Tuple[str, str, str]:
    canonical_path = str(Path(source_path).resolve())
    source_hash = str(source_sha256).strip().lower()
    if len(source_hash) != 64 or any(character not in "0123456789abcdef" for character in source_hash):
        raise ScenarioRegistryStoreError("migration source SHA-256 must be a lowercase hexadecimal digest")
    row = connection.execute(
        "SELECT attempt_identity FROM migration_item WHERE migration_run_id = ? "
        "AND source_path = ? AND source_sha256 = ? AND event_kind = 'snapshot'",
        (migration_run_id, canonical_path, source_hash),
    ).fetchone()
    if row is None:
        raise ScenarioRegistryStoreError(
            "migration source identity is not owned by this immutable snapshot"
        )
    return canonical_path, source_hash, str(row["attempt_identity"])


def _migration_item_current_in_transaction(
    connection: sqlite3.Connection,
    *,
    migration_run_id: str,
    source_path: str,
    source_sha256: str,
    attempt_identity: str,
) -> MigrationItemCurrent:
    rows = connection.execute(
        "SELECT migration_item_event_id, event_kind, completion_status, disposition, reason, details_json, recorded_at "
        "FROM migration_item WHERE migration_run_id = ? AND attempt_identity = ? "
        "AND source_path = ? AND source_sha256 = ? ORDER BY migration_item_event_id",
        (migration_run_id, attempt_identity, source_path, source_sha256),
    ).fetchall()
    if not rows:
        raise ScenarioRegistryStoreError("migration source identity has no durable history")
    history = tuple(_migration_event_from_row(row) for row in rows)
    terminals = [event for event in history if event.event_kind == "terminal"]
    terminal = terminals[-1] if terminals else None
    attempted = any(event.event_kind == "attempted" for event in history)
    launch_claimed = any(event.event_kind == "launch_claimed" for event in history)
    if terminal is not None:
        status = terminal.disposition
    elif attempted:
        status = "attempted"
    else:
        status = "snapshotted"
    return MigrationItemCurrent(
        migration_run_id=migration_run_id,
        attempt_identity=attempt_identity,
        source_path=source_path,
        source_sha256=source_sha256,
        status=status,
        terminal_disposition=terminal.disposition if terminal is not None else None,
        launch_claimed=launch_claimed,
        history=history,
    )


def snapshot_migration_run(
    connection: sqlite3.Connection,
    scenarios_root: Path,
    *,
    launcher_identity: str,
) -> MigrationRunSnapshot:
    """Capture exact scenario bytes without parsing, and append immutable snapshot events.

    One root owns one resumable migration run.  Repeating a snapshot returns
    that run; changed source bytes append a new path/SHA item identity rather
    than retrying the old identity.
    """
    root = scenarios_root.resolve()
    if not root.is_dir():
        raise ScenarioRegistryStoreError(f"Scenario root does not exist: {root}")
    launcher = launcher_identity.strip()
    if not launcher:
        raise ScenarioRegistryStoreError("migration launcher identity is required")
    sources: List[Tuple[str, str]] = []
    for path in sorted(root.glob("*.json"), key=lambda item: str(item.resolve())):
        if not path.is_file():
            continue
        canonical_path = str(path.resolve())
        try:
            source_sha256 = hashlib.sha256(path.read_bytes()).hexdigest()
        except OSError as exc:
            raise ScenarioRegistryStoreError(f"Could not snapshot scenario source {path}: {exc}") from exc
        sources.append((canonical_path, source_sha256))
    run_identity = _identity(
        "caol-scenario-migration-run-v1",
        str(root),
    )
    migration_run_id = _identity("caol-scenario-migration-run-id-v1", run_identity)
    items = tuple(
        MigrationSnapshotItem(
            source_path=source_path,
            source_sha256=source_sha256,
            attempt_identity=_migration_item_identity(migration_run_id, source_path, source_sha256),
        )
        for source_path, source_sha256 in sources
    )
    with immediate_transaction(connection):
        existing = connection.execute(
            "SELECT launcher_identity FROM migration_run WHERE migration_run_id = ?",
            (migration_run_id,),
        ).fetchone()
        if existing is None:
            connection.execute(
                "INSERT INTO migration_run( migration_run_id, run_identity, launch_status, launch_reason, "
                "launcher_identity, details_json ) VALUES( ?, ?, 'snapshotted', 'source_bytes_captured', ?, ? )",
                (
                    migration_run_id,
                    run_identity,
                    launcher,
                    _json_text({"scenarios_root": str(root), "initial_item_count": len(items)}),
                ),
            )
        elif str(existing["launcher_identity"]) != launcher:
            raise ScenarioRegistryStoreError(
                "immutable migration run was created by a different launcher identity"
            )
        existing_snapshots = {
            (str(row["source_path"]), str(row["source_sha256"]))
            for row in connection.execute(
                "SELECT source_path, source_sha256 FROM migration_item WHERE migration_run_id = ? "
                "AND event_kind = 'snapshot'",
                (migration_run_id,),
            )
        }
        new_items = [
            item for item in items
            if (item.source_path, item.source_sha256) not in existing_snapshots
        ]
        if new_items:
            connection.executemany(
                "INSERT INTO migration_item( migration_run_id, manifest_id, attempt_identity, source_path, "
                "source_sha256, event_kind, completion_status, disposition, reason, details_json ) "
                "VALUES( ?, NULL, ?, ?, ?, 'snapshot', 'pending', 'snapshotted', 'source_bytes_captured', ? )",
                [
                    (
                        migration_run_id,
                        item.attempt_identity,
                        item.source_path,
                        item.source_sha256,
                        _json_text({"scenarios_root": str(root)}),
                    )
                    for item in new_items
                ],
            )
    return MigrationRunSnapshot(
        migration_run_id=migration_run_id,
        run_identity=run_identity,
        scenarios_root=str(root),
        items=items,
    )


def migration_run_snapshot(connection: sqlite3.Connection, migration_run_id: str) -> MigrationRunSnapshot:
    """Read the immutable snapshot belonging to one resumable migration run."""
    run = connection.execute(
        "SELECT run_identity, details_json FROM migration_run WHERE migration_run_id = ?",
        (migration_run_id,),
    ).fetchone()
    if run is None:
        raise ScenarioRegistryStoreError("migration run does not exist")
    details = _json_object(str(run["details_json"]), "migration run details")
    scenarios_root = str(details.get("scenarios_root", "")).strip()
    if not scenarios_root:
        raise ScenarioRegistryStoreError("migration run has no scenarios root")
    rows = connection.execute(
        "SELECT source_path, source_sha256, attempt_identity FROM migration_item "
        "WHERE migration_run_id = ? AND event_kind = 'snapshot' ORDER BY source_path, source_sha256",
        (migration_run_id,),
    ).fetchall()
    return MigrationRunSnapshot(
        migration_run_id=migration_run_id,
        run_identity=str(run["run_identity"]),
        scenarios_root=scenarios_root,
        items=tuple(
            MigrationSnapshotItem(
                source_path=str(row["source_path"]),
                source_sha256=str(row["source_sha256"]),
                attempt_identity=str(row["attempt_identity"]),
            )
            for row in rows
        ),
    )


def record_migration_run_success(
    connection: sqlite3.Connection,
    *,
    migration_run_id: str,
    summary: Mapping[str, Any],
) -> Dict[str, Any]:
    """Append an idempotent completion event for a proven exact final set."""
    if not bool(summary.get("completion_ready", False)):
        raise ScenarioRegistryStoreError("migration run cannot succeed before final-set invariants hold")
    summary_json = _json_text(summary)
    completion_identity = _identity(
        "caol-scenario-migration-completion-v1",
        migration_run_id,
        summary_json,
    )
    with immediate_transaction(connection):
        run = connection.execute(
            "SELECT migration_run_id FROM migration_run WHERE migration_run_id = ?",
            (migration_run_id,),
        ).fetchone()
        if run is None:
            raise ScenarioRegistryStoreError("migration run does not exist")
        existing = connection.execute(
            "SELECT migration_run_event_id FROM migration_run_event WHERE migration_run_id = ? "
            "AND event_kind = 'completion' AND completion_identity = ?",
            (migration_run_id, completion_identity),
        ).fetchone()
        if existing is None:
            connection.execute(
                "INSERT INTO migration_run_event( migration_run_id, event_kind, completion_identity, status, reason, details_json ) "
                "VALUES( ?, 'completion', ?, 'succeeded', 'exact_final_set_complete', ? )",
                (migration_run_id, completion_identity, summary_json),
            )
        row = connection.execute(
            "SELECT migration_run_event_id, recorded_at FROM migration_run_event WHERE migration_run_id = ? "
            "AND event_kind = 'completion' AND completion_identity = ?",
            (migration_run_id, completion_identity),
        ).fetchone()
    return {
        "migration_run_event_id": int(row["migration_run_event_id"]),
        "completion_identity": completion_identity,
        "recorded_at": str(row["recorded_at"]),
        "idempotent": existing is not None,
    }


def migration_item_current(
    connection: sqlite3.Connection,
    *,
    migration_run_id: str,
    source_path: Path | str,
    source_sha256: str,
) -> MigrationItemCurrent:
    """Return the latest derived state and complete append-only event history."""
    canonical_path, source_hash, attempt_identity = _require_migration_source_identity(
        connection,
        migration_run_id=migration_run_id,
        source_path=source_path,
        source_sha256=source_sha256,
    )
    return _migration_item_current_in_transaction(
        connection,
        migration_run_id=migration_run_id,
        source_path=canonical_path,
        source_sha256=source_hash,
        attempt_identity=attempt_identity,
    )


def record_migration_attempt(
    connection: sqlite3.Connection,
    *,
    migration_run_id: str,
    source_path: Path | str,
    source_sha256: str,
) -> MigrationItemCurrent:
    """Durably record an attempt before any caller parses or validates source bytes."""
    with immediate_transaction(connection):
        canonical_path, source_hash, attempt_identity = _require_migration_source_identity(
            connection,
            migration_run_id=migration_run_id,
            source_path=source_path,
            source_sha256=source_sha256,
        )
        current = _migration_item_current_in_transaction(
            connection,
            migration_run_id=migration_run_id,
            source_path=canonical_path,
            source_sha256=source_hash,
            attempt_identity=attempt_identity,
        )
        if current.status == "snapshotted":
            connection.execute(
                "INSERT INTO migration_item( migration_run_id, manifest_id, attempt_identity, source_path, "
                "source_sha256, event_kind, completion_status, disposition, reason, details_json ) "
                "VALUES( ?, NULL, ?, ?, ?, 'attempted', 'in_progress', 'attempted', 'pre_parse_claim', '{}' )",
                (migration_run_id, attempt_identity, canonical_path, source_hash),
            )
        return _migration_item_current_in_transaction(
            connection,
            migration_run_id=migration_run_id,
            source_path=canonical_path,
            source_sha256=source_hash,
            attempt_identity=attempt_identity,
        )


def claim_migration_item_launch(
    connection: sqlite3.Connection,
    *,
    migration_run_id: str,
    source_path: Path | str,
    source_sha256: str,
    launch_identity: str,
) -> MigrationItemCurrent:
    """Append the one durable pre-launch claim for an unterminalized item."""
    claim = launch_identity.strip()
    if not claim:
        raise ScenarioRegistryStoreError("migration launch identity is required")
    with immediate_transaction(connection):
        canonical_path, source_hash, attempt_identity = _require_migration_source_identity(
            connection,
            migration_run_id=migration_run_id,
            source_path=source_path,
            source_sha256=source_sha256,
        )
        current = _migration_item_current_in_transaction(
            connection,
            migration_run_id=migration_run_id,
            source_path=canonical_path,
            source_sha256=source_hash,
            attempt_identity=attempt_identity,
        )
        if current.terminal_disposition is not None:
            raise ScenarioRegistryStoreError("terminal migration item cannot receive a launch claim")
        if current.status != "attempted":
            raise ScenarioRegistryStoreError("migration item must be attempted before launch claim")
        existing_claims = [event for event in current.history if event.event_kind == "launch_claimed"]
        if existing_claims:
            if existing_claims[0].details.get("launch_identity") != claim:
                raise ScenarioRegistryStoreError("migration item already has a different launch claim")
            return current
        connection.execute(
            "INSERT INTO migration_item( migration_run_id, manifest_id, attempt_identity, source_path, "
            "source_sha256, event_kind, completion_status, disposition, reason, details_json ) "
            "VALUES( ?, NULL, ?, ?, ?, 'launch_claimed', 'in_progress', 'launch_claimed', "
            "'canonical_launch_claimed', ? )",
            (
                migration_run_id,
                attempt_identity,
                canonical_path,
                source_hash,
                _json_text({"launch_identity": claim}),
            ),
        )
        return _migration_item_current_in_transaction(
            connection,
            migration_run_id=migration_run_id,
            source_path=canonical_path,
            source_sha256=source_hash,
            attempt_identity=attempt_identity,
        )


def record_migration_terminal(
    connection: sqlite3.Connection,
    *,
    migration_run_id: str,
    source_path: Path | str,
    source_sha256: str,
    disposition: str,
    reason: str,
    details: Optional[Mapping[str, Any]] = None,
) -> MigrationItemCurrent:
    """Append one terminal disposition; matching replay is idempotent, conflict fails."""
    terminal = disposition.strip()
    terminal_reason = reason.strip()
    terminal_details = dict(details or {})
    if terminal not in _MIGRATION_TERMINAL_DISPOSITIONS:
        raise ScenarioRegistryStoreError(f"unsupported migration terminal disposition: {terminal}")
    if not terminal_reason:
        raise ScenarioRegistryStoreError("migration terminal reason is required")
    with immediate_transaction(connection):
        canonical_path, source_hash, attempt_identity = _require_migration_source_identity(
            connection,
            migration_run_id=migration_run_id,
            source_path=source_path,
            source_sha256=source_sha256,
        )
        current = _migration_item_current_in_transaction(
            connection,
            migration_run_id=migration_run_id,
            source_path=canonical_path,
            source_sha256=source_hash,
            attempt_identity=attempt_identity,
        )
        source_removed_before_parse = (
            current.status == "snapshotted"
            and terminal == "failed"
            and terminal_reason == "source_removed_during_migration"
        )
        if current.status == "snapshotted" and not source_removed_before_parse:
            raise ScenarioRegistryStoreError("migration item must be attempted before terminal disposition")
        if current.terminal_disposition is not None:
            terminal_event = next(event for event in current.history if event.event_kind == "terminal")
            if (
                terminal_event.disposition != terminal
                or terminal_event.reason != terminal_reason
                or _json_text(terminal_event.details) != _json_text(terminal_details)
            ):
                raise ScenarioRegistryStoreError("migration item already has a conflicting terminal disposition")
            return current
        connection.execute(
            "INSERT INTO migration_item( migration_run_id, manifest_id, attempt_identity, source_path, "
            "source_sha256, event_kind, completion_status, disposition, reason, details_json ) "
            "VALUES( ?, NULL, ?, ?, ?, 'terminal', 'terminal', ?, ?, ? )",
            (
                migration_run_id,
                attempt_identity,
                canonical_path,
                source_hash,
                terminal,
                terminal_reason,
                _json_text(terminal_details),
            ),
        )
        return _migration_item_current_in_transaction(
            connection,
            migration_run_id=migration_run_id,
            source_path=canonical_path,
            source_sha256=source_hash,
            attempt_identity=attempt_identity,
        )


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


def _repository_relative_file(value: Any, *, field: str) -> Path:
    if not isinstance(value, str) or not value.strip():
        raise ScenarioRegistryStoreError(f"{field} must be a non-empty repository-relative path")
    root = repository_root().resolve()
    candidate = (root / value).resolve()
    try:
        candidate.relative_to(root)
    except ValueError as exc:
        raise ScenarioRegistryStoreError(f"{field} escapes the repository") from exc
    return candidate


def _source_binding_validation_records(staged: StagedManifest) -> Tuple[Tuple[str, str, Mapping[str, Any]], ...]:
    """Recompute the R-008 footing facts from their independent source owners."""
    declaration = _json_object(staged.declaration_json, "staged manifest declaration")
    validation = declaration.get("source_binding_validation")
    if validation is None:
        return ()
    if not isinstance(validation, Mapping):
        raise ScenarioRegistryStoreError("source_binding_validation must be an object")
    keys = validation.get("capabilities")
    if not isinstance(keys, list) or any(not isinstance(key, str) or not key for key in keys):
        raise ScenarioRegistryStoreError("source_binding_validation.capabilities is malformed")

    def records(state: str, **details: Any) -> Tuple[Tuple[str, str, Mapping[str, Any]], ...]:
        return tuple((key, state, {"proof_depth": "persistence", **details}) for key in keys)

    validator = validation.get("validator")
    if validator not in {
            "r008_closure_046_source_binding",
            "r008_natural_wait_progress_source_binding",
    }:
        return records("contradicted", reason="unsupported_source_binding_validator")
    try:
        artifact_path = _repository_relative_file(
            validation.get("bootstrap_artifact"), field="source_binding_validation.bootstrap_artifact",
        )
        bootstrap_bytes = artifact_path.read_bytes()
        bootstrap = json.loads(bootstrap_bytes.decode("utf-8"))
        fixture = _repository_relative_file(
            "tools/openclaw_harness/fixtures/saves/"
            f"{declaration.get('fixture_profile', '')}/{declaration.get('fixture', '')}/manifest.json",
            field="source fixture manifest",
        )
        fixture_bytes = fixture.read_bytes()
        fixture_manifest = json.loads(fixture_bytes.decode("utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError, ScenarioRegistryStoreError) as exc:
        return records("contradicted", reason="source_binding_artifact_unavailable", error=str(exc))
    if not isinstance(bootstrap, Mapping) or not isinstance(fixture_manifest, Mapping):
        return records("contradicted", reason="source_binding_artifact_malformed")
    if validator == "r008_natural_wait_progress_source_binding":
        return _natural_wait_progress_source_binding_records(
            declaration, validation, fixture_manifest, fixture_bytes, fixture, artifact_path, bootstrap_bytes,
            staged.source_sha256,
        )

    capabilities = declaration.get("capabilities")
    source_binding = declaration.get("source_binding")
    footing = capabilities.get("world.r008_closure_046.fixed_native_footing") if isinstance(capabilities, Mapping) else None
    transform_route = capabilities.get("capabilities.shakedown.transform_free_m095") if isinstance(capabilities, Mapping) else None
    bootstrap_source = bootstrap.get("source_chain") if isinstance(bootstrap.get("source_chain"), Mapping) else {}
    bootstrap_footing = bootstrap.get("footing") if isinstance(bootstrap.get("footing"), Mapping) else {}
    fixture_source = fixture_manifest.get("source_binding") if isinstance(fixture_manifest.get("source_binding"), Mapping) else {}
    runtime_contract = declaration.get("runtime_contract")
    expected = {
        "raw_seed": str(bootstrap_source.get("raw_seed")) if isinstance(bootstrap_source, Mapping) else None,
        "world": bootstrap_source.get("world_name") if isinstance(bootstrap_source, Mapping) else None,
        "camp_omt": "177,13,0",
        "hostile_origin_omt": "177,9,0",
        "watch_omt": "174,13,0",
        "watch_route_cost": 10,
        "save_transforms": ["player_mutations"],
        "stabilizer_traits": [
            "DEBUG_LS",
            "DEBUG_NOTEMP",
            "DEBUG_STAMINA",
            "DEBUG_CARDIO",
            "DEBUG_CLAIRVOYANCE",
            "DEBUG_NIGHTVISION",
        ],
    }
    native_camp = bootstrap_footing.get("native_camp") if isinstance(bootstrap_footing.get("native_camp"), Mapping) else {}
    hostile_origin = bootstrap_footing.get("hostile_origin") if isinstance(bootstrap_footing.get("hostile_origin"), Mapping) else {}
    candidate_lane = bootstrap_footing.get("candidate_lane") if isinstance(bootstrap_footing.get("candidate_lane"), Mapping) else {}
    expected["camp_omt"] = str(native_camp.get("site_id", "")).rsplit("@", 1)[-1]
    expected["hostile_origin_omt"] = ",".join(str(item) for item in hostile_origin.get("origin_omt", ()))
    expected["watch_omt"] = ",".join(str(item) for item in candidate_lane.get("watch_omt", ()))
    expected["watch_route_cost"] = candidate_lane.get("watch_route_cost")
    checks = (
        isinstance(footing, Mapping) and dict(footing) == expected,
        isinstance(source_binding, Mapping) and source_binding.get("bootstrap_artifact") == validation.get("bootstrap_artifact"),
        fixture_source.get("raw_seed") == bootstrap_source.get("raw_seed")
        and fixture_source.get("world_name") == bootstrap_source.get("world_name")
        and fixture_source.get("native_camp") == native_camp.get("site_id")
        and fixture_source.get("hostile_origin_omt") == hostile_origin.get("origin_omt")
        and fixture_source.get("watch_omt") == candidate_lane.get("watch_omt")
        and fixture_source.get("watch_route_cost") == candidate_lane.get("watch_route_cost"),
        fixture_manifest.get("save_transforms") == [{
            "kind": "player_mutations",
            "player_save": "#R2xvcnkgVHJlam8=.sav.zzip",
            "mutations": expected["stabilizer_traits"],
        }],
        transform_route == "native_wait_save_quit_relaunch",
        isinstance(runtime_contract, Mapping)
        and "fixture-save-transform" in runtime_contract.get("forbidden_input", ()),
    )
    details = {
        "manifest_sha256": staged.source_sha256,
        "bootstrap_artifact": str(artifact_path),
        "bootstrap_sha256": hashlib.sha256(bootstrap_bytes).hexdigest(),
        "fixture_manifest": str(fixture),
        "fixture_manifest_sha256": hashlib.sha256(fixture_bytes).hexdigest(),
    }
    return records("inspected" if all(checks) else "contradicted", **details,
                   **({} if all(checks) else {"reason": "source_binding_fact_mismatch"}))


def _natural_wait_progress_source_binding_records(
    declaration: Mapping[str, Any], validation: Mapping[str, Any], fixture_manifest: Mapping[str, Any],
    fixture_bytes: bytes, fixture_path: Path, artifact_path: Path, bootstrap_bytes: bytes,
    manifest_sha256: str,
) -> Tuple[Tuple[str, str, Mapping[str, Any]], ...]:
    """Verify the R-008 observation footing from its production-world artifact."""
    keys = validation.get("capabilities", ())

    def records(state: str, **details: Any) -> Tuple[Tuple[str, str, Mapping[str, Any]], ...]:
        return tuple((str(key), state, {"proof_depth": "persistence", **details}) for key in keys)

    source_generation = fixture_manifest.get("source_generation")
    fixture_source = fixture_manifest.get("source_binding")
    capabilities = declaration.get("capabilities")
    source_binding = declaration.get("source_binding")
    footing = capabilities.get("world.natural_bandit_safe_watch_footing") if isinstance(capabilities, Mapping) else None
    # The fixture manifest does not duplicate the feasibility candidate rows.  The
    # source artifact remains the independent owner of those facts.
    try:
        bootstrap = json.loads(bootstrap_bytes.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError):
        return records("contradicted", reason="source_binding_artifact_malformed")
    rows = bootstrap.get("candidate_rows", ()) if isinstance(bootstrap, Mapping) else ()
    selected = next((row for row in rows if isinstance(row, Mapping) and row.get("outcome") == "selected"), None)
    watch = selected.get("watch", {}) if isinstance(selected, Mapping) else {}
    target = selected.get("target_omt", {}) if isinstance(selected, Mapping) else {}
    expected_traits = declaration.get("required_stabilizer_traits")
    setup_contract = declaration.get("setup_receipt_contract")
    required_player_abs_omt = (
        setup_contract.get("required_player_abs_omt")
        if isinstance(setup_contract, Mapping) else None
    )
    expected_position_transform = None
    if isinstance(required_player_abs_omt, list) and len(required_player_abs_omt) == 3:
        try:
            camp_omt = [int(value) for value in str(bootstrap.get("natural_bandit_site_id", "")).rsplit("@", 1)[1].split(",")]
            watch_omt = [int(value) for value in required_player_abs_omt]
        except (IndexError, ValueError):
            return records("contradicted", reason="source_binding_watch_position_malformed")
        expected_position_transform = {
            "kind": "player_near_overmap_special",
            "player_save": declaration.get("installed_save_player"),
            "special_id": "bandit_camp",
            "site_index": 1,
            "offset_omt": [watch_omt[index] - camp_omt[index] for index in range(3)],
        }
    expected_transform = [{
        "kind": "player_mutations",
        "player_save": declaration.get("installed_save_player"),
        "mutations": expected_traits,
    }]
    if expected_position_transform is not None:
        expected_transform.insert(0, expected_position_transform)
    expected_footing = {
        "raw_seed": str(bootstrap.get("raw_seed")),
        "camp_omt": str(bootstrap.get("natural_bandit_site_id", "")).rsplit("@", 1)[-1],
        "hostile_origin_omt": ",".join(str(target.get(axis)) for axis in ("x", "y", "z")),
        "watch_omt": ",".join(str(watch.get(axis)) for axis in ("x", "y", "z")),
        "watch_route_cost": selected.get("watch_route_cost") if isinstance(selected, Mapping) else None,
        "persistent_ecology_unchanged": True,
    }
    expected_source = {
        "bootstrap_artifact": validation.get("bootstrap_artifact"),
        "raw_seed": str(bootstrap.get("raw_seed")),
        "world_name": declaration.get("world"),
        "native_camp": bootstrap.get("natural_bandit_site_id"),
        "hostile_origin_omt": [target.get(axis) for axis in ("x", "y", "z")],
        "watch_omt": [watch.get(axis) for axis in ("x", "y", "z")],
        "watch_route_cost": selected.get("watch_route_cost") if isinstance(selected, Mapping) else None,
    }
    master_sha256 = ""
    try:
        master_sha256 = hashlib.sha256(
            (fixture_path.parent / "save" / str(declaration.get("world", "")) / "master.gsav").read_bytes()
        ).hexdigest()
    except OSError:
        pass
    checks = (
        isinstance(source_generation, Mapping)
        and source_generation.get("mode") == "production_harness_new_world"
        and source_generation.get("feasibility_artifact") == validation.get("bootstrap_artifact")
        and source_generation.get("persistent_ecology_unchanged") is True,
        isinstance(footing, Mapping) and dict(footing) == expected_footing,
        isinstance(source_binding, Mapping)
        and source_binding.get("bootstrap_artifact") == validation.get("bootstrap_artifact"),
        isinstance(fixture_source, Mapping)
        and all(fixture_source.get(key) == value for key, value in expected_source.items()),
        isinstance(fixture_source, Mapping)
        and fixture_source.get("world_master_gsav_sha256") == master_sha256,
        _fixture_transform_contains_required_mutations(fixture_manifest.get("save_transforms"), expected_transform),
        isinstance(declaration.get("runtime_contract"), Mapping)
        and "fixture-save-transform-after-install" in declaration["runtime_contract"].get("forbidden_input", ()),
    )
    details = {
        "manifest_sha256": manifest_sha256,
        "bootstrap_artifact": str(artifact_path),
        "bootstrap_sha256": hashlib.sha256(bootstrap_bytes).hexdigest(),
        "fixture_manifest_sha256": hashlib.sha256(fixture_bytes).hexdigest(),
    }
    return records("inspected" if all(checks) else "contradicted", **details,
                   **({} if all(checks) else {"reason": "source_binding_fact_mismatch"}))


def _fixture_transform_contains_required_mutations(
    transforms: Any, expected: Sequence[Mapping[str, Any]],
) -> bool:
    """Accept inherited fixture-only mutations while requiring every declared transform."""
    if not isinstance(transforms, list):
        return False
    for required in expected:
        if required.get("kind") != "player_mutations":
            if required not in transforms:
                return False
            continue
        matches = [item for item in transforms if isinstance(item, Mapping)
                   and item.get("kind") == "player_mutations"
                   and item.get("player_save") == required.get("player_save")]
        if not matches:
            return False
        if not set(required.get("mutations", ())).issubset(set(matches[0].get("mutations", ()) )):
            return False
    return True


def _append_source_binding_capability_evidence(connection: sqlite3.Connection, staged: StagedManifest) -> None:
    for capability_key, evidence_state, details in _source_binding_validation_records(staged):
        row = connection.execute(
            "SELECT value_json FROM manifest_capability_current WHERE manifest_id = ? AND capability_key = ?",
            (staged.manifest_id, capability_key),
        ).fetchone()
        if row is None:
            raise ScenarioRegistryStoreError("source-binding validation references a missing capability")
        value_json = str(row["value_json"])
        value_sha256 = _identity(
            "caol-source-binding-capability-validation-v1", staged.manifest_id, staged.source_sha256,
            capability_key, evidence_state, value_json, _json_text(details),
        )
        connection.execute(
            "INSERT OR IGNORE INTO capability_evidence_history( "
            "manifest_id, capability_key, evidence_kind, evidence_state, value_json, value_sha256, details_json "
            ") VALUES( ?, ?, 'source_binding_validation', ?, ?, ?, ? )",
            (staged.manifest_id, capability_key, evidence_state, value_json, value_sha256, _json_text(details)),
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


def detect_scenario_relations(connection: sqlite3.Connection) -> Dict[str, int]:
    """Persist structured duplicate/subsumption candidates as review evidence only.

    This owner touches relation materializations/history exclusively.  It does
    not change lifecycle, tokens, report verification, or selection behavior.
    Callers rebuilding the projection invoke it inside their existing atomic
    transaction; standalone callers receive the same transaction boundary.
    """
    candidate_kinds = ("exact_duplicate_candidate", "likely_subsumed_by_candidate")

    def detect() -> Dict[str, int]:
        rows = connection.execute(
            "SELECT manifest_id, revision, last_content_sha256, declaration_json FROM manifest_current "
            "WHERE present = 1 ORDER BY manifest_id"
        ).fetchall()
        entries = []
        for row in rows:
            declaration = _json_object(str(row["declaration_json"]), "manifest declaration")
            contract = normalize_relation_contract(declaration)
            if contract is not None:
                entries.append((
                    str(row["manifest_id"]),
                    int(row["revision"]),
                    str(row["last_content_sha256"] or ""),
                    contract,
                ))
        placeholders = ", ".join("?" for _kind in candidate_kinds)
        connection.execute(
            f"DELETE FROM manifest_relation_current WHERE relation_kind IN ( {placeholders} )",
            candidate_kinds,
        )
        candidates: List[Tuple[str, str, str]] = []
        for subject_id, _subject_revision, _subject_sha, subject_contract in entries:
            for candidate_id, _candidate_revision, _candidate_sha, candidate_contract in entries:
                if subject_id == candidate_id:
                    continue
                if subject_contract == candidate_contract:
                    candidates.append((subject_id, "exact_duplicate_candidate", candidate_id))
                elif relation_contract_likely_subsumes(subject_contract, candidate_contract):
                    candidates.append((subject_id, "likely_subsumed_by_candidate", candidate_id))
        connection.executemany(
            "INSERT INTO manifest_relation_current( manifest_id, relation_kind, target_kind, target_key, route_role ) "
            "VALUES( ?, ?, 'manifest', ?, '' )",
            candidates,
        )
        entry_by_id = {entry[0]: entry for entry in entries}
        for subject_id, relation_kind, candidate_id in candidates:
            _manifest_id, revision, source_sha256, _contract = entry_by_id[subject_id]
            target_sha256 = entry_by_id[candidate_id][2]
            connection.execute(
                "INSERT OR IGNORE INTO manifest_relation_history( "
                "manifest_id, relation_kind, target_kind, target_key, route_role, event_kind, revision, details_json "
                ") VALUES( ?, ?, 'manifest', ?, '', 'review_candidate', ?, ? )",
                (
                    subject_id,
                    relation_kind,
                    candidate_id,
                    revision,
                    _json_text({
                        "review_required": True,
                        "source_sha256": source_sha256,
                        "target_source_sha256": target_sha256,
                    }),
                ),
            )
        return {
            "exact_duplicates": sum(kind == "exact_duplicate_candidate" for _subject, kind, _target in candidates),
            "likely_subsumptions": sum(kind == "likely_subsumed_by_candidate" for _subject, kind, _target in candidates),
        }

    if connection.in_transaction:
        return detect()
    with immediate_transaction(connection):
        return detect()


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
            _append_source_binding_capability_evidence(connection, entry)

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

        detect_scenario_relations(connection)

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
    # Compact probe JSON flattens startup.screen to startup_screen.  Keep the
    # canonical extractor pointed at the same runtime owner in both forms.
    if not screen and isinstance(report.get("startup_screen"), dict):
        screen = report["startup_screen"]
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
    certification_round = dict(report.get("certification_round")) if isinstance(report.get("certification_round"), Mapping) else {}
    for key in ("round_id", "authority_id", "binding_id", "event_stream_id", "manifest_sha256", "lifecycle_state"):
        if key not in certification_round and report.get(key) is not None:
            certification_round[key] = report.get(key)
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
    fixture_install = startup.get("fixture_install")
    if isinstance(fixture_install, Mapping) and isinstance(fixture_install.get("binding"), Mapping):
        fixture["installed"]["binding"] = dict(fixture_install["binding"])
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
    profile_install = startup.get("profile_snapshot")
    if isinstance(profile_install, Mapping) and isinstance(profile_install.get("binding"), Mapping):
        profile["snapshot_install"]["binding"] = dict(profile_install["binding"])
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
        "wec_authority": validate_authority_fact(report.get("wec_authority")),
        "certification_round": certification_round,
        # Certification is deliberately evaluated from the immutable live
        # event/receipt payload; absent lifecycle data earns no final credit.
        "certification_lifecycle": report.get("certification_lifecycle"),
        "supersedes_verification_id": supersedes_verification_id,
    }


def _is_diagnostic_replay_report(report: Mapping[str, Any], facts: Mapping[str, Any]) -> bool:
    """Identify replay authority from immutable report classification fields."""
    if report.get("diagnostic_replay") is True:
        return True
    if str(report.get("mode", "")).strip().lower() in {"diagnostic_replay", "diagnostic-replay"}:
        return True
    proof = facts.get("proof")
    if isinstance(proof, Mapping) and str(proof.get("evidence_class", "")).strip().lower() == "diagnostic replay":
        return True
    authority = facts.get("wec_authority")
    return (
        isinstance(authority, Mapping)
        and isinstance(authority.get("fact"), Mapping)
        and str(authority["fact"].get("evidence_class", "")).strip().lower() == "diagnostic replay"
    )


def _is_setup_only_report(report: Mapping[str, Any], facts: Mapping[str, Any]) -> bool:
    """Reject manufactured setup facts unless sealed feature evidence follows them."""
    raw = report.get("scenario_setup", report.get("scenario_interventions"))
    if raw is None:
        return False
    if isinstance(raw, Mapping) and not raw:
        return False
    receipts = raw.get("interventions") if isinstance(raw, Mapping) else raw
    # The canonical probe always carries its setup projection.  An empty
    # projection asserts no manufactured state, so it is not a setup-only
    # report and must not block otherwise valid report ingestion.
    if receipts == []:
        return False
    if not isinstance(receipts, list) or not receipts:
        raise ScenarioRegistryStoreError("scenario setup receipts must be a non-empty list")
    for receipt in receipts:
        if not isinstance(receipt, Mapping):
            raise ScenarioRegistryStoreError("scenario setup receipt must be an object")
        if receipt.get("evidence_effect") != "none_for_manufactured_state" or receipt.get("gameplay_credit") is not False:
            raise ScenarioRegistryStoreError("manufactured scenario state must remain setup-only")
    proof = facts.get("proof")
    authority = facts.get("wec_authority")
    authority_fact = authority.get("fact") if isinstance(authority, Mapping) else None
    if (
        isinstance(proof, Mapping)
        and proof.get("status") == "green"
        and proof.get("feature_proof") is True
        and proof.get("route_feature_proof") is True
        and isinstance(authority, Mapping)
        and authority.get("status") == "sealed"
        and isinstance(authority_fact, Mapping)
        and authority_fact.get("evidence_class") == "focused feature proof"
    ):
        return False
    return True


def _append_setup_only_interventions(
    connection: sqlite3.Connection,
    *,
    facts: Mapping[str, Any],
    report: Mapping[str, Any],
) -> Tuple[str, ...]:
    """Bind report setup receipts to the current manifest without verification credit."""
    raw = report.get("scenario_setup", report.get("scenario_interventions"))
    receipts = raw.get("interventions") if isinstance(raw, Mapping) else raw
    if not isinstance(receipts, list):
        raise ScenarioRegistryStoreError("scenario setup receipts are unavailable")
    manifest_source = _object(facts["manifest"], "manifest")
    manifest = connection.execute(
        "SELECT manifest_id, current_sha256 FROM manifest_current WHERE source_path = ? AND present = 1 "
        "AND current_sha256 = ?",
        (str(manifest_source["source_path"]), str(manifest_source["source_sha256"])),
    ).fetchone()
    if manifest is None:
        # The firewall still accepts a setup-only diagnostic reference without
        # a projected owner.  It records no lifecycle history and can never
        # acquire verification or final-gate credit.
        return ()
    validation = connection.execute(
        "SELECT validation_id FROM scenario_validation_history WHERE manifest_id = ? AND manifest_sha256 = ? "
        "AND validation_status = 'valid' ORDER BY recorded_at DESC, validation_id DESC LIMIT 1",
        (str(manifest["manifest_id"]), str(manifest["current_sha256"])),
    ).fetchone()
    result = []
    for receipt in receipts:
        if not isinstance(receipt, Mapping):
            raise ScenarioRegistryStoreError("scenario setup receipt is malformed")
        operation = _string(receipt.get("operation"), "scenario setup operation")
        arguments = receipt.get("arguments", {})
        target = receipt.get("target", {})
        native = receipt.get("native_receipt", {})
        before = receipt.get("before_facts", {})
        after = receipt.get("after_facts", {})
        if not all(isinstance(item, Mapping) for item in (arguments, target, native, before, after)):
            raise ScenarioRegistryStoreError("scenario setup receipt fields must be objects")
        intervention_id = _identity(
            "caol-scenario-intervention-v1", str(manifest["manifest_id"]),
            str(validation["validation_id"]) if validation is not None else "",
            operation, _json_text(arguments), _json_text(target), _json_text(native),
        )
        connection.execute(
            "INSERT OR IGNORE INTO scenario_intervention_history( "
            "intervention_id, manifest_id, validation_id, operation, arguments_json, target_json, native_receipt_json, "
            "before_facts_json, after_facts_json, evidence_effect, preparation_status "
            ") VALUES( ?, ?, ?, ?, ?, ?, ?, ?, ?, 'none_for_manufactured_state', ? )",
            (
                intervention_id, str(manifest["manifest_id"]),
                str(validation["validation_id"]) if validation is not None else None,
                operation, _json_text(arguments), _json_text(target), _json_text(native),
                _json_text(before), _json_text(after),
                "prepared" if native.get("accepted") is True else "unprepared",
            ),
        )
        result.append(intervention_id)
    return tuple(result)


def _validate_required_r008_setup_receipt(
    connection: sqlite3.Connection,
    *,
    facts: Mapping[str, Any],
    report: Mapping[str, Any],
) -> None:
    """Fail closed when an R-008 report loses or alters its fixture-only setup binding."""
    manifest_source = _object(facts["manifest"], "manifest")
    manifest = connection.execute(
        "SELECT declaration_json FROM manifest_current WHERE source_path = ? AND present = 1 AND current_sha256 = ?",
        (str(manifest_source["source_path"]), str(manifest_source["source_sha256"])),
    ).fetchone()
    if manifest is None:
        raise ScenarioRegistryStoreError("R-008 setup receipt manifest is stale or unavailable")
    declaration = _json_object(str(manifest["declaration_json"]), "R-008 setup receipt manifest declaration")
    contract = declaration.get("setup_receipt_contract")
    if not isinstance(contract, Mapping) or contract.get("kind") != "r008_source_bound_fixture":
        return
    raw = report.get("scenario_setup")
    if not isinstance(raw, Mapping) or raw.get("contract_kind") != "r008_source_bound_fixture":
        raise ScenarioRegistryStoreError("R-008 setup receipts are unavailable")
    receipts = raw.get("interventions")
    if not isinstance(receipts, list) or len(receipts) != 1 or not isinstance(receipts[0], Mapping):
        raise ScenarioRegistryStoreError("R-008 setup receipts must contain one source-bound receipt")
    receipt = receipts[0]
    arguments = receipt.get("arguments")
    target = receipt.get("target")
    native = receipt.get("native_receipt")
    before = receipt.get("before_facts")
    after = receipt.get("after_facts")
    source = contract.get("source_binding")
    traits = contract.get("stabilizer_traits")
    if not all(isinstance(value, Mapping) for value in (arguments, target, native, before, after, source)) or not isinstance(traits, list):
        raise ScenarioRegistryStoreError("R-008 setup receipt is malformed")
    expected_world = str(contract.get("world", ""))
    expected_save = str(contract.get("player_save", ""))
    expected_player_abs_omt = contract.get("required_player_abs_omt")
    if expected_player_abs_omt is not None and (
            not isinstance(expected_player_abs_omt, list) or len(expected_player_abs_omt) != 3):
        raise ScenarioRegistryStoreError("R-008 setup receipt required player OMT is malformed")
    transform = native.get("transform")
    position_transform = native.get("position_transform")
    expected_arguments = {"world": expected_world, "player_save": expected_save,
                          "stabilizer_traits": traits}
    if expected_player_abs_omt is not None:
        expected_arguments["required_player_abs_omt"] = expected_player_abs_omt
    exact = (
        raw.get("status") == "prepared"
        and receipt.get("operation") == "fixture_install_and_r008_source_bound_stabilizer_setup"
        and arguments == expected_arguments
        and target == {"world": expected_world, "player_save": expected_save}
        and native.get("owner") == "fixture_save_transform"
        and native.get("accepted") is True
        and isinstance(transform, Mapping)
        and transform.get("kind") == "player_mutations"
        and transform.get("player_save") == expected_save
        and transform.get("mutations") == traits
        and (
            expected_player_abs_omt is None or (
                isinstance(position_transform, Mapping)
                and position_transform.get("kind") == "player_near_overmap_special"
                and position_transform.get("player_save") == expected_save
                and position_transform.get("target_omt") == expected_player_abs_omt
                and after.get("player_abs_omt") == expected_player_abs_omt
            )
        )
        and before.get("source_binding") == source
        and after.get("source_binding") == source
        and after.get("master_gsav_sha256") == source.get("world_master_gsav_sha256")
        and isinstance(after.get("player_traits"), list)
        and all(trait in after.get("player_traits") for trait in traits)
        and receipt.get("evidence_effect") == "none_for_manufactured_state"
        and receipt.get("gameplay_credit") is False
    )
    if not exact:
        raise ScenarioRegistryStoreError("R-008 setup receipt is stale, mismatched, or fabricated")


def _certification_round_check(
    connection: sqlite3.Connection, *, authority: Mapping[str, Any],
    round_facts: Mapping[str, Any], report_facts: Mapping[str, Any],
) -> Dict[str, Any]:
    """Check final-round credit against registry-owned, current round facts."""
    round_id = str(round_facts.get("round_id", "") or "").strip()
    if not round_id:
        return {"eligible": False, "reason": "missing_certification_round"}
    row = connection.execute("SELECT * FROM certification_round WHERE round_id = ?", (round_id,)).fetchone()
    if row is None:
        return {"eligible": False, "reason": "unregistered_certification_round", "round_id": round_id}
    derived = certification_round_facts(connection, round_id)
    expected = {key: derived[key] for key in (
        "round_id", "authority_id", "binding_id", "event_stream_id", "manifest_sha256",
    )}
    for key, value in expected.items():
        supplied = str(round_facts.get(key, "") or "").strip()
        if not supplied:
            return {"eligible": False, "reason": f"missing_certification_round_{key}", "round_id": round_id}
        if supplied != value:
            return {"eligible": False, "reason": f"certification_round_{key}_mismatch", "round_id": round_id}
    if str(authority.get("authority_id", "")) != expected["authority_id"]:
        return {"eligible": False, "reason": "certification_round_authority_mismatch", "round_id": round_id}
    if str(authority.get("run_id", "")) != round_id or str(authority.get("binding_id", "")) != expected["binding_id"]:
        return {"eligible": False, "reason": "certification_round_authority_identity_mismatch", "round_id": round_id}
    lifecycle = connection.execute(
        "SELECT event_kind, scenario_lineage_id, authority_id, binding_id, event_stream_id "
        "FROM certification_round_lifecycle WHERE round_id = ? ORDER BY event_sequence DESC LIMIT 1", (round_id,)
    ).fetchone()
    state = str(round_facts.get("lifecycle_state", "") or "").strip().lower()
    if lifecycle is None or str(lifecycle["event_kind"]).lower() not in {"active", "complete", "completed", "started"}:
        return {"eligible": False, "reason": "certification_round_lifecycle_not_active", "round_id": round_id}
    current_state = str(lifecycle["event_kind"]).lower()
    if state and state not in {current_state, "complete" if current_state == "completed" else current_state}:
        return {"eligible": False, "reason": "certification_round_lifecycle_mismatch", "round_id": round_id}
    if any(str(lifecycle[key]) != expected[key] for key in ("authority_id", "binding_id", "event_stream_id")):
        return {"eligible": False, "reason": "certification_round_event_identity_mismatch", "round_id": round_id}
    manifest = json.loads(str(row["manifest_json"]))
    authoritative = manifest["binding"]["authoritative_components"]
    expected_source = str(authoritative["scenario"].get("content_sha256", ""))
    observed_source = str((report_facts.get("manifest") or {}).get("source_sha256", "") or "").strip()
    if not expected_source or expected_source != observed_source:
        return {"eligible": False, "reason": "certification_round_manifest_source_mismatch", "round_id": round_id}
    expected_runtime = str(authoritative["executable"].get("content_sha256", ""))
    observed_runtime = str(((report_facts.get("runtime") or {}).get("runtime_binding_observed") or {}).get("executable_sha256", "") or "").strip()
    if not expected_runtime or expected_runtime != observed_runtime:
        return {"eligible": False, "reason": "certification_round_current_executable_mismatch", "round_id": round_id}
    if connection.execute("SELECT 1 FROM certification_round_invalidation WHERE round_id = ?", (round_id,)).fetchone() is not None:
        return {"eligible": False, "reason": "certification_round_invalidated", "round_id": round_id}
    lifecycle_payload = report_facts.get("certification_lifecycle")
    if not isinstance(lifecycle_payload, Mapping):
        return {"eligible": False, "reason": "missing_continuous_lifecycle", "round_id": round_id}
    lifecycle_check = evaluate_continuous_certification(
        round_id=round_id,
        binding_id=expected["binding_id"],
        world_id=str(lifecycle_payload.get("world_id", "")),
        player_id=str(lifecycle_payload.get("player_id", "")),
        actor_ids=lifecycle_payload.get("actor_ids", ()),
        events=lifecycle_payload.get("events", ()),
        authority=str(authority.get("authority", "")),
    )
    if lifecycle_check.get("status") != "green":
        return {
            "eligible": False,
            "reason": "continuous_lifecycle_failed",
            "first_divergence": lifecycle_check.get("first_divergence", ""),
            "round_id": round_id,
        }
    return {"eligible": True, "reason": "current_certification_round", "round_id": round_id}


def _report_named_gate_verdicts(
    report: Mapping[str, Any],
) -> Tuple[Dict[str, Tuple[str, ...]], Dict[str, Tuple[str, ...]]]:
    """Retain only named structured ledger verdicts; prose and HUD fields grant nothing."""
    startup_collected: Dict[str, List[str]] = {}
    step_collected: Dict[str, List[str]] = {}
    startup = report.get("startup") if isinstance(report.get("startup"), Mapping) else {}
    ledgers: Tuple[Tuple[Any, str, Dict[str, List[str]]], ...] = (
        (startup.get("startup_step_ledger"), "step", startup_collected),
        (report.get("step_ledger"), "primitive_step", step_collected),
    )
    for ledger, label_key, collected in ledgers:
        if not isinstance(ledger, list):
            continue
        for row in ledger:
            if not isinstance(row, Mapping):
                continue
            label = str(row.get(label_key, "")).strip()
            verdict = str(row.get("verdict", "")).strip().lower()
            if label and verdict:
                collected.setdefault(label, []).append(verdict)
    return (
        {label: tuple(verdicts) for label, verdicts in startup_collected.items()},
        {label: tuple(verdicts) for label, verdicts in step_collected.items()},
    )


def _declared_capability_gates(declaration: Mapping[str, Any]) -> Mapping[str, Mapping[str, Tuple[str, ...]]]:
    proof_route = declaration.get("proof_route")
    capabilities = declaration.get("capabilities")
    if not isinstance(proof_route, Mapping) or not isinstance(capabilities, Mapping):
        return {}
    raw_gates = proof_route.get("capability_gates")
    if not isinstance(raw_gates, Mapping):
        return {}
    positive_labels = {
        str(label)
        for role in ("precondition", "production_behavior", "terminal_persistence", "artifact_verdict")
        for label in proof_route.get(role, [])
        if isinstance(label, str) and label.strip()
    }
    gates: Dict[str, Mapping[str, Tuple[str, ...]]] = {}
    for capability_key, depth_gates in raw_gates.items():
        if not isinstance(capability_key, str) or capability_key not in capabilities or not isinstance(depth_gates, Mapping):
            continue
        mapped: Dict[str, Tuple[str, ...]] = {}
        for depth, labels in depth_gates.items():
            if depth not in _KNOWN_PROOF_DEPTHS or not isinstance(labels, list):
                continue
            names = tuple(str(label).strip() for label in labels if isinstance(label, str) and str(label).strip())
            if names and all(label in positive_labels for label in names):
                mapped[str(depth)] = names
        if mapped:
            gates[capability_key] = mapped
    return gates


def _append_named_capability_gate_evidence(
    connection: sqlite3.Connection,
    *,
    manifest_id: str,
    verification_id: str,
    declaration: Mapping[str, Any],
    report: Mapping[str, Any],
) -> None:
    """Append only explicit, named green/red gates for declared capabilities."""
    capability_gates = _declared_capability_gates(declaration)
    if not capability_gates:
        return
    declared_values = {
        str(row["capability_key"]): str(row["value_json"])
        for row in connection.execute(
            "SELECT capability_key, value_json FROM manifest_capability_current WHERE manifest_id = ?",
            (manifest_id,),
        )
    }
    startup_gates, step_gates = _report_named_gate_verdicts(report)
    for capability_key, depth_gates in capability_gates.items():
        value_json = declared_values.get(capability_key)
        if value_json is None:
            continue
        for depth, labels in depth_gates.items():
            observed = startup_gates if depth == "startup" else step_gates
            verdicts = tuple(observed.get(label, ()) for label in labels)
            if any(any(verdict.startswith(("red", "blocked")) for verdict in gate_verdicts) for gate_verdicts in verdicts):
                evidence_state = "contradicted"
            elif all(gate_verdicts and all(verdict.startswith("green") for verdict in gate_verdicts) for gate_verdicts in verdicts):
                evidence_state = "inspected" if depth == "startup" else "run-verified"
            else:
                continue
            details = {
                "proof_depth": depth,
                "gate_labels": list(labels),
                "gate_verdicts": {
                    label: list(observed.get(label, ()))
                    for label in labels
                },
            }
            value_sha256 = _identity(
                "caol-scenario-named-capability-gate-v1",
                manifest_id,
                verification_id,
                capability_key,
                evidence_state,
                value_json,
                _json_text(details),
            )
            connection.execute(
                "INSERT OR IGNORE INTO capability_evidence_history( "
                "manifest_id, verification_id, capability_key, evidence_kind, evidence_state, value_json, value_sha256, details_json "
                ") VALUES( ?, ?, ?, 'named_proof_gate', ?, ?, ?, ? )",
                (
                    manifest_id,
                    verification_id,
                    capability_key,
                    evidence_state,
                    value_json,
                    value_sha256,
                    _json_text(details),
                ),
            )


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
        "manifest_id, report_id, binding_kind, binding_fingerprint, binding_status, payload_json "
        ") VALUES( ?, ?, ?, ?, ?, ? )",
        (manifest_id, report_id, kind, fingerprint, status, _json_text(payload)),
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
    route_verdict = str(proof.get("route_verdict", "")).strip().lower()
    # A focused transaction may keep its aggregate report yellow when startup
    # support is inconclusive while still persisting a decisive red route
    # verdict.  Route lifecycle owns that verdict; treating it as unknown
    # hides the unresolved current contradiction from repair authority.
    if status == "red" or route_verdict.startswith("red_"):
        return "contradicted"
    repair_bootstrap = details.get("repair_bootstrap") if isinstance(details, dict) else None
    if isinstance(repair_bootstrap, Mapping) and repair_bootstrap.get("zero_credit") is True and \
            repair_bootstrap.get("terminal_result") == "current_source_runtime_compatible":
        return "repair_bootstrap_proven"
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
        "SELECT value_sha256 FROM capability_evidence_history "
        "WHERE manifest_id = ? AND verification_id IS NULL AND capability_key = '_registry.proof_route' "
        "AND evidence_kind = 'route_resolution' "
        "ORDER BY capability_evidence_id DESC LIMIT 1",
        (manifest_id,),
    ).fetchone()
    if existing is None or str(existing["value_sha256"]) != value_sha256:
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
    details_json = _json_text(details)
    latest = connection.execute(
        "SELECT quarantine_kind, details_json FROM quarantine_history WHERE manifest_id = ? AND route_key = ? "
        "ORDER BY quarantine_event_id DESC LIMIT 1",
        (manifest_id, route_key),
    ).fetchone()
    if latest is not None and str(latest["quarantine_kind"]) == quarantine_kind and \
            str(latest["details_json"]) == details_json:
        return
    connection.execute(
        "INSERT INTO quarantine_history( manifest_id, route_key, quarantine_kind, details_json ) "
        "VALUES( ?, ?, ?, ? )",
        (manifest_id, route_key, quarantine_kind, details_json),
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


_QUARANTINE_REASONS = frozenset({
    "invalid",
    "blocked",
    "broken",
    "contradicted",
    "stale",
    "retirement_pending",
})


def quarantine_scenario(
    connection: sqlite3.Connection,
    *,
    reason: str,
    details: Mapping[str, Any],
    manifest_id: Optional[str] = None,
    route_key: Optional[str] = None,
    source_path: Optional[Path | str] = None,
    source_sha256: Optional[str] = None,
    quarantine_kind: Optional[str] = None,
    lifecycle_event_kind: Optional[str] = None,
    invalidation_reason: Optional[str] = None,
) -> Dict[str, Any]:
    """Append one nonselectable quarantine outcome without changing source or retirement state.

    A migration may encounter bytes which cannot become a manifest projection.  Its
    immutable migration terminal remains the evidence in that case.  When a prior
    projection shares the source path, this owner also preserves an explicit
    quarantine event and invalidates every outstanding selection token for it.
    """
    quarantine_reason = str(reason).strip().lower()
    if quarantine_reason not in _QUARANTINE_REASONS:
        raise ScenarioRegistryStoreError(f"unsupported quarantine reason: {quarantine_reason}")
    if manifest_id is None and source_path is None:
        raise ScenarioRegistryStoreError("quarantine requires a manifest_id or source_path")
    if source_sha256 is not None:
        source_sha256 = str(source_sha256).strip().lower()
        if len(source_sha256) != 64 or any(character not in "0123456789abcdef" for character in source_sha256):
            raise ScenarioRegistryStoreError("quarantine source SHA-256 must be a lowercase hexadecimal digest")

    def append() -> Dict[str, Any]:
        target_manifest_id = str(manifest_id).strip() if manifest_id is not None else ""
        canonical_source_path = str(Path(source_path).resolve()) if source_path is not None else ""
        if target_manifest_id:
            manifest = connection.execute(
                "SELECT manifest_id, source_path, present FROM manifest_current WHERE manifest_id = ?",
                (target_manifest_id,),
            ).fetchone()
        else:
            manifest = connection.execute(
                "SELECT manifest_id, source_path, present FROM manifest_current WHERE source_path = ?",
                (canonical_source_path,),
            ).fetchone()
        if manifest is None:
            return {"manifest_id": None, "quarantined": False, "invalidated_tokens": 0}
        if canonical_source_path and str(manifest["source_path"]) != canonical_source_path:
            raise ScenarioRegistryStoreError("quarantine manifest and source path do not match")
        if not bool(manifest["present"]):
            return {
                "manifest_id": str(manifest["manifest_id"]),
                "quarantined": False,
                "invalidated_tokens": 0,
                "source_absent": True,
            }
        target_manifest_id = str(manifest["manifest_id"])
        target_route_key = str(route_key).strip() if route_key is not None else "_migration"
        if not target_route_key:
            raise ScenarioRegistryStoreError("quarantine route key must not be empty")
        evidence = dict(details)
        evidence.update({
            "quarantine_reason": quarantine_reason,
            "source_path": canonical_source_path or str(manifest["source_path"]),
        })
        if source_sha256 is not None:
            evidence["source_sha256"] = source_sha256
        _append_quarantine_if_changed(
            connection,
            manifest_id=target_manifest_id,
            route_key=target_route_key,
            quarantine_kind=quarantine_kind or f"quarantined_{quarantine_reason}",
            details=evidence,
        )
        _append_lifecycle_if_changed(
            connection,
            manifest_id=target_manifest_id,
            event_kind=lifecycle_event_kind or f"quarantine_{quarantine_reason}",
            details=evidence,
        )
        invalidated = _invalidate_manifest_tokens(
            connection,
            manifest_id=target_manifest_id,
            reason=invalidation_reason or f"quarantine_{quarantine_reason}",
            details=evidence,
        )
        return {
            "manifest_id": target_manifest_id,
            "quarantined": True,
            "invalidated_tokens": invalidated,
        }

    if connection.in_transaction:
        return append()
    with immediate_transaction(connection):
        return append()


_RETIREMENT_REASONS = frozenset({
    "cannot_launch_unique_diagnostic",
    "exact_duplicate",
    "fully_subsumed",
    "temporary_historical_one_off",
    "missing_fixture_helper",
    "startup_only_superseded",
})


def _retirement_candidate_rows(
    connection: sqlite3.Connection, *, manifest_ids: Sequence[str] = (),
) -> Dict[str, Dict[str, Any]]:
    """Derive review-only candidate reasons from present source and relation evidence."""
    candidates: Dict[str, Dict[str, Any]] = {}
    requested = tuple(sorted({str(item).strip() for item in manifest_ids if str(item).strip()}))
    clause = ""
    if requested:
        clause = " WHERE manifest_id IN (" + ", ".join("?" for _ in requested) + ")"
    rows = connection.execute(
        "SELECT manifest_id, source_path, current_sha256, present FROM manifest_current" + clause + " ORDER BY manifest_id",
        requested,
    ).fetchall()
    for row in rows:
        manifest_id = str(row["manifest_id"])
        if not bool(row["present"]):
            continue
        route_evidence = _current_route_evidence(connection, manifest_id)
        lifecycle_state, lifecycle_reason = _current_lifecycle_state(
            connection,
            manifest_id=manifest_id,
            present=True,
            route_evidence=route_evidence,
        )
        if lifecycle_state == "retired":
            continue
        reasons: Dict[str, list[Dict[str, str]]] = {}
        successors: Dict[str, Dict[str, str]] = {}
        relations = connection.execute(
            "SELECT relation_kind, target_key FROM manifest_relation_current WHERE manifest_id = ? "
            "AND target_kind = 'manifest' ORDER BY relation_kind, target_key",
            (manifest_id,),
        ).fetchall()
        for relation in relations:
            relation_kind = str(relation["relation_kind"])
            successor_id = str(relation["target_key"])
            reason = {
                "exact_duplicate_candidate": "exact_duplicate",
                "likely_subsumed_by_candidate": "fully_subsumed",
            }.get(relation_kind)
            if reason is None:
                continue
            reasons.setdefault(reason, []).append({
                "relation_kind": relation_kind,
                "successor_manifest_id": successor_id,
            })
            successors[successor_id] = {
                "manifest_id": successor_id,
                "relation_kind": relation_kind,
            }
        quarantine_rows = connection.execute(
            "SELECT quarantine_kind, details_json FROM quarantine_history WHERE manifest_id = ? "
            "ORDER BY quarantine_event_id",
            (manifest_id,),
        ).fetchall()
        for quarantine in quarantine_rows:
            kind = str(quarantine["quarantine_kind"])
            details = _json_object(str(quarantine["details_json"]), "quarantine details")
            migration_reason = str(details.get("migration_reason", ""))
            if kind == "quarantined_blocked" and "fixture" in migration_reason:
                reasons.setdefault("missing_fixture_helper", []).append({"quarantine_kind": kind})
            elif kind == "quarantined_broken":
                reasons.setdefault("cannot_launch_unique_diagnostic", []).append({"quarantine_kind": kind})
        if reasons:
            candidates[manifest_id] = {
                "manifest_id": manifest_id,
                "source_path": str(row["source_path"]),
                "source_sha256": str(row["current_sha256"] or ""),
                "lifecycle": {"state": lifecycle_state, "reason": lifecycle_reason},
                "reasons": {key: tuple(value) for key, value in sorted(reasons.items())},
                "successors": tuple(successors[key] for key in sorted(successors)),
            }
    return candidates


def retirement_candidates(connection: sqlite3.Connection) -> Tuple[Mapping[str, Any], ...]:
    """Return inspect-only retirement candidates; this owner changes no lifecycle state."""
    candidates = _retirement_candidate_rows(connection)
    return tuple(candidates[key] for key in sorted(candidates))


def _retirement_action_history(connection: sqlite3.Connection, action_id: str) -> Tuple[Mapping[str, Any], ...]:
    rows = connection.execute(
        "SELECT event_kind, details_json, recorded_at FROM retirement_action_history "
        "WHERE action_id = ? ORDER BY retirement_action_event_id",
        (action_id,),
    ).fetchall()
    return tuple({
        "event_kind": str(row["event_kind"]),
        "details": dict(_json_object(str(row["details_json"]), "retirement action details")),
        "recorded_at": str(row["recorded_at"]),
    } for row in rows)


def registry_status(
    connection: sqlite3.Connection,
    *,
    include_lifecycle_states: Sequence[str] = (),
    manifest_ids: Sequence[str] = (),
) -> Tuple[Mapping[str, Any], ...]:
    """Expose active status by default, with explicit inspect-only lifecycle expansion."""
    snapshots = build_registry_query_candidate_snapshot(
        connection,
        include_lifecycle_states=include_lifecycle_states,
        manifest_ids=manifest_ids,
    )
    candidates = _retirement_candidate_rows(connection, manifest_ids=manifest_ids)
    result = []
    for snapshot in snapshots:
        manifest_id = str(snapshot.scenario_id)
        source_path = str(snapshot.explanation["manifest"]["source_path"])
        relations = tuple({
            "relation_kind": str(row["relation_kind"]),
            "target_kind": str(row["target_kind"]),
            "target_key": str(row["target_key"]),
            "route_role": str(row["route_role"]),
        } for row in connection.execute(
            "SELECT relation_kind, target_kind, target_key, route_role FROM manifest_relation_current "
            "WHERE manifest_id = ? ORDER BY relation_kind, target_kind, target_key, route_role",
            (manifest_id,),
        ))
        action_rows = connection.execute(
            "SELECT DISTINCT action_id FROM retirement_action_history WHERE manifest_id = ? ORDER BY action_id",
            (manifest_id,),
        ).fetchall()
        result.append({
            "manifest": snapshot.explanation["manifest"],
            "lifecycle": snapshot.explanation["lifecycle"],
            "token_eligible": snapshot.token_eligible,
            "retirement_candidate": candidates.get(manifest_id),
            "relations": relations,
            "r019_acceptance_matrix": tuple({
                "guarded_report_id": str(row["guarded_report_id"]),
                "primitive_report_id": str(row["primitive_report_id"]),
                "status": str(row["status"]),
                "details": dict(_json_object(str(row["details_json"]), "R-019 acceptance matrix details")),
                "recorded_at": str(row["recorded_at"]),
            } for row in connection.execute(
                "SELECT guarded_report_id, primitive_report_id, status, details_json, recorded_at "
                "FROM r019_acceptance_matrix_evaluation_history WHERE manifest_id = ? "
                "ORDER BY matrix_evaluation_event_id",
                (manifest_id,),
            )),
            "playtest_witnesses": tuple({
                "witness_id": str(row["witness_id"]),
                "verdict": str(row["verdict"]),
                "evidence_ceiling": str(row["evidence_ceiling"]),
                "review_decision": str(row["decision"] or ""),
                "recorded_at": str(row["recorded_at"]),
            } for row in connection.execute(
                "SELECT witness.witness_id, witness.verdict, witness.evidence_ceiling, "
                "review.decision, witness.recorded_at FROM playtest_witness_history AS witness "
                "LEFT JOIN playtest_witness_review_history AS review ON review.witness_id = witness.witness_id "
                "WHERE witness.manifest_id = ? ORDER BY witness.recorded_at, witness.witness_id", (manifest_id,),
            )),
            "history": {
                "lifecycle": tuple({
                    "event_kind": str(row["event_kind"]),
                    "details": dict(_json_object(str(row["details_json"]), "lifecycle details")),
                    "recorded_at": str(row["recorded_at"]),
                } for row in connection.execute(
                    "SELECT event_kind, details_json, recorded_at FROM lifecycle_history "
                    "WHERE manifest_id = ? ORDER BY lifecycle_event_id", (manifest_id,),
                )),
                "retirement": tuple({
                    "retirement_kind": str(row["retirement_kind"]),
                    "authority": str(row["authority"]),
                    "reason": str(row["reason"]),
                    "details": dict(_json_object(str(row["details_json"]), "retirement details")),
                    "recorded_at": str(row["recorded_at"]),
                } for row in connection.execute(
                    "SELECT retirement_kind, authority, reason, details_json, recorded_at FROM retirement_history "
                    "WHERE manifest_id = ? ORDER BY retirement_event_id", (manifest_id,),
                )),
                "relations": tuple({
                    "relation_kind": str(row["relation_kind"]),
                    "target_kind": str(row["target_kind"]),
                    "target_key": str(row["target_key"]),
                    "route_role": str(row["route_role"]),
                    "event_kind": str(row["event_kind"]),
                    "revision": int(row["revision"]),
                    "details": dict(_json_object(str(row["details_json"]), "relation details")),
                    "recorded_at": str(row["recorded_at"]),
                } for row in connection.execute(
                    "SELECT relation_kind, target_kind, target_key, route_role, event_kind, revision, details_json, recorded_at "
                    "FROM manifest_relation_history WHERE manifest_id = ? ORDER BY relation_event_id", (manifest_id,),
                )),
                "verifications": tuple({
                    "verification_id": str(row["verification_id"]),
                    "report_id": str(row["report_id"]),
                    "route_key": str(row["route_key"]),
                    "outcome_kind": str(row["outcome_kind"]),
                    "proof_status": str(row["proof_status"]),
                    "details": dict(_json_object(str(row["details_json"]), "verification details")),
                    "recorded_at": str(row["recorded_at"]),
                } for row in connection.execute(
                    "SELECT verification_id, report_id, route_key, outcome_kind, proof_status, details_json, recorded_at "
                    "FROM verification_history WHERE manifest_id = ? ORDER BY recorded_at, verification_id", (manifest_id,),
                )),
                "report_ingestions": tuple({
                    "report_id": str(row["report_id"]),
                    "report_path": str(row["report_path"]),
                    "report_sha256": str(row["report_sha256"]),
                    "report_kind": str(row["report_kind"]),
                    "ingestion_status": str(row["ingestion_status"]),
                    "error": str(row["error_text"]),
                    "recorded_at": str(row["recorded_at"]),
                } for row in connection.execute(
                    "SELECT report_id, report_path, report_sha256, report_kind, ingestion_status, error_text, recorded_at "
                    "FROM report_ingestion_history WHERE manifest_id = ? ORDER BY recorded_at, report_id", (manifest_id,),
                )),
                "evidence": tuple({
                    "capability_key": str(row["capability_key"]),
                    "evidence_kind": str(row["evidence_kind"]),
                    "evidence_state": str(row["evidence_state"]),
                    "details": dict(_json_object(str(row["details_json"]), "evidence details")),
                    "recorded_at": str(row["recorded_at"]),
                } for row in connection.execute(
                    "SELECT capability_key, evidence_kind, evidence_state, details_json, recorded_at "
                    "FROM capability_evidence_history WHERE manifest_id = ? ORDER BY capability_evidence_id", (manifest_id,),
                )),
                "migration": tuple({
                    "migration_run_id": str(row["migration_run_id"]),
                    "event_kind": str(row["event_kind"]),
                    "disposition": str(row["disposition"]),
                    "reason": str(row["reason"]),
                    "details": dict(_json_object(str(row["details_json"]), "migration details")),
                    "recorded_at": str(row["recorded_at"]),
                } for row in connection.execute(
                    "SELECT migration_run_id, event_kind, disposition, reason, details_json, recorded_at FROM migration_item "
                    "WHERE source_path = ? ORDER BY migration_item_event_id", (source_path,),
                )),
                "actions": tuple({"action_id": str(row["action_id"]), "events": _retirement_action_history(connection, str(row["action_id"]))}
                                 for row in action_rows),
            },
        })
    return tuple(result)


def _active_canonical_manifest(connection: sqlite3.Connection, manifest_id: str) -> Mapping[str, Any]:
    row = connection.execute(
        "SELECT manifest_id, source_path, current_sha256, present FROM manifest_current WHERE manifest_id = ?",
        (manifest_id,),
    ).fetchone()
    if row is None or not bool(row["present"]):
        raise ScenarioRegistryStoreError("retirement successor is not source-present")
    route_evidence = _current_route_evidence(connection, manifest_id)
    state, _reason = _current_lifecycle_state(
        connection, manifest_id=manifest_id, present=True, route_evidence=route_evidence,
    )
    if state != "active":
        raise ScenarioRegistryStoreError("retirement successor is not active")
    try:
        observed_sha256 = hashlib.sha256(Path(str(row["source_path"])).read_bytes()).hexdigest()
    except OSError as exc:
        raise ScenarioRegistryStoreError(f"retirement successor source is unavailable: {exc}") from exc
    if observed_sha256 != str(row["current_sha256"] or ""):
        raise ScenarioRegistryStoreError("retirement successor source SHA-256 is stale")
    return {"manifest_id": str(row["manifest_id"]), "source_path": str(row["source_path"]), "source_sha256": observed_sha256}


def _retirement_coverage_guard(
    connection: sqlite3.Connection,
    *,
    manifest_id: str,
    successor_manifest_id: str,
) -> Mapping[str, Any]:
    subject_capabilities = {
        str(row["capability_key"]): (str(row["value_json"]), str(row["declared_state"]))
        for row in connection.execute(
            "SELECT capability_key, value_json, declared_state FROM manifest_capability_current WHERE manifest_id = ?",
            (manifest_id,),
        )
    }
    successor_capabilities = {
        str(row["capability_key"]): (str(row["value_json"]), str(row["declared_state"]))
        for row in connection.execute(
            "SELECT capability_key, value_json, declared_state FROM manifest_capability_current WHERE manifest_id = ?",
            (successor_manifest_id,),
        )
    }
    missing_capabilities = sorted(
        key for key, value in subject_capabilities.items() if successor_capabilities.get(key) != value
    )
    subject_routes = {
        (str(row["route_role"]), str(row["step_label"]))
        for row in connection.execute(
            "SELECT route_role, step_label FROM manifest_proof_route_current WHERE manifest_id = ?", (manifest_id,),
        )
    }
    successor_routes = {
        (str(row["route_role"]), str(row["step_label"]))
        for row in connection.execute(
            "SELECT route_role, step_label FROM manifest_proof_route_current WHERE manifest_id = ?", (successor_manifest_id,),
        )
    }
    missing_routes = sorted(subject_routes - successor_routes)
    missing_negative_controls = [
        route for route in missing_routes if "negative" in route[0] or "disallowed" in route[0]
    ]
    missing_failure_controls = [route for route in missing_routes if "failure" in route[0]]
    return {
        "required_capabilities": sorted(subject_capabilities),
        "missing_capabilities": missing_capabilities,
        "required_proof_routes": [list(route) for route in sorted(subject_routes)],
        "missing_proof_routes": [list(route) for route in missing_routes],
        "missing_negative_controls": [list(route) for route in missing_negative_controls],
        "missing_failure_controls": [list(route) for route in missing_failure_controls],
        "coverage_ready": not missing_capabilities and not missing_routes,
    }


def approve_retirement(
    connection: sqlite3.Connection,
    *,
    manifest_id: str,
    successor_manifest_id: str,
    source_sha256: str,
    reason: str,
    reviewer_identity: str,
    approval: str,
) -> Mapping[str, Any]:
    """Prepare one reviewer-approved, SHA-bound retirement without deleting its source."""
    approved_reason = str(reason).strip()
    reviewer = str(reviewer_identity).strip()
    if approved_reason not in _RETIREMENT_REASONS:
        raise ScenarioRegistryStoreError("retirement reason is not owner-approved")
    if not reviewer or str(approval).strip().lower() != "approved":
        raise ScenarioRegistryStoreError("retirement requires explicit reviewer approval")
    requested_sha256 = str(source_sha256).strip().lower()
    if len(requested_sha256) != 64 or any(character not in "0123456789abcdef" for character in requested_sha256):
        raise ScenarioRegistryStoreError("retirement source SHA-256 must be a lowercase hexadecimal digest")
    with immediate_transaction(connection):
        subject = connection.execute(
            "SELECT source_path, current_sha256, present FROM manifest_current WHERE manifest_id = ?", (manifest_id,),
        ).fetchone()
        if subject is None or not bool(subject["present"]):
            raise ScenarioRegistryStoreError("retirement subject is not source-present")
        subject_routes = _current_route_evidence(connection, manifest_id)
        subject_state, _subject_reason = _current_lifecycle_state(
            connection, manifest_id=manifest_id, present=True, route_evidence=subject_routes,
        )
        if subject_state != "active":
            raise ScenarioRegistryStoreError("retirement subject is not active")
        if requested_sha256 != str(subject["current_sha256"] or ""):
            raise ScenarioRegistryStoreError("retirement subject SHA-256 is stale")
        try:
            observed_sha256 = hashlib.sha256(Path(str(subject["source_path"])).read_bytes()).hexdigest()
        except OSError as exc:
            raise ScenarioRegistryStoreError(f"retirement subject source is unavailable: {exc}") from exc
        if observed_sha256 != requested_sha256:
            raise ScenarioRegistryStoreError("retirement subject source SHA-256 changed")
        candidates = _retirement_candidate_rows(connection)
        candidate = candidates.get(manifest_id)
        if candidate is None or approved_reason not in candidate["reasons"]:
            raise ScenarioRegistryStoreError("retirement subject has no matching review candidate")
        approved_successors = {
            evidence["successor_manifest_id"]
            for evidence in candidate["reasons"][approved_reason]
            if "successor_manifest_id" in evidence
        }
        if successor_manifest_id not in approved_successors:
            raise ScenarioRegistryStoreError("retirement successor is not supported by the approved reason")
        successor = _active_canonical_manifest(connection, successor_manifest_id)
        coverage = _retirement_coverage_guard(
            connection, manifest_id=manifest_id, successor_manifest_id=successor_manifest_id,
        )
        if not bool(coverage["coverage_ready"]):
            raise ScenarioRegistryStoreError("retirement would remove required active coverage")
        action_id = _identity(
            "caol-scenario-retirement-action-v1", manifest_id, successor_manifest_id,
            requested_sha256, approved_reason, reviewer,
        )
        existing = connection.execute(
            "SELECT retirement_action_event_id FROM retirement_action_history WHERE action_id = ? AND event_kind = 'approved'",
            (action_id,),
        ).fetchone()
        details = {
            "approval": "approved",
            "coverage": coverage,
            "successor": successor,
        }
        if existing is None:
            connection.execute(
                "INSERT INTO retirement_action_history( action_id, manifest_id, successor_manifest_id, source_path, "
                "source_sha256, reviewer_identity, retirement_reason, event_kind, details_json ) "
                "VALUES( ?, ?, ?, ?, ?, ?, ?, 'approved', ? )",
                (action_id, manifest_id, successor_manifest_id, str(subject["source_path"]), requested_sha256,
                 reviewer, approved_reason, _json_text(details)),
            )
        quarantine_scenario(
            connection,
            manifest_id=manifest_id,
            route_key="_retirement",
            reason="retirement_pending",
            details={"action_id": action_id, "successor_manifest_id": successor_manifest_id},
        )
    return {"action_id": action_id, "approved": True, "idempotent": existing is not None, "coverage": coverage}


def execute_retirement_action(connection: sqlite3.Connection, action_id: str) -> Mapping[str, Any]:
    """Delete only an approved exact source, then append retirement or resumable failure evidence."""
    action = connection.execute(
        "SELECT manifest_id, successor_manifest_id, source_path, source_sha256, reviewer_identity, retirement_reason "
        "FROM retirement_action_history WHERE action_id = ? AND event_kind = 'approved'",
        (action_id,),
    ).fetchone()
    if action is None:
        raise ScenarioRegistryStoreError("retirement action is not approved")
    history = _retirement_action_history(connection, action_id)
    if any(event["event_kind"] == "retired" for event in history):
        return {"action_id": action_id, "retired": True, "idempotent": True}
    manifest_id = str(action["manifest_id"])
    successor_manifest_id = str(action["successor_manifest_id"])
    source_path = Path(str(action["source_path"]))
    source_sha256 = str(action["source_sha256"])
    try:
        _active_canonical_manifest(connection, successor_manifest_id)
        coverage = _retirement_coverage_guard(
            connection, manifest_id=manifest_id, successor_manifest_id=successor_manifest_id,
        )
        if not bool(coverage["coverage_ready"]):
            raise ScenarioRegistryStoreError("retirement would remove required active coverage")
        if source_path.exists():
            observed_sha256 = hashlib.sha256(source_path.read_bytes()).hexdigest()
            if observed_sha256 != source_sha256:
                raise ScenarioRegistryStoreError("retirement subject source SHA-256 changed")
            source_path.unlink()
    except (OSError, ScenarioRegistryStoreError) as exc:
        with immediate_transaction(connection):
            connection.execute(
                "INSERT OR IGNORE INTO retirement_action_history( action_id, manifest_id, successor_manifest_id, source_path, "
                "source_sha256, reviewer_identity, retirement_reason, event_kind, details_json ) "
                "VALUES( ?, ?, ?, ?, ?, ?, ?, 'failed', ? )",
                (action_id, manifest_id, successor_manifest_id, str(source_path), source_sha256,
                 str(action["reviewer_identity"]), str(action["retirement_reason"]), _json_text({"error": str(exc)})),
            )
            quarantine_scenario(
                connection, manifest_id=manifest_id, route_key="_retirement", reason="broken",
                details={"action_id": action_id, "error": str(exc)},
            )
        return {"action_id": action_id, "retired": False, "resumable": True, "error": str(exc)}
    with immediate_transaction(connection):
        subject = connection.execute(
            "SELECT present, last_content_sha256 FROM manifest_current WHERE manifest_id = ?", (manifest_id,),
        ).fetchone()
        if subject is None or str(subject["last_content_sha256"] or "") != source_sha256:
            raise ScenarioRegistryStoreError("retirement action no longer matches its manifest history")
        existing_retirement = connection.execute(
            "SELECT retirement_event_id FROM retirement_history WHERE manifest_id = ?", (manifest_id,)
        ).fetchone()
        if existing_retirement is None:
            details = {
                "action_id": action_id,
                "successor_manifest_id": successor_manifest_id,
                "source_path": str(source_path),
                "source_sha256": source_sha256,
            }
            connection.execute(
                "UPDATE manifest_current SET present = 0, current_sha256 = NULL, absent_at = CURRENT_TIMESTAMP "
                "WHERE manifest_id = ?",
                (manifest_id,),
            )
            connection.execute(
                "INSERT INTO lifecycle_history( manifest_id, event_kind, revision, cause_sha256, details_json ) "
                "SELECT manifest_id, 'retired', revision, last_content_sha256, ? FROM manifest_current WHERE manifest_id = ?",
                (_json_text(details), manifest_id),
            )
            connection.execute(
                "INSERT INTO retirement_history( manifest_id, retirement_kind, authority, reason, details_json ) "
                "VALUES( ?, 'approved_source_removal', ?, ?, ? )",
                (manifest_id, str(action["reviewer_identity"]), str(action["retirement_reason"]), _json_text(details)),
            )
        connection.execute(
            "INSERT OR IGNORE INTO retirement_action_history( action_id, manifest_id, successor_manifest_id, source_path, "
            "source_sha256, reviewer_identity, retirement_reason, event_kind, details_json ) "
            "VALUES( ?, ?, ?, ?, ?, ?, ?, 'retired', ? )",
            (action_id, manifest_id, successor_manifest_id, str(source_path), source_sha256,
             str(action["reviewer_identity"]), str(action["retirement_reason"]), _json_text({"source_removed": True})),
        )
    return {"action_id": action_id, "retired": True, "idempotent": False}


def _resolve_route_evidence(
    connection: sqlite3.Connection,
    *,
    manifest_id: str,
    route_key: str,
) -> str:
    """Append the current route decision without rewriting report or declaration history."""
    rows = connection.execute(
        "SELECT verification_id, proof_status, supersedes_verification_id, details_json FROM verification_history AS verification "
        "WHERE manifest_id = ? AND route_key = ? AND EXISTS( "
        "SELECT 1 FROM verification_resolution_history AS resolution "
        "WHERE resolution.verification_id = verification.verification_id "
        "AND resolution.resolution_event_id = ( SELECT MAX( latest.resolution_event_id ) "
        "FROM verification_resolution_history AS latest WHERE latest.verification_id = verification.verification_id ) "
        "AND resolution.resolution_kind = 'compatible' )",
        (manifest_id, route_key),
    ).fetchall()
    # Preserve the newest focused red route verdict even when a later yellow
    # report is still binding-compatible.  That report has not proved the
    # route, so it cannot erase the red transaction or prevent the exact route
    # from receiving a fresh repair authority.  A hard-proven successor below
    # still resolves this contradiction through its explicit supersession.
    historical = connection.execute(
        "SELECT verification_id, proof_status, supersedes_verification_id, details_json "
        "FROM verification_history WHERE manifest_id = ? AND route_key = ? "
        "ORDER BY recorded_at DESC, verification_id DESC",
        (manifest_id, route_key),
    ).fetchall()
    focused_red = None
    for row in historical:
        details = json.loads(str(row["details_json"]))
        proof = details.get("proof", {}) if isinstance(details, dict) else {}
        if (
                str(proof.get("status", "")).strip().lower() == "yellow"
                and str(proof.get("route_verdict", "")).strip().lower().startswith("red_")):
            focused_red = row
            break
    if focused_red is not None and all(
            str(row["verification_id"]) != str(focused_red["verification_id"])
            for row in rows):
        rows = tuple(rows) + (focused_red,)
    by_id = {str(row["verification_id"]): row for row in rows}
    hard_proven = {
        verification_id
        for verification_id, row in by_id.items()
        if _verification_evidence_state(row) == "hard_proven"
    }
    hard_proven = _playtest_witness_hard_proven_candidates(
        connection, manifest_id=manifest_id, route_key=route_key,
        candidate_rows=by_id, report_local_hard_proven=hard_proven,
    )
    contradicted = {
        verification_id
        for verification_id, row in by_id.items()
        if _verification_evidence_state(row) == "contradicted"
    }
    repair_bootstrap_proven = {
        verification_id
        for verification_id, row in by_id.items()
        if _verification_evidence_state(row) == "repair_bootstrap_proven"
    }
    superseded = {
        str(row["supersedes_verification_id"])
        for row in by_id.values()
        if (
            _verification_evidence_state(row) in {"hard_proven", "repair_bootstrap_proven"}
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
    elif repair_bootstrap_proven:
        evidence_state = "run-verified"
    else:
        evidence_state = "unknown"
    details = {
        "route_key": route_key,
        "compatible_verification_ids": sorted(by_id),
        "hard_proven_verification_ids": sorted(hard_proven),
        "repair_bootstrap_verification_ids": sorted(repair_bootstrap_proven),
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
    current_bootstrap_authority = _current_bootstrap_revalidation(
        connection,
        manifest_id=manifest_id,
        route_evidence=_current_route_evidence(connection, manifest_id),
    )
    if not rows and current_bootstrap_authority is None:
        quarantine_scenario(
            connection,
            manifest_id=manifest_id,
            route_key=route_key,
            reason="stale",
            details=details,
            quarantine_kind="quarantined_no_compatible_verification",
            lifecycle_event_kind="proof_route_stale",
            invalidation_reason="proof_route_stale",
        )
    elif not rows:
        _append_lifecycle_if_changed(
            connection,
            manifest_id=manifest_id,
            event_kind="current_bootstrap_authority_retained",
            details=current_bootstrap_authority,
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
    elif evidence_state in {"contradicted", "stale"}:
        quarantine_scenario(
            connection,
            manifest_id=manifest_id,
            route_key=route_key,
            reason=evidence_state,
            details=details,
            lifecycle_event_kind=f"proof_route_{evidence_state}",
            invalidation_reason=f"proof_route_{evidence_state}",
        )
    elif evidence_state != "hard_proven":
        _invalidate_outstanding_tokens(
            connection,
            manifest_id=manifest_id,
            route_key=route_key,
            reason=f"proof_route_{evidence_state}",
            details=details,
        )
        _append_lifecycle_if_changed(
            connection,
            manifest_id=manifest_id,
            event_kind=f"proof_route_{evidence_state}",
            details=details,
        )
    else:
        _append_lifecycle_if_changed(
            connection,
            manifest_id=manifest_id,
            event_kind=f"proof_route_{evidence_state}",
            details=details,
        )
    return evidence_state


def _r019_matrix_hard_proven_candidates(
    connection: sqlite3.Connection,
    *,
    manifest_id: str,
    route_key: str,
    candidate_rows: Mapping[str, sqlite3.Row],
    report_local_hard_proven: set[str],
) -> set[str]:
    """Return R-019 candidates only when their current matrix is green.

    A report-local feature verdict is intentionally insufficient for this
    route.  The persisted relation must name every current candidate report;
    any missing, stale, superseded, duplicate, or red input leaves the whole
    route unproved.
    """
    declaration = connection.execute(
        "SELECT declaration_json FROM manifest_current WHERE manifest_id = ?", (manifest_id,)
    ).fetchone()
    if declaration is None or _json_object(
            str(declaration["declaration_json"]), "R-019 acceptance declaration"
    ).get("name") != "r019.keep_watch_acceptance_mcw":
        return report_local_hard_proven
    event = connection.execute(
        "SELECT status, details_json FROM r019_acceptance_matrix_evaluation_history "
        "WHERE manifest_id = ? ORDER BY matrix_evaluation_event_id DESC LIMIT 1",
        (manifest_id,),
    ).fetchone()
    if event is None or str(event["status"]) != "green":
        return set()
    details = _json_object(str(event["details_json"]), "R-019 acceptance matrix details")
    inputs = details.get("inputs")
    if not isinstance(inputs, Mapping) or not inputs:
        return set()
    report_ids: set[str] = set()
    for packet in inputs.values():
        if not isinstance(packet, Mapping):
            return set()
        report_id = str(packet.get("report_id", "")).strip()
        if not report_id or report_id in report_ids:
            return set()
        report_ids.add(report_id)
    rows = connection.execute(
        "SELECT verification_id, report_id, route_key, supersedes_verification_id FROM verification_history "
        "WHERE manifest_id = ?",
        (manifest_id,),
    ).fetchall()
    input_verifications = {
        str(row["verification_id"]): row for row in rows
        if str(row["report_id"]) in report_ids and str(row["route_key"]) == route_key
    }
    if len(input_verifications) != len(report_ids):
        return set()
    verification_ids = set(input_verifications)
    if verification_ids - set(candidate_rows) or verification_ids - report_local_hard_proven:
        return set()
    superseded = connection.execute(
        "SELECT supersedes_verification_id FROM verification_history WHERE manifest_id = ? "
        "AND supersedes_verification_id IS NOT NULL",
        (manifest_id,),
    ).fetchall()
    if any(str(row["supersedes_verification_id"]) in verification_ids for row in superseded):
        return set()
    return verification_ids


def _playtest_witness_hard_proven_candidates(
    connection: sqlite3.Connection, *, manifest_id: str, route_key: str,
    candidate_rows: Mapping[str, sqlite3.Row], report_local_hard_proven: set[str],
) -> set[str]:
    """Let a current accepted cited witness repair classification for any playtest."""
    declaration_row = connection.execute(
        "SELECT declaration_json FROM manifest_current WHERE manifest_id = ?", (manifest_id,)
    ).fetchone()
    declaration = _json_object(
        str(declaration_row["declaration_json"]), "playtest witness declaration",
    ) if declaration_row is not None else {}
    runtime_contract = declaration.get("runtime_contract")
    permitted_input = runtime_contract.get("permitted_input", []) \
        if isinstance(runtime_contract, Mapping) else []
    witness_required = "cockpit:run.witness" in permitted_input
    witness_exists = connection.execute(
        "SELECT 1 FROM playtest_witness_history WHERE manifest_id = ? LIMIT 1",
        (manifest_id,),
    ).fetchone() is not None
    if not witness_required and not witness_exists:
        return report_local_hard_proven
    accepted = connection.execute(
        "SELECT witness.witness_id FROM playtest_witness_history AS witness "
        "JOIN playtest_witness_review_history AS review ON review.review_id = ("
        "SELECT latest.review_id FROM playtest_witness_review_history AS latest "
        "WHERE latest.witness_id = witness.witness_id "
        "ORDER BY latest.rowid DESC LIMIT 1) "
        "WHERE witness.manifest_id = ? AND witness.verdict = 'proved' "
        "AND witness.evidence_ceiling = 'focused' AND review.decision = 'accept' "
        "ORDER BY review.recorded_at DESC, review.review_id DESC LIMIT 1",
        (manifest_id,),
    ).fetchone()
    if accepted is None:
        return set()
    report_ids = {
        str(row["report_id"]) for row in connection.execute(
            "SELECT report_id FROM playtest_witness_report_history WHERE witness_id = ?",
            (str(accepted["witness_id"]),),
        )
    }
    if not report_ids:
        return set()
    rows = connection.execute(
        "SELECT verification_id, report_id, route_key FROM verification_history WHERE manifest_id = ?", (manifest_id,)
    ).fetchall()
    verification_ids = {str(row["verification_id"]) for row in rows if str(row["report_id"]) in report_ids and str(row["route_key"]) == route_key}
    if len(verification_ids) != len(report_ids) or verification_ids - set(candidate_rows):
        return set()
    placeholders = ",".join("?" for _ in verification_ids)
    if verification_ids and connection.execute(
            "SELECT 1 FROM verification_history AS prior JOIN verification_history AS successor "
            "ON successor.supersedes_verification_id = prior.verification_id "
            f"WHERE prior.verification_id IN ({placeholders}) LIMIT 1",
            tuple(verification_ids),
    ).fetchone() is not None:
        return set()
    return verification_ids


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


def _capsule_binding_from_report(
    connection: sqlite3.Connection, report: Mapping[str, Any], facts: Mapping[str, Any]
) -> Tuple[str, Mapping[str, Any]]:
    """Resolve a complete immutable round binding or an explicit focused binding."""
    round_facts = facts.get("certification_round")
    round_id = str(round_facts.get("round_id", "")).strip() if isinstance(round_facts, Mapping) else ""
    if round_id:
        row = connection.execute(
            "SELECT binding_id, manifest_json FROM certification_round WHERE round_id = ?", (round_id,)
        ).fetchone()
        if row is None:
            raise ScenarioRegistryStoreError("diagnostic capsule round binding is not registry-owned")
        manifest = _json_object(str(row["manifest_json"]), "certification round manifest")
        binding = manifest.get("binding")
        if not isinstance(binding, Mapping) or str(binding.get("sha256", "")) != str(row["binding_id"]):
            raise ScenarioRegistryStoreError("diagnostic capsule round binding is stale")
        return str(row["binding_id"]), binding
    raw = report.get("capsule_binding")
    if not isinstance(raw, Mapping):
        raw = report.get("diagnostic_capsule_binding")
    if not isinstance(raw, Mapping):
        raise ScenarioRegistryStoreError("diagnostic capsule binding is missing")
    required = ("state", "player", "actors", "owner")
    if any(key not in raw or raw[key] in (None, "", [], {}) for key in required):
        raise ScenarioRegistryStoreError("diagnostic capsule binding is incomplete")
    binding = json.loads(json.dumps(raw, sort_keys=True, separators=(",", ":")))
    binding_id = str(raw.get("binding_id", raw.get("sha256", ""))).strip()
    if not binding_id:
        binding_id = hashlib.sha256(_json_text(binding).encode("utf-8")).hexdigest()
    return binding_id, binding


def _append_capsule_candidate_for_report(
    connection: sqlite3.Connection,
    *, report: Mapping[str, Any], facts: Mapping[str, Any], report_id: str,
    verification_id: str, report_path: str, report_sha256: str,
) -> Optional[Dict[str, Any]]:
    """Append one candidate only after report verification and artifact checks."""
    raw = report.get("diagnostic_capsule_candidate")
    if raw is None:
        raw = report.get("capsule_candidate")
    if raw is None:
        return None
    if not isinstance(raw, Mapping):
        raise ScenarioRegistryStoreError("diagnostic capsule candidate must be an object")
    source_kind = str(raw.get("source_kind", raw.get("kind", "focused"))).strip().lower()
    if source_kind in {"diagnostic_replay", "replay", "diagnostic-replay"}:
        raise ScenarioRegistryStoreError("diagnostic replay cannot create a capsule candidate")
    proof = facts.get("proof") if isinstance(facts.get("proof"), Mapping) else {}
    if str(proof.get("status", "")).strip().lower() not in {"green", "passed", "complete", "completed"}:
        raise ScenarioRegistryStoreError("diagnostic capsule candidate requires a green proof")
    cleanup = raw.get("cleanup", report.get("cleanup"))
    if isinstance(cleanup, Mapping):
        accepted = cleanup.get("accepted", cleanup.get("status") in {"accepted", "green", "complete"})
        if accepted is not True:
            raise ScenarioRegistryStoreError("diagnostic capsule candidate requires accepted cleanup")
    artifact = raw.get("saved_artifact", raw.get("saved_receipt", raw.get("artifact")))
    if not isinstance(artifact, Mapping):
        raise ScenarioRegistryStoreError("diagnostic capsule saved artifact is missing")
    artifact_path = str(artifact.get("path", "")).strip()
    artifact_sha256 = str(artifact.get("sha256", artifact.get("hash", ""))).strip().lower()
    if not artifact_path or len(artifact_sha256) != 64 or any(c not in "0123456789abcdef" for c in artifact_sha256):
        raise ScenarioRegistryStoreError("diagnostic capsule artifact reference is malformed")
    try:
        artifact_bytes = Path(artifact_path).resolve(strict=True).read_bytes()
    except OSError as exc:
        raise ScenarioRegistryStoreError("diagnostic capsule artifact is unreadable") from exc
    if hashlib.sha256(artifact_bytes).hexdigest() != artifact_sha256:
        raise ScenarioRegistryStoreError("diagnostic capsule artifact hash mismatch")
    binding_id, binding = _capsule_binding_from_report(connection, report, facts)
    site_id = _string(raw.get("site_id", raw.get("site", "")), "diagnostic_capsule_candidate.site_id")
    operation = _string(raw.get("operation", ""), "diagnostic_capsule_candidate.operation")
    generation = str(raw.get("generation", "")).strip()
    owner = _string(raw.get("owner", binding.get("owner", "")), "diagnostic_capsule_candidate.owner")
    actor_ids = raw.get("actor_ids", raw.get("actors"))
    if not isinstance(actor_ids, list) or not actor_ids or not all(isinstance(item, str) and item.strip() for item in actor_ids):
        raise ScenarioRegistryStoreError("diagnostic capsule candidate actors are missing")
    if not generation:
        raise ScenarioRegistryStoreError("diagnostic capsule candidate generation is missing")
    gate_id = _string(raw.get("gate_id", ""), "diagnostic_capsule_candidate.gate_id")
    gate_index = raw.get("gate_index")
    if type(gate_index) is not int or gate_index < 0:
        raise ScenarioRegistryStoreError("diagnostic capsule candidate gate index is invalid")
    if str(raw.get("gate_verdict", raw.get("verdict", "green"))).lower() not in {"green", "passed", "complete", "completed"}:
        raise ScenarioRegistryStoreError("diagnostic capsule candidate gate is not green")
    startup_gates, step_gates = _report_named_gate_verdicts(report)
    proven_verdicts = startup_gates.get(gate_id, ()) + step_gates.get(gate_id, ())
    if not proven_verdicts or not all(verdict.startswith("green") for verdict in proven_verdicts):
        raise ScenarioRegistryStoreError("diagnostic capsule candidate gate is not report-proven")
    durable_timestamp = _string(raw.get("durable_timestamp", raw.get("timestamp", "")), "diagnostic_capsule_candidate.durable_timestamp")
    run_id = _string(report.get("run_id", raw.get("run_id", "")), "diagnostic_capsule_candidate.run_id")
    candidate_id = _identity(
        "caol-diagnostic-capsule-candidate-v1", artifact_path, artifact_sha256, binding_id, site_id, operation, generation,
        _json_text(sorted(actor_ids)), owner, gate_id, str(gate_index), durable_timestamp,
    )
    payload = {
        "candidate_id": candidate_id, "report_id": report_id, "verification_id": verification_id,
        "run_id": run_id, "report_path": report_path, "report_sha256": report_sha256,
        "artifact_path": str(Path(artifact_path).resolve()), "artifact_sha256": artifact_sha256,
        "binding_id": binding_id, "binding_json": _json_text(binding), "site_id": site_id,
        "operation": operation, "generation": generation, "actor_ids_json": _json_text(sorted(actor_ids)),
        "owner": owner, "gate_id": gate_id, "gate_index": gate_index,
        "durable_timestamp": durable_timestamp, "source_kind": source_kind,
        "details_json": _json_text({"source_run_id": run_id, "source_report_id": report_id}),
    }
    existing = connection.execute(
        "SELECT * FROM diagnostic_capsule_candidate WHERE candidate_id = ?", (candidate_id,)
    ).fetchone()
    if existing is not None:
        if any(str(existing[key]) != str(payload[key]) for key in payload if key != "candidate_id"):
            raise ScenarioRegistryStoreError("conflicting duplicate diagnostic capsule candidate")
        return {"candidate_id": candidate_id, "idempotent": True}
    connection.execute(
        "INSERT INTO diagnostic_capsule_candidate( "
        "candidate_id, report_id, verification_id, run_id, report_path, report_sha256, artifact_path, artifact_sha256, "
        "binding_id, binding_json, site_id, operation, generation, actor_ids_json, owner, gate_id, gate_index, "
        "durable_timestamp, source_kind, details_json ) VALUES( ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ? )",
        tuple(payload.values()),
    )
    return {"candidate_id": candidate_id, "idempotent": False}


def query_diagnostic_capsule_candidates(
    connection: sqlite3.Connection, *, binding_id: str, run_id: Optional[str] = None,
) -> Tuple[Mapping[str, Any], ...]:
    """Return immutable compatible candidates; never expose the querying run's own row."""
    params: List[Any] = [binding_id]
    sql = "SELECT * FROM diagnostic_capsule_candidate WHERE binding_id = ?"
    if run_id is not None:
        sql += " AND run_id <> ?"
        params.append(run_id)
    sql += " ORDER BY gate_index DESC, durable_timestamp DESC, candidate_id"
    rows = connection.execute(sql, params).fetchall()
    return tuple(dict(row) for row in rows)


_CAPSULE_BINDING_DIMENSIONS = ("state", "player", "actors", "owner")
_CAPSULE_IDENTITY_DIMENSIONS = ("site_id", "operation", "generation", "actor_ids", "owner")


def _capsule_selector_value(value: Any) -> Any:
    """Canonicalize a selector value without accepting missing values."""
    return json.loads(_json_text(value))


def select_diagnostic_capsule_candidate(
    connection: sqlite3.Connection, *, failed_run: Mapping[str, Any], run_id: Optional[str] = None,
) -> Mapping[str, Any]:
    """Recommend a compatible immutable capsule, without changing registry state.

    Every binding and identity dimension is checked independently.  Historical
    candidates that fail compatibility remain in the result with reasons; this
    is deliberately a recommendation only and never issues launch authority.
    """
    if not isinstance(failed_run, Mapping):
        raise ScenarioRegistryQueryError("failed_run must be an object")
    binding = failed_run.get("binding", failed_run.get("capsule_binding"))
    if not isinstance(binding, Mapping):
        raise ScenarioRegistryQueryError("failed_run binding is missing")
    expected_binding = {
        key: binding.get(key) for key in _CAPSULE_BINDING_DIMENSIONS
    }
    expected_binding_id = str(failed_run.get("binding_id", failed_run.get("sha256", ""))).strip()
    expected_identity = {
        "site_id": failed_run.get("site_id", failed_run.get("site")),
        "operation": failed_run.get("operation"),
        "generation": failed_run.get("generation"),
        "actor_ids": failed_run.get("actor_ids", failed_run.get("actors")),
        "owner": failed_run.get("owner", binding.get("owner")),
    }
    missing = [key for key, value in (("binding_id", expected_binding_id), *expected_binding.items(), *expected_identity.items())
               if value in (None, "", [], {})]
    if missing:
        raise ScenarioRegistryQueryError("failed_run dimensions missing: " + ", ".join(missing))
    expected_identity["actor_ids"] = sorted(str(item) for item in expected_identity["actor_ids"])

    source_run_id = str(run_id if run_id is not None else failed_run.get("run_id", "")).strip()
    rows = connection.execute(
        "SELECT * FROM diagnostic_capsule_candidate ORDER BY candidate_id"
    ).fetchall()
    compatible: List[Mapping[str, Any]] = []
    rejected: List[Mapping[str, Any]] = []
    for sqlite_row in rows:
        row = dict(sqlite_row)
        if source_run_id and str(row.get("run_id", "")) == source_run_id:
            continue
        reasons: List[Mapping[str, Any]] = []

        def reject(dimension: str, reason: str, observed: Any = None) -> None:
            reasons.append({"dimension": dimension, "reason": reason, "observed": observed})

        if not row.get("binding_id"):
            reject("binding_id", "missing")
        elif str(row["binding_id"]) != expected_binding_id:
            reject("binding_id", "unequal", row["binding_id"])
        try:
            candidate_binding = json.loads(str(row.get("binding_json", "")))
        except (TypeError, ValueError):
            candidate_binding = None
        if not isinstance(candidate_binding, Mapping):
            reject("binding", "stale")
        else:
            for dimension, expected in expected_binding.items():
                if dimension not in candidate_binding or candidate_binding[dimension] in (None, "", [], {}):
                    reject("binding." + dimension, "missing")
                elif _capsule_selector_value(candidate_binding[dimension]) != _capsule_selector_value(expected):
                    reject("binding." + dimension, "unequal", candidate_binding[dimension])
        for dimension, expected in expected_identity.items():
            column = "actor_ids_json" if dimension == "actor_ids" else dimension
            raw_observed = row.get(column)
            if raw_observed in (None, "", [], {}):
                reject(dimension, "missing")
                continue
            if dimension == "actor_ids":
                try:
                    observed = sorted(str(item) for item in json.loads(str(raw_observed)))
                except (TypeError, ValueError):
                    reject(dimension, "stale", raw_observed)
                    continue
            else:
                observed = str(raw_observed)
            if observed != expected:
                reject(dimension, "unequal", observed)
        gate_index = row.get("gate_index")
        if type(gate_index) is not int or gate_index < 0:
            reject("gate_index", "stale", gate_index)
        if not str(row.get("durable_timestamp", "")).strip():
            reject("durable_timestamp", "missing")
        if reasons:
            rejected.append({"candidate": row, "reasons": tuple(reasons)})
        else:
            compatible.append(row)

    # Stable passes make each ranking dimension explicit: ID ascending breaks
    # exact ties, then durable time descending, then proven gate descending.
    compatible.sort(key=lambda row: str(row["candidate_id"]))
    compatible.sort(key=lambda row: str(row["durable_timestamp"]), reverse=True)
    compatible.sort(key=lambda row: int(row["gate_index"]), reverse=True)
    selected = compatible[0] if compatible else None
    if selected is None:
        rank_reason = "no compatible diagnostic capsule"
    else:
        rank_reason = (
            "highest proven gate index; then latest durable timestamp; then lexicographically smallest candidate_id "
            f"({selected['candidate_id']})"
        )
    return {
        "selected_candidate": selected,
        "selected": selected,
        "selection_reason": rank_reason,
        "rank_reason": rank_reason,
        "compatible_candidates": tuple(compatible),
        "rejected_candidates": tuple(rejected),
    }


select_diagnostic_capsule = select_diagnostic_capsule_candidate


def _witness_from_report(value: Mapping[str, Any]) -> Optional[Dict[str, Any]]:
    """Read the generic live-final witness without knowing the claim trajectory."""
    for step in value.get("steps", []):
        live = step.get("cockpit_live_session") if isinstance(step, Mapping) else None
        final = live.get("final") if isinstance(live, Mapping) else None
        detail = final.get("stop_detail") if isinstance(final, Mapping) else None
        validation = detail.get("witness_validation") if isinstance(detail, Mapping) else None
        journal = detail.get("evidence_journal") if isinstance(detail, Mapping) else None
        if isinstance(validation, Mapping) and isinstance(journal, Mapping):
            return {"validation": dict(validation), "journal": dict(journal)}
    return None


def record_playtest_witness(
    connection: sqlite3.Connection, *, manifest_id: str, report_ids: Sequence[str],
    charter: Mapping[str, Any], journal: Mapping[str, Any], statement: Mapping[str, Any],
) -> Dict[str, Any]:
    """Persist one mechanically grounded witness over immutable report references."""
    reports = tuple(dict.fromkeys(str(item).strip() for item in report_ids if str(item).strip()))
    if not reports:
        raise ScenarioRegistryStoreError("playtest witness requires an immutable report reference")
    found = connection.execute(
        "SELECT COUNT(*) FROM report_ingestion_history WHERE manifest_id = ? AND report_id IN (" +
        ",".join("?" for _ in reports) + ") AND ingestion_status = 'ingested'",
        (manifest_id, *reports),
    ).fetchone()
    if found is None or int(found[0]) != len(reports):
        raise ScenarioRegistryStoreError("playtest witness report binding is missing or mismatched")
    identities_value = journal.get("identities") \
        if journal.get("schema") == "caol-playtest-evidence-journal-set-v1" \
        else [journal.get("identity")]
    if not isinstance(identities_value, list) or any(
            not isinstance(identity, Mapping) for identity in identities_value):
        raise ScenarioRegistryStoreError("playtest witness journal identity is missing")
    if len(identities_value) != len(reports):
        raise ScenarioRegistryStoreError("playtest witness run/report identity count mismatched")
    expected_identities: set[tuple[str, str, str, str, str]] = set()
    for report_id in reports:
        row = connection.execute(
            "SELECT report.report_path, report.report_sha256, verification.details_json "
            "FROM report_ingestion_history AS report "
            "JOIN verification_history AS verification ON verification.report_id = report.report_id "
            "WHERE report.report_id = ? ORDER BY verification.recorded_at DESC, verification.rowid DESC LIMIT 1",
            (report_id,),
        ).fetchone()
        if row is None:
            raise ScenarioRegistryStoreError("playtest witness report verification identity is missing")
        try:
            report_path, report_bytes = _report_path_and_bytes(Path(str(row["report_path"])))
            if hashlib.sha256(report_bytes).hexdigest() != str(row["report_sha256"]):
                raise ScenarioRegistryStoreError("playtest witness report identity is stale")
            report_value = json.loads(report_bytes.decode("utf-8"))
            details = _json_object(str(row["details_json"]), "playtest witness verification")
        except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
            raise ScenarioRegistryStoreError("playtest witness report identity is unreadable") from exc
        finals = [
            live.get("final")
            for step in report_value.get("steps", []) if isinstance(step, Mapping)
            for live in [step.get("cockpit_live_session")]
            if isinstance(live, Mapping) and isinstance(live.get("final"), Mapping)
        ]
        if len(finals) != 1:
            raise ScenarioRegistryStoreError("playtest witness report has no unique live final identity")
        runtime = details.get("runtime")
        observed = runtime.get("runtime_binding_observed") \
            if isinstance(runtime, Mapping) else None
        manifest = details.get("manifest")
        if not isinstance(observed, Mapping) or not isinstance(manifest, Mapping):
            raise ScenarioRegistryStoreError("playtest witness source or executable identity is missing")
        expected_identities.add((
            str(report_value.get("scenario", "")),
            str(manifest.get("source_sha256", "")),
            str(observed.get("executable_sha256", "")),
            str(finals[0].get("run_id", "")),
            str(finals[0].get("binding_id", "")),
        ))
    supplied_identities = {
        (
            str(identity.get("scenario_id", "")),
            str(identity.get("source_identity", "")),
            str(identity.get("executable_identity", "")),
            str(identity.get("run_id", "")),
            str(identity.get("binding_id", "")),
        )
        for identity in identities_value
    }
    if len(supplied_identities) != len(identities_value) or supplied_identities != expected_identities:
        raise ScenarioRegistryStoreError("playtest witness source/executable/run/ownership binding mismatched")
    try:
        validation = validate_witness_statement(
            charter=charter, journal=journal, statement=statement,
        )
    except WitnessError as exc:
        raise ScenarioRegistryStoreError(str(exc)) from exc
    normalized = validation["witness"]
    witness_id = _identity(
        "caol-playtest-witness-v1", manifest_id, *reports,
        str(validation["journal_sha256"]), str(normalized["witness_sha256"]),
    )
    def write_witness() -> None:
        connection.execute(
            "INSERT OR IGNORE INTO playtest_witness_history( witness_id, manifest_id, charter_json, "
            "journal_json, statement_json, validation_json, verdict, evidence_ceiling ) "
            "VALUES( ?, ?, ?, ?, ?, ?, ?, ? )",
            (witness_id, manifest_id, _json_text(dict(charter)), _json_text(dict(journal)),
             _json_text(dict(statement)), _json_text(validation), str(normalized["verdict"]),
             str(normalized["evidence_ceiling"])),
        )
        for report_id in reports:
            connection.execute(
                "INSERT OR IGNORE INTO playtest_witness_report_history( witness_id, report_id ) VALUES( ?, ? )",
                (witness_id, report_id),
            )
    if connection.in_transaction:
        write_witness()
    else:
        with immediate_transaction(connection):
            write_witness()
    return {"witness_id": witness_id, "report_ids": list(reports), **validation}


def review_playtest_witness(
    connection: sqlite3.Connection, *, witness_id: str, decision: str,
    rationale: str, concrete_risk: str = "", reviewer_role: str = "coordinator",
) -> Dict[str, Any]:
    """Persist the coordinator's causal judgment separately from mechanics."""
    role = str(reviewer_role).strip()
    if role not in {"coordinator", "mutation-reviewer"}:
        raise ScenarioRegistryStoreError("playtest witness reviewer role is invalid")
    row = connection.execute(
        "SELECT manifest_id, validation_json FROM playtest_witness_history WHERE witness_id = ?",
        (str(witness_id).strip(),),
    ).fetchone()
    if row is None:
        raise ScenarioRegistryStoreError("playtest witness is unknown")
    validation = _json_object(str(row["validation_json"]), "playtest witness validation")
    try:
        review = review_witness(
            validation, decision=str(decision), rationale=str(rationale),
            concrete_risk=str(concrete_risk),
        )
    except WitnessError as exc:
        raise ScenarioRegistryStoreError(str(exc)) from exc
    review_id = _identity(
        "caol-playtest-witness-review-v1", str(witness_id), role, str(decision),
        str(rationale), str(concrete_risk),
    )
    eligibility: Dict[str, str] = {}
    with immediate_transaction(connection):
        connection.execute(
            "INSERT OR IGNORE INTO playtest_witness_review_history( review_id, witness_id, reviewer_role, decision, "
            "rationale, concrete_risk, review_json ) VALUES( ?, ?, ?, ?, ?, ?, ? )",
            (review_id, str(witness_id), role, str(decision), str(rationale),
             str(concrete_risk), _json_text(review)),
        )
        for route in connection.execute(
                "SELECT DISTINCT verification.route_key FROM playtest_witness_report_history AS linked "
                "JOIN verification_history AS verification ON verification.report_id = linked.report_id "
                "WHERE linked.witness_id = ? AND verification.manifest_id = ?",
                (str(witness_id), str(row["manifest_id"])),):
            route_key = str(route["route_key"])
            eligibility[route_key] = _resolve_route_evidence(
                connection, manifest_id=str(row["manifest_id"]), route_key=route_key,
            )
    return {"review_id": review_id, "witness_id": str(witness_id),
            "reviewer_role": role, "eligibility": eligibility, **review}


def _record_r019_acceptance_matrix(
    connection: sqlite3.Connection, *, manifest_id: str,
) -> Optional[Dict[str, Any]]:
    """Bind the two independently ingested R-019 reports, never their copies."""
    manifest = connection.execute(
        "SELECT declaration_json FROM manifest_current WHERE manifest_id = ?", (manifest_id,)
    ).fetchone()
    if manifest is None or _json_object(
            str(manifest["declaration_json"]), "R-019 acceptance declaration"
    ).get("name") != "r019.keep_watch_acceptance_mcw":
        return None
    reports: List[Dict[str, Any]] = []
    for row in connection.execute(
            "SELECT report_id, report_path FROM report_ingestion_history "
            "WHERE manifest_id = ? AND ingestion_status = 'ingested' ORDER BY recorded_at, report_id",
            (manifest_id,)):
        try:
            value = json.loads(Path(str(row["report_path"])).read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            continue
        packet = value.get("r019_acceptance_matrix") if isinstance(value, Mapping) else None
        if not isinstance(packet, Mapping) and isinstance(value, Mapping):
            for step in value.get("steps", []):
                live = step.get("cockpit_live_session") if isinstance(step, Mapping) else None
                final = live.get("final") if isinstance(live, Mapping) else None
                detail = final.get("stop_detail") if isinstance(final, Mapping) else None
                candidate = detail.get("r019_acceptance_matrix") if isinstance(detail, Mapping) else None
                if isinstance(candidate, Mapping):
                    packet = candidate
                    break
        if isinstance(packet, Mapping):
            authority = value.get("wec_authority") if isinstance(value, Mapping) else None
            reports.append({"report_id": str(row["report_id"]),
                            "evidence_class": value.get("evidence_class"),
                            "r019_acceptance_matrix": {
                                **packet,
                                "registry_authority_id": str(authority.get("authority_id", ""))
                                if isinstance(authority, Mapping) else "",
                                "registry_executable_binding": str(authority.get("binding_id", ""))
                                if isinstance(authority, Mapping) else "",
                            }})
    relation = validate_r019_acceptance_matrix(reports)
    inputs = relation.get("inputs", {})
    if not isinstance(inputs, Mapping) or not isinstance(inputs.get("guarded"), Mapping) or \
            not isinstance(inputs.get("primitive"), Mapping):
        return relation
    guarded_id = str(inputs["guarded"].get("report_id", ""))
    primitive_id = str(inputs["primitive"].get("report_id", ""))
    if not guarded_id or not primitive_id:
        return relation
    connection.execute(
        "INSERT OR IGNORE INTO r019_acceptance_matrix_history( "
        "manifest_id, guarded_report_id, primitive_report_id, status, details_json ) "
        "VALUES( ?, ?, ?, ?, ? )",
        (manifest_id, guarded_id, primitive_id, str(relation["status"]), _json_text(relation)),
    )
    details_json = _json_text(relation)
    connection.execute(
        "INSERT OR IGNORE INTO r019_acceptance_matrix_evaluation_history( "
        "manifest_id, guarded_report_id, primitive_report_id, status, details_json, details_sha256 "
        ") VALUES( ?, ?, ?, ?, ?, ? )",
        (manifest_id, guarded_id, primitive_id, str(relation["status"]), details_json,
         hashlib.sha256(details_json.encode("utf-8")).hexdigest()),
    )
    return relation


def _r019_aggregation_pair(
    connection: sqlite3.Connection, *, guarded_report_id: str, primitive_report_id: str,
) -> tuple[str, Dict[str, Any]]:
    """Load exactly the requested current pair; never discover inputs from history."""
    guarded_report_id = str(guarded_report_id).strip()
    primitive_report_id = str(primitive_report_id).strip()
    if not guarded_report_id or not primitive_report_id:
        raise ScenarioRegistryStoreError("r019_aggregation_report_id_missing")
    if guarded_report_id == primitive_report_id:
        raise ScenarioRegistryStoreError("r019_aggregation_duplicate_report_id")
    rows = connection.execute(
        "SELECT report_id, manifest_id, report_path, report_sha256, ingestion_status FROM report_ingestion_history "
        "WHERE report_id IN (?, ?)", (guarded_report_id, primitive_report_id),
    ).fetchall()
    by_id = {str(row["report_id"]): row for row in rows}
    if set(by_id) != {guarded_report_id, primitive_report_id}:
        raise ScenarioRegistryStoreError("r019_aggregation_report_absent")
    manifest_ids = {str(row["manifest_id"]) for row in rows}
    if len(manifest_ids) != 1 or any(str(row["ingestion_status"]) != "ingested" for row in rows):
        raise ScenarioRegistryStoreError("r019_aggregation_report_not_current")
    manifest_id = manifest_ids.pop()
    manifest = connection.execute(
        "SELECT declaration_json FROM manifest_current WHERE manifest_id = ?", (manifest_id,)
    ).fetchone()
    if manifest is None or _json_object(
            str(manifest["declaration_json"]), "R-019 aggregation declaration"
    ).get("name") != "r019.keep_watch_acceptance_mcw":
        raise ScenarioRegistryStoreError("r019_aggregation_not_r019")
    reports: Dict[str, Any] = {}
    for report_id, row in by_id.items():
        canonical_path, report_bytes = _report_path_and_bytes(Path(str(row["report_path"])))
        if hashlib.sha256(report_bytes).hexdigest() != str(row["report_sha256"]):
            raise ScenarioRegistryStoreError("r019_aggregation_report_stale")
        value = json.loads(report_bytes.decode("utf-8"))
        if not isinstance(value, Mapping):
            raise ScenarioRegistryStoreError("r019_aggregation_report_malformed")
        packet = value.get("r019_acceptance_matrix")
        if not isinstance(packet, Mapping):
            for step in value.get("steps", []):
                live = step.get("cockpit_live_session") if isinstance(step, Mapping) else None
                final = live.get("final") if isinstance(live, Mapping) else None
                detail = final.get("stop_detail") if isinstance(final, Mapping) else None
                candidate = detail.get("r019_acceptance_matrix") if isinstance(detail, Mapping) else None
                if isinstance(candidate, Mapping):
                    packet = candidate
                    break
        authority = value.get("wec_authority")
        if not isinstance(packet, Mapping) or not isinstance(authority, Mapping):
            raise ScenarioRegistryStoreError("r019_aggregation_receipt_missing")
        reports[report_id] = {
            "report_id": report_id,
            "evidence_class": value.get("evidence_class"),
            "r019_acceptance_matrix": {
                **packet,
                "registry_authority_id": str(authority.get("authority_id", "")),
                "registry_executable_binding": str(authority.get("binding_id", "")),
            },
        }
        verification = connection.execute(
            "SELECT 1 FROM verification_history WHERE report_id = ? LIMIT 1", (report_id,)
        ).fetchone()
        if verification is None:
            raise ScenarioRegistryStoreError("r019_aggregation_verification_absent")
        if connection.execute(
                "SELECT 1 FROM verification_history AS prior JOIN verification_history AS successor "
                "ON successor.supersedes_verification_id = prior.verification_id "
                "WHERE prior.report_id = ? LIMIT 1", (report_id,),
        ).fetchone() is not None:
            raise ScenarioRegistryStoreError("r019_aggregation_report_superseded")
    if str(reports[guarded_report_id]["r019_acceptance_matrix"].get("role", "")) != "guarded" or \
            str(reports[primitive_report_id]["r019_acceptance_matrix"].get("role", "")) != "primitive":
        raise ScenarioRegistryStoreError("r019_aggregation_role_id_mismatch")
    relation = validate_r019_acceptance_matrix((reports[guarded_report_id], reports[primitive_report_id]))
    if relation.get("status") != "green":
        raise ScenarioRegistryStoreError("r019_aggregation_relation_red:" + ",".join(relation.get("errors", [])))
    return manifest_id, relation


def issue_r019_aggregation_token(
    connection: sqlite3.Connection, *, guarded_report_id: str, primitive_report_id: str,
) -> RegistryR019AggregationToken:
    """Authorize precisely one current pair before writing its zero-credit terminal."""
    try:
        manifest_id, relation = _r019_aggregation_pair(
            connection, guarded_report_id=guarded_report_id, primitive_report_id=primitive_report_id,
        )
    except (OSError, UnicodeDecodeError, json.JSONDecodeError, ScenarioRegistryStoreError) as exc:
        return RegistryR019AggregationToken("", False, str(exc), str(guarded_report_id), str(primitive_report_id))
    if connection.execute(
            "SELECT 1 FROM r019_aggregation_terminal_history WHERE manifest_id = ? AND guarded_report_id = ? "
            "AND primitive_report_id = ? LIMIT 1", (manifest_id, guarded_report_id, primitive_report_id),
    ).fetchone() is not None:
        return RegistryR019AggregationToken("", False, "r019_aggregation_pair_already_terminalized",
                                            guarded_report_id, primitive_report_id)
    details = {
        "authority_kind": "registry_r019_zero_credit_aggregation",
        "manifest_id": manifest_id,
        "guarded_report_id": guarded_report_id,
        "primitive_report_id": primitive_report_id,
        "relation_sha256": hashlib.sha256(_json_text(relation).encode("utf-8")).hexdigest(),
    }
    token_id = _identity("caol-r019-aggregation-token-v1", _json_text(details))
    with immediate_transaction(connection):
        existing = connection.execute(
            "SELECT 1 FROM token_history WHERE token_id = ? AND event_kind = 'r019_aggregation_claimed' LIMIT 1",
            (token_id,),
        ).fetchone()
        if existing is not None:
            return RegistryR019AggregationToken(token_id, False, "token_already_claimed",
                                                guarded_report_id, primitive_report_id)
        connection.execute(
            "INSERT OR IGNORE INTO token_history( token_id, manifest_id, verification_id, route_key, event_kind, reason, details_json ) "
            "VALUES( ?, ?, NULL, 'r019_zero_credit_aggregation', 'r019_aggregation_issued', 'exact_current_pair', ? )",
            (token_id, manifest_id, _json_text(details)),
        )
    return RegistryR019AggregationToken(token_id, True, "issued", guarded_report_id, primitive_report_id)


def finalize_r019_aggregation_token(
    connection: sqlite3.Connection, token_id: str,
) -> Dict[str, Any]:
    """Consume an aggregation authority and append its immutable zero-credit packet."""
    token_id = str(token_id).strip()
    issued = connection.execute(
        "SELECT manifest_id, details_json FROM token_history WHERE token_id = ? AND event_kind = 'r019_aggregation_issued' "
        "ORDER BY token_event_id LIMIT 1", (token_id,),
    ).fetchone()
    if issued is None:
        return {"status": "rejected_token", "reason": "token_unknown", "token_id": token_id}
    claimed = connection.execute(
        "SELECT 1 FROM token_history WHERE token_id = ? AND event_kind = 'r019_aggregation_claimed' LIMIT 1",
        (token_id,),
    ).fetchone()
    if claimed is not None:
        return {"status": "rejected_token", "reason": "token_already_claimed", "token_id": token_id}
    details = _json_object(str(issued["details_json"]), "R-019 aggregation token")
    guarded_report_id = _string(details.get("guarded_report_id"), "R-019 guarded report ID")
    primitive_report_id = _string(details.get("primitive_report_id"), "R-019 primitive report ID")
    try:
        manifest_id, relation = _r019_aggregation_pair(
            connection, guarded_report_id=guarded_report_id, primitive_report_id=primitive_report_id,
        )
    except (OSError, UnicodeDecodeError, json.JSONDecodeError, ScenarioRegistryStoreError) as exc:
        with immediate_transaction(connection):
            connection.execute(
                "INSERT OR IGNORE INTO token_history( token_id, manifest_id, verification_id, route_key, event_kind, reason, details_json ) "
                "VALUES( ?, ?, NULL, 'r019_zero_credit_aggregation', 'r019_aggregation_invalidated', ?, '{}' )",
                (token_id, str(issued["manifest_id"]), str(exc)),
            )
        return {"status": "rejected_terminal", "reason": str(exc), "token_id": token_id}
    if manifest_id != str(issued["manifest_id"]):
        return {"status": "rejected_terminal", "reason": "r019_aggregation_manifest_changed", "token_id": token_id}
    packet = {
        "schema": "caol-r019-zero-credit-aggregation-v1",
        "authority_token": token_id,
        "credit": "zero",
        "guarded_report_id": guarded_report_id,
        "primitive_report_id": primitive_report_id,
        "relation": relation,
    }
    packet_json = _json_text(packet)
    with immediate_transaction(connection):
        connection.execute(
            "INSERT INTO token_history( token_id, manifest_id, verification_id, route_key, event_kind, reason, details_json ) "
            "VALUES( ?, ?, NULL, 'r019_zero_credit_aggregation', 'r019_aggregation_claimed', 'canonical_terminal', '{}' )",
            (token_id, manifest_id),
        )
        connection.execute(
            "INSERT INTO r019_aggregation_terminal_history( token_id, manifest_id, guarded_report_id, primitive_report_id, packet_json, packet_sha256 ) "
            "VALUES( ?, ?, ?, ?, ?, ? )",
            (token_id, manifest_id, guarded_report_id, primitive_report_id, packet_json,
             hashlib.sha256(packet_json.encode("utf-8")).hexdigest()),
        )
    return {"status": "terminalized", "token_id": token_id, "credit": "zero",
            "guarded_report_id": guarded_report_id, "primitive_report_id": primitive_report_id}


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
        "SELECT report_id, report_kind, ingestion_status, error_text FROM report_ingestion_history "
        "WHERE report_path = ? AND report_sha256 = ?",
        (canonical_path, report_sha256),
    ).fetchone()
    if existing is not None:
        non_authoritative = str(existing["ingestion_status"]) == "ingested_non_authoritative"
        return {
            "report_id": str(existing["report_id"]),
            "status": "ingested" if non_authoritative else str(existing["ingestion_status"]),
            "classification": str(existing["report_kind"]) if non_authoritative else None,
            "non_authoritative": non_authoritative,
            "error": str(existing["error_text"]),
            "final_gates": {"automated_certification": False, "windows_feel": False} if non_authoritative else None,
            "idempotent": True,
        }
    try:
        report = json.loads(report_bytes.decode("utf-8"))
        if not isinstance(report, dict):
            raise ScenarioRegistryStoreError("Report top level must be an object")
        facts = _extract_report_facts(report)
        r019_packet = report.get("r019_acceptance_matrix")
        r019_final: Optional[Mapping[str, Any]] = None
        if not isinstance(r019_packet, Mapping):
            for step in report.get("steps", []):
                live = step.get("cockpit_live_session") if isinstance(step, Mapping) else None
                final = live.get("final") if isinstance(live, Mapping) else None
                detail = final.get("stop_detail") if isinstance(final, Mapping) else None
                candidate = detail.get("r019_acceptance_matrix") if isinstance(detail, Mapping) else None
                if isinstance(candidate, Mapping):
                    r019_packet = candidate
                    r019_final = final
                    break
        if isinstance(r019_packet, Mapping) and str(r019_packet.get("role", "")).strip() in {
                "guarded", "primitive"}:
            receipt_errors = validate_r019_report_packet(r019_packet)
            role_receipt = r019_packet.get("role_receipt")
            if r019_final is None or not isinstance(role_receipt, Mapping) or \
                    str(role_receipt.get("run_id", "")) != str(r019_final.get("run_id", "")) or \
                    str(role_receipt.get("binding_id", "")) != str(r019_final.get("binding_id", "")):
                receipt_errors.append("r019_receipt_not_bound_to_immutable_live_final")
            if receipt_errors:
                raise ScenarioRegistryStoreError("R-019 immutable receipt rejected: " + ", ".join(receipt_errors))
        report_witness = _witness_from_report(report)
        if isinstance(report_witness, Mapping):
            validation = report_witness.get("validation")
            journal = report_witness.get("journal")
            if not isinstance(validation, Mapping) or not isinstance(journal, Mapping) or \
                    validation.get("status") != "mechanically_valid" or \
                    validation.get("journal_sha256") != journal.get("journal_sha256"):
                raise ScenarioRegistryStoreError("playtest witness is not bound to its immutable journal")
        _validate_required_r008_setup_receipt(connection, facts=facts, report=report)
        # A report may carry an explicit save/relaunch receipt packet.  When it
        # does, validate it before any report facts can become registry state;
        # ordinary reports without this optional packet retain their existing
        # evidence class and receive no inferred relaunch credit.
        relaunch_packet = report.get("relaunch_receipts")
        if relaunch_packet is not None:
            if not isinstance(relaunch_packet, Mapping):
                raise ScenarioRegistryStoreError("relaunch_receipts must be an object")
            try:
                facts["relaunch_normalization"] = normalize_relaunch_receipt(
                    before_save=relaunch_packet.get("before_save"),
                    after_load=relaunch_packet.get("after_load"),
                    transition=relaunch_packet.get("transition"),
                    expected_world_id=str(relaunch_packet.get("expected_world_id", "") or ""),
                    expected_run_id=str(relaunch_packet.get("expected_run_id", "") or ""),
                )
            except RelaunchReceiptError as exc:
                raise ScenarioRegistryStoreError(str(exc)) from exc
        if _is_diagnostic_replay_report(report, facts):
            classification = "diagnostic replay"
            with immediate_transaction(connection):
                connection.execute(
                    "INSERT INTO report_ingestion_history( report_id, report_path, report_sha256, report_kind, ingestion_status ) "
                    "VALUES( ?, ?, ?, ?, 'ingested_non_authoritative' )",
                    (report_id, canonical_path, report_sha256, classification),
                )
            return {
                "report_id": report_id,
                "status": "ingested",
                "classification": classification,
                "non_authoritative": True,
                "verification_id": None,
                "final_gates": {"automated_certification": False, "windows_feel": False},
                "diagnostic_capsule": None,
                "intervention_ids": [],
                "idempotent": False,
            }
        raw_authority = report.get("wec_authority")
        proof = facts.get("proof")
        potential_sealed_feature_proof = (
            isinstance(proof, Mapping)
            and proof.get("status") == "green"
            and proof.get("feature_proof") is True
            and proof.get("route_feature_proof") is True
            and isinstance(raw_authority, Mapping)
            and str(raw_authority.get("authority_id", "")).strip()
        )
        if not potential_sealed_feature_proof and _is_setup_only_report(report, facts):
            classification = "setup-only"
            with immediate_transaction(connection):
                intervention_ids = _append_setup_only_interventions(
                    connection, facts=facts, report=report
                )
                connection.execute(
                    "INSERT INTO report_ingestion_history( report_id, report_path, report_sha256, report_kind, ingestion_status ) "
                    "VALUES( ?, ?, ?, ?, 'ingested_non_authoritative' )",
                    (report_id, canonical_path, report_sha256, classification),
                )
            return {
                "report_id": report_id,
                "status": "ingested",
                "classification": classification,
                "non_authoritative": True,
                "verification_id": None,
                "final_gates": {"automated_certification": False, "windows_feel": False},
                "diagnostic_capsule": None,
                "intervention_ids": list(intervention_ids),
                "idempotent": False,
            }
        if isinstance(raw_authority, Mapping) and str(raw_authority.get("authority_id", "")).strip():
            authority_row = connection.execute(
                "SELECT authority_id, evidence_class, authority, run_id, binding_id, source_sha256, owner "
                "FROM wec_authority_history WHERE authority_id = ?",
                (str(raw_authority["authority_id"]),),
            ).fetchone()
            if authority_row is None:
                raise ScenarioRegistryStoreError("WEC authority is not registry-owned")
            stored_source = str(authority_row["source_sha256"])
            if stored_source != str(facts["manifest"]["source_sha256"]):
                raise ScenarioRegistryStoreError("WEC authority source does not match report manifest")
            if str(report.get("run_id", "")) != str(authority_row["run_id"]):
                raise ScenarioRegistryStoreError("WEC authority run does not match report run")
            if str(report.get("binding_id", "")) != str(authority_row["binding_id"]):
                raise ScenarioRegistryStoreError("WEC authority binding does not match report binding")
            facts["wec_authority"] = {
                "status": "sealed",
                "fact": {key: str(authority_row[key]) for key in (
                    "authority_id", "evidence_class", "authority", "run_id", "binding_id", "source_sha256", "owner",
                )},
            }
        if _is_setup_only_report(report, facts):
            classification = "setup-only"
            with immediate_transaction(connection):
                intervention_ids = (
                    _append_setup_only_interventions(connection, facts=facts, report=report)
                    if classification == "setup-only" else ()
                )
                connection.execute(
                    "INSERT INTO report_ingestion_history( report_id, report_path, report_sha256, report_kind, ingestion_status ) "
                    "VALUES( ?, ?, ?, ?, 'ingested_non_authoritative' )",
                    (report_id, canonical_path, report_sha256, classification),
                )
            return {
                "report_id": report_id,
                "status": "ingested",
                "classification": classification,
                "non_authoritative": True,
                "verification_id": None,
                "final_gates": {"automated_certification": False, "windows_feel": False},
                "diagnostic_capsule": None,
                "intervention_ids": list(intervention_ids),
                "idempotent": False,
            }
        authority_fact = facts["wec_authority"].get("fact") if isinstance(facts["wec_authority"], Mapping) else None
        round_check = {"eligible": False, "reason": "not_certification_authority"}
        if isinstance(authority_fact, Mapping) and authority_fact.get("evidence_class") == "automated continuous-round certification":
            round_check = _certification_round_check(
                connection, authority=authority_fact,
                round_facts=facts["certification_round"], report_facts=facts,
            )
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
            authority_fact = facts["wec_authority"].get("fact") if isinstance(facts["wec_authority"], Mapping) else None
            if isinstance(authority_fact, Mapping) and authority_fact.get("authority_id"):
                runtime_binding_row = None
                for candidate in connection.execute(
                    "SELECT payload_json FROM binding_history WHERE manifest_id = ? AND binding_kind = 'runtime' "
                    "ORDER BY binding_event_id DESC",
                    (manifest_id,),
                ):
                    candidate_payload = _json_object(str(candidate["payload_json"]), "runtime binding payload")
                    if candidate_payload.get("verification_id") == verification_id:
                        runtime_binding_row = candidate_payload
                        break
                if runtime_binding_row is None:
                    raise ScenarioRegistryStoreError("WEC authority runtime binding is missing")
                runtime_facts = runtime_binding_row.get("facts", {})
                if (authority_fact.get("evidence_class") != "automated continuous-round certification" and
                        str(authority_fact.get("binding_id", "")) != str(runtime_facts.get("executable_sha256", ""))):
                    raise ScenarioRegistryStoreError("WEC authority binding does not match current runtime executable")
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
                    "runtime": facts["runtime"],
                    "requested_supersedes_verification_id": facts["supersedes_verification_id"],
                    "wec_authority": facts["wec_authority"],
                    "certification_round": facts["certification_round"],
                    "certification_lifecycle": facts.get("certification_lifecycle"),
                    "certification_round_check": round_check,
                }),
            ),
        )
        capsule_result = _append_capsule_candidate_for_report(
            connection,
            report=report,
            facts=facts,
            report_id=report_id,
            verification_id=verification_id,
            report_path=canonical_path,
            report_sha256=report_sha256,
        )
        declaration_row = connection.execute(
            "SELECT declaration_json FROM manifest_current WHERE manifest_id = ?",
            (manifest_id,),
        ).fetchone()
        if declaration_row is None:
            raise ScenarioRegistryStoreError("Report manifest disappeared during ingestion")
        _append_named_capability_gate_evidence(
            connection,
            manifest_id=manifest_id,
            verification_id=verification_id,
            declaration=_json_object(str(declaration_row["declaration_json"]), "manifest declaration"),
            report=report,
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
        # Scenario-specific matrix history remains readable for old reports,
        # but new eligibility is owned by cited playtest witnesses.
        r019_acceptance_matrix = None
        playtest_witness = None
        if isinstance(report_witness, Mapping):
            validation = report_witness["validation"]
            playtest_witness = record_playtest_witness(
                connection, manifest_id=manifest_id, report_ids=[report_id],
                charter=validation["charter"], journal=report_witness["journal"],
                statement=validation["witness"],
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
        "r019_acceptance_matrix": r019_acceptance_matrix,
        "playtest_witness": playtest_witness,
        "final_gates": derive_final_gate_eligibility(
            facts["wec_authority"].get("fact"),
            proof_status=str(proof.get("status", "")),
            resolution=resolution,
            registry_owned=bool(
                isinstance(facts["wec_authority"].get("fact"), Mapping)
                and facts["wec_authority"]["fact"].get("authority_id")
            ),
            certification_round_valid=bool(round_check.get("eligible")),
        ),
        "diagnostic_capsule": capsule_result,
        "idempotent": False,
    }


def issue_wec_authority(
    connection: sqlite3.Connection, *, evidence_class: str, authority: str,
    run_id: str, binding_id: str, source_sha256: str, owner: str = "",
) -> Dict[str, str]:
    """Persist non-final WEC authority before execution.

    Automated certification authority is intentionally absent from this public
    API.  It is issued only by ``create_certification_round`` after that owner
    has reloaded a current registry selection token and canonical launch facts.
    Windows feel remains an external, non-machine-verifiable attestation.
    """
    from wec_evidence import WEC_CLASS_SET
    if evidence_class not in WEC_CLASS_SET or not run_id.strip() or not binding_id.strip():
        raise ScenarioRegistryStoreError("invalid WEC authority inputs")
    if evidence_class == "diagnostic replay":
        raise ScenarioRegistryStoreError("diagnostic replay cannot create registry authority")
    if evidence_class in {"automated continuous-round certification", "Windows feel evidence"}:
        raise ScenarioRegistryStoreError("final-gate authority cannot be caller-issued")
    if len(source_sha256) != 64 or any(char not in "0123456789abcdef" for char in source_sha256.lower()):
        raise ScenarioRegistryStoreError("WEC source SHA-256 must be a hexadecimal digest")
    existing = connection.execute(
        "SELECT authority_id, evidence_class, authority, run_id, binding_id, source_sha256, owner "
        "FROM wec_authority_history WHERE run_id = ?", (run_id,)
    ).fetchone()
    if existing is not None:
        return {key: str(existing[key]) for key in (
            "authority_id", "evidence_class", "authority", "run_id", "binding_id", "source_sha256", "owner",
        )}
    authority_id = hashlib.sha256(("caol-wec-authority-v1:" + uuid.uuid4().hex).encode()).hexdigest()
    fact = {
        "authority_id": authority_id, "evidence_class": evidence_class, "authority": authority,
        "run_id": run_id, "binding_id": binding_id, "source_sha256": source_sha256.lower(), "owner": owner,
    }
    with immediate_transaction(connection):
        connection.execute(
            "INSERT INTO wec_authority_history( authority_id, evidence_class, authority, run_id, binding_id, source_sha256, owner ) "
            "VALUES( ?, ?, ?, ?, ?, ?, ? )",
            tuple(fact.values()),
        )
    return fact


def _verification_final_gates(connection: sqlite3.Connection, row: sqlite3.Row) -> Dict[str, Any]:
    """Derive final-gate facts for one immutable automated verification."""
    details = _json_object(str(row["details_json"]), "verification details")
    authority = details.get("wec_authority", {})
    authority_fact = authority.get("fact") if isinstance(authority, Mapping) else None
    current_round = {"eligible": False, "reason": "not_certification_authority"}
    if isinstance(authority_fact, Mapping) and authority_fact.get("evidence_class") == \
            "automated continuous-round certification":
        current_round = _certification_round_check(
            connection, authority=authority_fact,
            round_facts=details.get("certification_round", {}), report_facts=details,
        )
    gates = derive_final_gate_eligibility(
        authority_fact,
        proof_status=str(row["proof_status"]),
        resolution=str(row["resolution_kind"]),
        registry_owned=bool(
            isinstance(authority, Mapping)
            and isinstance(authority.get("fact"), Mapping)
            and authority["fact"].get("authority_id")
        ),
        certification_round_valid=bool(current_round.get("eligible")),
    )
    return {"details": details, "gates": gates, "round": current_round}


def _eligible_certification_verification(
    connection: sqlite3.Connection, certification_verification_id: str,
) -> Tuple[sqlite3.Row, Mapping[str, Any]]:
    """Return one currently-green automated certification, or fail closed."""
    row = connection.execute(
        "SELECT verification.verification_id, verification.proof_status, verification.details_json, "
        "COALESCE((SELECT resolution_kind FROM verification_resolution_history AS resolution "
        "WHERE resolution.verification_id = verification.verification_id "
        "ORDER BY resolution.resolution_event_id DESC LIMIT 1), 'unknown') AS resolution_kind "
        "FROM verification_history AS verification WHERE verification.verification_id = ?",
        (str(certification_verification_id).strip(),),
    ).fetchone()
    if row is None:
        raise ScenarioRegistryStoreError("certification verification is unavailable")
    evaluated = _verification_final_gates(connection, row)
    if not evaluated["gates"]["automated_certification"]:
        raise ScenarioRegistryStoreError("certification verification is not currently eligible")
    return row, evaluated


def _windows_build_reference(
    value: Mapping[str, Any], *, certification_executable_sha256: str,
) -> Dict[str, str]:
    """Keep the handoff concrete while excluding debug and scripted proof controls."""
    required = {"platform", "executable_path", "executable_sha256", "world"}
    if set(value) != required:
        raise ScenarioRegistryStoreError("Windows handoff build reference must contain exactly platform, executable_path, executable_sha256, and world")
    platform = str(value["platform"]).strip().lower()
    executable_path = str(value["executable_path"]).strip()
    executable_sha256 = str(value["executable_sha256"]).strip().lower()
    world = str(value["world"]).strip()
    if platform != "windows" or not executable_path or not world or len(executable_sha256) != 64 or \
            any(character not in "0123456789abcdef" for character in executable_sha256):
        raise ScenarioRegistryStoreError("Windows handoff build reference is malformed")
    if executable_sha256 != certification_executable_sha256:
        raise ScenarioRegistryStoreError("Windows handoff executable does not match the certified binding")
    return {
        "platform": "windows",
        "executable_path": executable_path,
        "executable_sha256": executable_sha256,
        "world": world,
    }


def prepare_windows_feel_handoff(
    connection: sqlite3.Connection, *, certification_verification_id: str,
    windows_build: Mapping[str, Any],
) -> Dict[str, Any]:
    """Prepare one ordinary Windows play handoff from a current certification pass."""
    verification, evaluated = _eligible_certification_verification(
        connection, certification_verification_id,
    )
    details = evaluated["details"]
    round_facts = details.get("certification_round", {})
    if not isinstance(round_facts, Mapping):
        raise ScenarioRegistryStoreError("certification verification has no sealed round facts")
    binding_id = str(round_facts.get("binding_id", "")).strip()
    round_id = str(round_facts.get("round_id", "")).strip()
    if not binding_id or not round_id:
        raise ScenarioRegistryStoreError("certification verification round identity is incomplete")
    certification_round = connection.execute(
        "SELECT manifest_json FROM certification_round WHERE round_id = ?", (round_id,)
    ).fetchone()
    if certification_round is None:
        raise ScenarioRegistryStoreError("certification verification round is unavailable")
    manifest = _json_object(str(certification_round["manifest_json"]), "certification round manifest")
    binding = manifest.get("binding")
    authoritative = binding.get("authoritative_components") if isinstance(binding, Mapping) else None
    executable = authoritative.get("executable") if isinstance(authoritative, Mapping) else None
    certified_executable_sha256 = str(
        executable.get("content_sha256", "") if isinstance(executable, Mapping) else ""
    ).strip().lower()
    if len(certified_executable_sha256) != 64 or any(
            character not in "0123456789abcdef" for character in certified_executable_sha256):
        raise ScenarioRegistryStoreError("certification verification executable binding is incomplete")
    build = _windows_build_reference(
        windows_build, certification_executable_sha256=certified_executable_sha256,
    )
    ordinary_play = {
        "kind": "ordinary-windows-play",
        "launch": "Launch the supplied Windows build and continue the supplied world.",
        "world": build["world"],
        "judgment": "Play normally, then record your own pass or fail judgment.",
    }
    with immediate_transaction(connection):
        existing = connection.execute(
            "SELECT handoff_id, certification_binding_id, certification_round_id, windows_build_json, ordinary_play_json "
            "FROM windows_feel_handoff WHERE certification_verification_id = ?",
            (str(verification["verification_id"]),),
        ).fetchone()
        if existing is not None:
            if (str(existing["certification_binding_id"]) != binding_id or
                    str(existing["certification_round_id"]) != round_id or
                    _json_object(str(existing["windows_build_json"]), "Windows handoff build") != build):
                raise ScenarioRegistryStoreError("certified Windows handoff is immutable and cannot be repaired")
            return windows_feel_handoff_status(connection, str(existing["handoff_id"]))
        handoff_id = _identity(
            "caol-windows-feel-handoff-v1", str(verification["verification_id"]),
            binding_id, _json_text(build),
        )
        connection.execute(
            "INSERT INTO windows_feel_handoff( handoff_id, certification_verification_id, certification_binding_id, "
            "certification_round_id, windows_build_json, ordinary_play_json, state ) VALUES( ?, ?, ?, ?, ?, ?, 'pending' )",
            (handoff_id, str(verification["verification_id"]), binding_id, round_id,
             _json_text(build), _json_text(ordinary_play)),
        )
    return windows_feel_handoff_status(connection, handoff_id)


def windows_feel_handoff_status(
    connection: sqlite3.Connection, handoff_id: Optional[str] = None,
) -> Dict[str, Any]:
    """Display the immutable external owner note without claiming caller authentication."""
    where, arguments = ("", ()) if handoff_id is None else (" WHERE handoff.handoff_id = ?", (str(handoff_id),))
    rows = connection.execute(
        "SELECT handoff.*, judgment.outcome, judgment.author, judgment.notes, judgment.recorded_at AS judged_at "
        "FROM windows_feel_handoff AS handoff LEFT JOIN windows_feel_judgment AS judgment "
        "ON judgment.handoff_id = handoff.handoff_id" + where + " ORDER BY handoff.recorded_at, handoff.handoff_id",
        arguments,
    ).fetchall()
    handoffs = []
    for row in rows:
        outcome = str(row["outcome"] or "pending")
        handoffs.append({
            "handoff_id": str(row["handoff_id"]),
            "certification_verification_id": str(row["certification_verification_id"]),
            "certification_binding_id": str(row["certification_binding_id"]),
            "certification_round_id": str(row["certification_round_id"]),
            "state": outcome,
            "windows_build": _json_object(str(row["windows_build_json"]), "Windows handoff build"),
            "ordinary_play": _json_object(str(row["ordinary_play_json"]), "ordinary Windows play handoff"),
            "judgment": None if outcome == "pending" else {
                "author": str(row["author"]), "outcome": outcome, "notes": str(row["notes"]),
                "recorded_at": str(row["judged_at"]),
                "authority": "external-owner-attestation",
                "machine_verified": False,
            },
        })
    if handoff_id is not None and not handoffs:
        raise ScenarioRegistryStoreError("Windows feel handoff is unavailable")
    return {"handoffs": handoffs}


def record_windows_feel_judgment(
    connection: sqlite3.Connection, *, handoff_id: str, outcome: str, author: str, notes: str = "",
) -> Dict[str, Any]:
    """Append one immutable external Josef-labelled note without authenticating its caller."""
    normalized_outcome = str(outcome).strip().lower()
    if normalized_outcome not in {"pass", "fail"} or str(author).strip() != "Josef":
        raise ScenarioRegistryStoreError(
            "Windows feel notes use the Josef owner label; the local registry does not authenticate callers"
        )
    with immediate_transaction(connection):
        handoff = connection.execute(
            "SELECT certification_verification_id FROM windows_feel_handoff WHERE handoff_id = ?", (str(handoff_id),)
        ).fetchone()
        if handoff is None:
            raise ScenarioRegistryStoreError("Windows feel handoff is unavailable")
        _eligible_certification_verification(connection, str(handoff["certification_verification_id"]))
        existing = connection.execute(
            "SELECT outcome, author, notes FROM windows_feel_judgment WHERE handoff_id = ?", (str(handoff_id),)
        ).fetchone()
        if existing is not None:
            if (str(existing["outcome"]), str(existing["author"]), str(existing["notes"])) == \
                    (normalized_outcome, "Josef", str(notes)):
                return windows_feel_handoff_status(connection, str(handoff_id))
            raise ScenarioRegistryStoreError("Windows feel judgment is immutable and cannot be repaired")
        connection.execute(
            "INSERT INTO windows_feel_judgment( handoff_id, outcome, author, notes ) VALUES( ?, ?, 'Josef', ? )",
            (str(handoff_id), normalized_outcome, str(notes)),
        )
    return windows_feel_handoff_status(connection, str(handoff_id))


def final_gate_eligibility(connection: sqlite3.Connection) -> Dict[str, Any]:
    """Derive machine gates without authenticating the external Windows feel decision."""
    result = {
        "automated_certification": False,
        "windows_feel": False,
        "windows_feel_authority": "external-non-machine-verifiable",
        "external_windows_feel_attestations": [],
        "authoritative_verification_ids": [],
        "overall_acceptance": False,
        "overall_acceptance_state": "automated-certification-required",
    }
    rows = connection.execute(
        "SELECT verification.verification_id, verification.proof_status, verification.details_json, "
        "COALESCE((SELECT resolution_kind FROM verification_resolution_history AS resolution "
        "WHERE resolution.verification_id = verification.verification_id "
        "ORDER BY resolution.resolution_event_id DESC LIMIT 1), 'unknown') AS resolution_kind "
        "FROM verification_history AS verification ORDER BY verification.recorded_at, verification.verification_id"
    ).fetchall()
    for row in rows:
        gates = _verification_final_gates(connection, row)["gates"]
        if gates["automated_certification"]:
            result["authoritative_verification_ids"].append(str(row["verification_id"]))
        result["automated_certification"] = result["automated_certification"] or gates["automated_certification"]
    for row in connection.execute(
            "SELECT handoff.handoff_id, handoff.certification_verification_id, judgment.outcome "
            "FROM windows_feel_handoff AS handoff JOIN windows_feel_judgment AS judgment "
            "ON judgment.handoff_id = handoff.handoff_id ORDER BY judgment.recorded_at, handoff.handoff_id"):
        try:
            _eligible_certification_verification(connection, str(row["certification_verification_id"]))
        except ScenarioRegistryStoreError:
            continue
        result["external_windows_feel_attestations"].append({
            "handoff_id": str(row["handoff_id"]),
            "outcome": str(row["outcome"]),
            "machine_verified": False,
        })
    if result["automated_certification"]:
        result["overall_acceptance_state"] = "external-owner-judgment-required"
    return result


def ingest_token_linked_report_reference(
    connection: sqlite3.Connection,
    token_id: str,
    report_path: Path,
    *,
    adapters: BindingAdapters,
    witness_charter: Optional[Mapping[str, Any]] = None,
) -> Dict[str, Any]:
    """Ingest one durable report and retain its selected-token verification link."""
    canonical_path, report_bytes = _report_path_and_bytes(report_path)
    report_id = _identity(
        "caol-scenario-report-v1",
        canonical_path,
        hashlib.sha256(report_bytes).hexdigest(),
    )
    prior = connection.execute(
        "SELECT details_json FROM token_history WHERE token_id = ? "
        "AND event_kind = 'verification_run' AND reason = 'report_ingested'",
        (str(token_id).strip(),),
    ).fetchone()
    if prior is not None:
        prior_details = _json_object(str(prior["details_json"]), "token verification run details")
        if str(prior_details.get("report_id", "")) == report_id:
            return {
                "status": "ingested",
                "token_id": str(token_id).strip(),
                "report_id": report_id,
                "idempotent": True,
            }
        record_selection_token_rejection(
            connection,
            str(token_id).strip(),
            reason="multiple_report_runs",
            details={"prior_report_id": str(prior_details.get("report_id", "")), "report_id": report_id},
        )
        return {
            "status": "rejected_multiple_reports",
            "token_id": str(token_id).strip(),
            "report_id": report_id,
        }

    selection = reload_selection_token_for_launch(
        connection, token_id, witness_charter=witness_charter,
    )
    if not selection.accepted:
        return {
            "status": "rejected_token",
            "reason": selection.reason,
            "token_id": selection.token_id,
        }

    ingested = ingest_report_reference(connection, report_path, adapters=adapters)
    if str(ingested.get("status", "")) != "ingested":
        record_selection_token_rejection(
            connection,
            selection.token_id,
            reason="report_ingest_" + str(ingested.get("status", "unknown")),
            details={
                "report_id": str(ingested.get("report_id", "")),
                "error": str(ingested.get("error", "")),
            },
        )
        return {
            "status": "rejected_report",
            "reason": str(ingested.get("status", "unknown")),
            "token_id": selection.token_id,
            "report_id": str(ingested.get("report_id", "")),
        }

    if ingested.get("classification") == "setup-only":
        with immediate_transaction(connection):
            connection.execute(
                "INSERT OR IGNORE INTO token_history( "
                "token_id, manifest_id, verification_id, route_key, event_kind, reason, details_json "
                ") SELECT token_id, manifest_id, verification_id, route_key, 'verification_run', "
                "'setup_only_report_ingested', ? FROM token_history "
                "WHERE token_id = ? AND event_kind = 'issued' ORDER BY token_event_id LIMIT 1",
                (_json_text({
                    "report_id": str(ingested["report_id"]),
                    "intervention_ids": list(ingested.get("intervention_ids", [])),
                    "final_gates": dict(ingested.get("final_gates", {})),
                }), selection.token_id),
            )
        return {
            "status": "ingested_setup_only",
            "token_id": selection.token_id,
            "report_id": str(ingested["report_id"]),
            "intervention_ids": list(ingested.get("intervention_ids", [])),
            "final_gates": dict(ingested.get("final_gates", {})),
        }

    issued = connection.execute(
        "SELECT manifest_id, verification_id, route_key FROM token_history "
        "WHERE token_id = ? AND event_kind = 'issued' ORDER BY token_event_id LIMIT 1",
        (selection.token_id,),
    ).fetchone()
    verification = connection.execute(
        "SELECT verification_id, manifest_id, route_key FROM verification_history WHERE report_id = ?",
        (str(ingested["report_id"]),),
    ).fetchone()
    if (
        issued is None
        or verification is None
        or str(verification["manifest_id"]) != str(issued["manifest_id"])
        or str(verification["route_key"]) != str(issued["route_key"])
    ):
        record_selection_token_rejection(
            connection,
            selection.token_id,
            reason="report_verification_identity_mismatch",
            details={
                "report_id": str(ingested.get("report_id", "")),
                "verification_id": str(verification["verification_id"]) if verification else "",
            },
        )
        return {
            "status": "rejected_report_identity",
            "token_id": selection.token_id,
            "report_id": str(ingested.get("report_id", "")),
        }

    with immediate_transaction(connection):
        inserted = connection.execute(
            "INSERT OR IGNORE INTO token_history( "
            "token_id, manifest_id, verification_id, route_key, event_kind, reason, details_json "
            ") VALUES( ?, ?, ?, ?, 'verification_run', 'report_ingested', ? )",
            (
                selection.token_id,
                str(issued["manifest_id"]),
                str(verification["verification_id"]),
                str(issued["route_key"]),
                _json_text({
                    "report_id": str(ingested["report_id"]),
                    "report_path": str(report_path.resolve()),
                    "report_ingestion_idempotent": bool(ingested.get("idempotent", False)),
                }),
            ),
        ).rowcount == 1
    return {
        "status": "ingested",
        "token_id": selection.token_id,
        "report_id": str(ingested["report_id"]),
        "verification_id": str(verification["verification_id"]),
        "idempotent": not inserted,
    }


def ingest_bootstrap_token_linked_report_reference(
    connection: sqlite3.Connection,
    token_id: str,
    report_path: Path,
    *,
    adapters: BindingAdapters,
) -> Dict[str, Any]:
    """Ingest the sole report from a claimed bootstrap authority without minting a selection token."""
    selection = reload_bootstrap_token_for_launch(connection, token_id, require_claimed=True)
    if not selection.accepted:
        return {"status": "rejected_token", "reason": selection.reason, "token_id": selection.token_id}
    canonical_path, report_bytes = _report_path_and_bytes(report_path)
    report_id = _identity("caol-scenario-report-v1", canonical_path, hashlib.sha256(report_bytes).hexdigest())
    prior = connection.execute(
        "SELECT 1 FROM token_history WHERE token_id = ? AND event_kind = 'bootstrap_verification_run'",
        (selection.token_id,),
    ).fetchone()
    if prior is not None:
        return {"status": "rejected_multiple_reports", "token_id": selection.token_id, "report_id": report_id}
    ingested = ingest_report_reference(connection, report_path, adapters=adapters)
    if str(ingested.get("status", "")) != "ingested":
        record_bootstrap_token_rejection(
            connection, selection.token_id, reason="report_ingest_" + str(ingested.get("status", "unknown")),
            details={"report_id": str(ingested.get("report_id", "")), "error": str(ingested.get("error", ""))},
        )
        return {"status": "rejected_report", "token_id": selection.token_id}
    with immediate_transaction(connection):
        issued = _bootstrap_token_details(connection, selection.token_id)
        if issued is None:
            raise ScenarioRegistryStoreError("bootstrap token disappeared during report ingestion")
        connection.execute(
            "INSERT INTO token_history( token_id, manifest_id, verification_id, route_key, event_kind, reason, details_json ) "
            "VALUES( ?, ?, NULL, ?, 'bootstrap_verification_run', 'report_ingested', ? )",
            (
                selection.token_id, str(issued["manifest_id"]), str(issued["route_key"]),
                _json_text({"report_id": str(ingested["report_id"]), "report_path": str(report_path.resolve())}),
            ),
        )
    return {"status": "ingested", "token_id": selection.token_id, "report_id": str(ingested["report_id"])}


def ingest_repair_token_linked_report_reference(
    connection: sqlite3.Connection,
    token_id: str,
    report_path: Path,
    *,
    adapters: BindingAdapters,
) -> Dict[str, Any]:
    """Accept terminal authority only from one hard proof superseding the bound red verification."""
    selection = reload_repair_token_for_launch(connection, token_id, require_claimed=True)
    if not selection.accepted:
        return {"status": "rejected_token", "reason": selection.reason, "token_id": selection.token_id}
    canonical_path, report_bytes = _report_path_and_bytes(report_path)
    report_id = _identity("caol-scenario-report-v1", canonical_path, hashlib.sha256(report_bytes).hexdigest())
    prior = connection.execute(
        "SELECT 1 FROM token_history WHERE token_id = ? AND event_kind = 'repair_verification_run'",
        (selection.token_id,),
    ).fetchone()
    if prior is not None:
        record_repair_token_rejection(
            connection, selection.token_id, reason="multiple_report_runs", details={"report_id": report_id},
        )
        return {"status": "rejected_multiple_reports", "token_id": selection.token_id, "report_id": report_id}

    ingested = ingest_report_reference(connection, report_path, adapters=adapters)
    issued = _repair_token_details(connection, selection.token_id)
    if issued is None:
        raise ScenarioRegistryStoreError("repair token disappeared during report ingestion")

    def consume(reason: str, details: Mapping[str, Any]) -> None:
        with immediate_transaction(connection):
            connection.execute(
                "INSERT INTO token_history( token_id, manifest_id, verification_id, route_key, event_kind, reason, details_json ) "
                "VALUES( ?, ?, ?, ?, 'repair_verification_run', ?, ? )",
                (
                    selection.token_id, str(issued["manifest_id"]), issued["verification_id"],
                    str(issued["route_key"]), reason, _json_text(dict(details)),
                ),
            )
            _record_repair_token_rejection(
                connection, issued=issued, reason=reason, details=details,
            )

    if str(ingested.get("status", "")) != "ingested":
        details = {"report_id": str(ingested.get("report_id", "")), "status": str(ingested.get("status", "")),
                   "error": str(ingested.get("error", ""))}
        consume("report_ingest_" + str(ingested.get("status", "unknown")), details)
        return {"status": "rejected_report", "token_id": selection.token_id, **details}
    verification = connection.execute(
        "SELECT verification_id, manifest_id, route_key, supersedes_verification_id, details_json "
        "FROM verification_history WHERE report_id = ?",
        (str(ingested["report_id"]),),
    ).fetchone()
    if verification is None:
        consume("report_verification_missing", {"report_id": str(ingested["report_id"])})
        return {"status": "rejected_report_identity", "token_id": selection.token_id}
    failure = ""
    if str(verification["manifest_id"]) != str(issued["manifest_id"]) or str(verification["route_key"]) != str(issued["route_key"]):
        failure = "report_verification_identity_mismatch"
    elif str(verification["supersedes_verification_id"] or "") != str(issued["verification_id"]):
        failure = "required_supersession_missing_or_wrong"
    elif _verification_evidence_state(verification) != "hard_proven":
        failure = "report_not_hard_proven"
    else:
        resolution = connection.execute(
            "SELECT resolution_kind FROM verification_resolution_history WHERE verification_id = ? "
            "ORDER BY resolution_event_id DESC LIMIT 1",
            (str(verification["verification_id"]),),
        ).fetchone()
        if resolution is None or str(resolution["resolution_kind"]) != "compatible":
            failure = "report_binding_not_compatible"
    if failure:
        consume(failure, {
            "report_id": str(ingested["report_id"]),
            "verification_id": str(verification["verification_id"]),
            "supersedes_verification_id": str(verification["supersedes_verification_id"] or ""),
        })
        return {"status": "rejected_report", "reason": failure, "token_id": selection.token_id,
                "report_id": str(ingested["report_id"])}
    route = next(
        (item for item in _current_route_evidence(connection, str(issued["manifest_id"]))
         if str(item.get("route_key", "")) == str(issued["route_key"])),
        None,
    )
    details = route.get("details", {}) if isinstance(route, Mapping) else {}
    if (
            route is None or route.get("evidence_state") != "run-verified"
            or str(issued["verification_id"]) not in {
                str(item) for item in details.get("superseded_contradiction_ids", ())}):
        consume("route_supersession_not_authoritative", {
            "report_id": str(ingested["report_id"]), "verification_id": str(verification["verification_id"]),
        })
        return {"status": "rejected_report", "reason": "route_supersession_not_authoritative",
                "token_id": selection.token_id, "report_id": str(ingested["report_id"])}
    with immediate_transaction(connection):
        connection.execute(
            "INSERT INTO token_history( token_id, manifest_id, verification_id, route_key, event_kind, reason, details_json ) "
            "VALUES( ?, ?, ?, ?, 'repair_verification_run', 'report_ingested_authoritative', ? )",
            (
                selection.token_id, str(issued["manifest_id"]), str(verification["verification_id"]),
                str(issued["route_key"]), _json_text({"report_id": str(ingested["report_id"]),
                "red_verification_id": str(issued["verification_id"])}),
            ),
        )
    return {"status": "ingested", "token_id": selection.token_id, "report_id": str(ingested["report_id"]),
            "verification_id": str(verification["verification_id"]), "idempotent": False}


def ingest_repair_compatibility_terminal(
    connection: sqlite3.Connection,
    token_id: str,
    terminal_path: Path,
) -> Dict[str, Any]:
    """Supersede one repair contradiction with a zero-credit bound terminal.

    This is deliberately narrower than report ingestion.  It proves only that
    the current repair authority still names the same source/runtime footing
    and reached a controlled-client or registry-authoritative terminal.  It
    cannot become a feature report or an R-019 acceptance-matrix input.
    """
    selection = reload_repair_token_for_launch(connection, token_id, require_claimed=True)
    if not selection.accepted:
        return {"status": "rejected_token", "reason": selection.reason, "token_id": selection.token_id}
    canonical_path, terminal_bytes = _report_path_and_bytes(terminal_path)
    try:
        terminal = _json_object(terminal_bytes.decode("utf-8"), "repair compatibility terminal")
    except (UnicodeDecodeError, json.JSONDecodeError, ScenarioRegistryStoreError) as exc:
        return {"status": "rejected_terminal", "reason": "terminal_unreadable", "error": str(exc),
                "token_id": selection.token_id}
    issued = _repair_token_details(connection, selection.token_id)
    if issued is None:
        raise ScenarioRegistryStoreError("repair token disappeared during terminal validation")
    issued_details = _json_object(str(issued["details_json"]), "repair token details")
    if issued_details.get("authority_kind") == "registry_repair_r019_current_source_successor":
        return {"status": "rejected_terminal", "reason": "r019_successor_required", "token_id": selection.token_id}
    binding = issued.get("binding") if isinstance(issued, Mapping) else None
    # The repair token stores a full current binding, while its launch-facing
    # runtime packet is the canonical value a controlled client can actually
    # receive and echo without reconstructing private fixture/profile facts.
    expected_runtime = selection.runtime_binding
    if terminal.get("schema") != "caol-repair-compatibility-terminal-v1" or \
            terminal.get("repair_token") != selection.token_id:
        return {"status": "rejected_terminal", "reason": "terminal_identity_mismatch", "token_id": selection.token_id}
    if terminal.get("gameplay_credit") is not False or terminal.get("matrix_credit") is not False:
        return {"status": "rejected_terminal", "reason": "terminal_attempted_gameplay_promotion", "token_id": selection.token_id}
    if terminal.get("runtime_binding") != expected_runtime:
        return {"status": "rejected_terminal", "reason": "terminal_runtime_binding_mismatch", "token_id": selection.token_id}
    controlled_client = terminal.get("controlled_client")
    authoritative_terminal = terminal.get("authoritative_terminal")
    client_valid = isinstance(controlled_client, Mapping) and controlled_client.get("status") == "connected" and \
        controlled_client.get("runtime_binding") == expected_runtime
    terminal_valid = isinstance(authoritative_terminal, Mapping) and \
        authoritative_terminal.get("kind") == "registry_current_source_runtime_compatibility" and \
        authoritative_terminal.get("status") == "terminal" and \
        authoritative_terminal.get("runtime_binding") == expected_runtime
    if not client_valid and not terminal_valid:
        return {"status": "rejected_terminal", "reason": "terminal_client_or_authority_missing", "token_id": selection.token_id}
    if terminal.get("terminal_result") != "current_source_runtime_compatible":
        return {"status": "rejected_terminal", "reason": "terminal_not_compatible", "token_id": selection.token_id}
    report_id = _identity("caol-repair-compatibility-terminal-v1", canonical_path,
                          hashlib.sha256(terminal_bytes).hexdigest())
    verification_id = _identity("caol-repair-compatibility-verification-v1", report_id, str(issued["route_key"]))
    aggregate_binding = _identity("caol-repair-compatibility-binding-v1", _json_text(binding))
    with immediate_transaction(connection):
        prior = connection.execute(
            "SELECT 1 FROM token_history WHERE token_id = ? AND event_kind = 'repair_verification_run'",
            (selection.token_id,),
        ).fetchone()
        if prior is not None:
            return {"status": "rejected_multiple_terminals", "token_id": selection.token_id}
        connection.execute(
            "INSERT INTO report_ingestion_history( report_id, manifest_id, report_path, report_sha256, report_kind, ingestion_status ) "
            "VALUES( ?, ?, ?, ?, 'repair-compatibility-terminal', 'ingested_non_feature' )",
            (report_id, str(issued["manifest_id"]), canonical_path, hashlib.sha256(terminal_bytes).hexdigest()),
        )
        details = {
            "proof": {"status": "green", "feature_proof": False, "evidence_class": "repair bootstrap"},
            "repair_bootstrap": {"zero_credit": True, "matrix_credit": False,
                                  "terminal_result": "current_source_runtime_compatible"},
            "runtime": expected_runtime,
        }
        connection.execute(
            "INSERT INTO verification_history( verification_id, manifest_id, report_id, route_key, binding_fingerprint, outcome_kind, proof_status, supersedes_verification_id, details_json ) "
            "VALUES( ?, ?, ?, ?, ?, 'repair_bootstrap_compatible', 'green', ?, ? )",
            (verification_id, str(issued["manifest_id"]), report_id, str(issued["route_key"]), aggregate_binding,
             str(issued["verification_id"]), _json_text(details)),
        )
        _append_resolution_if_changed(
            connection, verification_id=verification_id, manifest_id=str(issued["manifest_id"]),
            route_key=str(issued["route_key"]), resolution_kind="compatible",
            binding_fingerprint=aggregate_binding, details={"repair_bootstrap": True},
        )
        _resolve_route_evidence(connection, manifest_id=str(issued["manifest_id"]), route_key=str(issued["route_key"]))
        connection.execute(
            "INSERT INTO token_history( token_id, manifest_id, verification_id, route_key, event_kind, reason, details_json ) "
            "VALUES( ?, ?, ?, ?, 'repair_verification_run', 'repair_bootstrap_terminal_authoritative', ? )",
            (selection.token_id, str(issued["manifest_id"]), verification_id, str(issued["route_key"]),
             _json_text({"report_id": report_id, "red_verification_id": str(issued["verification_id"])})),
        )
    return {"status": "ingested_zero_credit", "token_id": selection.token_id, "report_id": report_id,
            "verification_id": verification_id, "idempotent": False}


def ingest_r019_current_source_repair_successor(
    connection: sqlite3.Connection,
    token_id: str,
    successor_path: Path,
) -> Dict[str, Any]:
    """Resolve one stale R-019 red row through its repaired current HUD boundary.

    Unlike the compatibility terminal, this receipt is deliberately bound to
    the current manifest bytes and records the repaired visible HUD boundary.
    It is still repair infrastructure: it has no gameplay or matrix credit and
    may never become an R-019 report input.
    """
    selection = reload_repair_token_for_launch(connection, token_id, require_claimed=True)
    if not selection.accepted:
        return {"status": "rejected_token", "reason": selection.reason, "token_id": selection.token_id}
    canonical_path, successor_bytes = _report_path_and_bytes(successor_path)
    try:
        successor = _json_object(successor_bytes.decode("utf-8"), "R-019 repair successor")
    except (UnicodeDecodeError, json.JSONDecodeError, ScenarioRegistryStoreError) as exc:
        return {"status": "rejected_successor", "reason": "successor_unreadable", "error": str(exc),
                "token_id": selection.token_id}
    issued = _repair_token_details(connection, selection.token_id)
    if issued is None:
        raise ScenarioRegistryStoreError("repair token disappeared during successor validation")
    manifest = connection.execute(
        "SELECT source_path, current_sha256, declaration_json FROM manifest_current WHERE manifest_id = ?",
        (str(issued["manifest_id"]),),
    ).fetchone()
    if manifest is None:
        return {"status": "rejected_successor", "reason": "manifest_absent", "token_id": selection.token_id}
    declaration = _json_object(str(manifest["declaration_json"]), "R-019 repair declaration")
    if declaration.get("name") != "r019.keep_watch_acceptance_mcw":
        return {"status": "rejected_successor", "reason": "successor_not_r019", "token_id": selection.token_id}
    expected_manifest = {"source_path": str(Path(str(manifest["source_path"])).resolve()),
                         "source_sha256": str(manifest["current_sha256"] or "").lower()}
    hud_step = next((step for step in declaration.get("steps", ())
                     if isinstance(step, Mapping) and step.get("label") ==
                     "post_load_r019_keep_watch_acceptance_hud"), None)
    expected_text = hud_step.get("expected_screen_text_after_contains") if isinstance(hud_step, Mapping) else None
    expected_fact = hud_step.get("expected_visible_fact") if isinstance(hud_step, Mapping) else None
    boundary = successor.get("hud_boundary")
    if successor.get("schema") != "caol-r019-current-source-repair-successor-v1" or \
            successor.get("repair_token") != selection.token_id:
        return {"status": "rejected_successor", "reason": "successor_identity_mismatch", "token_id": selection.token_id}
    if successor.get("manifest") != expected_manifest:
        return {"status": "rejected_successor", "reason": "successor_manifest_mismatch", "token_id": selection.token_id}
    if successor.get("runtime_binding") != selection.runtime_binding:
        return {"status": "rejected_successor", "reason": "successor_runtime_binding_mismatch", "token_id": selection.token_id}
    if successor.get("gameplay_credit") is not False or successor.get("matrix_credit") is not False:
        return {"status": "rejected_successor", "reason": "successor_attempted_credit", "token_id": selection.token_id}
    if not isinstance(boundary, Mapping) or boundary.get("status") != "terminal":
        return {"status": "rejected_successor", "reason": "successor_nonterminal", "token_id": selection.token_id}
    if boundary.get("step_label") != "post_load_r019_keep_watch_acceptance_hud" or \
            boundary.get("expected_visible_fact") != expected_fact or \
            boundary.get("observed_screen_text") != expected_text:
        return {"status": "rejected_successor", "reason": "successor_visible_fact_missing", "token_id": selection.token_id}
    if successor.get("terminal_result") != "current_source_r019_hud_boundary_repaired":
        return {"status": "rejected_successor", "reason": "successor_terminal_not_repaired", "token_id": selection.token_id}
    report_id = _identity("caol-r019-current-source-repair-successor-v1", canonical_path,
                          hashlib.sha256(successor_bytes).hexdigest())
    verification_id = _identity("caol-r019-current-source-repair-verification-v1", report_id,
                                str(issued["route_key"]))
    with immediate_transaction(connection):
        prior = connection.execute(
            "SELECT 1 FROM token_history WHERE token_id = ? AND event_kind = 'repair_verification_run'",
            (selection.token_id,),
        ).fetchone()
        if prior is not None:
            return {"status": "rejected_multiple_successors", "token_id": selection.token_id}
        connection.execute(
            "INSERT INTO report_ingestion_history( report_id, manifest_id, report_path, report_sha256, report_kind, ingestion_status ) "
            "VALUES( ?, ?, ?, ?, 'r019-current-source-repair-successor', 'ingested_non_feature' )",
            (report_id, str(issued["manifest_id"]), canonical_path, hashlib.sha256(successor_bytes).hexdigest()),
        )
        details = {
            "proof": {"status": "green", "feature_proof": False, "evidence_class": "repair successor"},
            "repair_bootstrap": {"zero_credit": True, "matrix_credit": False,
                                 "terminal_result": "current_source_runtime_compatible"},
            "r019_repair_successor": {"hud_boundary": dict(boundary), "current_manifest": expected_manifest},
        }
        connection.execute(
            "INSERT INTO verification_history( verification_id, manifest_id, report_id, route_key, binding_fingerprint, outcome_kind, proof_status, supersedes_verification_id, details_json ) "
            "VALUES( ?, ?, ?, ?, ?, 'repair_successor_compatible', 'green', ?, ? )",
            (verification_id, str(issued["manifest_id"]), report_id, str(issued["route_key"]),
             _identity("caol-r019-current-source-repair-binding-v1", _json_text(selection.runtime_binding)),
             str(issued["verification_id"]), _json_text(details)),
        )
        _append_resolution_if_changed(
            connection, verification_id=verification_id, manifest_id=str(issued["manifest_id"]),
            route_key=str(issued["route_key"]), resolution_kind="compatible",
            binding_fingerprint=_identity("caol-r019-current-source-repair-binding-v1", _json_text(selection.runtime_binding)),
            details={"repair_successor": True},
        )
        _resolve_route_evidence(connection, manifest_id=str(issued["manifest_id"]), route_key=str(issued["route_key"]))
        connection.execute(
            "INSERT INTO token_history( token_id, manifest_id, verification_id, route_key, event_kind, reason, details_json ) "
            "VALUES( ?, ?, ?, ?, 'repair_verification_run', 'r019_current_source_repair_successor', ? )",
            (selection.token_id, str(issued["manifest_id"]), verification_id, str(issued["route_key"]),
             _json_text({"report_id": report_id, "red_verification_id": str(issued["verification_id"])})),
        )
    return {"status": "ingested_zero_credit", "token_id": selection.token_id, "report_id": report_id,
            "verification_id": verification_id, "idempotent": False}


def reconcile_report_bindings(connection: sqlite3.Connection, *, adapters: BindingAdapters) -> Dict[str, int]:
    """Recompute report/manifest/runtime/fixture/profile compatibility by reference."""
    reconciled = 0
    stale = 0
    adapters = _memoized_binding_adapters(adapters)
    touched_routes: set[Tuple[str, str]] = set()
    references = connection.execute(
        "SELECT report_id, report_path, report_sha256 FROM report_ingestion_history WHERE ingestion_status = 'ingested'"
    ).fetchall()
    facts_by_report = _reconciled_report_facts_by_report(
        connection, {str(reference["report_id"]) for reference in references},
    )
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
            facts = facts_by_report.get(report_id)
            if facts is None:
                raise ScenarioRegistryStoreError("ingested report has no binding facts")
        except (OSError, UnicodeDecodeError, json.JSONDecodeError, ScenarioRegistryStoreError) as exc:
            reason = str(exc)

        with immediate_transaction(connection):
            verification_id = str(verification["verification_id"])
            manifest_id = str(verification["manifest_id"])
            route_key = str(verification["route_key"])
            touched_routes.add((manifest_id, route_key))
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
    # Individual report reconciliation can temporarily retain a prior compatible
    # verdict while a later report on the same route becomes stale.  Reduce each
    # touched route once after every bound report has been refreshed, so lifecycle
    # and bootstrap authority see the current aggregate rather than that transient.
    for manifest_id, route_key in touched_routes:
        with immediate_transaction(connection):
            _resolve_route_evidence(
                connection,
                manifest_id=manifest_id,
                route_key=route_key,
            )
    return {"reconciled": reconciled, "stale": stale}


def _memoized_binding_adapters(adapters: BindingAdapters) -> BindingAdapters:
    """Observe each current binding owner once for an identical expectation."""
    cache: Dict[Tuple[str, str], Mapping[str, Any]] = {}

    def memoize(kind: str, adapter: Callable[[Mapping[str, Any]], Mapping[str, Any]]) -> Callable[[Mapping[str, Any]], Mapping[str, Any]]:
        def observe(expected: Mapping[str, Any]) -> Mapping[str, Any]:
            key = (kind, _json_text(expected))
            if key not in cache:
                cache[key] = adapter(expected)
            return cache[key]
        return observe

    return BindingAdapters(
        runtime=memoize("runtime", adapters.runtime),
        fixture=memoize("fixture", adapters.fixture),
        profile=memoize("profile", adapters.profile),
    )


def _reconciled_report_facts_by_report(
    connection: sqlite3.Connection, report_ids: set[str],
) -> Dict[str, Dict[str, Any]]:
    """Load the immutable normalized facts captured during report ingestion.

    Reconciliation still reads and hashes the referenced report, so a changed
    or missing reference becomes stale.  Decoding a report body again is not
    necessary: its binding expectations were already persisted atomically with
    the verification.  This avoids repeatedly materializing unbounded opaque
    probe payloads that cannot affect binding compatibility.
    """
    if not report_ids:
        return {}
    placeholders = ", ".join("?" for _ in report_ids)
    bindings_by_report: Dict[str, Dict[str, Mapping[str, Any]]] = {}
    invalid_reports: set[str] = set()
    for row in connection.execute(
            "SELECT report_id, binding_kind, payload_json FROM binding_history "
            "WHERE report_id IN (" + placeholders + ") "
            "AND binding_kind IN ('manifest', 'runtime', 'fixture', 'profile')",
            tuple(report_ids),
    ):
        report_id = str(row["report_id"] or "")
        try:
            payload = _json_object(str(row["payload_json"]), "binding payload")
        except ScenarioRegistryStoreError:
            invalid_reports.add(report_id)
            continue
        if str(payload.get("report_id", "")) != report_id:
            invalid_reports.add(report_id)
            continue
        kind = str(row["binding_kind"])
        if report_id in bindings_by_report and kind in bindings_by_report[report_id]:
            invalid_reports.add(report_id)
            continue
        bindings_by_report.setdefault(report_id, {})[kind] = payload
    facts_by_report: Dict[str, Dict[str, Any]] = {}
    for report_id, bindings in bindings_by_report.items():
        facts: Dict[str, Any] = {}
        for kind in ("manifest", "runtime", "fixture", "profile"):
            payload = bindings.get(kind)
            if payload is None:
                continue
            facts[kind] = dict(_object(payload.get("expected"), f"{kind} binding expected"))
        if report_id not in invalid_reports and len(facts) == 4:
            facts_by_report[report_id] = facts
    return facts_by_report
