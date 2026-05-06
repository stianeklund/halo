#!/usr/bin/env python3
"""LLM Auto-Lift Harness — autonomous candidate generator + validation runner.

Treats the LLM as an untrusted code generator. Produces candidates, validates
them through the existing pipeline, and classifies results into a review queue.
Never commits to the repository.

Usage:
    python3 tools/llm_auto_lift.py score              # Rank unported functions
    python3 tools/llm_auto_lift.py cache-context ...   # Build Ghidra context packs
    python3 tools/llm_auto_lift.py generate ...        # Generate + validate candidates
    python3 tools/llm_auto_lift.py review              # Show pending results
    python3 tools/llm_auto_lift.py promote             # Apply auto_accept results
"""

from __future__ import annotations

import abc
import argparse
import json
import logging
import os
import re
import shlex
import shutil
import subprocess
import sys
import textwrap
import time
from dataclasses import asdict, dataclass, field
from datetime import datetime, timezone
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
INLINE_ASM_RE = re.compile(r"\b(?:__asm|asm\s+volatile|asm\s*\(|__asm__)\b")
INCLUDE_RE = re.compile(r"^\s*#\s*include\b", re.MULTILINE)
PRAGMA_RE = re.compile(r"^\s*#\s*pragma\b", re.MULTILINE)

DISQUALIFIED_OBJECTS = {"<xdk_stubs>", "<common>"}

# States
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
class ContextPack:
    schema_version: int
    target: dict
    ghidra: dict
    kb_context: dict
    source_context: dict
    hazards: dict
    delinked_available: bool
    constraints: list[str]


@dataclass
class LLMResponse:
    body_text: str
    model: str
    input_tokens: int
    output_tokens: int
    latency_ms: float


@dataclass
class ValidationStep:
    name: str
    passed: bool
    warnings: list[str] = field(default_factory=list)
    error: str = ""
    failure_class: str = ""


@dataclass
class LiftResult:
    addr: str
    name: str
    state: str
    attempts: int
    xdk_match_pct: Optional[float]
    abi_clean: bool
    warnings: list[str]
    validation_steps: list[dict]
    llm_provider: str
    llm_model: str
    total_input_tokens: int
    total_output_tokens: int
    duration_s: float


# ---------------------------------------------------------------------------
# Knowledge base helpers (thin wrappers to avoid importing clang at top level)
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
        """Extract function signatures from inlines.h and types.h for LLM context."""
        sigs = []
        for header_name in ["src/inlines.h", "src/types.h"]:
            header = ROOT / header_name
            if not header.exists():
                continue
            text = header.read_text(encoding="utf-8", errors="replace")
            # Extract function-like declarations and #define macros
            for line in text.splitlines():
                stripped = line.strip()
                if stripped.startswith("//") or stripped.startswith("/*"):
                    continue
                # Function declarations/definitions
                if re.match(r"^(?:static\s+)?(?:inline\s+)?(?:void|int|float|double|char|short|unsigned|bool|real_t|__int64)\b.*\w+\s*\(", stripped):
                    sig = stripped.split("{")[0].strip().rstrip(";").strip()
                    if sig and len(sig) < 200:
                        sigs.append(sig)
                # Macro definitions that rename functions
                elif re.match(r"^#define\s+\w+\s+\w+", stripped):
                    sigs.append(stripped)
        return sigs[:50]

    def _gather_ghidra_context(self, target: LiftTarget) -> dict:
        # Check cache first
        cache_file = CONTEXT_CACHE / f"{target.name}.json"
        if cache_file.exists():
            try:
                cached = json.loads(cache_file.read_text(encoding="utf-8"))
                log.info("Loaded cached Ghidra context for %s", target.name)
                return cached
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
            # Cap at 200 lines
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
            "Return ONLY the function body (everything between and including the outermost { and }).",
            "Do NOT include the function signature, return type, or parameter list.",
            "Do not change the function signature.",
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
    """Lightweight MCP client for the standard Ghidra MCP server."""

    ENDPOINT = "http://127.0.0.1:8090/sse"

    def __init__(self, timeout: float = 15.0):
        self.timeout = timeout
        self._session_url: Optional[str] = None

    def _connect(self) -> str:
        import urllib.request
        import urllib.parse

        req = urllib.request.Request(self.ENDPOINT, method="GET")
        resp = urllib.request.urlopen(req, timeout=self.timeout)

        deadline = time.monotonic() + self.timeout
        while True:
            if time.monotonic() >= deadline:
                raise RuntimeError("Timed out waiting for SSE bootstrap")
            raw = resp.readline()
            if not raw:
                raise RuntimeError("SSE stream closed")
            line = raw.decode("utf-8", errors="replace").strip()
            if line.startswith("data:"):
                path = line[5:].strip()
                if path.startswith("/messages/?session_id="):
                    parsed = urllib.parse.urlparse(self.ENDPOINT)
                    return f"{parsed.scheme}://{parsed.netloc}{path}"
                raise RuntimeError(f"Unexpected SSE payload: {path!r}")

    def _ensure_session(self):
        if self._session_url:
            return
        self._session_url = self._connect()
        self._post_rpc(self._session_url, {
            "jsonrpc": "2.0", "id": 0, "method": "initialize",
            "params": {"protocolVersion": "2024-11-05",
                       "capabilities": {},
                       "clientInfo": {"name": "auto-lift", "version": "0.1.0"}},
        })
        self._post_rpc(self._session_url, {
            "jsonrpc": "2.0", "method": "notifications/initialized",
        })

    def _post_rpc(self, url: str, payload: dict) -> Optional[dict]:
        import urllib.request

        data = json.dumps(payload).encode("utf-8")
        req = urllib.request.Request(
            url, data=data,
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        with urllib.request.urlopen(req, timeout=self.timeout) as resp:
            if resp.status >= 400:
                raise RuntimeError(f"HTTP {resp.status}")
        return None

    def call_tool(self, tool_name: str, arguments: dict) -> dict:
        self._ensure_session()
        assert self._session_url

        import urllib.request

        rpc_id = int(time.monotonic() * 1000) % 999999
        payload = {
            "jsonrpc": "2.0",
            "id": rpc_id,
            "method": "tools/call",
            "params": {"name": tool_name, "arguments": arguments},
        }
        data = json.dumps(payload).encode("utf-8")
        req = urllib.request.Request(
            self._session_url, data=data,
            headers={"Content-Type": "application/json"},
            method="POST",
        )

        # The MCP server sends the response as an SSE event on the original
        # SSE stream, not on this POST response.  For simplicity, we re-open
        # the SSE stream and read the next data event.  This works for
        # single-shot calls but would need queuing for concurrent use.
        with urllib.request.urlopen(req, timeout=self.timeout):
            pass

        # Read result from SSE stream — simplified: re-connect and read
        sse_req = urllib.request.Request(self.ENDPOINT, method="GET")
        with urllib.request.urlopen(sse_req, timeout=self.timeout) as resp:
            deadline = time.monotonic() + self.timeout
            while time.monotonic() < deadline:
                raw = resp.readline()
                if not raw:
                    break
                line = raw.decode("utf-8", errors="replace").strip()
                if line.startswith("data:") and '"result"' in line:
                    try:
                        msg = json.loads(line[5:].strip())
                        if msg.get("id") == rpc_id:
                            return msg.get("result", {})
                    except json.JSONDecodeError:
                        continue

        raise RuntimeError(f"No response for tool call {tool_name}")


# ---------------------------------------------------------------------------
# LLM Adapters
# ---------------------------------------------------------------------------

SYSTEM_PROMPT = textwrap.dedent("""\
    You are translating Ghidra decompiler output into idiomatic C for a
    Halo CE Xbox binary recovery project. The target is x86 MSVC 7.1
    compiled code running on the original Xbox.

    Your output MUST be exactly the function body — everything between and
    including the outermost {{ and }}. Do NOT include the function signature,
    return type, or parameter list. Do NOT wrap output in markdown fences.

    Preserve all struct offsets, pointer arithmetic, control flow shape, and
    side-effect ordering from the decompiler output. When the decompiler
    shows an offset like (param_1 + 0x3c), keep that exact offset.

    Use C99 types. Use (int) for float-to-int casts. Never call compiler
    intrinsics by name or address.
""")


def _build_user_prompt(pack: ContextPack) -> str:
    parts = []
    parts.append(f"## Target Function\n```\n{pack.target['decl']}\n```\n")

    if pack.ghidra.get("decompile_c"):
        parts.append(f"## Ghidra Decompiler Output\n```c\n{pack.ghidra['decompile_c']}\n```\n")

    if pack.ghidra.get("disassembly"):
        parts.append(f"## Disassembly\n```asm\n{pack.ghidra['disassembly']}\n```\n")

    if pack.ghidra.get("callees"):
        parts.append(f"## Callees\n{', '.join(pack.ghidra['callees'])}\n")

    if pack.kb_context.get("sibling_functions"):
        decls = []
        for f in pack.kb_context["sibling_functions"][:15]:
            status = " [ported]" if f["ported"] else ""
            decls.append(f"  {f['decl']}{status}")
        parts.append(f"## Sibling Functions in Same Object\n" + "\n".join(decls) + "\n")

    if pack.kb_context.get("data_symbols"):
        decls = [f"  {d['decl']}" for d in pack.kb_context["data_symbols"][:10]]
        parts.append(f"## Data Symbols\n" + "\n".join(decls) + "\n")

    if pack.source_context.get("available_functions"):
        funcs = pack.source_context["available_functions"][:30]
        parts.append("## Available Inline/Math Functions\nThese are defined in project headers and available without additional #include:\n```\n" + "\n".join(funcs) + "\n```\n")

    if pack.source_context.get("neighboring_bodies"):
        for nb in pack.source_context["neighboring_bodies"][:2]:
            parts.append(f"## Example: {nb['name']}\n```c\n{nb['body']}\n```\n")

    if pack.hazards.get("has_fpu"):
        parts.append("## Warning: FPU\nThis function uses floating-point. Use (int)float_expr for casts. Preserve x87 operand order from disassembly.\n")

    parts.append("## Constraints\n" + "\n".join(f"- {c}" for c in pack.constraints) + "\n")
    parts.append("## Output\nReturn ONLY the function body (the { ... } block). No signature, no markdown fences.")

    return "\n".join(parts)


def _build_correction_prompt(pack: ContextPack, prev_output: str,
                             failure_class: str, failure_detail: str) -> str:
    guidance = {
        "parse": "Your previous output was not valid C. Return ONLY the function body wrapped in braces { }. No markdown, no signature.",
        "signature": "You included the function signature. Return ONLY the body (the { ... } block), not the return type or parameter list.",
        "forbidden": f"Your output contained a forbidden pattern:\n{failure_detail}\nRemove it and use the equivalent C idiom.",
        "build": f"Your implementation failed to compile:\n{failure_detail}\nFix ONLY the compilation error. Do not change the overall structure.",
        "abi": f"The ABI audit found register argument issues:\n{failure_detail}\nEnsure arguments match the declared signature exactly.",
        "xdk": f"The XDK structural comparison found differences:\n{failure_detail}\nAdjust control flow to more closely match the original binary.",
        "fpu": f"FPU operand order differs from the original:\n{failure_detail}\nCheck subtraction and multiplication order against the disassembly.",
    }

    parts = [
        f"## Previous Attempt\n```c\n{prev_output}\n```\n",
        f"## Failure: {failure_class}\n{guidance.get(failure_class, failure_detail)}\n",
        "## Constraints\n" + "\n".join(f"- {c}" for c in pack.constraints) + "\n",
        "## Output\nReturn ONLY the corrected function body (the { ... } block).",
    ]
    return "\n".join(parts)


class LLMAdapter(abc.ABC):

    @abc.abstractmethod
    def generate(self, system: str, user: str) -> LLMResponse: ...

    @abc.abstractmethod
    def provider_name(self) -> str: ...


class ClaudeAdapter(LLMAdapter):

    def __init__(self, model: str = "claude-sonnet-4-20250514", temperature: float = 0.2):
        self.model = model
        self.temperature = temperature
        self._client = None

    def _get_client(self):
        if self._client is None:
            import anthropic
            self._client = anthropic.Anthropic()
        return self._client

    def generate(self, system: str, user: str) -> LLMResponse:
        client = self._get_client()
        t0 = time.monotonic()
        resp = client.messages.create(
            model=self.model,
            max_tokens=4096,
            temperature=self.temperature,
            system=system,
            messages=[{"role": "user", "content": user}],
        )
        latency = (time.monotonic() - t0) * 1000
        body = resp.content[0].text if resp.content else ""
        return LLMResponse(
            body_text=body,
            model=self.model,
            input_tokens=resp.usage.input_tokens,
            output_tokens=resp.usage.output_tokens,
            latency_ms=latency,
        )

    def provider_name(self) -> str:
        return "claude"


class OpenAIAdapter(LLMAdapter):

    def __init__(self, model: str = "gpt-4o", temperature: float = 0.2):
        self.model = model
        self.temperature = temperature
        self._client = None

    def _get_client(self):
        if self._client is None:
            import openai
            self._client = openai.OpenAI()
        return self._client

    def generate(self, system: str, user: str) -> LLMResponse:
        client = self._get_client()
        t0 = time.monotonic()
        resp = client.chat.completions.create(
            model=self.model,
            temperature=self.temperature,
            max_tokens=4096,
            messages=[
                {"role": "system", "content": system},
                {"role": "user", "content": user},
            ],
        )
        latency = (time.monotonic() - t0) * 1000
        choice = resp.choices[0]
        body = choice.message.content or ""
        return LLMResponse(
            body_text=body,
            model=self.model,
            input_tokens=resp.usage.prompt_tokens if resp.usage else 0,
            output_tokens=resp.usage.completion_tokens if resp.usage else 0,
            latency_ms=latency,
        )

    def provider_name(self) -> str:
        return "openai"


class GeminiAdapter(LLMAdapter):

    def __init__(self, model: str = "gemini-2.5-flash", temperature: float = 0.2):
        self.model = model
        self.temperature = temperature
        self._client = None

    def _get_client(self):
        if self._client is None:
            import google.generativeai as genai
            self._client = genai.GenerativeModel(self.model)
        return self._client

    def generate(self, system: str, user: str) -> LLMResponse:
        import google.generativeai as genai

        client = self._get_client()
        t0 = time.monotonic()
        resp = client.generate_content(
            f"{system}\n\n{user}",
            generation_config=genai.types.GenerationConfig(
                temperature=self.temperature,
                max_output_tokens=4096,
            ),
        )
        latency = (time.monotonic() - t0) * 1000
        body = resp.text if resp.text else ""
        # Gemini doesn't always expose token counts the same way
        usage_meta = getattr(resp, "usage_metadata", None)
        return LLMResponse(
            body_text=body,
            model=self.model,
            input_tokens=getattr(usage_meta, "prompt_token_count", 0) if usage_meta else 0,
            output_tokens=getattr(usage_meta, "candidates_token_count", 0) if usage_meta else 0,
            latency_ms=latency,
        )

    def provider_name(self) -> str:
        return "gemini"


def _make_adapter(provider: str, model: str = "", temperature: float = 0.2) -> LLMAdapter:
    if provider == "claude":
        return ClaudeAdapter(model=model or "claude-sonnet-4-20250514", temperature=temperature)
    elif provider == "openai":
        return OpenAIAdapter(model=model or "gpt-4o", temperature=temperature)
    elif provider == "gemini":
        return GeminiAdapter(model=model or "gemini-2.5-flash", temperature=temperature)
    else:
        raise ValueError(f"Unknown LLM provider: {provider}")


# ---------------------------------------------------------------------------
# Output Validator
# ---------------------------------------------------------------------------

class OutputValidator:

    def __init__(self, *, worktree_path: Path, target: LiftTarget, pack: ContextPack,
                 build_cmd: str, skip_xdk: bool = False, xdk_threshold: float = 50.0):
        self.worktree = worktree_path
        self.target = target
        self.pack = pack
        self.build_cmd = build_cmd
        self.skip_xdk = skip_xdk
        self.xdk_threshold = xdk_threshold

    def validate(self, body: str, artifact_dir: Path, attempt: int) -> list[ValidationStep]:
        steps: list[ValidationStep] = []

        # Step 1: Parse
        step = self._check_parse(body)
        steps.append(step)
        if not step.passed:
            return steps

        # Step 2: Extract body if full function was returned
        body, step = self._check_signature(body)
        steps.append(step)
        if not step.passed:
            return steps

        # Step 3: Forbidden patterns
        step = self._check_forbidden(body)
        steps.append(step)
        if not step.passed:
            return steps

        # Step 4: Build
        step = self._check_build(body, artifact_dir, attempt)
        steps.append(step)
        if not step.passed:
            return steps

        # Step 5: ABI audit
        step = self._check_abi(artifact_dir, attempt)
        steps.append(step)
        if not step.passed:
            return steps

        # Step 6: XDK verify
        step = self._check_xdk(artifact_dir, attempt)
        steps.append(step)

        return steps

    def _check_parse(self, body: str) -> ValidationStep:
        cleaned = body.strip()

        # Strip markdown fencing
        if cleaned.startswith("```"):
            lines = cleaned.splitlines()
            if lines[0].startswith("```"):
                lines = lines[1:]
            if lines and lines[-1].strip() == "```":
                lines = lines[:-1]
            cleaned = "\n".join(lines).strip()

        if not cleaned.startswith("{"):
            # Maybe the LLM returned the full function
            brace = cleaned.find("{")
            if brace < 0:
                return ValidationStep("parse", False, error="No opening brace found", failure_class="parse")

        # Check balanced braces
        depth = 0
        for ch in cleaned:
            if ch == "{":
                depth += 1
            elif ch == "}":
                depth -= 1
                if depth < 0:
                    return ValidationStep("parse", False, error="Unbalanced braces (extra })", failure_class="parse")
        if depth != 0:
            return ValidationStep("parse", False, error=f"Unbalanced braces (depth={depth})", failure_class="parse")

        return ValidationStep("parse", True)

    def _check_signature(self, body: str) -> tuple[str, ValidationStep]:
        cleaned = body.strip()

        # Strip markdown fencing
        if cleaned.startswith("```"):
            lines = cleaned.splitlines()
            if lines[0].startswith("```"):
                lines = lines[1:]
            if lines and lines[-1].strip() == "```":
                lines = lines[:-1]
            cleaned = "\n".join(lines).strip()

        if cleaned.startswith("{"):
            return cleaned, ValidationStep("signature", True)

        # Try to extract just the body
        brace = cleaned.find("{")
        if brace >= 0:
            extracted = cleaned[brace:]
            return extracted, ValidationStep("signature", True, warnings=["Extracted body from full function"])

        return cleaned, ValidationStep("signature", False, error="Could not find function body start", failure_class="signature")

    def _check_forbidden(self, body: str) -> ValidationStep:
        errors = []

        if INLINE_ASM_RE.search(body):
            errors.append("Contains inline assembly")
        if INTRINSIC_RE.search(body):
            errors.append("Contains MSVC intrinsic address reference")
        if INCLUDE_RE.search(body):
            errors.append("Contains #include directive")
        if PRAGMA_RE.search(body):
            errors.append("Contains #pragma directive")

        # Check for direct calls to intrinsic names
        intrinsic_names = ["_ftol2", "_chkstk", "__SEH_prolog", "__SEH_epilog",
                           "_allmul", "_aullshr", "_aullrem", "_aulldiv"]
        for name in intrinsic_names:
            if re.search(rf"\b{re.escape(name)}\s*\(", body):
                errors.append(f"Direct call to MSVC intrinsic {name}")

        if errors:
            return ValidationStep("forbidden", False, error="; ".join(errors), failure_class="forbidden")

        return ValidationStep("forbidden", True)

    def _check_build(self, body: str, artifact_dir: Path, attempt: int) -> ValidationStep:
        # Write the full function (signature + body) to a temp file
        decl = re.sub(r"@<\w+>", "", self.target.decl).rstrip(";").strip()
        full_function = f"{decl}\n{body}\n"

        candidate_file = artifact_dir / f"candidate_v{attempt}.c"
        candidate_file.write_text(full_function, encoding="utf-8")

        # Apply via maintain.py
        source_path = self.target.source_path
        maintain_cmd = [
            sys.executable,
            str(ROOT / "tools" / "analysis" / "maintain.py"),
            "--update-from", str(candidate_file),
            source_path,
        ]
        proc = subprocess.run(
            maintain_cmd, cwd=str(self.worktree),
            capture_output=True, text=True, timeout=30,
        )
        if proc.returncode != 0:
            err = (proc.stderr or proc.stdout or "unknown error")[:500]
            return ValidationStep("build", False, error=f"maintain.py failed: {err}", failure_class="build")

        # Update kb.json to mark as ported
        self._mark_ported_in_worktree()

        # Build
        build_log = artifact_dir / f"build_v{attempt}.log"
        build_argv = shlex.split(self.build_cmd)
        proc = subprocess.run(
            build_argv, cwd=str(self.worktree),
            capture_output=True, text=True, timeout=120,
        )
        build_log.write_text(
            f"$ {self.build_cmd}\nexit={proc.returncode}\n\n"
            f"[stdout]\n{proc.stdout}\n[stderr]\n{proc.stderr}",
            encoding="utf-8",
        )
        if proc.returncode != 0:
            err = (proc.stderr or proc.stdout or "")[-800:]
            return ValidationStep("build", False, error=f"Build failed:\n{err}", failure_class="build")

        return ValidationStep("build", True)

    def _mark_ported_in_worktree(self):
        kb_path = self.worktree / "kb.json"
        kb = json.loads(kb_path.read_text(encoding="utf-8"))
        for obj in kb["objects"]:
            for func in obj.get("functions", []):
                if func.get("addr") == self.target.addr:
                    func["ported"] = True
                    break
        kb_path.write_text(json.dumps(kb, indent=2), encoding="utf-8")

    def _check_abi(self, artifact_dir: Path, attempt: int) -> ValidationStep:
        abi_log = artifact_dir / f"abi_v{attempt}.json"
        abi_cmd = [
            sys.executable,
            str(ROOT / "tools" / "audit" / "audit_reg_abi.py"),
            "--target", self.target.name,
            "--json",
        ]
        try:
            proc = subprocess.run(
                abi_cmd, cwd=str(self.worktree),
                capture_output=True, text=True, timeout=30,
            )
            abi_log.write_text(proc.stdout or proc.stderr or "", encoding="utf-8")

            if proc.returncode != 0:
                return ValidationStep("abi", False, error=proc.stdout[:500], failure_class="abi")

            # Parse JSON for warnings
            warnings = []
            try:
                data = json.loads(proc.stdout)
                warnings = data.get("warnings", [])
            except json.JSONDecodeError:
                pass

            return ValidationStep("abi", True, warnings=warnings)

        except subprocess.TimeoutExpired:
            return ValidationStep("abi", False, error="ABI audit timed out", failure_class="abi")

    def _check_xdk(self, artifact_dir: Path, attempt: int) -> ValidationStep:
        if self.skip_xdk or not self.pack.delinked_available:
            return ValidationStep("xdk", True, warnings=["skipped (no delinked ref)"])

        xdk_log = artifact_dir / f"xdk_v{attempt}.log"
        xdk_cmd = [
            sys.executable,
            str(ROOT / "tools" / "verify" / "xdk_verify.py"),
            self.target.source_path,
            "--function", self.target.name,
            "--show-diffs",
            "--threshold", "0",
        ]
        try:
            proc = subprocess.run(
                xdk_cmd, cwd=str(self.worktree),
                capture_output=True, text=True, timeout=60,
            )
            output = proc.stdout + "\n" + proc.stderr
            xdk_log.write_text(output, encoding="utf-8")

            # Parse match percentage
            match_pct = None
            m = re.search(r"(\d+(?:\.\d+)?)\s*%\s*match", output)
            if m:
                match_pct = float(m.group(1))

            # Parse FPU warnings
            fpu_warns = re.findall(r"\[FPU-WARN\].*", output)
            warnings = [f"xdk_match={match_pct}%"] if match_pct is not None else []
            warnings.extend(fpu_warns)

            if match_pct is not None and match_pct < self.xdk_threshold:
                failure_class = "fpu" if fpu_warns else "xdk"
                return ValidationStep(
                    "xdk", False, warnings=warnings,
                    error=f"XDK match {match_pct}% < threshold {self.xdk_threshold}%",
                    failure_class=failure_class,
                )

            return ValidationStep("xdk", True, warnings=warnings)

        except subprocess.TimeoutExpired:
            return ValidationStep("xdk", False, error="XDK verify timed out", failure_class="xdk")


# ---------------------------------------------------------------------------
# Worktree Manager
# ---------------------------------------------------------------------------

class WorktreeManager:

    def __init__(self, repo_root: Path, worktree_path: Path):
        self.repo_root = repo_root
        self.worktree_path = worktree_path
        self._created = False

    def create(self) -> Path:
        if self.worktree_path.exists():
            self.cleanup()
        subprocess.run(
            ["git", "worktree", "add", "--detach", str(self.worktree_path)],
            cwd=str(self.repo_root), check=True,
            capture_output=True, text=True,
        )
        self._created = True
        log.info("Created worktree at %s", self.worktree_path)
        return self.worktree_path

    def configure_build(self):
        build_dir = self.worktree_path / "build"
        if not (build_dir / "Makefile").exists():
            log.info("Configuring CMake in worktree...")
            # Mirror the main build's cmake configuration
            toolchain = self.repo_root / "toolchains" / "llvm.cmake"
            cmake_args = ["cmake", "-B", "build", "-S", "."]
            if toolchain.exists():
                cmake_args.append(f"-DCMAKE_TOOLCHAIN_FILE={toolchain}")
            proc = subprocess.run(
                cmake_args,
                cwd=str(self.worktree_path),
                capture_output=True, text=True,
                timeout=120,
            )
            if proc.returncode != 0:
                log.error("CMake configure failed: %s", proc.stderr[-500:])
                raise RuntimeError(f"CMake configure failed in worktree: {proc.stderr[-300:]}")

    def reset(self):
        if self._created:
            subprocess.run(
                ["git", "checkout", "--", "."],
                cwd=str(self.worktree_path),
                capture_output=True, text=True,
            )

    def cleanup(self):
        if self.worktree_path.exists():
            subprocess.run(
                ["git", "worktree", "remove", "--force", str(self.worktree_path)],
                cwd=str(self.repo_root),
                capture_output=True, text=True,
            )
            self._created = False
            log.info("Cleaned up worktree at %s", self.worktree_path)


# ---------------------------------------------------------------------------
# Auto-Lift Runner
# ---------------------------------------------------------------------------

class AutoLiftRunner:

    def __init__(self, *, adapter: LLMAdapter, ghidra_live: bool = False,
                 build_cmd: str = "", skip_xdk: bool = False,
                 xdk_threshold: float = 50.0, max_retries: int = 2):
        self.adapter = adapter
        self.ghidra_live = ghidra_live
        self.build_cmd = build_cmd or f"{sys.executable} tools/build/build.py -q --target halo"
        self.skip_xdk = skip_xdk
        self.xdk_threshold = xdk_threshold
        self.max_retries = max_retries
        self.pack_builder = ContextPackBuilder(ghidra_live=ghidra_live)

    def run_batch(self, targets: list[LiftTarget], batch_dir: Path) -> list[LiftResult]:
        batch_dir.mkdir(parents=True, exist_ok=True)

        worktree_path = batch_dir / "worktree"
        wt = WorktreeManager(ROOT, worktree_path)

        results: list[LiftResult] = []
        try:
            wt.create()
            wt.configure_build()

            for target in targets:
                log.info("=" * 60)
                log.info("Processing %s (%s) from %s", target.name, target.addr, target.object_name)
                result = self._run_one(target, batch_dir, wt)
                results.append(result)

                # Reset worktree for next function
                wt.reset()
        finally:
            wt.cleanup()

        # Write batch summary
        summary = {
            "batch_id": batch_dir.name,
            "llm_provider": self.adapter.provider_name(),
            "candidates_attempted": len(results),
            "auto_accept": sum(1 for r in results if r.state == AUTO_ACCEPT),
            "needs_review": sum(1 for r in results if r.state == NEEDS_REVIEW),
            "reject": sum(1 for r in results if r.state == REJECT),
            "total_input_tokens": sum(r.total_input_tokens for r in results),
            "total_output_tokens": sum(r.total_output_tokens for r in results),
            "results": [asdict(r) for r in results],
        }
        (batch_dir / "summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")

        return results

    def _run_one(self, target: LiftTarget, batch_dir: Path, wt: WorktreeManager) -> LiftResult:
        t0 = time.monotonic()
        func_dir = batch_dir / target.name
        func_dir.mkdir(parents=True, exist_ok=True)

        # Build context pack
        pack = self.pack_builder.build(target)
        pack_dict = asdict(pack)
        (func_dir / "context.json").write_text(json.dumps(pack_dict, indent=2), encoding="utf-8")

        if not pack.ghidra.get("decompile_c"):
            log.warning("No decompiler output for %s — skipping", target.name)
            return LiftResult(
                addr=target.addr, name=target.name, state=REJECT, attempts=0,
                xdk_match_pct=None, abi_clean=False, warnings=["No decompiler output"],
                validation_steps=[], llm_provider=self.adapter.provider_name(),
                llm_model="", total_input_tokens=0, total_output_tokens=0,
                duration_s=time.monotonic() - t0,
            )

        # Generate + validate loop
        system_prompt = SYSTEM_PROMPT
        user_prompt = _build_user_prompt(pack)
        total_in = 0
        total_out = 0
        model = ""
        best_body = ""
        best_steps: list[ValidationStep] = []

        for attempt in range(1, self.max_retries + 2):
            log.info("  Attempt %d/%d", attempt, self.max_retries + 1)

            # Generate
            try:
                if attempt == 1:
                    resp = self.adapter.generate(system_prompt, user_prompt)
                else:
                    # Correction prompt
                    failed_step = next((s for s in best_steps if not s.passed), None)
                    if failed_step:
                        correction = _build_correction_prompt(
                            pack, best_body, failed_step.failure_class, failed_step.error
                        )
                        resp = self.adapter.generate(system_prompt, correction)
                    else:
                        break  # All passed, no need for correction

                total_in += resp.input_tokens
                total_out += resp.output_tokens
                model = resp.model
                body = resp.body_text.strip()

            except Exception as e:
                log.error("  LLM generation failed: %s", e)
                best_steps = [ValidationStep("llm_call", False, error=str(e), failure_class="parse")]
                break

            # Save raw candidate
            (func_dir / f"candidate_v{attempt}.c").write_text(body, encoding="utf-8")

            # Validate
            validator = OutputValidator(
                worktree_path=wt.worktree_path,
                target=target,
                pack=pack,
                build_cmd=self.build_cmd,
                skip_xdk=self.skip_xdk,
                xdk_threshold=self.xdk_threshold,
            )
            steps = validator.validate(body, func_dir, attempt)

            # Save validation log
            step_dicts = [asdict(s) for s in steps]
            (func_dir / f"validation_v{attempt}.json").write_text(
                json.dumps(step_dicts, indent=2), encoding="utf-8"
            )

            best_body = body
            best_steps = steps

            # Check if all passed
            if all(s.passed for s in steps):
                log.info("  All validation steps passed on attempt %d", attempt)
                break
            else:
                failed = next(s for s in steps if not s.passed)
                log.info("  Failed at step '%s': %s", failed.name, failed.error[:120])

        # Save final candidate (full function)
        if best_body:
            decl = re.sub(r"@<\w+>", "", target.decl).rstrip(";").strip()
            # Extract just body if needed
            cleaned = best_body.strip()
            if cleaned.startswith("```"):
                lines = cleaned.splitlines()
                if lines[0].startswith("```"):
                    lines = lines[1:]
                if lines and lines[-1].strip() == "```":
                    lines = lines[:-1]
                cleaned = "\n".join(lines).strip()
            if not cleaned.startswith("{"):
                brace = cleaned.find("{")
                if brace >= 0:
                    cleaned = cleaned[brace:]
            (func_dir / "candidate_final.c").write_text(f"{decl}\n{cleaned}\n", encoding="utf-8")

        # Classify result
        all_passed = all(s.passed for s in best_steps)
        all_warnings = []
        for s in best_steps:
            all_warnings.extend(s.warnings)

        xdk_match = None
        for s in best_steps:
            for w in s.warnings:
                m = re.match(r"xdk_match=([\d.]+)%", w)
                if m:
                    xdk_match = float(m.group(1))

        abi_clean = all(s.passed for s in best_steps if s.name == "abi")
        has_fpu_warn = any("[FPU-WARN]" in w for s in best_steps for w in s.warnings)

        if all_passed and not has_fpu_warn:
            state = AUTO_ACCEPT
        elif all_passed:
            state = NEEDS_REVIEW
        else:
            state = REJECT

        attempts_used = min(
            sum(1 for f in func_dir.glob("candidate_v*.c")),
            self.max_retries + 1,
        )

        result = LiftResult(
            addr=target.addr,
            name=target.name,
            state=state,
            attempts=attempts_used,
            xdk_match_pct=xdk_match,
            abi_clean=abi_clean,
            warnings=all_warnings,
            validation_steps=[asdict(s) for s in best_steps],
            llm_provider=self.adapter.provider_name(),
            llm_model=model,
            total_input_tokens=total_in,
            total_output_tokens=total_out,
            duration_s=time.monotonic() - t0,
        )

        (func_dir / "result.json").write_text(json.dumps(asdict(result), indent=2), encoding="utf-8")

        log.info("  Result: %s (attempts=%d, xdk=%s)", state, attempts_used,
                 f"{xdk_match:.1f}%" if xdk_match is not None else "n/a")

        return result


# ---------------------------------------------------------------------------
# Review mode
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
            xdk = f"{r['xdk_match_pct']:.0f}%" if r.get("xdk_match_pct") is not None else "n/a"
            print(f"  [{icon}] {r.get('name', '?'):30s}  {r.get('state', '?'):15s}  "
                  f"xdk={xdk}  attempts={r.get('attempts', 0)}")


# ---------------------------------------------------------------------------
# Promote mode
# ---------------------------------------------------------------------------

def cmd_promote(args: argparse.Namespace):
    if not ARTIFACT_ROOT.exists():
        print("No artifacts found.")
        return

    # Find latest batch
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

        # Read target info from context.json
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
        description="LLM Auto-Lift Harness — candidate generator + validation runner",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument("-v", "--verbose", action="store_true", help="Verbose logging")
    sub = ap.add_subparsers(dest="mode", required=True)

    # -- score --
    p_score = sub.add_parser("score", help="Rank unported functions by liftability")
    p_score.add_argument("--object", default="", help="Filter to one object")
    p_score.add_argument("--min-score", type=int, default=0, help="Minimum score")
    p_score.add_argument("--limit", type=int, default=30, help="Max results")
    p_score.add_argument("--json", action="store_true", help="JSON output")

    # -- cache-context --
    p_cache = sub.add_parser("cache-context", help="Build Ghidra context packs (requires MCP)")
    p_cache.add_argument("--target", default="", help="Specific function address or name")
    p_cache.add_argument("--batch", type=int, default=5, help="Number of top targets")
    p_cache.add_argument("--object", default="", help="Filter to one object")
    p_cache.add_argument("--min-score", type=int, default=30, help="Minimum liftability score")

    # -- generate --
    p_gen = sub.add_parser("generate", help="Generate + validate candidates")
    p_gen.add_argument("--target", default="", help="Specific function address or name")
    p_gen.add_argument("--batch", type=int, default=5, help="Number of top targets")
    p_gen.add_argument("--object", default="", help="Filter to one object")
    p_gen.add_argument("--min-score", type=int, default=30, help="Minimum liftability score")
    p_gen.add_argument("--llm-provider", default="claude", choices=["claude", "openai", "gemini"])
    p_gen.add_argument("--llm-model", default="", help="Override model")
    p_gen.add_argument("--temperature", type=float, default=0.2)
    p_gen.add_argument("--max-retries", type=int, default=2, help="Correction retries (default: 2)")
    p_gen.add_argument("--skip-xdk", action="store_true", help="Skip XDK verify")
    p_gen.add_argument("--xdk-threshold", type=float, default=50.0, help="XDK match %% for auto_accept")
    p_gen.add_argument("--build-cmd", default="", help="Override build command")

    # -- review --
    p_review = sub.add_parser("review", help="Show pending results")
    p_review.add_argument("--limit", type=int, default=5, help="Max batches to show")

    # -- promote --
    p_promote = sub.add_parser("promote", help="Apply auto_accept results to main worktree")
    p_promote.add_argument("--batch", default="", help="Specific batch directory name")
    p_promote.add_argument("--apply", action="store_true", help="Actually apply (default: dry run)")

    args = ap.parse_args()
    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.INFO,
        format="%(levelname)s: %(message)s",
    )

    if args.mode == "score":
        cmd_score(args)
    elif args.mode == "cache-context":
        cmd_cache_context(args)
    elif args.mode == "generate":
        cmd_generate(args)
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

    # Count total unported for reference
    kb_raw = _load_kb_raw()
    total_unported = sum(
        1 for obj in kb_raw["objects"]
        for f in obj.get("functions", [])
        if not f.get("ported")
    )
    print(f"\n{len(targets)} scored candidates of {total_unported} unported")


def cmd_cache_context(args: argparse.Namespace):
    scorer = LiftabilityScorer()
    if args.target:
        targets = scorer.score_all()
        targets = [t for t in targets if t.addr == args.target or t.name == args.target]
    else:
        targets = scorer.score_all(object_filter=args.object, min_score=args.min_score)
        targets = targets[: args.batch]

    if not targets:
        print("No targets found.")
        return

    # Check Ghidra MCP
    check_cmd = [sys.executable, str(ROOT / "tools" / "audit" / "check_ghidra_mcp.py")]
    proc = subprocess.run(check_cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        print("ERROR: Ghidra MCP not available.")
        print("You might have forgotten to start tools/mcp-servers.sh or ghidra may not be running?")
        sys.exit(1)

    builder = ContextPackBuilder(ghidra_live=True)
    CONTEXT_CACHE.mkdir(parents=True, exist_ok=True)

    for t in targets:
        print(f"Caching context for {t.name} ({t.addr})...")
        pack = builder.build(t)
        cache_file = CONTEXT_CACHE / f"{t.name}.json"
        ghidra_ctx = pack.ghidra
        cache_file.write_text(json.dumps(ghidra_ctx, indent=2), encoding="utf-8")
        has_decomp = bool(ghidra_ctx.get("decompile_c"))
        has_disasm = bool(ghidra_ctx.get("disassembly"))
        print(f"  decompile={'yes' if has_decomp else 'NO'}  disasm={'yes' if has_disasm else 'NO'}  "
              f"callers={len(ghidra_ctx.get('callers', []))}  callees={len(ghidra_ctx.get('callees', []))}")

    print(f"\nCached {len(targets)} context packs to {CONTEXT_CACHE}")


def cmd_generate(args: argparse.Namespace):
    scorer = LiftabilityScorer()
    if args.target:
        all_targets = scorer.score_all()
        targets = [t for t in all_targets if t.addr == args.target or t.name == args.target]
        if not targets:
            print(f"Target {args.target} not found or already ported.")
            sys.exit(1)
    else:
        targets = scorer.score_all(object_filter=args.object, min_score=args.min_score)
        targets = targets[: args.batch]

    if not targets:
        print("No eligible targets found.")
        sys.exit(1)

    # Check that all targets have cached context
    missing_ctx = []
    for t in targets:
        cache_file = CONTEXT_CACHE / f"{t.name}.json"
        if not cache_file.exists():
            missing_ctx.append(t.name)

    if missing_ctx:
        print(f"WARNING: {len(missing_ctx)} target(s) have no cached Ghidra context.")
        print(f"  Missing: {', '.join(missing_ctx[:5])}")
        print("  Run 'cache-context' first, or pass --ghidra-live if Ghidra MCP is running.")
        print("  Proceeding without decompiler output (likely to fail).")

    adapter = _make_adapter(args.llm_provider, args.llm_model, args.temperature)

    batch_id = f"batch_{datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S')}"
    batch_dir = ARTIFACT_ROOT / batch_id

    print(f"Starting batch {batch_id} with {len(targets)} target(s)")
    print(f"Provider: {args.llm_provider}, Model: {args.llm_model or 'default'}")
    print(f"Max retries: {args.max_retries}, XDK threshold: {args.xdk_threshold}%")
    print()

    runner = AutoLiftRunner(
        adapter=adapter,
        ghidra_live=False,  # Use cached context
        build_cmd=args.build_cmd,
        skip_xdk=args.skip_xdk,
        xdk_threshold=args.xdk_threshold,
        max_retries=args.max_retries,
    )

    results = runner.run_batch(targets, batch_dir)

    # Summary
    print(f"\n{'=' * 60}")
    print(f"Batch complete: {batch_id}")
    accept = sum(1 for r in results if r.state == AUTO_ACCEPT)
    review = sum(1 for r in results if r.state == NEEDS_REVIEW)
    reject = sum(1 for r in results if r.state == REJECT)
    print(f"  auto_accept: {accept}")
    print(f"  needs_review: {review}")
    print(f"  reject: {reject}")
    total_in = sum(r.total_input_tokens for r in results)
    total_out = sum(r.total_output_tokens for r in results)
    print(f"  tokens: {total_in} in, {total_out} out")
    print(f"  artifacts: {batch_dir}")

    if accept > 0:
        print(f"\nTo promote accepted results:")
        print(f"  python3 tools/llm_auto_lift.py promote --batch {batch_id} --apply")


if __name__ == "__main__":
    main()
