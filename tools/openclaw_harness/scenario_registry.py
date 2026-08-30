#!/usr/bin/env python3
"""Read-only validation and normalization for harness scenario declarations.

Scenario JSON files are declarations, not evidence.  This module never writes a
manifest and deliberately does not derive capability facts from descriptive
text, filenames, fixtures, or steps.
"""

from __future__ import annotations

import copy
import hashlib
import json
from pathlib import Path
from typing import Any, Dict, List, Mapping


MANIFEST_VERSION = 1
CHECKPOINT_CHAIN_MANIFEST_VERSION = 2
CAPABILITY_NAMESPACE_PREFIXES = (
    "player.",
    "local_place.",
    "actors.",
    "world.",
    "capabilities.",
    "runtime.",
)
PROOF_ROUTE_ROLES = (
    "precondition",
    "production_behavior",
    "terminal_persistence",
    "artifact_verdict",
    "disallowed_shortcuts",
)
PROOF_DEPTHS = (
    "startup",
    "interaction",
    "terminal",
    "persistence",
    "replay",
)
RUNTIME_REQUIREMENT_KEYS = (
    "os",
    "source",
    "executable",
    "profile",
    "fixture",
    "helper",
    "peekaboo",
    "input",
    "ocr",
    "cleanup",
)
_RELATION_PROSE_FIELDS = frozenset({
    "artifact_description",
    "artifact_narration",
    "artifact_pattern",
    "artifact_patterns",
    "artifact_source",
    "comment",
    "comments",
    "description",
    "recommendation",
    "recommended_test_command",
})
_RELATION_PROOF_DEPTHS = {
    "precondition": 0,
    "production_behavior": 1,
    "artifact_verdict": 2,
    "terminal_persistence": 3,
    "disallowed_shortcuts": 4,
}


class ManifestValidationError(ValueError):
    """A scenario declaration cannot safely be treated as versioned."""


def _error(path: Path, message: str) -> ManifestValidationError:
    return ManifestValidationError(f"Invalid scenario manifest {path}: {message}")


def _is_json_scalar(value: Any) -> bool:
    return value is None or isinstance(value, (bool, int, float, str))


def _is_scalar_list(value: Any) -> bool:
    return isinstance(value, list) and all(_is_json_scalar(item) for item in value)


def _is_bounded_object(value: Any) -> bool:
    """Allow one explicit object layer with scalar or scalar-list leaves.

    This permits typed records such as coordinates or actor state while avoiding
    arbitrary nested prose-shaped payloads.  It supplies a structural bound,
    rather than an invented count limit.
    """
    return isinstance(value, dict) and all(
        isinstance(key, str) and key.strip() and (_is_json_scalar(item) or _is_scalar_list(item))
        for key, item in value.items()
    )


def _is_capability_value(value: Any) -> bool:
    return _is_json_scalar(value) or _is_scalar_list(value) or _is_bounded_object(value)


def _require_string_list(value: Any, *, path: Path, field: str) -> List[str]:
    if not isinstance(value, list) or not value or any(not isinstance(item, str) or not item.strip() for item in value):
        raise _error(path, f"{field} must be a non-empty list of strings")
    return list(value)


def _step_labels(manifest: Mapping[str, Any], *, path: Path) -> List[str]:
    initial_steps = manifest.get("steps")
    if not isinstance(initial_steps, list):
        raise _error(path, "steps must be a list for a versioned proof_route")
    step_groups = [("steps", initial_steps)]
    post_relaunch = manifest.get("post_relaunch")
    if post_relaunch is not None:
        if not isinstance(post_relaunch, dict):
            raise _error(path, "post_relaunch must be an object for a versioned proof_route")
        post_relaunch_steps = post_relaunch.get("steps")
        if not isinstance(post_relaunch_steps, list):
            raise _error(path, "post_relaunch.steps must be a list for a versioned proof_route")
        step_groups.append(("post_relaunch.steps", post_relaunch_steps))
    labels: List[str] = []
    for group_name, steps in step_groups:
        for index, step in enumerate(steps, start=1):
            if not isinstance(step, dict):
                raise _error(path, f"{group_name}[{index}] must be an object for a versioned proof_route")
            label = step.get("label")
            if not isinstance(label, str) or not label.strip():
                raise _error(path, f"{group_name}[{index}].label must be a non-empty string for a versioned proof_route")
            labels.append(label)
    if len(set(labels)) != len(labels):
        raise _error(path, "initial and post-relaunch steps must have unique labels for a versioned proof_route")
    return labels


def _validate_capabilities(value: Any, *, path: Path) -> None:
    if not isinstance(value, dict):
        raise _error(path, "capabilities must be an object")
    for key, capability in value.items():
        if not isinstance(key, str) or not any(key.startswith(prefix) for prefix in CAPABILITY_NAMESPACE_PREFIXES):
            raise _error(path, f"capabilities key {key!r} is outside the stable namespaces")
        if not _is_capability_value(capability):
            raise _error(path, f"capabilities[{key!r}] must be a JSON scalar, scalar list, or bounded object")


def _validate_runtime_contract(
    value: Any, *, path: Path, allow_gameplay_proof: bool = False,
) -> None:
    if not isinstance(value, dict):
        raise _error(path, "runtime_contract must be an object")

    required = {
        "permitted_input": list,
        "forbidden_input": list,
        "setup_only_debug": bool,
        "disposable_copy": bool,
        "helpers": list,
        "permissions": list,
        "platform": list,
        "profile": str,
        "fixture": str,
        "requirements": dict,
        "grants_gameplay_proof": bool,
    }
    for key, expected_type in required.items():
        field_value = value.get(key)
        if type(field_value) is not expected_type:
            raise _error(path, f"runtime_contract.{key} must be {expected_type.__name__}")
    if value["grants_gameplay_proof"] is not False and not allow_gameplay_proof:
        raise _error(path, "runtime_contract.grants_gameplay_proof must be false")
    for key in ("permitted_input", "forbidden_input", "helpers", "permissions", "platform"):
        if any(not isinstance(item, str) or not item.strip() for item in value[key]):
            raise _error(path, f"runtime_contract.{key} must contain only non-empty strings")

    requirements = value["requirements"]
    for key in RUNTIME_REQUIREMENT_KEYS:
        if key not in requirements:
            raise _error(path, f"runtime_contract.requirements.{key} is required")
        if not _is_capability_value(requirements[key]):
            raise _error(path, f"runtime_contract.requirements.{key} has an unsupported JSON shape")


def _validate_proof_route(value: Any, manifest: Mapping[str, Any], *, path: Path) -> None:
    if not isinstance(value, dict):
        raise _error(path, "proof_route must be an object")
    labels = set(_step_labels(manifest, path=path))
    for role in PROOF_ROUTE_ROLES:
        references = _require_string_list(value.get(role), path=path, field=f"proof_route.{role}")
        unknown = [label for label in references if label not in labels]
        if unknown:
            raise _error(path, f"proof_route.{role} references unknown step label(s): {', '.join(unknown)}")
    capability_gates = value.get("capability_gates")
    if capability_gates is None:
        return
    if not isinstance(capability_gates, dict):
        raise _error(path, "proof_route.capability_gates must be an object when present")
    declared_capabilities = manifest.get("capabilities")
    if not isinstance(declared_capabilities, dict):
        raise _error(path, "proof_route.capability_gates requires declared capabilities")
    eligible_labels = set().union(*(set(value[role]) for role in PROOF_ROUTE_ROLES[:-1]))
    for capability_key, depth_gates in capability_gates.items():
        if capability_key not in declared_capabilities:
            raise _error(path, f"proof_route.capability_gates references undeclared capability {capability_key!r}")
        if not isinstance(depth_gates, dict) or not depth_gates:
            raise _error(path, f"proof_route.capability_gates[{capability_key!r}] must be a non-empty object")
        for depth, gates in depth_gates.items():
            if depth not in PROOF_DEPTHS:
                raise _error(path, f"proof_route.capability_gates[{capability_key!r}] has unknown proof depth {depth!r}")
            references = _require_string_list(
                gates,
                path=path,
                field=f"proof_route.capability_gates[{capability_key!r}][{depth!r}]",
            )
            ineligible = [label for label in references if label not in eligible_labels]
            if ineligible:
                raise _error(
                    path,
                    f"proof_route.capability_gates[{capability_key!r}][{depth!r}] references non-production gate(s): {', '.join(ineligible)}",
                )


def _validate_source_binding_validation(value: Any, manifest: Mapping[str, Any], *, path: Path) -> None:
    """Validate the declaration of a repository-owned source-fact validator."""
    if not isinstance(value, dict):
        raise _error(path, "source_binding_validation must be an object")
    if set(value) - {"validator", "bootstrap_artifact", "capabilities", "exclusive_review_required"}:
        raise _error(path, "source_binding_validation contains an unsupported field")
    if value.get("validator") not in {
            "r008_closure_046_source_binding",
            "r008_natural_wait_progress_source_binding",
    }:
        raise _error(path, "source_binding_validation.validator is unsupported")
    bootstrap_artifact = value.get("bootstrap_artifact")
    if not isinstance(bootstrap_artifact, str) or not bootstrap_artifact.strip():
        raise _error(path, "source_binding_validation.bootstrap_artifact must be a non-empty string")
    capabilities = _require_string_list(
        value.get("capabilities"), path=path, field="source_binding_validation.capabilities",
    )
    declared = manifest.get("capabilities")
    if not isinstance(declared, dict) or any(key not in declared for key in capabilities):
        raise _error(path, "source_binding_validation.capabilities must name declared capabilities")
    if "exclusive_review_required" in value and type(value["exclusive_review_required"]) is not bool:
        raise _error(path, "source_binding_validation.exclusive_review_required must be a boolean")


def _validate_checkpoint_gate_expectations(value: Any, *, path: Path, field: str) -> None:
    if not isinstance(value, list) or not value:
        raise _error(path, f"{field} must be a non-empty list")
    for index, expectation in enumerate(value, start=1):
        if not isinstance(expectation, dict):
            raise _error(path, f"{field}[{index}] must be an object")
        if set(expectation) != {"kind", "predicate"}:
            raise _error(path, f"{field}[{index}] must contain exactly kind and predicate")
        if expectation.get("kind") not in {"structured_event", "semantic_state", "saved_artifact"}:
            raise _error(path, f"{field}[{index}].kind must name structured_event, semantic_state, or saved_artifact")
        predicate = expectation.get("predicate")
        if not _is_bounded_object(predicate) or not predicate:
            raise _error(path, f"{field}[{index}].predicate must be a non-empty bounded object")


def _validate_checkpoint_safe_ui(value: Any, *, path: Path, field: str) -> None:
    if path.name in {
            "bandit.r005_continuous_hostile_ecology_certification.json",
            "bandit.r005_natural_route_qualification.json",
            "bandit.r005_direct_native_route_qualification.json",
            "bandit.r005_native_wait_qualification.json",
    }:
        if value != {"semantic_state": {"required": True}}:
            raise _error(path, f"{field} must require semantic_state for the improved R-007 route")
        return
    if not isinstance(value, dict) or set(value) != {"screen_text_contains"}:
        raise _error(path, f"{field} must contain exactly screen_text_contains")
    _require_string_list(value.get("screen_text_contains"), path=path,
                         field=f"{field}.screen_text_contains")


def _validate_checkpoint_proof_route(
    value: Any,
    manifest: Mapping[str, Any],
    gate_ids: List[str],
    *,
    path: Path,
) -> None:
    if not isinstance(value, dict):
        raise _error(path, "proof_route must be an object")
    allowed = {"gates", "terminal", "capability_gates"}
    unknown_fields = set(value) - allowed
    if unknown_fields:
        raise _error(path, f"proof_route has unknown field(s): {', '.join(sorted(unknown_fields))}")
    gates = _require_string_list(value.get("gates"), path=path, field="proof_route.gates")
    if gates != gate_ids:
        raise _error(path, "proof_route.gates must contain every proof gate in declaration order")
    terminal = _require_string_list(value.get("terminal"), path=path, field="proof_route.terminal")
    if terminal != [gate_ids[-1]]:
        raise _error(path, "proof_route.terminal must contain exactly the final proof gate")
    capability_gates = value.get("capability_gates")
    if capability_gates is None:
        return
    if not isinstance(capability_gates, dict):
        raise _error(path, "proof_route.capability_gates must be an object when present")
    declared_capabilities = manifest.get("capabilities")
    if not isinstance(declared_capabilities, dict):
        raise _error(path, "proof_route.capability_gates requires declared capabilities")
    for capability_key, depth_gates in capability_gates.items():
        if capability_key not in declared_capabilities:
            raise _error(path, f"proof_route.capability_gates references undeclared capability {capability_key!r}")
        if not isinstance(depth_gates, dict) or not depth_gates:
            raise _error(path, f"proof_route.capability_gates[{capability_key!r}] must be a non-empty object")
        for depth, gates_at_depth in depth_gates.items():
            if depth not in PROOF_DEPTHS:
                raise _error(path, f"proof_route.capability_gates[{capability_key!r}] has unknown proof depth {depth!r}")
            references = _require_string_list(
                gates_at_depth,
                path=path,
                field=f"proof_route.capability_gates[{capability_key!r}][{depth!r}]",
            )
            unknown = [gate_id for gate_id in references if gate_id not in gate_ids]
            if unknown:
                raise _error(
                    path,
                    f"proof_route.capability_gates[{capability_key!r}] references unknown proof gate(s): {', '.join(unknown)}",
                )


def _validate_checkpoint_chain_fields(manifest: Mapping[str, Any], *, path: Path) -> None:
    required_fields = (
        "capabilities", "runtime_contract", "run_class", "observer_character",
        "installed_save_player", "proof_gates", "proof_route",
    )
    for field in required_fields:
        if field not in manifest:
            raise _error(path, f"{field} is required by manifest_version {CHECKPOINT_CHAIN_MANIFEST_VERSION}")
    _validate_capabilities(manifest["capabilities"], path=path)
    _validate_runtime_contract(
        manifest["runtime_contract"], path=path,
        allow_gameplay_proof=manifest.get("name") in {
            "r018.raw_wait_acceptance_mcw", "r019.keep_watch_acceptance_mcw",
            "r019.primitive_safe_popup_comparison_mcw",
        },
    )
    if manifest["run_class"] not in {"combat", "non_combat"}:
        raise _error(path, "run_class must be combat or non_combat")
    if type(manifest["observer_character"]) is not bool:
        raise _error(path, "observer_character must be boolean")
    observer_safety_mode = manifest.get("observer_safety_mode")
    if observer_safety_mode is not None and observer_safety_mode != "invisible":
        raise _error(path, "observer_safety_mode must be invisible when declared")
    player_save = manifest["installed_save_player"]
    if not isinstance(player_save, str) or not player_save.strip():
        raise _error(path, "installed_save_player must be a non-empty string")
    step_labels = _step_labels(manifest, path=path)
    proof_gates = manifest["proof_gates"]
    if not isinstance(proof_gates, list) or not proof_gates:
        raise _error(path, "proof_gates must be a non-empty ordered list")
    gate_ids: List[str] = []
    prior_gate_id = ""
    prior_boundary_index = -1
    for index, gate in enumerate(proof_gates, start=1):
        if not isinstance(gate, dict):
            raise _error(path, f"proof_gates[{index}] must be an object")
        if set(gate) != {"id", "label", "boundary_step", "predecessors", "expectations", "checkpoint_safe_ui"}:
            raise _error(path, f"proof_gates[{index}] has an unsupported schema")
        gate_id = gate.get("id")
        if not isinstance(gate_id, str) or not gate_id.strip() or gate_id in gate_ids:
            raise _error(path, f"proof_gates[{index}].id must be a unique non-empty string")
        label = gate.get("label")
        if not isinstance(label, str) or not label.strip():
            raise _error(path, f"proof_gates[{index}].label must be a non-empty string")
        boundary_step = gate.get("boundary_step")
        if not isinstance(boundary_step, str) or boundary_step not in step_labels:
            raise _error(path, f"proof_gates[{index}].boundary_step must name a scenario step")
        boundary_index = step_labels.index(boundary_step)
        if boundary_index <= prior_boundary_index:
            raise _error(path, "proof gate boundaries must be strictly ordered by scenario step")
        predecessors = gate.get("predecessors")
        if not isinstance(predecessors, list) or any(
                not isinstance(item, str) or not item.strip() for item in predecessors):
            raise _error(path, f"proof_gates[{index}].predecessors must be a list of non-empty strings")
        expected_predecessors = [] if not prior_gate_id else [prior_gate_id]
        if predecessors != expected_predecessors:
            raise _error(path, "proof gates must declare the immediately preceding gate as their only predecessor")
        _validate_checkpoint_gate_expectations(
            gate.get("expectations"), path=path, field=f"proof_gates[{index}].expectations",
        )
        _validate_checkpoint_safe_ui(
            gate.get("checkpoint_safe_ui"), path=path, field=f"proof_gates[{index}].checkpoint_safe_ui",
        )
        gate_ids.append(gate_id)
        prior_gate_id = gate_id
        prior_boundary_index = boundary_index
    causal_boundary_gate = manifest.get("causal_boundary_gate")
    if causal_boundary_gate is not None:
        if not isinstance(causal_boundary_gate, str) or causal_boundary_gate not in gate_ids:
            raise _error(path, "causal_boundary_gate must name a declared proof gate")
        selected_gate = next(gate for gate in proof_gates if gate["id"] == causal_boundary_gate)
        has_owner_predicate = any(
            expectation.get("kind") == "structured_event" and
            isinstance(expectation.get("predicate"), dict) and
            ("simulation_owner" in expectation["predicate"] or "owner" in expectation["predicate"])
            for expectation in selected_gate["expectations"]
        )
        if not has_owner_predicate:
            raise _error(path, "causal_boundary_gate must declare a structured owner predicate")
    _validate_checkpoint_proof_route(manifest["proof_route"], manifest, gate_ids, path=path)


def _validate_versioned_fields(manifest: Mapping[str, Any], *, path: Path) -> None:
    version = manifest.get("manifest_version")
    if type(version) is not int or version not in {MANIFEST_VERSION, CHECKPOINT_CHAIN_MANIFEST_VERSION}:
        raise _error(path, f"manifest_version must be integer {MANIFEST_VERSION} or {CHECKPOINT_CHAIN_MANIFEST_VERSION}")
    if version == CHECKPOINT_CHAIN_MANIFEST_VERSION:
        _validate_checkpoint_chain_fields(manifest, path=path)
        return
    for field in ("capabilities", "runtime_contract", "proof_route"):
        if field not in manifest:
            raise _error(path, f"{field} is required by manifest_version {MANIFEST_VERSION}")
    _validate_capabilities(manifest["capabilities"], path=path)
    _validate_runtime_contract(manifest["runtime_contract"], path=path)
    _validate_proof_route(manifest["proof_route"], manifest, path=path)
    if "source_binding_validation" in manifest:
        _validate_source_binding_validation(manifest["source_binding_validation"], manifest, path=path)
    if manifest.get("name") == "bandit.r008_closure_046_source_bound_m095_mcw":
        validation = manifest.get("source_binding_validation")
        if not isinstance(validation, Mapping):
            raise _error(path, "R-008 M095 requires source_binding_validation before its footing is selectable")


def _normalized_field(manifest: Mapping[str, Any], field: str, *, legacy: bool) -> Dict[str, Any]:
    if field not in manifest:
        return {
            "state": "unknown",
            "review_required": True,
            "value": None,
        }
    return {
        "state": "declared" if not legacy else "declared_unversioned",
        "review_required": legacy,
        "value": copy.deepcopy(manifest[field]),
    }


def _relation_json(value: Any) -> Any:
    """Canonicalize supported JSON without deriving facts from prose."""
    if isinstance(value, dict):
        return {
            str(key): _relation_json(value[key])
            for key in sorted(value)
        }
    if isinstance(value, list):
        return [_relation_json(item) for item in value]
    return copy.deepcopy(value)


def _relation_step(step: Mapping[str, Any]) -> Dict[str, Any]:
    arguments = {
        str(key): value
        for key, value in step.items()
        if key not in {"kind", "label"} and str(key).lower() not in _RELATION_PROSE_FIELDS
    }
    return {
        "label": str(step.get("label", "")),
        "kind": str(step.get("kind", "")),
        "arguments": _relation_json(arguments),
    }


def normalize_relation_contract(manifest: Mapping[str, Any]) -> Dict[str, Any] | None:
    """Return the structured review contract for duplicate/subsumption analysis.

    Names, descriptions, comments, recommendations, and artifact narration are
    deliberately absent.  Legacy declarations remain review-required and have
    no relation contract until they opt into the versioned schema.
    """
    version = manifest.get("manifest_version")
    if version not in {MANIFEST_VERSION, CHECKPOINT_CHAIN_MANIFEST_VERSION}:
        return None
    capabilities = manifest.get("capabilities")
    runtime_contract = manifest.get("runtime_contract")
    proof_route = manifest.get("proof_route")
    steps = manifest.get("steps")
    if (
        not isinstance(capabilities, Mapping)
        or not isinstance(runtime_contract, Mapping)
        or not isinstance(proof_route, Mapping)
        or not isinstance(steps, list)
        or not all(isinstance(step, Mapping) for step in steps)
    ):
        return None
    requirements = runtime_contract.get("requirements")
    if not isinstance(requirements, Mapping):
        return None
    footing = {
        "fixture": runtime_contract.get("fixture"),
        "profile": runtime_contract.get("profile"),
        "requirements_fixture": requirements.get("fixture"),
        "requirements_profile": requirements.get("profile"),
        "helpers": runtime_contract.get("helpers"),
        "permissions": runtime_contract.get("permissions"),
        "platform": runtime_contract.get("platform"),
        "setup_only_debug": runtime_contract.get("setup_only_debug"),
        "disposable_copy": runtime_contract.get("disposable_copy"),
    }
    contract = {
        "hard_requirements": _relation_json(requirements),
        "capabilities": _relation_json(capabilities),
        "footing": _relation_json(footing),
        "permitted_input": _relation_json(runtime_contract.get("permitted_input")),
        "forbidden_input": _relation_json(runtime_contract.get("forbidden_input")),
        "steps": [_relation_step(step) for step in steps],
        "proof_route": _relation_json(proof_route),
    }
    if version == CHECKPOINT_CHAIN_MANIFEST_VERSION:
        contract["checkpoint_contract"] = {
            "manifest_version": version,
            "run_class": _relation_json(manifest.get("run_class")),
            "observer_character": _relation_json(manifest.get("observer_character")),
            "installed_save_player": _relation_json(manifest.get("installed_save_player")),
            "capabilities": _relation_json(capabilities),
            "runtime_contract": _relation_json(runtime_contract),
            "proof_gates": _relation_json(manifest.get("proof_gates")),
            "proof_route": _relation_json(proof_route),
        }
        if "observer_safety_mode" in manifest:
            contract["checkpoint_contract"]["observer_safety_mode"] = _relation_json(
                manifest["observer_safety_mode"]
            )
        if "required_stabilizer_traits" in manifest:
            contract["checkpoint_contract"]["required_stabilizer_traits"] = _relation_json(
                manifest["required_stabilizer_traits"]
            )
    return contract


def _relation_mapping_contains(container: Mapping[str, Any], required: Mapping[str, Any]) -> bool:
    return all(key in container and container[key] == value for key, value in required.items())


def _relation_production_sequence(contract: Mapping[str, Any]) -> List[Dict[str, Any]]:
    route = contract["proof_route"]
    labels = route.get("production_behavior", []) if isinstance(route, Mapping) else []
    by_label = {step["label"]: step for step in contract["steps"]}
    return [by_label[label] for label in labels if label in by_label]


def _relation_is_subsequence(subject: List[Dict[str, Any]], candidate: List[Dict[str, Any]]) -> bool:
    position = 0
    for expected in subject:
        while position < len(candidate) and candidate[position] != expected:
            position += 1
        if position == len(candidate):
            return False
        position += 1
    return True


def relation_contract_likely_subsumes(
    subject: Mapping[str, Any],
    candidate: Mapping[str, Any],
) -> bool:
    """Conservatively identify a review-only successor candidate.

    The candidate must run on the same footing/input contract, require no
    additional hard facts, preserve the subject production sequence, and carry
    every named outcome/control gate at equal or greater proof depth.
    """
    if (
        subject["footing"] != candidate["footing"]
        or subject["permitted_input"] != candidate["permitted_input"]
        or subject["forbidden_input"] != candidate["forbidden_input"]
    ):
        return False
    if not _relation_mapping_contains(subject["hard_requirements"], candidate["hard_requirements"]):
        return False
    if not _relation_mapping_contains(candidate["capabilities"], subject["capabilities"]):
        return False
    if not _relation_is_subsequence(
        _relation_production_sequence(subject),
        _relation_production_sequence(candidate),
    ):
        return False
    subject_route = subject["proof_route"]
    candidate_route = candidate["proof_route"]
    if not isinstance(subject_route, Mapping) or not isinstance(candidate_route, Mapping):
        return False
    candidate_depths: Dict[str, int] = {}
    for role, depth in _RELATION_PROOF_DEPTHS.items():
        labels = candidate_route.get(role, [])
        if isinstance(labels, list):
            for label in labels:
                if isinstance(label, str):
                    candidate_depths[label] = max(candidate_depths.get(label, -1), depth)
    for role in ("terminal_persistence", "artifact_verdict", "disallowed_shortcuts"):
        labels = subject_route.get(role, [])
        if not isinstance(labels, list):
            return False
        for label in labels:
            if not isinstance(label, str) or candidate_depths.get(label, -1) < _RELATION_PROOF_DEPTHS[role]:
                return False
    return True


def validate_manifest(manifest: Any, *, path: Path) -> Dict[str, Any]:
    """Return a non-mutating normalized view of one scenario declaration.

    Missing versioned fields are a legacy compatibility state, never a positive
    fact.  A present manifest_version opts into strict validation.  The source
    binding is calculated from the exact bytes read from ``path`` so later run
    reports can identify the declaration they used without modifying it.
    """
    if not isinstance(manifest, dict):
        raise _error(path, "top level must be an object")
    try:
        source_bytes = path.read_bytes()
    except OSError as exc:
        raise _error(path, f"could not read source bytes: {exc}") from exc
    try:
        source_manifest = json.loads(source_bytes.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise _error(path, f"source is not valid UTF-8 JSON: {exc}") from exc
    if not isinstance(source_manifest, dict) or source_manifest != manifest:
        raise _error(path, "provided declaration does not match the source bytes")

    versioned = "manifest_version" in manifest
    if versioned:
        _validate_versioned_fields(manifest, path=path)
    else:
        # A legacy declaration may adopt any one structured field before it
        # adopts manifest_version.  Validate what it explicitly declares, but
        # leave the missing fields unknown/review-required.
        if "capabilities" in manifest:
            _validate_capabilities(manifest["capabilities"], path=path)
        if "runtime_contract" in manifest:
            _validate_runtime_contract(manifest["runtime_contract"], path=path)
        if "proof_route" in manifest:
            _validate_proof_route(manifest["proof_route"], manifest, path=path)

    normalized_fields = {
        field: _normalized_field(manifest, field, legacy=not versioned)
        for field in ("manifest_version", "capabilities", "runtime_contract", "proof_route")
    }
    if manifest.get("manifest_version") == CHECKPOINT_CHAIN_MANIFEST_VERSION:
        normalized_fields.update({
            field: _normalized_field(manifest, field, legacy=False)
            for field in (
                "run_class", "observer_character", "observer_safety_mode",
                "installed_save_player", "proof_gates",
            )
            if field in manifest
        })
        if "required_stabilizer_traits" in manifest:
            normalized_fields["required_stabilizer_traits"] = _normalized_field(
                manifest, "required_stabilizer_traits", legacy=False
            )
    review_required = any(field["review_required"] for field in normalized_fields.values())
    return {
        "declaration": copy.deepcopy(manifest),
        "source": {
            "path": str(path.resolve()),
            "sha256": hashlib.sha256(source_bytes).hexdigest(),
        },
        "normalized": normalized_fields,
        "validation": {
            "status": "review_required" if review_required else "valid",
            "review_required": review_required,
            "manifest_version": manifest.get("manifest_version") if versioned else None,
        },
    }
