"""Fail-closed corridor evidence for the R-005 route-observation prerequisite."""

from __future__ import annotations

import re
import hashlib
import json
from typing import Any, Mapping, Sequence


_PATH_OMTS = re.compile(r'path_omts="([^"]*)"')
_DESTINATION = re.compile(r'destination=(-?\d+),(-?\d+),(-?\d+)')
_REQUESTED_START = re.compile(r'requested_start=(-?\d+),(-?\d+),(-?\d+)')
_REQUESTED_END = re.compile(r'requested_end=(-?\d+),(-?\d+),(-?\d+)')
_ACTUAL_FIRST = re.compile(r'actual_first=(-?\d+),(-?\d+),(-?\d+)')
_ACTUAL_TERMINAL = re.compile(r'actual_terminal=(-?\d+),(-?\d+),(-?\d+)')


def _omt_path(value: object) -> list[list[int]] | None:
    if not isinstance(value, list) or not value:
        return None
    result: list[list[int]] = []
    for omt in value:
        if not isinstance(omt, list) or len(omt) != 3 or any(
                isinstance(component, bool) or not isinstance(component, int)
                for component in omt):
            return None
        result.append(list(omt))
    return result


def _omt(value: object) -> list[int] | None:
    path = _omt_path([value])
    return path[0] if path is not None else None


def _native_path_from_origin(
    path: Sequence[Sequence[int]], origin: Sequence[int], destination: Sequence[int],
) -> list[list[int]] | None:
    """Normalize the native constructor's destination-to-source log encoding."""
    normalized = _omt_path(list(path))
    if normalized is None:
        return None
    if normalized[0] == list(origin) and normalized[-1] == list(destination):
        return normalized
    if normalized[0] == list(destination) and normalized[-1] == list(origin):
        return list(reversed(normalized))
    return None


def parse_native_planned_corridor(
    lines: Sequence[str], destination: Sequence[int], *,
    requested_start: Sequence[int] | None = None,
) -> dict[str, Any]:
    """Extract one production route-constructor corridor, never a waypoint plan."""
    if len(destination) != 3 or any(
            isinstance(component, bool) or not isinstance(component, int)
            for component in destination):
        return {"status": "blocked", "reason": "invalid_destination"}
    if requested_start is not None:
        if isinstance(requested_start, (str, bytes)):
            return {"status": "blocked", "reason": "invalid_requested_start"}
        try:
            expected_start = _omt(list(requested_start))
        except TypeError:
            expected_start = None
        if expected_start is None:
            return {"status": "blocked", "reason": "invalid_requested_start"}
    else:
        expected_start = None
    matches: list[tuple[list[list[int]], dict[str, list[int]]]] = []
    expected_destination = tuple(destination)
    for line in lines:
        if "component=overmap_route event=constructed" not in line:
            continue
        destination_match = _DESTINATION.search(line)
        path_match = _PATH_OMTS.search(line)
        if destination_match is None or path_match is None:
            continue
        observed_destination = tuple(int(item) for item in destination_match.groups())
        if observed_destination != expected_destination:
            continue
        native_request = {
            "requested_start": _REQUESTED_START.search(line),
            "requested_end": _REQUESTED_END.search(line),
            "actual_first": _ACTUAL_FIRST.search(line),
            "actual_terminal": _ACTUAL_TERMINAL.search(line),
        }
        if expected_start is not None:
            if "native_preview_request=true" not in line or "world_mutation=false" not in line or \
                    any(value is None for value in native_request.values()):
                continue
            request_points = {
                key: [int(item) for item in value.groups()]
                for key, value in native_request.items() if value is not None
            }
            if request_points["requested_start"] != expected_start or \
                    request_points["requested_end"] != list(destination):
                continue
        else:
            request_points = {}
        corridor: list[list[int]] = []
        try:
            for raw_omt in path_match.group(1).split(";"):
                corridor.append([int(item) for item in raw_omt.split(",")])
        except ValueError:
            return {"status": "blocked", "reason": "malformed_native_corridor"}
        if _omt_path(corridor) is None:
            return {"status": "blocked", "reason": "malformed_native_corridor"}
        if expected_start is not None and (
                request_points["actual_first"] != corridor[0] or
                request_points["actual_terminal"] != corridor[-1] or
                _native_path_from_origin(corridor, expected_start, destination) is None):
            return {"status": "blocked", "reason": "native_preview_receipt_endpoint_mismatch"}
        matches.append((corridor, request_points))
    if not matches:
        return {
            "status": "blocked",
            "reason": "missing_native_segment_preview_request"
            if expected_start is not None else "missing_native_corridor",
        }
    if len(matches) != 1:
        return {"status": "blocked", "reason": "ambiguous_native_corridor"}
    return {
        "status": "green",
        "destination": list(destination),
        "planned_corridor": matches[0][0],
        "source": "native_overmap_route_constructor",
        **({
            "requested_start": expected_start,
            "requested_end": list(destination),
            "actual_first": matches[0][1]["actual_first"],
            "actual_terminal": matches[0][1]["actual_terminal"],
            "world_mutation": False,
        } if expected_start is not None else {}),
    }


def select_r005_corridor_candidate(
    candidate: Mapping[str, Any], failed_corridors: Sequence[Mapping[str, Any]], *,
    current_binding_id: str, expected_start_omt: Sequence[int],
    expected_destination_omt: Sequence[int],
) -> dict[str, Any]:
    """Select a bound corridor without reusing a hostile failed corridor cell."""
    if not current_binding_id:
        return {"status": "blocked", "reason": "missing_current_binding"}
    if isinstance(expected_destination_omt, (str, bytes)):
        return {"status": "blocked", "reason": "invalid_expected_destination"}
    try:
        expected_destination = _omt(list(expected_destination_omt))
    except TypeError:
        return {"status": "blocked", "reason": "invalid_expected_destination"}
    if expected_destination is None:
        return {"status": "blocked", "reason": "invalid_expected_destination"}
    if isinstance(expected_start_omt, (str, bytes)):
        return {"status": "blocked", "reason": "invalid_expected_start"}
    try:
        expected_start = _omt(list(expected_start_omt))
    except TypeError:
        return {"status": "blocked", "reason": "invalid_expected_start"}
    if expected_start is None:
        return {"status": "blocked", "reason": "invalid_expected_start"}
    candidate_path = _omt_path(candidate.get("planned_corridor"))
    if candidate_path is None:
        return {"status": "blocked", "reason": "missing_candidate_corridor"}
    candidate_destination = _omt(candidate.get("destination_omt"))
    candidate_start = _omt(candidate.get("start_omt"))
    if candidate_destination != expected_destination or candidate_start != expected_start or \
            candidate_path[0] != expected_destination or candidate_path[-1] != expected_start:
        return {
            "status": "blocked",
            "reason": "candidate_corridor_not_bound_to_required_endpoints",
        }
    if str(candidate.get("binding_id", "")) != current_binding_id or not candidate.get("receipt_sha256"):
        return {"status": "blocked", "reason": "unbound_candidate_corridor"}

    candidate_points = {tuple(omt) for omt in candidate_path}
    validated_evidence: list[tuple[str, list[list[int]], tuple[int, int, int]]] = []
    compared_reports: list[str] = []
    for evidence in failed_corridors:
        report_id = str(evidence.get("report_id", ""))
        failed_path = _omt_path(evidence.get("planned_corridor"))
        boundary = evidence.get("first_hostile_boundary")
        if not report_id or failed_path is None or not isinstance(boundary, Mapping):
            return {"status": "blocked", "reason": "missing_failed_corridor_evidence", "report_id": report_id}
        if str(evidence.get("binding_id", "")) != current_binding_id:
            return {"status": "blocked", "reason": "stale_failed_corridor_evidence", "report_id": report_id}
        if not evidence.get("receipt_sha256") or evidence.get("ingestion") != "red_ingested" or \
                evidence.get("cleanup") != "accepted":
            return {"status": "blocked", "reason": "unbound_failed_corridor_evidence", "report_id": report_id}
        if boundary.get("prompt_response") != "none":
            return {"status": "blocked", "reason": "failed_prompt_was_answered", "report_id": report_id}
        evidence_scope = evidence.get("corridor_scope", "required_route")
        if evidence_scope == "required_route":
            failed_destination = _omt(evidence.get("destination_omt"))
            failed_start = _omt(evidence.get("start_omt"))
            if failed_destination != expected_destination or failed_start != expected_start or \
                    failed_path[0] != expected_destination or failed_path[-1] != expected_start:
                return {
                    "status": "blocked",
                    "reason": "failed_corridor_not_bound_to_required_endpoints",
                    "report_id": report_id,
                }
        elif evidence_scope == "qualification_leg":
            leg_destination = _omt(evidence.get("leg_destination_omt"))
            leg_start = _omt(evidence.get("leg_start_omt"))
            if leg_destination is None or leg_start is None or \
                    failed_path[0] != leg_destination or failed_path[-1] != leg_start:
                return {
                    "status": "blocked",
                    "reason": "failed_qualification_leg_not_bound_to_declared_endpoints",
                    "report_id": report_id,
                }
        else:
            return {
                "status": "blocked",
                "reason": "unknown_failed_corridor_scope",
                "report_id": report_id,
            }
        boundary_omt = boundary.get("omt")
        if not isinstance(boundary_omt, list) or len(boundary_omt) != 3 or \
                tuple(boundary_omt) not in {tuple(omt) for omt in failed_path}:
            return {"status": "blocked", "reason": "hostile_boundary_not_on_corridor", "report_id": report_id}
        boundary_point = tuple(boundary_omt)

        validated_evidence.append((report_id, failed_path, boundary_point))

    for report_id, failed_path, boundary_point in validated_evidence:
        if boundary_point in candidate_points:
            return {
                "status": "blocked",
                "reason": "candidate_corridor_overlaps_hostile_boundary",
                "report_id": report_id,
                "overlap_omts": [list(boundary_point)],
            }

    for report_id, failed_path, _boundary_point in validated_evidence:
        required_endpoints = {tuple(expected_destination), tuple(expected_start)}
        overlap = sorted(
            candidate_points.intersection(tuple(omt) for omt in failed_path) - required_endpoints
        )
        if overlap:
            return {
                "status": "blocked",
                "reason": "candidate_corridor_overlaps_preserved_failure",
                "report_id": report_id,
                "overlap_omts": [list(omt) for omt in overlap],
            }
        compared_reports.append(report_id)
    return {
        "status": "selected_zero_credit_only",
        "candidate_id": str(candidate.get("candidate_id", "")),
        "compared_reports": compared_reports,
        "destination_omt": expected_destination,
        "planned_corridor": candidate_path,
        "qualification_authority": "not_started",
    }


def plan_r005_native_waypoint_segments(
    failed_corridors: Sequence[Mapping[str, Any]], *,
    current_binding_id: str, expected_start_omt: Sequence[int],
    expected_destination_omt: Sequence[int],
) -> dict[str, Any]:
    """Derive a zero-credit native waypoint plan from five bound hostile boundaries.

    This function deliberately plans only observation waypoints.  It neither dispatches
    movement nor infers a traversable corridor: each resulting segment must still be
    constructed by the production overmap router and passed to
    ``compose_r005_native_waypoint_segments``.
    """
    if len(failed_corridors) != 5:
        return {
            "status": "blocked",
            "reason": "five_current_bound_hostile_boundaries_required",
        }
    if not current_binding_id:
        return {"status": "blocked", "reason": "missing_current_binding"}
    if isinstance(expected_start_omt, (str, bytes)) or isinstance(expected_destination_omt, (str, bytes)):
        return {"status": "blocked", "reason": "invalid_required_endpoints"}
    try:
        expected_start = _omt(list(expected_start_omt))
        expected_destination = _omt(list(expected_destination_omt))
    except TypeError:
        return {"status": "blocked", "reason": "invalid_required_endpoints"}
    if expected_start is None or expected_destination is None:
        return {"status": "blocked", "reason": "invalid_required_endpoints"}

    boundaries: list[dict[str, Any]] = []
    for evidence in failed_corridors:
        report_id = str(evidence.get("report_id", ""))
        failed_path = _omt_path(evidence.get("planned_corridor"))
        boundary = evidence.get("first_hostile_boundary")
        if not report_id or failed_path is None or not isinstance(boundary, Mapping):
            return {"status": "blocked", "reason": "missing_failed_corridor_evidence", "report_id": report_id}
        if str(evidence.get("binding_id", "")) != current_binding_id:
            return {"status": "blocked", "reason": "stale_failed_corridor_evidence", "report_id": report_id}
        if not evidence.get("receipt_sha256") or evidence.get("ingestion") != "red_ingested" or \
                evidence.get("cleanup") != "accepted":
            return {"status": "blocked", "reason": "unbound_failed_corridor_evidence", "report_id": report_id}
        if boundary.get("prompt_response") != "none":
            return {"status": "blocked", "reason": "failed_prompt_was_answered", "report_id": report_id}
        boundary_omt = _omt(boundary.get("omt"))
        if boundary_omt is None or tuple(boundary_omt) not in {tuple(omt) for omt in failed_path}:
            return {"status": "blocked", "reason": "hostile_boundary_not_on_corridor", "report_id": report_id}
        boundaries.append({"report_id": report_id, "omt": boundary_omt})

    # A flank outside the western-most prohibited boundary is mechanically derived
    # from the current evidence.  It is a waypoint proposal, not a claim that the
    # native router will use that flank or that it is safe to travel.
    flank_x = min([expected_start[0], expected_destination[0]] + [
        item["omt"][0] for item in boundaries
    ]) - 1
    if flank_x == expected_start[0] or flank_x == expected_destination[0]:
        return {"status": "blocked", "reason": "waypoint_flank_not_distinct"}
    waypoints = [
        expected_start,
        [flank_x, expected_start[1], expected_start[2]],
        [flank_x, expected_destination[1], expected_destination[2]],
        expected_destination,
    ]
    prohibited = {tuple(item["omt"]) for item in boundaries}
    for waypoint in waypoints:
        if tuple(waypoint) in prohibited:
            return {"status": "blocked", "reason": "waypoint_is_prohibited_hostile_cell"}
    return {
        "status": "planned_zero_credit_only",
        "current_binding_id": current_binding_id,
        "start_omt": expected_start,
        "destination_omt": expected_destination,
        "hostile_boundaries": boundaries,
        "waypoints": waypoints,
        "segments": [
            {
                "segment_id": f"native_waypoint_{index + 1}",
                "origin_omt": waypoints[index],
                "destination_omt": waypoints[index + 1],
            }
            for index in range(len(waypoints) - 1)
        ],
        "movement_dispatched": False,
        "qualification_authority": "not_started",
    }


def compose_r005_native_waypoint_segments(
    waypoint_plan: Mapping[str, Any], native_segments: Sequence[Mapping[str, Any]],
    failed_corridors: Sequence[Mapping[str, Any]], *, current_binding_id: str,
) -> dict[str, Any]:
    """Fail closed unless bound native segment previews compose to the destination."""
    if waypoint_plan.get("status") != "planned_zero_credit_only":
        return {"status": "blocked", "reason": "waypoint_plan_not_ready"}
    if not current_binding_id or waypoint_plan.get("current_binding_id") != current_binding_id:
        return {"status": "blocked", "reason": "waypoint_plan_binding_not_current"}
    waypoints = _omt_path(waypoint_plan.get("waypoints"))
    planned_segments = waypoint_plan.get("segments")
    if waypoints is None or not isinstance(planned_segments, list) or len(waypoints) != len(planned_segments) + 1:
        return {"status": "blocked", "reason": "invalid_waypoint_plan"}
    if len(native_segments) != len(planned_segments):
        return {"status": "blocked", "reason": "native_segment_count_mismatch"}

    prohibited: set[tuple[int, int, int]] = set()
    failed_interiors: set[tuple[int, int, int]] = set()
    for evidence in failed_corridors:
        if str(evidence.get("binding_id", "")) != current_binding_id or not evidence.get("receipt_sha256") or \
                evidence.get("ingestion") != "red_ingested" or evidence.get("cleanup") != "accepted":
            return {"status": "blocked", "reason": "unbound_failed_corridor_evidence"}
        failed_path = _omt_path(evidence.get("planned_corridor"))
        boundary = evidence.get("first_hostile_boundary")
        boundary_omt = _omt(boundary.get("omt")) if isinstance(boundary, Mapping) else None
        if failed_path is None or boundary_omt is None or tuple(boundary_omt) not in {tuple(omt) for omt in failed_path}:
            return {"status": "blocked", "reason": "hostile_boundary_not_on_corridor"}
        prohibited.add(tuple(boundary_omt))
        failed_interiors.update(tuple(omt) for omt in failed_path[1:-1])

    composed: list[list[int]] = []
    receipt_parts: list[dict[str, Any]] = []
    for index, (planned, observed) in enumerate(zip(planned_segments, native_segments)):
        if not isinstance(planned, Mapping):
            return {"status": "blocked", "reason": "invalid_waypoint_plan"}
        expected_origin = waypoints[index]
        expected_destination = waypoints[index + 1]
        if planned.get("origin_omt") != expected_origin or planned.get("destination_omt") != expected_destination:
            return {"status": "blocked", "reason": "waypoint_segment_not_bound_to_plan", "segment_index": index}
        if str(observed.get("binding_id", "")) != current_binding_id or not observed.get("receipt_sha256"):
            return {"status": "blocked", "reason": "native_segment_not_currently_bound", "segment_index": index}
        receipt = observed.get("native_preview_receipt")
        if not isinstance(receipt, Mapping):
            return {"status": "blocked", "reason": "native_segment_receipt_missing", "segment_index": index}
        if any(not receipt.get(key) for key in ("run", "scenario", "source", "executable")) or \
                receipt.get("world_mutation") is not False:
            return {"status": "blocked", "reason": "native_segment_receipt_not_bound", "segment_index": index}
        raw_path = _omt_path(observed.get("planned_corridor"))
        if raw_path is None or receipt.get("requested_start") != expected_origin or \
                receipt.get("requested_end") != expected_destination or \
                receipt.get("actual_first") != raw_path[0] or \
                receipt.get("actual_terminal") != raw_path[-1] or \
                receipt.get("exact_native_corridor") != raw_path:
            return {"status": "blocked", "reason": "native_segment_receipt_endpoint_mismatch", "segment_index": index}
        native_path = _native_path_from_origin(
            raw_path, expected_origin, expected_destination,
        )
        if native_path is None:
            return {"status": "blocked", "reason": "native_segment_did_not_reach_declared_waypoint", "segment_index": index}
        path_points = {tuple(omt) for omt in native_path}
        overlap = sorted(path_points.intersection(prohibited))
        if overlap:
            return {
                "status": "blocked", "reason": "native_segment_contains_prohibited_hostile_cell",
                "segment_index": index, "overlap_omts": [list(omt) for omt in overlap],
            }
        interior_overlap = sorted(path_points.intersection(failed_interiors))
        if interior_overlap:
            return {
                "status": "blocked", "reason": "native_segment_overlaps_failed_interior_corridor",
                "segment_index": index, "overlap_omts": [list(omt) for omt in interior_overlap],
            }
        if composed and composed[-1] != native_path[0]:
            return {"status": "blocked", "reason": "native_segments_do_not_compose", "segment_index": index}
        composed.extend(native_path if not composed else native_path[1:])
        receipt_parts.append({
            "segment_id": planned.get("segment_id", ""),
            "receipt_sha256": str(observed["receipt_sha256"]),
        })
    if not composed or composed[0] != waypoints[0] or composed[-1] != waypoints[-1]:
        return {"status": "blocked", "reason": "native_segments_do_not_compose_to_destination"}

    candidate = {
        "candidate_id": "native_waypoint_composed_corridor",
        "binding_id": current_binding_id,
        "receipt_sha256": hashlib.sha256(
            json.dumps(receipt_parts, sort_keys=True, separators=(",", ":")).encode("utf-8")
        ).hexdigest(),
        "start_omt": waypoints[0],
        "destination_omt": waypoints[-1],
        # The existing selector's preserved convention is destination-to-start.
        "planned_corridor": list(reversed(composed)),
    }
    selection = select_r005_corridor_candidate(
        candidate, failed_corridors, current_binding_id=current_binding_id,
        expected_start_omt=waypoints[0], expected_destination_omt=waypoints[-1],
    )
    if selection.get("status") != "selected_zero_credit_only":
        return {"status": "blocked", "reason": "composed_corridor_not_selectable", "selection": selection}
    return {
        "status": "selected_zero_credit_only",
        "waypoints": waypoints,
        "native_corridor": composed,
        "selector_candidate": candidate,
        "selection": selection,
        "movement_dispatched": False,
        "qualification_authority": "not_started",
    }


def derive_r005_native_waypoint_segments_from_previews(
    waypoint_plan: Mapping[str, Any], native_previews: Sequence[Mapping[str, Any]], *,
    current_binding_id: str,
) -> dict[str, Any]:
    """Reject deprecated avatar-origin preview slicing.

    A suffix of an avatar-origin preview is not a native segment request from its
    preceding waypoint.  Callers must submit one explicit-start receipt per
    segment to ``compose_r005_native_waypoint_segments`` instead.
    """
    del waypoint_plan, native_previews, current_binding_id
    return {
        "status": "blocked",
        "reason": "avatar_origin_preview_cannot_bind_segment_start",
    }
