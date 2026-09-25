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


def render_trajectory(report: dict[str, Any], briefing: dict[str, Any] | None = None,
                      *, stale: bool = False, error: str | None = None,
                      context: dict[str, Any] | None = None) -> str:
    if briefing is not None and not isinstance(briefing, dict):
        briefing = {"error": "invalid briefing cache"}
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
    headline, details = briefing_content(briefing or {})
    live = bool(axes) and not stale and (report.get("latest_task_result") == "active"
                                      or any(item.get("status") == "active" for item in subtasks))
    state = ("Last recorded trajectory · update unavailable" if stale else
             "Active trajectory" if live else "Recorded trajectory" if axes else "No active trajectory")
    context_label = " · ".join(
        f"{label} {_escape(value)}" for label, value in (context or {}).items()
        if value is not None and value != ""
    )
    header_label = context_label or (state if axes or stale else "")
    state_label = f"<span>{header_label}</span>" if header_label else ""
    notices = []
    if error and error != "no active claim":
        notices.append(f'<p class="radar-notice">Trajectory unavailable · {_escape(error)}</p>')
    if briefing and (briefing.get("stale") or briefing.get("error")):
        notices.append('<p class="radar-notice">Briefing update unavailable'
                       + (' · showing the last saved briefing.' if headline else '.') + '</p>')
    return (
        '<section class="trajectory"><header class="radar-briefing">'
        f'<div class="radar-kicker">NAVIGATION {state_label}</div>'
        f'<h2><strong>{_escape(headline or ("Tracking " + str(report.get("claim", "the current work")) if axes else "Standing by for the next trajectory."))}</strong></h2></header>'
        f'{"".join(notices)}'
        f'{render_attention_spider(report, axes, gaps, bool(subtasks), live=live)}'
        f'<div class="radar-details">{details}</div>'
        '</section>'
    )


def briefing_content(value: dict[str, Any]) -> tuple[str, str]:
    """Keep the recorded headline and every detail in the combined radar panel."""
    summary = value.get("summary") if isinstance(value, dict) else None
    if isinstance(summary, dict) and "headline" in summary:
        fields = "".join(
            f'<div class="radar-detail"><h3>{label}</h3><p>{_escape(summary.get(key, ""))}</p></div>'
            for key, label in (("changed", "What changed"), ("next", "Next"), ("snag", "Obstacle"))
            if summary.get(key)
        )
        return str(summary.get("headline", "")), fields
    if isinstance(summary, dict):
        parts = [str(summary.get(key, "")).strip()
                 for key in ("cooking", "changed", "snag", "next", "health")
                 if str(summary.get(key, "")).strip()]
    elif isinstance(summary, str):
        parts = [part for part in re.split(r"(?<=[.!?])\s+", summary.strip(), maxsplit=1) if part]
    else:
        parts = []
    if not parts:
        return "", ""
    return parts[0], "".join(f'<p>{_escape(part)}</p>' for part in parts[1:])


def worker_dot_positions(count: int) -> list[tuple[float, float]]:
    """Owner-requested display capacity: 12 dots; excess is labelled separately."""
    visible = min(max(0, count), 12)
    if visible == 1:
        return [(0.0, 0.0)]
    radius = 12 if visible <= 6 else 22
    return [(radius * math.cos(-math.pi / 2 + 2 * math.pi * i / visible),
             radius * math.sin(-math.pi / 2 + 2 * math.pi * i / visible))
            for i in range(visible)]


def solar_filaments() -> str:
    """A stable magnetic field; activity changes its visibility, not its geometry."""
    import random
    rng = random.Random(6701)
    paths = []
    def point(radius: float, angle: float) -> str:
        return f"{160 + radius * math.cos(angle):.2f} {160 + radius * math.sin(angle):.2f}"
    for index in range(64):
        angle = math.radians(index * 360 / 64 + rng.uniform(-3, 3))
        spread = rng.uniform(.025, .12)
        height = rng.uniform(138, 178)
        paths.append(f'<path d="M{point(121, angle - spread)} C{point(height, angle - spread * 2)} {point(height, angle + spread * 2)} {point(122, angle + spread)}" stroke-width="{rng.uniform(.6, 1.6):.2f}" opacity="{rng.uniform(.18, .65):.2f}"/>')
    return ''.join(paths)


def worker_emblem(model: str) -> str:
    emblem = ('<path fill="#8ff0cf" stroke="#d5fff0" stroke-width="1.2" stroke-linejoin="round" d="M25 4A15 15 0 1 0 25 32C13 29 13 7 25 4Z"/>'
              if model == "luna" else
              '<circle cx="18" cy="18" r="10" fill="#f2bd63"/>'
              '<path d="M18 1v5m0 24v5M1 18h5m24 0h5M6 6l4 4m16 16 4 4M30 6l-4 4M10 26l-4 4" '
              'stroke="#ffe0a1" stroke-width="2" stroke-linecap="round"/>'
              if model == "sol" else '')
    if model == "astra":
        emblem = '<path fill="#fff0d6" d="m18 2 3.7 11.2L34 13l-9.8 7.4L28 32l-10-6.6L8 32l3.8-11.6L2 13l12.3.2Z"/><circle cx="18" cy="18" r="3" fill="#fffbe5"/>'
    return emblem


def render_worker_scale(counts: dict[str, dict[str, int]]) -> str:
    models = ("astra", "sol", "luna")
    levels = ("low", "medium", "high", "xhigh", "max")
    marks = ['<line class="strength-axis" x1="48" y1="114" x2="348" y2="114"/>']
    for index, level in enumerate(levels):
        x = 48 + index * 300 // (len(levels) - 1)
        active = [model for model in models for _ in range(max(0, counts.get(model, {}).get(level, 0)))]
        marks.append(f'<circle class="strength-stop" cx="{x}" cy="114" r="2"/>')
        for model, (dx, dy) in zip(active, worker_dot_positions(len(active))):
            marks.append(f'<g class="worker-dot" data-model="{model}" transform="translate({x + dx - 6:.3f} {58 + dy - 6:.3f})">'
                         f'<title>{model} · {level} reasoning</title>'
                         f'<svg width="12" height="12" viewBox="0 0 36 36">{worker_emblem(model)}</svg></g>')
        if len(active) > 12:
            marks.append(f'<text class="strength-overflow" x="{x}" y="8">+{len(active) - 12}</text>')
        marks.append(f'<text class="strength-label" x="{x}" y="156">{level}</text>')
    legend = []
    descriptions = []
    for model in models:
        description = model + ": " + ", ".join(f"{level}: {counts.get(model, {}).get(level, 0)}" for level in levels)
        descriptions.append(description)
        legend.append(f'<div role="img" aria-label="{_escape(description)}" title="{_escape(description)}"><span>{model}</span>'
                      f'<svg class="model-emblem" viewBox="0 0 36 36" aria-hidden="true">{worker_emblem(model)}</svg></div>')
    return ('<div class="worker-scale">'
            f'<svg class="shared-worker-axis" viewBox="0 0 396 170" role="img" aria-label="{_escape("; ".join(descriptions))}">'
            + "".join(marks) + '</svg><div class="worker-legend">' + "".join(legend) + '</div></div>')


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
    *, live: bool = False,
) -> str:
    gap_ids = [
        str(axis.get("subtask_id" if uses_subtasks else "gap_id", "?"))
        for axis in axes_data
    ]
    raw_series = report.get("attention")
    series = [item for item in raw_series if isinstance(item, dict)] if isinstance(raw_series, list) else []
    radius = max(174, len(gap_ids) * 12)
    size = radius * 2 + 100
    center = size / 2

    def point(index: int, distance: float) -> tuple[float, float]:
        angle = -math.pi / 2 + (2 * math.pi * index / max(1, len(gap_ids)))
        return center + distance * math.cos(angle), center + distance * math.sin(angle)

    grid: list[str] = []
    for fraction in (0.25, 0.5, 0.75, 1.0):
        grid.append(f'<circle cx="{center}" cy="{center}" r="{radius * fraction}" />')
    for bearing in range(0, 360, 5):
        angle = math.radians(bearing)
        inner = radius + (13 if bearing % 30 == 0 else 18)
        outer = radius + 23
        grid.append(f'<line x1="{center + inner * math.cos(angle):.1f}" y1="{center + inner * math.sin(angle):.1f}" '
                    f'x2="{center + outer * math.cos(angle):.1f}" y2="{center + outer * math.sin(angle):.1f}" />')
    axes: list[str] = []
    nodes: list[str] = []
    node_data: list[tuple[str, str, str, str, str, float, float]] = []
    cards: dict[str, list[tuple[float, str]]] = {"left": [], "right": []}
    latest_gap = str(report.get("latest_task_gap") or "")
    for index, (gap_id, gap) in enumerate(zip(gap_ids, axes_data)):
        x, y = point(index, radius)
        axes.append(f'<line x1="{center:.1f}" y1="{center:.1f}" x2="{x:.1f}" y2="{y:.1f}" />')
        status = str(gap.get("status", "open"))
        active = status == "active" if uses_subtasks else (
            gap_id == latest_gap and report.get("latest_task_result") == "active"
        )
        tone = "active" if active else "proved" if status in {"proved", "done"} else "open"
        summary = " ".join(str(gap.get("summary", "No explanation recorded.")).split())
        label = gap_id + ("" if uses_subtasks else " r" + str(gap.get("revision", "?")))
        state = ("active" if active else status) + ("" if uses_subtasks else " · " + str(gap.get("attempts", 0)) + " attempts")
        contact = f"{index + 1:02d}"
        node_data.append((label, state, summary, tone, contact, x, y))
        side = "left" if x < center - 1 or (abs(x - center) <= 1 and index == 0) else "right"
        cards[side].append((y,
            f'<article id="trajectory-contact-{index}" class="radar-contact {tone}" data-radar-index="{index}" style="--contact-order:{index + 1}">'
            f'<div class="radar-contact-heading"><span class="contact-number">{contact}</span>'
            f'<span class="contact-id">{_escape(label)}</span></div>'
            f'<p>{_escape(summary)}</p><span class="contact-state">{_escape(state)}</span></article>'
        ))

    shapes: list[str] = []
    headings: list[str] = []
    destinations: list[str] = []
    measurements: dict[int, list[str]] = {}
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
            measurements.setdefault(index, []).append(
                f'{item.get("label", key)} · relative {relative:.2f} · cosine {raw:.3f}')
            x, y = point(index, radius * relative)
            coordinates.append(f"{x:.1f},{y:.1f}")
            circles.append(
                f'<circle cx="{x:.1f}" cy="{y:.1f}" r="3"><title>{_escape(str(item.get("label", key)))} '
                f'→ {_escape(gap_id)} · relative {relative:.2f} · cosine {raw:.3f}</title></circle>'
            )
            if key == "target" and relative > 0:
                course = f'M{center:.1f},{center:.1f} L{x:.1f},{y:.1f}'
                ship_x, ship_y = point(index, radius * relative * .78)
                bearing = math.degrees(math.atan2(y - center, x - center)) + 90
                headings.append(
                    f'<g class="assigned-bearing{" heading-live" if live else ""}" data-assigned-index="{index}">'
                    f'<title>{"Assigned to" if live else "Last assignment"} {_escape(gap_id)}</title>'
                    f'<path class="assigned-course-halo" d="{course}" />'
                    f'<path class="assigned-course" d="{course}" />'
                    f'<g class="assigned-ship" transform="translate({ship_x:.1f} {ship_y:.1f}) rotate({bearing:.1f})">'
                    '<path class="ship-exhaust" d="M-3 9 L0 21 L3 9Z" />'
                    '<path d="M0 -12 L8 9 L0 5 L-8 9Z" /></g></g>'
                )
                destinations.append(f'<a href="#trajectory-contact-{index}">{index + 1:02d} · {_escape(gap_id)}</a>')
        label = str(item.get("label", key))
        source = str(item.get("source", ""))
        if key != "target":
            shapes.append(
                f'<g class="attention-series attention-{css_key}"><polygon points="{" ".join(coordinates)}">'
                f'<title>{_escape(label)} · {_escape(source)}</title></polygon>{"".join(circles)}</g>'
            )
        legend.append(
            f'<span title="{_escape(source)}"><i class="attention-key attention-{css_key}"></i>{_escape(label)}</span>'
        )
    for index, (label, state, summary, tone, contact, x, y) in enumerate(node_data):
        # Boundary markers cover maximum-attention points; retain all their values
        # in the marker tooltip as well as on the individual series points.
        tooltip = " · ".join([label, state, summary, *measurements.get(index, [])])
        nodes.append(
            f'<a href="#trajectory-contact-{index}" class="trajectory-node {tone}">'
            f'<title>{_escape(tooltip)}</title>'
            f'<circle data-radar-marker="{index}" cx="{x:.1f}" cy="{y:.1f}" r="15" />'
            f'<text x="{x:.1f}" y="{y + 4:.1f}">{contact}</text></a>'
        )
    gap_cards: list[str] = []
    # The side cards already contain gap descriptions unless the map uses subtasks.
    for gap in gaps if uses_subtasks else []:
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
        f'{"Subtask constellation" if uses_subtasks else "Work constellation"}</h3>'
        f'<span>{"Relative pull · not completion" if series else "Waiting for attention data"}</span></div>'
        f'<div class="radar-stage{" radar-idle" if not axes_data else ""}">'
        '<svg class="radar-links" aria-hidden="true"></svg>'
        '<div class="radar-scope">'
        f'<svg class="radar-map" viewBox="0 0 {size} {size}" role="img" aria-label="Attention distribution across '
        f'{"ledger subtasks" if uses_subtasks else "closure gaps"}">'
        f'<circle class="radar-disc" cx="{center}" cy="{center}" r="{radius + 27}" />'
        f'<g class="attention-grid">{"".join(grid)}{"".join(axes)}</g>'
        f'<path class="radar-origin" d="M{center - 8} {center}h16 M{center} {center - 8}v16" />'
        f'{"".join(shapes)}{"".join(headings)}<g class="attention-nodes">{"".join(nodes)}</g></svg>'
        f'{"<p class=radar-standby>No active trajectory<br><span>Awaiting work coordinates</span></p>" if not axes_data else ""}'
        '</div>'
        f'<div class="radar-contacts radar-left">{"".join(card for _, card in sorted(cards["left"]))}</div>'
        f'<div class="radar-contacts radar-right">{"".join(card for _, card in sorted(cards["right"]))}</div></div>'
        + (f'<div class="assigned-destination"><span>{"Assigned to" if live else "Last assignment"}</span>'
           f'{"".join(destinations)}</div>' if destinations else '') +
        f'<div class="attention-legend">{"".join(legend)}</div>'
        + (f'<div class="attention-claim"><strong>{_escape(report.get("claim", ""))}</strong>'
           f'<span>{_escape(report.get("latest_task") or "No active attempt")}</span></div>'
           if report.get("claim") or report.get("latest_task") else '') +
        f'<div class="gap-explanations">{"".join(gap_cards)}</div>'
        + ('<p>Each line is scaled to its own strongest work item. Hover a point for raw cosine similarity.</p>' if series else '') +
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
            f"WHERE claim.lineage_id = ? AND claim.released_at IS NULL{where}",
            (clock["lineage"],),
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
                runs = connection.execute(
                    "SELECT run_id FROM supervisor_attempts "
                    "WHERE lineage_id=? AND finished_at IS NULL",
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


def native_mutator_state(workspace: Path, refresh_seconds: int) -> dict[str, Any] | None:
    config = workspace / ".de67/state/workspace.json"
    if not config.exists() or json.loads(config.read_text()).get("persistent_mutator") is not True:
        return None
    path = workspace / ".de67/state/mutator-session.json"
    if not path.exists():
        return {"glowing": False, "status": "idle"}
    session = json.loads(path.read_text())
    address = workspace / ".de67/state/mutator-input.json"
    if session.get("state") == "active" and address.exists():
        binding = json.loads(address.read_text())
        if (binding.get("workspace") == str(workspace.resolve())
                and binding.get("thread_id") == session.get("thread_id")
                and binding.get("state") == "active"):
            try:
                os.kill(binding["runner_pid"], 0)
                os.kill(binding["server_pid"], 0)
                return {"glowing": True, "status": session.get("mode", "working")}
            except ProcessLookupError:
                pass
    if (session.get("result") == "completed"
            and 0 <= time.time() - session.get("updated_at", 0) <= refresh_seconds):
        return {"glowing": True, "status": "replied"}
    return {"glowing": False, "status": "idle"}


def openclaw_mutator_state(database: Path, refresh_seconds: int) -> dict[str, Any]:
    """Read the dedicated mutator agent's activity, never its message contents."""
    uri = f"file:{quote(str(database.resolve()), safe='/:')}?mode=ro"
    connection = sqlite3.connect(uri, uri=True, timeout=0)
    try:
        rows = connection.execute(
            "SELECT windows.status, windows.ended_at FROM session_nodes AS nodes "
            "JOIN session_windows AS windows ON windows.session_id=nodes.current_session_id "
            "WHERE nodes.archived_at IS NULL"
        ).fetchall()
        queued = connection.execute(
            "SELECT 1 FROM session_pending_inputs AS inputs "
            "JOIN session_nodes AS nodes ON inputs.session_id=nodes.current_session_id "
            "WHERE nodes.archived_at IS NULL AND inputs.state='queued' "
            "AND inputs.consumed_event_id IS NULL LIMIT 1"
        ).fetchone()
    finally:
        connection.close()
    if any(status == "running" for status, _ in rows):
        return {"glowing": True, "status": "working"}
    if queued:
        return {"glowing": True, "status": "queued"}
    # Keep quick replies visible for one normal refresh interval as well.
    if any(status == "done" and ended is not None
           and 0 <= time.time() - ended / 1000 <= refresh_seconds for status, ended in rows):
        return {"glowing": True, "status": "replied"}
    return {"glowing": False, "status": "idle"}


def _reverse_session_lines(path: Path):
    """Read complete lines backward without a fixed activity-history cutoff."""
    with path.open("rb") as source:
        position = source.seek(0, os.SEEK_END)
        pending = b""
        while position:
            size = min(position, 65536)
            position -= size
            source.seek(position)
            lines = (source.read(size) + pending).split(b"\n")
            pending = lines[0]
            for line in reversed(lines[1:]):
                yield line.decode("utf-8", errors="replace")
        if pending:
            yield pending.decode("utf-8", errors="replace")


def _tool_is_waiting(payload: dict[str, Any]) -> bool:
    name = str(payload.get("name", "")).rsplit(".", 1)[-1]
    if name in ("wait_agent", "wait_threads", "sleep", "wait", "request_user_input"):
        return True
    try:
        arguments = json.loads(payload.get("arguments", "{}"))
    except (ValueError, TypeError):
        arguments = {}
    if name == "write_stdin":
        return not arguments.get("chars")
    if name == "exec":
        # Code-mode wraps the same shell call in JavaScript. Decode only its
        # literal command; never execute trace contents to discover activity.
        source = str(payload.get("input", ""))
        match = re.search(r'(?:\bcmd|"cmd")\s*:\s*("(?:\\.|[^"\\])*")', source)
        if not match or "tools.exec_command(" not in source:
            if "tools.write_stdin(" not in source:
                return False
            chars = re.search(r'(?:\bchars|"chars")\s*:', source)
            return chars is None or bool(re.match(r'\s*(?:""|\'\')\s*[,}]', source[chars.end():]))
        try:
            arguments = {"cmd": json.loads(match[1])}
        except ValueError:
            return False
    elif name != "exec_command":
        return False
    try:
        lexer = shlex.shlex(arguments.get("cmd", ""), posix=True, punctuation_chars=";&|")
        lexer.whitespace_split = True
        argv = list(lexer)
    except ValueError:
        return False
    if (len(argv) < 3 or not re.fullmatch(r"python(?:\d+(?:\.\d+)*)?", Path(argv[0]).name)
            or Path(argv[1]).name != "worker_library.py"):
        return False
    rest = argv[2:]
    if len(rest) >= 2 and rest[0] == "--workspace":
        rest = rest[2:]
    return bool(rest and rest[0] == "wait" and not any(
        token in (";", "&&", "||", "|", "&") for token in rest))


def _session_activity(path: Path) -> str:
    """Project the latest execution signal, ignoring accounting and incoming mail."""
    for line in _reverse_session_lines(path):
        try:
            record = json.loads(line)
        except ValueError:
            continue
        payload = record.get("payload", {})
        kind = payload.get("type")
        if record.get("type") == "event_msg":
            if kind in ("task_complete", "turn_aborted"):
                return "waiting"
            if kind == "task_started":
                return "working"
        if record.get("type") != "response_item":
            continue
        if kind in ("function_call", "custom_tool_call"):
            return "waiting" if _tool_is_waiting(payload) else "working"
        if kind in ("function_call_output", "custom_tool_call_output", "reasoning"):
            return "working"
        if kind == "message" and payload.get("role") == "assistant":
            return "waiting" if payload.get("phase") == "final" else "working"
    return "unknown"


def coordinator_activity(workspace: Path, sessions_root: Path) -> str:
    session = _active_coordinator_id(workspace)
    if not session:
        return "unknown"
    index = sessions_root.parent / "state_5.sqlite"
    uri = f"file:{quote(str(index), safe='/:')}?mode=ro"
    connection = sqlite3.connect(uri, uri=True, timeout=0)
    try:
        row = connection.execute("SELECT rollout_path FROM threads WHERE id=?", (session,)).fetchone()
    finally:
        connection.close()
    return _session_activity(Path(row[0])) if row else "unknown"


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
    if row[2] != "gpt-6-astra" or row[3] not in ("low", "medium", "high", "max"):
        raise ValueError("active mutator model or reasoning effort unavailable")
    return {"id": str(row[0]), "effort": str(row[3])}


def worker_state(workspace: Path, sessions_root: Path) -> dict[str, Any]:
    """Project active roster subagents from Codex's existing read-only session records."""
    counts = {model: {effort: 0 for effort in ("low", "medium", "high", "xhigh", "max")}
              for model in ("luna", "sol", "astra")}
    mutator = _active_mutator(workspace, sessions_root)
    counted = {mutator["id"]} if mutator else set()
    if mutator:
        counts["astra"][mutator["effort"]] += 1
    active_claims = _active_worker_claims(workspace)
    if active_claims == {}:
        return {"counts": counts, "available": True}
    index = sessions_root.parent / "state_5.sqlite"
    if active_claims and index.is_file():
        owners = set(active_claims.values())
        if len(owners) != 1:
            raise ValueError("active worker ownership is ambiguous")
        uri = f"file:{quote(str(index), safe='/:')}?mode=ro"
        connection = sqlite3.connect(uri, uri=True, timeout=0)
        try:
            # Library workers have durable owners but need not have native
            # spawn edges. Seed their helper trees from those same claims.
            seeds = sorted(owners | set(active_claims))
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
    active_coordinator_id = _active_coordinator_id(workspace) or (
        next(iter(active_claims.values())) if active_claims else None
    )
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
    active_claims = _active_worker_claims(workspace)
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
            context = _trace_fuel(path)
            model = str(context.get("model", "")).lower().rsplit("-", 1)[-1]
            effort = str(context.get("effort", "")).lower()
            if model in counts and effort in counts[model] and candidate_id not in counted:
                counts[model][effort] += 1
                counted.add(candidate_id)
        if not changed:
            break
        pending = next_pending
    return {"counts": counts, "available": True, "error": None}



_TOKEN_TRACES: dict[tuple[str, bool], dict[str, Any]] = {}


def _trace_fuel(path: Path, *, windows: list[tuple[float, float | None]] | None = None) -> dict[str, Any]:
    """Read appended context and usage; retain the producing model for each delta."""
    from datetime import datetime
    key = (str(path), windows is not None)
    selection = None if windows is None else tuple(sorted(set(windows), key=lambda window: (
        window[0], float("inf") if window[1] is None else window[1])))
    stat = path.stat()
    cached = _TOKEN_TRACES.get(key)
    if cached is None or stat.st_size < cached["offset"] or cached["windows"] != selection:
        cached = {"offset": 0, "fresh": None, "observed": 0, "partial": False, "points": [],
                  "model": None, "effort": None, "worker_points": [], "windows": selection,
                  "worker_totals": {"astra": 0, "sol": 0, "luna": 0, "other": 0}}
        _TOKEN_TRACES[key] = cached

    def record(delta: int, timestamp: float | None = None) -> None:
        if selection is not None:
            if timestamp is None:
                cached["partial"] = True
                return
            if not any(start <= timestamp and (end is None or timestamp < end)
                       for start, end in selection):
                return
        model = str(cached["model"]).lower().rsplit("-", 1)[-1]
        role = model if model in ("astra", "sol", "luna") else "other"
        cached["observed"] += delta
        cached["worker_totals"][role] += delta
        if timestamp is not None:
            cached["points"].append((timestamp, delta))
            cached["worker_points"].append((timestamp, delta, role))

    with path.open("rb") as stream:
        stream.seek(cached["offset"])
        while True:
            line = stream.readline()
            if not line or not line.endswith(b"\n"):
                break
            cached["offset"] = stream.tell()
            if b'"token_count"' not in line and b'"turn_context"' not in line:
                continue
            try:
                item = json.loads(line)
                payload = item.get("payload", {})
                if item.get("type") == "turn_context":
                    cached["model"] = payload.get("model")
                    cached["effort"] = payload.get("effort")
                    continue
                if item.get("type") != "event_msg" or payload.get("type") != "token_count":
                    continue
                usage = (payload.get("info") or {}).get("total_token_usage") or {}
                fresh = int(usage["input_tokens"]) - int(usage["cached_input_tokens"]) + int(usage["output_tokens"])
                if fresh < 0:
                    continue
                timestamp = datetime.fromisoformat(item["timestamp"].replace("Z", "+00:00")).timestamp()
                previous = cached["fresh"]
                if selection is not None and not any(
                    start <= timestamp and (end is None or timestamp < end) for start, end in selection
                ):
                    # Keep the baseline for subsequent deltas, but uncertainty
                    # in another assignment does not describe this selection.
                    cached["fresh"] = fresh
                    continue
                if previous is None or fresh < previous:
                    # Resuming after compaction can reset cumulative counters.
                    # The latest turn is new use; an inherited baseline is not.
                    last = (payload.get("info") or {}).get("last_token_usage") or {}
                    if all(key in last for key in ("input_tokens", "cached_input_tokens", "output_tokens")):
                        delta = int(last["input_tokens"]) - int(last["cached_input_tokens"]) + int(last["output_tokens"])
                        if delta < 0:
                            continue
                        record(delta, timestamp)
                        cached["partial"] |= fresh != delta
                    else:
                        # Only a cumulative observation is available. Keep it in
                        # the partial total, without inventing an instant burst.
                        record(fresh)
                        cached["partial"] = True
                else:
                    delta = fresh - previous
                    record(delta, timestamp)
                cached["fresh"] = fresh
            except (ValueError, TypeError, KeyError):
                continue
    cached["points"] = [(t, n) for t, n in cached["points"] if t >= time.time() - 86400]
    cached["worker_points"] = [(t, n, role) for t, n, role in cached["worker_points"]
                               if t >= time.time() - 86400]
    return cached


def fuel_state(workspace: Path, sessions_root: Path) -> dict[str, Any]:
    config = json.loads((workspace / ".de67/state/workspace.json").read_text())["clock"]
    clock_path = Path(config["state"]).expanduser()
    if not clock_path.is_absolute():
        clock_path = workspace / clock_path
    connection = sqlite3.connect(f"file:{quote(str(clock_path), safe='/:')}?mode=ro", uri=True, timeout=0)
    try:
        attempts = connection.execute(
            "SELECT role,run_id FROM supervisor_attempts WHERE lineage_id=?",
            (config["lineage"],)).fetchall()
        claims = []
        claim_columns = _table_columns(connection, "worker_claims")
        claim_windows_unavailable = bool(claim_columns) and not {
            "lineage_id", "worker_id", "coordinator_session_id", "claimed_at", "released_at"
        }.issubset(claim_columns)
        if claim_columns and not claim_windows_unavailable:
            claims = connection.execute(
                "SELECT worker_id,coordinator_session_id,claimed_at,released_at "
                "FROM worker_claims WHERE lineage_id=?",
                (config["lineage"],),
            ).fetchall()
    finally:
        connection.close()
    roots = {}
    missing = int(claim_windows_unavailable)
    for role, run_id in attempts:
        try:
            session = (workspace / ".de67/state/coordinator-runs" / run_id / "session_id.txt").read_text().strip()
            if session:
                roots[session] = "astra" if role == "mutation-reviewer" else "coordinator"
            else:
                missing += 1
        except OSError:
            missing += 1
    # Retain every assignment in this campaign, including released ones.
    # Reused conversations and their helpers inherit the union of its windows.
    worker_windows: dict[str, list[tuple[float, float | None]]] = {}
    for worker, owner, start, end in claims:
        if roots.get(owner) == "coordinator":
            roots.setdefault(worker, "workers")
            worker_windows.setdefault(worker, []).append((float(start), None if end is None else float(end)))
    index = sessions_root.parent / "state_5.sqlite"
    connection = sqlite3.connect(f"file:{quote(str(index), safe='/:')}?mode=ro", uri=True, timeout=0)
    sessions = {}
    session_windows: dict[str, list[tuple[float, float | None]]] = {}
    try:
        for root, role in roots.items():
            rows = connection.execute(
                "WITH RECURSIVE tree(id) AS (SELECT ? UNION SELECT child_thread_id "
                "FROM thread_spawn_edges JOIN tree ON parent_thread_id=tree.id) "
                "SELECT threads.id,rollout_path FROM threads JOIN tree ON threads.id=tree.id",
                (root,)).fetchall()
            if not rows:
                missing += 1
            for session, path in rows:
                sessions[session] = (roots.get(session, "astra" if role == "astra" else "workers"), Path(path))
                if root in worker_windows:
                    session_windows.setdefault(session, []).extend(worker_windows[root])
    finally:
        connection.close()
    totals = {"astra": 0, "coordinator": 0, "sol": 0, "luna": 0, "other": 0}
    known = 0
    now = time.time()
    bins = [0] * 24  # One-hour display bins over the last twenty-four hours.
    series = {role: [0] * len(bins) for role in totals}
    for session, (role, path) in sessions.items():
        try:
            usage = _trace_fuel(path, windows=session_windows.get(session) if role == "workers" else None)
            if usage["fresh"] is None:
                missing += 1
                continue
            known += 1
            if role == "workers":
                for worker_role, total in usage["worker_totals"].items():
                    totals[worker_role] += total
                points = usage["worker_points"]
            else:
                totals[role] += usage["observed"]
                points = ((stamp, delta, role) for stamp, delta in usage["points"])
            missing += bool(usage["partial"])
            for stamp, delta, point_role in points:
                bucket = int((stamp - (now - 86400)) / 3600)
                if 0 <= bucket < len(bins):
                    bins[bucket] += delta
                    series[point_role][bucket] += delta
        except OSError:
            missing += 1
    return {"available": bool(known), "totals": totals, "bins": bins, "series": series,
            "partial": bool(missing), "sessions": known, "lineage": config["lineage"]}


def render_fuel(fuel: dict[str, Any]) -> str:
    def compact(value: int) -> str:
        return f"{value / 1000000:.2f}m" if value >= 1000000 else f"{value / 1000:.1f}k" if value >= 1000 else str(value)
    if not fuel.get("available"):
        return '<aside class="fuel"><small>fresh tokens</small><strong>—</strong><span>usage unavailable</span></aside>'
    totals = fuel["totals"]
    total = sum(totals.values())
    title = ("Indexed campaign sessions and descendants; input − cached input + output. "
             "Includes completed workers and Astra reviews; excludes narrator and unrelated sessions. "
             + ("Some session accounting is unavailable; shown total is partial." if fuel["partial"] else "")
             + f" Exact observed total: {total:,}.")
    bins = fuel["bins"]
    peak = max(bins) or 1
    # Round the scale upward to readable steps; keep zero honest during idle periods.
    magnitude = 10 ** math.floor(math.log10(peak))
    ceiling = next(step * magnitude for step in (1, 2, 2.5, 5, 10) if step * magnitude >= peak)
    def axis_label(value: float) -> str:
        return f"{value / 1000000:g}m" if value >= 1000000 else f"{value / 1000:g}k" if value >= 1000 else f"{value:g}"
    roles = [("astra", "astra", "#fff0d6"), ("coordinator", "coordinator", "#eabd69"),
             ("sol", "worker sol", "#f2bd63"), ("luna", "worker luna", "#82dfbd")]
    if totals.get("other", 0):
        roles.append(("other", "other workers", "#9997a0"))
    cumulative = [0] * len(bins)
    layers = []
    def coordinates(values: list[int]) -> list[str]:
        return [f"{i * 140 / max(1, len(values)-1):.1f},{115 - value / ceiling * 108:.1f}"
                for i, value in enumerate(values)]
    for role, label, color in roles:
        baseline = coordinates(cumulative)
        cumulative = [a + b for a, b in zip(cumulative, fuel["series"][role])]
        upper = coordinates(cumulative)
        layers.append(f'<g class="fuel-series" data-role="{role}" style="color:{color}">'
                      f'<title>{label}</title><polygon points="{" ".join(upper + baseline[::-1])}" '
                      f'fill="currentColor" fill-opacity=".72"/></g>')
    ticks = "".join(
        f'<span style="top:{position}%">{axis_label(value)}</span>'
        for value, position in ((ceiling, 0), (ceiling / 2, 50), (0, 100))
    )
    positive_totals = [totals[role] for role, _, _ in roles if totals[role] > 0]
    smallest, largest = (min(positive_totals), max(positive_totals)) if positive_totals else (1, 10)
    nice_ticks = [step * 10 ** exponent
                  for exponent in range(math.floor(math.log10(smallest)) - 1,
                                        math.ceil(math.log10(largest)) + 2)
                  for step in (1, 2, 5)]
    lower = max(value for value in nice_ticks if value <= smallest)
    upper = min(value for value in nice_ticks if value >= largest)
    if lower == upper:
        lower = max(value for value in nice_ticks if value < smallest)
        upper = min(value for value in nice_ticks if value > largest)
    log_minimum, log_span = math.log10(lower), math.log10(upper / lower)
    def total_position(value: float) -> float:
        return 100 * (math.log10(value) - log_minimum) / log_span
    interior = [value for value in nice_ticks if lower < value < upper]
    middle = min(interior, key=lambda value: abs(total_position(value) - 50)) if interior else None
    total_ticks = (lower, middle, upper) if middle is not None else (lower, upper)
    bar_axis = '<div class="fuel-bar-axis" aria-label="Role total logarithmic axis">' + "".join(
        f'<span style="left:{total_position(value):.2f}%">{axis_label(value)}</span>'
        for value in total_ticks) + '</div>' if positive_totals else ''
    rocket = ('<svg viewBox="0 0 24 16" aria-hidden="true">'
              '<path d="M7 5L7 1L13 4M7 11L7 15L13 12" fill="currentColor"/>'
              '<path d="M6 5Q14 2 22 8Q14 14 6 11Z" fill="currentColor"/>'
              '<circle cx="15" cy="8" r="2" fill="#101318"/>'
              '<path d="M5 6L0 8L5 10Z" fill="currentColor" opacity=".55"/></svg>')
    legend = '<div class="fuel-legend">' + "".join(
        f'<span><i style="color:{color}" aria-hidden="true"></i>{label.removeprefix("worker ")}</span>'
        for role, label, color in roles) + '</div>'
    rows = "".join(
        f'<span title="{_escape(label)}: {totals[role]:,} fresh tokens" aria-label="{_escape(label)}: {totals[role]:,} fresh tokens">'
        + (f'<em style="left:{total_position(totals[role]):.2f}%;color:{color}">{rocket}</em>' if totals[role] > 0 else '')
        + f'<b>{compact(totals[role])}</b></span>'
        for role, label, color in roles)
    return (f'<aside class="fuel" title="{_escape(title)}">'
            f'<div class="fuel-spark" role="img" aria-label="Stacked fresh-token use over the last twenty-four hours; upper edge is the total. Linear right axis: 0 to {axis_label(ceiling)} tokens per hour.">'
            f'<svg viewBox="0 0 140 122" preserveAspectRatio="none" aria-hidden="true">{"".join(layers)}</svg><div class="fuel-spark-axis">{ticks}</div></div>'
            f'<span class="fuel-period">tokens / hour · last 24h</span>{legend}<div class="fuel-bars" title="Dot positions use a logarithmic axis spanning the positive role totals. Zero totals have no dot. Tooltips show exact totals."><small>role totals · log scale</small>{rows}{bar_axis}</div>'
            f'<strong class="fuel-total"><span>total</span>{compact(total)}{"<sup>~</sup>" if fuel["partial"] else ""}</strong>'
            f'<span class="fuel-scope">campaign{" · partial" if fuel["partial"] else ""}</span></aside>')



def read_subscription_limits(codex: str, timeout: float = 15) -> dict[str, Any]:
    """Read account quota over a private stdio connection; never start a model turn."""
    import queue

    flags = {"creationflags": subprocess.CREATE_NO_WINDOW} if os.name == "nt" else {}
    process = subprocess.Popen(
        [codex, "app-server", "--listen", "stdio://"], cwd=Path.home(),
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
        text=True, encoding="utf-8", **flags,
    )
    replies: queue.Queue[Any] = queue.Queue()

    def receive() -> None:
        try:
            for line in process.stdout:
                try:
                    replies.put(json.loads(line))
                except ValueError:
                    continue
        finally:
            replies.put(None)

    reader = threading.Thread(target=receive, daemon=True)
    reader.start()
    deadline = time.monotonic() + timeout

    def send(message: dict[str, Any]) -> None:
        process.stdin.write(json.dumps(message) + "\n")
        process.stdin.flush()

    def response(identifier: int) -> dict[str, Any]:
        while True:
            left = deadline - time.monotonic()
            if left <= 0:
                raise TimeoutError("Subscription read timed out")
            try:
                message = replies.get(timeout=left)
            except queue.Empty:
                raise TimeoutError("Subscription read timed out") from None
            if message is None:
                raise RuntimeError("Subscription connection closed")
            if isinstance(message, dict) and message.get("id") == identifier:
                if "error" in message:
                    raise RuntimeError("Subscription read unavailable")
                return message["result"]

    try:
        send({"id": 1, "method": "initialize", "params": {
            "clientInfo": {"name": "de67_dashboard", "version": "1"}}})
        response(1)
        send({"method": "initialized"})
        send({"id": 2, "method": "account/rateLimits/read"})
        return response(2)
    finally:
        if process.poll() is None:
            process.terminate()
        try:
            process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
        reader.join(timeout=1)
        process.stdin.close()
        process.stdout.close()


def weekly_subscription(result: dict[str, Any], now: float) -> dict[str, Any]:
    """Select the Codex weekly bucket, not a model-specific or five-hour limit."""
    buckets = result.get("rateLimitsByLimitId")
    bucket = buckets.get("codex") if isinstance(buckets, dict) else result.get("rateLimits")
    if not isinstance(bucket, dict):
        return {"available": False}
    for name in ("primary", "secondary"):
        window = bucket.get(name)
        if not isinstance(window, dict) or window.get("windowDurationMins") != 10080:
            continue
        used = window.get("usedPercent")
        if isinstance(used, bool) or not isinstance(used, (int, float)) or not math.isfinite(used):
            continue
        used = min(100.0, max(0.0, used))
        reset = window.get("resetsAt")
        if isinstance(reset, bool) or not isinstance(reset, (int, float)) or not math.isfinite(reset):
            reset = None
        seconds = 10080 * 60
        elapsed = now - (reset - seconds) if reset is not None else None
        pace = used / (100 * elapsed / seconds) if elapsed is not None and 0 < elapsed < seconds else None
        return {"available": True, "used": used, "remaining": 100 - used,
                "reset": reset, "pace": pace, "ngmi": pace is not None and pace > 1,
                "observed": now}
    return {"available": False}


class SubscriptionUsage:
    """Optional, cached account read. Network work never holds the dashboard lock."""
    def __init__(self, codex: str, refresh_seconds: float = 60) -> None:
        self.codex = codex
        self.refresh_seconds = refresh_seconds
        self._lock = threading.Lock()
        self._state: dict[str, Any] = {"available": False, "loading": True}
        self._next = 0.0
        self._running = False

    def _refresh(self) -> None:
        try:
            state = weekly_subscription(read_subscription_limits(self.codex), time.time())
            if not state.get("available"):
                raise ValueError("Weekly allowance unavailable")
        except Exception:
            with self._lock:
                state = dict(self._state, stale=True, loading=False)
        with self._lock:
            self._state = state
            self._running = False
            self._next = time.monotonic() + self.refresh_seconds

    def snapshot(self) -> dict[str, Any]:
        with self._lock:
            if not self._running and time.monotonic() >= self._next:
                self._running = True
                threading.Thread(target=self._refresh, daemon=True).start()
            state = dict(self._state)
        if state.get("reset") is not None and state["reset"] <= time.time():
            state["stale"] = True
        return state


# Trusted widget CSS travels with the fragment so already-open tabs receive visual updates.
SUBSCRIPTION_STYLE = """<style>.galaxy{height:260px;pointer-events:none}
.galaxy svg{position:absolute;top:0;left:0;height:500px;pointer-events:auto}
.subscription{position:absolute;top:125px;left:28px;width:250px;z-index:2;display:grid;gap:7px;color:#bcb2c9}
.subscription>small{font-size:10px;letter-spacing:.08em}
.subscription-reading{display:flex;align-items:baseline;justify-content:space-between;gap:12px}
.subscription-reading strong{font-size:27px;color:#e8e0ed;font-weight:400;white-space:nowrap}
.subscription-reading strong span{font-size:inherit;color:inherit;letter-spacing:0}
.subscription-status{font-size:27px;color:#8ad5b3;font-weight:400;text-transform:none}
.subscription-tank{height:12px;background:#382c40;border-radius:6px;overflow:hidden;mask-image:repeating-linear-gradient(to right,#000 0,#000 calc(8.333333% - 3px),transparent calc(8.333333% - 3px),transparent 8.333333%)}
.subscription-tank>i{display:block;height:100%;background:#f7ac66;border-radius:0}
.subscription{padding:14px 0;width:250px;left:0;top:72px}
.work-clock{padding-left:0;padding-right:0}
.work-clock,.mutation-total{border:0;background:transparent;border-radius:0}

.subscription>span{font-size:10px;line-height:1.6;color:#b2a6be}
.subscription.ngmi .subscription-status{color:#ff787f}

.subscription.stale .subscription-status{color:#c7aa79}
@media(max-width:650px){
.galaxy{height:145px}.galaxy svg{height:320px}
.subscription{position:relative;top:auto;left:0;width:min(100%,300px);margin:0 0 28px}
}
</style>"""


def render_subscription(state: dict[str, Any] | None) -> str:
    if state is None:
        return ""
    if not state.get("available"):
        status = "checking allowance…" if state.get("loading") else "usage unavailable"
        return SUBSCRIPTION_STYLE + f'<aside class="subscription"><small>fuel</small><span>{status}</span></aside>'
    remaining = state["remaining"]
    stale = state.get("stale", False)
    tone = " stale" if stale else " ngmi" if state.get("ngmi") else ""
    reset = state.get("reset")
    reset_text = (time.strftime("%d %b", time.localtime(reset))
                  if reset is not None else "unavailable")
    pace = state.get("pace")
    status = "stale reading" if stale else "ngmi" if state.get("ngmi") else "" if pace is not None else "pace unknown"
    pace_text = f'{pace:.2f}x pace' if pace is not None and not stale else "pace unknown"
    title = ("Account-wide subscription allowance, including work outside this campaign. "
             "Pace compares percentage used with percentage of the week elapsed. "
             "ngmi means continuing that average would exhaust the allowance before reset; it is an estimate.")
    return (SUBSCRIPTION_STYLE + f'<aside class="subscription{tone}" title="{_escape(title)}">'
            f'<div class="subscription-reading"><strong><span>fuel</span> {remaining:g}%</strong>'
            f'<b class="subscription-status">{status}</b></div>'
            f'<div class="subscription-tank" role="meter" aria-label="Weekly allowance remaining" '
            f'aria-valuemin="0" aria-valuemax="100" aria-valuenow="{remaining:g}">'
            f'<i style="width:{remaining:g}%"></i></div>'
            + f'<span>{pace_text}, reset {reset_text}</span></aside>')


class Dashboard:
    def __init__(self, workspace: Path, refresh_seconds: int = 30,
                 sessions_root: Path | None = None,
                 sidecar_script: Path | None = None,
                 fratbro_script: Path | None = None,
                 fratbro_cache: Path | None = None,
                 fratbro_codex: str = "codex",
                 mutator_activity_db: Path | None = None,
                 subscription_codex: str | None = None) -> None:
        self.workspace = workspace
        self.refresh_seconds = refresh_seconds
        self.sessions_root = sessions_root or Path.home() / ".codex/sessions"
        self.sidecar_script = sidecar_script
        self.fratbro_script = fratbro_script
        self.fratbro_cache = fratbro_cache
        self.fratbro_codex = fratbro_codex
        self.subscription = SubscriptionUsage(subscription_codex) if subscription_codex else None
        self.mutator_activity_db = mutator_activity_db
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
            value = {
                "data": (read_sidecar(self.sidecar_script, self.workspace, state, claim)
                         if claim else {"gaps": [], "subtasks": []}),
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
                process["activity"] = coordinator_activity(self.workspace, self.sessions_root)
            except Exception:
                process["activity"] = "unknown"
            try:
                workers = worker_state(self.workspace, self.sessions_root)
            except Exception as error:
                workers = {"counts": {}, "available": False, "error": str(error)}
            try:
                fuel = fuel_state(self.workspace, self.sessions_root)
            except Exception as error:
                fuel = {"available": False, "error": str(error)}
            fratbro = self._fratbro_source(ledger, clock)
            try:
                mutator_activity = native_mutator_state(self.workspace, self.refresh_seconds)
                if mutator_activity is None and self.mutator_activity_db is not None:
                    mutator_activity = openclaw_mutator_state(self.mutator_activity_db, self.refresh_seconds)
                if mutator_activity is None:
                    mutator_activity = {"glowing": False, "status": "disabled"}
            except Exception:
                mutator_activity = {"glowing": False, "status": "unavailable"}
            return {"dfs": dfs, "ledger": ledger, "clock": clock, "sidecar": sidecar,
                    "fratbro": fratbro, "fuel": fuel, "mutator_activity": mutator_activity,
                    "subscription": self.subscription.snapshot() if self.subscription else None,
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
        nav = '<nav><a class="%s" href="/">Overview</a><a class="%s" href="/dfs">FS</a><a data-refresh href="?refresh=1">Refresh snapshot ↻</a></nav>' % (
            "selected" if tab == "overview" else "", "selected" if tab == "dfs" else "")
        meta = ""  # Explicit refresh keeps reading and navigation stable.
        refresh_label = f'Live · every {self.refresh_seconds}s' if self.refresh_seconds else 'Manual refresh'
        source_bits = ['<span>Snapshot ' + time.strftime("%H:%M:%S") + '</span>']
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
            worker_counts = workers.get("counts", {})
            worker_body = (
                '<div class="roster-scales">' + render_worker_scale(worker_counts) + '</div>'
                if workers.get("available") else
                f'<p class="subtle">Workers unavailable · {_escape(workers.get("error", "unknown source"))}</p>'
            )
            sun_state = ("off" if process_role == "mutation-reviewer" else
                         "on" if coordinator == "running" else
                         "waiting" if coordinator == "waiting" else
                         "unknown" if coordinator == "unknown" else "off")
            astra_counts = worker_counts.get("astra", {})
            astra_total = sum(astra_counts.values())
            mutator_activity = state.get("mutator_activity") or {}
            if mutation_running:
                astra_state, astra_label = "on", "Astra mutator: reviewing"
            elif mutator_activity.get("glowing"):
                astra_state = "on"
                astra_label = "Astra mutator: " + str(mutator_activity.get("status", "working"))
            elif astra_total:
                astra_state, astra_label = "on", f"Astra: {astra_total} active"
            elif workers.get("available") or mutator_activity.get("status") in {"idle", "disabled"}:
                astra_state, astra_label = "off", "Astra mutator: idle"
            else:
                astra_state, astra_label = "unknown", "Astra: activity unavailable"
            sun_activity = process.get("activity", "unknown") if sun_state == "on" else sun_state
            import random
            rng = random.Random(67)
            stars = []
            # A stable, irregular field: clustered arms, a broken dust lane, and
            # sparse foreground stars. Refreshing state does not reshuffle the sky.
            for i in range(3600):
                x = rng.uniform(0, 1100)
                center = 440 - .28 * x + 15 * math.sin(x / 180) + 6 * math.sin(x / 65)
                width = 19 + 22 * math.exp(-((x - 400) / 230) ** 2) + 7 * math.sin(x / 83) ** 2
                if i < 2750:
                    arm = -16 if rng.random() < .58 else 19
                    y = center + arm + rng.gauss(0, width)
                    rift = center + 5 * math.sin(x / 51)
                    if abs(y - rift) < 6 + 4 * math.sin(x / 67) ** 2 and rng.random() < .86:
                        continue
                    radius = rng.uniform(.25, .70)
                    opacity = rng.uniform(.22, .70)
                else:
                    y = rng.uniform(8, 492)
                    radius = rng.uniform(.35, 1.05)
                    opacity = rng.uniform(.16, .68)
                if not 5 < y < 495:
                    continue
                if i % 131 == 0:
                    radius, opacity = 1.25, .95
                # Fade the band into the sparse outer field, including at the
                # SVG edges, rather than ending the foreground stars in a strip.
                distance = abs(y - center)
                envelope = .12 + .88 * math.exp(-(distance / (width * 1.8)) ** 2)
                edge = min(1.0, y / 45, (500 - y) / 70, x / 45, (1100 - x) / 45)
                edge = max(0.0, edge)
                opacity *= envelope * edge * edge * (3 - 2 * edge)
                stars.append(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="{radius:.2f}" opacity="{opacity:.2f}"/>')
            mutations = ((clock_data.get("mutations", 0) + clock_data.get("random_mutations", 0))
                         if clock_data else "—")
            due = ("due now" if random_remaining == 0 else
                   f"due in {random_remaining} results" if random_remaining is not None else "")
            cosmos_html = (
                '<section class="cosmos" aria-label="Live campaign">'
                f'<svg class="outer-stars {astra_state}" viewBox="0 0 1100 600" preserveAspectRatio="none" aria-hidden="true">'
                + "".join(
                    f'<circle cx="{rng.uniform(8, 1092):.2f}" cy="{rng.uniform(210, 588):.2f}" r="{rng.uniform(.3, .85):.2f}" opacity="{rng.uniform(.16, .48):.2f}"/>'
                    for _ in range(125)
                ) + '</svg>'
                f'<div class="cosmos-meta"><div class="work-clock">'
                f'<strong>work: {_escape(work_value)}</strong><span>deadline: {remaining}</span></div>'
                f'<div class="mutation-total"><small>mutations</small><strong>{_escape(mutations)}</strong>'
                f'<span title="{_escape(random_note)}">{_escape(due)}</span></div></div>'
                f'<div class="galaxy {astra_state}" title="{_escape(astra_label)}" role="img" aria-label="{_escape(astra_label)}">'
                '<svg viewBox="0 0 1100 500" preserveAspectRatio="none" aria-hidden="true"><defs><filter id="dust"><feGaussianBlur stdDeviation="10"/></filter></defs>'
                '<g fill="none" stroke="currentColor" filter="url(#dust)">'
                '<path d="M-20 425Q180 414 380 325T740 235T1120 110" stroke-width="20" opacity=".055"/>'
                '<path d="M-20 462Q180 454 380 365T740 275T1120 150" stroke-width="15" opacity=".045"/>'
                '</g>'
                '<g fill="currentColor">' + "".join(stars) + '</g></svg></div>'
                f'{render_subscription(state.get("subscription"))}'
                '<div class="cosmos-deck">'
                f'<div class="sun {sun_state} activity-{_escape(sun_activity)}" title="Coordinator: {_escape(sun_activity)}" role="img" aria-label="Coordinator: {_escape(sun_activity)}">'
                '<span>coordinator</span><svg viewBox="0 0 320 320" aria-hidden="true">'
                '<defs><radialGradient id="sun-glow"><stop offset=".55" stop-color="currentColor" stop-opacity=".65"/><stop offset="1" stop-color="currentColor" stop-opacity="0"/></radialGradient>'
                '<radialGradient id="sun-face" cx="38%" cy="32%" r="75%"><stop stop-color="currentColor"/><stop offset=".72" stop-color="currentColor" stop-opacity=".85"/><stop offset="1" stop-color="currentColor" stop-opacity=".5"/></radialGradient>'
                '<filter id="solar-grain"><feTurbulence type="fractalNoise" baseFrequency=".18" numOctaves="3" seed="67"/><feColorMatrix type="matrix" values="0 0 0 0 .38 0 0 0 0 .18 0 0 0 0 .025 0 0 0 2 -.65"/><feComposite in2="SourceGraphic" operator="in"/></filter>'
                '<clipPath id="sun-disc"><circle cx="160" cy="160" r="121"/></clipPath></defs>'
                '<circle class="sun-aura" cx="160" cy="160" r="174" fill="url(#sun-glow)"/>'
                '<circle class="sun-rim" cx="160" cy="160" r="126" fill="none" stroke="currentColor" stroke-width="2"/>'
                '<g class="sun-corona" fill="none" stroke="currentColor" stroke-linecap="round">'
                '<circle cx="160" cy="160" r="131" stroke-width="12" opacity=".14"/>'
                + solar_filaments()
                + ''.join(f'<path d="M149 42 C130 24 143 {tip} 163 {tip - 3} C149 13 178 20 172 42" transform="rotate({angle} 160 160)" stroke-width="{width}" opacity="{opacity}"/>'
                          for angle, tip, width, opacity in ((0, 0, 2.5, .8), (43, 13, 2, .55), (88, -5, 3, .85), (130, 10, 2, .6), (180, 2, 2.5, .75), (229, 15, 2, .55), (273, -2, 3, .9), (319, 9, 2, .65))) + '</g>'
                '<circle cx="160" cy="160" r="122" fill="url(#sun-face)"/>'
                '<g class="sun-surface" clip-path="url(#sun-disc)"><circle cx="160" cy="160" r="122" filter="url(#solar-grain)"/>'
                '<g fill="none" stroke="#fff0c5" opacity=".75"><path d="M74 197C43 168 122 154 88 207M74 197C50 167 105 168 88 207M207 96C188 56 247 87 220 116M207 96C195 74 235 90 220 116" stroke-width="1.5"/></g></g>'
                '</svg></div>'
                f'<div class="cosmos-workers">{worker_body}</div>{render_fuel(state.get("fuel", {}))}</div></section>'
            )
            active_html = render_work_digest(ledger_data["active"])
            upcoming_html = render_work_digest(upcoming)
            waiting_html = (
                '<section><h2>Waiting on event</h2><div class="ledger-list">'
                f'{render_work_digest(ledger_data["waiting"])}</div></section>'
                if ledger_data["waiting"] else ""
            )
            blocked_html = render_work_digest(ledger_data["blocked"])
            sidecar_html = render_trajectory(
                sidecar.get("data") or {}, fratbro if self.fratbro_cache else None,
                context={"claim": active_claim, "deadline generation": deadline.get("generation"),
                         "restart": restart.get("generation")},
                stale=bool(sidecar.get("stale")),
                error=sidecar.get("error") if self.sidecar_script else None,
            ) if self.sidecar_script or self.fratbro_cache else ""
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
            body = f'{cosmos_html}{sidecar_html}{finding_html}<section class="work-section"><div class="eyebrow">THE WORK / CURRENT SCOPE</div><h2>Work in focus</h2><div class="subtle">{details}</div><div class="ledger-list">{active_html}</div></section><section class="work-section"><div class="eyebrow">ON THE HORIZON</div><h2>Up next</h2><div class="ledger-list">{upcoming_html}</div></section>{waiting_html}<section class="work-section"><div class="eyebrow">NEEDS ATTENTION</div><h2>Blocked work</h2><div class="ledger-list">{blocked_html}</div></section>'
        # Stable region IDs let the browser update optional panels in place.
        for css, key in (("cosmos", "campaign"), ("trajectory", "trajectory"),
                         ("fratbro", "briefing"), ("activity", "finding"),
                         ("document", "document")):
            body = body.replace(f'class="{css}"', f'id="panel-{key}" data-panel class="{css}"', 1)
        for heading, key in (("THE WORK / CURRENT SCOPE", "focus"),
                             ("ON THE HORIZON", "upcoming"), ("NEEDS ATTENTION", "blocked")):
            body = body.replace(f'<section class="work-section"><div class="eyebrow">{heading}',
                                f'<section id="panel-{key}" data-panel class="work-section"><div class="eyebrow">{heading}')
        body = body.replace('<section><h2>Waiting on event', '<section id="panel-waiting" data-panel><h2>Waiting on event')
        page = f'''<!doctype html><html lang="en"><head><meta charset="utf-8">{meta}
<meta name="viewport" content="width=device-width,initial-scale=1"><title>de67</title>
<style>
html{{overflow-anchor:none}}
#refresh-status{{font-size:11px;min-height:16px}}
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
.fratbro{{border-left:2px solid var(--blue);padding:6px 0 6px 25px;margin:38px 0 28px;max-width:none;width:100%}}
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
.worker-scale:nth-child(2) .worker-dot{{fill:#a8baff}}
.strength-label{{fill:#aebdc6;font:11px -apple-system,sans-serif;text-anchor:middle}}
.strength-overflow{{fill:var(--text);font:10px -apple-system,sans-serif;text-anchor:middle}}
@media(max-width:650px){{.roster-scales{{grid-template-columns:1fr;gap:20px}}.worker-scale svg{{max-height:none}}}}


:root{{--terminal:"Cascadia Code","SFMono-Regular",Consolas,"Liberation Mono",monospace;--bg:#101117;--muted:#989ba9}}
body{{background:radial-gradient(ellipse at 30% 0%,#30213b30,transparent 48%),var(--bg)}}
body,body *{{font-family:var(--terminal)!important}}
header h1{{font-size:48px;letter-spacing:-3px;color:#626370}}
header h1.supervisor-running{{color:#cf9bdc;text-shadow:0 0 30px #bc80cf25}}
header>span{{font-size:10px;max-width:60%;overflow-wrap:anywhere;text-align:right}}
nav{{margin:16px 0 24px;border-color:#30303b}}nav a{{font-size:11px}}
.cosmos{{margin:0;padding:0}}
.cosmos-meta{{display:flex;justify-content:space-between;gap:28px;align-items:flex-start;position:relative;z-index:1}}
.work-clock{{border:1px solid #76598066;border-radius:5px;padding:14px 18px;max-width:72%;display:grid;gap:9px}}
.cosmos small{{font-size:10px;letter-spacing:.08em;margin:0;color:#9893a5}}
.work-clock strong{{font-size:13px;color:#d7bfdf;overflow-wrap:anywhere;font-weight:400}}
.work-clock>span{{font-size:22px;color:#dedbe6;letter-spacing:.06em}}
.mutation-total{{text-align:right;padding-top:5px;display:grid;gap:8px;flex-shrink:0}}
.mutation-total strong{{font-size:30px;font-weight:400;color:#d2c7da}}
.mutation-total>span{{font-size:10px;color:var(--muted)}}
.galaxy{{color:#686976;position:relative;margin:-55px -12px -30px;opacity:.40;pointer-events:auto}}
.galaxy.on{{color:#fff0d6;opacity:1}}.galaxy.unknown{{opacity:.22}}
.galaxy svg{{display:block;width:100%;height:180px}}.galaxy>span{{position:absolute;right:5%;top:32%;font-size:10px;letter-spacing:.2em}}
.cosmos-deck{{display:grid;grid-template-columns:minmax(0,.21fr) minmax(0,.49fr) minmax(0,.30fr);gap:32px;align-items:center}}
.sun{{color:#555761;padding:0 0 28px;text-align:center}}.sun.on{{color:#e9bc70}}.sun.waiting{{color:#a78e66}}.sun.unknown{{color:#41434c}}
.sun>span{{font-size:12px;letter-spacing:.1em}}.sun svg{{display:block;width:100%;height:auto;margin-top:16px}}
.cosmos .roster-scales{{grid-template-columns:1fr;gap:4px}}
.cosmos .worker-scale{{width:min(100%,469px);justify-self:center;display:grid;grid-template-columns:minmax(0,1fr) 65px;column-gap:8px;align-items:center}}
.cosmos .worker-scale>svg:not(.model-emblem){{grid-column:1;grid-row:1;height:170px;justify-self:start;width:auto;max-width:100%}}
.worker-legend{{grid-column:2;grid-row:1;display:grid;gap:4px;text-align:center}}
.worker-legend>div{{display:grid;justify-items:center;gap:3px}}
.worker-legend span{{font-size:11px;color:#c7ccd7}}
.cosmos .worker-legend .model-emblem{{grid-column:auto;grid-row:auto;width:30px;height:30px;margin:0}}
.cosmos .scale-heading{{grid-column:2;grid-row:1;align-self:end;padding:0;display:block}}
.cosmos .scale-heading strong{{font-size:13px;font-weight:400;color:#c7ccd7}}
.cosmos .scale-heading span{{display:none}}
.cosmos .model-emblem{{grid-column:2;grid-row:2;align-self:start;width:30px;height:30px;margin:9px 0 0}}
.cosmos .worker-scale[data-model="sol"] .worker-dot{{fill:#f2bd63}}
.cosmos .worker-scale[data-model="luna"] .worker-dot{{fill:#7ee6c2}}
.cosmos .worker-scale[data-model="astra"] .worker-dot{{fill:#fff0d6;filter:drop-shadow(0 0 4px #ffdc9d)}}
.cosmos .worker-scale[data-model="astra"] .model-emblem{{filter:drop-shadow(0 0 4px #ffdc9d)}}
.cosmos .strength-label{{font-size:10px;fill:#9597a5}}.cosmos .strength-axis{{stroke:#42434e}}
@media(max-width:650px){{
main{{padding:24px 18px}}header h1{{font-size:42px}}.cosmos-meta{{gap:12px}}.work-clock{{max-width:68%;padding:12px}}.work-clock strong{{font-size:11px}}.work-clock>span{{font-size:19px}}.mutation-total>span{{max-width:86px;line-height:1.5}}
.cosmos-deck{{grid-template-columns:1fr;gap:12px}}.sun{{width:180px;padding:0;margin:0 auto 12px}}.sun>span{{font-size:10px}}.sun svg{{margin-top:0}}
.galaxy{{margin:-14px -10px -8px}}.galaxy svg{{height:auto}}.cosmos .worker-scale>svg:not(.model-emblem){{height:auto;width:100%}}.galaxy>span{{font-size:8px;top:40%}}
.cosmos .roster-scales{{gap:20px}}.cosmos .worker-scale{{grid-template-columns:minmax(0,1fr) 48px;gap:4px}}.cosmos .scale-heading strong{{font-size:11px}}
}}


.fuel{{align-self:center;color:#b8accb;min-width:0;padding:0}}
.fuel svg{{display:block;width:100%;height:150px;margin:0 0 5px}}
.fuel-spark{{position:relative;width:calc(100% - 48px);height:150px;margin-bottom:5px}}
.fuel .fuel-spark svg{{width:100%;height:100%;margin:0}}
.fuel-spark-axis{{position:absolute;left:100%;top:calc(100% * 7 / 122);bottom:calc(100% * 7 / 122);border-left:1.5px solid #686976;font-size:10px;color:var(--muted)}}
.fuel-spark-axis span{{position:absolute;left:7px;transform:translateY(-50%);white-space:nowrap}}
.fuel-spark-axis span::before{{content:"";position:absolute;left:-8px;top:50%;width:3px;border-top:1.5px solid var(--muted)}}
@media(max-width:650px){{.fuel-spark{{height:190px}}.fuel-spark-axis{{font-size:12px}}}}
.fuel-bar-axis{{position:relative;height:20px;border-top:1.5px solid #686976;margin-top:1px;color:var(--muted);font-size:10px}}
.fuel-bar-axis span{{position:absolute;top:5px;transform:translateX(-50%)}}
.fuel-bar-axis span::before{{content:"";position:absolute;left:50%;top:-6px;height:3px;border-left:1.5px solid var(--muted)}}
.fuel-bar-axis span:first-child{{transform:none}}.fuel-bar-axis span:last-child{{transform:translateX(-100%)}}
.fuel>span{{display:block;font-size:9px;color:#777480}}
.fuel .fuel-period{{font-size:8px;color:#96909f}}
.fuel .fuel-legend{{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin:15px 0 20px;padding:10px;border:1px solid #39313f;border-radius:6px}}
.fuel-legend>span{{display:flex;align-items:center;gap:6px;font-size:9px;color:#b1a9bb}}
.fuel-legend i{{display:inline-block;width:20px;height:6px;border-radius:2px;background:currentColor;flex-shrink:0}}
.fuel-bars{{display:grid;gap:13px;padding-right:48px}}
.fuel-bars>small{{font-size:8px;color:#96909f;margin:0;white-space:nowrap}}
.fuel-bars>span{{position:relative;display:flex;align-items:center;height:8px}}
.fuel-bars em{{position:absolute;width:24px;height:16px;transform:translateX(-50%)}}
.fuel-bars em svg{{width:24px;height:16px;margin:0;overflow:visible}}
@media(max-width:650px){{.fuel-bar-axis{{font-size:12px}}}}
.fuel-bars b{{position:absolute;left:calc(100% + 7px);width:41px;text-align:right;font-size:10px;font-weight:400;color:#c9bfd4}}
.fuel .fuel-total{{display:flex;align-items:baseline;gap:5px;font-size:17px;font-weight:700;letter-spacing:0;margin:17px 0 4px;padding-top:10px;border-top:1px solid #35303e;color:#dfd4e7}}
.fuel-total>span{{margin-right:auto;font-size:10px;font-weight:700}}
.fuel sup{{font-size:10px;vertical-align:top}}
@media(max-width:900px) and (min-width:651px){{.cosmos-deck{{grid-template-columns:140px minmax(0,1fr) minmax(180px,.85fr);gap:20px}}}}
@media(max-width:650px){{.cosmos-deck{{grid-template-columns:1fr}}.fuel{{width:100%;padding:14px 0 0;display:block}}.fuel svg{{height:190px}}.fuel .fuel-legend{{gap:10px}}.fuel-legend>span{{font-size:11px}}.fuel .fuel-period{{font-size:9px}}}}



.sun>span{{display:inline-block;transform:translateY(7px);font-size:14px}}
.work-clock{{position:relative;z-index:2;background:#14131c}}
.work-clock>strong,.work-clock>span{{font-size:14px;font-weight:400;letter-spacing:.015em;line-height:1.55;overflow-wrap:anywhere}}
@media(max-width:650px){{.sun>span{{font-size:12px}}.work-clock>strong,.work-clock>span{{font-size:12px}}.work-clock{{max-width:74%}}}}


.cosmos{{isolation:isolate}}
.galaxy{{margin:-90px -12px -75px;color:#8f8998;opacity:.62;z-index:0}}
.galaxy.on{{color:#fff0d6;opacity:1}}.galaxy.unknown{{opacity:.30}}
.galaxy,.outer-stars{{transition:color 1.8s ease,opacity 1.8s ease}}
@media(prefers-reduced-motion:reduce){{.galaxy,.outer-stars{{transition:none}}}}
.galaxy svg{{height:260px}}
.cosmos-deck{{position:relative;z-index:1}}
.galaxy>span{{top:40%;right:5%}}
@media(max-width:650px){{.galaxy{{margin:-45px -10px -35px}}.galaxy svg{{height:145px}}.galaxy>span{{top:52%}}}}


.mutation-total{{position:relative;z-index:2;background:#14131c;border:1px solid #76598066;border-radius:5px;padding:14px 18px}}
@media(max-width:650px){{.mutation-total{{padding:12px}}}}


.cosmos{{position:relative}}
.outer-stars{{position:absolute;inset:0;width:100%;height:100%;z-index:0;pointer-events:none;fill:#8f8998;opacity:.62}}
.outer-stars.on{{fill:#fff0d6;opacity:1}}.outer-stars.unknown{{opacity:.30}}


.sun svg{{width:240px;max-width:100%;margin:16px auto 0}}
@media(max-width:650px){{.sun svg{{width:180px;margin-top:8px}}}}


.sun{{align-self:center}}
.sun svg{{width:160px}}
@media(max-width:650px){{.sun svg{{width:120px}}}}


@media(min-width:651px){{.cosmos-deck{{transform:translateY(-1cm)}}.sun{{display:flex;flex-direction:column;align-items:center;width:min(100%,180px);justify-self:end;align-self:center;padding:0}}.sun>span{{width:100%;margin-bottom:16px;transform:none}}.sun svg{{width:min(100%,160px);margin:0}}}}


header h1{{font-family:var(--terminal)!important;font-weight:400;letter-spacing:0;font-style:normal}}


/* Carry the header's violet palette through the reading panels. */
:root{{--panel:#191620;--line:#39313f;--blue:#c49bd4}}
.trajectory{{background:linear-gradient(150deg,#241c2b88,#17141d88);border-color:#483750}}
.attention-panel{{background:#19161f;border-color:#3d3245}}
.attention-grid polygon,.attention-grid line{{stroke:#39313f}}
.trajectory-node rect{{fill:#211c29;stroke:#4b3b57}}
.trajectory-node.active rect{{fill:#30233a;stroke:var(--blue)}}
.trajectory-center rect{{fill:#211927;stroke:var(--blue)}}
.gap-explanation{{background:#1c1722;border-color:#3b3045}}
.gap-explanation.active{{background:#2a2033}}
.work-card{{background:linear-gradient(135deg,#282031,#1b1622);border-color:#493952}}
.work-card.complete{{background:#1a161f;border-color:#352c3e}}
.work-card h3,.document p,.document li{{color:#e1d9e8}}
.brief-lead{{color:#e5d9ec}}
.text-link:hover{{color:#ead1f5}}
.ledger-item{{background:linear-gradient(90deg,#c49bd411,transparent 68%)}}
.ledger-item::before{{background:linear-gradient(180deg,var(--blue),#715681)}}
code,pre{{background:#15111b}}


.sun svg{{overflow:visible}}.sun{{transition:color 1.8s ease}}
.sun .sun-corona{{opacity:0;transform-origin:160px 160px;transform:scale(.78);transition:transform 1.8s ease,opacity 1.8s ease}}
.sun-aura,.sun-rim{{opacity:0;transition:opacity 1.8s ease}}
.sun-surface{{opacity:0;transition:opacity 1.8s ease}}.sun.on .sun-surface,.sun.waiting .sun-surface{{opacity:.18}}
.sun.on.activity-working .sun-surface{{opacity:.7}}
.sun.on,.sun.waiting{{color:#bf985e}}.sun.on .sun-rim,.sun.waiting .sun-rim{{opacity:.4}}
.sun.on .sun-aura,.sun.waiting .sun-aura{{opacity:.18}}
.sun.on.activity-working{{color:#ffd58c}}
.sun.on.activity-working .sun-corona{{opacity:1;transform:scale(1);animation:solar-breath 7s ease-in-out infinite}}
.sun.on.activity-working .sun-aura{{opacity:.85}}.sun.on.activity-working .sun-rim{{opacity:.9}}
@keyframes solar-breath{{0%,100%{{transform:scale(1)}}50%{{transform:scale(1.035)}}}}
@media(prefers-reduced-motion:reduce){{.sun,.sun .sun-corona,.sun-aura,.sun-rim,.sun-surface{{transition:none;animation:none}}}}

.trajectory{{--radar-font:ui-monospace,"SFMono-Regular",Menlo,Consolas,monospace;font-family:var(--radar-font);position:relative;padding:28px 28px 20px;background:none;border:0;border-radius:0;margin-top:18.667px}}
@media(min-width:651px){{.cosmos+.trajectory{{margin-top:calc(18.667px - 0.666667cm)}}}}
.trajectory::before{{content:"";position:absolute;inset:0;pointer-events:none;border:1px solid #80628d;border-radius:8px;mask:linear-gradient(#000 0 0) left top/26px 22px no-repeat,linear-gradient(#000 0 0) right top/26px 22px no-repeat,linear-gradient(#000 0 0) left bottom/26px 22px no-repeat,linear-gradient(#000 0 0) right bottom/26px 22px no-repeat}}
.trajectory .radar-briefing{{display:block;margin:0;padding:0 0 22px;border:0}}
.radar-kicker{{display:flex;justify-content:space-between;gap:12px;flex-wrap:wrap;color:#caabd6;font-size:10px;letter-spacing:.18em;line-height:1.6;padding-top:12px;border-top:1px solid #594262}}
.radar-kicker span{{display:inline;color:#a99bad;letter-spacing:.04em;font-size:10px}}
.trajectory .radar-briefing h2{{font:600 clamp(17px,1.9vw,23px)/1.55 var(--radar-font);letter-spacing:-.025em;max-width:76ch;margin:13px 0 0;overflow-wrap:anywhere}}
.radar-briefing strong{{font:inherit;color:#eee4f1}}
.trajectory .attention-panel{{padding:18px 0 0;border:0;border-top:1px solid #46374c;border-radius:0;background:none;overflow:visible}}
.attention-heading{{gap:12px;flex-wrap:wrap}}.trajectory .attention-heading h3{{font:500 11px/1.5 var(--radar-font);color:#bdaac5;letter-spacing:.08em;text-transform:uppercase}}
.trajectory .attention-heading>span{{font-size:10px;color:#a69aaa}}
.radar-stage{{position:relative;isolation:isolate;display:grid;grid-template-areas:"left map right";grid-template-columns:minmax(0,1fr) minmax(240px,1.85fr) minmax(0,1fr);gap:22px;align-items:center;margin:18px 0}}
.radar-scope{{grid-area:map;min-width:0;position:relative}}
.trajectory .radar-map{{display:block;width:100%;min-width:0;height:auto;overflow:visible}}
.trajectory .radar-links{{position:absolute;inset:0;z-index:-1;display:block;width:100%;height:100%;min-width:0;pointer-events:none;overflow:visible}}
.radar-links path{{fill:none;stroke:#9d77af;stroke-width:1;opacity:.38}}
.radar-disc{{fill:none;stroke:#8b659655;stroke-width:1}}
.trajectory .attention-grid circle,.trajectory .attention-grid line{{fill:none;stroke:#755484;stroke-opacity:.32;stroke-width:1}}
.radar-origin{{fill:none;stroke:#c4a4ce;stroke-width:1;opacity:.65}}
.assigned-course-halo{{fill:none;stroke:#fff1cf;stroke-width:8;opacity:.09}}
.assigned-course{{fill:none;stroke:#fff1cf;stroke-width:2.5;stroke-dasharray:9 7;stroke-linecap:round}}
.assigned-ship{{fill:#fff1cf;stroke:#fffbea;stroke-width:1;filter:drop-shadow(0 0 5px #ffdd9988)}}
.assigned-ship .ship-exhaust{{fill:#f1bb75;stroke:none;opacity:.7}}
.heading-live .assigned-course{{animation:course-flow 2s linear infinite}}
.assigned-destination{{display:flex;justify-content:center;align-items:baseline;flex-wrap:wrap;gap:7px 13px;margin:14px 0 8px;font-size:11px;line-height:1.8;overflow-wrap:anywhere}}
.assigned-destination span{{color:#c1aa86;font-size:10px;text-transform:uppercase;letter-spacing:.12em}}
.assigned-destination a{{color:#fff1cf;text-decoration:none;border-bottom:1px solid #9d82524d;min-width:0}}
.assigned-destination a:hover,.assigned-destination a:focus{{border-color:#fff1cf}}
.trajectory .attention-key.attention-target{{background:#fff1cf;box-shadow:0 0 5px #ffdb9e66}}
@keyframes course-flow{{to{{stroke-dashoffset:-32}}}}
@media(prefers-reduced-motion:reduce){{.heading-live .assigned-course{{animation:none}}}}
.trajectory .trajectory-node circle{{fill:#211928;stroke:#ba9ac8;stroke-width:1.5}}
.trajectory .trajectory-node text{{fill:#ddd0e4;font:500 12px var(--radar-font);text-anchor:middle}}
.trajectory .trajectory-node.active circle{{fill:#50334e;stroke:#f0c6e7;stroke-width:2.5}}
.trajectory .trajectory-node.proved circle{{stroke:#8acbb0}}.trajectory .trajectory-node.open circle{{stroke:#c9ad7f}}
.trajectory .trajectory-node:hover circle,.trajectory .trajectory-node:focus circle{{fill:#65476a;stroke:#fff0ff}}
.radar-contacts{{display:flex;flex-direction:column;gap:14px;min-width:0}}.radar-left{{grid-area:left}}.radar-right{{grid-area:right}}
.radar-contact{{--contact-line:#55425e;position:relative;min-width:0;padding:13px 14px;border:1px solid var(--contact-line);border-radius:0;background:#211b2977;clip-path:polygon(0 0,calc(100% - 12px) 0,100% 12px,100% 100%,0 100%);overflow-wrap:anywhere;scroll-margin-top:20px}}
.radar-contact::after{{content:"";position:absolute;right:0;top:0;width:12px;height:12px;pointer-events:none;background:linear-gradient(45deg,transparent 44%,var(--contact-line) 47%,var(--contact-line) 53%,transparent 56%)}}
.radar-contact.active{{--contact-line:#d9bb89;border-left:3px solid #fff1cf;background:linear-gradient(135deg,#4c3a352e,#251c2c77)}}
.radar-contact.proved{{border-left:2px solid #8acbb0}}.radar-contact.open{{border-left:2px solid #c9ad7f}}
.radar-contact:target{{--contact-line:#dec2ea;box-shadow:inset 3px 0 #dec2ea}}
.radar-contact-heading{{display:flex;align-items:baseline;gap:9px;min-width:0;line-height:1.55}}
.contact-number{{flex:none;display:inline-grid;place-items:center;min-width:22px;height:22px;border:1px solid var(--contact-line);font-size:10px;color:#c6a9d4}}.contact-id{{font-size:11px;color:#dfcbe8;min-width:0}}
.radar-contact p{{margin:9px 0 10px;color:#e0d8e5;font-size:12px;line-height:1.7;white-space:normal}}
.contact-state{{display:block;font-size:10px;line-height:1.6;color:#b9aaba}}.radar-contact.active .contact-state{{color:#efc8df}}
.trajectory .attention-legend{{font-size:10px;line-height:1.8;gap:6px 14px;margin:16px 0 10px;flex-wrap:wrap}}
.trajectory .attention-legend span{{white-space:normal;overflow-wrap:anywhere}}
.trajectory .attention-claim{{flex-wrap:wrap;overflow-wrap:anywhere;line-height:1.7;gap:5px 12px}}
.trajectory .attention-claim strong{{font:500 12px var(--radar-font);color:#d2b5de}}.trajectory .attention-claim span{{font-size:10px;min-width:0}}
.trajectory .attention-panel>p{{font-size:10px;line-height:1.8;margin:10px 0;color:#a899b1}}
.radar-idle{{grid-template-columns:1fr;grid-template-areas:"map";margin:6px 0 12px}}
.radar-idle .radar-scope{{width:min(100%,350px);margin:auto}}.radar-idle .radar-contacts{{display:none}}.radar-idle .radar-map{{opacity:.45}}
.radar-standby{{position:absolute;top:52%;left:0;width:100%;margin:0;text-align:center;font-size:12px;color:#d0b6db;line-height:1.9}}
.radar-standby span{{color:#a997b1;font-size:10px}}
.radar-details{{margin-top:24px;padding-top:7px;border-top:1px solid #46374c}}.radar-details:empty{{display:none}}
.radar-detail{{display:grid;grid-template-columns:110px minmax(0,1fr);gap:20px;padding:15px 0}}
.radar-detail+.radar-detail{{border-top:1px solid #3b2e4233}}
.trajectory .radar-detail h3{{margin:3px 0 0;font:500 10px/1.8 var(--radar-font);color:#baa1c5;letter-spacing:.08em;text-transform:uppercase}}
.radar-details p{{font:400 12px/1.9 var(--radar-font);color:#c9becf;margin:0;overflow-wrap:anywhere;white-space:pre-line}}
.radar-details>p{{margin:14px 0}}.radar-notice{{font-size:11px;line-height:1.7;color:#d9ba8c;overflow-wrap:anywhere}}
.trajectory .gap-explanation{{min-width:0;border-radius:0;overflow-wrap:anywhere}}.trajectory .gap-explanation div{{flex-wrap:wrap}}.trajectory .gap-explanation div span{{min-width:0}}
@media(max-width:800px){{.radar-stage{{grid-template-columns:repeat(2,minmax(0,1fr));grid-template-areas:"map map" "left right";gap:16px}}.radar-scope{{width:min(100%,430px);margin:auto}}.trajectory .radar-links{{display:none}}.radar-idle{{grid-template-areas:"map";grid-template-columns:1fr}}}}
@media(max-width:500px){{.trajectory{{padding:20px 16px}}.radar-stage{{display:flex;flex-direction:column;align-items:stretch}}.radar-scope{{order:0}}.radar-contacts{{display:contents}}.radar-contact{{order:var(--contact-order)}}.radar-detail{{grid-template-columns:minmax(0,1fr);gap:6px}}.radar-contact p{{font-size:12px}}.radar-kicker{{gap:6px}}}}


</style><script src="/live_refresh.js" defer></script></head><body><main data-dashboard data-refresh-seconds="{self.refresh_seconds}"><header id="dashboard-header"><h1 class="supervisor-{_escape(supervisor)}" title="Supervisor: {_escape(supervisor)}" aria-label="de67 · supervisor {_escape(supervisor)}">de67</h1><span>{_escape(self.workspace.name)}</span></header>{nav}<div id="refresh-status" class="subtle" role="status">{refresh_label}</div><div id="dashboard-content">{body}</div><footer id="dashboard-sources">{''.join(source_bits)}</footer></main></body></html>'''
        return page.encode("utf-8")


def serve(workspace: Path, bind: str, port: int, refresh_seconds: int,
          sessions_root: Path | None = None,
          sidecar_script: Path | None = None,
          fratbro_script: Path | None = None,
          fratbro_cache: Path | None = None,
          fratbro_codex: str = "codex",
          mutator_activity_db: Path | None = None,
          subscription_codex: str | None = None) -> None:
    dashboard = Dashboard(workspace.resolve(), refresh_seconds, sessions_root, sidecar_script,
                          fratbro_script, fratbro_cache, fratbro_codex, mutator_activity_db, subscription_codex)

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
            if path not in ("/", "/dfs", "/ledger", "/briefing", "/live_refresh.js"):
                self.send_error(404)
                return
            try:
                is_script = path == "/live_refresh.js"
                payload = (Path(__file__).with_name("live_refresh.js").read_bytes() if is_script
                           else dashboard.render(path.strip("/") or "overview"))
                self.send_response(200)
                self.send_header("Content-Type", "text/javascript; charset=utf-8" if is_script else "text/html; charset=utf-8")
                self.send_header("Content-Length", str(len(payload)))
                self.send_header("Cache-Control", "no-store")
                self.send_header("Content-Security-Policy", "default-src 'none'; script-src 'self'; connect-src 'self'; style-src 'unsafe-inline'; base-uri 'none'; form-action 'none'; frame-ancestors 'none'")
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
    parser.add_argument("--refresh-seconds", type=int, default=30,
                        help="Background refresh interval; 0 disables automatic updates")
    parser.add_argument("--codex-sessions", type=Path, default=None,
                        help="Codex session root used for optional active-worker counts")
    parser.add_argument("--sidecar-script", type=Path, default=None,
                        help="Optional de67 trajectory_sidecar.py path")
    parser.add_argument("--fratbro-script", type=Path, default=None,
                        help="Optional Luna-low fratbro_narrator.py path")
    parser.add_argument("--fratbro-cache", type=Path, default=None,
                        help="Optional narrator cache outside the configured workspace")
    parser.add_argument("--fratbro-codex", default="codex",
                        help="Codex executable used only by the optional narrator")
    parser.add_argument("--mutator-activity-db", type=Path, default=None,
                        help="Optional dedicated OpenClaw mutator agent SQLite store; activity lights the galaxy")
    parser.add_argument("--subscription-codex", default=None,
                        help="Optional Codex executable for account-wide weekly quota; no model calls")
    args = parser.parse_args()
    if args.refresh_seconds < 0:
        parser.error("--refresh-seconds cannot be negative")
    if bool(args.fratbro_script) != bool(args.fratbro_cache):
        parser.error("--fratbro-script and --fratbro-cache must be configured together")
    serve(args.workspace, args.bind, args.port, args.refresh_seconds,
          args.codex_sessions, args.sidecar_script, args.fratbro_script, args.fratbro_cache,
          args.fratbro_codex, args.mutator_activity_db, args.subscription_codex)


if __name__ == "__main__":
    main()
