#!/usr/bin/env python3
"""Deterministic fake Codex boundary for the real tmux stack integration test."""
from __future__ import annotations

import json
import os
import signal
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from deadline_harness import DeadlineHarness

workspace = Path(os.environ["DE67_WORKSPACE"])
state_root = workspace / ".de67/state"
release = state_root / "service-test-release"
while not release.exists():
    time.sleep(0.01)

arguments = sys.argv[1:]
try:
    sandbox = arguments[arguments.index("--sandbox") + 1]
except (ValueError, IndexError) as error:
    raise AssertionError("Codex invocation lacks --sandbox") from error

events_path = state_root / "stack-events.jsonl"
events = events_path.read_text().splitlines() if events_path.exists() else []
role = os.environ["DE67_PROCESS_ROLE"]
coordinator_round = sum(json.loads(line)["role"] == "coordinator" for line in events) + 1
if (
    role == "coordinator"
    and coordinator_round == 2
    and os.environ.get("DE67_STACK_RESTART_AFTER_ROUND_ONE") == "1"
):
    signal.signal(signal.SIGTERM, signal.SIG_IGN)
    continuation = state_root / "service-test-continue-after-restart"
    while not continuation.exists():
        time.sleep(0.01)
event = {
    "role": role,
    "sandbox": sandbox,
    "start_token": os.environ.get("DE67_SUPERVISOR_START_TOKEN"),
    "generation": os.environ.get("DE67_COORDINATOR_RESTART_GENERATION"),
    "resumed": "resume" in arguments,
}
with events_path.open("a", encoding="utf-8") as output:
    output.write(json.dumps(event) + "\n")

lineage = os.environ["DE67_LINEAGE"]
state = Path(os.environ["DE67_DEADLINE_STATE"])
product = workspace / "product.txt"
ledger = workspace / ".de67/work-ledger.md"
suggestions = workspace / ".de67/mutation-suggestions.md"

with DeadlineHarness(state) as harness:
    if role == "mutation-reviewer":
        with product.open("a", encoding="utf-8") as output:
            output.write("mutation reviewer inspected editable state\n")
        suggestions.write_text(
            "# Mutation suggestions\n\n## Completed suggestions\n\n"
            "Consumed by stack mutation reviewer.\n\n## Pending suggestions\n",
            encoding="utf-8",
        )
        harness.request_coordinator_restart(lineage, "stack mutation completed")
    elif coordinator_round == 1:
        harness.start_task(lineage, "round-one", "R-STACK", 60)
        harness.complete_task(lineage, "round-one", "round one worker proof")
        with product.open("a", encoding="utf-8") as output:
            output.write("round one worker result\n")
        with ledger.open("a", encoding="utf-8") as output:
            output.write("\nRound one evidence recorded.\n")
    elif coordinator_round == 2:
        harness.start_task(lineage, "round-two", "R-STACK", 60)
        harness.complete_task(lineage, "round-two", "round two worker proof")
        with product.open("a", encoding="utf-8") as output:
            output.write("round two worker result\n")
        suggestions.write_text(
            "# Mutation suggestions\n\n## Pending suggestions\n\n"
            "- Owner-authorized [trigger]: Review the stack handoff.\n",
            encoding="utf-8",
        )
    elif coordinator_round == 3:
        generation = int(os.environ["DE67_COORDINATOR_RESTART_GENERATION"])
        harness.acknowledge_coordinator_restart(
            lineage, generation, os.environ["DE67_COORDINATOR_RUN_ID"]
        )
        harness.start_task(lineage, "post-mutation", "R-STACK", 60)
        harness.complete_task(lineage, "post-mutation", "fresh coordinator proof")
        with product.open("a", encoding="utf-8") as output:
            output.write("post-mutation worker result\n")
        (workspace / ".de67/FS.md").write_text(
            "# FS\n\nStatus: Frozen\n\n- [x] R-STACK — Stack complete\n",
            encoding="utf-8",
        )
        ledger.write_text("# Work ledger\n\n## Active work\n", encoding="utf-8")
    else:
        raise AssertionError(f"unexpected coordinator round {coordinator_round}")

print(json.dumps({"type": "thread.started", "thread_id": "stack-test-session"}), flush=True)
