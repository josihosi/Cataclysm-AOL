"""Omit unchanged, explicitly marked guidance from a reused worker's delivery.

The complete original packet remains the authority and comparison input. This
helper owns no cache and makes no assertion about which conversation received it.
"""
from __future__ import annotations

import re


_PREFIX = '<!-- worker-standing'
_NAME = r'[A-Za-z0-9][A-Za-z0-9_.-]*'
_MARKER = re.compile(r'<!-- worker-standing:(begin|end) name="(' + _NAME + r')" -->')


def standing_section(name: str, text: str) -> str:
    """Mark renderer-owned standing text; task and owner text stays outside."""
    if not isinstance(name, str) or re.fullmatch(_NAME, name) is None:
        raise ValueError('Standing section needs a simple nonempty name')
    return (f'<!-- worker-standing:begin name="{name}" -->\n'
            + text + ('\n' if text and not text.endswith('\n') else '')
            + f'<!-- worker-standing:end name="{name}" -->\n')


def _sections(text: str) -> dict[str, tuple[int, int, str]] | None:
    sections = {}
    opened = None
    offset = 0
    for line in text.splitlines(keepends=True):
        if _PREFIX in line:
            marker = _MARKER.fullmatch(line.rstrip('\r\n'))
            if marker is None:
                return None
            kind, name = marker.groups()
            if kind == 'begin':
                if opened is not None or name in sections:
                    return None
                opened = (name, offset, offset + len(line))
            else:
                if opened is None or opened[0] != name:
                    return None
                sections[name] = (opened[1], offset + len(line), text[opened[2]:offset])
                opened = None
        offset += len(line)
    return sections if opened is None and sections else None


def delivery_text(current: str, previous: str | None = None) -> tuple[str, dict]:
    """Compare exact named contents; ambiguous or legacy packets stay complete."""
    current_sections = _sections(current)
    previous_sections = _sections(previous) if previous is not None else None
    metadata = {'comparison_available': current_sections is not None and previous_sections is not None,
                'full_utf8_bytes': len(current.encode('utf-8')),
                'delivered_utf8_bytes': len(current.encode('utf-8')),
                'omitted_sections': [], 'changed_sections': []}
    if not metadata['comparison_available']:
        return current, metadata
    parts = []
    offset = 0
    for name, (start, end, body) in current_sections.items():
        parts.append(current[offset:start])
        earlier = previous_sections.get(name)
        if earlier is not None and earlier[2] == body:
            parts.append(f'Standing guidance {name} unchanged.\n')
            metadata['omitted_sections'].append(name)
        else:
            parts.append(current[start:end])
            metadata['changed_sections'].append(name)
        offset = end
    parts.append(current[offset:])
    rendered = ''.join(parts)
    metadata['delivered_utf8_bytes'] = len(rendered.encode('utf-8'))
    return rendered, metadata
