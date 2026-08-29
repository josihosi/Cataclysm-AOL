"""Deterministic, fail-closed identity primitives for future round manifests.

This module only computes identities.  It does not own registry rows, leases, or
round lifecycle.  Inputs are deliberately explicit so callers cannot silently
bind a guessed world, player, or actor set.
"""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import subprocess
from types import MappingProxyType
from typing import Any, Iterable, Mapping, Sequence


class IdentityBindingError(ValueError):
    """A required identity component is absent, unreadable, or ambiguous."""


class RoundManifestError(IdentityBindingError):
    """A complete-round manifest is missing, malformed, or has been forged."""


_EXCLUDED_DIRS = frozenset({"harness_runs", "cache", "__pycache__", ".git"})


def canonical_bytes(value: Any) -> bytes:
    """Encode JSON identity data independently of mapping insertion order."""
    try:
        return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")
    except (TypeError, ValueError) as exc:
        raise IdentityBindingError(f"identity value is not canonical JSON: {exc}") from exc


def canonical_digest(value: Any, *, domain: str) -> str:
    if not domain.strip():
        raise IdentityBindingError("identity digest domain is empty")
    digest = hashlib.sha256()
    digest.update(domain.encode("utf-8"))
    digest.update(b"\0")
    digest.update(canonical_bytes(value))
    return digest.hexdigest()


def _files(root: Path, paths: Iterable[Path] | None = None) -> list[Path]:
    root = root.resolve()
    candidates = list(paths) if paths is not None else list(root.rglob("*"))
    result: list[Path] = []
    for candidate in candidates:
        path = (candidate if candidate.is_absolute() else root / candidate).resolve()
        try:
            relative = path.resolve().relative_to(root)
        except ValueError as exc:
            raise IdentityBindingError(f"identity path is outside root: {path}") from exc
        if any(part in _EXCLUDED_DIRS for part in relative.parts):
            continue
        if path.is_file():
            result.append(path)
        elif not path.exists():
            raise IdentityBindingError(f"identity path is missing: {path}")
    return sorted(result, key=lambda item: item.relative_to(root).as_posix())


def byte_tree_digest(root: Path, paths: Iterable[Path] | None = None, *, domain: str) -> dict[str, Any]:
    """Hash sorted relative paths and bytes; mtime and directory order never participate."""
    root = root.resolve()
    if not root.exists():
        raise IdentityBindingError(f"identity root is missing: {root}")
    digest = hashlib.sha256()
    digest.update(domain.encode("utf-8"))
    digest.update(b"\0")
    records: list[dict[str, str]] = []
    for path in _files(root, paths):
        relative = path.relative_to(root).as_posix()
        try:
            content = path.read_bytes()
        except OSError as exc:
            raise IdentityBindingError(f"identity file is unreadable: {path}: {exc}") from exc
        file_sha256 = hashlib.sha256(content).hexdigest()
        digest.update(relative.encode("utf-8"))
        digest.update(b"\0")
        digest.update(content)
        digest.update(b"\0")
        records.append({"path": relative, "sha256": file_sha256})
    if not records:
        raise IdentityBindingError(f"identity root has no readable files: {root}")
    return {"sha256": digest.hexdigest(), "files": records}


def git_worktree_identity(repo_root: Path, relevant_paths: Sequence[str]) -> dict[str, Any]:
    """Hash tracked, modified, and untracked relevant files by path and bytes."""
    root = repo_root.resolve()
    command = ["git", "-C", str(root), "ls-files", "-z", "--cached", "--others", "--exclude-standard", "--", *relevant_paths]
    try:
        result = subprocess.run(command, capture_output=True, check=False)
    except OSError as exc:
        raise IdentityBindingError(f"git worktree listing failed: {exc}") from exc
    if result.returncode != 0:
        raise IdentityBindingError((result.stderr or b"git worktree listing failed").decode("utf-8", errors="replace"))
    paths = sorted(set(item for item in result.stdout.decode("utf-8", errors="surrogateescape").split("\0") if item))
    digest = hashlib.sha256(b"caol-worktree-identity-v1\0")
    records: list[dict[str, str]] = []
    for relative in paths:
        path = root / relative
        if not path.is_file():
            raise IdentityBindingError(f"tracked or untracked identity file is missing: {path}")
        try:
            content = path.read_bytes()
        except OSError as exc:
            raise IdentityBindingError(f"identity file is unreadable: {path}: {exc}") from exc
        digest.update(relative.encode("utf-8", errors="surrogateescape"))
        digest.update(b"\0")
        digest.update(content)
        digest.update(b"\0")
        records.append({"path": relative, "sha256": hashlib.sha256(content).hexdigest()})
    if not records:
        raise IdentityBindingError("relevant worktree has no tracked or untracked files")
    return {"sha256": digest.hexdigest(), "files": records}


def file_identity(path: Path, *, domain: str) -> dict[str, Any]:
    """Hash one required installed input, rejecting missing or unreadable files."""
    path = path.resolve()
    if not path.is_file():
        raise IdentityBindingError(f"required identity file is missing: {path}")
    try:
        content = path.read_bytes()
    except OSError as exc:
        raise IdentityBindingError(f"required identity file is unreadable: {path}: {exc}") from exc
    digest = hashlib.sha256(domain.encode("utf-8") + b"\0" + content).hexdigest()
    return {
        "path": str(path),
        "sha256": digest,
        # The domain-separated digest binds this component.  Keep the raw
        # byte digest as well because the registry authority and the started
        # executable report are both byte-oriented external facts.
        "content_sha256": hashlib.sha256(content).hexdigest(),
    }


def installed_world_save_identity(world_dir: Path, player_save: str) -> dict[str, Any]:
    world_dir = world_dir.resolve()
    selected = str(player_save).strip()
    if not selected or Path(selected).name != selected:
        raise IdentityBindingError("installed player save must be one explicit filename")
    save_path = world_dir / selected
    if not save_path.is_file():
        raise IdentityBindingError(f"installed player save is missing: {save_path}")
    tree = byte_tree_digest(world_dir, domain="caol-world-save:v1")
    if not any(record["path"] == selected for record in tree["files"]):
        raise IdentityBindingError(f"installed player save is excluded or unreadable: {save_path}")
    return {"world": str(world_dir), "player_save": selected, "tree": tree}


def saved_player_identity(payload: Mapping[str, Any], *, player_save: str) -> dict[str, Any]:
    player = payload.get("player")
    if not isinstance(player, Mapping):
        raise IdentityBindingError("saved player payload has no player object")
    stable_id = player.get("character_id", player.get("id", player.get("name")))
    if stable_id in (None, ""):
        raise IdentityBindingError("saved player payload has no stable identity")
    return {"player_save": player_save, "player": dict(player)}


def ecology_actor_identity(audit_output: Mapping[str, Any]) -> list[dict[str, Any]]:
    raw = audit_output.get("actors", audit_output.get("active_actors", audit_output.get("active_monsters")))
    if not isinstance(raw, Sequence) or isinstance(raw, (str, bytes)):
        raise IdentityBindingError("ecology actor audit has no actor list")
    actors: list[dict[str, Any]] = []
    for actor in raw:
        if not isinstance(actor, Mapping):
            raise IdentityBindingError("ecology actor audit contains a non-object actor")
        actor_id = actor.get("actor_id", actor.get("id"))
        if actor_id in (None, ""):
            raise IdentityBindingError("ecology actor audit contains actor without stable identity")
        actors.append(dict(actor, actor_id=actor_id))
    return normalized_records(actors, key_fields=("actor_id",))


def authoritative_identity_binding(*, repo_root: Path, executable: Path,
                                   runtime_paths: Sequence[str], data_config_roots: Sequence[Path],
                                   harness_roots: Sequence[Path], scenario_path: Path,
                                   fixture_path: Path | None, profile_path: Path | None,
                                   world_dir: Path, player_save: str,
                                   saved_player_payload: Mapping[str, Any],
                                   ecology_audit: Mapping[str, Any] | None = None) -> dict[str, Any]:
    """Produce every G2 component from explicit authoritative installed inputs."""
    components: dict[str, Any] = {
        "worktree": git_worktree_identity(repo_root, runtime_paths),
        "executable": file_identity(executable, domain="caol-executable:v1"),
        "data_config": [byte_tree_digest(root, domain="caol-data-config:v1") for root in data_config_roots],
        "harness": [byte_tree_digest(root, domain="caol-harness-source:v1") for root in harness_roots],
        "scenario": file_identity(scenario_path, domain="caol-scenario:v1"),
        "fixture": file_identity(fixture_path, domain="caol-fixture:v1") if fixture_path else None,
        "profile": file_identity(profile_path, domain="caol-profile:v1") if profile_path else None,
        "world_save": installed_world_save_identity(world_dir, player_save),
        "player": saved_player_identity(saved_player_payload, player_save=player_save),
        # Actors are discovered from the started process and bound exactly
        # once after launch; preflight seals an explicitly empty actor set.
        "actors": ecology_actor_identity(ecology_audit) if ecology_audit is not None else [],
    }
    if any(value is None for value in (components["fixture"], components["profile"])):
        raise IdentityBindingError("fixture and profile inputs are required and must be explicit")
    # A complete certification binding is deliberately over every canonical
    # component fact, not merely the five historical aggregate groups.  The
    # aggregate view remains below for callers which only need its stable
    # values; it is not the authority digest.
    binding = complete_identity_binding(
        worktree=components["worktree"],
        data_config={"roots": components["data_config"], "harness": components["harness"],
                     "scenario": components["scenario"], "fixture": components["fixture"],
                     "profile": components["profile"], "executable": components["executable"]},
        world_save=components["world_save"], player=components["player"], actors=components["actors"],
    )
    canonical_components = {
        name: component_identity(name, value)
        for name, value in components.items()
    }
    binding["components"] = canonical_components
    binding["sha256"] = canonical_digest(
        {name: canonical_components[name]["sha256"] for name in _BINDING_COMPONENTS},
        domain="caol-complete-binding:v1",
    )
    binding["authoritative_components"] = components
    return binding


def normalized_records(records: Sequence[Mapping[str, Any]], *, key_fields: Sequence[str]) -> list[dict[str, Any]]:
    """Sort identity-bearing records by explicit stable keys and reject ambiguity."""
    result = [dict(record) for record in records]
    if not result:
        raise IdentityBindingError("identity record set is empty")
    keys: list[tuple[Any, ...]] = []
    for record in result:
        key = tuple(record.get(field) for field in key_fields)
        if any(value in (None, "", []) for value in key):
            raise IdentityBindingError(f"identity record is missing stable key {key_fields}: {record}")
        keys.append(key)
    if len(set(keys)) != len(keys):
        raise IdentityBindingError("identity records contain duplicate stable keys")
    return [record for _, record in sorted(zip(keys, result), key=lambda pair: pair[0])]


def component_identity(name: str, value: Any) -> dict[str, Any]:
    if not name.strip() or value is None:
        raise IdentityBindingError(f"required identity component is missing: {name or '<unnamed>'}")
    return {"sha256": canonical_digest(value, domain=f"caol-identity:{name}:v1"), "value": value}


def complete_identity_binding(*, worktree: Any, data_config: Any, world_save: Any,
                              player: Mapping[str, Any], actors: Sequence[Mapping[str, Any]]) -> dict[str, Any]:
    """Compose the five manifest components with one deterministic binding digest."""
    actor_records = normalized_records(actors, key_fields=("actor_id",)) if actors else []
    components = {
        "worktree": component_identity("worktree", worktree),
        "data_config": component_identity("data_config", data_config),
        "world_save": component_identity("world_save", world_save),
        "player": component_identity("player", player),
        "actors": component_identity("actors", actor_records),
    }
    return {
        "schema": 1,
        "components": components,
        "sha256": canonical_digest({key: value["sha256"] for key, value in components.items()}, domain="caol-complete-binding:v1"),
    }


_ROUND_MANIFEST_SCHEMA = 1
_ROUND_MANIFEST_VERSION = 1
_ROUND_MANIFEST_FIELDS = frozenset({
    "schema", "version", "round_id", "scenario_lineage_id", "authority_id",
    "authority_kind", "event_stream_id", "event_stream_schema", "binding_id",
    "binding", "manifest_sha256",
})
_BINDING_COMPONENTS = ("worktree", "executable", "data_config", "harness", "scenario",
                       "fixture", "profile", "world_save", "player", "actors")

# One order is shared by manifest comparison, lifecycle rechecks, and lease
#/probe reporting.  Keeping this beside the identity owner prevents callers
# from accidentally selecting the first item by mapping/set iteration order.
CERTIFICATION_COMPARISON_ORDER = (
    "round_id", "scenario_lineage_id", "authority_id", "authority_kind", "event_stream_id",
    *_BINDING_COMPONENTS,
    "binding_id", "world_save_progression", "world_save_sequence", "event_stream",
    "lease_ownership", "recheck_inputs",
)


def order_certification_mismatches(mismatches: Iterable[Any]) -> list[str]:
    """Return unique mismatch names in the stable certification order."""
    present = {str(item) for item in mismatches if str(item).strip()}
    rank = {name: index for index, name in enumerate(CERTIFICATION_COMPARISON_ORDER)}
    return sorted(present, key=lambda name: (rank.get(name, len(rank)), name))


def first_certification_mismatch(mismatches: Iterable[Any]) -> str:
    """Select the one deterministic first mismatch, or an empty name."""
    ordered = order_certification_mismatches(mismatches)
    return ordered[0] if ordered else ""


def _frozen(value: Any) -> Any:
    """Freeze nested manifest data while retaining normal mapping/list access."""
    if isinstance(value, Mapping):
        return MappingProxyType({key: _frozen(item) for key, item in value.items()})
    if isinstance(value, list):
        return tuple(_frozen(item) for item in value)
    return value


def _plain(value: Any) -> Any:
    if isinstance(value, Mapping):
        return {key: _plain(item) for key, item in value.items()}
    if isinstance(value, (list, tuple)):
        return [_plain(item) for item in value]
    return value


def _manifest_payload(manifest: Mapping[str, Any]) -> dict[str, Any]:
    return _plain({key: manifest[key] for key in _ROUND_MANIFEST_FIELDS if key != "manifest_sha256"})


def _require_text(value: Any, name: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise RoundManifestError(f"manifest {name} is missing or empty")
    return value


def _validate_round_manifest(manifest: Mapping[str, Any]) -> None:
    if not isinstance(manifest, Mapping):
        raise RoundManifestError("round manifest is not an object")
    unknown = set(manifest) - _ROUND_MANIFEST_FIELDS
    missing = _ROUND_MANIFEST_FIELDS - set(manifest)
    if unknown:
        raise RoundManifestError(f"manifest has unknown fields: {', '.join(sorted(unknown))}")
    if missing:
        raise RoundManifestError(f"manifest is missing fields: {', '.join(sorted(missing))}")
    if manifest["schema"] != _ROUND_MANIFEST_SCHEMA or manifest["version"] != _ROUND_MANIFEST_VERSION:
        raise RoundManifestError("manifest schema/version is unsupported")
    for name in ("round_id", "scenario_lineage_id", "authority_id", "authority_kind", "event_stream_id", "binding_id", "manifest_sha256"):
        _require_text(manifest[name], name)
    if manifest["event_stream_schema"] != _ROUND_MANIFEST_SCHEMA:
        raise RoundManifestError("event stream schema is unsupported")
    binding = manifest["binding"]
    if not isinstance(binding, Mapping) or binding.get("schema") != _ROUND_MANIFEST_SCHEMA:
        raise RoundManifestError("complete binding is missing or unsupported")
    if set(binding.get("components", {})) != set(_BINDING_COMPONENTS):
        raise RoundManifestError("complete binding has missing or unknown component groups")
    if set(binding.get("authoritative_components", {})) != set(_BINDING_COMPONENTS):
        raise RoundManifestError("complete binding has missing or unknown authoritative components")
    components = binding["components"]
    authoritative = binding["authoritative_components"]
    expected_components = {
        name: component_identity(name, _plain(authoritative[name]))
        for name in _BINDING_COMPONENTS
    }
    if _plain(components) != expected_components:
        raise RoundManifestError("complete binding component facts do not match authoritative facts")
    expected_binding_id = canonical_digest(
        {name: expected_components[name]["sha256"] for name in _BINDING_COMPONENTS},
        domain="caol-complete-binding:v1",
    )
    if binding.get("sha256") != expected_binding_id or binding.get("sha256") != manifest["binding_id"]:
        raise RoundManifestError("binding_id does not match authoritative complete binding")
    expected = canonical_digest(_manifest_payload(manifest), domain="caol-round-manifest:v1")
    if expected != manifest["manifest_sha256"]:
        raise RoundManifestError("round manifest seal does not verify")


def seal_complete_round_manifest(*, round_id: str, scenario_lineage_id: str,
                                 authority_id: str, authority_kind: str,
                                 event_stream_id: str, **producer_inputs: Any) -> Mapping[str, Any]:
    """Seal one complete round by deriving (never accepting) its binding ID."""
    binding = authoritative_identity_binding(**producer_inputs)
    manifest: dict[str, Any] = {
        "schema": _ROUND_MANIFEST_SCHEMA,
        "version": _ROUND_MANIFEST_VERSION,
        "round_id": _require_text(round_id, "round_id"),
        "scenario_lineage_id": _require_text(scenario_lineage_id, "scenario_lineage_id"),
        "authority_id": _require_text(authority_id, "authority_id"),
        "authority_kind": _require_text(authority_kind, "authority_kind"),
        "event_stream_id": _require_text(event_stream_id, "event_stream_id"),
        "event_stream_schema": _ROUND_MANIFEST_SCHEMA,
        "binding_id": binding["sha256"],
        "binding": binding,
    }
    manifest["manifest_sha256"] = canonical_digest(manifest, domain="caol-round-manifest:v1")
    sealed = _frozen(manifest)
    _validate_round_manifest(sealed)
    return sealed


def compare_round_manifest(manifest: Mapping[str, Any], *, round_id: str,
                           scenario_lineage_id: str, authority_id: str,
                           authority_kind: str, event_stream_id: str,
                           **producer_inputs: Any) -> dict[str, Any]:
    """Recompute authoritative inputs and return ordered drift names, without mutation."""
    _validate_round_manifest(manifest)
    current = authoritative_identity_binding(**producer_inputs)
    mismatches: list[str] = []
    for name, expected in (("round_id", round_id), ("scenario_lineage_id", scenario_lineage_id),
                           ("authority_id", authority_id), ("authority_kind", authority_kind),
                           ("event_stream_id", event_stream_id)):
        if manifest[name] != expected:
            mismatches.append(name)
    if current["sha256"] != manifest["binding_id"]:
        sealed_components = manifest["binding"]["authoritative_components"]
        current_components = current["authoritative_components"]
        for name in _BINDING_COMPONENTS:
            sealed_value = canonical_digest(_plain(sealed_components[name]), domain=f"caol-authoritative-component:{name}:v1")
            current_value = canonical_digest(current_components[name], domain=f"caol-authoritative-component:{name}:v1")
            if sealed_value != current_value:
                mismatches.append(name)
        if "binding_id" not in mismatches:
            mismatches.append("binding_id")
    mismatches = order_certification_mismatches(mismatches)
    result = {"ok": not mismatches, "mismatches": mismatches}
    if mismatches:
        result["first_mismatch"] = first_certification_mismatch(mismatches)
    return result


def recheck_round_manifest(manifest: Mapping[str, Any], **kwargs: Any) -> dict[str, Any]:
    """Explicit alias for the non-mutating round-manifest comparison operation."""
    return compare_round_manifest(manifest, **kwargs)
