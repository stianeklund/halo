"""Extract retrieval signals (pseudocode, C source, decl) for ported functions.

Called by `build_index.py` to populate the DuckDB index with one row per
ported function. The lifting agent's nearest-neighbor query later runs
against these rows.

Sources:
- pseudocode: `artifacts/auto_lift/context_cache/<NAME>.json#decompile_c`
  if it has been cached for this function. Many functions will be missing
  this field and that is fine — pseudocode embedding is optional, and the
  C-source embedding alone is still useful.
- c_source: parsed in-place from the function's source file using a small
  bracket-aware scanner. We deliberately do NOT preprocess via cpp here —
  the agent benefits from seeing the same indentation, comments, and
  intrinsic patterns the eventual lift will produce.
"""

from __future__ import annotations

import hashlib
import json
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Iterator, Optional

from .db import REPO_ROOT

KB_PATH = REPO_ROOT / "kb.json"
CONTEXT_CACHE = REPO_ROOT / "artifacts" / "auto_lift" / "context_cache"


@dataclass
class FunctionRecord:
    addr: str
    name: str
    obj_name: str
    source_path: Optional[str]
    decl: str
    pseudocode: Optional[str]
    c_source: Optional[str]
    pseudocode_sha: Optional[str]
    c_source_sha: Optional[str]


def _short_sha(text: Optional[str]) -> Optional[str]:
    if not text:
        return None
    return hashlib.sha256(text.encode("utf-8", errors="replace")).hexdigest()[:16]


def _parse_name_from_decl(decl: str) -> str:
    cleaned = re.sub(r"@<\w+>", "", decl)
    cleaned = re.sub(r"/\*.*?\*/", "", cleaned)
    m = re.search(r"(\w+)\s*\(", cleaned)
    return m.group(1) if m else ""


def _find_matching(text: str, start: int, open_ch: str, close_ch: str) -> int:
    """Return the index of the closing delimiter that matches `text[start]`.

    Comment-, char-, and string-aware so we do not get fooled by `{` or `}`
    inside string literals or `/* ... */` comments.
    """
    depth = 0
    i = start
    in_str = in_char = in_line = in_block = False
    n = len(text)
    while i < n:
        c = text[i]
        if in_line:
            if c == "\n":
                in_line = False
        elif in_block:
            if c == "*" and i + 1 < n and text[i + 1] == "/":
                in_block = False
                i += 1
        elif in_str:
            if c == "\\":
                i += 1
            elif c == '"':
                in_str = False
        elif in_char:
            if c == "\\":
                i += 1
            elif c == "'":
                in_char = False
        elif c == "/" and i + 1 < n:
            if text[i + 1] == "/":
                in_line = True
            elif text[i + 1] == "*":
                in_block = True
                i += 1
        elif c == '"':
            in_str = True
        elif c == "'":
            in_char = True
        elif c == open_ch:
            depth += 1
        elif c == close_ch:
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return -1


def slice_c_function(text: str, name: str) -> Optional[str]:
    """Return the function definition for `name` (signature + body) or None."""
    pattern = re.compile(rf"\b{re.escape(name)}\s*\(")
    for m in pattern.finditer(text):
        open_paren = text.find("(", m.start())
        close_paren = _find_matching(text, open_paren, "(", ")")
        if close_paren < 0:
            continue
        rest = close_paren + 1
        next_brace = text.find("{", rest)
        next_semi = text.find(";", rest)
        if next_brace < 0:
            continue
        if 0 <= next_semi < next_brace:
            continue  # prototype, not definition
        close_brace = _find_matching(text, next_brace, "{", "}")
        if close_brace < 0:
            continue
        # Walk back to start of the line containing the function name to
        # capture the return type / storage class.
        line_start = text.rfind("\n", 0, m.start()) + 1
        return text[line_start:close_brace + 1]
    return None


def _resolve_source(source: str) -> Path:
    if source.startswith("src/"):
        return REPO_ROOT / source
    return REPO_ROOT / "src" / "halo" / source


def _read_pseudocode(name: str) -> Optional[str]:
    p = CONTEXT_CACHE / f"{name}.json"
    if not p.exists():
        return None
    try:
        ctx = json.loads(p.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return None
    return ctx.get("decompile_c") or None


def iter_ported_records() -> Iterator[FunctionRecord]:
    """Yield one FunctionRecord per ported function in kb.json.

    Records may have `pseudocode=None` and/or `c_source=None`. Callers
    decide whether to skip incomplete records or store them anyway.
    """
    raw = json.loads(KB_PATH.read_text(encoding="utf-8"))
    # Cache one source-text load per file across all functions in that TU.
    text_cache: dict[Path, Optional[str]] = {}

    for obj in raw.get("objects", []):
        obj_name = obj.get("name", "")
        source = obj.get("source")
        for fn in obj.get("functions", []):
            if not fn.get("ported"):
                continue
            decl = fn.get("decl", "")
            addr = (fn.get("addr") or "").lower()
            name = _parse_name_from_decl(decl)
            if not name or not addr:
                continue

            c_source: Optional[str] = None
            source_path: Optional[str] = None
            if source:
                p = _resolve_source(source)
                if p not in text_cache:
                    try:
                        text_cache[p] = p.read_text(encoding="utf-8") if p.exists() else None
                    except OSError:
                        text_cache[p] = None
                text = text_cache[p]
                if text is not None:
                    c_source = slice_c_function(text, name)
                    source_path = str(p.relative_to(REPO_ROOT)) if p.is_relative_to(REPO_ROOT) else str(p)

            pseudocode = _read_pseudocode(name)

            yield FunctionRecord(
                addr=addr,
                name=name,
                obj_name=obj_name,
                source_path=source_path,
                decl=decl,
                pseudocode=pseudocode,
                c_source=c_source,
                pseudocode_sha=_short_sha(pseudocode),
                c_source_sha=_short_sha(c_source),
            )


def to_db_dict(rec: FunctionRecord) -> dict:
    return {
        "addr": rec.addr,
        "name": rec.name,
        "obj_name": rec.obj_name,
        "source_path": rec.source_path,
        "decl": rec.decl,
        "pseudocode": rec.pseudocode,
        "c_source": rec.c_source,
        "pseudocode_sha": rec.pseudocode_sha,
        "c_source_sha": rec.c_source_sha,
    }
