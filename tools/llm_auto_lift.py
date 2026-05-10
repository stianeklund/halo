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

MSVC_INTRINSIC_ADDRS = {
    "1d90e0", "1d9068", "1dd5c8", "1dd601",
    "1dd620", "1dd660", "1dd680", "1dd770",
}
INTRINSIC_RE = re.compile(
    r"0x(?:" + "|".join(MSVC_INTRINSIC_ADDRS) + r")\b", re.IGNORECASE
)

DISQUALIFIED_OBJECTS = {"<xdk_stubs>", "<common>"}

# States (used by legacy review/promote)
AUTO_ACCEPT = "auto_accept"
NEEDS_REVIEW = "needs_review"
REJECT = "reject"


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


def _has_delinked_ref(source_path: str, units: dict[str, dict]) -> bool:
    unit = _find_objdiff_unit(source_path, units)
    if not unit:
        return False
    base = unit.get("base_path", "")
    return (ROOT / base).exists() if base else False


_PDB_PROPOSALS_CACHE: Optional[set[str]] = None
_PURE_LEAF_CACHE: Optional[set[str]] = None


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


def _load_pure_leaf_addrs() -> set[str]:
    """Return the set of addresses verified as pure leaves by unicorn_diff.

    Populated as a side-effect of real `tools/equivalence/unicorn_diff.py`
    runs (see `_record_leaf_classification` there). Empty until at least
    one Unicorn diff has executed; cheap to load (single small JSON read).
    """
    global _PURE_LEAF_CACHE
    if _PURE_LEAF_CACHE is not None:
        return _PURE_LEAF_CACHE
    cache_path = ROOT / "tools" / "equivalence" / "leaf_cache.json"
    addrs: set[str] = set()
    if cache_path.exists():
        try:
            data = json.loads(cache_path.read_text(encoding="utf-8"))
            for k, v in data.items():
                if v == "leaf":
                    addrs.add(k.lower())
        except (json.JSONDecodeError, OSError):
            pass
    _PURE_LEAF_CACHE = addrs
    return addrs


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

                score = 0
                details: dict[str, int] = {}

                # Delinked reference
                if _has_delinked_ref(source_path, self.objdiff_units):
                    score += 20
                    details["delinked_ref"] = 20

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

                # Verified pure leaf — unicorn_diff already proved this
                # function has no external relocations. The behavioral
                # differential lane (`/verify equivalence`) is available as
                # a strong post-lift gate.
                if addr.lower() in _load_pure_leaf_addrs():
                    score += 5
                    details["eq_pure_leaf"] = 5

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

    def __init__(self, *, ghidra_live: bool = False):
        self.ghidra_live = ghidra_live
        self.kb_raw = _load_kb_raw()
        self.objdiff_units = _load_objdiff_units()

    def build(self, target: LiftTarget) -> ContextPack:
        kb_context = self._gather_kb_context(target)
        source_context = self._gather_source_context(target)
        ghidra_ctx = self._gather_ghidra_context(target)
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
        self._post({"jsonrpc": "2.0", "method": "notifications/initialized", "params": {}})
        self._read_response(0)

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
    p_cache.add_argument("--object", default="", help="Filter to one object")
    p_cache.add_argument("--min-score", type=int, default=30, help="Minimum liftability score")
    p_cache.add_argument("--frontier-limit", type=int, default=50, help="Frontier rows to consider")

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
        print(json.dumps(out, indent=2))
        return

    print(f"{'Score':>5}  {'Address':>10}  {'Object':<35}  {'Name':<35}  {'Factors'}")
    print("-" * 120)
    for t in targets:
        factors = ", ".join(f"{k}=+{v}" for k, v in t.score_details.items())
        print(f"{t.score:>5}  {t.addr:>10}  {t.object_name:<35}  {t.name:<35}  {factors}")

    kb_raw = _load_kb_raw()
    total_unported = sum(
        1 for obj in kb_raw["objects"]
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
        print(json.dumps(out, indent=2))
        return

    print(f"{'Total':>5}  {'Lift':>4}  {'Fr':>3}  {'Lane':<13}  {'Address':>10}  {'Object':<35}  {'Name':<35}  {'Reasons'}")
    print("-" * 140)
    for item in selected:
        target = item.target
        reasons = ", ".join(item.reasons)
        print(
            f"{item.total_score:>5}  {item.liftability_score:>4}  {item.frontier_score:>3}  "
            f"{item.lane:<13}  {target.addr:>10}  {target.object_name:<35}  {target.name:<35}  {reasons}"
        )

    print("\nLane guide:")
    print("  auto-lift     -> high confidence; use /lift <target>")
    print("  cache-context -> cache Ghidra context, then /lift <target>")
    print("  manual-lift   -> use /lift; strategically useful but needs more care")
    print("  defer         -> low priority for now")


def cmd_cache_context(args: argparse.Namespace):
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

    # Check Ghidra MCP
    check_cmd = [sys.executable, str(ROOT / "tools" / "audit" / "check_ghidra_mcp.py")]
    proc = subprocess.run(check_cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        print("ERROR: Ghidra MCP not available.")
        print("You might have forgotten to start tools/shell/mcp-servers.sh or ghidra may not be running?")
        sys.exit(1)

    builder = ContextPackBuilder(ghidra_live=True)
    CONTEXT_CACHE.mkdir(parents=True, exist_ok=True)

    retrieval_available = _check_retrieval_index()

    for t in targets:
        print(f"Caching context for {t.name} ({t.addr})...")
        pack = builder.build(t)
        cache_file = CONTEXT_CACHE / f"{t.name}.json"
        ghidra_ctx = pack.ghidra
        has_decomp = bool(ghidra_ctx.get("decompile_c"))
        has_disasm = bool(ghidra_ctx.get("disassembly"))
        print(f"  decompile={'yes' if has_decomp else 'NO'}  disasm={'yes' if has_disasm else 'NO'}  "
              f"callers={len(ghidra_ctx.get('callers', []))}  callees={len(ghidra_ctx.get('callees', []))}")

        if retrieval_available and has_decomp:
            neighbors = _query_retrieval_neighbors(ghidra_ctx["decompile_c"])
            if neighbors:
                ghidra_ctx["similar_neighbors"] = neighbors
                print(f"  retrieval: {len(neighbors)} similar ported function(s) injected")
            else:
                print(f"  retrieval: no similar neighbors above threshold")
        elif not retrieval_available:
            pass  # silently skip — index not built yet

        cache_file.write_text(json.dumps(ghidra_ctx, indent=2), encoding="utf-8")

    print(f"\nCached {len(targets)} context packs to {CONTEXT_CACHE}")


if __name__ == "__main__":
    main()
