"""Current waiting input state, without a history of full response packets."""
from pathlib import Path
import json
import os


PREFIX = "openclaw_harness_semantic_step: "


def finish_plain_waiting(run_dir, report, *, cleanup_complete):
    """End an exploratory playtest without manufacturing a certification archive.

    Registry selection still authenticates launch. Reduced waiting records are
    deliberately not ingested as formal feature-proof reports.
    """
    cleanup = report.get("cleanup", {})
    lines = ["Playtest ended." if report.get("ok") else "Playtest ended with a failure."]
    for key in ("error", "reason", "verdict"):
        value = report.get(key)
        if isinstance(value, str) and value:
            lines.append(value.replace("_", " "))
    lines.append("Cleanup: " + str(cleanup.get("status", "unknown")).replace("_", " ") + ".")
    path = run_dir / "playtest-summary.txt"
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    retire_startup_records(run_dir)
    for name in ("probe.step_ledger.json", "proof.gates.json"):
        (run_dir / name).unlink(missing_ok=True)
    session = os.environ.get("OPENCLAW_COCKPIT_BRIDGE_SESSION_DIR")
    binding = os.environ.get("OPENCLAW_COCKPIT_BRIDGE_BINDING_ID")
    if session and binding and cleanup_complete:
        from process_performance import write_json
        write_json(Path(session) / "cockpit.bridge.safe_to_cleanup.json", {
            "schema": "caol-cockpit-scenario-terminalization-v1", "binding_id": binding,
            "state": "safe_to_cleanup", "report_path": str(path.resolve()),
            "cleanup": {key: cleanup[key] for key in ("status", "native_exit_credit") if key in cleanup},
        })


def retire_startup_records(directory):
    """After admission, startup-only projections no longer own any live state."""
    for name in ("startup.result.json", "startup.step_ledger.json", "contract.preflight.json",
                 "plan.json", "debug.final.log", "semantic.native.log", "semantic.native.full.log",
                 "semantic.native.full.log.ref.json"):
        (directory / name).unlink(missing_ok=True)


def activate_native_snapshot(path: Path):
    """Switch a paused, admitted World session from startup history to current state."""
    if Path(str(path) + ".plain").exists():
        return
    current = {}
    with path.open(encoding="utf-8") as stream:
        for line in stream:
            if line.startswith(PREFIX):
                value = json.loads(line[len(PREFIX):])
                if value.get("event") in {"frame", "surface_descriptor"}:
                    current[value["event"]] = {**value, "_source_offset": 0, "_source_end": 1}
    if "surface_descriptor" not in current:
        raise ValueError("Plain waiting requires an admitted native input surface")
    body = "".join(PREFIX + json.dumps(value, separators=(",", ":")) + "\n"
                   for value in current.values())
    Path(str(path) + ".seed").write_text(body, encoding="utf-8")
    temporary = Path(str(path) + ".activate")
    temporary.write_text(body, encoding="utf-8")
    os.replace(temporary, path)
    Path(str(path) + ".plain").touch()
    # These startup projections are superseded by the current native state.
    # No plain-mode reader uses them after activation.
    retire_startup_records(path.parent)


def collect_waiting_snapshot(directory, path, owner):
    from process_performance import read_json, write_json
    snapshot = read_json(Path(str(path) + ".performance"))
    if not snapshot:
        return {"status": "measuring", "alarms": [], "recoveries": []}
    if snapshot.get("run_id") != owner["run_id"]:
        return {"status": "unavailable", "reason": "waiting_measurement_run_mismatch"}
    state_path = directory / "waiting-performance-state.json"
    previous = read_json(state_path)
    if previous.get("owner") != owner:
        previous = {}
    alarms, recoveries = [], []
    if snapshot.get("alarm_count", 0) > previous.get("alarm_count", 0):
        alarms.append({"kind": "waiting_slow", "mean_seconds": snapshot["last_alarm_mean"],
                       "sample_count": 100, "limit_seconds": .010,
                       "message": "Tell the coordinator: waiting performance needs attention."})
    # A recovery followed by another slowdown can occur between collections.
    # Never describe that currently slow game as recovered.
    if (snapshot.get("recovery_count", 0) > previous.get("recovery_count", 0)
            and not snapshot.get("alarmed")):
        recoveries.append({"kind": "waiting_recovery"})
    write_json(state_path, {"owner": owner, "alarm_count": snapshot.get("alarm_count", 0),
                           "recovery_count": snapshot.get("recovery_count", 0)})
    return {"status": "measured" if snapshot.get("sample_count") == 100 else "measuring",
            "waiting": snapshot, "alarms": alarms, "recoveries": recoveries}


def compact_response(response):
    """Keep only what the waiting client needs to act or establish an outcome."""
    from gameplay_display import observation

    result = {key: response[key] for key in ("ok", "error", "reason", "action_outcome") if key in response}
    failure = response.get("failure")
    if isinstance(failure, dict):
        result["failure"] = {key: failure[key] for key in ("reason", "unused_authority") if key in failure}
        detail = failure.get("detail")
        if isinstance(detail, dict) and "action_outcome" in detail:
            result["failure"]["detail"] = {"action_outcome": detail["action_outcome"]}
    observed = observation(response)
    if observed:
        surface = observed["surface"]
        result["observation"] = {
            key: observed[key] for key in ("observation_id", "run_id", "game_minutes", "game_turn")
            if key in observed}
        result["observation"]["surface"] = {
            "kind": surface.get("kind"), "actions": surface.get("actions", []),
            "facts": {key: value for key, value in surface.get("facts", {}).items()
                      if key in {"text", "title", "messages", "activity_type"}},
        }
    terminal = response.get("result")
    if isinstance(terminal, dict) and terminal.get("schema") == "caol-cockpit-live-final-v1":
        result["result"] = {key: terminal[key] for key in ("schema", "state", "cleanup") if key in terminal}
    return result


def retire_collected_packets(session: Path, current_request: str):
    """An admitted successor proves the client has consumed its predecessor.

    Keep the latest response for crash recovery. This applies only to explicitly
    selected plain waiting sessions, whose client stores current input state.
    """
    # The next request may already be spooled once this receipt is published.
    # Only a completed predecessor's receipt authorizes retiring its packet.
    for receipt in (session / "responses").glob("*.receipt.json"):
        request_id = receipt.name.removesuffix(".receipt.json")
        if request_id != current_request:
            for path in (session / "requests" / (request_id + ".json"),
                         session / "responses" / (request_id + ".json"), receipt):
                path.unlink(missing_ok=True)
