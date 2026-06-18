"""Hazard pattern detection and lift-learnings section lookup.

Scans Ghidra pseudocode for known hazard patterns and surfaces the
relevant lift-learnings sections as compact briefs for injection.
"""

from __future__ import annotations

import re
from pathlib import Path
from typing import Optional

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
LIFT_LEARNINGS_PATH = REPO_ROOT / "docs" / "lift-learnings.md"

HAZARD_PATTERNS: dict[str, dict] = {
    "xcall": {
        "section": 1,
        "keywords": [r"\bXCALL\b", r"\bthunk_FUN_", r"indirect.call"],
    },
    "stack_alias": {
        "section": 2,
        "keywords": [r"&local_\w+", r"\bmemset\b.*\b0x[4-9a-f][0-9a-f]", r"contiguous"],
    },
    "reg_arg_null": {
        "section": 3,
        "keywords": [r"@<\w+>.*NULL", r"register.*NULL.*arg"],
    },
    "param_loop": {
        "section": 4,
        "keywords": [r"param_\d\s*\+\+", r"param_\d\s*\+=", r"param_\d\s*=\s*param_\d\s*\+"],
    },
    "float_pointer": {
        "section": 6,
        "keywords": [r"\bLODWORD\b", r"\bHIDWORD\b", r"\b\(float\)\(int\)", r"float.*union"],
    },
    "cdecl_grouping": {
        "section": 7,
        "keywords": [r"ADD\s+ESP\s*,\s*0x[0-9a-f]+", r"\(\)\s*;.*\bPUSH\b"],
    },
    "accumulator": {
        "section": 8,
        "keywords": [r"\biVar\d+\b.*\+=", r"running.*total", r"count\s*\+="],
    },
    "register_order": {
        "section": 10,
        "keywords": [r"@<e[abcd]x>", r"@<e[sd]i>"],
    },
    "builder_count": {
        "section": 11,
        "keywords": [r"_new\b.*return", r"builder.*count", r"create.*return.*int"],
    },
    "concat": {
        "section": 13,
        "keywords": [r"\bCONCAT\d+\b", r"\bCONCAT22\b", r"\bCONCAT44\b"],
    },
    "void_eax": {
        "section": 16,
        "keywords": [r"\bREP\s+MOVSD\b", r"void.*\*.*param_1.*return", r"out.param.*EAX"],
    },
    "addr_offset": {
        "section": 17,
        "keywords": [r"0x[0-9a-f]{5,}\s*\+\s*\d", r"DAT_\w+\s*\+"],
    },
    "chkstk": {
        "section": 20,
        "keywords": [r"\b_chkstk\b", r"\b__chkstk\b"],
    },
}

_section_cache: dict[int, str] = {}


def detect_hazards(pseudocode: str) -> list[str]:
    """Scan pseudocode for known hazard patterns, return matching tag names."""
    tags = []
    for tag, info in HAZARD_PATTERNS.items():
        for kw in info["keywords"]:
            if re.search(kw, pseudocode, re.IGNORECASE):
                tags.append(tag)
                break
    return tags


def _load_sections() -> dict[int, str]:
    """Parse lift-learnings.md into {section_num: section_text}."""
    global _section_cache
    if _section_cache:
        return _section_cache

    if not LIFT_LEARNINGS_PATH.exists():
        return {}

    text = LIFT_LEARNINGS_PATH.read_text(encoding="utf-8")
    lines = text.splitlines(keepends=True)

    header_re = re.compile(r"^## (\d+)\.\s+(.+)")
    sections: dict[int, str] = {}
    current_num: Optional[int] = None
    current_lines: list[str] = []

    for line in lines:
        m = header_re.match(line)
        if m:
            if current_num is not None:
                sections[current_num] = "".join(current_lines)
            current_num = int(m.group(1))
            current_lines = [line]
        elif current_num is not None:
            current_lines.append(line)

    if current_num is not None:
        sections[current_num] = "".join(current_lines)

    _section_cache = sections
    return sections


def read_section(section_num: int) -> Optional[str]:
    """Return the full text of a lift-learnings section by number."""
    sections = _load_sections()
    return sections.get(section_num)


def _extract_prevention(section_text: str) -> str:
    """Extract actionable bullet points from a section."""
    lines = section_text.splitlines()

    # Look for Prevention/Detection subsection first
    in_prevention = False
    result: list[str] = []
    for line in lines:
        lower = line.strip().lower()
        if lower.startswith("### prevention") or lower.startswith("### detection"):
            in_prevention = True
            continue
        if in_prevention:
            if line.startswith("### ") or line.startswith("## "):
                break
            result.append(line)
    if result:
        return "\n".join(result).strip()

    # Fallback: grab all bullet lines from the section (they contain the
    # actionable guidance in lift-learnings)
    bullets = [l for l in lines[1:] if l.strip().startswith("- ")]
    return "\n".join(bullets[:5]).strip()


def format_hazard_brief(tag: str, max_chars: int = 400) -> str:
    """Format a single hazard tag as a compact warning with section content."""
    info = HAZARD_PATTERNS.get(tag)
    if not info:
        return ""
    section_num = info["section"]
    section_text = read_section(section_num)
    if not section_text:
        return f"- lift-learnings section {section_num} (not found on disk)"

    header_line = section_text.splitlines()[0] if section_text else ""
    prevention = _extract_prevention(section_text)

    brief = f"**{header_line.strip()}**"
    if prevention:
        brief += f"\n{prevention}"

    if len(brief) > max_chars:
        brief = brief[:max_chars - 15] + "\n... (truncated)"
    return brief


def format_hazard_briefs(tags: list[str], max_chars: int = 1000) -> str:
    """Format multiple hazard tags as a combined warning block."""
    if not tags:
        return ""

    parts = ["## Relevant lift-learnings sections\n"]
    budget = max_chars - len(parts[0])

    per_tag_budget = max(200, budget // len(tags))

    for tag in tags:
        brief = format_hazard_brief(tag, max_chars=per_tag_budget)
        if brief:
            parts.append(brief)

    text = "\n\n".join(parts)
    if len(text) > max_chars:
        text = text[:max_chars - 20] + "\n\n... (truncated)"
    return text
