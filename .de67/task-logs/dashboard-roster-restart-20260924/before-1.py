#!/usr/bin/env python3
"""Optional read-only de67 workspace dashboard."""

from __future__ import annotations

import argparse
import hashlib
import html
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import math
import os
from pathlib import Path
import re
import shlex
import sqlite3
import subprocess
import sys
import threading
import time
from typing import Any
from urllib.parse import quote
import socketserver


def _escape(value: Any) -> str:
    return html.escape(str(value), quote=True)


def _inline(text: str) -> str:
    escaped = _escape(text)
    escaped = re.sub(r"`([^`]+)`", r"<code>\1</code>", escaped)
    escaped = re.sub(r"\*\*([^*]+)\*\*", r"<strong>\1</strong>", escaped)
    return escaped


def render_markdown(text: str) -> str:
    """Render a deliberately small Markdown subset; raw HTML is always escaped."""
    result: list[str] = []
    paragraph: list[str] = []
    in_code = False
    code: list[str] = []
    list_open = False

    def flush_paragraph() -> None:
        if paragraph:
            result.append(f"<p>{_inline(' '.join(paragraph))}</p>")
            paragraph.clear()

    def close_list() -> None:
        nonlocal list_open
        if list_open:
            result.append("</ul>")
            list_open = False

    for line in text.splitlines():
        if re.fullmatch(r"\s*<!--\s*DE67:[^<>]*-->\s*", line):
            flush_paragraph()
            close_list()
            continue
        if line.lstrip().startswith("```"):
            flush_paragraph()
            close_list()
            if in_code:
                result.append(f"<pre><code>{_escape(chr(10).join(code))}</code></pre>")
                code.clear()
            in_code = not in_code
            continue
        if in_code:
            code.append(line)
            continue
        heading = re.match(r"^(#{1,6})\s+(.*)$", line)
        item = re.match(r"^\s*[-*]\s+(.*)$", line)
        if heading:
            flush_paragraph()
            close_list()
            level = len(heading.group(1))
            result.append(f"<h{level}>{_inline(heading.group(2))}</h{level}>")
        elif item:
            flush_paragraph()
            if not list_open:
                result.append("<ul>")
                list_open = True
            body = item.group(1)
            checked = re.match(r"^\[([ xX])\]\s*(.*)$", body)
            if checked:
                mark = "☑" if checked.group(1).lower() == "x" else "☐"
                body = f"{mark} {checked.group(2)}"
            result.append(f"<li>{_inline(body)}</li>")
        elif not line.strip():
            flush_paragraph()
            close_list()
        else:
            close_list()
            paragraph.append(line.strip())
    if in_code:
        result.append(f"<pre><code>{_escape(chr(10).join(code))}</code></pre>")
    flush_paragraph()
    close_list()
    return "\n".join(result)


def render_ledger_section(text: str) -> str:
    """Keep each top-level ledger item inside one continuous decorative rail."""
    if re.search(r"^#{1,6}\s+", text, re.M):
        return render_markdown(text) if text.strip() else "<p>None.</p>"
    blocks: list[list[str]] = []
    preface: list[str] = []
    current: list[str] | None = None
    for line in text.splitlines():
        if re.match(r"^[-*]\s+", line):
            if current is not None:
                blocks.append(current)
            current = [re.sub(r"^[-*]\s+", "", line, count=1)]
        elif current is None:
            preface.append(line)
        else:
            current.append(line[2:] if line.startswith("  ") else line)
    if current is not None:
        blocks.append(current)
    if not blocks:
        return render_markdown(text) if text.strip() else "<p>None.</p>"
    rendered: list[str] = []
    if any(line.strip() for line in preface):
        rendered.append(render_markdown("\n".join(preface)))
    for block in blocks:
        title = block[0]
        checked = re.match(r"^\[([ xX])\]\s*(.*)$", title)
        if checked:
            title = checked.group(2)
        detail = render_markdown("\n".join(block[1:])) if len(block) > 1 else ""
        rendered.append(
            '<article class="ledger-item">'
            f'<div class="ledger-title">{_inline(title)}</div>{detail}</article>'
        )
    return "".join(rendered)


def read_sidecar(script: Path, workspace: Path, state: Path, claim: str) -> dict[str, Any]:
    completed = subprocess.run(
        [
            sys.executable,
            str(script),
            "--workspace", str(workspace),
            "--state", str(state),
            "--claim", claim,
            "--json",
        ],
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    if completed.returncode != 0:
        raise RuntimeError(completed.stdout.strip() or completed.stderr.strip()
                           or "trajectory sidecar failed")
    value = json.loads(completed.stdout)
    if not isinstance(value, dict) or not isinstance(value.get("gaps"), list):
        raise ValueError("trajectory sidecar returned an invalid report")
    return value


def render_trajectory(report: dict[str, Any]) -> str:
    raw_gaps = report.get("gaps")
    gaps = [
        gap if isinstance(gap, dict) else {"summary": str(gap)}
        for gap in raw_gaps
    ] if isinstance(raw_gaps, list) else []
    raw_subtasks = report.get("subtasks")
    subtasks = [
        item if isinstance(item, dict) else {"summary": str(item)}
        for item in raw_subtasks
    ] if isinstance(raw_subtasks, list) else []
    axes = subtasks or gaps
    if not axes:
        return '<section class="trajectory"><h2>Trajectory sidecar</h2><p class="subtle">No closure trajectory.</p></section>'
    return (
        '<section class="trajectory"><h2>Trajectory sidecar</h2>'
        f'{render_attention_spider(report, axes, gaps, bool(subtasks))}'
        '</section>'
    )


def render_fratbro_status(value: dict[str, Any]) -> str:
    summary = value.get("summary") if isinstance(value, dict) else None
    if isinstance(summary, dict) and "headline" in summary:
        fields = "".join(
            f'<div class="brief-field"><small>{label}</small><p>{_escape(summary.get(key, ""))}</p></div>'
            for key, label in (("changed", "What changed"), ("next", "Next"), ("snag", "Obstacle"))
            if summary.get(key)
        )
        return ('<section class="fratbro"><div class="eyebrow">BRIEFING</div>'
                f'<h2>{_escape(summary.get("headline", ""))}</h2><div class="brief-grid">{fields}</div></section>')
    if isinstance(summary, dict):
        parts = [str(summary.get(key, "")).strip()
                 for key in ("cooking", "changed", "snag", "next", "health")
                 if str(summary.get(key, "")).strip()]
    elif isinstance(summary, str):
        parts = re.split(r"(?<=[.!?])\s+(?=[A-Z])", summary.strip())
    else:
        parts = []
    if not parts:
        return '<section class="fratbro"><h2>Briefing</h2><p class="subtle">Waiting for a fresh briefing.</p></section>'
    lead = parts[0]
    return ('<section class="fratbro"><div class="eyebrow">BRIEFING</div>'
            f'<h2>At a glance</h2><p class="brief-lead">{_escape(lead)}</p>'
            f'<a class="text-link" href="/briefing">Read the full briefing ↗</a></section>')


def worker_dot_positions(count: int) -> list[tuple[float, float]]:
    """Owner-requested display capacity: 12 dots; excess is labelled separately."""
    visible = min(max(0, count), 12)
    if visible == 1:
        return [(0.0, 0.0)]
    if visible > 6:
        return [((i % 4 - (min(4, visible - (i // 4) * 4) - 1) / 2) * 18,
                 (i // 4 - (math.ceil(visible / 4) - 1) / 2) * 18)
                for i in range(visible)]
    radius = 18
    return [(radius * math.cos(-math.pi / 2 + 2 * math.pi * i / visible),
             radius * math.sin(-math.pi / 2 + 2 * math.pi * i / visible))
            for i in range(visible)]


def render_worker_scale(model: str, counts: dict[str, int]) -> str:
    levels = ("low", "medium", "high", "xhigh", "max")
    total = sum(counts.get(level, 0) for level in levels)
    description = ", ".join(f"{level}: {counts.get(level, 0)}" for level in levels)
    marks = ['<line class="strength-axis" x1="48" y1="34" x2="348" y2="34"/>']
    for index, level in enumerate(levels):
        x = 48 + index * 300 // (len(levels) - 1)
        count = counts.get(level, 0)
        marks.append(f'<circle class="strength-stop" cx="{x}" cy="34" r="2"/>')
        for dx, dy in worker_dot_positions(count):
            marks.append(f'<circle class="worker-dot" cx="{x + dx:.3f}" cy="{34 + dy:.3f}" r="8.1725"><title>{_escape(model.title())} · {level.title()} reasoning</title></circle>')
        if count > 12:
            marks.append(f'<text class="strength-overflow" x="{x}" y="8">+{count - 12}</text>')
        marks.append(f'<text class="strength-label" x="{x}" y="76">{level.title()}</text>')
    emblems = {
        "luna": '<path fill="#7ee6c2" d="M25 4a14 14 0 1 0 0 28A16 16 0 0 1 25 4Z"/>',
        "terra": '<circle cx="18" cy="18" r="14" fill="#a8baff"/><path fill="#344f79" d="m9 8 8-3 3 6-5 4-1 6-5-3Zm13 11 8-2-2 9-6 4-3-6Z"/>',
        "astra": '<path fill="#ffe89a" d="m18 2 3.7 11.2L34 13l-9.8 7.4L28 32l-10-6.6L8 32l3.8-11.6L2 13l12.3.2Z"/><circle cx="18" cy="18" r="3" fill="#fffbe5"/>',
    }
    emblem = emblems.get(model, emblems["terra"])
    return (
        f'<div class="worker-scale worker-scale-{_escape(model)}"><div class="scale-heading"><strong>{_escape(model.title())}</strong>'
        f'<span><b>{total}</b> active</span></div>'
        f'<svg class="model-emblem" viewBox="0 0 36 36" aria-hidden="true">{emblem}</svg>'
        f'<svg viewBox="0 0 396 90" role="img" aria-label="{_escape(model.title() + ": " + description)}">'
        + "".join(marks) + '</svg></div>'
    )


def render_work_digest(text: str) -> str:
    """Extract headings without inventing status or rewriting evidence."""
    blocks = []
    current = None
    for line in text.splitlines():
        if re.match(r"^[-*]\s+", line):
            if current is not None:
                blocks.append(current)
            current = [re.sub(r"^[-*]\s+", "", line)]
        elif current is not None:
            current.append(line[2:] if line.startswith("  ") else line)
    if current is not None:
        blocks.append(current)
    if not blocks:
        return '<p class="empty-state">Nothing listed here.</p>'
    cards = []
    completed_count = 0
    for block in blocks:
        checked = re.match(r"^\[([ xX])\]\s*(.*)", block[0])
        title = checked.group(2) if checked else block[0]
        complete = bool(checked and checked.group(1).lower() == "x")
        i = 1
        while i < len(block) and block[i].strip() and not re.match(r"^\s*[-*#]", block[i]):
            title += " " + block[i].strip()
            i += 1
        match = re.match(r"(R-[\w.-]+)\s*[—–:]\s*(.*)", title)
        identity, title = (match.group(1), match.group(2)) if match else ("", title)
        if complete:
            completed_count += 1
            continue
        title = re.split(r"(?<=[.!?])\s+|:\s+", title, maxsplit=1)[0]
        status = "Open"
        tone = "complete" if complete else "open"
        cards.append(
            f'<article class="work-card {tone}"><div class="work-meta">'
            f'<span>{_escape(identity)}</span><span class="work-state">{status}</span></div>'
            f'<h3>{_inline(title)}</h3>'
            '<a class="text-link" href="/ledger">Evidence &amp; handoff ↗</a></article>'
        )
    if completed_count:
        cards.append(f'<a class="completed-reference" href="/ledger">{completed_count} completed items · View the record ↗</a>')
    return "".join(cards)


def render_attention_spider(
    report: dict[str, Any],
    axes_data: list[dict[str, Any]],
    gaps: list[dict[str, Any]],
    uses_subtasks: bool = False,
) -> str:
    gap_ids = [
        str(axis.get("subtask_id" if uses_subtasks else "gap_id", "?"))
        for axis in axes_data
    ]
    raw_series = report.get("attention")
    series = [item for item in raw_series if isinstance(item, dict)] if isinstance(raw_series, list) else []
    radius = max(174, len(gap_ids) * 18)
    node_radius = radius + 80
    size = node_radius * 2 + 130
    center = size / 2

    def point(index: int, distance: float) -> tuple[float, float]:
        angle = -math.pi / 2 + (2 * math.pi * index / len(gap_ids))
        return center + distance * math.cos(angle), center + distance * math.sin(angle)

    grid: list[str] = []
    for fraction in (0.25, 0.5, 0.75, 1.0):
        coordinates = " ".join(
            f"{x:.1f},{y:.1f}" for x, y in (point(index, radius * fraction) for index in range(len(gap_ids)))
        )
        grid.append(f'<polygon points="{coordinates}" />')
    axes: list[str] = []
    nodes: list[str] = []
    latest_gap = str(report.get("latest_task_gap") or "")
    for index, (gap_id, gap) in enumerate(zip(gap_ids, axes_data)):
        x, y = point(index, radius)
        node_x, node_y = point(index, node_radius)
        axes.append(f'<line x1="{center:.1f}" y1="{center:.1f}" x2="{x:.1f}" y2="{y:.1f}" />')
        status = str(gap.get("status", "open"))
        active = status == "active" if uses_subtasks else (
            gap_id == latest_gap and report.get("latest_task_result") == "active"
        )
        tone = "active" if active else "proved" if status in {"proved", "done"} else "open"
        summary = " ".join(str(gap.get("summary", "")).split())
        nodes.append(
            f'<g class="trajectory-node {tone}" transform="translate({node_x - 58:.1f} {node_y - 28:.1f})">'
            f'<title>{_escape(summary)}</title><rect width="116" height="56" rx="8" />'
            f'<text x="58" y="21">{_escape(gap_id)}'
            f'{"" if uses_subtasks else " r" + _escape(gap.get("revision", "?"))}</text>'
            f'<text class="node-state" x="58" y="41">{_escape("active" if active else status)}'
            f'{"" if uses_subtasks else " · " + _escape(gap.get("attempts", 0)) + " attempts"}</text></g>'
        )

    shapes: list[str] = []
    legend: list[str] = []
    available_keys = {"target", "code", "test", "result"}
    for item in series:
        key = str(item.get("key", "other"))
        css_key = key if key in available_keys else "other"
        values = {
            str(entry.get("gap_id")): entry
            for entry in item.get("points", [])
            if isinstance(entry, dict)
        }
        coordinates: list[str] = []
        circles: list[str] = []
        for index, gap_id in enumerate(gap_ids):
            entry = values.get(gap_id, {})
            try:
                relative = max(0.0, min(1.0, float(entry.get("relative_pull", 0))))
            except (TypeError, ValueError):
                relative = 0.0
            try:
                raw = max(0.0, float(entry.get("raw_relation", 0)))
            except (TypeError, ValueError):
                raw = 0.0
            x, y = point(index, radius * relative)
            coordinates.append(f"{x:.1f},{y:.1f}")
            circles.append(
                f'<circle cx="{x:.1f}" cy="{y:.1f}" r="3"><title>{_escape(str(item.get("label", key)))} '
                f'→ {_escape(gap_id)} · relative {relative:.2f} · cosine {raw:.3f}</title></circle>'
            )
        label = str(item.get("label", key))
        source = str(item.get("source", ""))
        shapes.append(
            f'<g class="attention-series attention-{css_key}"><polygon points="{" ".join(coordinates)}">'
            f'<title>{_escape(label)} · {_escape(source)}</title></polygon>{"".join(circles)}</g>'
        )
        legend.append(
            f'<span title="{_escape(source)}"><i class="attention-key attention-{css_key}"></i>{_escape(label)}</span>'
        )
    gap_cards: list[str] = []
    for gap in gaps:
        gap_id = str(gap.get("gap_id", "?"))
        summary = " ".join(str(gap.get("summary", "No explanation recorded.")).split())
        status = str(gap.get("status", "open"))
        active = gap_id == latest_gap and report.get("latest_task_result") == "active"
        tone = "active" if active else "proved" if status == "proved" else "open"
        gap_cards.append(
            f'<article class="gap-explanation {tone}">'
            f'<div><span>{_escape(gap_id)} r{_escape(gap.get("revision", "?"))}</span>'
            f'<strong class="gap-state">{_escape("active" if active else status)}</strong></div>'
            f'<p>{_escape(summary)}</p></article>'
        )
    return (
        f'<article class="attention-panel"><div class="attention-heading"><h3>'
        f'{"Subtask attention" if uses_subtasks else "Attention spider"}</h3>'
        f'<span>{"Relative pull · not completion" if series else "Waiting for attention data"}</span></div>'
        f'<svg viewBox="0 0 {size} {size}" role="img" aria-label="Attention distribution across '
        f'{"ledger subtasks" if uses_subtasks else "closure gaps"}">'
        f'<g class="attention-grid">{"".join(grid)}{"".join(axes)}</g>'
        f'{"".join(shapes)}<g class="attention-nodes">{"".join(nodes)}</g></svg>'
        f'<div class="attention-legend">{"".join(legend)}</div>'
        f'<div class="attention-claim"><strong>{_escape(report.get("claim", "Claim"))}</strong>'
        f'<span>{_escape(report.get("latest_task") or "No active attempt")}</span></div>'
        f'<div class="gap-explanations">{"".join(gap_cards)}</div>'
        '<p>Each line is scaled to its own strongest gap. Hover a point for raw cosine similarity.</p>'
        '</article>'
    )


def _read_snapshot(path: Path) -> tuple[str, dict[str, Any]]:
    before = path.stat()
    raw = path.read_bytes()
    after = path.stat()
    if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
        raise OSError("source changed while it was being read")
    invalid_utf8 = False
    try:
        text = raw.decode("utf-8")
    except UnicodeDecodeError:
        text = raw.decode("utf-8", errors="replace")
        invalid_utf8 = True
    return text, {
        "hash": hashlib.sha256(raw).hexdigest()[:12],
        "mtime": after.st_mtime,
        "invalid_utf8": invalid_utf8,
    }


def _read_specification_snapshot(path: Path) -> tuple[str, dict[str, Any]]:
    """Read the canonical FS with the same visible decode errors as other panels."""
    text, identity = _read_snapshot(path)
    identity["path"] = str(path)
    return text, identity


def parse_ledger(text: str) -> dict[str, Any]:
    sections: dict[str, list[str]] = {"active": [], "waiting": [], "blocked": []}
    current: str | None = None
    for line in text.splitlines():
        heading = re.match(r"^#{1,6}\s+(.+?)\s*$", line, re.I)
        if heading:
            name = heading.group(1).lower()
            owning_claim = OWNING_CLAIM.match(heading.group(1))
            target = None
            if "active" in name:
                target = "active"
            elif any(label in name for label in ("waiting", "queued", "pending")):
                target = "waiting"
            elif "blocked" in name:
                target = "blocked"
            if owning_claim and current:
                sections[current].append(line)
            elif target:
                current = target
            else:
                current = None
        elif current and line.strip():
            sections[current].append(line)
    if not any(sections.values()) and text.strip():
        sections["active"].append(text.strip())
    active = "\n".join(sections["active"]).strip()
    waiting = "\n".join(sections["waiting"]).strip()
    blocked = "\n".join(sections["blocked"]).strip()
    claim = next(
        (
            match.group("claim")
            for line in active.splitlines()
            if (match := OWNING_CLAIM.match(line))
        ),
        None,
    )
    return {"active": active, "waiting": waiting, "blocked": blocked, "claim": claim}


CLAIM_ID_PATTERN = r"R-[A-Za-z0-9._-]+"
CLAIM_ID = re.compile(rf"^{CLAIM_ID_PATTERN}$")
OWNING_CLAIM = re.compile(
    rf"^(?:(?:#{{1,6}}\s+)|(?:[-*]\s+(?:\[[ xX]\]\s+|Blocked:\s+)))?"
    rf"(?P<claim>{CLAIM_ID_PATTERN})(?=[ \t]+—|[ \t]*$)",
)
RED_DFS_CLAIM = re.compile(
    rf"^- \[ \] 🔴 (?P<claim>{CLAIM_ID_PATTERN})[ \t]+—[ \t]+\S.*$"
)
DFS_STATUS = re.compile(r"^\s*(?:-\s*)?Status:\s*`?(?P<status>[A-Za-z]+)\b", re.I)
FENCE = re.compile(r"^[ \t]*(?P<marker>`{3,}|~{3,})")
FENCE_CLOSE = re.compile(r"^[ \t]*(?P<marker>`{3,}|~{3,})[ \t]*$")


def _owning_claim_ids(text: str) -> set[str]:
    return {
        match.group("claim")
        for line in _outside_fences(text)
        if (match := OWNING_CLAIM.match(line))
    }


def _claim_id(value: Any) -> str | None:
    return value if isinstance(value, str) and CLAIM_ID.fullmatch(value) else None


def _outside_fences(text: str):
    opening_marker: str | None = None
    for line in text.splitlines():
        if opening_marker is not None:
            closing = FENCE_CLOSE.match(line)
            if closing:
                marker = closing.group("marker")
                if marker[0] == opening_marker[0] and len(marker) >= len(opening_marker):
                    opening_marker = None
            continue
        if opening := FENCE.match(line):
            opening_marker = opening.group("marker")
            continue
        yield line


def _dfs_status(dfs: str) -> str | None:
    for line in _outside_fences(dfs):
        if match := DFS_STATUS.match(line):
            return match.group("status").casefold()
    return None


def upcoming_dfs_work(dfs: str, ledger: dict[str, Any], active_claim: str | None) -> str:
    """Project later frozen-DFS claims without creating another work authority."""
    if _dfs_status(dfs) not in {"frozen", "refrozen"}:
        return ""
    excluded = _owning_claim_ids(
        "\n".join((ledger["active"], ledger["waiting"], ledger["blocked"]))
    )
    if isinstance(active_claim, str):
        excluded.update(_owning_claim_ids(active_claim))
    upcoming: list[str] = []
    for line in _outside_fences(dfs):
        claim = RED_DFS_CLAIM.match(line)
        if not claim:
            continue
        if claim.group("claim") in excluded:
            continue
        upcoming.append(line)
    return "\n".join(upcoming)


def _table_columns(connection: sqlite3.Connection, table: str) -> set[str]:
    safe = table.replace('"', '""')
    return {row[1] for row in connection.execute(f'PRAGMA table_info("{safe}")')}


def _latest(connection: sqlite3.Connection, table: str, order: str) -> dict[str, Any] | None:
    if table not in {row[0] for row in connection.execute("SELECT name FROM sqlite_master WHERE type='table'")}:
        return None
    columns = _table_columns(connection, table)
    if order not in columns:
        return None
    safe_table = table.replace('"', '""')
    safe_order = order.replace('"', '""')
    row = connection.execute(
        f'SELECT * FROM "{safe_table}" ORDER BY "{safe_order}" DESC LIMIT 1'
    ).fetchone()
    return dict(row) if row else None


def _active_task(connection: sqlite3.Connection) -> dict[str, Any] | None:
    columns = _table_columns(connection, "tasks")
    if not columns or "started_at" not in columns:
        return None
    terminal_columns = [
        name for name in ("completed_at", "terminal_at", "attempt_terminal_at", "abandoned_at")
        if name in columns
    ]
    where = " AND ".join(f'"{name}" IS NULL' for name in terminal_columns) or "1"
    row = connection.execute(
        f'SELECT * FROM "tasks" WHERE {where} ORDER BY "started_at" DESC LIMIT 1'
    ).fetchone()
    return dict(row) if row else None


def _latest_task(connection: sqlite3.Connection) -> dict[str, Any] | None:
    return _latest(connection, "tasks", "started_at")


def _active_deadline(
    connection: sqlite3.Connection, task: dict[str, Any] | None
) -> dict[str, Any] | None:
    """Resolve the deadline belonging to the active task, with legacy fallback."""
    columns = _table_columns(connection, "claim_deadline_generations")
    if not columns:
        return None
    filters: list[tuple[str, Any]] = []
    for task_column, deadline_column in (
        ("lineage_id", "lineage_id"),
        ("claim_id", "claim_id"),
        ("deadline_generation", "generation"),
    ):
        value = task.get(task_column) if task else None
        if deadline_column in columns and value is not None:
            filters.append((deadline_column, value))
    if filters:
        where = " AND ".join(f'"{column}" = ?' for column, _value in filters)
        order = ' ORDER BY "generation" DESC' if "generation" in columns else ""
        row = connection.execute(
            f'SELECT * FROM "claim_deadline_generations" WHERE {where}{order} LIMIT 1',
            tuple(value for _column, value in filters),
        ).fetchone()
        return dict(row) if row else None
    order_column = "started_at" if "started_at" in columns else "generation"
    if "retired_at" in columns:
        row = connection.execute(
            f'SELECT * FROM "claim_deadline_generations" '
            f'WHERE "retired_at" IS NULL ORDER BY "{order_column}" DESC LIMIT 1'
        ).fetchone()
        if row:
            return dict(row)
    return _latest(connection, "claim_deadline_generations", order_column)


def _completed_mutation_counts(connection: sqlite3.Connection) -> tuple[int, int, dict[str, Any] | None]:
    tables = {row[0] for row in connection.execute(
        "SELECT name FROM sqlite_master WHERE type='table'"
    )}
    ordinary = 0
    for table in ("deadline_mutation_components", "integrity_mutation_components"):
        if table in tables and "component" in _table_columns(connection, table):
            ordinary += int(connection.execute(
                f'SELECT COUNT(*) FROM "{table}" WHERE "component" = ?', ("macro",)
            ).fetchone()[0])
    random = 0
    next_random = None
    if "random_mutation_cycles" in tables:
        columns = _table_columns(connection, "random_mutation_cycles")
        evidence_columns = [name for name in (
            "resolution_evidence", "ordinary_resolution_evidence",
            "universal_resolution_evidence",
        ) if name in columns]
        if evidence_columns:
            where = " OR ".join(f'"{name}" IS NOT NULL' for name in evidence_columns)
            random = int(connection.execute(
                f'SELECT COUNT(*) FROM "random_mutation_cycles" WHERE {where}'
            ).fetchone()[0])
            if {"cycle_number", "interval_windows", "due_after_terminal_windows"}.issubset(columns):
                unresolved = " AND ".join(f'"{name}" IS NULL' for name in evidence_columns)
                row = connection.execute(
                    f'SELECT * FROM "random_mutation_cycles" WHERE {unresolved} '
                    'ORDER BY "cycle_number" ASC LIMIT 1'
                ).fetchone()
                if row:
                    task_columns = _table_columns(connection, "tasks") if "tasks" in tables else set()
                    terminal_columns = [name for name in (
                        "attempt_terminal_at", "terminal_at", "completed_at", "abandoned_at"
                    ) if name in task_columns]
                    terminal_windows = 0
                    if terminal_columns:
                        terminal_where = " OR ".join(
                            f'"{name}" IS NOT NULL' for name in terminal_columns
                        )
                        parameters = []
                        if "attempt_terminal_at" in task_columns:
                            terminal_where = '"attempt_terminal_at" IS NOT NULL'
                        if {"attempt_terminal_kind", "abandonment_reason"}.issubset(task_columns):
                            terminal_where = f"({terminal_where}) AND NOT (attempt_terminal_kind = 'restart_normalized' OR (attempt_terminal_kind = 'abandoned' AND abandonment_reason = 'external_supervisor_restart_normalization'))"
                        if "lineage_id" in task_columns and "lineage_id" in columns:
                            terminal_where = f"({terminal_where}) AND lineage_id = ?"
                            parameters.append(row["lineage_id"])
                        terminal_windows = int(connection.execute(
                            f'SELECT COUNT(*) FROM "tasks" WHERE {terminal_where}', parameters
                        ).fetchone()[0])
                    next_random = dict(row)
                    next_random["terminal_windows"] = terminal_windows
                    next_random["remaining_windows"] = max(
                        0, int(row["due_after_terminal_windows"]) - terminal_windows
                    )
    return ordinary, random, next_random


def _mutation_review_state(connection: sqlite3.Connection) -> dict[str, Any]:
    """Project an active mutation review from existing append-only clock state."""
    tables = {row[0] for row in connection.execute(
        "SELECT name FROM sqlite_master WHERE type='table'"
    )}
    pending: list[tuple[float, str]] = []
    review_families = (
        ("claim_deadline_generation_incidents", "deadline_generation_mutation_components",
         ("lineage_id", "claim_id", "generation"), "Deadline"),
        ("claim_deadline_incidents", "deadline_mutation_components",
         ("lineage_id", "claim_id"), "Deadline"),
    )
    for incident_table, component_table, keys, label in review_families:
        if incident_table not in tables or component_table not in tables:
            continue
        incident = connection.execute(
            f'SELECT * FROM "{incident_table}" ORDER BY "recorded_at" DESC LIMIT 1'
        ).fetchone()
        if incident is None:
            continue
        if "reviewed_at" in incident.keys() and incident["reviewed_at"] is not None:
            continue
        where = " AND ".join(f'"{key}" = ?' for key in keys)
        components = {row[0] for row in connection.execute(
            f'SELECT "component" FROM "{component_table}" WHERE {where}',
            tuple(incident[key] for key in keys),
        )}
        if not {"micro", "macro"}.issubset(components):
            pending.append((float(incident["recorded_at"]), label))

    integrity_incident_columns = _table_columns(connection, "incidents") if "incidents" in tables else set()
    integrity_component_columns = (
        _table_columns(connection, "integrity_mutation_components")
        if "integrity_mutation_components" in tables else set()
    )
    if (
        {"kind", "recorded_at", "lineage_id", "task_id"}.issubset(integrity_incident_columns)
        and {"component", "lineage_id", "task_id"}.issubset(integrity_component_columns)
    ):
        incident = connection.execute(
            "SELECT * FROM incidents WHERE kind = 'integrity_breach' "
            "ORDER BY recorded_at DESC LIMIT 1"
        ).fetchone()
        if incident is not None:
            components = {row[0] for row in connection.execute(
                "SELECT component FROM integrity_mutation_components "
                "WHERE lineage_id = ? AND task_id = ?",
                (incident["lineage_id"], incident["task_id"]),
            )}
            if not {"micro", "macro"}.issubset(components):
                pending.append((float(incident["recorded_at"]), "Integrity"))

    if "random_mutation_cycles" in tables:
        columns = _table_columns(connection, "random_mutation_cycles")
        required = {"due_task_id", "ordinary_resolution_evidence", "universal_required",
                    "universal_resolution_evidence", "cycle_number"}
        if required.issubset(columns):
            row = connection.execute(
                "SELECT * FROM random_mutation_cycles WHERE due_task_id IS NOT NULL "
                "AND (ordinary_resolution_evidence IS NULL "
                "OR (universal_required = 1 AND universal_resolution_evidence IS NULL)) "
                "ORDER BY cycle_number DESC LIMIT 1"
            ).fetchone()
            if row is not None:
                pending.append((float(row["cycle_number"]), "Random"))

    if not pending:
        return {"running": False, "kind": None}
    _order, kind = max(pending, key=lambda item: item[0])
    return {"running": True, "kind": kind}


def read_clock(path: Path) -> dict[str, Any]:
    uri = f"file:{quote(str(path.resolve()).replace(os.sep, '/'), safe='/:')}?mode=ro"
    connection = sqlite3.connect(uri, uri=True, timeout=0)
    connection.row_factory = sqlite3.Row
    try:
        connection.execute("PRAGMA query_only=ON")
        connection.execute("BEGIN")
        task = _active_task(connection)
        latest_task = _latest_task(connection)
        deadline = _active_deadline(connection, task)
        restart = _latest(connection, "coordinator_restart_requests", "generation")
        finding = _latest(connection, "worker_findings", "reported_at")
        mutations, random_mutations, next_random = _completed_mutation_counts(connection)
        mutation_review = _mutation_review_state(connection)
        connection.execute("COMMIT")
        return {"task": task, "latest_task": latest_task,
                "deadline": deadline, "restart": restart,
                "finding": finding,
                "mutations": mutations, "random_mutations": random_mutations,
                "next_random_mutation": next_random, "mutation_review": mutation_review}
    finally:
        connection.close()


def _command_owns_workspace(command: str, workspace: Path) -> bool:
    """Compare process workspace arguments by filesystem identity, not spelling."""
    try:
        arguments = shlex.split(command, posix=os.name != "nt")
        value = arguments[arguments.index("--workspace") + 1]
        return Path(value.strip('"')).expanduser().resolve() == workspace.resolve()
    except (OSError, RuntimeError, ValueError, IndexError):
        return False


def process_state(workspace: Path) -> dict[str, Any]:
    pid_path = workspace / ".de67/state/coordinator-supervisor.pid"
    pid: int | None = None
    try:
        pid = int(pid_path.read_text(encoding="ascii").strip())
        os.kill(pid, 0)
    except (OSError, ValueError):
        pid = None
    coordinator = "unknown"
    try:
        output = subprocess.run(
            ["ps", "-axo", "pid=,ppid=,command="], capture_output=True, text=True,
            timeout=1, check=False,
        ).stdout
        if pid is None:
            for line in output.splitlines():
                match = re.match(r"\s*(\d+)\s+\d+\s+(.*)", line)
                if (match and "coordinator_supervisor.py" in match.group(2)
                        and _command_owns_workspace(match.group(2), workspace)):
                    pid = int(match.group(1))
                    break
        if pid is None:
            return {"supervisor": "absent", "coordinator": "absent",
                    "role": None, "pid": None}
        children = [line for line in output.splitlines() if re.match(rf"\s*\d+\s+{pid}\s+", line)]
        coordinator = "running" if any("codex-remote-run" in line or "codex" in line for line in children) else "waiting"
    except (OSError, subprocess.SubprocessError):
        if pid is None:
            return {"supervisor": "unknown", "coordinator": "unknown",
                    "role": None, "pid": None}
    role = "coordinator"
    run_root = workspace / ".de67/state/coordinator-runs"
    for status_path in sorted(
        run_root.glob("*/status.txt"),
        key=lambda path: path.stat().st_mtime,
        reverse=True,
    ):
        try:
            if status_path.read_text(encoding="ascii").strip() != "RUNNING":
                continue
            if _recorded_run_pid_is_alive(status_path) is False:
                continue
            role = (
                "mutation-reviewer"
                if status_path.parent.name.startswith("mutation-")
                else "coordinator"
            )
            break
        except (OSError, UnicodeError):
            continue
    return {"supervisor": "running", "coordinator": coordinator,
            "role": role, "pid": pid}


def _session_header(path: Path) -> dict[str, Any]:
    record: dict[str, Any] = {}
    with path.open("r", encoding="utf-8", errors="replace") as source:
        for line in source:
            try:
                item = json.loads(line)
            except (TypeError, ValueError):
                continue
            payload = item.get("payload", {})
            if item.get("type") == "session_meta":
                record.update({
                    "id": payload.get("id"),
                    "parent": payload.get("parent_thread_id"),
                    "cwd": payload.get("cwd"),
                    "timestamp": payload.get("timestamp") or item.get("timestamp"),
                })
            elif item.get("type") == "turn_context":
                record["model"] = payload.get("model")
                record["effort"] = payload.get("effort")
            if record.get("id") and record.get("model"):
                break
    return record


def _session_complete(path: Path) -> bool:
    size = path.stat().st_size
    with path.open("rb") as source:
        source.seek(max(0, size - 131072))
        tail = source.read().decode("utf-8", errors="replace")
    started = max(tail.rfind('"type":"task_started"'),
                  tail.rfind('"type": "task_started"'))
    terminal = max(
        tail.rfind('"type":"task_complete"'),
        tail.rfind('"type": "task_complete"'),
        tail.rfind('"type":"turn_aborted"'),
        tail.rfind('"type": "turn_aborted"'),
    )
    return terminal >= 0 and terminal > started


def _active_worker_claims(workspace: Path) -> dict[str, str] | None:
    """Return live primary worker-to-coordinator ownership, or None for legacy clocks."""
    config_path = workspace / ".de67/state/workspace.json"
    config = json.loads(config_path.read_text(encoding="utf-8"))
    clock = config.get("clock")
    if not isinstance(clock, dict):
        raise ValueError("workspace configuration lacks clock state")
    configured = clock.get("state")
    if not isinstance(configured, str) or not configured.strip():
        raise ValueError("workspace clock.state must be a non-empty path")
    state = Path(configured).expanduser()
    if not state.is_absolute():
        state = (workspace / state).resolve()
    uri = f"file:{quote(str(state.resolve()).replace(os.sep, '/'), safe='/:')}?mode=ro"
    connection = sqlite3.connect(uri, uri=True, timeout=0)
    connection.row_factory = sqlite3.Row
    try:
        connection.execute("PRAGMA query_only=ON")
        tables = {row[0] for row in connection.execute(
            "SELECT name FROM sqlite_master WHERE type='table'"
        )}
        if not {"tasks", "worker_claims"}.issubset(tables):
            return None
        task_columns = _table_columns(connection, "tasks")
        terminal_columns = [
            name for name in (
                "completed_at", "terminal_at", "attempt_terminal_at", "abandoned_at"
            ) if name in task_columns
        ]
        terminal = " AND ".join(f'task."{name}" IS NULL' for name in terminal_columns)
        where = f" AND {terminal}" if terminal else ""
        rows = connection.execute(
            "SELECT claim.worker_id, claim.coordinator_session_id "
            "FROM worker_claims AS claim JOIN tasks AS task "
            "ON task.lineage_id = claim.lineage_id AND task.task_id = claim.task_id "
            f"WHERE claim.released_at IS NULL{where}"
        ).fetchall()
        return {str(row["worker_id"]): str(row["coordinator_session_id"]) for row in rows}
    finally:
        connection.close()


def _recorded_run_pid_is_alive(status_path: Path) -> bool | None:
    """Return whether a recorded runner PID is alive, or None for legacy records."""
    pid_path = status_path.with_name("pid.txt")
    try:
        pid = int(pid_path.read_text(encoding="ascii").strip())
    except FileNotFoundError:
        return None
    except (OSError, UnicodeError, ValueError):
        return False
    if pid <= 0:
        return False
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return True
    except OSError:
        return False
    return True


def _active_coordinator_id(workspace: Path) -> str | None:
    """Read one unambiguous active coordinator id from passive durable run records."""
    status_root = workspace / ".de67/state"
    try:
        output = subprocess.run(
            ["ps", "axww", "-o", "pid=,command="], capture_output=True,
            text=True, timeout=1, check=False,
        ).stdout
        commands = [
            line.split(None, 1)[1] for line in output.splitlines()
            if "coordinator_supervisor.py" in line
            and _command_owns_workspace(line.split(None, 1)[1], workspace)
        ]
        if len(commands) != 1:
            raise ValueError("active coordinator supervisor is ambiguous")
        command = commands[0]
        arguments = [
            argument.strip('"')
            for argument in shlex.split(command, posix=os.name != "nt")
        ]
        run_root_index = arguments.index("--run-root") + 1
        status_root = Path(arguments[run_root_index]).expanduser().resolve()
    except (OSError, ValueError, IndexError, subprocess.SubprocessError):
        pass
    active_sessions: set[str] = set()
    indexed_runs = False
    status_paths = status_root.glob("**/status.txt")
    # Modern supervisors already index unfinished runs. Avoid rereading all
    # historical artifacts merely to discover the live session.
    config_path = workspace / ".de67/state/workspace.json"
    if config_path.is_file():
        config = json.loads(config_path.read_text(encoding="utf-8"))
        clock = config["clock"]
        state = Path(clock["state"]).expanduser()
        if not state.is_absolute():
            state = workspace / state
        uri = f"file:{quote(str(state.resolve()), safe='/:')}?mode=ro"
        connection = sqlite3.connect(uri, uri=True, timeout=0)
        try:
            tables = {row[0] for row in connection.execute(
                "SELECT name FROM sqlite_master WHERE type='table'"
            )}
            if "supervisor_attempts" in tables:
                indexed_runs = True
                runs = connection.execute(
                    "SELECT run_id FROM supervisor_attempts "
                    "WHERE lineage_id=? AND finished_at IS NULL AND role='coordinator'",
                    (clock["lineage"],),
                ).fetchall()
                status_paths = [
                    workspace / ".de67/state/coordinator-runs" / row[0] / "status.txt"
                    for row in runs
                ]
        finally:
            connection.close()
    for status_path in status_paths:
        try:
            if status_path.read_text(encoding="ascii").strip() != "RUNNING":
                continue
            if _recorded_run_pid_is_alive(status_path) is False:
                continue
            session_id = status_path.with_name("session_id.txt").read_text(
                encoding="ascii"
            ).strip()
            if session_id:
                active_sessions.add(session_id)
        except (OSError, UnicodeError):
            continue
    if active_sessions:
        if len(active_sessions) != 1:
            raise ValueError("multiple active coordinator sessions")
        return next(iter(active_sessions))

    if indexed_runs:
        return None

    # Compatibility with direct codex_runner.py records from older deployments.
    run_root = workspace / ".de67/state/runner-runs"
    statuses = sorted(run_root.glob("*/status.json"),
                      key=lambda path: path.stat().st_mtime, reverse=True)
    for status_path in statuses:
        try:
            status = json.loads(status_path.read_text(encoding="utf-8", errors="replace"))
            if status.get("status") != "running":
                continue
            events = status_path.with_name("events.jsonl")
            size = events.stat().st_size
            with events.open("rb") as source:
                source.seek(max(0, size - 524288))
                tail = source.read().decode("utf-8", errors="replace")
            for line in reversed(tail.splitlines()):
                try:
                    item = json.loads(line)
                except (TypeError, ValueError):
                    continue
                event = item.get("item", {})
                coordinator_id = event.get("sender_thread_id")
                if coordinator_id:
                    return str(coordinator_id)
        except (OSError, TypeError, ValueError):
            continue
    return None


def _active_mutator(workspace: Path, sessions_root: Path) -> dict[str, str] | None:
    """Read the existing persistent session binding; it grants no worker ownership."""
    binding_path = workspace / ".de67/state/mutator-session.json"
    if not binding_path.is_file():
        return None
    binding = json.loads(binding_path.read_text(encoding="utf-8"))
    if (binding.get("state") != "active"
            or Path(binding.get("workspace", "")).resolve() != workspace.resolve()):
        return None
    pid = binding.get("runner_pid")
    if not isinstance(pid, int) or pid <= 0:
        return None
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return None
    except PermissionError:
        pass
    index = sessions_root.parent / "state_5.sqlite"
    uri = f"file:{quote(str(index), safe='/:')}?mode=ro"
    connection = sqlite3.connect(uri, uri=True, timeout=0)
    try:
        row = connection.execute(
            "SELECT id, cwd, model, reasoning_effort FROM threads WHERE id = ?",
            (binding.get("thread_id"),),
        ).fetchone()
    finally:
        connection.close()
    if row is None or Path(row[1]).resolve() != workspace.resolve():
        raise ValueError("active mutator session metadata unavailable")
    if row[2] != "gpt-6-astra" or row[3] not in ("low", "medium", "high", "xhigh", "max"):
        raise ValueError("active mutator model or reasoning effort unavailable")
    return {"id": str(row[0]), "effort": str(row[3])}


def _worker_execution(workspace: Path, claims: dict[str, str]) -> tuple[dict[str, str], dict[str, dict[str, str]]]:
    """Join retained claims to current library execution without changing claim history."""
    registry = workspace / ".de67/state/worker-library/registry.sqlite3"
    if not registry.exists():
        return claims, {}
    clock = json.loads((workspace / ".de67/state/workspace.json").read_text())["clock"]
    state = Path(clock["state"]).expanduser()
    if not state.is_absolute():
        state = workspace / state
    connection = sqlite3.connect(registry.resolve().as_uri() + "?mode=ro", uri=True, timeout=0)
    connection.row_factory = sqlite3.Row
    try:
        rows = connection.execute(
            "SELECT a.worker_id,a.status,a.binding,w.model,w.effort FROM assignments a "
            "JOIN workers w ON a.name=w.name AND a.worker_id=w.thread_id "
            "WHERE a.state_path=? AND a.lineage=? ORDER BY a.updated_at",
            (str(state.resolve()), clock["lineage"])).fetchall()
    finally:
        connection.close()
    owners, metadata = dict(claims), {}
    for row in rows:
        worker = row["worker_id"]
        if worker not in claims:
            continue
        owners.pop(worker, None)
        binding = json.loads(row["binding"])
        if (row["status"] != "running"
                or binding.get("workspace") != str(workspace.resolve())
                or binding.get("deadline_state") != str(state.resolve())
                or binding.get("lineage") != clock["lineage"]):
            continue
        owners[worker] = binding["thread_id"]
        metadata[worker] = {"model": row["model"], "effort": row["effort"]}
    return owners, metadata


def worker_state(workspace: Path, sessions_root: Path) -> dict[str, Any]:
    """Project active roster subagents from Codex's existing read-only session records."""
    counts = {model: {effort: 0 for effort in ("low", "medium", "high", "xhigh", "max")}
              for model in ("luna", "terra", "astra", "sol")}
    mutator = _active_mutator(workspace, sessions_root)
    counted = {mutator["id"]} if mutator else set()
    if mutator:
        counts["astra"][mutator["effort"]] += 1
    active_claims = _active_worker_claims(workspace)
    metadata = {}
    if active_claims:
        active_claims, metadata = _worker_execution(workspace, active_claims)
    if active_claims == {}:
        return {"counts": counts, "available": True}
    active_coordinator_id = _active_coordinator_id(workspace)
    index = sessions_root.parent / "state_5.sqlite"
    if active_claims and index.is_file():
        owners = set(active_claims.values())
        uri = f"file:{quote(str(index), safe='/:')}?mode=ro"
        connection = sqlite3.connect(uri, uri=True, timeout=0)
        try:
            # Library workers have durable owners but need not have native
            # spawn edges. Seed their helper trees from those same claims.
            seeds = sorted(owners | set(active_claims) | ({active_coordinator_id} if active_coordinator_id else set()))
            values = ",".join("(?)" for _ in seeds)
            rows = connection.execute(
                f"WITH RECURSIVE tree(id) AS (VALUES {values} UNION "
                "SELECT child_thread_id FROM thread_spawn_edges JOIN tree "
                "ON parent_thread_id=tree.id) "
                "SELECT rollout_path FROM threads JOIN tree ON threads.id=tree.id",
                seeds,
            ).fetchall()
            paths = [Path(row[0]) for row in rows]
        finally:
            connection.close()
    else:
        paths = sorted(sessions_root.glob("**/rollout-*.jsonl"), reverse=True)
    root_path: Path | None = None
    root: dict[str, Any] = {}
    target = workspace.resolve()
    for path in paths:
        candidate = _session_header(path)
        try:
            candidate_cwd = Path(candidate.get("cwd", "")).resolve()
        except (OSError, RuntimeError):
            continue
        if (
            candidate_cwd == target
            and (
                candidate.get("id") == active_coordinator_id
                or (active_coordinator_id is None and not candidate.get("parent"))
            )
        ):
            root_path, root = path, candidate
            break
    if root_path is None or not root.get("id"):
        return {"counts": counts, "available": False, "error": "active Codex session unavailable"}
    candidates: list[tuple[Path, dict[str, Any]]] = []
    for path in paths:
        if path == root_path:
            continue
        candidate = _session_header(path)
        try:
            candidate_cwd = Path(candidate.get("cwd", "")).resolve()
        except (OSError, RuntimeError):
            continue
        if candidate_cwd != target:
            continue
        candidates.append((path, candidate))

    # Durable claims identify current primaries, including reused library
    # workers. Codex's native spawn tree supplies their helper descendants.
    root_id = str(root["id"])
    descendants = {root_id}
    pending = candidates
    while pending:
        next_pending: list[tuple[Path, dict[str, Any]]] = []
        changed = False
        for path, candidate in pending:
            candidate_id = str(candidate.get("id", ""))
            parent = str(candidate.get("parent", ""))
            if active_claims is not None and candidate_id in active_claims:
                parent = active_claims[candidate_id]
            if parent not in descendants:
                next_pending.append((path, candidate))
                continue
            if (
                active_claims is not None
                and parent == root_id
                and active_claims.get(candidate_id) != root_id
            ):
                # A direct coordinator child is a primary worker only while its
                # durable claim belongs to this coordinator. Do not inherit the
                # descendants of a released or owner-lost primary.
                changed = True
                continue
            descendants.add(candidate_id)
            changed = True
            if active_claims is None and _session_complete(path):
                continue
            if active_claims is not None and parent != root_id and _session_complete(path):
                continue
            context = metadata.get(candidate_id, candidate)
            model = str(context.get("model", "")).lower().rsplit("-", 1)[-1]
            effort = str(context.get("effort", "")).lower()
            if model in counts and effort in counts[model] and candidate_id not in counted:
                counts[model][effort] += 1
                counted.add(candidate_id)
        if not changed:
            break
        pending = next_pending
    return {"counts": counts, "available": True, "error": None}



class Dashboard:
    def __init__(self, workspace: Path, refresh_seconds: int = 0,
                 sessions_root: Path | None = None,
                 sidecar_script: Path | None = None,
                 fratbro_script: Path | None = None,
                 fratbro_cache: Path | None = None,
                 fratbro_codex: str = "codex") -> None:
        self.workspace = workspace
        self.refresh_seconds = refresh_seconds
        self.sessions_root = sessions_root or Path.home() / ".codex/sessions"
        self.sidecar_script = sidecar_script
        self.fratbro_script = fratbro_script
        self.fratbro_cache = fratbro_cache
        self.fratbro_codex = fratbro_codex
        self._lock = threading.Lock()
        self._good: dict[str, dict[str, Any]] = {}
        self._sidecar_signature: tuple[Any, ...] | None = None
        self._fratbro_signature: tuple[Any, ...] | None = None
        self._fratbro_process: subprocess.Popen[str] | None = None
        self._fratbro_last_active_workers: tuple[str, ...] = ()

    def _fratbro_lifecycle_signature(self, clock: dict[str, Any]) -> tuple[Any, ...] | None:
        """Emit only durable worker-start and worker-finish lifecycle events."""
        claims = _active_worker_claims(self.workspace)
        active_workers = tuple(sorted(claims)) if claims else ()
        if active_workers:
            self._fratbro_last_active_workers = active_workers
            return ("active", active_workers)
        if not self._fratbro_last_active_workers:
            return None
        return ("terminal", self._fratbro_last_active_workers)

    def _fratbro_source(self, ledger: dict[str, Any], clock: dict[str, Any]) -> dict[str, Any]:
        if self.fratbro_cache is None:
            return {}
        signature = self._fratbro_lifecycle_signature(clock.get("data", {}))
        if signature is None:
            try:
                return json.loads(self.fratbro_cache.read_text(encoding="utf-8"))
            except (OSError, TypeError, ValueError):
                return {}
        signature_text = hashlib.sha256(
            json.dumps(signature, separators=(",", ":")).encode("utf-8")
        ).hexdigest()
        if (self.fratbro_script is not None and signature != self._fratbro_signature
                and (self._fratbro_process is None or self._fratbro_process.poll() is not None)):
            self._fratbro_signature = signature
            self._fratbro_process = subprocess.Popen(
                [sys.executable, str(self.fratbro_script), "--workspace", str(self.workspace),
                 "--cache", str(self.fratbro_cache), "--signature", signature_text,
                 "--codex", self.fratbro_codex,
                 "--codex-sessions", str(self.sessions_root)],
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, text=True,
            )
        try:
            return json.loads(self.fratbro_cache.read_text(encoding="utf-8"))
        except (OSError, TypeError, ValueError):
            return {}

    def _markdown_source(self, name: str, path: Path) -> dict[str, Any]:
        try:
            text, identity = (_read_specification_snapshot(path) if name == "dfs"
                              else _read_snapshot(path))
            value = {"text": text, "html": render_markdown(text), "identity": identity,
                     "observed": time.time(), "stale": False, "error": None}
            self._good[name] = value
            return value
        except Exception as error:  # display failure is deliberately isolated
            previous = dict(self._good.get(name, {}))
            previous.update({"stale": bool(previous), "error": str(error), "observed": time.time()})
            return previous

    def _clock_source(self) -> dict[str, Any]:
        try:
            config_path = self.workspace / ".de67/state/workspace.json"
            config = json.loads(config_path.read_text(encoding="utf-8"))
            configured = config["clock"]["state"]
            if not isinstance(configured, str) or not configured.strip():
                raise ValueError("workspace clock.state must be a non-empty path")
            path = Path(configured).expanduser()
            if not path.is_absolute():
                path = (self.workspace / path).resolve()
            value = {"data": read_clock(path), "path": path, "observed": time.time(),
                     "stale": False, "error": None}
            self._good["clock"] = value
            return value
        except Exception as error:
            previous = dict(self._good.get("clock", {}))
            previous.update({"stale": bool(previous), "error": str(error), "observed": time.time()})
            return previous

    def _sidecar_source(self, state: Path, claim: str | None) -> dict[str, Any]:
        if self.sidecar_script is None:
            return {"data": None, "stale": False, "error": "not configured"}
        try:
            state_stat = state.stat()
            script_stat = self.sidecar_script.stat()
            signature = (
                claim, state_stat.st_size, state_stat.st_mtime_ns,
                script_stat.st_size, script_stat.st_mtime_ns,
            )
            if signature == self._sidecar_signature and "sidecar" in self._good:
                return self._good["sidecar"]
            if not claim:
                raise ValueError("no active claim")
            value = {
                "data": read_sidecar(self.sidecar_script, self.workspace, state, claim),
                "observed": time.time(), "stale": False, "error": None,
            }
            self._sidecar_signature = signature
            self._good["sidecar"] = value
            return value
        except Exception as error:
            if claim and "No closure gaps found" in str(error):
                value = {
                    "data": {"claim": claim, "gaps": [], "churn_vector": {}},
                    "observed": time.time(), "stale": False, "error": None,
                }
                self._sidecar_signature = signature if "signature" in locals() else None
                self._good["sidecar"] = value
                return value
            previous = dict(self._good.get("sidecar", {}))
            previous.update({"stale": bool(previous), "error": str(error),
                             "observed": time.time()})
            return previous

    def snapshot(self) -> dict[str, Any]:
        with self._lock:
            root = self.workspace / ".de67"
            dfs = self._markdown_source("dfs", root / "FS.md")
            ledger = self._markdown_source("ledger", root / "work-ledger.md")
            clock = self._clock_source()
            clock_data = clock.get("data", {})
            task = clock_data.get("task") or {}
            latest_task = clock_data.get("latest_task") or {}
            claim = (_claim_id(task.get("claim_id"))
                     or _claim_id(latest_task.get("claim_id"))
                     or parse_ledger(
                ledger.get("text", "")
            ).get("claim"))
            clock_path = clock.get("path")
            if isinstance(clock_path, Path):
                sidecar = self._sidecar_source(clock_path, claim)
            else:
                previous = dict(self._good.get("sidecar", {}))
                previous.update({"stale": bool(previous),
                                 "error": clock.get("error") or "clock unavailable",
                                 "observed": time.time()})
                sidecar = previous
            try:
                process = process_state(self.workspace)
                process_error = None
            except Exception as error:
                process, process_error = {}, str(error)
            try:
                workers = worker_state(self.workspace, self.sessions_root)
            except Exception as error:
                workers = {"counts": {}, "available": False, "error": str(error)}
            fratbro = self._fratbro_source(ledger, clock)
            return {"dfs": dfs, "ledger": ledger, "clock": clock, "sidecar": sidecar,
                    "fratbro": fratbro,
                    "process": process,
                    "workers": workers, "process_error": process_error, "observed": time.time()}

    def render(self, tab: str) -> bytes:
        state = self.snapshot()
        dfs, ledger, clock = state["dfs"], state["ledger"], state["clock"]
        sidecar = state["sidecar"]
        fratbro = state.get("fratbro", {})
        ledger_data = parse_ledger(ledger.get("text", ""))
        clock_data = clock.get("data", {})
        task = clock_data.get("task") or {}
        latest_task = clock_data.get("latest_task") or {}
        active_claim = (_claim_id(task.get("claim_id"))
                        or _claim_id(latest_task.get("claim_id"))
                        or ledger_data["claim"])
        upcoming = upcoming_dfs_work(dfs.get("text", ""), ledger_data, active_claim)
        deadline = clock_data.get("deadline") or {}
        restart = clock_data.get("restart") or {}
        finding = clock_data.get("finding") or {}
        mutation_review = clock_data.get("mutation_review") or {"running": False}
        next_random = clock_data.get("next_random_mutation") or {}
        process = state["process"]
        workers = state["workers"]
        now = time.time()
        deadline_at = deadline.get("deadline_at") or task.get("deadline_at")
        remaining = "—"
        if isinstance(deadline_at, (int, float)):
            seconds = int(deadline_at - now)
            sign = "−" if seconds < 0 else ""
            seconds = abs(seconds)
            remaining = f"{sign}{seconds // 3600:02d}:{seconds % 3600 // 60:02d}:{seconds % 60:02d}"
        random_remaining = next_random.get("remaining_windows")
        if random_remaining is None:
            random_note = ""
        elif random_remaining == 0:
            random_note = "Random mutation due now"
        else:
            suffix = "result" if random_remaining == 1 else "results"
            random_note = f"Next random in {random_remaining} worker {suffix}"
        finding_age = ""
        if isinstance(finding.get("reported_at"), (int, float)):
            age_seconds = max(0, int(now - finding["reported_at"]))
            if age_seconds < 60:
                finding_age = f"{age_seconds}s ago"
            elif age_seconds < 3600:
                finding_age = f"{age_seconds // 60}m ago"
            else:
                finding_age = f"{age_seconds // 3600}h ago"

        def lamp(label: str, value: str, tone: str) -> str:
            return f'<div class="lamp"><span class="dot {tone}"></span><small>{_escape(label)}</small><strong>{_escape(value)}</strong></div>'

        supervisor = process.get("supervisor", "unknown")
        coordinator = process.get("coordinator", "unknown")
        process_role = process.get("role")
        mutation_running = bool(
            mutation_review.get("running") or process_role == "mutation-reviewer"
        )
        work_value = "Blocked" if ledger_data["blocked"] else (
            task.get("task_id") or active_claim or "Idle"
        )
        work_tone = (
            "red" if ledger_data["blocked"] else
            "green" if ledger_data["active"] and supervisor == "running" else
            "yellow" if ledger_data["active"] else "grey"
        )
        nav = '<nav><a class="%s" href="/">Overview</a><a class="%s" href="/dfs">FS</a><a href="?refresh=1">Refresh snapshot ↻</a></nav>' % (
            "selected" if tab == "overview" else "", "selected" if tab == "dfs" else "")
        meta = ""  # Explicit refresh keeps reading and navigation stable.
        source_bits = ['<span>Snapshot ' + time.strftime("%H:%M:%S") + ' · refresh manually</span>']
        for label, source in (("Markdown", ledger), ("FS", dfs), ("SQLite", clock)):
            tone = "yellow" if source.get("stale") else "red" if source.get("error") else "green"
            detail = source.get("error") or source.get("identity", {}).get("hash") or "healthy"
            source_bits.append(f'<span><i class="dot {tone}"></i>{_escape(label)} <em>{_escape(detail)}</em></span>')
        if tab == "ledger":
            body = '<section class="document"><div class="eyebrow">SOURCE DOCUMENT</div><h2>Work ledger</h2>' + ledger.get("html", "<p>Unavailable</p>") + '</section>'
        elif tab == "briefing":
            summary = fratbro.get("summary", "")
            if isinstance(summary, dict):
                summary = "\n\n".join(str(v) for v in summary.values())
            body = '<section class="document"><div class="eyebrow">BRIEFING</div><h2>Full briefing</h2>' + render_markdown(str(summary)) + '</section>'
        elif tab == "dfs":
            body = f'<section class="document">{dfs.get("html", "<p>FS unavailable.</p>")}</section>'
        else:
            cards = "".join([
                lamp("Supervisor", supervisor.title(), "green" if supervisor == "running" else "grey"),
                lamp("Coordinator", (
                    "Mutation reviewer" if process_role == "mutation-reviewer"
                    else coordinator.title()
                ), "yellow" if process_role == "mutation-reviewer" else "green" if coordinator == "running" else "yellow" if coordinator == "waiting" else "grey"),
                lamp("Work", work_value, work_tone),
                lamp("Mutation review", "Running" if mutation_running else "Off",
                     "yellow" if mutation_running else "grey"),
                f'<div class="metric"><small>Deadline</small><strong>{remaining}</strong></div>',
                f'<div class="metric"><small>Mutations</small><strong>{_escape((clock_data.get("mutations", 0) + clock_data.get("random_mutations", 0)) if clock_data else "—")}</strong><span class="metric-note">{_escape(random_note)}</span></div>',
            ])
            worker_counts = workers.get("counts", {})
            if workers.get("available"):
                worker_body = '<div class="roster-scales">' + "".join(
                    render_worker_scale(model, worker_counts.get(model, {}))
                    for model in ("luna", "terra", "astra")
                ) + '</div>'

            else:
                worker_body = f'<p class="subtle">Unavailable · {_escape(workers.get("error", "unknown source"))}</p>'
            workers_html = f'<section class="workers"><h2>Active agents</h2>{worker_body}</section>'
            active_html = render_work_digest(ledger_data["active"])
            upcoming_html = render_work_digest(upcoming)
            waiting_html = (
                '<section><h2>Waiting on event</h2><div class="ledger-list">'
                f'{render_work_digest(ledger_data["waiting"])}</div></section>'
                if ledger_data["waiting"] else ""
            )
            blocked_html = render_work_digest(ledger_data["blocked"])
            if sidecar.get("data"):
                sidecar_html = render_trajectory(sidecar["data"])
            elif sidecar.get("error") == "not configured":
                sidecar_html = ""
            else:
                sidecar_html = (
                    '<section class="trajectory"><h2>Trajectory sidecar</h2>'
                    f'<p class="subtle">Unavailable · {_escape(sidecar.get("error", "no report"))}</p></section>'
                )
            details = " · ".join(filter(None, [
                f'claim {_escape(active_claim)}' if active_claim else "",
                f'gap {_escape(task.get("closure_gap_id"))} r{_escape(task.get("closure_gap_revision"))}' if task.get("closure_gap_id") else "",
                f'deadline generation {_escape(deadline.get("generation"))}' if deadline.get("generation") else "",
                f'restart {_escape(restart.get("generation"))}' if restart.get("generation") else "",
            ]))
            finding_html = ""
            if finding:
                finding_html = (
                    '<div class="activity"><small>Latest finding</small>'
                    f'<strong>{_escape(finding.get("task_id", ""))}</strong>'
                    f'<span>{_escape(finding.get("short_verdict", ""))}</span>'
                    f'<em>{_escape(finding_age)}</em></div>'
                )
            fratbro_html = render_fratbro_status(fratbro) if self.fratbro_cache else ""
            body = f'<div class="status">{cards}</div>{workers_html}{sidecar_html}{fratbro_html}{finding_html}<section class="work-section"><div class="eyebrow">THE WORK / CURRENT SCOPE</div><h2>Work in focus</h2><div class="subtle">{details}</div><div class="ledger-list">{active_html}</div></section><section class="work-section"><div class="eyebrow">ON THE HORIZON</div><h2>Up next</h2><div class="ledger-list">{upcoming_html}</div></section>{waiting_html}<section class="work-section"><div class="eyebrow">NEEDS ATTENTION</div><h2>Blocked work</h2><div class="ledger-list">{blocked_html}</div></section>'
        page = f'''<!doctype html><html lang="en"><head><meta charset="utf-8">{meta}
<meta name="viewport" content="width=device-width,initial-scale=1"><title>de67</title>
<style>
:root{{--bg:#101318;--panel:#1a1e24;--line:#343a43;--text:#eee9df;--muted:#9ca3ad;--green:#75c84c;--yellow:#f0bc28;--red:#e05248;--blue:#75a7d8}}
*{{box-sizing:border-box}}body{{margin:0;background:var(--bg);color:var(--text);font:15px system-ui,sans-serif}}main{{max-width:1180px;margin:auto;padding:24px}}header{{display:flex;align-items:baseline;gap:22px}}h1{{font-size:25px;margin:0}}header span,.subtle{{color:var(--muted)}}nav{{display:flex;margin:18px 0;border-bottom:1px solid var(--line)}}nav a{{color:var(--muted);text-decoration:none;padding:10px 16px}}nav a.selected{{color:var(--text);border:1px solid var(--line);border-bottom-color:var(--bg);border-radius:6px 6px 0 0;margin-bottom:-1px}}nav a:last-child{{margin-left:auto}}.status{{display:grid;grid-template-columns:repeat(5,1fr);gap:10px}}.lamp,.metric,section,.activity{{background:var(--panel);border:1px solid var(--line);border-radius:7px}}.lamp,.metric{{padding:13px 14px;min-height:82px}}small{{display:block;color:var(--muted);margin-bottom:10px}}strong{{font-size:18px}}.metric-note{{display:block;color:var(--muted);font-size:11px;margin-top:5px;white-space:nowrap}}.workers{{padding:12px 16px}}table{{width:100%;border-collapse:collapse}}th,td{{padding:7px 12px;text-align:center;border-top:1px solid var(--line)}}thead th{{border-top:0;color:var(--muted);font-size:12px;font-weight:500}}tbody th{{text-align:left}}td{{font-variant-numeric:tabular-nums;color:var(--muted)}}td.active-count{{color:var(--green);font-weight:700}}.activity{{display:grid;grid-template-columns:100px max-content 1fr max-content;align-items:center;gap:12px;margin-top:10px;padding:10px 14px}}.activity small{{margin:0}}.activity strong{{font-size:13px}}.activity span{{overflow:hidden;text-overflow:ellipsis;white-space:nowrap}}.activity em{{color:var(--muted);font-style:normal;font-size:12px}}.dot{{display:inline-block;width:11px;height:11px;border-radius:50%;margin-right:8px}}.green{{background:var(--green)}}.yellow{{background:var(--yellow)}}.red{{background:var(--red)}}.grey{{background:#737983}}section{{margin-top:12px;padding:16px}}h2{{font-size:16px;margin:0 0 12px}}h3{{font-size:15px}}p,li{{line-height:1.55}}code{{background:#11151a;padding:2px 4px;border-radius:3px}}pre{{overflow:auto;background:#11151a;padding:12px;border-radius:5px}}.ledger-list{{margin-top:12px}}.ledger-item{{position:relative;margin:10px 0 0;padding:12px 16px 12px 22px;border:0;border-radius:0;background:linear-gradient(90deg,rgba(117,167,216,.08),transparent 68%)}}.ledger-item::before{{content:"";position:absolute;left:0;top:6px;bottom:6px;width:4px;border-radius:4px;background:linear-gradient(180deg,var(--blue),#536c86)}}.ledger-title{{font-weight:650;line-height:1.45}}.ledger-item ul{{list-style:none;margin:8px 0 0;padding-left:0;color:var(--muted)}}.ledger-item li{{padding:4px 0}}.ledger-item p{{margin:8px 0 0;color:var(--muted)}}.trajectory{{padding-bottom:12px}}.trajectory-scroll{{overflow:auto;display:flex;justify-content:center}}.trajectory svg{{display:block;width:min(100%,560px);height:auto;min-width:500px}}.trajectory-lines line{{stroke:var(--line);stroke-width:2}}.trajectory-node rect{{fill:#20252c;stroke:var(--line);stroke-width:2}}.trajectory-node.open rect{{stroke:var(--yellow)}}.trajectory-node.proved rect{{stroke:var(--green)}}.trajectory-node.active rect{{fill:#202b35;stroke:var(--blue);stroke-width:3}}.trajectory-node text,.trajectory-center text{{fill:var(--text);font:600 13px system-ui,sans-serif;text-anchor:middle}}.trajectory-node .node-state,.trajectory-center .node-state{{fill:var(--muted);font-size:10px;font-weight:500}}.trajectory-center rect{{fill:#111820;stroke:var(--blue);stroke-width:3}}.trajectory-note{{color:var(--muted);font-size:11px;text-align:center;line-height:1.5;padding:0 8px 4px}}footer{{display:flex;gap:25px;flex-wrap:wrap;color:var(--muted);padding:14px 4px}}footer em{{font-style:normal;color:#747c87;margin-left:5px}}.document{{padding:22px}}@media(max-width:900px){{.status{{grid-template-columns:1fr 1fr 1fr}}}}@media(max-width:600px){{.status{{grid-template-columns:1fr 1fr}}header span{{display:none}}.activity{{grid-template-columns:1fr}}.activity span{{white-space:normal}}.trajectory svg{{min-width:460px}}}}
.status{{grid-template-columns:repeat(6,1fr)}}
.trajectory-observations{{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:6px;margin-top:10px}}.trajectory-observations span{{display:flex;flex-direction:column;min-width:0;padding:7px 9px;border:1px solid var(--line);border-radius:5px;color:var(--muted);font-size:10px;line-height:1.35}}.trajectory-observations b{{color:var(--text);font-size:10px;font-weight:600;text-transform:capitalize;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}}.attention-panel{{min-width:0;padding:12px 12px 10px;border:1px solid var(--line);border-radius:6px;background:#151a20;overflow:auto}}.attention-heading{{display:flex;align-items:baseline;justify-content:space-between;gap:12px}}.attention-heading h3{{margin:0;font-size:13px}}.attention-heading span{{color:var(--muted);font-size:12px}}.attention-panel svg{{display:block;width:min(100%,760px);height:auto;min-width:540px;margin:auto}}.attention-grid polygon{{fill:none;stroke:#303741;stroke-width:1}}.attention-grid line{{stroke:#303741;stroke-width:1}}.attention-series polygon{{stroke-width:2.5;stroke-linejoin:round}}.attention-series circle{{stroke:none}}.attention-target polygon{{fill:none;stroke:#eee9df;stroke-dasharray:6 5}}.attention-target circle{{fill:#eee9df}}.attention-code polygon{{fill:rgba(117,167,216,.13);stroke:var(--blue)}}.attention-code circle{{fill:var(--blue)}}.attention-test polygon{{fill:rgba(240,188,40,.09);stroke:var(--yellow)}}.attention-test circle{{fill:var(--yellow)}}.attention-result polygon{{fill:rgba(189,128,214,.08);stroke:#bd80d6}}.attention-result circle{{fill:#bd80d6}}.attention-other polygon{{fill:none;stroke:#aab0b8}}.attention-other circle{{fill:#aab0b8}}.attention-legend{{display:flex;justify-content:center;gap:8px 13px;flex-wrap:wrap;color:var(--muted);font-size:12px}}.attention-legend span{{white-space:nowrap}}.attention-key{{display:inline-block;width:14px;height:3px;margin:0 5px 3px 0;border-radius:3px}}.attention-key.attention-target{{background:#eee9df}}.attention-key.attention-code{{background:var(--blue)}}.attention-key.attention-test{{background:var(--yellow)}}.attention-key.attention-result{{background:#bd80d6}}.attention-key.attention-other{{background:#aab0b8}}.attention-claim{{display:flex;justify-content:center;align-items:baseline;gap:9px;margin-top:7px}}.attention-claim strong{{font-size:14px}}.attention-claim span{{color:var(--muted);font-size:11px}}.attention-panel>p{{margin:6px 0 0;text-align:center;color:var(--muted);font-size:10px;line-height:1.4}}
.gap-explanations{{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:8px;max-width:900px;margin:14px auto 0}}.gap-explanation{{position:relative;margin:0;padding:10px 12px 11px;border:1px solid var(--line);border-left:4px solid var(--yellow);border-radius:6px;background:#171c22}}.gap-explanation.proved{{border-left-color:var(--green)}}.gap-explanation.active{{border-left-color:var(--blue);background:#18212a}}.gap-explanation div{{display:flex;justify-content:space-between;gap:8px;margin-bottom:6px}}.gap-explanation div span{{font-size:12px;font-weight:700}}.gap-explanation .gap-state{{font-size:11px;font-weight:750;line-height:1.2;text-transform:capitalize}}.gap-explanation.open .gap-state{{color:var(--yellow)}}.gap-explanation.active .gap-state{{color:var(--blue)}}.gap-explanation.proved .gap-state{{color:var(--green)}}.gap-explanation p{{margin:5px 0 0;color:var(--muted);font-size:11px;font-weight:400;line-height:1.45}}
.fratbro h2 em{{color:var(--yellow);font-size:11px;font-style:normal;margin-left:6px}}.fratbro>div{{display:grid;grid-template-columns:130px 1fr;gap:12px;padding:7px 0;border-top:1px solid var(--line)}}.fratbro>div:first-of-type{{border-top:0}}.fratbro>div strong{{font-size:12px;color:var(--muted)}}.fratbro>div span{{line-height:1.5}}
@media(max-width:1000px){{.status{{grid-template-columns:1fr 1fr 1fr}}}}
@media(max-width:700px){{.gap-explanations{{grid-template-columns:1fr}}.attention-heading{{align-items:flex-start;flex-direction:column}}}}
@media(max-width:600px){{.status{{grid-template-columns:1fr 1fr}}}}

/* Owner overview: quiet chrome, clear type, source details on dedicated reading pages. */
:root{{--bg:#0c1116;--panel:#121a22;--line:#2b3945;--text:#f1f3ef;--muted:#adbbc5;--blue:#7edac9;--green:#8fd6ab;--yellow:#e8bd74}}
body{{background:radial-gradient(ellipse at 90% 0%,#16302f55,transparent 55%),var(--bg);font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif}}
main{{max-width:1200px;padding:48px 36px}}
header{{justify-content:space-between;align-items:center;gap:20px}}
header h1{{font-size:38px;letter-spacing:-2px;font-weight:750}}header h1 .brand-dot{{color:var(--blue);font-size:38px}}
header>span{{font-size:12px;letter-spacing:.03em}}
nav{{margin:26px 0 30px;gap:20px;align-items:center}}
nav a{{padding:12px 0;font-size:13px}}
nav a.selected{{border:0;border-bottom:2px solid var(--blue);border-radius:0;color:var(--text)}}
.status{{grid-template-columns:repeat(6,minmax(0,1fr));gap:0;border-block:1px solid var(--line);padding:22px 0}}
.lamp,.metric{{background:none;border:0;border-radius:0;padding:0 18px;min-height:82px;border-right:1px solid var(--line)}}
.lamp:first-child{{padding-left:0}}.metric:last-child{{border-right:0}}
.lamp small,.metric small{{text-transform:uppercase;font-size:10px;letter-spacing:.12em;margin-bottom:12px}}
.lamp strong,.metric strong{{font-size:20px;font-weight:600;letter-spacing:-.03em;overflow-wrap:anywhere}}
.lamp .dot{{width:12px;height:12px;float:right;margin:2px 0 0 5px}}
.metric-note{{font-size:10px;white-space:normal;line-height:1.5;margin-top:8px}}
section{{border:0;border-radius:0;background:none;padding:28px 0;margin-top:14px}}
h2{{font-size:22px;letter-spacing:-.035em;font-weight:600;margin-bottom:20px}}
.workers{{display:flex;align-items:center;gap:40px;padding:22px 0;border-bottom:1px solid var(--line);margin:0}}
.workers h2{{margin:0;font-size:12px;text-transform:uppercase;letter-spacing:.1em;color:var(--muted)}}
.roster{{display:flex;gap:42px}}.roster-member{{display:flex;gap:12px;align-items:center;min-width:175px}}
.roster-avatar{{display:grid;place-items:center;width:34px;height:34px;border:1px solid #3b605b;border-radius:50%;color:var(--blue);font-size:13px}}
.roster-member strong{{display:block;font-size:14px}}.roster-member div>span{{display:block;font-size:11px;color:var(--muted);margin-top:4px}}
.roster-member b{{font-size:22px;font-weight:400;color:var(--blue);margin-left:16px}}
.trajectory{{padding:30px 24px;background:linear-gradient(150deg,#18292c55,#121a2244);border:1px solid #2d4245;border-radius:16px;margin-top:28px}}
.fratbro{{border-left:2px solid var(--blue);padding:6px 0 6px 25px;margin:38px 0 28px;max-width:880px}}
.eyebrow{{font-size:10px;letter-spacing:.16em;color:var(--blue);font-weight:650;margin-bottom:10px}}
.fratbro .eyebrow{{display:block}}.fratbro>div.brief-grid{{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:24px;border:0;padding:0}}.brief-field small{{color:var(--blue);font-size:10px;text-transform:uppercase;letter-spacing:.1em;margin-bottom:8px}}.brief-field p{{font-size:15px;line-height:1.7;margin:0}}.fratbro h2{{font-size:20px;margin-bottom:12px}}.brief-lead{{font-size:17px;line-height:1.75;color:#e0e9e8;margin:0 0 14px}}
.text-link{{color:var(--blue);text-decoration:none;font-size:12px;font-weight:550;display:inline-block;padding:8px 0}}
.text-link:hover{{text-decoration:underline;color:#b6ffec}}
.activity{{border:0;border-block:1px solid var(--line);border-radius:0;background:none;padding:18px 0;grid-template-columns:105px max-content 1fr max-content}}
.activity span{{white-space:normal;font-size:12px;line-height:1.6;color:#ccd5dc}}
.work-section{{padding:32px 0 0}}.work-section h2{{font-size:27px;margin-bottom:12px}}
.work-section>.subtle{{font:11px ui-monospace,monospace;margin-bottom:24px}}
.ledger-list{{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:18px}}
.work-card{{padding:24px;background:linear-gradient(135deg,#1b2a32,#141d25);border:1px solid #334753;border-radius:12px;display:flex;flex-direction:column;align-items:flex-start}}
.work-meta{{display:flex;justify-content:space-between;gap:20px;width:100%;font:11px ui-monospace,monospace;color:var(--blue);margin-bottom:18px}}
.work-state{{font:10px -apple-system,sans-serif;color:#e8bd74;letter-spacing:.03em}}
.work-card.complete{{background:#121b22;border-color:#283640}}.complete .work-state{{color:var(--green)}}
.work-card h3{{font-size:17px;line-height:1.6;font-weight:550;color:#f1f4f5;margin:0 0 18px}}
.completed-reference{{grid-column:1/-1;color:var(--muted);text-decoration:none;font-size:12px;padding:12px 0}}.work-card .text-link{{margin-top:auto}}.empty-state{{font-size:14px;color:var(--muted);padding:4px 0}}
.document{{max-width:900px;font-size:16px;line-height:1.8}}.document p,.document li{{line-height:1.85;color:#dbe4ea}}.document h2{{margin-top:34px}}.document code{{overflow-wrap:anywhere}}
footer{{border-top:1px solid var(--line);margin-top:42px;font-size:10px;gap:15px}}
footer em{{display:inline;color:var(--muted);overflow-wrap:anywhere}}footer span{{cursor:default}}
a:focus-visible{{outline:2px solid var(--blue);outline-offset:5px}}
@media(max-width:900px){{.status{{grid-template-columns:repeat(3,1fr);gap:24px 0}}.lamp:nth-child(4){{padding-left:0}}.lamp:nth-child(3){{border:0}}.ledger-list{{grid-template-columns:1fr}}}}
@media(max-width:600px){{main{{padding:24px 20px}}.status{{grid-template-columns:repeat(2,1fr)}}.lamp,.metric{{padding:0 12px}}.workers{{align-items:flex-start;flex-direction:column;gap:18px}}.roster{{gap:20px;flex-wrap:wrap}}.roster-member{{min-width:130px}}.activity{{grid-template-columns:1fr;gap:8px}}.fratbro{{padding-left:18px}}.trajectory{{padding:20px 12px}}.work-card{{padding:20px}}.lamp strong,.metric strong{{font-size:18px}}.attention-panel{{min-width:0}}}}


.roster{{flex:1;justify-content:space-between;gap:24px}}.roster-member{{flex:1;min-width:0}}.roster-member>b{{display:none}}
.effort-strip{{display:flex;gap:5px;margin-top:8px;flex-wrap:wrap}}
.effort-chip{{display:inline-flex;gap:7px;align-items:center;border:1px solid #35414a;border-radius:4px;padding:4px 7px;font-size:10px;color:#aab6c0}}
.effort-chip b{{font-size:10px;font-weight:500;color:#aab6c0;margin:0}}
.effort-chip.engaged{{background:#22483e;border-color:#75c8ab;color:#dcfff1}}.effort-chip.engaged b{{color:#dcfff1}}
@media(max-width:700px){{.workers{{display:block}}.workers h2{{margin-bottom:18px}}.roster{{display:flex;flex-wrap:wrap;gap:22px}}.roster-member{{flex-basis:100%}}}}

.lamp .dot.green{{background:#39e878;box-shadow:0 0 0 3px #39e87818}}.lamp .dot.yellow{{background:#ffc44d}}.lamp .dot.grey{{background:#68717a}}

.workers{{display:block;padding:24px 0 16px}}.workers h2{{margin-bottom:20px}}
.roster-scales{{display:grid;grid-template-columns:1fr 1fr;gap:48px}}
.scale-heading{{display:flex;justify-content:space-between;align-items:baseline;padding:0 10px}}
.scale-heading strong{{font-size:15px}}.scale-heading span{{font-size:11px;color:var(--muted)}}
.scale-heading b{{font-size:23px;font-weight:500;color:var(--text);margin-right:5px}}
.worker-scale svg{{display:block;width:100%;overflow:visible}}
.worker-scale svg.model-emblem{{width:36px;height:36px;margin:8px 10px 0}}
.strength-axis{{stroke:#36464e;stroke-width:1}}.strength-stop{{fill:#68777f}}
.worker-dot{{fill:#7ee6c2;stroke:var(--bg);stroke-width:1}}
.worker-scale-terra .worker-dot{{fill:#a8baff}}
.worker-scale-astra .worker-dot{{fill:#ffe89a;filter:drop-shadow(0 0 4px #ffdc62)}}
.worker-scale-astra .model-emblem{{filter:drop-shadow(0 0 6px #ffdc62)}}
.strength-label{{fill:#aebdc6;font:11px -apple-system,sans-serif;text-anchor:middle}}
.strength-overflow{{fill:var(--text);font:10px -apple-system,sans-serif;text-anchor:middle}}
@media(max-width:650px){{.roster-scales{{grid-template-columns:1fr;gap:20px}}.worker-scale svg{{max-height:125px}}}}

</style></head><body><main><header><h1>de67<span class="brand-dot">.</span></h1><span>{_escape(self.workspace.name)}</span></header>{nav}{body}<footer>{''.join(source_bits)}</footer></main></body></html>'''
        return page.encode("utf-8")


def serve(workspace: Path, bind: str, port: int, refresh_seconds: int,
          sessions_root: Path | None = None,
          sidecar_script: Path | None = None,
          fratbro_script: Path | None = None,
          fratbro_cache: Path | None = None,
          fratbro_codex: str = "codex") -> None:
    dashboard = Dashboard(workspace.resolve(), refresh_seconds, sessions_root, sidecar_script,
                          fratbro_script, fratbro_cache, fratbro_codex)

    class Server(ThreadingHTTPServer):
        def server_bind(self) -> None:
            # HTTPServer normally reverse-resolves the bind address only to populate
            # server_name. That can stall service startup on offline/home DNS.
            socketserver.TCPServer.server_bind(self)
            self.server_name = str(self.server_address[0])
            self.server_port = int(self.server_address[1])

    class Handler(BaseHTTPRequestHandler):
        def do_GET(self) -> None:  # noqa: N802
            path = self.path.split("?", 1)[0]
            if path not in ("/", "/dfs", "/ledger", "/briefing"):
                self.send_error(404)
                return
            try:
                payload = dashboard.render(path.strip("/") or "overview")
                self.send_response(200)
                self.send_header("Content-Type", "text/html; charset=utf-8")
                self.send_header("Content-Length", str(len(payload)))
                self.send_header("Cache-Control", "no-store")
                self.send_header("Content-Security-Policy", "default-src 'none'; style-src 'unsafe-inline'; base-uri 'none'; form-action 'none'; frame-ancestors 'none'")
                self.send_header("X-Content-Type-Options", "nosniff")
                self.end_headers()
                self.wfile.write(payload)
            except Exception as error:
                payload = f"Dashboard display error: {_escape(error)}".encode("utf-8")
                self.send_response(503)
                self.send_header("Content-Type", "text/plain; charset=utf-8")
                self.send_header("Content-Length", str(len(payload)))
                self.end_headers()
                self.wfile.write(payload)

        def log_message(self, format: str, *args: Any) -> None:
            print(f"dashboard: {format % args}")

    server = Server((bind, port), Handler)
    print(f"de67 dashboard: http://{bind}:{server.server_port} ({workspace})")
    server.serve_forever()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workspace", required=True, type=Path)
    parser.add_argument("--bind", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8767)
    parser.add_argument("--refresh-seconds", type=int, default=0)
    parser.add_argument("--codex-sessions", type=Path, default=None,
                        help="Codex session root used for optional active-worker counts")
    parser.add_argument("--sidecar-script", type=Path, default=None,
                        help="Optional de67 trajectory_sidecar.py path")
    parser.add_argument("--fratbro-script", type=Path, default=None,
                        help="Optional Luna-medium fratbro_narrator.py path")
    parser.add_argument("--fratbro-cache", type=Path, default=None,
                        help="Optional narrator cache outside the configured workspace")
    parser.add_argument("--fratbro-codex", default="codex",
                        help="Codex executable used only by the optional narrator")
    args = parser.parse_args()
    if args.refresh_seconds < 0:
        parser.error("--refresh-seconds cannot be negative")
    if bool(args.fratbro_script) != bool(args.fratbro_cache):
        parser.error("--fratbro-script and --fratbro-cache must be configured together")
    serve(args.workspace, args.bind, args.port, args.refresh_seconds,
          args.codex_sessions, args.sidecar_script, args.fratbro_script, args.fratbro_cache,
          args.fratbro_codex)


if __name__ == "__main__":
    main()
