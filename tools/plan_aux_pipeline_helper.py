#!/usr/bin/env python3

"""Small helper for the Plan/Aux packaging workflow.

V1 is deliberately narrow:
- print a normalized contract for review before canon edits
- merge an explicit corrections overlay into that contract
- emit reviewer-visible markdown snippets for the aux doc and canon files
- patch known existing canon headings in place when the heading already exists

It still does not attempt broad freeform canon mutation or automatic Andi handoff
packet generation. Those boundaries are intentional so the workflow stays visible
instead of turning into hidden magic.
"""

from __future__ import annotations

import argparse
import copy
import json
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable

REQUEST_KINDS = {
    "proposed-item",
    "greenlight",
    "parked-request",
    "reclassification",
}
CLASSIFICATIONS = {
    "active",
    "parked",
    "bottom-of-stack",
}
TESTING_NEEDED = {"yes", "no"}

REPO_CONFIG = {
    "files": {
        "roadmap": {"path": "Plan.md", "role": "source-of-truth", "required": True},
        "queue": {
            "path": "TODO.md",
            "role": "current work queue",
            "required": True,
            "include_states": ["active"],
        },
        "success": {"path": "SUCCESS.md", "role": "success ledger", "required": True},
        "testing": {
            "path": "TESTING.md",
            "role": "testing ledger",
            "required": True,
            "update_when": [
                "validation obligations change",
                "evidence expectations change",
                "pending probes change",
            ],
        },
        "aux": {"dir": "doc", "role": "auxiliary docs", "required": True},
    },
    "authority": {
        "canonical_role": "roadmap",
        "rule": "Plan.md wins truth disagreements",
    },
    "classification_labels": {
        "active": "ACTIVE / GREENLIT",
        "parked": "PARKED",
        "bottom-of-stack": "GREENLIT / BOTTOM-OF-STACK",
    },
    "handoff": {
        "enabled": False,
        "notes": "deliberately deferred until the main canon path is real",
    },
    "patching": {
        "mode": "known-existing-headings-only",
        "notes": [
            "Plan/Aux pipeline helper patches only known canon anchors in place.",
            "If a target heading does not already exist, use emit output for manual review instead.",
        ],
    },
}

SCHEMA_EXAMPLE = {
    "title": "Example lane title",
    "request_kind": "greenlight",
    "summary": "One-paragraph contract for an already-understood item.",
    "scope": [
        "Do the narrow thing that is actually agreed.",
        "Keep the workflow visible and bounded.",
    ],
    "non_goals": [
        "Do not silently redesign the workflow.",
    ],
    "success_state": [
        "A visible completion condition that a reviewer can recognize.",
    ],
    "testing_impact": {
        "needed": "yes",
        "notes": [
            "Name the smallest honest validation for this item.",
        ],
    },
    "classification": "active",
    "status_label": "ACTIVE / GREENLIT TOOLING",
    "aux_doc": {
        "path": "doc/example-lane-2026-04-19.md",
        "status": "active auxiliary contract",
    },
    "queue": {
        "current_target": [
            "The next concrete executor step for the active lane.",
        ],
        "out_of_scope": [
            "Nearby tempting drift that should stay out.",
        ],
    },
    "testing": {
        "current_state": [
            "The current evidence state for this item.",
        ],
        "pending_probes": [
            "The next evidence question, if any.",
        ],
    },
    "open_questions": [],
}


@dataclass
class ValidationError(Exception):
    messages: list[str]

    def __str__(self) -> str:
        return "\n".join(self.messages)


@dataclass
class ApplyResult:
    written_files: list[Path]


def load_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        data = json.load(handle)
    if not isinstance(data, dict):
        raise ValidationError([f"{path} must contain a top-level JSON object."])
    return data


def dump_json(data: dict[str, Any], path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        json.dump(data, handle, indent=2, ensure_ascii=False)
        handle.write("\n")


def deep_merge(base: Any, overlay: Any) -> Any:
    if isinstance(base, dict) and isinstance(overlay, dict):
        merged = {key: copy.deepcopy(value) for key, value in base.items()}
        for key, value in overlay.items():
            if key in merged:
                merged[key] = deep_merge(merged[key], value)
            else:
                merged[key] = copy.deepcopy(value)
        return merged
    return copy.deepcopy(overlay)


def require_string(spec: dict[str, Any], key: str, errors: list[str]) -> str:
    value = spec.get(key)
    if not isinstance(value, str) or not value.strip():
        errors.append(f"{key} must be a non-empty string.")
        return ""
    return value.strip()


def optional_string(spec: dict[str, Any], key: str, errors: list[str]) -> str:
    value = spec.get(key, "")
    if value in (None, ""):
        return ""
    if not isinstance(value, str) or not value.strip():
        errors.append(f"{key} must be a non-empty string when present.")
        return ""
    return value.strip()


def require_string_list(container: dict[str, Any], key: str, errors: list[str]) -> list[str]:
    value = container.get(key)
    if not isinstance(value, list) or not value:
        errors.append(f"{key} must be a non-empty list of strings.")
        return []
    cleaned: list[str] = []
    for index, item in enumerate(value):
        if not isinstance(item, str) or not item.strip():
            errors.append(f"{key}[{index}] must be a non-empty string.")
        else:
            cleaned.append(item.strip())
    return cleaned


def optional_string_list(container: dict[str, Any], key: str, errors: list[str]) -> list[str]:
    value = container.get(key, [])
    if value in (None, ""):
        return []
    if not isinstance(value, list):
        errors.append(f"{key} must be a list of strings when present.")
        return []
    cleaned: list[str] = []
    for index, item in enumerate(value):
        if not isinstance(item, str) or not item.strip():
            errors.append(f"{key}[{index}] must be a non-empty string.")
        else:
            cleaned.append(item.strip())
    return cleaned


def validate_spec(spec: dict[str, Any]) -> dict[str, Any]:
    errors: list[str] = []

    title = require_string(spec, "title", errors)
    request_kind = require_string(spec, "request_kind", errors)
    summary = require_string(spec, "summary", errors)
    scope = require_string_list(spec, "scope", errors)
    non_goals = require_string_list(spec, "non_goals", errors)
    success_state = require_string_list(spec, "success_state", errors)
    classification = require_string(spec, "classification", errors)
    status_override = optional_string(spec, "status_label", errors)

    if request_kind and request_kind not in REQUEST_KINDS:
        errors.append(
            f"request_kind must be one of: {', '.join(sorted(REQUEST_KINDS))}."
        )
    if classification and classification not in CLASSIFICATIONS:
        errors.append(
            f"classification must be one of: {', '.join(sorted(CLASSIFICATIONS))}."
        )

    testing_impact = spec.get("testing_impact")
    if not isinstance(testing_impact, dict):
        errors.append("testing_impact must be an object.")
        testing_impact = {}
    testing_needed = require_string(testing_impact, "needed", errors)
    testing_notes = require_string_list(testing_impact, "notes", errors)
    if testing_needed and testing_needed not in TESTING_NEEDED:
        errors.append("testing_impact.needed must be 'yes' or 'no'.")

    aux_doc = spec.get("aux_doc")
    if not isinstance(aux_doc, dict):
        errors.append("aux_doc must be an object.")
        aux_doc = {}
    aux_doc_path = require_string(aux_doc, "path", errors)
    aux_doc_status = aux_doc.get("status")
    if aux_doc_status is not None and (
        not isinstance(aux_doc_status, str) or not aux_doc_status.strip()
    ):
        errors.append("aux_doc.status must be a non-empty string when present.")
    aux_doc_status = (aux_doc_status or "").strip()
    if aux_doc_path and not aux_doc_path.startswith("doc/"):
        errors.append("aux_doc.path must live under doc/ for this repo.")

    queue = spec.get("queue", {})
    if queue in (None, ""):
        queue = {}
    if not isinstance(queue, dict):
        errors.append("queue must be an object when present.")
        queue = {}
    queue_current = optional_string_list(queue, "current_target", errors)
    queue_out_of_scope = optional_string_list(queue, "out_of_scope", errors)

    testing = spec.get("testing", {})
    if testing in (None, ""):
        testing = {}
    if not isinstance(testing, dict):
        errors.append("testing must be an object when present.")
        testing = {}
    testing_current_state = optional_string_list(testing, "current_state", errors)
    testing_pending_probes = optional_string_list(testing, "pending_probes", errors)

    open_questions = optional_string_list(spec, "open_questions", errors)

    if classification == "active" and not queue_current:
        errors.append("queue.current_target is required for active items.")

    if errors:
        raise ValidationError(errors)

    normalized = {
        "title": title,
        "request_kind": request_kind,
        "summary": summary,
        "scope": scope,
        "non_goals": non_goals,
        "success_state": success_state,
        "testing_impact": {
            "needed": testing_needed,
            "notes": testing_notes,
        },
        "classification": classification,
        "status_label": status_override,
        "aux_doc": {
            "path": aux_doc_path,
            "status": aux_doc_status,
        },
        "queue": {
            "current_target": queue_current,
            "out_of_scope": queue_out_of_scope,
        },
        "testing": {
            "current_state": testing_current_state,
            "pending_probes": testing_pending_probes,
        },
        "open_questions": open_questions,
    }
    return normalized


def bullet_lines(items: list[str], checkbox: str | None = None, indent: str = "") -> str:
    prefix = "- " if checkbox is None else f"- [{checkbox}] "
    return "\n".join(f"{indent}{prefix}{item}" for item in items)


def status_label(classification: str) -> str:
    return REPO_CONFIG["classification_labels"][classification]


def effective_status_label(spec: dict[str, Any]) -> str:
    return spec["status_label"] or status_label(spec["classification"])


def default_aux_status(spec: dict[str, Any]) -> str:
    if spec["aux_doc"]["status"]:
        return spec["aux_doc"]["status"]
    mapping = {
        "active": "active auxiliary contract",
        "parked": "parked request",
        "bottom-of-stack": "greenlit bottom-of-stack contract",
    }
    return mapping[spec["classification"]]


def build_patch_matrix(spec: dict[str, Any]) -> list[dict[str, Any]]:
    classification = spec["classification"]
    testing_needed = spec["testing_impact"]["needed"] == "yes"
    return [
        {
            "role": "aux",
            "action": "create-or-update",
            "target": spec["aux_doc"]["path"],
            "source_fields": [
                "title",
                "summary",
                "scope",
                "non_goals",
                "success_state",
                "testing_impact",
                "classification",
            ],
        },
        {
            "role": "roadmap",
            "action": "patch-existing-section",
            "target": "Plan.md",
            "source_fields": [
                "title",
                "summary",
                "scope",
                "non_goals",
                "classification",
                "queue.current_target",
            ],
        },
        {
            "role": "success",
            "action": "patch-existing-section",
            "target": "SUCCESS.md",
            "source_fields": ["title", "success_state", "classification"],
        },
        {
            "role": "queue",
            "action": "patch-current-active-block" if classification == "active" else "no-change",
            "target": "TODO.md",
            "source_fields": ["title", "classification", "queue.current_target"],
        },
        {
            "role": "testing",
            "action": "patch-existing-section-and-probes" if testing_needed else "no-change",
            "target": "TESTING.md",
            "source_fields": [
                "title",
                "testing_impact",
                "testing.current_state",
                "testing.pending_probes",
            ],
        },
    ]


def render_repo_config() -> str:
    return json.dumps(REPO_CONFIG, indent=2, ensure_ascii=False)


def render_contract(spec: dict[str, Any]) -> str:
    lines = [
        f"title: {spec['title']}",
        f"request_kind: {spec['request_kind']}",
        f"classification: {spec['classification']}",
        f"status_label: {effective_status_label(spec)}",
        f"aux_doc: {spec['aux_doc']['path']}",
        f"summary: {spec['summary']}",
        "scope:",
        *(f"  - {item}" for item in spec["scope"]),
        "non_goals:",
        *(f"  - {item}" for item in spec["non_goals"]),
        "success_state:",
        *(f"  - {item}" for item in spec["success_state"]),
        "testing_impact:",
        f"  needed: {spec['testing_impact']['needed']}",
        "  notes:",
        *(f"    - {item}" for item in spec["testing_impact"]["notes"]),
        "open_questions:",
    ]
    if spec["open_questions"]:
        lines.extend(f"  - {item}" for item in spec["open_questions"])
    else:
        lines.append("  - none")
    return "\n".join(lines)


def render_patch_matrix(spec: dict[str, Any]) -> str:
    lines = ["patch_matrix:"]
    for row in build_patch_matrix(spec):
        lines.append(f"  {row['role']}:")
        lines.append(f"    action: {row['action']}")
        lines.append(f"    target: {row['target']}")
        lines.append(
            "    source_fields: [" + ", ".join(row["source_fields"]) + "]"
        )
    return "\n".join(lines)


def render_aux_doc(spec: dict[str, Any]) -> str:
    status = default_aux_status(spec)
    blocks = [
        f"# {spec['title']}",
        "",
        f"Status: {status}",
        "",
        spec["summary"],
        "",
        "## Request kind",
        "",
        f"- {spec['request_kind']}",
        "",
        "## Classification",
        "",
        f"- {spec['classification']}",
        "",
        "## Scope",
        "",
        bullet_lines(spec["scope"]),
        "",
        "## Non-goals",
        "",
        bullet_lines(spec["non_goals"]),
        "",
        "## Success state",
        "",
        bullet_lines(spec["success_state"], checkbox=" "),
        "",
        "## Testing impact",
        "",
        f"- needed: {spec['testing_impact']['needed']}",
        bullet_lines(spec["testing_impact"]["notes"]),
    ]
    if spec["open_questions"]:
        blocks.extend([
            "",
            "## Open questions",
            "",
            bullet_lines(spec["open_questions"]),
        ])
    return "\n".join(blocks).rstrip() + "\n"


def render_plan_section_body(spec: dict[str, Any]) -> str:
    lines = [
        f"**Status:** {effective_status_label(spec)}",
        "",
        spec["summary"],
        "",
        "What this item should do:",
        bullet_lines(spec["scope"]),
        "",
        "Non-goals:",
        bullet_lines(spec["non_goals"]),
        "",
        f"Canonical contract lives at `{spec['aux_doc']['path']}`.",
    ]
    return "\n".join(lines).rstrip()


def render_plan_current_target_body(spec: dict[str, Any]) -> str:
    lines = [
        f"- advance the active **{spec['title']}** lane",
        "- current focus:",
        *(f"  - {item}" for item in spec["queue"]["current_target"]),
    ]
    if spec["queue"]["out_of_scope"]:
        lines.extend([
            "- keep out of scope:",
            *(f"  - {item}" for item in spec["queue"]["out_of_scope"]),
        ])
    return "\n".join(lines).rstrip()


def render_plan_snippet(spec: dict[str, Any]) -> str:
    lines = [
        f"## {spec['title']}",
        "",
        render_plan_section_body(spec),
    ]
    return "\n".join(lines).rstrip() + "\n"


def render_success_section(spec: dict[str, Any]) -> str:
    lines = [
        f"## {spec['title']}",
        "",
        f"Status: {effective_status_label(spec)}",
        "",
        "Success state:",
        bullet_lines(spec["success_state"], checkbox=" "),
        "",
        "Notes:",
        f"- Canonical contract lives at `{spec['aux_doc']['path']}`.",
        "- Generated via `tools/plan_aux_pipeline_helper.py`; review before paste.",
    ]
    return "\n".join(lines).rstrip() + "\n"


def render_success_snippet(spec: dict[str, Any]) -> str:
    return render_success_section(spec)


def render_todo_now_body(spec: dict[str, Any]) -> str:
    if spec["classification"] != "active":
        return ""
    lines = [
        f"Active lane: **{spec['title']}**.",
        "",
        "Current target:",
    ]
    lines.extend(
        f"{index}. {item}" for index, item in enumerate(spec["queue"]["current_target"], start=1)
    )
    if spec["queue"]["out_of_scope"]:
        lines.extend([
            "",
            "Out of scope right now:",
            bullet_lines(spec["queue"]["out_of_scope"]),
        ])
    return "\n".join(lines).rstrip()


def render_todo_snippet(spec: dict[str, Any]) -> str:
    body = render_todo_now_body(spec)
    return body + ("\n" if body else "")


def render_testing_section_body(spec: dict[str, Any]) -> str:
    current_state = spec["testing"]["current_state"] or [
        "Add the current evidence state here before pasting into TESTING.md.",
    ]
    pending_probes = spec["testing"]["pending_probes"]
    lines = [
        "Current honest state:",
        bullet_lines(current_state),
        "",
        "Pending probes:",
    ]
    if pending_probes:
        lines.append(bullet_lines(pending_probes))
    else:
        lines.append("- none")
    lines.extend([
        "",
        "Testing impact notes:",
        bullet_lines(spec["testing_impact"]["notes"]),
    ])
    return "\n".join(lines).rstrip()


def render_testing_snippet(spec: dict[str, Any]) -> str:
    if spec["testing_impact"]["needed"] != "yes":
        return ""
    lines = [
        f"### {spec['title']}",
        "",
        render_testing_section_body(spec),
    ]
    return "\n".join(lines).rstrip() + "\n"


def render_pending_probes_body(spec: dict[str, Any]) -> str:
    pending_probes = spec["testing"]["pending_probes"]
    if pending_probes:
        return bullet_lines(pending_probes)
    return "- none"


def render_preview_packet(spec: dict[str, Any]) -> str:
    sections = [
        "# Repo config",
        "",
        "```json",
        render_repo_config(),
        "```",
        "",
        "# Contract preview",
        "",
        "```yaml",
        render_contract(spec),
        "```",
        "",
        "# Patch matrix",
        "",
        "```yaml",
        render_patch_matrix(spec),
        "```",
    ]
    return "\n".join(sections).rstrip() + "\n"


def write_outputs(spec: dict[str, Any], out_dir: Path, force: bool) -> list[Path]:
    out_dir.mkdir(parents=True, exist_ok=True)
    outputs = {
        "00_preview.md": render_preview_packet(spec),
        Path(spec["aux_doc"]["path"]).name: render_aux_doc(spec),
        "plan.snippet.md": render_plan_snippet(spec),
        "success.snippet.md": render_success_snippet(spec),
    }
    todo_snippet = render_todo_snippet(spec)
    if todo_snippet:
        outputs["todo.snippet.md"] = todo_snippet
    testing_snippet = render_testing_snippet(spec)
    if testing_snippet:
        outputs["testing.snippet.md"] = testing_snippet

    written: list[Path] = []
    for name, content in outputs.items():
        path = out_dir / name
        if path.exists() and not force:
            raise ValidationError([
                f"Refusing to overwrite {path}. Use --force if that is intentional."
            ])
        path.write_text(content, encoding="utf-8")
        written.append(path)
    return written


def ensure_trailing_newline(text: str) -> str:
    return text.rstrip("\n") + "\n"


def split_body_lines(text: str) -> list[str]:
    stripped = text.rstrip("\n")
    if not stripped:
        return []
    return stripped.splitlines()


def find_unique_index(
    lines: list[str],
    predicate: Callable[[str], bool],
    description: str,
) -> int:
    matches = [index for index, line in enumerate(lines) if predicate(line)]
    if not matches:
        raise ValidationError([f"Could not find {description} in target file."])
    if len(matches) > 1:
        raise ValidationError([f"Found multiple matches for {description}; bounded patching needs a unique anchor."])
    return matches[0]


def find_section_end(lines: list[str], start_index: int, stop_prefixes: tuple[str, ...]) -> int:
    for index in range(start_index + 1, len(lines)):
        if any(lines[index].startswith(prefix) for prefix in stop_prefixes):
            return index
    return len(lines)


def replace_body_after_heading(
    text: str,
    heading_predicate: Callable[[str], bool],
    description: str,
    new_body: str,
    stop_prefixes: tuple[str, ...],
) -> str:
    lines = text.splitlines()
    start_index = find_unique_index(lines, heading_predicate, description)
    end_index = find_section_end(lines, start_index, stop_prefixes)
    body_lines = split_body_lines(new_body)
    tail = lines[end_index:]
    if tail and tail[0] == "":
        tail = tail[1:]
    new_lines = lines[: start_index + 1] + [""] + body_lines + ([""] + tail if tail else [])
    return ensure_trailing_newline("\n".join(new_lines))


def replace_section(
    text: str,
    heading_predicate: Callable[[str], bool],
    description: str,
    replacement: str,
    stop_prefixes: tuple[str, ...],
) -> str:
    lines = text.splitlines()
    start_index = find_unique_index(lines, heading_predicate, description)
    end_index = find_section_end(lines, start_index, stop_prefixes)
    replacement_lines = split_body_lines(replacement)
    tail = lines[end_index:]
    if tail and tail[0] == "":
        tail = tail[1:]
    new_lines = lines[:start_index] + replacement_lines + ([""] + tail if tail else [])
    return ensure_trailing_newline("\n".join(new_lines))


def replace_block_after_anchor(
    text: str,
    anchor_predicate: Callable[[str], bool],
    anchor_description: str,
    new_body: str,
    end_predicate: Callable[[str], bool],
    end_description: str,
    allow_eof: bool = False,
) -> str:
    lines = text.splitlines()
    start_index = find_unique_index(lines, anchor_predicate, anchor_description)
    end_matches = [
        index for index in range(start_index + 1, len(lines)) if end_predicate(lines[index])
    ]
    if not end_matches:
        if allow_eof:
            end_index = len(lines)
        else:
            raise ValidationError([
                f"Could not find {end_description} after {anchor_description} in target file."
            ])
    else:
        end_index = end_matches[0]
    body_lines = split_body_lines(new_body)
    tail = lines[end_index:]
    if tail and tail[0] == "":
        tail = tail[1:]
    new_lines = lines[: start_index + 1] + [""] + body_lines + ([""] + tail if tail else [])
    return ensure_trailing_newline("\n".join(new_lines))


def apply_spec(spec: dict[str, Any], root: Path) -> ApplyResult:
    root = root.resolve()
    plan_path = root / REPO_CONFIG["files"]["roadmap"]["path"]
    todo_path = root / REPO_CONFIG["files"]["queue"]["path"]
    success_path = root / REPO_CONFIG["files"]["success"]["path"]
    testing_path = root / REPO_CONFIG["files"]["testing"]["path"]
    aux_path = root / spec["aux_doc"]["path"]

    missing = [
        str(path)
        for path in (plan_path, todo_path, success_path, testing_path)
        if not path.exists()
    ]
    if missing:
        raise ValidationError([f"Missing required repo files under {root}: {', '.join(missing)}"])

    rendered_aux = render_aux_doc(spec)

    plan_text = plan_path.read_text(encoding="utf-8")
    plan_text = replace_body_after_heading(
        plan_text,
        lambda line: line.startswith("## ") and spec["title"] in line,
        f"Plan section for {spec['title']}",
        render_plan_section_body(spec),
        ("## ",),
    )
    if spec["classification"] == "active":
        plan_text = replace_block_after_anchor(
            plan_text,
            lambda line: line == "Current target:",
            "Plan current target anchor",
            render_plan_current_target_body(spec),
            lambda line: line == "Explicit greenlit backlog behind the current slice:",
            "Plan backlog anchor",
        )

    success_text = success_path.read_text(encoding="utf-8")
    success_text = replace_section(
        success_text,
        lambda line: line == f"## {spec['title']}",
        f"SUCCESS section for {spec['title']}",
        render_success_section(spec),
        ("## ",),
    )

    todo_text: str | None = None
    if spec["classification"] == "active":
        todo_text = todo_path.read_text(encoding="utf-8")
        todo_text = replace_block_after_anchor(
            todo_text,
            lambda line: line == "## Now",
            "TODO now anchor",
            render_todo_now_body(spec),
            lambda line: False,
            "TODO end-of-file anchor",
            allow_eof=True,
        )

    testing_text: str | None = None
    if spec["testing_impact"]["needed"] == "yes":
        testing_text = testing_path.read_text(encoding="utf-8")
        testing_text = replace_body_after_heading(
            testing_text,
            lambda line: line == f"### {spec['title']}",
            f"TESTING section for {spec['title']}",
            render_testing_section_body(spec),
            ("### ", "## "),
        )
        testing_text = replace_block_after_anchor(
            testing_text,
            lambda line: line == "## Pending probes",
            "TESTING pending probes anchor",
            render_pending_probes_body(spec),
            lambda line: line == "---",
            "TESTING reusable-commands divider",
        )

    aux_path.parent.mkdir(parents=True, exist_ok=True)
    aux_path.write_text(rendered_aux, encoding="utf-8")
    plan_path.write_text(plan_text, encoding="utf-8")
    success_path.write_text(success_text, encoding="utf-8")
    written_files: list[Path] = [aux_path, plan_path, success_path]

    if todo_text is not None:
        todo_path.write_text(todo_text, encoding="utf-8")
        written_files.append(todo_path)

    if testing_text is not None:
        testing_path.write_text(testing_text, encoding="utf-8")
        written_files.append(testing_path)

    return ApplyResult(written_files=written_files)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Plan/Aux packaging helper for contract preview, correction merge, bounded canon patching, and snippet emission."
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    subparsers.add_parser("schema", help="Print an example JSON spec.")

    show = subparsers.add_parser("show", help="Validate a spec and print the review packet.")
    show.add_argument("spec", type=Path)

    merge = subparsers.add_parser(
        "merge-corrections",
        help="Deep-merge an explicit corrections overlay into a base spec.",
    )
    merge.add_argument("spec", type=Path)
    merge.add_argument("corrections", type=Path)
    merge.add_argument("--output", "-o", type=Path, required=True)

    emit = subparsers.add_parser(
        "emit",
        help="Validate a spec and emit aux/canon snippet files into a review directory.",
    )
    emit.add_argument("spec", type=Path)
    emit.add_argument("--out-dir", type=Path, required=True)
    emit.add_argument("--force", action="store_true")

    apply_cmd = subparsers.add_parser(
        "apply",
        help="Patch known existing canon headings in place under a repo root.",
    )
    apply_cmd.add_argument("spec", type=Path)
    apply_cmd.add_argument("--root", type=Path, default=Path("."))

    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    try:
        if args.command == "schema":
            json.dump(SCHEMA_EXAMPLE, sys.stdout, indent=2, ensure_ascii=False)
            sys.stdout.write("\n")
            return 0

        if args.command == "show":
            spec = validate_spec(load_json(args.spec))
            sys.stdout.write(render_preview_packet(spec))
            return 0

        if args.command == "merge-corrections":
            base = load_json(args.spec)
            corrections = load_json(args.corrections)
            merged = deep_merge(base, corrections)
            validate_spec(merged)
            dump_json(merged, args.output)
            print(f"wrote merged spec to {args.output}")
            return 0

        if args.command == "emit":
            spec = validate_spec(load_json(args.spec))
            written = write_outputs(spec, args.out_dir, args.force)
            for path in written:
                print(path)
            return 0

        if args.command == "apply":
            spec = validate_spec(load_json(args.spec))
            result = apply_spec(spec, args.root)
            for path in result.written_files:
                print(path)
            return 0

        parser.error("unknown command")
        return 2
    except ValidationError as exc:
        for message in exc.messages:
            print(f"error: {message}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
