#!/usr/bin/env python3
"""Exercise real Sol/named-worker decisions in a disposable evidence-only workspace.

Prepare/inspect are portable and do not call a model. Run uses the candidate Unix
App Server transport and real worker-library commands. It stages a mutator source
correction through real restart state; it does not claim to test mutation review.
All model output, failed stages and raw usage remain available for review.
"""
from __future__ import annotations

import argparse
from collections import Counter
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import sqlite3
import subprocess
import sys
import uuid


ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / "scripts"
FIXTURE = ROOT / "tests/fixtures/worker_library_scenarios.json"
MARKER = "worker-library-simulation.json"
sys.path.insert(0, str(SCRIPTS))


def read_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(path: Path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def digest(path: Path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fixture_path(workspace: Path, relative: str) -> Path:
    path = (workspace / relative).resolve()
    if not path.is_relative_to(workspace.resolve()):
        raise ValueError("Fixture path escapes its workspace: " + relative)
    return path


def manifest(workspace: Path):
    value = read_json(workspace / MARKER)
    if value.get("schema") != "de67.worker-library-simulation-run.v1":
        raise ValueError("Workspace is not an initialized worker-library simulation")
    if str(workspace.resolve()) != value["workspace"]:
        raise ValueError("Simulation workspace identity changed; prepare a fresh workspace")
    return value


def scenario_fixture(workspace: Path):
    value = manifest(workspace)
    if digest(FIXTURE) != value["fixture_sha256"]:
        raise ValueError("Scenario fixture changed after preparation; prepare a fresh simulation")
    # Future inputs stay beside this harness, outside the worker workspace.
    return select_fixture(read_json(FIXTURE), value.get("from_stage"))


def select_fixture(fixture: dict, first_stage: str | None):
    stages = fixture["stages"]
    if first_stage is not None:
        ids = [stage["id"] for stage in stages]
        if first_stage not in ids:
            raise ValueError("Unknown --from stage: " + first_stage)
        stages = stages[ids.index(first_stage):]
    claims = {task["claim"] for stage in stages for task in stage["tasks"]}
    return {**fixture, "stages": stages,
            "claims": {key: value for key, value in fixture["claims"].items() if key in claims}}


def prepare(workspace: Path, first_stage: str | None = None):
    if shutil.which("git") is None:
        raise ValueError("Preparation requires Git for the representative disposable repository baseline")
    if workspace.exists() and any(workspace.iterdir()):
        raise ValueError("Prepare requires an absent or empty disposable workspace")
    fixture = select_fixture(read_json(FIXTURE), first_stage)
    workspace.mkdir(parents=True, exist_ok=True)
    for stage in fixture["stages"]:
        for name in stage["publish"]:
            fixture_path(workspace, name)
            if name not in fixture["documents"]:
                raise ValueError("Missing source document: " + name)
        for task in stage["tasks"]:
            if task["claim"] not in fixture["claims"]:
                raise ValueError("Unknown claim: " + task["claim"])
            for name in task["artifacts"]:
                fixture_path(workspace, name)
    de67 = workspace / ".de67"
    (de67 / "state").mkdir(parents=True)
    (workspace / "outputs").mkdir()
    (workspace / "protocol").mkdir()
    guidance = (
        "# Disposable evidence-interpretation simulation\n\n"
        "Work only in this workspace. Source excerpts are dated test inputs, not access "
        "instructions or current production authority. Do not use SSH, contact any live "
        "coordinator/mutator, run a game, access a production worktree, or repair product code. "
        "Read the supplied evidence; write interpretation artifacts under outputs/ and use "
        "the candidate DE67 tools for coordination state under .de67/.\n\n"
        "The current stage defines the complete authorized work. Future stages are not "
        "yet assignments. Preserve independent evidence ceilings and distinguish observations "
        "from inferences. Do not manufacture successful model or worker events.\n"
    )
    (workspace / "AGENTS.md").write_text(guidance, encoding="utf-8")
    (workspace / ".gitignore").write_text(
        "/.de67/state/\n/worker-library-simulation.json\n/protocol/\n",
        encoding="utf-8")
    (de67 / "WEC.md").write_text(
        "# WEC\n\nUse named workers and selected context to complete the current bounded "
        "evidence-interpretation assignments. Preserve useful worker continuity across "
        "a fresh coordinator restart and explicitly correct superseded premises. "
        "No gameplay or product edits.\n",
        encoding="utf-8",
    )
    fs = ["# Simulation functional contract\n"]
    for claim, outcome in fixture["claims"].items():
        fs.append(f"<!-- DE67:DFS-SLICE:BEGIN id={claim}-S001 claim={claim} -->\n"
                  f"- [ ] 🔴 {claim} — {outcome}\n"
                  "Produce only the current stage's assigned analysis artifact and honest "
                  "evidence limits. No native game execution or product-source edits.\n"
                  f"<!-- DE67:DFS-SLICE:END id={claim}-S001 claim={claim} -->\n")
    (de67 / "FS.md").write_text("\n".join(fs), encoding="utf-8")
    (de67 / "work-ledger.md").write_text("# Current simulation assignments\n", encoding="utf-8")
    for name in ("phase3-policy.d67", "phase3-policy.json", "phase3-contracts.json"):
        shutil.copyfile(ROOT / "assets/environment" / name, de67 / name)
    write_json(de67 / "state/workspace.json", {
        "agent_transport": "app-server", "agent_transport_python": sys.executable,
        "guidance": {"source": str(workspace / "AGENTS.md"),
                     "sha256": digest(workspace / "AGENTS.md"), "effective": True},
    })
    value = {
        "schema": "de67.worker-library-simulation-run.v1", "workspace": str(workspace),
        "created_at": datetime.now(timezone.utc).isoformat(),
        "lineage": "worker-library-sim-" + uuid.uuid4().hex,
        "fixture_sha256": digest(FIXTURE), "candidate_scripts": str(SCRIPTS),
        "from_stage": fixture["stages"][0]["id"],
        "selected_stages": [stage["id"] for stage in fixture["stages"]],
        "stages": [], "evidence_class": fixture["evidence_class"],
    }
    write_json(workspace / MARKER, value)
    subprocess.run(["git", "init", "--quiet", "--initial-branch=codex/worker-library-simulation"],
                   cwd=workspace, check=True, capture_output=True, text=True)
    subprocess.run(["git", "add", "--", "AGENTS.md", ".gitignore", ".de67/WEC.md", ".de67/FS.md",
                    ".de67/FS.md", ".de67/work-ledger.md", ".de67/phase3-policy.d67",
                    ".de67/phase3-policy.json", ".de67/phase3-contracts.json"],
                   cwd=workspace, check=True, capture_output=True, text=True)
    subprocess.run(["git", "-c", "user.name=DE67 Simulation", "-c", "user.email=simulation@localhost",
                    "-c", "commit.gpgsign=false", "commit", "--quiet", "-m", "Initialize isolated simulation fixtures"],
                   cwd=workspace, check=True, capture_output=True, text=True)
    return {"prepared": str(workspace), "model_calls": 0,
            "selected_stages": value["selected_stages"],
            "next": "Run only after the candidate package is ready; run invokes real models."}


def clock_snapshot(workspace: Path, lineage: str):
    state = workspace / ".de67/state/deadlines.sqlite3"
    if not state.exists():
        return {"tasks": [], "worker_claims": [], "worker_checkpoints": [],
                "coordinator_restart_requests": []}
    with sqlite3.connect(state.as_uri() + "?mode=ro", uri=True) as connection:
        connection.row_factory = sqlite3.Row
        tables = {row[0] for row in connection.execute("SELECT name FROM sqlite_master WHERE type='table'")}
        return {table: [dict(row) for row in connection.execute(
            f"SELECT * FROM {table} WHERE lineage_id=?", (lineage,))] if table in tables else []
            for table in ("tasks", "worker_claims", "worker_checkpoints", "coordinator_restart_requests")}


def snapshot(workspace: Path, lineage: str):
    result = clock_snapshot(workspace, lineage)
    try:
        import worker_library
        result["worker_library"] = worker_library.catalog(
            workspace, state=workspace / ".de67/state/deadlines.sqlite3", lineage=lineage)
    except (ImportError, OSError, ValueError, sqlite3.Error) as error:
        result["worker_library"] = {"unavailable": str(error)}
    if result["worker_claims"]:
        try:
            from work_context import token_usage_view, thread_records, connect_index
            from usage_projection import usage_projection
            result["token_usage"] = token_usage_view(
                workspace, workspace / ".de67/state/deadlines.sqlite3", lineage, details=True)
            metadata = thread_records(result["worker_claims"])
            index = connect_index(workspace)
            try:
                result["simulation_usage"] = usage_projection(
                    index, metadata, root_id=None, details=True)
            finally:
                index.close()
            result["simulation_usage"]["selection_basis"] = (
                "Exact worker and coordinator UUIDs claimed only in this isolated simulation; "
                "each session's own response counted once across all its jobs. Native helper "
                "costs require separate tree coverage and are not silently assumed present.")
        except (OSError, ValueError, sqlite3.Error) as error:
            result["token_usage"] = {"unavailable": str(error)}
    return result


def publish_stage(workspace: Path, fixture: dict, stage: dict):
    for relative in stage["publish"]:
        path = fixture_path(workspace, relative)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(fixture["documents"][relative], encoding="utf-8")
    # Only current tasks are actionable. Earlier results remain in durable state and files.
    lines = ["# Current simulation assignments\n",
             "These are evidence interpretation jobs. Historical product IDs are provenance only.\n"]
    for claim in dict.fromkeys(task["claim"] for task in stage["tasks"]):
        lines.extend([f"- [ ] {claim} — {fixture['claims'][claim]}",
                      f"  - DFS slices: `{claim}-S001`"])
        for task in stage["tasks"]:
            if task["claim"] == claim:
                lines.append(f"  - Assignment {task['id']}: {task['outcome']}")
    (workspace / ".de67/work-ledger.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


def stage_prompt(workspace: Path, state: Path, lineage: str, run_id: str,
                 generation: int | None, stage: dict, resumed: bool):
    from coordinator_supervisor import coordinator_prompt, coordinator_continuation_prompt
    base = coordinator_continuation_prompt() if resumed else coordinator_prompt(
        workspace, state, lineage, run_id, generation,
        "Simulation mutator correction: reconcile the existing night caller with the retained report."
        if generation is not None else None)
    # The prefix prevents the runner's fresh-prompt regeneration from discarding this bounded scope.
    return (
        "Coordinate this isolated worker-library simulation stage.\n" + base +
        "\nCurrent owner scope is the stage below. Complete only its listed task IDs and artifacts, "
        "record genuine worker result receipts and their task terminal transitions, and then end "
        "this coordinator turn. Unlisted FS items are future simulation inputs, not permission "
        "to start additional work. Do not close a product claim from simulated evidence.\n"
        "Use the candidate named worker library and its real App Server worker sessions under "
        "the runtime contracts above. Open the exact listed tasks with your own honest estimates "
        "for these analysis jobs and obtain their real policy-issued dispatch packets. "
        "You may read the named worker CLI's --help for exact commands.\n"
        f"Named worker CLI: {sys.executable} {SCRIPTS / 'worker_library.py'} --workspace {workspace}\n"
        "Return a compact account of the actual reuse/creation choices and relevant uncertainty. "
        "Do not self-award a bloat or persistence pass. Never fabricate or patch result receipts, "
        "runtime worker identities or event streams. Never use SSH/network, production tokens, "
        "or native game execution.\n\n" + json.dumps(
            {key: stage[key] for key in ("id", "publish", "tasks", "clock_context") if key in stage},
            ensure_ascii=False, indent=2) + "\n")


def completed_stage(workspace: Path, stage: dict, clock: dict):
    tasks = {task["task_id"]: task for task in clock["tasks"]}
    receipted = {row["task_id"] for row in clock["worker_checkpoints"]
                 if row["kind"] == "result-receipt-v1"}
    missing = []
    for task in stage["tasks"]:
        row = tasks.get(task["id"], {})
        if row.get("completed_at") is None or task["id"] not in receipted:
            missing.append(task["id"] + ": genuine completed task/result receipt missing")
        missing.extend(relative + ": output missing" for relative in task["artifacts"]
                       if not fixture_path(workspace, relative).is_file())
    return missing


def run(workspace: Path, codex: str, through: str | None):
    if sys.platform == "win32":
        raise ValueError("Real simulation requires the candidate Unix App Server transport")
    import worker_library  # Fail before model startup if the candidate package is incomplete.
    from coordinator_supervisor import run_child
    from deadline_harness import DeadlineHarness
    value = manifest(workspace)
    fixture = scenario_fixture(workspace)
    if through and through not in {stage["id"] for stage in fixture["stages"]}:
        raise ValueError("Unknown --through stage: " + through)
    state = workspace / ".de67/state/deadlines.sqlite3"
    run_root = workspace / ".de67/state/simulation-runs"
    run_root.mkdir(exist_ok=True)
    lineage = value["lineage"]
    if value["stages"] and not value["stages"][-1].get("completed"):
        raise ValueError("Last stage is incomplete; inspect its actual evidence before starting a new run")
    for index, stage in enumerate(fixture["stages"]):
        if index < len(value["stages"]):
            if through == stage["id"]:
                break
            continue
        publish_stage(workspace, fixture, stage)
        run_id = stage["id"] + "-" + uuid.uuid4().hex[:12]
        generation = None
        resume = value["stages"][-1]["coordinator_session_id"] if value["stages"] else None
        with DeadlineHarness(state) as harness:
            if stage.get("restart"):
                restart = harness.request_coordinator_restart(
                    lineage, "Isolated simulation mutator correction: current night source has an "
                    "existing production caller; preserve observation and revise the old hypothesis.")
                generation = restart["coordinator_restart"]["generation"]
                harness.claim_coordinator_restart(lineage, generation, run_id)
                write_json(workspace / "protocol/staged-mutation.json", {
                    "classification": "staged fixture correction with real coordinator restart; no mutation-review proof",
                    "correction": "evidence/night-source-correction.md", "restart": restart})
                resume = None
        write_json(workspace / "protocol" / (stage["id"] + "-before.json"), snapshot(workspace, lineage))
        prompt = stage_prompt(workspace, state, lineage, run_id, generation, stage, bool(resume))
        candidate = {name: digest(SCRIPTS / name) for name in (
            "worker_library.py", "codex_app_server_runner.py", "coordinator_supervisor.py",
            "policy_kernel.py", "context_library.py", "worker_packet.py", "work_context.py",
            "instruction_context.py", "deadline_harness.py")}
        record = {"id": stage["id"], "run_id": run_id, "candidate_sha256": candidate,
                  "coordinator_session_id": None, "restart_generation": generation, "completed": False}
        value["stages"].append(record)
        write_json(workspace / MARKER, value)
        print(json.dumps({"stage": stage["id"], "state": "starting", "resumes": resume}), flush=True)
        child = run_child(
            [sys.executable, "-B", str(SCRIPTS / "codex_runner.py")], workspace, state,
            lineage, run_root, run_id, generation, resume_session_id=resume,
            prompt_override=prompt,
            extra_env={"DE67_CODEX": codex, "DE67_AGENT_TRANSPORT": "app-server",
                       "DE67_AGENT_TRANSPORT_PYTHON": sys.executable,
                       "DE67_RUNNER_ROOT": str(workspace / ".de67/state/runner-runs"),
                       "DE67_COORDINATOR_MODEL": "gpt-5.6-sol",
                       "DE67_COORDINATOR_REASONING_EFFORT": "low",
                       "DE67_COORDINATOR_SANDBOX": "workspace-write"},
        )
        session_file = child.run_dir / "session_id.txt"
        record["coordinator_session_id"] = session_file.read_text().strip() if session_file.exists() else None
        record["exit_code"] = child.exit_code
        after = snapshot(workspace, lineage)
        write_json(workspace / "protocol" / (stage["id"] + "-after.json"), after)
        record["missing"] = completed_stage(workspace, stage, after)
        record["completed"] = child.exit_code == 0 and bool(record["coordinator_session_id"]) and not record["missing"]
        write_json(workspace / MARKER, value)
        print(json.dumps({"stage": stage["id"], "completed": record["completed"],
                          "missing": record["missing"]}), flush=True)
        inspect(workspace)
        if not record["completed"] or through == stage["id"]:
            break
    return inspect(workspace)


def inspect(workspace: Path):
    value = manifest(workspace)
    fixture = scenario_fixture(workspace)
    clock = clock_snapshot(workspace, value["lineage"])
    owners = {row["task_id"]: row["worker_id"] for row in clock["worker_claims"]}
    def same(first, second):
        return owners[first] == owners[second] if first in owners and second in owners else None
    commands, raw_usage, messages, worker_messages, deliveries = [], [], [], [], []
    event_paths = sorted((workspace / ".de67/state/runner-runs").glob("*/events.jsonl"))
    event_paths.extend(sorted((workspace / ".de67/state/worker-library/events").glob("*.jsonl")))
    for path in event_paths:
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            try:
                event = json.loads(line)
            except json.JSONDecodeError:
                continue
            if event.get("method") == "de67/workerDelivery/prepared":
                deliveries.append({"source": str(path.relative_to(workspace)), "line": number,
                                   "event": event})
            item = event.get("item", {})
            item_completed = event.get("type") == "item.completed"
            if event.get("worker_name") and event.get("method") == "item/completed":
                from codex_app_server_runner import normalize_item
                item = normalize_item(event.get("params", {}).get("item", {}))
                item_completed = True
            if item_completed and item.get("type") == "command_execution":
                command = item.get("command", "")
                commands.append({"source": str(path.relative_to(workspace)), "line": number,
                                 "actor": event.get("worker_name", "coordinator"),
                                 "task_id": event.get("task_id"), "command": command,
                                 "output_bytes": len(str(item.get("aggregated_output", "")).encode())})
            if item.get("type") == "agent_message" and item_completed:
                target = worker_messages if event.get("worker_name") else messages
                target.append({"source": str(path.relative_to(workspace)), "line": number,
                               "actor": event.get("worker_name", "coordinator"),
                               "task_id": event.get("task_id"), "text": item.get("text", "")})
            if "usage" in str(event.get("type", "")).lower() or "usage" in str(event.get("method", "")).lower() or "usage" in event:
                raw_usage.append({"source": str(path.relative_to(workspace)), "line": number, "event": event})
    repeated = Counter(json.dumps(item["command"], sort_keys=True) for item in commands)
    packet_paths = sorted((workspace / ".de67/state/worker-dispatch").glob("*.md"))
    packet_paths.extend(sorted((workspace / ".de67/state").rglob("prompt.txt")))
    packets = [{"path": str(path.relative_to(workspace)), "bytes": path.stat().st_size,
                "role": "dispatch_packet" if "worker-dispatch" in path.parts else (
                    "runner_prompt" if "runner-runs" in path.parts else "supervisor_prompt_copy"),
                "sha256": digest(path)} for path in packet_paths]
    outputs = [{"path": str(path.relative_to(workspace)), "bytes": path.stat().st_size,
                "sha256": digest(path)} for path in (workspace / "outputs").rglob("*") if path.is_file()]
    sessions = {stage["id"]: stage.get("coordinator_session_id") for stage in value["stages"]}
    night_sessions = [sessions.get(name) for name in ("night-footing", "restart-correction")]
    result = {
        "schema": "de67.worker-library-simulation-observation.v1",
        "evidence_class": fixture["evidence_class"], "stages": value["stages"],
        "selected_stages": [stage["id"] for stage in fixture["stages"]],
        "task_worker_ids": owners,
        "identity_observations": {
            "related_sound_same_worker_uuid": same("SIM-SOUND-001", "SIM-SOUND-002"),
            "night_worker_uuid_preserved_after_restart": same("SIM-NIGHT-001", "SIM-NIGHT-002"),
            "fresh_successor_coordinator_uuid": night_sessions[0] != night_sessions[1]
                if all(night_sessions) else None,
            "independent_charter_worker_distinct_from_sound": not same("SIM-SOUND-002", "SIM-NPC-001")
                if "SIM-SOUND-002" in owners and "SIM-NPC-001" in owners else None,
        },
        "bloat_observations": {"command_calls": commands,
            "prepared_worker_deliveries": deliveries,
            "repeated_exact_commands": [{"command": json.loads(command), "count": count}
                                        for command, count in repeated.items() if count > 1],
            "prompt_and_dispatch_files": packets,
            "note": "Counts/bytes are measurements, not pass thresholds. Repeated commands may be useful; inspect their purpose."},
        "raw_usage_events": raw_usage, "coordinator_messages": messages,
        "worker_messages": worker_messages, "output_artifacts": outputs,
        "review_questions": fixture["review_questions"] if len(value["stages"]) == len(fixture["stages"])
            else ["Future scenario review questions remain outside the worker workspace until the last stage."],
        "review_status": "Manual review of real choices and artifact content required; this report does not self-award success.",
    }
    write_json(workspace / "protocol/observations.json", result)
    return {"report": str(workspace / "protocol/observations.json"),
            "completed_stages": sum(bool(stage.get("completed")) for stage in value["stages"]),
            "required_stages": len(fixture["stages"]),
            "selected_stages": result["selected_stages"],
            "incomplete_stages": [stage["id"] for stage in value["stages"] if not stage.get("completed")],
            "identity_observations": result["identity_observations"],
            "review_status": result["review_status"]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("prepare", "run", "inspect"))
    parser.add_argument("--workspace", type=Path, required=True)
    parser.add_argument("--codex", default=os.environ.get("DE67_CODEX", "codex"))
    parser.add_argument("--through", help="Stop at this named natural stage boundary for inspection")
    parser.add_argument("--from", dest="first_stage",
                        help="Prepare a fresh workspace starting at this stage; persists the selected subset")
    args = parser.parse_args()
    if args.first_stage is not None and args.command != "prepare":
        parser.error("--from is a prepare-only selection; run uses the persisted scope")
    workspace = args.workspace.expanduser().resolve()
    try:
        result = prepare(workspace, args.first_stage) if args.command == "prepare" else (
            run(workspace, args.codex, args.through) if args.command == "run" else inspect(workspace))
    except (OSError, ValueError, ImportError, sqlite3.Error, subprocess.CalledProcessError) as error:
        print(json.dumps({"error": str(error)}), file=sys.stderr)
        return 2
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 1 if args.command == "run" and result["incomplete_stages"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
