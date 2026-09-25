#!/usr/bin/env python3
"""Check compiled Phase-3 behavior against the contract corpus and report its encoded size."""

from __future__ import annotations

import argparse
import importlib.util
import json
import sys
import zlib
from pathlib import Path
from typing import Any, Mapping, Sequence


ROOT = Path(__file__).resolve().parents[1]
KERNEL_PATH = Path(__file__).resolve().with_name("policy_kernel.py")
SPEC = importlib.util.spec_from_file_location("de67_policy_kernel_compare", KERNEL_PATH)
assert SPEC is not None and SPEC.loader is not None
kernel = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = kernel
SPEC.loader.exec_module(kernel)


class ComparisonError(RuntimeError):
    pass


def legacy_trace_accepts(events: Sequence[Mapping[str, Any]]) -> bool:
    """Model the main branch's enforced trace gates, not its prose aspirations."""
    attempts: dict[str, bool] = {}
    for event in events:
        kind = event.get("event")
        task_id = str(event.get("task_id", ""))
        if kind == "task_started":
            if not task_id or task_id in attempts:
                return False
            attempts[task_id] = True
        elif kind in {"task_completed", "task_finding", "task_abandoned"}:
            if not attempts.get(task_id):
                return False
            attempts[task_id] = False
    return True


def compiled_trace_accepts(policy: Mapping[str, Any], events: Sequence[Mapping[str, Any]]) -> bool:
    try:
        kernel.validate_trace(policy, events)
        return True
    except kernel.PolicyError:
        return False


def compare(
    policy_path: Path,
    contracts_path: Path,
) -> dict[str, Any]:
    policy = kernel.load_policy(policy_path)
    contracts = json.loads(contracts_path.read_text(encoding="utf-8"))
    decisions: list[dict[str, Any]] = []
    for case in contracts["decision_cases"]:
        actual = kernel.decide(policy, case["facts"]).action
        decisions.append({
            "name": case["name"], "expected": case["action"], "actual": actual,
            "match": actual == case["action"],
        })
    traces: list[dict[str, Any]] = []
    for case in contracts["trace_cases"]:
        main = legacy_trace_accepts(case["events"])
        lab = compiled_trace_accepts(policy, case["events"])
        traces.append({
            "name": case["name"], "main": main, "lab": lab,
            "expected_main": case["main"], "expected_lab": case["lab"],
            "match": main == case["main"] and lab == case["lab"],
        })
    compiled_bytes = policy_path.stat().st_size
    normalized_source = kernel.canonical_bytes(policy)
    instruction_tape = kernel.symbol_codec.encode(kernel._lower_policy(policy))
    return {
        "compiled_policy_bytes": compiled_bytes,
        "normalized_source_bytes": len(normalized_source),
        "instruction_tape_bytes": len(instruction_tape),
        "plain_json_compiled_bytes": kernel.HEADER.size + len(zlib.compress(normalized_source, 9)),
        "decision_cases": decisions,
        "trace_cases": traces,
        "passed": all(case["match"] for case in decisions + traces),
    }


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--policy", type=Path, required=True)
    parser.add_argument("--contracts", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        report = compare(args.policy, args.contracts)
        print(json.dumps(report, indent=2, sort_keys=True))
        return 0 if report["passed"] else 1
    except (OSError, ValueError, json.JSONDecodeError, kernel.PolicyError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
