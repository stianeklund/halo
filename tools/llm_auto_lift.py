#!/usr/bin/env python3
"""Auto-Lift Target Selector & Context Cache.

Provides liftability scoring, frontier-aware target selection, and Ghidra
context caching. Code generation is delegated to the /lift skill which runs
inside Claude Code with full agent context.

Usage:
    python3 tools/llm_auto_lift.py score              # Rank unported functions
    python3 tools/llm_auto_lift.py select             # Combine frontier + liftability
    python3 tools/llm_auto_lift.py cache-context ...  # Build Ghidra context packs
    python3 tools/llm_auto_lift.py review             # Show legacy batch results
    python3 tools/llm_auto_lift.py promote            # Apply legacy batch results
"""

from __future__ import annotations

import argparse
import json
import logging
import os
import re
import subprocess
import sys
import time
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Optional

_tools_dir = os.path.dirname(os.path.abspath(__file__))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)

log = logging.getLogger("auto_lift")

ROOT = Path(__file__).resolve().parent.parent
ARTIFACT_ROOT = ROOT / "artifacts" / "auto_lift"
CONTEXT_CACHE = ARTIFACT_ROOT / "context_cache"
DELINKED_DIR = ROOT / "delinked"
OBJDIFF_JSON = ROOT / "objdiff.json"
KB_JSON = ROOT / "kb.json"
SRC_DIR = ROOT / "src"
MIZUCHI_DIR = ROOT / "artifacts" / "mizuchi"

MSVC_INTRINSIC_ADDRS = {
    "1d90e0", "1d9068", "1dd5c8", "1dd601",
    "1dd620", "1dd660", "1dd680", "1dd770",
}
INTRINSIC_RE = re.compile(
    r"0x(?:" + "|".join(MSVC_INTRINSIC_ADDRS) + r")\b", re.IGNORECASE
)

DISQUALIFIED_OBJECTS = {"<xdk_stubs>", "<common>"}

FAILURES_DIR = ARTIFACT_ROOT / "failures"

# States (used by legacy review/promote)
AUTO_ACCEPT = "auto_accept"
NEEDS_REVIEW = "needs_review"
REJECT = "reject"

# Known callee buffer requirements (synced with tools/audit/check_lift_hazards.py)
KNOWN_BUFFER_SIZES = {
    'FUN_0013fc20': (0x88, 'object placement init — memsets 0x88 bytes'),
}


# ---------------------------------------------------------------------------
# Helpers for decl parsing and context enrichment
# ---------------------------------------------------------------------------

def _decl_name(decl: str) -> str:
    """Extract function name from a kb.json decl string."""
    left = decl.split("(", 1)[0].strip()
    token = re.split(r"[\s\*]+", left)[-1]
    return token.strip()


def _decl_param_count(decl: str) -> int:
    """Count comma-separated parameters in a decl string."""
    m = re.search(r"\(([^)]*)\)", decl)
    if not m:
        return 0
    params = m.group(1).strip()
    if not params or params == "void":
        return 0
    depth = 0
    count = 1
    for ch in params:
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        elif ch == "," and depth == 0:
            count += 1
    return count


def _unwrap_mcp_text(text: str) -> str:
    """Unwrap MCP response JSON-string-wrapped text to raw decompile/disasm."""
    if not text:
        return ""
    try:
        inner = json.loads(text)
        if isinstance(inner, dict):
            return inner.get("result", text)
    except (json.JSONDecodeError, ValueError):
        pass
    return text


_PUSH_SCAN_SKIP = {
    "MOV", "MOVZX", "MOVSX", "CMOVB", "CMOVNB", "CMOVE", "CMOVNE",
    "LEA", "XOR", "AND", "OR", "NOT",
    "SHL", "SHR", "SAL", "SAR", "ROL", "ROR", "RCL", "RCR",
    "XCHG", "BSWAP", "CDQ", "CWDE", "CBW",
    "CMP", "TEST", "NOP",
    "FLD", "FST", "FADD", "FSUB", "FMUL", "FDIV",
    "FCOM", "FCOMP", "FUCOM", "FUCOMP", "FTST", "FABS", "FCHS",
    "FSTP",  # non-ESP already handled; ESP targets collected earlier
    "FNSTCW", "FLDCW", "FNSTSW", "FNCLEX",
}
_CTRL_FLOW_MNEMONICS = {
    "CALL", "RET", "RETN", "RETF", "IRET", "INT",
    "JMP", "JA", "JAE", "JB", "JBE", "JC", "JE", "JG", "JGE", "JL",
    "JLE", "JNA", "JNAE", "JNB", "JNBE", "JNC", "JNE", "JNG", "JNGE",
    "JNL", "JNLE", "JNO", "JNP", "JNS", "JNZ", "JO", "JP", "JPE",
    "JPO", "JS", "JZ", "JCXZ", "JECXZ",
    "LOOP", "LOOPE", "LOOPNE", "LOOPNZ", "LOOPZ",
}

_DISASM_LINE_RE = re.compile(
    r'^\s*([0-9a-fA-F]{8}):\s+([A-Z][A-Z0-9]*)\s*(.*?)\s*$'
)

_STRUCT_ACCESS_RE = re.compile(
    r'\b(MOV|LEA|MOVZX|MOVSX|FSTP|FST|FLD)\b.*\[(\w+)\s*\+\s*(0x[0-9a-fA-F]+)\]',
    re.IGNORECASE,
)
_BUF_DECL_RE = re.compile(
    r'(?:uint8_t|char|byte|undefined1|undefined)\s+(local_[0-9a-fA-F]+)\s*\[\s*(0x[0-9a-fA-F]+|\d+)\s*\]',
)


# ---------------------------------------------------------------------------
# Data classes
# ---------------------------------------------------------------------------

@dataclass
class LiftTarget:
    addr: str
    name: str
    decl: str
    object_name: str
    source_path: str
    has_reg_args: bool
    register_args: list[tuple[int, str]]
    score: int = 0
    score_details: dict = field(default_factory=dict)


@dataclass
class SelectedTarget:
    target: LiftTarget
    total_score: int
    liftability_score: int
    frontier_score: int
    frontier_rank: Optional[int]
    lane: str
    reasons: list[str]


@dataclass
class ContextPack:
    schema_version: int
    target: dict
    ghidra: dict
    kb_context: dict
    source_context: dict
    hazards: dict
    delinked_available: bool
    constraints: list[str]


# ---------------------------------------------------------------------------
# Knowledge base helpers
# ---------------------------------------------------------------------------

def _load_kb_raw() -> dict:
    return json.loads(KB_JSON.read_text(encoding="utf-8"))


def _load_objdiff_units() -> dict[str, dict]:
    if not OBJDIFF_JSON.exists():
        return {}
    data = json.loads(OBJDIFF_JSON.read_text(encoding="utf-8"))
    units = {}
    for u in data.get("units", []):
        src = u.get("metadata", {}).get("source_path", "")
        if src:
            units[src] = u
    return units


def _find_objdiff_unit(source_path: str, units: dict[str, dict]) -> Optional[dict]:
    source_path = source_path.replace("\\", "/")
    for key, unit in units.items():
        if source_path.endswith(key) or key.endswith(source_path):
            return unit
    return None


def _check_rejected_branch(name: str) -> Optional[tuple[str, str]]:
    """Return (branch_name, failure_stage) if a preserved rejected branch exists, else None."""
    failure_file = FAILURES_DIR / f"{name}.json"
    if not failure_file.exists():
        return None
    try:
        rec = json.loads(failure_file.read_text())
        branch = rec.get("branch", "")
        if not branch:
            return None
        # Verify the branch still exists in git
        result = subprocess.run(
            ["git", "branch", "--list", branch],
            capture_output=True, text=True, cwd=ROOT,
        )
        if branch not in result.stdout:
            return None
        stage = ""
        for attempt in rec.get("attempts", []):
            stage = attempt.get("failure_stage", "")
        return branch, stage
    except Exception:
        return None


_MANIFEST_CACHE: dict | None = None

def _load_manifest() -> dict:
    global _MANIFEST_CACHE
    if _MANIFEST_CACHE is None:
        p = DELINKED_DIR / "manifest.json"
        try:
            _MANIFEST_CACHE = json.loads(p.read_text()) if p.exists() else {}
        except (json.JSONDecodeError, OSError):
            _MANIFEST_CACHE = {}
    return _MANIFEST_CACHE


def _delinked_ref_status(source_path: str, addr: str, units: dict[str, dict]) -> str:
    """Return 'ok' | 'per_function' | 'needs_delink' | 'none'.

    'ok'           — TU-level delinked obj exists and is not a split TU.
    'per_function' — per-function obj exists at delinked/functions/<addr>.obj.
    'needs_delink' — TU is split, function not in it, per-function obj missing.
    'none'         — no delinked reference at all.
    """
    # Per-function file takes priority (already exported and usable)
    if addr:
        try:
            addr_hex = f"{int(addr, 16):08x}"
            if (DELINKED_DIR / "functions" / f"{addr_hex}.obj").exists():
                return "per_function"
        except ValueError:
            pass

    unit = _find_objdiff_unit(source_path, units)
    if not unit:
        return "none"
    base = unit.get("base_path", "")
    if not base or not (ROOT / base).exists():
        return "none"

    # TU file exists — is it a split TU that won't contain this function?
    # Match by obj_path since manifest keys use ':' (e.g. 'XNET:wsock.obj')
    # but file paths use '/' (e.g. 'delinked/XNET/wsock.obj').
    ref_abs = str((ROOT / base).resolve())
    is_split = any(
        v.get("split") and str(Path(v.get("obj_path", "")).resolve()) == ref_abs
        for v in _load_manifest().values()
        if isinstance(v, dict)
    )
    if is_split:
        return "needs_delink"

    return "ok"


def _has_delinked_ref(source_path: str, units: dict[str, dict]) -> bool:
    unit = _find_objdiff_unit(source_path, units)
    if not unit:
        return False
    base = unit.get("base_path", "")
    return (ROOT / base).exists() if base else False


_PDB_PROPOSALS_CACHE: Optional[set[str]] = None

# ---------------------------------------------------------------------------
# Source-implementation checker
# ---------------------------------------------------------------------------

_SOURCE_IMPL_CACHE: Optional[dict[str, str]] = None


def _build_source_impl_cache() -> dict[str, str]:
    """Scan src/ for FUN_<addr> function definitions.

    Returns a dict mapping normalized address (e.g. '0x5a640') to the first
    matching 'file:line' location.  Cached for the process lifetime.
    """
    global _SOURCE_IMPL_CACHE
    if _SOURCE_IMPL_CACHE is not None:
        return _SOURCE_IMPL_CACHE

    cache: dict[str, str] = {}
    fn_def_re = re.compile(
        r'^[A-Za-z_][A-Za-z0-9_*\s]+\bFUN_([0-9a-fA-F]{8})\s*\('
    )
    src_dir = ROOT / "src"
    for c_file in src_dir.rglob("*.c"):
        try:
            for lineno, line in enumerate(
                c_file.read_text(encoding="utf-8", errors="ignore").splitlines(), 1
            ):
                m = fn_def_re.match(line)
                if m:
                    addr_norm = "0x" + hex(int(m.group(1), 16))[2:]
                    if addr_norm not in cache:
                        rel = str(c_file.relative_to(ROOT))
                        cache[addr_norm] = f"{rel}:{lineno}"
        except OSError:
            pass
    _SOURCE_IMPL_CACHE = cache
    return cache


def _is_already_in_source(addr: str) -> Optional[str]:
    """Return 'file:line' if addr has a definition in src/, else None."""
    cache = _build_source_impl_cache()
    normalized = "0x" + hex(int(addr.lower().removeprefix("0x"), 16))[2:]
    return cache.get(normalized)


def _load_pdb_proposal_addrs() -> set[str]:
    """Return the set of addresses (lowercase hex with 0x prefix) that have
    a real-name proposal from the punpckhdq PDB import. Empty if the importer
    has not been run.
    """
    global _PDB_PROPOSALS_CACHE
    if _PDB_PROPOSALS_CACHE is not None:
        return _PDB_PROPOSALS_CACHE
    proposals_path = ROOT / "artifacts" / "punpckhdq_import" / "name_proposals.json"
    addrs: set[str] = set()
    if proposals_path.exists():
        try:
            data = json.loads(proposals_path.read_text(encoding="utf-8"))
            for p in data:
                if (p.get("real_name")
                        and p.get("confidence") in ("high", "medium")
                        and p.get("our_addr")):
                    addrs.add(p["our_addr"].lower())
        except (json.JSONDecodeError, OSError):
            pass
    _PDB_PROPOSALS_CACHE = addrs
    return addrs


_LEAF_CACHE_DATA: dict | None = None


def _load_leaf_cache() -> dict[str, dict]:
    """Return the full leaf classification cache.

    Each entry is {addr: {"class": "leaf"|"data_only"|"stubbable"|"non_leaf", ...}}.
    Handles both legacy string values and the extended dict schema.
    """
    global _LEAF_CACHE_DATA
    if _LEAF_CACHE_DATA is not None:
        return _LEAF_CACHE_DATA
    cache_path = ROOT / "tools" / "equivalence" / "leaf_cache.json"
    result: dict[str, dict] = {}
    if cache_path.exists():
        try:
            data = json.loads(cache_path.read_text(encoding="utf-8"))
            for k, v in data.items():
                if isinstance(v, str):
                    result[k.lower()] = {"class": v}
                elif isinstance(v, dict):
                    result[k.lower()] = v
        except (json.JSONDecodeError, OSError):
            pass
    _LEAF_CACHE_DATA = result
    return result


def _load_pure_leaf_addrs() -> set[str]:
    """Return the set of addresses classified as pure leaves."""
    return {a for a, d in _load_leaf_cache().items() if d.get("class") == "leaf"}


# ---------------------------------------------------------------------------
# Semantic retrieval helpers (Mizuchi-style neighbor injection)
# ---------------------------------------------------------------------------

def _check_retrieval_index() -> bool:
    """Return True if the retrieval index exists and has embeddings."""
    idx = ROOT / "tools" / "retrieval" / "index.duckdb"
    if not idx.exists():
        return False
    try:
        from retrieval import db as _ret_db
        con = _ret_db.connect(read_only=True)
        s = _ret_db.stats(con)
        con.close()
        return s.get("with_emb", 0) > 0
    except Exception:
        return False


def _query_retrieval_neighbors(pseudocode: str, top_k: int = 3) -> list[dict]:
    """Query the retrieval index for the top-K most-similar ported functions.

    Returns a list of dicts suitable for direct JSON serialization into
    the cached context pack. Returns [] on any error so cache-context
    never fails due to retrieval issues.
    """
    try:
        from retrieval.query import query_neighbors
        neighbors = query_neighbors(pseudocode, top_k=top_k, min_similarity=0.35)
        return [
            {
                "addr": n.addr,
                "name": n.name,
                "obj_name": n.obj_name,
                "decl": n.decl,
                "similarity": round(n.similarity, 4),
                "c_source": n.c_source,
            }
            for n in neighbors
        ]
    except Exception as exc:
        log.debug("retrieval query failed: %s", exc)
        return []


# ---------------------------------------------------------------------------
# Parsing helpers
# ---------------------------------------------------------------------------

def _parse_name_from_decl(decl: str) -> str:
    cleaned = re.sub(r"@<\w+>", "", decl)
    m = re.search(r"(\w+)\s*\(", cleaned)
    return m.group(1) if m else ""


def _parse_register_args(decl: str) -> list[tuple[int, str]]:
    open_paren = decl.find("(")
    close_paren = decl.rfind(")")
    if open_paren < 0 or close_paren < 0:
        return []
    params_src = decl[open_paren + 1 : close_paren]
    depth = 0
    buf: list[str] = []
    params: list[str] = []
    for ch in params_src:
        if ch in "(<":
            depth += 1
            buf.append(ch)
        elif ch in ")>":
            depth -= 1
            buf.append(ch)
        elif ch == "," and depth == 0:
            params.append("".join(buf))
            buf = []
        else:
            buf.append(ch)
    if buf:
        params.append("".join(buf))
    result = []
    reg_re = re.compile(r"@<(\w+)>")
    for i, p in enumerate(params):
        m = reg_re.search(p)
        if m:
            result.append((i, m.group(1).lower()))
    return result


def _count_params(decl: str) -> int:
    open_paren = decl.find("(")
    close_paren = decl.rfind(")")
    if open_paren < 0 or close_paren < 0:
        return 0
    inner = decl[open_paren + 1 : close_paren].strip()
    if not inner or inner == "void":
        return 0
    depth = 0
    count = 1
    for ch in inner:
        if ch in "(<":
            depth += 1
        elif ch in ")>":
            depth -= 1
        elif ch == "," and depth == 0:
            count += 1
    return count


# ---------------------------------------------------------------------------
# Liftability Scorer
# ---------------------------------------------------------------------------

class LiftabilityScorer:

    def __init__(self):
        self.kb_raw = _load_kb_raw()
        self.objdiff_units = _load_objdiff_units()

    def score_all(self, *, object_filter: str = "", min_score: int = 0) -> list[LiftTarget]:
        targets = []
        for obj in self.kb_raw["objects"]:
            obj_name = obj.get("name", "")
            if obj_name in DISQUALIFIED_OBJECTS:
                continue
            source = obj.get("source", "")
            if not source:
                continue
            if object_filter and obj_name != object_filter:
                continue

            source_path = f"src/halo/{source}" if not source.startswith("src/") else source
            source_exists = (ROOT / source_path).exists()

            ported_count = sum(
                1 for f in obj.get("functions", []) if f.get("ported")
            )

            total_funcs = len(obj.get("functions", []))
            unported_count = total_funcs - ported_count

            for func in obj.get("functions", []):
                if func.get("ported"):
                    continue
                decl = func.get("decl", "")
                addr = func.get("addr", "")
                name = _parse_name_from_decl(decl)
                reg_args = _parse_register_args(decl)
                param_count = _count_params(decl)

                # Hard disqualifiers
                if not name:
                    continue
                if len(reg_args) > 2:
                    continue

                # Skip functions already implemented in source (kb.json drift).
                # These have a FUN_<addr> definition in src/ but ported is not
                # yet set to true — re-selecting them wastes a full lift attempt.
                src_loc = _is_already_in_source(addr)
                if src_loc:
                    log.debug("skip %s (%s): implementation in %s", name, addr, src_loc)
                    continue

                score = 0
                details: dict[str, int] = {}

                # Delinked reference
                _dref = _delinked_ref_status(source_path, addr, self.objdiff_units)
                if _dref in ("ok", "per_function"):
                    score += 20
                    details["delinked_ref"] = 20
                elif _dref == "needs_delink":
                    score += 10
                    details["needs_delink"] = 10

                # No register args
                if not reg_args:
                    score += 15
                    details["no_reg_args"] = 15

                # Few parameters
                if param_count <= 3:
                    score += 10
                    details["few_params"] = 10

                # Source file exists with ported siblings
                if source_exists and ported_count > 0:
                    score += 10
                    details["active_tu"] = 10
                elif source_exists:
                    score += 3
                    details["source_exists"] = 3

                # Standard calling convention
                if "__stdcall" not in decl:
                    score += 5
                    details["cdecl"] = 5

                # Completing a TU (last 1-2 remaining)
                if ported_count > 0 and unported_count <= 2:
                    score += 12
                    details["completes_tu"] = 12

                # PDB-derived real name proposal exists for this address —
                # the punpckhdq corpus gives us a strong naming hint for the
                # lift agent and reduces guesswork.
                pdb_addrs = _load_pdb_proposal_addrs()
                if addr.lower() in pdb_addrs:
                    score += 10
                    details["pdb_named"] = 10

                leaf_entry = _load_leaf_cache().get(addr.lower(), {})
                leaf_class = leaf_entry.get("class", "")
                if leaf_class == "leaf":
                    score += 5
                    details["eq_pure_leaf"] = 5
                elif leaf_class == "data_only":
                    score += 3
                    details["eq_data_only"] = 3
                elif leaf_class == "stubbable":
                    score += 3
                    details["eq_stubbable"] = 3
                if leaf_entry.get("z3_proven"):
                    score += 5
                    details["z3_proven"] = 5
                cached_confidence = leaf_entry.get("confidence", "")
                if cached_confidence == "high":
                    score += 3
                    details["eq_high_conf"] = 3

                # Cached Ghidra context available
                cache_file = CONTEXT_CACHE / f"{name}.json"
                if cache_file.exists():
                    score += 5
                    details["cached_ctx"] = 5
                    # Estimate complexity from decompiler output
                    try:
                        ctx = json.loads(cache_file.read_text(encoding="utf-8"))
                        decomp_lines = len(ctx.get("decompile_c", "").splitlines())
                        if decomp_lines <= 30:
                            score += 8
                            details["small_func"] = 8
                        elif decomp_lines <= 60:
                            score += 4
                            details["medium_func"] = 4
                    except (json.JSONDecodeError, OSError):
                        pass

                if score < min_score:
                    continue

                targets.append(LiftTarget(
                    addr=addr,
                    name=name,
                    decl=decl,
                    object_name=obj_name,
                    source_path=source_path,
                    has_reg_args=bool(reg_args),
                    register_args=reg_args,
                    score=score,
                    score_details=details,
                ))

        targets.sort(key=lambda t: -t.score)
        return targets


def _load_frontier_priorities(limit: int) -> dict[str, dict]:
    cmd = [
        sys.executable,
        str(ROOT / "tools" / "analysis" / "frontier.py"),
        "--limit",
        str(limit),
        "--json",
    ]
    proc = subprocess.run(cmd, cwd=str(ROOT), capture_output=True, text=True)
    if proc.returncode != 0:
        log.warning("frontier.py failed; continuing with liftability-only selection")
        return {}

    try:
        payload = json.loads(proc.stdout)
    except json.JSONDecodeError:
        log.warning("frontier.py did not emit valid JSON; continuing with liftability-only selection")
        return {}

    priorities: dict[str, dict] = {}
    for rank, row in enumerate(payload.get("recommended", []), 1):
        object_bonus = max(0, 22 - rank * 2)
        object_name = str(row.get("object", ""))
        for candidate_index, candidate in enumerate(row.get("candidates", [])):
            name = candidate.get("name", "")
            addr = str(candidate.get("addr", "")).lower()
            if not name:
                continue
            candidate_bonus = max(0, 5 - candidate_index)
            bonus = object_bonus + candidate_bonus
            entry = {
                "bonus": bonus,
                "rank": rank,
                "object": object_name,
                "object_score": int(row.get("score", 0) or 0),
            }
            priorities[name] = entry
            if addr:
                priorities[addr] = entry
                priorities[addr.removeprefix("0x")] = entry
    return priorities


def _load_structural_prescreens() -> dict[str, "FunctionFeatures"]:
    """Run the structural pre-screener on delinked references.

    Returns a dict keyed by function name with difficulty assessments.
    Silently returns empty if the pre-screener is unavailable.
    """
    try:
        from analysis.structural_prescreen import screen_all
        results = screen_all()
        return {r.name: r for r in results}
    except Exception as exc:
        log.debug("structural prescreen unavailable: %s", exc)
        return {}


def _select_targets(
    targets: list[LiftTarget],
    *,
    frontier_limit: int,
    auto_threshold: int,
) -> list[SelectedTarget]:
    frontier = _load_frontier_priorities(frontier_limit)
    structural = _load_structural_prescreens()
    selected: list[SelectedTarget] = []

    for target in targets:
        frontier_entry = frontier.get(target.name) or frontier.get(target.addr.lower())
        frontier_score = int(frontier_entry.get("bonus", 0)) if frontier_entry else 0
        total_score = target.score + frontier_score
        reasons = [f"{k}=+{v}" for k, v in target.score_details.items()]
        frontier_rank = None
        if frontier_entry:
            frontier_rank = int(frontier_entry.get("rank", 0) or 0)
            reasons.append(f"frontier_rank={frontier_rank}")
            reasons.append(f"frontier=+{frontier_score}")

        if target.score >= auto_threshold and target.score_details.get("delinked_ref"):
            lane = "auto-lift"
        elif target.score >= auto_threshold:
            lane = "cache-context"
        elif frontier_score:
            lane = "manual-lift"
        else:
            lane = "defer"

        prescreen = structural.get(target.name)
        if prescreen:
            if prescreen.difficulty == "reject":
                total_score -= 50
                lane = "defer"
                reasons.append(f"struct_reject=-50({','.join(prescreen.risk_factors)})")
            elif prescreen.difficulty == "hard":
                total_score -= 20
                if lane == "auto-lift":
                    lane = "manual-lift"
                reasons.append(f"struct_hard=-20({','.join(prescreen.risk_factors)})")
            elif prescreen.difficulty == "easy":
                if prescreen.difficulty_score == 0:
                    bonus = 15
                elif prescreen.difficulty_score <= 5:
                    bonus = 8
                else:
                    bonus = 5
                total_score += bonus
                reasons.append(f"struct_easy=+{bonus}(score={prescreen.difficulty_score})")

        rejected = _check_rejected_branch(target.name)
        if rejected:
            branch, stage = rejected
            reasons.append(f"prior_rejected_branch({stage or 'unknown'})")

        selected.append(SelectedTarget(
            target=target,
            total_score=total_score,
            liftability_score=target.score,
            frontier_score=frontier_score,
            frontier_rank=frontier_rank,
            lane=lane,
            reasons=reasons,
        ))

    selected.sort(key=lambda item: (-item.total_score, item.target.object_name, item.target.addr))
    return selected


# ---------------------------------------------------------------------------
# Context Pack Builder
# ---------------------------------------------------------------------------

class ContextPackBuilder:

    def __init__(self, *, ghidra_live: bool = False, kb_raw: dict | None = None, objdiff_units: dict | None = None):
        self.ghidra_live = ghidra_live
        self.kb_raw = kb_raw if kb_raw is not None else _load_kb_raw()
        self.objdiff_units = objdiff_units if objdiff_units is not None else _load_objdiff_units()

    def build(self, target: LiftTarget) -> ContextPack:
        kb_context = self._gather_kb_context(target)
        source_context = self._gather_source_context(target)
        ghidra_ctx = self._gather_ghidra_context(target)
        ghidra_ctx = self._enrich_ghidra_context(target, ghidra_ctx)
        hazards = self._assess_hazards(target, ghidra_ctx)
        delinked = _has_delinked_ref(target.source_path, self.objdiff_units)

        return ContextPack(
            schema_version=1,
            target={
                "addr": target.addr,
                "name": target.name,
                "decl": target.decl,
                "object": target.object_name,
                "source_path": target.source_path,
                "register_args": target.register_args,
            },
            ghidra=ghidra_ctx,
            kb_context=kb_context,
            source_context=source_context,
            hazards=hazards,
            delinked_available=delinked,
            constraints=self._build_constraints(target),
        )

    def _gather_kb_context(self, target: LiftTarget) -> dict:
        for obj in self.kb_raw["objects"]:
            if obj.get("name") != target.object_name:
                continue
            sibling_funcs = []
            for f in obj.get("functions", []):
                if f.get("addr") == target.addr:
                    continue
                sibling_funcs.append({
                    "addr": f.get("addr", ""),
                    "decl": f.get("decl", ""),
                    "ported": bool(f.get("ported")),
                })
            data_syms = []
            for d in obj.get("data", []):
                data_syms.append({
                    "addr": d.get("addr", ""),
                    "decl": d.get("decl", ""),
                })
            return {
                "sibling_functions": sibling_funcs,
                "data_symbols": data_syms,
            }
        return {"sibling_functions": [], "data_symbols": []}

    def _gather_source_context(self, target: LiftTarget) -> dict:
        source_file = ROOT / target.source_path
        if not source_file.exists():
            return {"includes": [], "neighboring_bodies": []}

        text = source_file.read_text(encoding="utf-8", errors="replace")
        lines = text.splitlines()

        includes = [l for l in lines[:80] if l.strip().startswith("#include")]

        # Extract up to 3 neighboring ported function bodies for style reference
        neighbors: list[dict] = []
        func_re = re.compile(r"^(?:static\s+)?(?:void|int|float|char|short|unsigned|bool|real_t)\b.*?\b(\w+)\s*\(")
        i = 0
        while i < len(lines) and len(neighbors) < 3:
            m = func_re.match(lines[i])
            if m:
                fname = m.group(1)
                # Find the opening brace
                j = i
                while j < len(lines) and "{" not in lines[j]:
                    j += 1
                if j < len(lines):
                    brace_start = j
                    depth = 0
                    k = brace_start
                    for k in range(brace_start, min(len(lines), brace_start + 80)):
                        depth += lines[k].count("{") - lines[k].count("}")
                        if depth == 0:
                            body = "\n".join(lines[i : k + 1])
                            if len(body) < 2000:
                                neighbors.append({"name": fname, "body": body})
                            break
                    i = k + 1
                    continue
            i += 1

        # Gather available inline/math function signatures from key headers
        available_funcs = self._gather_available_functions()

        return {
            "includes": includes,
            "neighboring_bodies": neighbors,
            "available_functions": available_funcs,
        }

    def _gather_available_functions(self) -> list[str]:
        """Extract function signatures from inlines.h and types.h."""
        sigs = []
        for header_name in ["src/inlines.h", "src/types.h"]:
            header = ROOT / header_name
            if not header.exists():
                continue
            text = header.read_text(encoding="utf-8", errors="replace")
            for line in text.splitlines():
                stripped = line.strip()
                if stripped.startswith("//") or stripped.startswith("/*"):
                    continue
                if re.match(r"^(?:static\s+)?(?:inline\s+)?(?:void|int|float|double|char|short|unsigned|bool|real_t|__int64)\b.*\w+\s*\(", stripped):
                    sig = stripped.split("{")[0].strip().rstrip(";").strip()
                    if sig and len(sig) < 200:
                        sigs.append(sig)
                elif re.match(r"^#define\s+\w+\s+\w+", stripped):
                    sigs.append(stripped)
        return sigs[:50]

    def _gather_ghidra_context(self, target: LiftTarget) -> dict:
        # Check cache first — only trust it if it has actual content
        cache_file = CONTEXT_CACHE / f"{target.name}.json"
        if cache_file.exists():
            try:
                cached = json.loads(cache_file.read_text(encoding="utf-8"))
                if cached.get("decompile_c") or cached.get("disassembly"):
                    log.info("Loaded cached Ghidra context for %s", target.name)
                    return cached
                log.info("Cached context for %s is empty, will re-fetch", target.name)
            except (json.JSONDecodeError, KeyError):
                pass

        if not self.ghidra_live:
            return {
                "decompile_c": "",
                "disassembly": "",
                "callers": [],
                "callees": [],
            }

        ghidra_ctx = self._fetch_ghidra_context(target)

        # Cache the result
        CONTEXT_CACHE.mkdir(parents=True, exist_ok=True)
        cache_file.write_text(json.dumps(ghidra_ctx, indent=2), encoding="utf-8")
        log.info("Cached Ghidra context for %s", target.name)

        return ghidra_ctx

    def _fetch_ghidra_context(self, target: LiftTarget) -> dict:
        addr = target.addr
        if not addr.startswith("0x"):
            addr = f"0x{addr}"

        mcp = GhidraMCPClient()

        decompile_c = ""
        try:
            resp = mcp.call_tool("decompile_function", {"address": addr})
            decompile_c = resp.get("content", [{}])[0].get("text", "")
            lines = decompile_c.splitlines()
            if len(lines) > 200:
                decompile_c = "\n".join(lines[:200]) + "\n// ... truncated"
        except Exception as e:
            log.warning("Failed to decompile %s: %s", target.name, e)

        disassembly = ""
        try:
            resp = mcp.call_tool("disassemble_function", {"address": addr})
            disassembly = resp.get("content", [{}])[0].get("text", "")
            lines = disassembly.splitlines()
            if len(lines) > 500:
                disassembly = "\n".join(lines[:500]) + "\n; ... truncated"
        except Exception as e:
            log.warning("Failed to disassemble %s: %s", target.name, e)

        callers: list[str] = []
        try:
            resp = mcp.call_tool("get_function_callers", {"address": addr, "limit": 20})
            text = resp.get("content", [{}])[0].get("text", "")
            for line in text.splitlines():
                m = re.search(r"(FUN_[0-9a-fA-F]+|\w+)\s+@\s+0x", line)
                if m:
                    callers.append(m.group(1))
        except Exception as e:
            log.warning("Failed to get callers for %s: %s", target.name, e)

        callees: list[str] = []
        try:
            resp = mcp.call_tool("get_function_callees", {"address": addr, "limit": 20})
            text = resp.get("content", [{}])[0].get("text", "")
            for line in text.splitlines():
                m = re.search(r"(FUN_[0-9a-fA-F]+|\w+)\s+@\s+0x", line)
                if m:
                    callees.append(m.group(1))
        except Exception as e:
            log.warning("Failed to get callees for %s: %s", target.name, e)

        return {
            "decompile_c": decompile_c,
            "disassembly": disassembly,
            "callers": callers,
            "callees": callees,
        }

    def _assess_hazards(self, target: LiftTarget, ghidra_ctx: dict) -> dict:
        decomp = ghidra_ctx.get("decompile_c", "")
        disasm = ghidra_ctx.get("disassembly", "")
        combined = decomp + "\n" + disasm

        has_fpu = bool(re.search(r"\bfloat\b|\bdouble\b|\bFLD\b|\bFSTP\b|\bFMUL\b|\bFSUB\b|\bFDIV\b", combined, re.IGNORECASE))
        has_indirect = bool(re.search(r"CALL\s+(?:dword\s+)?\[", disasm)) or bool(re.search(r"\(\*\w+\)\s*\(", decomp))
        has_seh = bool(re.search(r"__SEH_prolog|__SEH_epilog|0x1dd5c8|0x1dd601", combined, re.IGNORECASE))
        has_intrinsics = bool(INTRINSIC_RE.search(combined))

        return {
            "has_fpu": has_fpu,
            "has_indirect_calls": has_indirect,
            "has_seh": has_seh,
            "has_intrinsics": has_intrinsics,
        }

    def _build_constraints(self, target: LiftTarget) -> list[str]:
        constraints = [
            "C89 only — declare all variables at top of block scope.",
            "Do not use inline assembly (__asm, asm volatile, __asm__, etc.).",
            "Do not call MSVC intrinsics (_ftol2, _chkstk, _allmul, _aullshr, _aullrem, _aulldiv). Use C casts and operators instead.",
            "Do not invent function or variable names not present in the context. Use field_XX or unknown_XX if unsure.",
            "Do not add #include or #pragma directives.",
            "Preserve control flow shape from the decompiler output.",
            "Preserve side-effect ordering.",
            "Use (int)float_expr for float-to-int conversion, not _ftol2.",
            "Use (int64_t)a * b for 64-bit multiply, not _allmul.",
        ]
        if target.has_reg_args:
            constraints.append(
                f"This function uses register arguments: {target.register_args}. "
                "Do not modify register argument handling — the build system generates thunks."
            )
        return constraints

    # ------------------------------------------------------------------
    # Callee lookup (lazy, shared across enrichment phases)
    # ------------------------------------------------------------------

    def _ensure_callee_lookup(self) -> dict:
        """Build {name: entry, addr: entry} for ALL kb.json functions."""
        cached = getattr(self, "_lazy_callee_lookup", None)
        if cached is not None:
            return cached
        by_name: dict = {}
        by_addr: dict = {}
        for obj in self.kb_raw.get("objects", []):
            for f in obj.get("functions", []):
                addr = f.get("addr", "").lower()
                decl = f.get("decl", "")
                name = _decl_name(decl)
                has_reg = "@" in decl
                entry = {
                    "addr": addr,
                    "name": name,
                    "decl": decl,
                    "ported": bool(f.get("ported")),
                    "has_reg_args": has_reg,
                    "param_count": _decl_param_count(decl),
                    "object": obj.get("name", ""),
                }
                if name:
                    by_name[name] = entry
                if addr:
                    by_addr[addr] = entry
                    stripped = addr.lstrip("0x")
                    if stripped != addr:
                        by_addr[stripped] = entry
        self._lazy_callee_lookup = {"by_name": by_name, "by_addr": by_addr}
        return self._lazy_callee_lookup

    # ------------------------------------------------------------------
    # Phase 1: Callee signature table
    # ------------------------------------------------------------------

    def _gather_callee_details(self, target: LiftTarget, ghidra_ctx: dict) -> dict:
        """For each callee, look up its kb.json signature and ported status."""
        lookup = self._ensure_callee_lookup()
        details: dict = {}
        for name in ghidra_ctx.get("callees", []):
            entry = lookup["by_name"].get(name)
            if not entry:
                m = re.match(r"FUN_00([0-9a-fA-F]+)$", name)
                if m:
                    entry = lookup["by_addr"].get(m.group(1))
            if entry:
                details[name] = entry
            else:
                details[name] = {"not_in_kb": True, "name": name}
        return details

    # ------------------------------------------------------------------
    # Phase 2: Call-site audit
    # ------------------------------------------------------------------

    def _audit_call_sites(self, target: LiftTarget, ghidra_ctx: dict) -> list:
        """Parse every CALL in disassembly, collect PUSH operands, cross-ref
        with callee kb.json signatures. Flag FPU-argument hazards only.

        Note: ARG_COUNT and REGISTER_ALIASING hazards have been removed.
        Push-count comparison is unreliable because register-arg callees
        (fastcall/thiscall @ecx/@edx) pass args in registers without pushes,
        and the backward scan cannot distinguish callee-saved prologue saves
        (EBX/ESI/EDI) from genuine argument pushes.
        """
        disasm = ghidra_ctx.get("disassembly", "")
        if not disasm:
            return []
        disasm_text = _unwrap_mcp_text(disasm)
        lookup = self._ensure_callee_lookup()
        audit: list = []

        instructions: list[tuple[int, str, str, str]] = []
        for lineno, line in enumerate(disasm_text.splitlines(), 1):
            m = _DISASM_LINE_RE.match(line.strip())
            if m:
                instructions.append((lineno, m.group(1), m.group(2), m.group(3)))

        if not instructions:
            return []

        for i, (lineno, addr, mnemonic, operands) in enumerate(instructions):
            if mnemonic != "CALL":
                continue

            pushes: list[dict] = []
            for j in range(i - 1, max(i - 20, -1), -1):
                _, pa, pm, po = instructions[j]
                if pm == "PUSH":
                    pushes.insert(0, {"addr": pa, "operands": po})
                elif pm == "FSTP" and "ESP" in po.upper():
                    pushes.insert(0, {"addr": pa, "operands": po, "is_fpu": True})
                elif pm in _CTRL_FLOW_MNEMONICS:
                    break
                elif pm == "POP" or (pm == "ADD" and "ESP" in po.upper()):
                    break
                elif pm == "FSTP":
                    pass
                elif pm not in _PUSH_SCAN_SKIP:
                    break

            if not pushes:
                continue

            callee_str = operands.strip()
            callee_info: dict = {}
            callee_name = ""
            call_m = re.match(r'(?:dword\s+ptr\s+\[)?(0x[0-9a-fA-F]+)', callee_str)
            if call_m:
                addr_key = call_m.group(1).lower().lstrip("0x")
                callee_info = lookup["by_addr"].get(addr_key, {})
                callee_name = callee_info.get("name", "")

            hazards: list[str] = []
            for p in pushes:
                if p.get("is_fpu"):
                    hazards.append(
                        "FPU_ARG: FSTP [ESP] replaces dummy stack slot "
                        "— Ghidra likely shows wrong argument"
                    )

            # Stack-cleanup cross-check (§7): scan forward for ADD ESP,N after CALL
            cleanup_args: int | None = None
            for fwd in range(i + 1, min(i + 6, len(instructions))):
                _, fa, fm, fo = instructions[fwd]
                if fm == "ADD" and "ESP" in fo.upper():
                    m2 = re.match(r'ESP\s*,\s*(0x[0-9a-fA-F]+|\d+)', fo, re.I)
                    if m2:
                        val = m2.group(1)
                        cleanup_bytes = int(val, 16) if val.startswith(("0x","0X")) else int(val)
                        cleanup_args = cleanup_bytes // 4
                    break
                elif fm in _CTRL_FLOW_MNEMONICS:
                    break

            if callee_info and cleanup_args is not None:
                decl = callee_info.get("decl", "")
                is_stdcall = "__stdcall" in decl or "WINAPI" in decl
                if not is_stdcall:
                    reg_arg_count = len(re.findall(r'@<\w+>', decl))
                    param_m = re.search(r'\(([^)]*)\)', decl)
                    if param_m:
                        praw = param_m.group(1).strip()
                        total_params = 0 if (not praw or praw == "void") else len(praw.split(","))
                        declared_stack = max(0, total_params - reg_arg_count)
                        if cleanup_args != declared_stack:
                            # §7 special: 0-arg getter with prior pushes
                            if declared_stack == 0 and reg_arg_count == 0 and len(pushes) > 0:
                                hazards.append(
                                    f"§7_GETTER_SWALLOWED: {callee_name or callee_str} declares 0 args "
                                    f"but cleanup={cleanup_args}; {len(pushes)} push(es) before it "
                                    f"likely belong to the NEXT call's arg list"
                                )
                            else:
                                hazards.append(
                                    f"ARG_COUNT: cleanup={cleanup_args} stack args, "
                                    f"decl={declared_stack} stack args "
                                    f"(total={total_params} params, {reg_arg_count} reg_args) — "
                                    f"verify against disasm push count"
                                )

            audit.append({
                "call_addr": addr,
                "callee_addr": callee_str,
                "callee_kb": callee_name,
                "callee_decl": callee_info.get("decl", ""),
                "callee_ported": callee_info.get("ported", False),
                "callee_has_reg_args": callee_info.get("has_reg_args", False),
                "cleanup_args": cleanup_args,
                "hazards": hazards,
            })

        return audit

    # ------------------------------------------------------------------
    # Phase 3: Struct field offset extraction
    # ------------------------------------------------------------------

    def _extract_struct_offsets(self, ghidra_ctx: dict) -> dict:
        """Parse MOV [reg+offset] patterns in disassembly to build verified
        field-offset tables per struct-pointer register.

        Only callee-saved registers (ESI, EDI, EBX) are included — these
        typically hold stable struct pointers across a function. Scratch
        registers (EAX/ECX/EDX) and the frame pointer (EBP) are excluded
        because their [reg+off] accesses conflate unrelated objects or are
        stack frame slots, not heap struct fields.

        Offsets with the 32-bit sign bit set (i.e. negative displacements
        mis-captured as large unsigned values) or exceeding 0x10000 are
        also discarded as nonsensical struct field indices.
        """
        # Registers that plausibly hold a stable struct pointer for an entire
        # function body. Scratch regs (EAX, ECX, EDX) and the frame pointer
        # (EBP) are excluded.
        _STRUCT_REGS = {"ESI", "EDI", "EBX"}
        _MAX_STRUCT_OFFSET = 0x10000

        disasm = ghidra_ctx.get("disassembly", "")
        if not disasm:
            return {}
        disasm_text = _unwrap_mcp_text(disasm)
        field_map: dict = {}

        for line in disasm_text.splitlines():
            for m in _STRUCT_ACCESS_RE.finditer(line):
                reg = m.group(2).upper()
                if reg not in _STRUCT_REGS:
                    continue
                offset = m.group(3).lower()
                val = int(offset, 16)
                # Reject negative displacements (32-bit sign bit set) and
                # unreasonably large offsets that cannot be struct fields.
                if val & 0x80000000 or val > _MAX_STRUCT_OFFSET:
                    continue
                field_map.setdefault(reg, {}).setdefault(offset, 0)
                field_map[reg][offset] += 1

        result: dict = {}
        for reg, offsets in sorted(field_map.items()):
            verified = sorted(
                [o for o, c in offsets.items() if c >= 2],
                key=lambda x: int(x, 16),
            )
            if verified:
                result[reg] = verified

        return result

    # ------------------------------------------------------------------
    # Phase 4: Pre-lift buffer alias annotation
    # ------------------------------------------------------------------

    def _annotate_buffer_aliases(self, ghidra_ctx: dict) -> dict:
        """Run buffer_alias_detector on the decompile and embed findings."""
        decompile = ghidra_ctx.get("decompile_c", "")
        if not decompile:
            return {}
        text_to_analyze = _unwrap_mcp_text(decompile)
        if not text_to_analyze.strip():
            return {}

        try:
            import importlib.util
            spec = importlib.util.spec_from_file_location(
                "buffer_alias_detector",
                str(ROOT / "tools" / "lift" / "buffer_alias_detector.py"),
            )
            bad = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(bad)
            hits, annotated = bad.analyze(text_to_analyze)

            if not hits:
                return {"total_hits": 0, "high_risk": 0, "low_risk": 0, "details": []}

            details: list[dict] = []
            for h in hits[:20]:
                details.append({
                    "line": h.line_no,
                    "local": h.local_name,
                    "buffer": h.buffer.name,
                    "offset_into_buffer": f"0x{h.buffer.base - h.local_offset:x}",
                    "high_risk": h.post_call,
                })

            result: dict = {
                "total_hits": len(hits),
                "high_risk": sum(1 for h in hits if h.post_call),
                "low_risk": sum(1 for h in hits if not h.post_call),
                "details": details,
            }
            # Store annotated decompile for agent reference
            if len(annotated) < 20000:
                result["annotated_decompile"] = annotated
            return result

        except Exception as e:
            log.warning("Buffer alias detection failed: %s", e)
            return {"error": str(e)}

    # ------------------------------------------------------------------
    # Phase 5: Buffer size verification
    # ------------------------------------------------------------------

    def _verify_buffer_sizes(self, ghidra_ctx: dict) -> list:
        """Flag local buffers that are smaller than what callees write."""
        decompile = ghidra_ctx.get("decompile_c", "")
        callee_details = ghidra_ctx.get("callee_details", {})
        if not decompile or not KNOWN_BUFFER_SIZES:
            return []
        decomp_text = _unwrap_mcp_text(decompile)
        if not decomp_text:
            return []

        buffers: dict[str, int] = {}
        for m in _BUF_DECL_RE.finditer(decomp_text):
            buf_name = m.group(1)
            size_str = m.group(2)
            size = int(size_str, 16) if size_str.startswith("0x") else int(size_str)
            if size >= 4:
                buffers[buf_name] = size

        warnings: list[str] = []
        for callee_name, (required_size, note) in KNOWN_BUFFER_SIZES.items():
            call_re = re.compile(re.escape(callee_name) + r"\s*\(([^)]*)\)")
            for call_m in call_re.finditer(decomp_text):
                args = call_m.group(1)
                first_arg = args.split(",")[0].strip()
                if first_arg in buffers:
                    declared = buffers[first_arg]
                    if declared < required_size:
                        warnings.append(
                            f"BUFFER_UNDERSIZED: '{first_arg}' is {declared} bytes "
                            f"but {callee_name} writes {required_size} bytes ({note})"
                        )

        return warnings

    # ------------------------------------------------------------------
    # Orchestrator
    # ------------------------------------------------------------------

    def _enrich_ghidra_context(self, target: LiftTarget, ghidra_ctx: dict) -> dict:
        """Run all enrichment phases and embed verified metadata into the
        Ghidra context dict.  All phases operate on already-fetched data;
        no additional MCP calls are made."""
        if not ghidra_ctx.get("disassembly") and not ghidra_ctx.get("decompile_c"):
            return ghidra_ctx

        ghidra_ctx["callee_details"] = self._gather_callee_details(target, ghidra_ctx)
        ghidra_ctx["call_site_audit"] = self._audit_call_sites(target, ghidra_ctx)
        ghidra_ctx["struct_offsets"] = self._extract_struct_offsets(ghidra_ctx)
        ghidra_ctx["buffer_alias"] = self._annotate_buffer_aliases(ghidra_ctx)
        ghidra_ctx["buffer_warnings"] = self._verify_buffer_sizes(ghidra_ctx)

        return ghidra_ctx


# ---------------------------------------------------------------------------
# Ghidra MCP Client (SSE + JSON-RPC, same protocol as check_ghidra_mcp.py)
# ---------------------------------------------------------------------------

class GhidraMCPClient:
    """Lightweight MCP client that keeps the SSE stream alive for responses."""

    ENDPOINT = "http://127.0.0.1:8090/sse"

    def __init__(self, timeout: float = 30.0):
        self.timeout = timeout
        self._session_url: Optional[str] = None
        self._sse_stream = None
        self._next_id = 1

    def _connect(self):
        import urllib.request
        import urllib.parse

        req = urllib.request.Request(self.ENDPOINT, method="GET")
        self._sse_stream = urllib.request.urlopen(req, timeout=self.timeout)

        deadline = time.monotonic() + self.timeout
        while True:
            if time.monotonic() >= deadline:
                raise RuntimeError("Timed out waiting for SSE bootstrap")
            raw = self._sse_stream.readline()
            if not raw:
                raise RuntimeError("SSE stream closed")
            line = raw.decode("utf-8", errors="replace").strip()
            if line.startswith("data:"):
                path = line[5:].strip()
                if path.startswith("/messages/?session_id="):
                    parsed = urllib.parse.urlparse(self.ENDPOINT)
                    self._session_url = f"{parsed.scheme}://{parsed.netloc}{path}"
                    return
                raise RuntimeError(f"Unexpected SSE payload: {path!r}")

    def _ensure_session(self):
        if self._session_url and self._sse_stream:
            return
        self._connect()
        self._post({"jsonrpc": "2.0", "id": 0, "method": "initialize",
                     "params": {"protocolVersion": "2024-11-05",
                                "capabilities": {},
                                "clientInfo": {"name": "auto-lift", "version": "0.1.0"}}})
        self._read_response(0)
        self._post({"jsonrpc": "2.0", "method": "notifications/initialized", "params": {}})

    def _reset_session(self):
        self._session_url = None
        if self._sse_stream is not None:
            try:
                self._sse_stream.close()
            except Exception:
                pass
        self._sse_stream = None

    def _post(self, payload: dict):
        import urllib.request
        data = json.dumps(payload).encode("utf-8")
        req = urllib.request.Request(
            self._session_url, data=data,
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        with urllib.request.urlopen(req, timeout=self.timeout):
            pass

    def _read_response(self, rpc_id: int) -> dict:
        deadline = time.monotonic() + self.timeout
        while time.monotonic() < deadline:
            raw = self._sse_stream.readline()
            if not raw:
                break
            line = raw.decode("utf-8", errors="replace").strip()
            if not line.startswith("data:"):
                continue
            payload = line[5:].strip()
            if not payload.startswith("{"):
                continue
            try:
                msg = json.loads(payload)
            except json.JSONDecodeError:
                continue
            if msg.get("id") == rpc_id:
                if "error" in msg:
                    raise RuntimeError(f"RPC error: {msg['error']}")
                return msg.get("result", {})
        raise RuntimeError(f"Timed out waiting for response to request {rpc_id}")

    def call_tool(self, tool_name: str, arguments: dict) -> dict:
        for attempt in (1, 2):
            try:
                self._ensure_session()
                rpc_id = self._next_id
                self._next_id += 1
                self._post({
                    "jsonrpc": "2.0",
                    "id": rpc_id,
                    "method": "tools/call",
                    "params": {"name": tool_name, "arguments": arguments},
                })
                return self._read_response(rpc_id)
            except Exception:
                if attempt == 2:
                    raise
                self._reset_session()
        raise RuntimeError("unreachable")


# ---------------------------------------------------------------------------
# Legacy review/promote (reads artifacts from old SDK-based generate runs)
# ---------------------------------------------------------------------------

def cmd_review(args: argparse.Namespace):
    if not ARTIFACT_ROOT.exists():
        print("No artifacts found.")
        return

    batch_dirs = sorted(ARTIFACT_ROOT.glob("batch_*"), reverse=True)
    if not batch_dirs:
        print("No batch runs found.")
        return

    for bd in batch_dirs[:args.limit]:
        summary_path = bd / "summary.json"
        if not summary_path.exists():
            continue
        summary = json.loads(summary_path.read_text(encoding="utf-8"))
        print(f"\n{'=' * 60}")
        print(f"Batch: {summary.get('batch_id', bd.name)}")
        print(f"Provider: {summary.get('llm_provider', '?')}")
        print(f"Results: {summary.get('auto_accept', 0)} accept, "
              f"{summary.get('needs_review', 0)} review, "
              f"{summary.get('reject', 0)} reject "
              f"/ {summary.get('candidates_attempted', 0)} total")
        print(f"Tokens: {summary.get('total_input_tokens', 0)} in, "
              f"{summary.get('total_output_tokens', 0)} out")

        for r in summary.get("results", []):
            icon = {"auto_accept": "+", "needs_review": "?", "reject": "x"}.get(r.get("state", ""), " ")
            vc71 = f"{r['vc71_match_pct']:.0f}%" if r.get("vc71_match_pct") is not None else "n/a"
            print(f"  [{icon}] {r.get('name', '?'):30s}  {r.get('state', '?'):15s}  "
                  f"vc71={vc71}  attempts={r.get('attempts', 0)}")


def cmd_promote(args: argparse.Namespace):
    if not ARTIFACT_ROOT.exists():
        print("No artifacts found.")
        return

    batch_dirs = sorted(ARTIFACT_ROOT.glob("batch_*"), reverse=True)
    if args.batch:
        batch_dir = ARTIFACT_ROOT / args.batch
    elif batch_dirs:
        batch_dir = batch_dirs[0]
    else:
        print("No batch runs found.")
        return

    summary_path = batch_dir / "summary.json"
    if not summary_path.exists():
        print(f"No summary in {batch_dir}")
        return

    summary = json.loads(summary_path.read_text(encoding="utf-8"))
    accepted = [r for r in summary.get("results", []) if r.get("state") == AUTO_ACCEPT]

    if not accepted:
        print("No auto_accept results to promote.")
        return

    print(f"Found {len(accepted)} auto_accept result(s):")
    for r in accepted:
        name = r.get("name", "?")
        func_dir = batch_dir / name
        final = func_dir / "candidate_final.c"
        if final.exists():
            print(f"  {name} -> {final}")
        else:
            print(f"  {name} -> MISSING candidate_final.c")

    if not args.apply:
        print("\nDry run. Use --apply to apply these candidates via maintain.py.")
        return

    for r in accepted:
        name = r.get("name", "?")
        func_dir = batch_dir / name
        final = func_dir / "candidate_final.c"
        if not final.exists():
            continue

        ctx_path = func_dir / "context.json"
        if not ctx_path.exists():
            continue
        ctx = json.loads(ctx_path.read_text(encoding="utf-8"))
        source_path = ctx["target"]["source_path"]

        maintain_cmd = [
            sys.executable,
            str(ROOT / "tools" / "analysis" / "maintain.py"),
            "--update-from", str(final),
            source_path,
        ]
        proc = subprocess.run(maintain_cmd, cwd=str(ROOT), capture_output=True, text=True)
        if proc.returncode == 0:
            print(f"  Applied {name} to {source_path}")
        else:
            print(f"  FAILED {name}: {(proc.stderr or proc.stdout)[:200]}")


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(
        description="Auto-Lift Target Selector & Context Cache",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument("-v", "--verbose", action="store_true", help="Verbose logging")
    ap.add_argument("-q", "--quiet", action="store_true", help="Suppress decorative output")
    sub = ap.add_subparsers(dest="mode")

    # -- score --
    p_score = sub.add_parser("score", help="Rank unported functions by liftability")
    p_score.add_argument("--object", default="", help="Filter to one object")
    p_score.add_argument("--min-score", type=int, default=0, help="Minimum score")
    p_score.add_argument("--limit", type=int, default=30, help="Max results")
    p_score.add_argument("--json", action="store_true", help="JSON output")

    # -- select --
    p_select = sub.add_parser("select", help="Combine frontier priority with liftability")
    p_select.add_argument("--object", default="", help="Filter to one object")
    p_select.add_argument("--min-score", type=int, default=0, help="Minimum liftability score")
    p_select.add_argument("--limit", type=int, default=20, help="Max results")
    p_select.add_argument("--frontier-limit", type=int, default=50, help="Frontier rows to consider")
    p_select.add_argument("--auto-threshold", type=int, default=30, help="Liftability score needed for automation lanes")
    p_select.add_argument("--json", action="store_true", help="JSON output")

    # -- cache-context --
    p_cache = sub.add_parser("cache-context", help="Build Ghidra context packs (requires MCP)")
    p_cache.add_argument("--target", default="", help="Specific function address or name")
    p_cache.add_argument("--batch", type=int, default=5, help="Number of top targets")
    p_cache.add_argument("--batch-frontier", type=int, default=0,
                         help="Convenience: set --batch=N and --frontier-limit=max(N*4,300)")
    p_cache.add_argument("--object", default="", help="Filter to one object")
    p_cache.add_argument("--min-score", type=int, default=30, help="Minimum liftability score")
    p_cache.add_argument("--frontier-limit", type=int, default=50, help="Frontier rows to consider")
    p_cache.add_argument("--force", action="store_true", help="Re-cache even if pack already exists")
    p_cache.add_argument("--stats", action="store_true", help="Print cache coverage and exit")

    # -- review (legacy) --
    p_review = sub.add_parser("review", help="Show legacy batch results")
    p_review.add_argument("--limit", type=int, default=5, help="Max batches to show")

    # -- promote (legacy) --
    p_promote = sub.add_parser("promote", help="Apply legacy batch results")
    p_promote.add_argument("--batch", default="", help="Specific batch directory name")
    p_promote.add_argument("--apply", action="store_true", help="Actually apply (default: dry run)")

    args = ap.parse_args()
    if args.mode is None:
        args.mode = "select"
        args.object = ""
        args.min_score = 0
        args.limit = 20
        args.frontier_limit = 50
        args.auto_threshold = 30
        args.json = False
    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.INFO,
        format="%(levelname)s: %(message)s",
    )

    if args.mode == "score":
        cmd_score(args)
    elif args.mode == "select":
        cmd_select(args)
    elif args.mode == "cache-context":
        cmd_cache_context(args)
    elif args.mode == "review":
        cmd_review(args)
    elif args.mode == "promote":
        cmd_promote(args)


def cmd_score(args: argparse.Namespace):
    scorer = LiftabilityScorer()
    targets = scorer.score_all(object_filter=args.object, min_score=args.min_score)
    targets = targets[: args.limit]

    if args.json:
        out = [asdict(t) for t in targets]
        kw = {"separators": (",", ":")} if args.quiet else {"indent": 2}
        print(json.dumps(out, **kw))
        return

    if not args.quiet:
        print(f"{'Score':>5}  {'Address':>10}  {'Object':<35}  {'Name':<35}  {'Factors'}")
        print("-" * 120)
    for t in targets:
        factors = ", ".join(f"{k}=+{v}" for k, v in t.score_details.items())
        print(f"{t.score:>5}  {t.addr:>10}  {t.object_name:<35}  {t.name:<35}  {factors}")

    total_unported = sum(
        1 for obj in scorer.kb_raw["objects"]
        for f in obj.get("functions", [])
        if not f.get("ported")
    )
    print(f"\n{len(targets)} scored candidates of {total_unported} unported")


def cmd_select(args: argparse.Namespace):
    scorer = LiftabilityScorer()
    targets = scorer.score_all(object_filter=args.object, min_score=args.min_score)
    selected = _select_targets(
        targets,
        frontier_limit=args.frontier_limit,
        auto_threshold=args.auto_threshold,
    )[: args.limit]

    if args.json:
        out = []
        for item in selected:
            row = asdict(item)
            row["target"] = asdict(item.target)
            out.append(row)
        kw = {"separators": (",", ":")} if args.quiet else {"indent": 2}
        print(json.dumps(out, **kw))
        return

    if not args.quiet:
        print(f"{'Total':>5}  {'Lift':>4}  {'Fr':>3}  {'Lane':<13}  {'Miz':>3}  {'Address':>10}  {'Object':<35}  {'Name':<35}  {'Reasons'}")
        print("-" * 146)
    for item in selected:
        target = item.target
        reasons = ", ".join(item.reasons)
        miz = "Y" if _find_mizuchi_result(target.name) else "-"
        has_failure = (FAILURES_DIR / f"{target.name}.json").exists()
        skip_marker = " [skip:prior_fail]" if has_failure else ""
        print(
            f"{item.total_score:>5}  {item.liftability_score:>4}  {item.frontier_score:>3}  "
            f"{item.lane:<13}  {miz:>3}  {target.addr:>10}  {target.object_name:<35}  {target.name:<35}  {reasons}{skip_marker}"
        )

    if not args.quiet:
        print("\nLane guide:")
        print("  auto-lift     -> high confidence; use /lift <target>")
        print("  cache-context -> cache Ghidra context, then /lift <target>")
        print("  manual-lift   -> use /lift; strategically useful but needs more care")
        print("  defer         -> low priority for now")

        rejected_entries = []
        if FAILURES_DIR.exists():
            for f in sorted(FAILURES_DIR.glob("*.json")):
                try:
                    rec = json.loads(f.read_text())
                    branch = rec.get("branch", "")
                    if not branch:
                        continue
                    result = subprocess.run(
                        ["git", "branch", "--list", branch],
                        capture_output=True, text=True, cwd=ROOT,
                    )
                    if branch not in result.stdout:
                        continue
                    stage = ""
                    for attempt in rec.get("attempts", []):
                        stage = attempt.get("failure_stage", stage)
                    rejected_entries.append((rec.get("target", f.stem), branch, stage,
                                             rec.get("addr", ""), rec.get("object", "")))
                except Exception:
                    continue
        if rejected_entries:
            print("\nRejected branches (prior work preserved — retry with /lift <target>):")
            print(f"  {'Target':<35}  {'Stage':<20}  {'Branch'}")
            print("  " + "-" * 80)
            for name, branch, stage, addr, obj in rejected_entries:
                print(f"  {name:<35}  {stage:<20}  {branch}")


def _find_mizuchi_result(func_name: str) -> str | None:
    """Return the best generated C code from the latest successful mizuchi run, or None."""
    run_files = sorted(MIZUCHI_DIR.glob("run-results-*.json"), reverse=True)
    for run_file in run_files:
        try:
            data = json.loads(run_file.read_text())
        except Exception:
            continue
        for result in data.get("results", []):
            if result.get("functionName") != func_name:
                continue
            if not result.get("success"):
                continue
            # Find the attempt with the best (lowest) diff count that has code
            best_code = ""
            best_diff = float("inf")
            for attempt in result.get("attempts", []):
                for plugin in attempt.get("pluginResults", []):
                    if plugin.get("pluginId") != "claude-runner":
                        continue
                    code = plugin.get("data", {}).get("generatedCode", "")
                    diff = plugin.get("data", {}).get("diffCount", float("inf"))
                    if diff is None:
                        diff = float("inf")
                    if code and diff < best_diff:
                        best_diff = diff
                        best_code = code
            if best_code:
                return best_code
    return None


def cmd_cache_context(args: argparse.Namespace):
    # --stats: print coverage report and exit
    if getattr(args, 'stats', False):
        cached = len([f for f in CONTEXT_CACHE.iterdir() if f.suffix == '.json']) if CONTEXT_CACHE.exists() else 0
        scorer = LiftabilityScorer()
        all_targets = scorer.score_all()
        # LiftTarget only includes unported functions; count all kb.json entries
        import json as _json
        with open(ROOT / 'kb.json', encoding='utf-8') as _f:
            _kb = _json.load(_f)
        def _count_kb_ported(node):
            c = 0
            if isinstance(node, dict):
                if 'addr' in node and 'decl' in node:
                    c += 1
                for v in node.values():
                    c += _count_kb_ported(v)
            elif isinstance(node, list):
                for item in node:
                    c += _count_kb_ported(item)
            return c
        total_unported = len(all_targets)
        total_all = len(scorer.score_all())
        print(f"Context cache coverage: {cached} cached / {total_unported} unported / {total_all} total")
        pct = 100.0 * cached / total_unported if total_unported else 0.0
        print(f"  Coverage: {pct:.1f}% of unported functions")
        return

    # --batch-frontier N: convenience for batch+frontier-limit
    if getattr(args, 'batch_frontier', 0):
        args.batch = args.batch_frontier
        args.frontier_limit = max(args.batch_frontier * 4, 300)

    scorer = LiftabilityScorer()
    if args.target:
        targets = scorer.score_all()
        targets = [t for t in targets if t.addr == args.target or t.name == args.target]
    else:
        scored = scorer.score_all(object_filter=args.object, min_score=args.min_score)
        selected = _select_targets(scored, frontier_limit=args.frontier_limit, auto_threshold=args.min_score)
        targets = [item.target for item in selected if item.lane != "defer"][: args.batch]

    if not targets:
        print("No targets found.")
        return

    # Skip already-cached unless --force
    if not getattr(args, 'force', False):
        CONTEXT_CACHE.mkdir(parents=True, exist_ok=True)
        targets = [t for t in targets if not (CONTEXT_CACHE / f"{t.name}.json").exists()]
        if not targets:
            print("All targets already cached. Use --force to re-cache.")
            return

    # Check Ghidra MCP
    check_cmd = [sys.executable, str(ROOT / "tools" / "audit" / "check_ghidra_mcp.py")]
    proc = subprocess.run(check_cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        print("ERROR: Ghidra MCP not available.")
        print("You might have forgotten to start tools/shell/mcp-servers.sh or ghidra may not be running?")
        sys.exit(1)

    builder = ContextPackBuilder(ghidra_live=True, kb_raw=scorer.kb_raw, objdiff_units=scorer.objdiff_units)
    CONTEXT_CACHE.mkdir(parents=True, exist_ok=True)

    retrieval_available = _check_retrieval_index()

    cached_count = 0
    failed_count = 0
    for t in targets:
        print(f"Caching context for {t.name} ({t.addr})...")
        cache_file = CONTEXT_CACHE / f"{t.name}.json"
        try:
            pack = builder.build(t)
        except Exception as exc:
            print(f"  ERROR: {exc} — skipping")
            failed_count += 1
            continue
        ghidra_ctx = pack.ghidra
        has_decomp = bool(ghidra_ctx.get("decompile_c"))
        has_disasm = bool(ghidra_ctx.get("disassembly"))
        print(f"  decompile={'yes' if has_decomp else 'NO'}  disasm={'yes' if has_disasm else 'NO'}  "
              f"callers={len(ghidra_ctx.get('callers', []))}  callees={len(ghidra_ctx.get('callees', []))}")

        # Enrichment stats
        callee_detail_count = len(ghidra_ctx.get("callee_details", {}))
        audit_count = len(ghidra_ctx.get("call_site_audit", []))
        audit_hazards = sum(
            len(cs.get("hazards", [])) for cs in ghidra_ctx.get("call_site_audit", [])
        )
        buf_alias = ghidra_ctx.get("buffer_alias", {})
        buf_alias_hits = buf_alias.get("total_hits", 0)
        buf_alias_high = buf_alias.get("high_risk", 0)
        buf_warnings = len(ghidra_ctx.get("buffer_warnings", []))
        struct_regs = len(ghidra_ctx.get("struct_offsets", {}))

        if callee_detail_count or audit_count:
            parts = []
            if callee_detail_count:
                parts.append(f"callee_sigs={callee_detail_count}")
            if audit_count:
                parts.append(f"call_sites={audit_count}")
                if audit_hazards:
                    parts.append(f"hazards={audit_hazards}")
            if buf_alias_hits:
                parts.append(f"buf_alias={buf_alias_hits}({buf_alias_high} high)")
            if buf_warnings:
                parts.append(f"buf_warns={buf_warnings}")
            if struct_regs:
                parts.append(f"struct_regs={struct_regs}")
            if parts:
                print(f"  enrichment: {'  '.join(parts)}")

        if retrieval_available and has_decomp:
            neighbors = _query_retrieval_neighbors(ghidra_ctx["decompile_c"])
            if neighbors:
                ghidra_ctx["similar_neighbors"] = neighbors
                print(f"  retrieval: {len(neighbors)} similar ported function(s) injected")
            else:
                print(f"  retrieval: no similar neighbors above threshold")
        elif not retrieval_available:
            pass  # silently skip — index not built yet

        mizuchi_code = _find_mizuchi_result(t.name)
        if mizuchi_code:
            ghidra_ctx["mizuchi_result"] = mizuchi_code
            print(f"  mizuchi: successful result injected ({len(mizuchi_code)} chars)")
        else:
            print(f"  mizuchi: no successful result found")

        cache_file.write_text(json.dumps(ghidra_ctx, indent=2), encoding="utf-8")
        cached_count += 1

    print(f"\nCached {cached_count}/{len(targets)} context packs to {CONTEXT_CACHE}"
          + (f"  ({failed_count} failed)" if failed_count else ""))
    print(f"Coverage: {len(list(CONTEXT_CACHE.glob('*.json')))} total cached")


if __name__ == "__main__":
    main()
