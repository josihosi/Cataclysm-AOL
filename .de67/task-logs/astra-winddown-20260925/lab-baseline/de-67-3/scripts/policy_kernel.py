#!/usr/bin/env python3
"""Compile, validate, minimize, and execute the DE67 Phase-3 policy kernel."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sqlite3
import struct
import subprocess
import sys
import zlib
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable, Mapping, Sequence

SCRIPT_ROOT = str(Path(__file__).resolve().parent)
if SCRIPT_ROOT not in sys.path:
    sys.path.insert(0, SCRIPT_ROOT)
import symbol_codec
from context_library import dispatch_context, selected_sources, task_view, ContextError
from instruction_context import common_guidance
from worker_packet import standing_section
from agent_mailbox import communication_contract
from work_context import context_view, provider_context, record_dispatch, dispatch_evidence_index
from mutation_guard import extract_dfs_slices, _active_work_blocks, _ledger_slice_ids, GuardError
from specification import SpecificationError, resolve


MAGIC = b"D67P"
HEADER = struct.Struct(">4sB32sI")
FORMAT = "de67.phase3.policy"
VERSION = 2


class PolicyError(RuntimeError):
    """Raised when policy compilation or execution is not trustworthy."""


@dataclass(frozen=True)
class Decision:
    action: str
    rule_id: str
    reads: tuple[str, ...]
    obligations: tuple[str, ...]


def canonical_bytes(value: Mapping[str, Any]) -> bytes:
    return json.dumps(
        value, sort_keys=True, separators=(",", ":"), ensure_ascii=True
    ).encode("ascii")


def validate_policy(value: Mapping[str, Any]) -> dict[str, Any]:
    policy = dict(value)
    if policy.get("format") != FORMAT or policy.get("version") != VERSION:
        raise PolicyError("Unsupported Phase-3 policy format or version")
    fallback = policy.get("fallback")
    rules = policy.get("rules")
    if not isinstance(fallback, dict) or not isinstance(rules, list) or not rules:
        raise PolicyError("Policy requires one fallback and at least one rule")
    ids: set[str] = set()
    normalized_rules: list[dict[str, Any]] = []
    for rule in rules:
        if not isinstance(rule, dict):
            raise PolicyError("Every policy rule must be an object")
        rule_id = rule.get("id")
        if not isinstance(rule_id, str) or not rule_id or rule_id in ids:
            raise PolicyError("Policy rule ids must be unique non-empty strings")
        ids.add(rule_id)
        if not isinstance(rule.get("priority"), int):
            raise PolicyError(f"Rule {rule_id} requires an integer priority")
        if not isinstance(rule.get("action"), str) or not rule["action"]:
            raise PolicyError(f"Rule {rule_id} requires an action")
        for key in ("all", "any", "none", "reads", "obligations"):
            if key in rule and not (
                isinstance(rule[key], list)
                and all(isinstance(item, str) and item for item in rule[key])
            ):
                raise PolicyError(f"Rule {rule_id} has invalid {key}")
        if not any(rule.get(key) for key in ("all", "any")):
            raise PolicyError(f"Rule {rule_id} has no positive predicate")
        normalized_rule = dict(rule)
        for key in ("all", "any", "none", "reads", "obligations"):
            normalized_rule.setdefault(key, [])
        normalized_rules.append(normalized_rule)
    for key in ("action", "reads", "obligations"):
        if key not in fallback:
            raise PolicyError(f"Fallback requires {key}")
    trace = policy.get("trace")
    if not isinstance(trace, dict) or not isinstance(trace.get("events"), dict):
        raise PolicyError("Policy requires a mutable trace event program")
    normalized_events: dict[str, dict[str, Any]] = {}
    for event, program in trace["events"].items():
        if not isinstance(event, str) or not event or not isinstance(program, dict):
            raise PolicyError("Trace events require named object programs")
        for key in ("requires", "forbids", "set", "clear"):
            if key in program and not (
                isinstance(program[key], list)
                and all(isinstance(item, str) and item for item in program[key])
            ):
                raise PolicyError(f"Trace event {event} has invalid {key}")
        keyed = program.get("keyed")
        if keyed is not None and not (
            isinstance(keyed, dict)
            and isinstance(keyed.get("field"), str)
            and keyed.get("operation") in {"start", "finish"}
        ):
            raise PolicyError(f"Trace event {event} has invalid keyed transition")
        normalized_program = dict(program)
        for key in ("requires", "forbids", "set", "clear"):
            normalized_program.setdefault(key, [])
        if keyed is not None:
            normalized_keyed = dict(keyed)
            normalized_keyed.setdefault("namespace", normalized_keyed["field"])
            normalized_keyed.setdefault("from", "live")
            normalized_keyed.setdefault("state", "live")
            normalized_program["keyed"] = normalized_keyed
        normalized_events[event] = normalized_program
    normalized_fallback = dict(fallback)
    normalized_fallback.setdefault("reads", [])
    normalized_fallback.setdefault("obligations", [])
    policy["fallback"] = normalized_fallback
    policy["rules"] = sorted(normalized_rules, key=lambda rule: (-rule["priority"], rule["id"]))
    policy["trace"] = {"events": normalized_events}
    return policy


def compile_policy(policy: Mapping[str, Any]) -> bytes:
    normalized = validate_policy(policy)
    raw = canonical_bytes(normalized)
    digest = hashlib.sha256(raw).digest()
    tape = symbol_codec.encode(_lower_policy(normalized))
    compressed = zlib.compress(tape, level=9)
    return HEADER.pack(MAGIC, VERSION, digest, len(tape)) + compressed


def load_policy_bytes(data: bytes) -> dict[str, Any]:
    if len(data) < HEADER.size:
        raise PolicyError("Compiled policy is truncated")
    magic, version, expected_digest, raw_size = HEADER.unpack(data[: HEADER.size])
    if magic != MAGIC or version != VERSION:
        raise PolicyError("Compiled policy has an invalid header")
    try:
        tape = zlib.decompress(data[HEADER.size :])
    except zlib.error as error:
        raise PolicyError("Compiled policy payload is corrupt") from error
    if len(tape) != raw_size:
        raise PolicyError("Compiled policy size does not match its payload")
    try:
        value = _raise_policy(symbol_codec.decode(tape))
    except symbol_codec.CodecError as error:
        raise PolicyError("Compiled policy symbol tape is invalid") from error
    normalized = validate_policy(value)
    if hashlib.sha256(canonical_bytes(normalized)).digest() != expected_digest:
        raise PolicyError("Compiled policy identity does not match its payload")
    return normalized


def _lower_policy(policy: Mapping[str, Any]) -> list[Any]:
    """Remove schema words and lower mutable policy values to positional vectors."""
    fallback = policy["fallback"]
    rules = [
        [
            rule["id"], rule["priority"], rule.get("all", []), rule.get("any", []),
            rule.get("none", []), rule["action"], rule.get("reads", []),
            rule.get("obligations", []),
        ]
        for rule in policy["rules"]
    ]
    events = []
    for name, program in sorted(policy["trace"]["events"].items()):
        keyed = program.get("keyed")
        keyed_vector = [] if keyed is None else [
            keyed["field"], keyed.get("namespace", keyed["field"]),
            keyed["operation"], keyed.get("from", "live"),
            keyed.get("state", "live"),
        ]
        events.append([
            name, program.get("requires", []), program.get("forbids", []),
            program.get("set", []), program.get("clear", []), keyed_vector,
        ])
    return [[fallback["action"], fallback.get("reads", []), fallback.get("obligations", [])], rules, events]


def _raise_policy(tape: Any) -> dict[str, Any]:
    """Reconstruct source-shaped policy from a schema-lowered instruction tape."""
    try:
        fallback_vector, rule_vectors, event_vectors = tape
        fallback = {
            "action": fallback_vector[0], "reads": fallback_vector[1],
            "obligations": fallback_vector[2],
        }
        rules = []
        for vector in rule_vectors:
            rule = {
                "id": vector[0], "priority": vector[1], "all": vector[2],
                "any": vector[3], "none": vector[4], "action": vector[5],
                "reads": vector[6], "obligations": vector[7],
            }
            rules.append(rule)
        events = {}
        for vector in event_vectors:
            program = {
                "requires": vector[1], "forbids": vector[2], "set": vector[3],
                "clear": vector[4],
            }
            if vector[5]:
                keyed = vector[5]
                program["keyed"] = {
                    "field": keyed[0], "namespace": keyed[1],
                    "operation": keyed[2], "from": keyed[3], "state": keyed[4],
                }
            events[vector[0]] = program
    except (IndexError, TypeError, ValueError) as error:
        raise PolicyError("Compiled policy instruction tape has an invalid shape") from error
    return {"format": FORMAT, "version": VERSION, "fallback": fallback, "rules": rules,
            "trace": {"events": events}}


def load_policy(path: Path) -> dict[str, Any]:
    return load_policy_bytes(path.read_bytes())


def _matches(rule: Mapping[str, Any], facts: frozenset[str]) -> bool:
    required = frozenset(rule.get("all", []))
    alternatives = frozenset(rule.get("any", []))
    excluded = frozenset(rule.get("none", []))
    return required <= facts and (not alternatives or bool(alternatives & facts)) and not excluded & facts


def decide(policy: Mapping[str, Any], facts: Iterable[str]) -> Decision:
    normalized = validate_policy(policy)
    observed = frozenset(str(fact) for fact in facts)
    matches = [rule for rule in normalized["rules"] if _matches(rule, observed)]
    if not matches:
        fallback = normalized["fallback"]
        return Decision(
            str(fallback["action"]),
            "fallback",
            tuple(fallback.get("reads", [])),
            tuple(fallback.get("obligations", [])),
        )
    priority = matches[0]["priority"]
    winners = [rule for rule in matches if rule["priority"] == priority]
    actions = {rule["action"] for rule in winners}
    if len(actions) != 1:
        raise PolicyError(
            "Ambiguous policy decision at priority "
            f"{priority}: " + ", ".join(rule["id"] for rule in winners)
        )
    reads = tuple(dict.fromkeys(item for rule in winners for item in rule.get("reads", [])))
    obligations = tuple(
        dict.fromkeys(item for rule in winners for item in rule.get("obligations", []))
    )
    return Decision(str(winners[0]["action"]), "+".join(rule["id"] for rule in winners), reads, obligations)


def decision_json(decision: Decision, facts: Iterable[str]) -> dict[str, Any]:
    return {
        "action": decision.action,
        "rule": decision.rule_id,
        "reads": list(decision.reads),
        "obligations": list(decision.obligations),
        "facts": sorted(set(facts)),
    }


def _ledger_objective(route: str) -> str:
    lines = route.splitlines()
    kept: list[str] = []
    active = True
    for line in lines:
        if re.match(r"^- \[[ xX]\] ", line):
            active = True
        elif line.startswith("  - "):
            active = False
        if active:
            kept.append(line)
    return "\n".join(kept).strip()


def _selected_ledger_frontier(route: str) -> str:
    lines = route.splitlines()
    selected: list[str] = []
    active = False
    for line in lines:
        if re.match(r"^- \[[ xX]\] ", line):
            active = False
        match = re.match(r"^  - ([^:\n]+):", line)
        if match is not None:
            # The coordinator owns the active projection; arbitrary labels cannot
            # silently remove an actionable continuation or its evidence.
            active = (match.group(1).strip().lower() != "dfs slices"
                      and not match.group(1).strip().lower().startswith("assignment "))
        if active:
            selected.append(line)
    return "\n".join(selected).strip()


def _referenced_entrypoints(*values: str) -> list[str]:
    result: list[str] = []
    for value in values:
        for token in re.findall(r"`([^`]+)`", value):
            candidate = token.strip()
            if (
                "/" not in candidate
                and not candidate.endswith((".py", ".cpp", ".h", ".json", ".md"))
            ):
                continue
            if candidate not in result:
                result.append(candidate)
    return result


def current_owner_contract(workspace: Path) -> str:
    """Carry explicit delivery corrections, never the Phase-2 WEC handoff."""
    path = workspace / ".de67/WEC.md"
    if not path.is_file():
        return ""
    source_bytes = path.read_bytes()
    source = source_bytes.decode("utf-8").replace("\r\n", "\n").replace("\r", "\n")
    begin, end = "<!-- DE67:OWNER-CONTRACT:BEGIN -->", "<!-- DE67:OWNER-CONTRACT:END -->"
    if begin not in source and end not in source:
        # The FS is Phase 3's contract. An unmarked WEC belongs to Phase 2.
        return ""
    else:
        if source.count(begin) != 1 or source.count(end) != 1:
            raise PolicyError("Owner contract requires one complete marked section")
        before, _, tail = source.partition(begin)
        body, marker, _ = tail.partition(end)
        if not marker or end in before or not body.strip():
            raise PolicyError("Owner contract markers are empty or out of order")
        body = body.strip()
    return (
        "Current owner contract (.de67/WEC.md marked section sha256 "
        + hashlib.sha256(body.encode("utf-8")).hexdigest() + "):\n" + body + "\n"
        "Apply these current constraints to this assignment and every helper handoff. "
        "They govern generic repair/finding permissions and supersede historical strategy, "
        "including no-replay advice where the owner requires fresh proof. Acknowledge relevant pending "
        "execution corrections and report applied evidence or deliberate deferral with its reason; "
        "do not silently drop them. Preserve earlier "
        "valid implementation and evidence at their original scope. Inconclusive attempts are not route "
        "bans: name the missing evidence and changed step that makes repetition informative.\n"
    )


def _dfs_worker_boundary(dfs_slice: str) -> str:
    """Preserve the whole outcome contract; omit only the legacy status projection."""
    return re.split(r"^Implementation status:\s*$", dfs_slice, maxsplit=1,
                    flags=re.MULTILINE)[0].strip()


def _worker_read_plan(
    workspace: Path,
    state: Path,
    lineage_id: str,
    claim_id: str,
    entrypoints: Sequence[str],
    *,
    playtest: bool,
) -> list[dict[str, str]]:
    plan: list[dict[str, str]] = []
    for entrypoint in entrypoints:
        plan.append({
            "source": entrypoint,
            "reason": "named by the current boundary as an implementation or evidence entrypoint",
        })
    if playtest:
        plan.append({
            "source": ".agents/skills/caol-harness/SKILL.md",
            "reason": "owns the current registry and cockpit authority/evidence route",
        })
    plan.extend([
        {
            "source": f".de67/FS.md slice for {claim_id}",
            "reason": "read on demand if the compact packet leaves the product or proof boundary ambiguous",
        },
        {
            "source": f".de67/work-ledger.md item for {claim_id}",
            "reason": "read on demand if repository evidence contradicts this packet or the active projection changes",
        },
    ])
    return plan


def _exploration_route(workspace: Path, claim_id: str, task_id: str) -> tuple[str, str]:
    ledger_path = workspace / ".de67/work-ledger.md"
    try:
        specification = resolve(workspace / ".de67")
    except SpecificationError as error:
        raise PolicyError(str(error)) from error
    dfs_path = specification.path
    ledger = ledger_path.read_text(encoding="utf-8")
    # Assignments are nested under the owning active claim item; cross-reference
    # mentions elsewhere must not become an assignment route.  The guard owns
    # parsing of active blocks and validation of their FS selector line.
    claim_blocks = [
        (reference, block)
        for reference, block in _active_work_blocks(ledger)
        if reference.split(" — ", 1)[0] == claim_id
    ]
    assigned_blocks = [
        item for item in claim_blocks
        if re.search(rf"^  - Assignment {re.escape(task_id)}:", item[1], re.MULTILINE)
    ]
    # Explicit assignments own their block. Legacy whole-claim tasks remain
    # compatible when exactly one slice-bearing claim block is unambiguous.
    owning_blocks = assigned_blocks
    if not owning_blocks and task_id != claim_id:
        owning_blocks = [
            item for item in claim_blocks
            if re.search(r"^  - DFS slices:", item[1], re.MULTILINE)
        ]
    if len(owning_blocks) != 1:
        raise PolicyError(
            f"Unbound exploration task {task_id} must have exactly one owning ledger item for {claim_id}"
        )
    reference, owning_route = owning_blocks[0]
    # Keep independent same-claim ledger frontiers visible in the packet while
    # taking FS content only from the selected owner block.
    route = "\n\n".join(block for _, block in claim_blocks)
    try:
        slice_ids = _ledger_slice_ids(owning_route, reference)
    except GuardError as error:
        raise PolicyError(str(error)) from error
    try:
        selected = extract_dfs_slices(dfs_path, claim_id, slice_ids)
    except GuardError as error:
        raise PolicyError(str(error)) from error
    if not selected.strip():
        raise PolicyError(
            f"Unbound exploration task {task_id} has empty selected FS slices for {claim_id}"
        )
    return route.strip(), selected.strip()


def worker_helper_contract() -> str:
    return ('When native helpers and Luna are available, use model="gpt-6-luna", '
            'fork_turns="none" and suitable effort for bounded discovery or suitable execution. '
            'Otherwise use focused local retrieval within this task. Helpers never own coordination '
            'records; the primary worker collects or stops them before returning. '
            'The primary worker owns every game it or its helpers launches, including failed startups '
            'and replacement processes, until verified OS exit or explicit ownership handoff. '
            'At finish use the native quit/save route appropriate to that run and reconcile all owned '
            'PID/birth identities and brokers. A hidden window, stopped broker, or run.finish response '
            'does not prove process exit. Preserve owner-retained sessions and report a failed graceful '
            'closure with its exact live identity; do not silently force-kill it. ')


def worker_outcome_contract() -> str:
    """Keep recoverable work inside the outcome and reserve terminal findings."""
    return (
        "Deliver the simplest coherent implementation of the requested behavior using the repository's "
        'existing mechanisms. Proposed decomposition is revisable. Consolidate temporary workarounds '
        'introduced during this assignment when they are no longer needed; keep unrelated refactoring '
        'outside the assignment. '
        "Repository-owned implementation, tooling, fixture, scenario, binding and observation repairs remain recoverable work within the assigned scope. "
        "When a prerequisite becomes a substantial independent investigation, ask Sol to decide its ownership; "
        "continue independent work and preserve live runs and useful understanding. Do not silently absorb unrelated prerequisites. "
        "When a proof prerequisite depends on output it is about to create, treat that work as "
        "non-credit bootstrap and validate the fresh output independently. "
        "Before interpreting absent behavior, establish its actual opportunity to occur and compare "
        "the premises of any earlier success. Communicate missing premises or ambiguous results "
        "so the coordinator can steer the same investigation. A gameplay bug requires both a "
        "contradiction under valid conditions and its causal code path; unresolved reproduction "
        "remains ledger work, with gameplay repair promotion owned by the user. "
        "Return completion only when the assigned outcome is "
        "settled. A disproved strategy is progress, not a task exit: preserve its evidence and "
        "continue while repository recovery "
        "remains. Return a formal finding only for a contradicted assigned outcome, materially "
        "different owner outcome, real external or human decision, unavailable capability, "
        "irreversible risk, or an authorized route you have genuinely exhausted."
    )


def worker_communication_contract() -> str:
    """Communicate decision-changing evidence without a reporting lifecycle."""
    return (
        "Named workers use the coordinator mailbox command supplied here; native children use "
        "send_message(target=\"/root\", message=...). Share progress or questions that can "
        "change coordination or another worker's work. Ask the coordinator for help when results "
        "surprise you, progress stalls, or uncertainty about where to look, how to approach the task, "
        "or how to interpret evidence could change your next steps. Share the "
        "relevant actual state, expected behavior, evidence and uncertainty so you can reason "
        "together before repeating an ineffective approach. If retrieval itself is awkward, name "
        "the question the available view could not answer and the relevant evidence handle; let "
        "Luna retrieve the facts and work with the coordinator on a useful ergonomic repair. "
        "Use the findings to revise the experiment. Continue authorized unblocked work "
        "while awaiting a reply. Messages are "
        "nonterminal and need no receipt, ledger entry, periodic report or quota. Your final response "
        "returns the assignment result. The /root address is not a durable worker identity. "
    )


def _write_worker_dispatch_packet(
    workspace: Path, task_name: str, message: str
) -> tuple[Path, str]:
    """Persist one immutable worker-only brief and return its content identity."""
    encoded = (message.rstrip() + "\n").encode("utf-8")
    digest = hashlib.sha256(encoded).hexdigest()
    packet_directory = workspace / ".de67" / "state" / "worker-dispatch"
    packet_directory.mkdir(parents=True, exist_ok=True)
    packet = packet_directory / f"{task_name}-{digest}.md"
    try:
        with packet.open("xb") as stream:
            stream.write(encoded)
    except FileExistsError:
        if packet.read_bytes() != encoded:
            raise PolicyError(f"Worker dispatch packet identity collision: {packet}")
    return packet.resolve(), digest


def worker_model_choices(workspace: Path) -> list[dict[str, str]]:
    """Expose available worker capabilities without choosing for the coordinator."""
    path = workspace / ".de67/state/workspace.json"
    configured = json.loads(path.read_text(encoding="utf-8")).get("worker_capabilities") if path.is_file() else None
    efforts_by_model = {
        "gpt-6-luna": ("low", "medium", "high", "xhigh", "max"),
        "gpt-6-sol": ("low", "medium", "high", "xhigh", "max", "ultra"),
        "gpt-6-astra": ("low", "medium", "high", "xhigh", "max", "ultra"),
    }
    capabilities = configured if configured is not None else [
        {"model": model, "reasoning_effort": effort}
        for model, efforts in efforts_by_model.items() for effort in efforts
    ]
    if not isinstance(capabilities, list):
        raise PolicyError("worker_capabilities must be a list")
    result = []
    for value in capabilities:
        if not isinstance(value, dict) or value.get("model") not in efforts_by_model:
            continue
        choice = {"model": value["model"], "reasoning_effort": value.get("reasoning_effort", "medium")}
        # Setup records successfully probed pairs; defaults do not restrict that roster.
        if choice["reasoning_effort"] not in efforts_by_model[choice["model"]]:
            raise PolicyError("Unsupported worker reasoning effort")
        if (choice["model"] == "gpt-6-astra"
                and choice["reasoning_effort"] not in efforts_by_model[choice["model"]]):
            raise PolicyError("Unsupported ordinary-worker model and reasoning effort")
        if choice not in result:
            result.append(choice)
    if not result:
        raise PolicyError("No configured ordinary-worker capability is available")
    return result


def exploration_assignment(route: str, task_id: str, claim_id: str) -> tuple[str, str]:
    """Bind an explicit task-sized ledger assignment, leaving broad work possible."""
    lines = route.splitlines()
    prefix = "  - Assignment " + task_id + ":"
    matches = [i for i, line in enumerate(lines) if line.startswith(prefix)]
    if len(matches) > 1:
        raise PolicyError("Duplicate ledger assignment for " + task_id)
    if matches:
        start = matches[0]
        body = [lines[start][len(prefix):].strip()]
        for line in lines[start + 1:]:
            if line.startswith("  - ") or line.startswith("- ") or line.startswith("#"):
                break
            body.append(line.strip())
        outcome = "\n".join(body).strip()
        if not outcome:
            raise PolicyError("Empty ledger assignment for " + task_id)
        return outcome, "task-specific ledger assignment"
    objective = _ledger_objective(route)
    if task_id != claim_id and re.search(r"^- \[ \] " + re.escape(task_id) + r"(?=\s|$)", route, re.MULTILINE):
        return objective, "task-specific ledger item"
    return objective, "whole-claim assignment; no narrower task assignment supplied"


def unbound_worker_spawns(
    workspace: Path, state: Path, lineage_id: str
) -> list[dict[str, Any]]:
    """Render the exact post-task-open spawn calls for every unbound task."""
    connection = sqlite3.connect(f"file:{state.resolve()}?mode=ro", uri=True)
    connection.row_factory = sqlite3.Row
    try:
        rows = connection.execute(
            """
            SELECT task.task_id, task.claim_id, task.phase_at_dispatch,
                   task.closure_gap_id, task.closure_gap_revision
            FROM tasks AS task
            LEFT JOIN worker_claims AS worker
              ON worker.lineage_id = task.lineage_id
             AND worker.task_id = task.task_id
            WHERE task.lineage_id = ? AND task.attempt_terminal_at IS NULL
              AND worker.task_id IS NULL
            ORDER BY task.started_at, task.task_id
            """,
            (lineage_id,),
        ).fetchall()
        spawns: list[dict[str, Any]] = []
        for row in rows:
            task_id = str(row["task_id"])
            claim_id = str(row["claim_id"])
            phase = str(row["phase_at_dispatch"])
            gap_id = row["closure_gap_id"]
            revision = row["closure_gap_revision"]
            gap = None
            assignment_scope = "bound closure gap"
            ledger_route = ""
            dfs_slice = ""
            if gap_id is not None and revision is not None:
                gap = connection.execute(
                    """
                    SELECT description, proof_route
                    FROM closure_gap_revisions
                    WHERE lineage_id = ? AND claim_id = ? AND gap_id = ?
                      AND revision = ?
                    ORDER BY closure_sequence DESC LIMIT 1
                    """,
                    (lineage_id, claim_id, gap_id, revision),
                ).fetchone()
                if gap is None:
                    raise PolicyError(
                        f"Unbound closure task {task_id} references missing gap revision"
                    )
                outcome, proof_route = str(gap["description"]), str(gap["proof_route"])
                try:
                    specification = resolve(workspace / ".de67")
                except SpecificationError as error:
                    raise PolicyError(str(error)) from error
                if specification.path.is_file():
                    match = re.search(
                        r"<!-- DE67:DFS-SLICE:BEGIN[^>]*claim=" + re.escape(claim_id)
                        + r"(?=\s|-->)[^>]*-->\n(.*?)\n<!-- DE67:DFS-SLICE:END[^>]*-->",
                        specification.text, re.DOTALL,
                    )
                    if match is None:
                        raise PolicyError(f"Closure task {task_id} has no current FS slice")
                    proof_route = _dfs_worker_boundary(match.group(1)) + "\nAssigned closure route:\n" + proof_route
            elif phase == "exploration":
                ledger_route, dfs_slice = _exploration_route(
                    workspace, claim_id, task_id
                )
                outcome, assignment_scope = exploration_assignment(ledger_route, task_id, claim_id)
                proof_route = _dfs_worker_boundary(dfs_slice)
            else:
                # Legacy closure attempts predate named gap bindings. Preserve
                # their exact claim route without pretending they are exploration.
                ledger_route, dfs_slice = _exploration_route(
                    workspace, claim_id, task_id
                )
                outcome, assignment_scope = exploration_assignment(ledger_route, task_id, claim_id)
                proof_route = _dfs_worker_boundary(dfs_slice)
            previous_row = connection.execute(
                "SELECT task_id, started_at, attempt_terminal_kind, attempt_terminal_at "
                "FROM tasks WHERE lineage_id = ? AND claim_id = ? AND task_id != ? "
                "ORDER BY started_at DESC, task_id DESC LIMIT 1",
                (lineage_id, claim_id, task_id),
            ).fetchone()
            previous_attempt = dict(previous_row) if previous_row is not None else None
            frontier = _selected_ledger_frontier(
                ledger_route
            ) if ledger_route else ""
            context = context_view(workspace, state, lineage_id, claim=claim_id,
                                   task=task_id, route=ledger_route + "\n" + proof_route)
            receipts = [item["receipt"] for item in context["selected_context"]]
            receipt_entrypoints = [path for receipt in receipts
                                   for path in receipt.get("entrypoints", [])]
            entrypoints = list(dict.fromkeys([
                *_referenced_entrypoints(outcome, frontier, proof_route),
                *receipt_entrypoints,
            ]))
            playtest = any(
                word in " ".join((outcome, frontier, proof_route)).lower()
                for word in ("playtest", "cockpit", "witness", "scenario registry")
            )
            read_plan = _worker_read_plan(
                workspace,
                state,
                lineage_id,
                claim_id,
                entrypoints,
                playtest=playtest,
            )
            task_name = "task_" + task_id.encode("utf-8").hex()
            reference_context = (
                "Related task results (independent contributions; no claim-wide latest-wins):\n"
                + json.dumps([
                    {key:item[key] for key in ("task_id", "receipt_id", "recorded_at",
                                              "relationship", "source_bytes")}
                    if receipts else item for item in context["related_results"]
                  ], ensure_ascii=False, sort_keys=True) + "\n"
                + "Broader product contract (not a task-sized completion requirement):\n" + proof_route + "\n"
                + "Claim ledger context (independent contributions retained):\n" + ledger_route + "\n"
                + "Directly related evidence, at its original scope:\n"
                + json.dumps(dispatch_evidence_index(context, detailed=True), ensure_ascii=False, sort_keys=True, indent=2) + "\n"
                + "Current harness context (observed snapshot, not live authority):\n"
                + json.dumps(provider_context(workspace, {
                    "entrypoints":_referenced_entrypoints(outcome, frontier, proof_route),
                    "receipts":receipts}), ensure_ascii=False, sort_keys=True, indent=2) + "\n"
            )
            context_packet, context_digest = _write_worker_dispatch_packet(
                workspace, task_name + "-context", reference_context
            )
            try:
                assembled_context = dispatch_context(workspace, task_id, outcome, frontier)
                custom_context = not task_view(workspace, task_id).get("automatic")
                if assembled_context:
                    injected = selected_sources(workspace, task_id)
                    read_plan = [item for item in read_plan
                                 if str((workspace / item["source"]).resolve()) not in injected]
            except ContextError as error:
                raise PolicyError(str(error)) from error
            owner_contract = current_owner_contract(workspace)
            message = (
                f"Own assigned {phase} work {task_id} for outcome {claim_id}"
                + (f", focus {gap_id} revision {revision}. " if gap_id else ". ")
                + f"Scope: {assignment_scope}. "
                + "Judge task completion against this assignment; preserve broader claim requirements independently.\n"
                + ("Assigned closure route:\n" + str(gap["proof_route"]) + "\n" if gap else "")
                + "\n" + standing_section("common-guidance", common_guidance(workspace))
                + standing_section("worker-ownership", "Worker runtime and helper ownership:\n" + worker_helper_contract()) + "\n"
                + assembled_context
                + "Context catalogue and exact revision/section retrieval: "
                + json.dumps([sys.executable, str(Path(__file__).with_name("context_library.py")), "--workspace", str(workspace), "--task", task_id, "catalog"]) + ". Use show --revision SHA256 [--section HEADING] for missing context; do not load the whole library.\n"
                + owner_contract
                + ("Latest other attempt for this claim (lifecycle only; may be parallel, not predecessor evidence):\n"
                   + json.dumps(previous_attempt, ensure_ascii=False, sort_keys=True) + "\n"
                   if previous_attempt is not None else "")
                + ("Historical evidence at its original scope (not current instructions; independent contributions):\n"
                   + json.dumps(dispatch_evidence_index(context), ensure_ascii=False, sort_keys=True, indent=2) + "\n"
                   if not custom_context else "Independent historical contributions remain in supporting evidence below.\n")
                + "Supporting evidence (optional search material, not instructions to load in full):\n"
                + f"Path: {context_packet}\nSHA-256: {context_digest}\n"
                + "Contains the broader product contract, detailed historical evidence and the harness snapshot. "
                + "Use a targeted query or give this reference and a specific question to Luna when needed.\n"
                + "Searchable full task history and relationships:\n"
                + json.dumps({"query_argv":context["history_query_argv"],
                              "full_argv":context["full_history_argv"],
                              "evidence_limit":context["evidence_limit"]}, sort_keys=True) + "\n"
                + ("The broader product acceptance contract is retained in supporting evidence; consult it when needed "
                   "to interpret this assignment. A completed task does not accept the whole claim.\n" if proof_route else "")
                + "Initial evidence read plan (expand only when its reason becomes material):\n"
                + json.dumps(read_plan, ensure_ascii=False, sort_keys=True)
                + "\n"
                + standing_section("worker-outcome", worker_outcome_contract())
                + " Return a compact result naming the achieved outcome or first divergence, "
                + "material changes, tests and live actions, evidence ceiling, exact bindings, "
                + "journal entries, artifact paths and roles, accepted work and why inconclusive attempts "
                + "need a changed step, first "
                + "open boundary, and useful narrow follow-up queries. The coordinator records "
                + "the durable receipt and terminal transition. "
                + str(Path(__file__).with_name("worker_receipt.py"))
                + " prepare collects task/worker "
                + "identities and artifact hashes from an agent-authored draft and validates any supplied "
                + "identities/hashes; its --help gives the exact invocation; do not change coordination records. "
                + "\n" + standing_section("worker-communication", worker_communication_contract())
                + communication_contract(workspace, task_id)
            )
            task_name = "task_" + task_id.encode("utf-8").hex()
            packet, packet_digest = _write_worker_dispatch_packet(
                workspace, task_name, message
            )
            record_dispatch(workspace, state, lineage_id, task_id, packet, packet_digest,
                            {"claim_id":claim_id, "closure_gap_id":gap_id,
                             "owner_contract_sha256":hashlib.sha256(owner_contract.encode("utf-8")).hexdigest(),
                             "closure_gap_revision":revision,
                             "evidence_receipts":[r["receipt_id"] for r in receipts],
                             "related_tasks":[r["task_id"] for r in context["related_results"]],
                             "relationship_basis":"exact route references or shared gap; not inferred ancestry"})
            spawn_message = (
                f"Own task {task_id}. Read your complete task brief from {packet}. "
                f"Verify its SHA-256 is {packet_digest}, then follow it."
            )
            spawns.append(
                {
                    "task_id": task_id,
                    "task_name": task_name,
                    "dispatch_packet": {
                        "path": str(packet),
                        "sha256": packet_digest,
                    },
                    "worker_library": {
                        "catalog_argv": [sys.executable, str(Path(__file__).with_name("worker_library.py")),
                                         "--workspace", str(workspace), "list"],
                        "assign_argv": [sys.executable, str(Path(__file__).with_name("worker_library.py")),
                                        "--workspace", str(workspace), "assign", "WORKER_NAME",
                                        "--task", task_id, "--packet", str(packet),
                                        "--sha256", packet_digest, "--state", str(state),
                                        "--lineage", lineage_id],
                    },
                    "instruction": (
                        "Consider reusing a named worker with a suitable job and useful context. "
                        "Use worker_library.assign_argv with that worker's name, creating a named "
                        "worker first if the job or concurrent ownership requires one. Preserve "
                        "the exact task and packet. For a native child, complete example_call.arguments "
                        "with one chosen model_choices capability, then call spawn_agent. "
                        "Choose model and reasoning effort separately for this assignment's "
                        "uncertainty and expected total work, using the existing selection guidance. "
                        "Then continue live coordination; call wait_agent "
                        "when no useful coordination decision remains. Do not finish while a "
                        "worker result is outstanding."
                    ),
                    "model_choices": worker_model_choices(workspace),
                    "assignment_scope": assignment_scope,
                    "example_call": {
                        "tool": "spawn_agent",
                        "arguments": {
                            "task_name": task_name,
                            "fork_turns": "none",
                            "message": spawn_message,
                        },
                    },
                }
            )
        return spawns
    finally:
        connection.close()


def policy_digest(policy: Mapping[str, Any]) -> str:
    return hashlib.sha256(canonical_bytes(validate_policy(policy))).hexdigest()


def load_contracts(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if value.get("format") != "de67.phase3.contracts" or value.get("version") != 1:
        raise PolicyError("Unsupported Phase-3 contract format or version")
    decisions = value.get("decision_cases")
    traces = value.get("trace_cases")
    if not isinstance(decisions, list) or not decisions or not isinstance(traces, list):
        raise PolicyError("Contracts require decision_cases and trace_cases")
    names: set[str] = set()
    for case in [*decisions, *traces]:
        if not isinstance(case, dict) or not isinstance(case.get("name"), str):
            raise PolicyError("Every contract case requires a name")
        if case["name"] in names:
            raise PolicyError(f"Duplicate contract case: {case['name']}")
        names.add(case["name"])
    return value


def guard_policy_candidate(
    candidate: Mapping[str, Any], contracts: Mapping[str, Any]
) -> dict[str, Any]:
    policy = validate_policy(candidate)
    cases: list[tuple[frozenset[str], str]] = []
    for case in contracts["decision_cases"]:
        facts = frozenset(case.get("facts", []))
        expected = case.get("action")
        if not isinstance(expected, str):
            raise PolicyError(f"Decision contract {case['name']} lacks an action")
        decision = decide(policy, facts)
        actual = decision.action
        if actual != expected:
            raise PolicyError(
                f"Decision contract {case['name']} expected {expected}, got {actual}"
            )
        for field in ("reads", "obligations"):
            required = case.get(f"required_{field}", [])
            if not isinstance(required, list) or not set(required) <= set(getattr(decision, field)):
                raise PolicyError(f"Decision contract {case['name']} lacks required {field}")
        cases.append((facts, expected))
    for case in contracts["trace_cases"]:
        expected = case.get("lab")
        if not isinstance(expected, bool) or not isinstance(case.get("events"), list):
            raise PolicyError(f"Trace contract {case['name']} is invalid")
        try:
            validate_trace(policy, case["events"])
            actual = True
        except PolicyError:
            actual = False
        if actual != expected:
            raise PolicyError(
                f"Trace contract {case['name']} expected lab={expected}, got {actual}"
            )
    minimized, removed = minimize_policy(policy, cases)
    if removed:
        raise PolicyError("Behaviorally redundant policy rules: " + ", ".join(removed))
    return minimized


def minimize_policy(
    policy: Mapping[str, Any], cases: Sequence[tuple[frozenset[str], str]]
) -> tuple[dict[str, Any], tuple[str, ...]]:
    """Delete rules whose removal preserves every supplied behavioral case."""
    candidate = validate_policy(policy)
    removed: list[str] = []
    for rule in list(reversed(candidate["rules"])):
        trial = dict(candidate)
        trial["rules"] = [item for item in candidate["rules"] if item["id"] != rule["id"]]
        if not trial["rules"]:
            continue
        try:
            if all(decide(trial, facts).action == expected for facts, expected in cases):
                candidate = validate_policy(trial)
                removed.append(str(rule["id"]))
        except PolicyError:
            continue
    return candidate, tuple(removed)


def validate_trace(policy: Mapping[str, Any], events: Sequence[Mapping[str, Any]]) -> None:
    """Execute the policy's mutable temporal event program."""
    programs = validate_policy(policy)["trace"]["events"]
    flags: set[str] = set()
    keyed: dict[tuple[str, str], str] = {}
    for index, event in enumerate(events):
        kind = str(event.get("event", ""))
        program = programs.get(kind)
        if program is None:
            continue
        required = set(program.get("requires", []))
        forbidden = set(program.get("forbids", []))
        if not required <= flags:
            raise PolicyError(f"trace[{index}] {kind} lacks state: {sorted(required - flags)}")
        if forbidden & flags:
            raise PolicyError(f"trace[{index}] {kind} conflicts with state: {sorted(forbidden & flags)}")
        transition = program.get("keyed")
        if transition:
            field = transition["field"]
            value = str(event.get(field, ""))
            namespace = str(transition.get("namespace", field))
            key = (namespace, value)
            if not value:
                raise PolicyError(f"trace[{index}] {kind} lacks {field}")
            if transition["operation"] == "start":
                if key in keyed:
                    raise PolicyError(f"trace[{index}] {kind} reuses {field}")
                keyed[key] = str(transition.get("state", "live"))
            else:
                expected = str(transition.get("from", "live"))
                if keyed.get(key) != expected:
                    raise PolicyError(f"trace[{index}] {kind} finishes non-{expected} {field}")
                keyed[key] = str(transition.get("state", kind))
        flags.difference_update(program.get("clear", []))
        flags.update(program.get("set", []))


def _table_exists(connection: sqlite3.Connection, name: str) -> bool:
    return connection.execute(
        "SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = ?", (name,)
    ).fetchone() is not None


def _terminal_result_was_consumed(
    connection: sqlite3.Connection, lineage_id: str, task: sqlite3.Row
) -> bool:
    """Return whether a newer durable route transition consumed this result."""
    task_id = str(task["task_id"])
    terminal_at = task["attempt_terminal_at"]
    if terminal_at is None:
        return False
    # A supervisor may retire an opened task before any worker claims it (for
    # example when a coordinator recovery interrupts dispatch).  There is no
    # ordinary-worker result to ingest in that case, so do not strand the
    # coordinator on receive_worker_result forever.
    if task["attempt_terminal_kind"] == "abandoned" and _table_exists(
        connection, "worker_claims"
    ):
        claimed = connection.execute(
            "SELECT 1 FROM worker_claims WHERE lineage_id = ? AND task_id = ? LIMIT 1",
            (lineage_id, task_id),
        ).fetchone()
        if claimed is None:
            return True
    if _table_exists(connection, "closure_gaps"):
        consumed = connection.execute(
            """
            SELECT 1 FROM closure_gaps
            WHERE lineage_id = ? AND closed_by_task_id = ? AND closed_at >= ?
            LIMIT 1
            """,
            (lineage_id, task_id, float(terminal_at)),
        ).fetchone()
        if consumed is not None:
            return True
    if (_table_exists(connection, "closure_gap_revisions")
            and "closure_gap_revision" in task.keys()
            and task["closure_gap_revision"] is not None):
        consumed = connection.execute(
            """
            SELECT 1 FROM closure_gap_revisions
            WHERE lineage_id = ? AND claim_id = ? AND closure_sequence = ?
              AND gap_id = ? AND revision = ?
              AND basis_task_id = ? AND recorded_at >= ?
            LIMIT 1
            """,
            (lineage_id, task["claim_id"], task["phase_sequence_at_dispatch"],
             task["closure_gap_id"], int(task["closure_gap_revision"]) + 1,
             task_id, float(terminal_at)),
        ).fetchone()
        if consumed is not None:
            return True
    if _table_exists(connection, "claim_phase_events"):
        consumed = connection.execute(
            """
            SELECT 1 FROM claim_phase_events
            WHERE lineage_id = ? AND basis_task_id = ? AND recorded_at >= ?
            LIMIT 1
            """,
            (lineage_id, task_id, float(terminal_at)),
        ).fetchone()
        if consumed is not None:
            return True
    return False


def workspace_facts(
    workspace: Path, state: Path, lineage_id: str, *, now: float
) -> frozenset[str]:
    facts: set[str] = set()
    current_claim: str | None = None
    terminal_task_ids: set[str] = set()
    owner_wait_claims: set[str] = set()
    owner_wait_gaps: set[tuple[str, int, str]] = set()
    connection = sqlite3.connect(f"file:{state.resolve()}?mode=ro", uri=True)
    connection.row_factory = sqlite3.Row
    try:
        if "route_kind" in {row[1] for row in connection.execute("PRAGMA table_info(closure_gap_revisions)")}:
            routes = connection.execute("""
                SELECT g.claim_id, g.closure_sequence, g.gap_id, r.route_kind FROM closure_gaps g
                JOIN closure_gap_revisions r USING (lineage_id, claim_id, closure_sequence, gap_id)
                JOIN claim_clocks c ON c.lineage_id=g.lineage_id AND c.claim_id=g.claim_id
                WHERE g.lineage_id = ? AND g.closed_at IS NULL AND c.phase = 'closure'
                  AND g.closure_sequence = (SELECT MAX(g2.closure_sequence) FROM closure_gaps g2
                    WHERE g2.lineage_id=g.lineage_id AND g2.claim_id=g.claim_id)
                  AND r.revision = (SELECT MAX(x.revision) FROM closure_gap_revisions x
                    WHERE x.lineage_id=g.lineage_id AND x.claim_id=g.claim_id
                      AND x.closure_sequence=g.closure_sequence AND x.gap_id=g.gap_id)
            """, (lineage_id,)).fetchall()
            owner_wait_gaps = {(str(r["claim_id"]), int(r["closure_sequence"]), str(r["gap_id"]))
                               for r in routes if r["route_kind"] == "owner_wait"}
            owner_wait_claims = {str(r["claim_id"]) for r in routes if r["route_kind"] == "owner_wait"} - {
                str(r["claim_id"]) for r in routes if r["route_kind"] == "executable"}
            if owner_wait_gaps:
                facts.add("owner_wait")
        if _table_exists(connection, "tasks"):
            rows = connection.execute(
                "SELECT * FROM tasks WHERE lineage_id = ? ORDER BY started_at DESC",
                (lineage_id,),
            ).fetchall()
            epoch_generation = None
            if _table_exists(connection, "external_supervisor_epochs"):
                epoch = connection.execute(
                    "SELECT generation FROM external_supervisor_epochs "
                    "WHERE lineage_id = ? ORDER BY generation DESC LIMIT 1",
                    (lineage_id,),
                ).fetchone()
                epoch_generation = int(epoch[0]) if epoch is not None else None
            terminal_task_ids = {str(row["task_id"]) for row in rows
                                 if row["attempt_terminal_at"] is not None}
            nonterminal = [row for row in rows if row["attempt_terminal_at"] is None]
            worker_claims_exist = _table_exists(connection, "worker_claims")
            claimed_ids: set[str] = set()
            ever_claimed_ids: set[str] = set()
            if worker_claims_exist:
                ever_claimed_ids = {
                    str(row[0])
                    for row in connection.execute(
                        "SELECT task_id FROM worker_claims WHERE lineage_id = ?",
                        (lineage_id,),
                    ).fetchall()
                }
                claimed_ids = {
                    str(row[0])
                    for row in connection.execute(
                        """
                        SELECT task_id FROM worker_claims
                        WHERE lineage_id = ? AND released_at IS NULL
                        """,
                        (lineage_id,),
                    ).fetchall()
                }
            live = [
                row for row in nonterminal
                if not worker_claims_exist or str(row["task_id"]) in claimed_ids
            ]
            from worker_library import returned_assignments
            returned = returned_assignments(workspace, state, lineage_id)
            if any(str(row["task_id"]) not in returned for row in live):
                facts.add("live_task")
            if any(str(row["task_id"]) not in ever_claimed_ids for row in nonterminal):
                facts.add("unbound_task")
            if not nonterminal:
                for row in rows:
                    if epoch_generation is not None and int(
                        row["supervisor_epoch_generation"]
                    ) < epoch_generation:
                        continue
                    kind = row["attempt_terminal_kind"]
                    if kind == "restart_normalized":
                        break
                    if kind:
                        if not _terminal_result_was_consumed(
                            connection, lineage_id, row
                        ):
                            facts.add(f"worker_{kind}")
                        break
        for table, fact in (
            ("claim_deadline_generation_incidents", "deadline_incident"),
            ("incidents", "integrity_incident"),
            ("coordinator_restart_requests", "restart_requested"),
        ):
            if _table_exists(connection, table):
                if table == "claim_deadline_generation_incidents":
                    # Match the harness's pending-incident view: a preserved
                    # generation-1 migration mirror is not a second incident.
                    query = """
                        SELECT 1 FROM claim_deadline_generation_incidents AS incident
                        WHERE incident.lineage_id = ? AND incident.reviewed_at IS NULL
                          AND NOT (incident.generation = 1 AND EXISTS (
                              SELECT 1 FROM claim_deadline_generation_incidents AS current
                              WHERE current.lineage_id = incident.lineage_id
                                AND current.claim_id = incident.claim_id
                                AND current.generation > 1
                                AND current.source_task_id = incident.source_task_id
                                AND current.recorded_at = incident.recorded_at
                          ))
                        LIMIT 1
                    """
                elif table == "coordinator_restart_requests":
                    query = f"SELECT 1 FROM {table} WHERE lineage_id = ? AND acknowledged_at IS NULL LIMIT 1"
                else:
                    query = (
                        f"SELECT 1 FROM {table} WHERE lineage_id = ? AND kind = 'integrity_breach' "
                        "AND reviewed_at IS NULL LIMIT 1"
                    )
                if connection.execute(query, (lineage_id,)).fetchone() is not None:
                    facts.add(fact)
        if _table_exists(connection, "claim_clocks"):
            if _table_exists(connection, "claim_deadline_generations"):
                clock = connection.execute(
                    """
                SELECT clock.claim_id, clock.phase,
                       generation.started_at, generation.deadline_at
                FROM claim_clocks AS clock
                JOIN claim_deadline_generations AS generation
                  ON generation.lineage_id = clock.lineage_id
                 AND generation.claim_id = clock.claim_id
                WHERE clock.lineage_id = ?
                  AND generation.retired_at IS NULL
                  AND generation.generation = (
                    SELECT MAX(latest.generation)
                    FROM claim_deadline_generations AS latest
                    WHERE latest.lineage_id = generation.lineage_id
                      AND latest.claim_id = generation.claim_id
                  )
                ORDER BY generation.started_at DESC, clock.claim_id
                    """,
                    (lineage_id,),
                ).fetchall()
            else:
                clock = connection.execute(
                    "SELECT * FROM claim_clocks WHERE lineage_id = ? "
                    "ORDER BY started_at DESC",
                    (lineage_id,),
                ).fetchall()
            accepted_claims = {
                str(row[0]) for row in connection.execute(
                    "SELECT claim_id FROM claim_acceptances WHERE lineage_id = ? AND invalidated_at IS NULL",
                    (lineage_id,),
                )
            } if _table_exists(connection, "claim_acceptances") else set()
            clock = next((row for row in clock
                          if str(row["claim_id"]) not in owner_wait_claims | accepted_claims), None)
            if clock is not None:
                current_claim = str(clock["claim_id"])
                facts.add("open_claim")
                if float(clock["deadline_at"]) <= now:
                    facts.add("deadline_expired")
                if str(clock["phase"]) == "closure":
                    facts.add("closure_ready")
        if _table_exists(connection, "closure_gaps") and current_claim is not None:
            if _table_exists(connection, "closure_gap_revisions"):
                open_gap = connection.execute(
                    """
                SELECT gap.lineage_id, gap.claim_id, gap.closure_sequence, gap.gap_id,
                       revision.proof_route
                FROM closure_gaps AS gap
                JOIN closure_gap_revisions AS revision
                  ON revision.lineage_id = gap.lineage_id
                 AND revision.claim_id = gap.claim_id
                 AND revision.closure_sequence = gap.closure_sequence
                 AND revision.gap_id = gap.gap_id
                 AND revision.revision = (
                    SELECT MAX(latest.revision)
                    FROM closure_gap_revisions AS latest
                    WHERE latest.lineage_id = gap.lineage_id
                      AND latest.claim_id = gap.claim_id
                      AND latest.closure_sequence = gap.closure_sequence
                      AND latest.gap_id = gap.gap_id
                 )
                WHERE gap.lineage_id = ? AND gap.claim_id = ?
                  AND gap.closed_at IS NULL
                ORDER BY gap.gap_id
                    """,
                    (lineage_id, current_claim),
                ).fetchall()
                open_gap = next((row for row in open_gap if (str(row["claim_id"]), int(row["closure_sequence"]), str(row["gap_id"])) not in owner_wait_gaps), None)
            else:
                open_gap = connection.execute(
                    """
                    SELECT *, NULL AS proof_route
                    FROM closure_gaps
                    WHERE lineage_id = ? AND claim_id = ? AND closed_at IS NULL
                    LIMIT 1
                    """,
                    (lineage_id, current_claim),
                ).fetchone()
            if open_gap is not None:
                facts.add("open_gap")
                if open_gap["proof_route"] is not None and str(open_gap["proof_route"]).strip():
                    facts.add("executable_route")
        if _table_exists(connection, "random_mutation_cycles"):
            random_due = connection.execute(
                """
                SELECT selected_lane FROM random_mutation_cycles
                WHERE lineage_id = ? AND due_task_id IS NOT NULL
                  AND resolution_evidence IS NULL
                ORDER BY cycle_number LIMIT 1
                """,
                (lineage_id,),
            ).fetchone()
            if random_due is not None:
                facts.add("random_mutation_due")
                if str(random_due["selected_lane"]) == "DFS.md":
                    facts.add("dfs_review_due")
    finally:
        connection.close()

    ledger = workspace / ".de67" / "work-ledger.md"
    ledger_text = ledger.read_text(encoding="utf-8") if ledger.is_file() else ""
    if "## " in ledger_text:
        facts.add("ledger_work")
    if current_claim is None and ledger_text:
        frontier_match = re.search(
            r"(?ms)^## Current delivery frontier\s*$\n(?P<body>.*?)(?=^## |\Z)",
            ledger_text,
        )
        frontier_text = frontier_match.group("body") if frontier_match else ledger_text
        connection = sqlite3.connect(f"file:{state.resolve()}?mode=ro", uri=True)
        connection.row_factory = sqlite3.Row
        try:
            closure_columns = {
                str(row[1]) for row in connection.execute("PRAGMA table_info(closure_gaps)")
            }
            if _table_exists(connection, "closure_gaps") and "gap_id" in closure_columns:
                mentioned = []
                mentioned_executable = False
                for row in connection.execute(
                    """SELECT claim_id, gap_id FROM closure_gaps
                       WHERE lineage_id = ? AND closed_at IS NULL""",
                    (lineage_id,),
                ).fetchall():
                    gap_id = str(row["gap_id"])
                    if (str(row["claim_id"]), gap_id) not in {(claim, gap) for claim, _, gap in owner_wait_gaps} and re.search(
                        r"(?<![A-Za-z0-9_-])" + re.escape(gap_id)
                        + r"(?![A-Za-z0-9_-])",
                        frontier_text,
                    ):
                        mentioned.append(str(row["claim_id"]))
                        if _table_exists(connection, "closure_gap_revisions"):
                            revision = connection.execute(
                                """
                                SELECT proof_route FROM closure_gap_revisions
                                WHERE lineage_id = ? AND claim_id = ? AND gap_id = ?
                                ORDER BY closure_sequence DESC, revision DESC LIMIT 1
                                """,
                                (lineage_id, str(row["claim_id"]), gap_id),
                            ).fetchone()
                            mentioned_executable = mentioned_executable or (
                                revision is not None
                                and bool(str(revision["proof_route"]).strip())
                            )
                if len(set(mentioned)) == 1:
                    facts.update(("closure_ready", "open_gap"))
                    if mentioned_executable:
                        facts.add("executable_route")
        finally:
            connection.close()
    if any(
        line.lstrip().lower().startswith("- blocked:")
        for line in ledger_text.splitlines()
    ):
        facts.add("blocked_ledger")
    # Execution labels only count in the selected frontier or active unchecked items.
    # Historical accepted text and owner-only gap proof cannot create a worker route.
    blocks = _active_work_blocks(ledger_text)
    frontier_match = re.search(r"(?ms)^## Current delivery frontier\s*$\n(.*?)(?=^## |\Z)", ledger_text)
    route_texts = [(reference.split()[0], block) for reference, block in blocks]
    if not blocks:
        route_texts.extend((section.splitlines()[0].removeprefix("## ").strip(), section)
                          for section in re.split(r"(?m)(?=^## )", ledger_text) if section.strip())
    if frontier_match:
        route_texts.append(("", frontier_match.group(1)))
    for owner, route_text in route_texts:
        if owner in owner_wait_claims:
            continue
        for line in route_text.splitlines():
            if not owner or owner == "Current delivery frontier":
                if any(re.search(r"(?<![A-Za-z0-9_-])" + re.escape(identity) + r"(?![A-Za-z0-9_-])", line)
                       for identity in owner_wait_claims | {gap for _, _, gap in owner_wait_gaps}):
                    continue
            assignment = re.match(r"(?i)^\s*- Assignment ([^:]+):\s*\S", line)
            if assignment and assignment.group(1).strip() in terminal_task_ids:
                continue
            if re.search(r"(?i)^\s*- (?:Next executable route|Active work|Assignment [^:]+):\s*\S", line):
                facts.add("executable_route")
    suggestions = workspace / ".de67" / "mutation-suggestions.md"
    if suggestions.is_file():
        pending = suggestions.read_text(encoding="utf-8").partition("## Pending suggestions")[2]
        pending = re.split(r"^#{1,2}\s+", pending, maxsplit=1, flags=re.MULTILINE)[0]
        # Legacy unlabelled entries and explicit [trigger] entries are
        # immediate owner gates.  [defer] entries are proposals for the next
        # regular review and must not retire an otherwise healthy coordinator.
        entries = (
            line[2:].strip()
            for line in pending.splitlines()
            if line.startswith("- ")
        )
        if any(
            entry.lower() != "none."
            and not re.match(
                r"^(?:owner-authorized\s+)?\[defer\]:?\s+",
                entry,
                re.IGNORECASE,
            )
            for entry in entries
        ):
            facts.add("pending_suggestions")
    try:
        specification = resolve(workspace / ".de67")
        open_work = "🔴" in specification.text or bool(_active_work_blocks(ledger_text))
    except SpecificationError:
        open_work = False
    facts.add("red_dfs_work" if open_work else "dfs_complete")
    return frozenset(facts)


def source_from_git(ref: str, path: str) -> bytes:
    result = subprocess.run(
        ["git", "show", f"{ref}:{path}"], capture_output=True, check=False
    )
    if result.returncode != 0:
        raise PolicyError(result.stderr.decode("utf-8", "replace").strip() or "git show failed")
    return result.stdout


def _parse_facts(raw: str) -> frozenset[str]:
    value = json.loads(raw)
    if not isinstance(value, list) or not all(isinstance(item, str) for item in value):
        raise PolicyError("Facts must be a JSON array of strings")
    return frozenset(value)


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    compile_parser = subparsers.add_parser("compile")
    compile_parser.add_argument("--source", type=Path, required=True)
    compile_parser.add_argument("--output", type=Path, required=True)
    decide_parser = subparsers.add_parser("decide")
    decide_parser.add_argument("--policy", type=Path, required=True)
    decide_parser.add_argument("--facts")
    decide_parser.add_argument("--workspace", type=Path)
    decide_parser.add_argument("--state", type=Path)
    decide_parser.add_argument("--lineage")
    decide_parser.add_argument("--now", type=float)
    inspect_parser = subparsers.add_parser("inspect")
    inspect_parser.add_argument("--policy", type=Path, required=True)
    decompile_parser = subparsers.add_parser("decompile")
    decompile_parser.add_argument("--policy", type=Path, required=True)
    guard_parser = subparsers.add_parser("guard")
    guard_parser.add_argument("--candidate", type=Path, required=True)
    guard_parser.add_argument("--contracts", type=Path, required=True)
    guard_parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        if args.command == "compile":
            source = json.loads(args.source.read_text(encoding="utf-8"))
            compiled = compile_policy(source)
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_bytes(compiled)
            print(json.dumps({"bytes": len(compiled), "digest": policy_digest(source)}))
            return 0
        if args.command == "inspect":
            policy = load_policy(args.policy)
            print(json.dumps({"digest": policy_digest(policy), "rules": len(policy["rules"])}))
            return 0
        if args.command == "decompile":
            print(json.dumps(load_policy(args.policy), indent=2, sort_keys=True))
            return 0
        if args.command == "guard":
            candidate = json.loads(args.candidate.read_text(encoding="utf-8"))
            contracts = load_contracts(args.contracts)
            guarded = guard_policy_candidate(candidate, contracts)
            compiled = compile_policy(guarded)
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_bytes(compiled)
            print(json.dumps({
                "bytes": len(compiled), "digest": policy_digest(guarded),
                "rules": len(guarded["rules"]),
            }, sort_keys=True))
            return 0
        policy = load_policy(args.policy)
        if args.facts is not None:
            facts = _parse_facts(args.facts)
        elif args.workspace and args.state and args.lineage:
            import time
            observed_at = args.now if args.now is not None else time.time()
            facts = workspace_facts(
                args.workspace.resolve(), args.state.resolve(), args.lineage, now=observed_at
            )
        else:
            raise PolicyError("decide requires --facts or workspace, state, and lineage")
        decision = decide(policy, facts)
        payload = decision_json(decision, facts)
        if decision.action == "spawn_worker" and args.facts is None:
            if not (args.state and args.lineage and args.workspace):
                raise PolicyError("spawn_worker requires workspace, state, and lineage")
            payload["worker_spawns"] = unbound_worker_spawns(
                args.workspace.resolve(), args.state.resolve(), args.lineage
            )
            payload["parallel_dispatch"] = (
                "Assign one distinct worker to each listed independent task before waiting."
            )
            payload["coordinator_next_action"] = (
                "Spawn every listed worker, then continue live coordination. Call wait_agent "
                "when no useful coordination decision remains. "
                "Do not finish the coordinator turn while a worker result is outstanding."
            )
        print(json.dumps(payload, sort_keys=True))
        return 0
    except (OSError, ValueError, json.JSONDecodeError, sqlite3.Error, PolicyError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
