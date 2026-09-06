"""Paged projections of an immutable scenario-query result; no registry mutations."""
from __future__ import annotations

from typing import Any, Mapping, Sequence
import json


def query_page(payload: Mapping[str, Any], receipt: Mapping[str, Any], *,
               offset: int, page_size: int, cli: Sequence[str],
               view: str = "matches", scenario_id: str | None = None) -> dict:
    if offset < 0 or page_size <= 0:
        raise ValueError("offset must be nonnegative and page size must be positive")
    result = payload["result"]
    stored = result["evaluation"]
    evaluation = stored["evaluation"]
    ranked = evaluation["ranked_scenario_ids"]
    snapshots = {item["scenario_id"]: item for item in stored["candidates"]}
    observations = {item["scenario_id"]: item for item in evaluation["candidates"]}
    ranks = {identity: rank for rank, identity in enumerate(ranked, start=1)}
    excluded = [identity for identity in snapshots if identity not in ranks]
    identities = ranked if view == "matches" else excluded
    if view not in ("matches", "excluded"):
        raise ValueError("view must be matches or excluded")
    if scenario_id is not None:
        if scenario_id not in snapshots:
            raise ValueError("scenario is absent from this saved query: " + scenario_id)
        identities = [scenario_id]
    rejection_groups = {}
    for identity in excluded:
        seen_causes = set()
        for failure in observations[identity]["hard_results"]:
            if failure.get("passed"):
                continue
            cause = {key: failure.get(key) for key in
                     ("key", "op", "expected", "reason", "evidence_state")}
            cause_key = json.dumps(cause, sort_keys=True)
            if cause_key in seen_causes:
                continue
            seen_causes.add(cause_key)
            group = rejection_groups.setdefault(cause_key,
                                                {**cause, "candidate_count": 0})
            group["candidate_count"] += 1
    candidates = []
    for identity in identities[offset:offset + page_size]:
        snapshot = snapshots[identity]
        explanation = snapshot["explanation"]
        manifest = explanation["manifest"]
        observed = observations[identity]
        candidates.append({
            "rank": ranks.get(identity),
            "scenario_id": identity, "name": manifest.get("name"),
            "revision": manifest.get("revision"), "manifest_sha256": manifest.get("sha256"),
            "source_path": manifest.get("source_path"),
            "lifecycle_state": snapshot["lifecycle_state"],
            "token_eligible": snapshot["token_eligible"],
            "matches": observed["hard_results"],
            "preferences": observed["preference_results"],
            "route_evidence_states": sorted({str(route.get("evidence_state", "unknown"))
                for route in explanation.get("route_evidence", [])}),
        })
        candidates[-1]["details_argv"] = [*cli, "registry-query-page", "--sha256",
            receipt["artifact"]["sha256"], "--scenario-id", identity]
        if scenario_id is not None:
            candidates[-1]["evidence"] = snapshot
    digest = receipt["artifact"]["sha256"]
    next_offset = offset + len(candidates)
    next_page = ([*cli, "registry-query-page", "--sha256", digest,
                  "--offset", str(next_offset), "--page-size", str(page_size), "--view", view]
                 if scenario_id is None and next_offset < len(identities) else None)
    readiness = result.get("source_executable_readiness", {})
    return {
        **dict(receipt),
        **{key: result.get(key) for key in
           ("query_id", "query_sha256", "selection_id", "token_id", "draft_path", "next_action")},
        "selected_scenario_id": ranked[0] if ranked else None,
        "source_executable_readiness": {key: readiness[key] for key in
            ("status", "reason", "evidence_ceiling", "executable_path", "build_entrypoint",
             "executable_sha256", "product_source_sha256", "product_build_receipt") if key in readiness},
        "candidates": candidates,
        "rejections": {"causes": list(rejection_groups.values()),
            "details_argv": [*cli, "registry-query-page", "--sha256", digest, "--view", "excluded"]},
        "page": {"view": view, "scenario_id": scenario_id,
                 "offset": offset, "page_size": page_size, "returned": len(candidates),
                 "total_matches": len(ranked),
                 "excluded_candidates": len(snapshots) - len(ranked), "next": next_page},
        "full_result": [*cli, "registry-query-artifact", "--sha256", digest],
        "authority": "Saved query snapshot; browsing does not reselect or issue authority. "
                     "The token belongs only to selected_scenario_id; launch revalidates current state.",
    }
