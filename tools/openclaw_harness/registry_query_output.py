"""Paged projections of an immutable scenario-query result; no registry mutations."""
from __future__ import annotations

from typing import Any, Mapping, Sequence


def query_page(payload: Mapping[str, Any], receipt: Mapping[str, Any], *,
               offset: int, page_size: int, cli: Sequence[str]) -> dict:
    if offset < 0 or page_size <= 0:
        raise ValueError("offset must be nonnegative and page size must be positive")
    result = payload["result"]
    stored = result["evaluation"]
    evaluation = stored["evaluation"]
    ranked = evaluation["ranked_scenario_ids"]
    snapshots = {item["scenario_id"]: item for item in stored["candidates"]}
    observations = {item["scenario_id"]: item for item in evaluation["candidates"]}
    candidates = []
    for rank, identity in enumerate(ranked[offset:offset + page_size], start=offset + 1):
        snapshot = snapshots[identity]
        explanation = snapshot["explanation"]
        manifest = explanation["manifest"]
        observed = observations[identity]
        candidates.append({
            "rank": rank, "scenario_id": identity, "name": manifest.get("name"),
            "revision": manifest.get("revision"), "manifest_sha256": manifest.get("sha256"),
            "source_path": manifest.get("source_path"),
            "lifecycle_state": snapshot["lifecycle_state"],
            "token_eligible": snapshot["token_eligible"],
            "matches": observed["hard_results"],
            "preferences": observed["preference_results"],
            "route_evidence_states": sorted({str(route.get("evidence_state", "unknown"))
                for route in explanation.get("route_evidence", [])}),
        })
    digest = receipt["artifact"]["sha256"]
    next_offset = offset + len(candidates)
    next_page = ([*cli, "registry-query-page", "--sha256", digest,
                  "--offset", str(next_offset), "--page-size", str(page_size)]
                 if next_offset < len(ranked) else None)
    readiness = result.get("source_executable_readiness", {})
    return {
        **dict(receipt),
        **{key: result.get(key) for key in
           ("query_id", "query_sha256", "selection_id", "token_id", "draft_path", "next_action")},
        "selected_scenario_id": ranked[0] if ranked else None,
        "source_executable_readiness": {key: readiness[key] for key in
            ("status", "reason", "evidence_ceiling", "executable_path") if key in readiness},
        "candidates": candidates,
        "page": {"offset": offset, "page_size": page_size, "returned": len(candidates),
                 "total_matches": len(ranked),
                 "excluded_candidates": len(snapshots) - len(ranked), "next": next_page},
        "full_result": [*cli, "registry-query-artifact", "--sha256", digest],
        "authority": "Saved query snapshot; browsing does not reselect or issue authority. "
                     "The token belongs only to selected_scenario_id; launch revalidates current state.",
    }
