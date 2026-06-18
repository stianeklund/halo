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
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterator, Optional

from .db import REPO_ROOT

KB_PATH = REPO_ROOT / "kb.json"
CONTEXT_CACHE = REPO_ROOT / "artifacts" / "auto_lift" / "context_cache"
VC71_SCORES_PATH = REPO_ROOT / "tools" / "verify" / "vc71_scores.json"
LIFT_RUNS_DIR = REPO_ROOT / "artifacts" / "lift_runs"
FAILURES_DIR = REPO_ROOT / "artifacts" / "auto_lift" / "failures"
BATCH_VERIFY_DIR = REPO_ROOT / "artifacts" / "batch_verify"

_FPU_RE = re.compile(
    r"\bfloat\b|\bFLD\b|\bFSTP\b|\bfVar\b|\bfloat10\b|\bFPU\b"
    r"|\bfabs\b|\bsqrt\b|\bfmod\b|\batan2\b|\bsin\b|\bcos\b",
    re.IGNORECASE,
)
_CALL_RE = re.compile(r"\b([a-zA-Z_]\w+)\s*\(")
_BRANCH_RE = re.compile(r"\b(if|else|switch|case|while|for)\b")

HAZARD_TAG_PATTERNS = {
    "xcall": re.compile(r"\bXCALL\b", re.IGNORECASE),
    "stack_alias": re.compile(r"&local_|buffer.alias|contiguous", re.IGNORECASE),
    "float_pointer": re.compile(r"float.*pointer|bit.smuggl|LODWORD|HIDWORD", re.IGNORECASE),
    "cdecl_grouping": re.compile(r"arg.mis.group|cdecl.*group|ADD ESP", re.IGNORECASE),
    "param_loop": re.compile(r"param.*\+\+|param.*loop|param.*corrupt", re.IGNORECASE),
    "accumulator": re.compile(r"accumulator|loop.state.*misread", re.IGNORECASE),
    "concat": re.compile(r"\bCONCAT\b", re.IGNORECASE),
    "void_eax": re.compile(r"void.*return.*EAX|implicit.*EAX|REP MOVSD", re.IGNORECASE),
    "addr_offset": re.compile(r"address.*offset.*value|addr.*add", re.IGNORECASE),
    "chkstk": re.compile(r"_chkstk|__chkstk", re.IGNORECASE),
    "register_order": re.compile(r"register.*order|caller.*site.*reg", re.IGNORECASE),
    "builder_count": re.compile(r"builder.*count|consumer.*ignor", re.IGNORECASE),
}


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
    vc71_score: Optional[float] = None
    call_count: Optional[int] = None
    branch_count: Optional[int] = None
    has_fpu: bool = False
    callee_set: Optional[str] = None


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


def _load_vc71_scores() -> dict[str, float]:
    """Load vc71_scores.json, return {function_name: score}."""
    if not VC71_SCORES_PATH.exists():
        return {}
    try:
        data = json.loads(VC71_SCORES_PATH.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {}
    scores = data.get("scores", {})
    return {name: entry["score"] for name, entry in scores.items()
            if isinstance(entry, dict) and "score" in entry}


def _extract_structural_features(
    c_source: Optional[str],
    pseudocode: Optional[str],
) -> tuple[int, int, bool, list[str]]:
    """Derive call_count, branch_count, has_fpu, callee_set from source text."""
    text = c_source or pseudocode or ""
    if not text:
        return 0, 0, False, []

    callees = set()
    call_count = 0
    skip_names = {
        "if", "else", "switch", "case", "while", "for", "return",
        "sizeof", "offsetof", "void", "int", "float", "char",
        "short", "long", "unsigned", "signed", "const", "static",
        "struct", "union", "enum", "typedef", "extern", "volatile",
        "do", "goto", "break", "continue", "default",
    }
    for m in _CALL_RE.finditer(text):
        name = m.group(1)
        if name not in skip_names:
            callees.add(name)
            call_count += 1

    branch_count = len(_BRANCH_RE.findall(text))
    has_fpu = bool(_FPU_RE.search(text))

    return call_count, branch_count, has_fpu, sorted(callees)


def _detect_hazard_tags(text: str) -> list[str]:
    """Scan text for known hazard patterns, return matching tag names."""
    tags = []
    for tag, pattern in HAZARD_TAG_PATTERNS.items():
        if pattern.search(text):
            tags.append(tag)
    return tags


def iter_ported_records() -> Iterator[FunctionRecord]:
    """Yield one FunctionRecord per ported function in kb.json.

    Records may have `pseudocode=None` and/or `c_source=None`. Callers
    decide whether to skip incomplete records or store them anyway.
    """
    raw = json.loads(KB_PATH.read_text(encoding="utf-8"))
    vc71_scores = _load_vc71_scores()
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

            call_count, branch_count, has_fpu, callee_list = (
                _extract_structural_features(c_source, pseudocode)
            )
            vc71_score = vc71_scores.get(name)

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
                vc71_score=vc71_score,
                call_count=call_count,
                branch_count=branch_count,
                has_fpu=has_fpu,
                callee_set=json.dumps(callee_list) if callee_list else None,
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


def iter_lift_outcomes() -> Iterator[dict]:
    """Yield outcome records from lift runs, failure records, and batch verify.

    Each dict has: addr, vc71_score, verdict, failure_reason, hazard_flags.
    Multiple sources may report on the same addr; callers should merge.
    """
    vc71_scores = _load_vc71_scores()

    for name, score in vc71_scores.items():
        addr_match = re.match(r"FUN_([0-9a-fA-F]+)", name)
        if addr_match:
            addr = "0x" + addr_match.group(1).lower()
        else:
            addr = None
        yield {
            "addr": addr,
            "name": name,
            "vc71_score": score,
            "verdict": None,
            "failure_reason": None,
            "hazard_flags": None,
        }

    if FAILURES_DIR.exists():
        for p in sorted(FAILURES_DIR.glob("*.json")):
            try:
                data = json.loads(p.read_text(encoding="utf-8"))
            except (OSError, json.JSONDecodeError):
                continue
            addr = data.get("address") or data.get("addr")
            if addr and not addr.startswith("0x"):
                addr = "0x" + addr
            reason = ""
            if data.get("reason"):
                reason = data["reason"]
            elif data.get("root_cause"):
                reason = data["root_cause"]
            elif data.get("attempts"):
                att = data["attempts"][0] if data["attempts"] else {}
                reason = att.get("error_summary", "")

            combined_text = f"{reason} {data.get('pipeline_output', '')}"
            tags = _detect_hazard_tags(combined_text)

            yield {
                "addr": addr,
                "name": data.get("target") or data.get("function"),
                "vc71_score": data.get("vc71_match"),
                "verdict": data.get("verdict", "reject").lower(),
                "failure_reason": reason[:500] if reason else None,
                "hazard_flags": json.dumps(tags) if tags else None,
            }

    if BATCH_VERIFY_DIR.exists():
        for p in sorted(BATCH_VERIFY_DIR.glob("*.json")):
            if p.name == "summary.json":
                continue
            try:
                data = json.loads(p.read_text(encoding="utf-8"))
            except (OSError, json.JSONDecodeError):
                continue
            target = data.get("target", "")
            addr_match = re.match(r"FUN_([0-9a-fA-F]+)", target)
            if not addr_match:
                continue
            addr = "0x" + addr_match.group(1).lower()
            status = data.get("status", "")
            verdict = "pass" if status == "pass" else ("fail" if status == "fail" else None)

            yield {
                "addr": addr,
                "name": target,
                "vc71_score": None,
                "verdict": verdict,
                "failure_reason": data.get("reason"),
                "hazard_flags": None,
            }
