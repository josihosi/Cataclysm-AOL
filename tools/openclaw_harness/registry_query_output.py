"""Paged projections of an immutable scenario-query result; no registry mutations."""
from __future__ import annotations

from typing import Any, Mapping, Sequence
import json
from pathlib import Path


def _staffed_camp_signal_leads_observation(steps: Sequence[Mapping[str, Any]]) -> dict | None:
    """Project the native read-only lead payload at the before/after boundary."""
    observations = []
    for step in steps:
        if not isinstance(step, Mapping):
            continue
        # A collected action records both sides; prefer its post-action frame,
        # while retaining the first current frame as the explicit baseline.
        frames = [step.get("current_frame"), step.get("next_frame")]
        for frame in frames:
            if not isinstance(frame, Mapping) or frame.get("kind") != "world":
                continue
            payload = frame.get("payload")
            value = payload.get("staffed_camp_signal_leads") if isinstance(payload, Mapping) else None
            if isinstance(value, str):
                try:
                    value = json.loads(value)
                except json.JSONDecodeError:
                    continue
            if not isinstance(value, Mapping):
                continue
            observations.append({
                "frame_id": frame.get("frame_id"),
                "game_minutes": frame.get("game_minutes"),
                "game_turn": frame.get("game_turn"),
                "payload": dict(value),
            })
    if not observations:
        return None
    # Collapse repeated current/next copies, preserving exact first/last data.
    distinct = []
    for observation in observations:
        if not distinct or observation["payload"] != distinct[-1]["payload"]:
            distinct.append(observation)
    return {
        "schema": "caol-staffed-camp-signal-leads-evidence-v1",
        "initial": distinct[0],
        "latest": distinct[-1],
        "distinct_observations": len(distinct),
        "source": "native_world_surface_payload.staffed_camp_signal_leads",
    }


def _run_observation(report: Mapping[str, Any], *, run_id: str,
                     receipt_id: str | None = None) -> dict:
    """Project one run receipt into a compact, explicitly bounded observation.

    Manifest declarations and native run observations intentionally remain in
    separate objects.  This is a presentation helper only: it never promotes
    a declared capability to a run fact.
    """
    expected = str(run_id).strip()
    if not expected:
        raise ValueError("run identity must be nonempty")
    steps = report.get("_semantic_steps", report.get("steps", []))
    if not isinstance(steps, list):
        steps = []
    staffed_camp_signal_leads = _staffed_camp_signal_leads_observation(steps)
    pay_receipts = []
    for step in steps:
        if not isinstance(step, Mapping):
            continue
        if step.get("action_id") == "shakedown.pay" and step.get("accepted") is True:
            frame = step.get("current_frame")
            if not isinstance(frame, Mapping):
                frame = {}
            pay_receipts.append({key: step.get(key) for key in
                                 ("action_id", "run_id")}
                                | {"game_minutes": frame.get("game_minutes", step.get("game_minutes")),
                                   "game_turn": frame.get("game_turn", step.get("game_turn"))})
    artifact_lines = []
    artifacts = report.get("artifacts", {})
    if isinstance(artifacts, Mapping):
        for group in artifacts.get("matches_by_pattern", []):
            if isinstance(group, Mapping):
                artifact_lines.extend(str(line) for line in group.get("lines", []))
    joined = "\n".join(artifact_lines)
    actor_ids = sorted({int(match) for match in __import__("re").findall(r"\bnpc=(\d+)\b", joined)})
    forced_fight = "shakedown_fight_advance" in joined
    trade_owner = any("shakedown_trade_ui opened" in line for line in artifact_lines)
    source_binding = {}
    binding = report.get("_runtime_binding", report.get("runtime", report.get("runtime_binding")))
    if isinstance(binding, Mapping):
        source_binding = dict(binding)
    proof = report.get("proof_classification", {})
    verdict = proof.get("verdict") if isinstance(proof, Mapping) else None
    declared = report.get("scenario_manifest", {})
    declaration = declared.get("normalized", declared) if isinstance(declared, Mapping) else {}
    manifest_source = declared.get("source", {}) if isinstance(declared, Mapping) else {}
    if isinstance(manifest_source, Mapping):
        source_binding = {"runtime": source_binding, "manifest": dict(manifest_source)}
    missing = []
    for field, present in (("trade_owner", trade_owner),
                           ("trade_ui_open", trade_owner),
                           ("payment_completed", any("result=paid" in line for line in artifact_lines)),
                           ("paid_departure", any("return=physical" in line for line in artifact_lines))):
        if not present:
            missing.append(field)
    return {
        "identity": {"run_id": expected, "receipt_id": receipt_id},
        "accepted_pay": {"accepted": bool(pay_receipts), "receipts": pay_receipts},
        "trade_owner": {"present": trade_owner, "status": "observed" if trade_owner else "missing"},
        "forced_fight": forced_fight,
        "actors": actor_ids,
        "source_binding": source_binding,
        "verdict": verdict or report.get("verdict"),
        "proof_depth": "interaction" if pay_receipts else "startup/load-or-inconclusive",
        "missing_fields": missing,
        "manifest_declarations": declaration,
        "run_observations": {
            "artifact_patterns": artifact_lines,
            "actor_ids_source": "run artifact lines",
            **({"staffed_camp_signal_leads": staffed_camp_signal_leads}
               if staffed_camp_signal_leads is not None else {}),
        },
    }


def load_run_report(repo_root: Path, run_id: str) -> Mapping[str, Any]:
    """Find a retained probe report by exact run identity, without launching."""
    run = str(run_id).strip()
    matches = sorted(repo_root.glob(f".userdata/*/harness_runs/{run}/probe.report.json"))
    if len(matches) != 1:
        raise ValueError(f"expected one retained run report for {run}, found {len(matches)}")
    report_path = matches[0]
    value = json.loads(report_path.read_text(encoding="utf-8"))
    if not isinstance(value, Mapping):
        raise ValueError("run report must contain an object")
    result = dict(value)
    semantic_path = report_path.parent / "semantic.steps.jsonl"
    if semantic_path.exists():
        result["_semantic_steps"] = [json.loads(line) for line in
                                      semantic_path.read_text(encoding="utf-8").splitlines() if line.strip()]
    binding_path = report_path.parent / "runtime.binding.json"
    if binding_path.exists():
        binding = json.loads(binding_path.read_text(encoding="utf-8"))
        if isinstance(binding, Mapping):
            result["_runtime_binding"] = dict(binding)
    return result


def query_page(payload: Mapping[str, Any], receipt: Mapping[str, Any], *,
               offset: int, page_size: int, cli: Sequence[str],
               view: str = "matches", scenario_id: str | None = None,
               run_report: Mapping[str, Any] | None = None,
               run_id: str | None = None, receipt_id: str | None = None) -> dict:
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
    projected = _run_observation(run_report, run_id=run_id, receipt_id=receipt_id) \
        if run_report is not None and run_id is not None else None
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
        **({"run_evidence": projected} if projected is not None else {}),
        "authority": "Saved query snapshot; browsing does not reselect or issue authority. "
                     "The token belongs only to selected_scenario_id; launch revalidates current state.",
    }
