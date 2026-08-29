#!/usr/bin/env python3
"""Receipt-bearing, disposable camp setup through the fixture save owner.

This is deliberately a setup transaction, never gameplay proof.  It writes
only the two persisted basecamp identities owned by
``player_basecamp_at_omt`` and restores their exact previous bytes when the
transaction completes.
"""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
from typing import Any, Dict, Mapping, Tuple

from startup_harness import (
    apply_player_basecamp_at_omt_transform,
    cleanup_extracted_overmap,
    extract_overmap_payload,
    overmap_file_coords_from_abs_omt,
    run_zzip,
)


class ControlledCampSetupError(RuntimeError):
    """The declared disposable camp could not be prepared safely."""


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _canonical_json(value: Any) -> str:
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":"))


def _player_camps(player_save: Path) -> Tuple[list[int], list[dict[str, Any]]]:
    extracted = player_save.with_suffix("")
    run_zzip(player_save)
    try:
        payload = json.loads(extracted.read_text(encoding="utf-8"))
        player = payload.get("player") if isinstance(payload, dict) else None
        location = player.get("location") if isinstance(player, dict) else None
        camps = player.get("camps", []) if isinstance(player, dict) else []
        if not isinstance(location, list) or len(location) < 3 or not isinstance(camps, list):
            raise ControlledCampSetupError("player save lacks camp identity fields")
        return [int(location[0]) // 24, int(location[1]) // 24, int(location[2])], [
            dict(camp) for camp in camps if isinstance(camp, dict)
        ]
    finally:
        if extracted.exists():
            extracted.unlink()


def _overmap_camps(overmap_path: Path) -> list[dict[str, Any]]:
    plain, _version, payload = extract_overmap_payload(overmap_path)
    try:
        camps = payload.get("camps", [])
        if not isinstance(camps, list):
            raise ControlledCampSetupError("overmap save has a non-list camp registry")
        return [dict(camp) for camp in camps if isinstance(camp, dict)]
    finally:
        cleanup_extracted_overmap(plain, keep=not bool(payload.get("_created_plain", False)))


def _player_payload(player_save: Path) -> dict[str, Any]:
    extracted = player_save.with_suffix("")
    run_zzip(player_save)
    try:
        payload = json.loads(extracted.read_text(encoding="utf-8"))
        if not isinstance(payload, dict):
            raise ControlledCampSetupError("player save payload is not an object")
        return json.loads(_canonical_json(payload))
    finally:
        if extracted.exists():
            extracted.unlink()


def _overmap_payload(overmap_path: Path) -> dict[str, Any]:
    plain, _version, payload = extract_overmap_payload(overmap_path)
    try:
        if not isinstance(payload, dict):
            raise ControlledCampSetupError("overmap save payload is not an object")
        return json.loads(_canonical_json(payload))
    finally:
        cleanup_extracted_overmap(plain, keep=not bool(payload.get("_created_plain", False)))


def _without_declared_camp(payload: Mapping[str, Any], *, declared_omt: list[int], player_owner: bool) -> str:
    """Compare each persisted owner after removing only its declared mutation."""
    normalized = json.loads(_canonical_json(payload))
    if player_owner:
        player = normalized.get("player")
        camps = player.get("camps") if isinstance(player, dict) else None
        if not isinstance(camps, list):
            raise ControlledCampSetupError("player save lacks a camp registry after setup")
        player["camps"] = [camp for camp in camps if not isinstance(camp, dict) or camp.get("pos") != declared_omt]
    else:
        camps = normalized.get("camps")
        if not isinstance(camps, list):
            raise ControlledCampSetupError("overmap save lacks a camp registry after setup")
        normalized["camps"] = [
            camp for camp in camps
            if not isinstance(camp, dict) or camp.get("pos") != declared_omt
        ]
    return _canonical_json(normalized)


def run_controlled_camp_setup(world_dir: Path, declaration: Mapping[str, Any]) -> Dict[str, Any]:
    """Prepare and clean a declared camp, returning an immutable-style receipt.

    A camp must be absent in both save owners at the declared absolute OMT.
    Any existing identity is unsafe: this avoids treating an idempotent fixture
    transform as evidence that a new controlled composition was created.
    """
    player_save_name = str(declaration.get("player_save", "")).strip()
    camp_name = str(declaration.get("camp_name", "")).strip()
    owner = str(declaration.get("owner", "your_followers")).strip()
    camp_omt = declaration.get("camp_omt")
    if not player_save_name or not camp_name or owner != "your_followers":
        raise ControlledCampSetupError("camp declaration needs player_save, camp_name, and owner your_followers")
    if not isinstance(camp_omt, list) or len(camp_omt) != 3:
        raise ControlledCampSetupError("camp declaration needs exact camp_omt=[x,y,z]")
    declared_omt = [int(value) for value in camp_omt]
    player_save = world_dir / player_save_name
    if not player_save.exists() or player_save.suffix != ".zzip":
        raise ControlledCampSetupError(f"player save is not a .zzip file: {player_save}")
    overmap_x, overmap_y, _local = overmap_file_coords_from_abs_omt(tuple(declared_omt))
    overmap_path = world_dir / "overmaps" / f"o.{overmap_x}.{overmap_y}.zzip"
    if not overmap_path.exists():
        raise ControlledCampSetupError(f"declared camp overmap is absent: {overmap_path}")

    before_player_omt, before_player_camps = _player_camps(player_save)
    before_overmap_camps = _overmap_camps(overmap_path)
    if before_player_omt != declared_omt:
        raise ControlledCampSetupError(
            f"unsafe placement: player OMT {before_player_omt} differs from declared camp OMT {declared_omt}"
        )
    if any(camp.get("pos") == declared_omt for camp in before_player_camps + before_overmap_camps):
        raise ControlledCampSetupError("unsafe placement: declared camp identity already exists")

    snapshots = {path: path.read_bytes() for path in (player_save, overmap_path)}
    before_player_payload = _player_payload(player_save)
    before_overmap_payload = _overmap_payload(overmap_path)
    native: Dict[str, Any] = {}
    after_player_omt: list[int] = []
    after_player_camps: list[dict[str, Any]] = []
    after_overmap_camps: list[dict[str, Any]] = []
    declared_mutation_only = False
    transaction_error = ""
    try:
        native = apply_player_basecamp_at_omt_transform(world_dir, {
            "kind": "player_basecamp_at_omt", "player_save": player_save_name,
            "camp_name": camp_name, "owner": owner,
        })
        after_player_omt, after_player_camps = _player_camps(player_save)
        after_overmap_camps = _overmap_camps(overmap_path)
        declared_mutation_only = (
            _without_declared_camp(
                _player_payload(player_save), declared_omt=declared_omt, player_owner=True,
            ) == _canonical_json(before_player_payload)
            and _without_declared_camp(
                _overmap_payload(overmap_path), declared_omt=declared_omt, player_owner=False,
            ) == _canonical_json(before_overmap_payload)
        )
    except Exception as exc:
        transaction_error = str(exc)
    finally:
        for path, contents in snapshots.items():
            path.write_bytes(contents)
    player_identity = {"pos": declared_omt}
    overmap_identity = {"owner": owner, "name": camp_name, "pos": declared_omt}
    invariant = (
        not transaction_error
        and after_player_omt == declared_omt
        and after_player_camps.count(player_identity) == 1
        and after_overmap_camps.count(overmap_identity) == 1
        and native.get("player_registry_present") is False
        and native.get("camp_added") is True
        and declared_mutation_only
    )
    restored_paths = sorted(str(path) for path in snapshots)
    restore_drift = [str(path) for path, contents in snapshots.items() if path.read_bytes() != contents]
    cleanup = {
        "owner": "controlled_camp_setup",
        "accepted": not restore_drift,
        "restored_paths": restored_paths,
    }
    if restore_drift:
        cleanup.update({"reason": "restore_byte_mismatch", "paths": restore_drift})
    return {
        "status": (
            "cleaned" if invariant and cleanup["accepted"] else
            "failed_invariant_cleaned" if not transaction_error and cleanup["accepted"] else
            "failed_transaction_cleaned" if cleanup["accepted"] else "failed_cleanup"
        ),
        "operation": "controlled_camp_setup",
        "arguments": {"camp_name": camp_name, "owner": owner, "camp_omt": declared_omt},
        "target": {"camp_omt": declared_omt, "player_save": player_save_name, "overmap": str(overmap_path.relative_to(world_dir))},
        "native_receipt": {
            "owner": "fixture_save_transform.player_basecamp_at_omt",
            "accepted": invariant,
            "receipt": native,
            **({"error": transaction_error} if transaction_error else {}),
        },
        "before_facts": {"player_omt": before_player_omt, "player_camps": before_player_camps, "overmap_camps": before_overmap_camps, "file_sha256": {str(path): _sha256(contents) for path, contents in snapshots.items()}},
        "after_facts": {"player_omt": after_player_omt, "player_camps": after_player_camps, "overmap_camps": after_overmap_camps, "invariant": "one player and one overmap identity at declared camp_omt with no undeclared owner mutation", "invariant_satisfied": invariant, "declared_mutation_only": declared_mutation_only, "file_sha256": {str(path): _sha256(path.read_bytes()) for path in snapshots}},
        "cleanup_receipt": cleanup,
        "evidence_effect": "none_for_manufactured_state",
        "gameplay_credit": False,
    }
