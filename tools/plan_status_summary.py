#!/usr/bin/env python3

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


SECTION_RE = re.compile( r"^##\s+(?P<title>.+?)\s*$", re.MULTILINE )
STATUS_RE = re.compile( r"^\*\*Status:\*\*\s+(?P<status>.+?)\s*$", re.MULTILINE )
DOC_POINTER_RE = re.compile( r"`(?P<path>(?:doc|tools)/[^`]+)`" )
MARKDOWN_STRIP_RE = re.compile( r"[*_`]" )
WHITESPACE_RE = re.compile( r"\s+" )


@dataclass
class SectionSummary:
    title: str
    status: str
    classification: str
    gist: str
    doc_pointer: str | None
    section_index: int


@dataclass
class ParseResult:
    sections: list[SectionSummary]
    warnings: list[str]


def strip_markdown( text: str ) -> str:
    cleaned = MARKDOWN_STRIP_RE.sub( "", text )
    cleaned = cleaned.replace( "[", "" ).replace( "]", "" )
    return WHITESPACE_RE.sub( " ", cleaned ).strip()


def extract_first_paragraph( body: str ) -> str:
    lines = body.splitlines()
    paragraph: list[str] = []
    for raw_line in lines:
        line = raw_line.strip()
        if not line:
            if paragraph:
                break
            continue
        if line.startswith( "**Status:**" ):
            continue
        if re.match( r"^(?:[-*]|\d+\.)\s+", line ):
            if paragraph:
                break
            continue
        if line.startswith( "---" ) or line.startswith( "## " ):
            break
        paragraph.append( strip_markdown( line ) )
    joined = " ".join( paragraph ).strip()
    if not joined:
        return ""
    first_sentence = re.split( r"(?<=[.!?])\s+", joined, maxsplit=1 )[0].strip()
    if len( first_sentence ) <= 220:
        return first_sentence
    return first_sentence[:217].rstrip() + "..."


def classification_from_status( status: str ) -> str | None:
    upper = status.upper()
    if "ACTIVE / GREENLIT" in upper:
        return "active"
    if "GREENLIT / BACKLOG" in upper or "GREENLIT / BOTTOM-OF-STACK" in upper:
        return "greenlit"
    if "PARKED" in upper:
        return "parked"
    if "CHECKPOINTED" in upper:
        return "checkpointed"
    return None


def classification_from_heading( title: str ) -> str | None:
    lowered = title.lower()
    if "active" in lowered:
        return "active"
    if "greenlit backlog" in lowered or "bottom-of-stack" in lowered:
        return "greenlit"
    if "parked" in lowered:
        return "parked"
    if "checkpointed" in lowered:
        return "checkpointed"
    return None


def extract_doc_pointer( body: str ) -> str | None:
    for line in body.splitlines():
        if "Canonical contract lives at" in line or "Broad synthesis paper / anchor doc:" in line:
            match = DOC_POINTER_RE.search( line )
            if match:
                return match.group( "path" )
    return None


def parse_plan( text: str ) -> ParseResult:
    matches = list( SECTION_RE.finditer( text ) )
    sections: list[SectionSummary] = []
    warnings: list[str] = []

    for index, match in enumerate( matches ):
        title = match.group( "title" ).strip()
        body_start = match.end()
        body_end = matches[index + 1].start() if index + 1 < len( matches ) else len( text )
        body = text[body_start:body_end]

        status_match = STATUS_RE.search( body )
        explicit_status = status_match.group( "status" ).strip() if status_match else ""
        explicit_classification = classification_from_status( explicit_status ) if explicit_status else None
        heading_classification = classification_from_heading( title )

        classification = explicit_classification or heading_classification
        if classification is None:
            if explicit_status:
                warnings.append(
                    f"Skipped `{title}` because its explicit status `{explicit_status}` is not recognizable."
                )
            continue

        if explicit_classification and heading_classification and explicit_classification != heading_classification:
            warnings.append(
                f"`{title}` conflicts with its explicit status `{explicit_status}`. Using the explicit status."
            )

        if not explicit_classification and heading_classification:
            warnings.append(
                f"`{title}` has no explicit `**Status:**` line. Inferred `{heading_classification}` from the heading."
            )

        status_text = explicit_status or f"INFERRED FROM HEADING ({heading_classification.upper()})"
        sections.append(
            SectionSummary(
                title=strip_markdown( title ),
                status=strip_markdown( status_text ),
                classification=classification,
                gist=extract_first_paragraph( body ),
                doc_pointer=extract_doc_pointer( body ),
                section_index=index,
            )
        )

    return ParseResult( sections=sections, warnings=warnings )


def order_greenlit( sections: Iterable[SectionSummary] ) -> list[SectionSummary]:
    active = [section for section in sections if section.classification == "active"]
    backlog = [section for section in sections if section.classification == "greenlit"]
    bottom = [section for section in sections if "BOTTOM-OF-STACK" in section.status.upper()]
    normal_backlog = [section for section in backlog if section not in bottom]
    return active + normal_backlog + bottom


def format_item( index: int, section: SectionSummary ) -> str:
    parts = [f"{index}. {section.title}", f"[{section.status}]"]
    if section.gist:
        parts.append( section.gist )
    if section.doc_pointer:
        parts.append( f"({section.doc_pointer})" )
    return " ".join( parts )


def render_view( parse_result: ParseResult, view: str ) -> str:
    if view == "active":
        sections = [section for section in parse_result.sections if section.classification == "active"]
    elif view == "greenlit":
        sections = order_greenlit(
            [section for section in parse_result.sections if section.classification in {"active", "greenlit"}]
        )
    elif view == "parked":
        sections = [section for section in parse_result.sections if section.classification == "parked"]
    else:
        blocks = [
            render_view( parse_result, "active" ),
            render_view( parse_result, "greenlit" ),
            render_view( parse_result, "parked" ),
        ]
        return "\n\n".join( block for block in blocks if block ).strip()

    header = view
    if not sections:
        body = "(none)"
    else:
        body = "\n".join( format_item( index, section ) for index, section in enumerate( sections, start=1 ) )
    return f"{header}\n{body}".strip()


def render_warnings( warnings: list[str] ) -> str:
    if not warnings:
        return "warnings\n(none)"
    lines = ["warnings"]
    lines.extend( f"- {warning}" for warning in warnings )
    return "\n".join( lines )


def normalize_view_tokens( tokens: list[str] ) -> str:
    if not tokens:
        return "all"
    if tokens[0] == "/plan":
        if len( tokens ) == 1:
            return "all"
        tokens = tokens[1:]
    if len( tokens ) != 1:
        raise ValueError( "Expected `/plan`, `/plan <view>`, or a single view argument." )
    token = tokens[0].strip().lower()
    if token == "plan":
        return "all"
    if token not in {"active", "greenlit", "parked", "all"}:
        raise ValueError( "View must be one of: active, greenlit, parked, all." )
    return token


def build_output( plan_path: Path, view: str ) -> str:
    parse_result = parse_plan( plan_path.read_text( encoding="utf-8" ) )
    return "\n\n".join( [render_warnings( parse_result.warnings ), render_view( parse_result, view )] ).strip()


SAMPLE_THIN_PLAN = """
# Plan

## 1. Active — Example lane

This is the current active lane.

## 2. Parked concept chain — Example parked lane

This stays parked until later.
""".strip()


SAMPLE_CONTRADICTORY_PLAN = """
# Plan

## 1. Parked concept chain — Confused lane

**Status:** ACTIVE / GREENLIT TOOLING

This lane says two different things.

Canonical contract lives at `doc/confused.md`.
""".strip()


def run_self_test() -> None:
    current_plan = Path( "Plan.md" ).read_text( encoding="utf-8" )
    parsed_current = parse_plan( current_plan )
    current_active_titles = [section.title for section in parsed_current.sections if section.classification == "active"]
    current_greenlit_titles = [section.title for section in parsed_current.sections if section.classification == "greenlit"]
    current_parked_titles = [section.title for section in parsed_current.sections if section.classification == "parked"]

    assert current_active_titles == ["Active lane — Bandit perf + persistence budget probe v0"]
    assert current_greenlit_titles == [
        "Greenlit backlog — Locker clutter / perf guardrail probe v0",
    ]
    assert any( "Bandit overmap AI" in title for title in current_parked_titles )
    assert any( "Hackathon feature lanes" in title for title in current_parked_titles )
    assert any( "Hackathon feature lanes" in warning for warning in parsed_current.warnings )

    greenlit_render = render_view( parsed_current, "greenlit" )
    assert "Bandit perf + persistence budget probe v0" in greenlit_render
    assert "Locker clutter / perf guardrail probe v0" in greenlit_render

    thin_parsed = parse_plan( SAMPLE_THIN_PLAN )
    assert [section.classification for section in thin_parsed.sections] == ["active", "parked"]
    assert len( thin_parsed.warnings ) == 2

    contradictory_parsed = parse_plan( SAMPLE_CONTRADICTORY_PLAN )
    assert contradictory_parsed.sections[0].classification == "active"
    assert any( "conflicts with its explicit status" in warning for warning in contradictory_parsed.warnings )

    print( "plan_status_summary self-test passed" )


def main() -> int:
    parser = argparse.ArgumentParser( description="Print compact Plan.md status summaries." )
    parser.add_argument( "view", nargs="*", help="`/plan`, `/plan <view>`, or one of: active, greenlit, parked, all" )
    parser.add_argument( "--plan", default="Plan.md", help="Path to the Plan.md file to read." )
    parser.add_argument( "--self-test", action="store_true", help="Run built-in parser checks and exit." )
    args = parser.parse_args()

    if args.self_test:
        run_self_test()
        return 0

    try:
        view = normalize_view_tokens( args.view )
    except ValueError as exc:
        print( str( exc ), file=sys.stderr )
        return 2

    plan_path = Path( args.plan )
    if not plan_path.is_file():
        print( f"Plan file not found: {plan_path}", file=sys.stderr )
        return 2

    print( build_output( plan_path, view ) )
    return 0


if __name__ == "__main__":
    raise SystemExit( main() )
