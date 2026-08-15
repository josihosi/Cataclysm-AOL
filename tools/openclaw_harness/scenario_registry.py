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
    steps = manifest.get("steps")
    if not isinstance(steps, list):
        raise _error(path, "steps must be a list for a versioned proof_route")
    labels: List[str] = []
    for index, step in enumerate(steps, start=1):
        if not isinstance(step, dict):
            raise _error(path, f"steps[{index}] must be an object for a versioned proof_route")
        label = step.get("label")
        if not isinstance(label, str) or not label.strip():
            raise _error(path, f"steps[{index}].label must be a non-empty string for a versioned proof_route")
        labels.append(label)
    if len(set(labels)) != len(labels):
        raise _error(path, "steps must have unique labels for a versioned proof_route")
    return labels


def _validate_capabilities(value: Any, *, path: Path) -> None:
    if not isinstance(value, dict):
        raise _error(path, "capabilities must be an object")
    for key, capability in value.items():
        if not isinstance(key, str) or not any(key.startswith(prefix) for prefix in CAPABILITY_NAMESPACE_PREFIXES):
            raise _error(path, f"capabilities key {key!r} is outside the stable namespaces")
        if not _is_capability_value(capability):
            raise _error(path, f"capabilities[{key!r}] must be a JSON scalar, scalar list, or bounded object")


def _validate_runtime_contract(value: Any, *, path: Path) -> None:
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
    if value["grants_gameplay_proof"] is not False:
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


def _validate_versioned_fields(manifest: Mapping[str, Any], *, path: Path) -> None:
    version = manifest.get("manifest_version")
    if type(version) is not int or version != MANIFEST_VERSION:
        raise _error(path, f"manifest_version must be integer {MANIFEST_VERSION}")
    for field in ("capabilities", "runtime_contract", "proof_route"):
        if field not in manifest:
            raise _error(path, f"{field} is required by manifest_version {MANIFEST_VERSION}")
    _validate_capabilities(manifest["capabilities"], path=path)
    _validate_runtime_contract(manifest["runtime_contract"], path=path)
    _validate_proof_route(manifest["proof_route"], manifest, path=path)


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
