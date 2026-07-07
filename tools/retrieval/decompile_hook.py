#!/usr/bin/env python3
"""PostToolUse hook: inject retrieval neighbors + hazard warnings after Ghidra decompile.

Wired in .claude/settings.json on mcp__ghidra__decompile_function and
mcp__ghidra__batch_decompile.

When the agent decompiles a function, this hook:
1. Queries the index for similar high-quality ported functions (worked examples)
2. Queries for hazard warnings from similar functions that failed
3. Detects hazard patterns in the pseudocode and surfaces lift-learnings sections
4. Combines all three into a single systemMessage (budget: 4000 chars)

Exit codes: 0 always (errors are logged, not fatal).
Output: {"systemMessage": "..."} on stdout when content is found.
"""

from __future__ import annotations

import json
import os
import re
import subprocess
import sys
import time
from pathlib import Path


def _canonical_repo_root() -> Path:
    """Resolve the canonical repo root that actually holds .venv + the index.

    `__file__`-relative resolution breaks when this hook runs from a linked git
    worktree: the worktree has no `.venv`, so the fallback spawns bare `python3`
    and the server dies on `ModuleNotFoundError: numpy`. Prefer, in order: an
    explicit HALO_REPO_ROOT override, the git common dir's parent (the main
    worktree shared by every linked worktree), the file-relative root, then the
    pinned dev-box path — accepting the first that carries both
    `.venv/bin/python3` and `tools/retrieval/`."""
    here = Path(__file__).resolve()
    candidates: list[Path] = []
    env = os.environ.get("HALO_REPO_ROOT")
    if env:
        candidates.append(Path(env))
    try:
        r = subprocess.run(
            ["git", "rev-parse", "--git-common-dir"],
            cwd=here.parent, capture_output=True, text=True, timeout=3,
        )
        if r.returncode == 0 and r.stdout.strip():
            common = Path(r.stdout.strip())
            if not common.is_absolute():
                common = (here.parent / common).resolve()
            candidates.append(common.parent)
    except Exception:
        pass
    candidates.append(here.parent.parent.parent)
    candidates.append(Path("/mnt/g/dev/halo"))
    for c in candidates:
        try:
            if (c / ".venv" / "bin" / "python3").exists() and (c / "tools" / "retrieval").is_dir():
                return c
        except OSError:
            pass
    return here.parent.parent.parent


REPO_ROOT = _canonical_repo_root()
SOCK_PATH = Path("/tmp/retrieval_server.sock")
KB_PATH = REPO_ROOT / "kb.json"

MIN_DECOMP_CHARS = 80
MAX_INJECT_CHARS = 4000
NEIGHBOR_BUDGET = 2000
HAZARD_BUDGET = 1000
SECTION_BUDGET = 1000

_kb_cache = None


def _load_kb_cache() -> dict:
    """Load a lightweight addr→{obj_name, source} mapping from kb.json."""
    global _kb_cache
    if _kb_cache is not None:
        return _kb_cache
    try:
        raw = json.loads(KB_PATH.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        _kb_cache = {}
        return _kb_cache
    mapping = {}
    for obj in raw.get("objects", []):
        obj_name = obj.get("name", "")
        source = obj.get("source", "")
        for fn in obj.get("functions", []):
            addr = (fn.get("addr") or "").lower()
            if addr:
                mapping[addr] = {"obj_name": obj_name, "source": source}
    _kb_cache = mapping
    return _kb_cache


def _extract_target_addr(payload: dict) -> str | None:
    """Try to extract the target address from a Ghidra tool input."""
    tool_input = payload.get("tool_input") or payload.get("input") or {}
    if isinstance(tool_input, str):
        try:
            tool_input = json.loads(tool_input)
        except json.JSONDecodeError:
            pass
    if isinstance(tool_input, dict):
        addr = tool_input.get("address") or tool_input.get("addr") or ""
        if addr:
            if not addr.startswith("0x"):
                addr = "0x" + addr
            return addr.lower()
    return None


def _lookup_target_context(payload: dict) -> tuple[str, str]:
    """Return (obj_name, source_path) for the decompiled function."""
    addr = _extract_target_addr(payload)
    if not addr:
        return "", ""
    kb = _load_kb_cache()
    info = kb.get(addr, {})
    return info.get("obj_name", ""), info.get("source", "")


def _extract_text(response: object) -> str:
    """Pull plain text from an MCP tool response (various shapes)."""
    if isinstance(response, str):
        return response
    if isinstance(response, list):
        parts = []
        for block in response:
            if isinstance(block, dict):
                text = block.get("text") or block.get("content", "")
                if text:
                    parts.append(str(text))
        return "\n".join(parts)
    if isinstance(response, dict):
        inner = response.get("content") or response.get("text", "")
        return _extract_text(inner) if inner != response else ""
    return ""


def _query_via_server(text: str, top_k: int = 3,
                      obj_name: str = "",
                      include_hazards: bool = True) -> dict | None:
    """Query the persistent server via Unix socket.

    Returns {"markdown": "...", "hazard_warnings": "..."} or None if
    the server is not running.
    """
    import socket as _socket
    if not SOCK_PATH.exists():
        return None
    try:
        sock = _socket.socket(_socket.AF_UNIX, _socket.SOCK_STREAM)
        sock.settimeout(10)
        sock.connect(str(SOCK_PATH))
        req = json.dumps({
            "text": text,
            "top_k": top_k,
            "obj_name": obj_name,
            "include_hazards": include_hazards,
        }).encode() + b"\n"
        sock.sendall(req)
        buf = b""
        while True:
            chunk = sock.recv(65536)
            if not chunk:
                break
            buf += chunk
            if b"\n" in buf:
                break
        sock.close()
        resp = json.loads(buf.split(b"\n")[0])
        if resp.get("ok"):
            return {
                "markdown": resp.get("markdown", ""),
                "hazard_warnings": resp.get("hazard_warnings", ""),
            }
        return {"markdown": "", "hazard_warnings": ""}
    except (OSError, json.JSONDecodeError, KeyError):
        return None


def _detect_and_format_sections(decomp_text: str) -> str:
    """Detect hazard patterns in pseudocode and return relevant section briefs."""
    try:
        from tools.retrieval.hazard_lookup import detect_hazards, format_hazard_briefs
        tags = detect_hazards(decomp_text)
        if tags:
            return format_hazard_briefs(tags, max_chars=SECTION_BUDGET)
    except ImportError:
        pass
    return ""


def main() -> int:
    try:
        payload = json.loads(sys.stdin.read())
    except (json.JSONDecodeError, OSError):
        return 0

    tool_name = payload.get("tool_name", "")
    if tool_name not in (
        "mcp__ghidra__decompile_function",
        "mcp__ghidra__batch_decompile",
        "mcp__ghidra__force_decompile",
    ):
        return 0

    response = (
        payload.get("tool_response")
        or payload.get("output")
        or payload.get("result")
        or {}
    )
    decomp_text = _extract_text(response)

    if not decomp_text or len(decomp_text) < MIN_DECOMP_CHARS:
        return 0

    obj_name, source_path = _lookup_target_context(payload)

    t0 = time.monotonic()
    result = _query_via_server(decomp_text, obj_name=obj_name, include_hazards=True)
    if result is None:
        server_py = REPO_ROOT / "tools" / "retrieval" / "server.py"
        venv_python = REPO_ROOT / ".venv" / "bin" / "python3"
        py = str(venv_python) if venv_python.exists() else "python3"
        log_path = Path("/tmp/retrieval_server.log")
        import subprocess as _sp
        _sp.Popen(
            [py, str(server_py)],
            stdout=open(log_path, "a"), stderr=_sp.STDOUT,
            cwd=str(REPO_ROOT),
            start_new_session=True,
        )
        print(
            json.dumps({"systemMessage": (
                "[retrieval-hook] Server not running — started in background "
                f"(log: {log_path}). "
                "It will auto-rebuild the index if stale, then serve queries."
            )}),
            flush=True,
        )
        return 0

    elapsed = time.monotonic() - t0

    neighbors_md = result.get("markdown", "")
    hazard_warnings_md = result.get("hazard_warnings", "")
    section_md = _detect_and_format_sections(decomp_text)

    parts = []
    budget_left = MAX_INJECT_CHARS

    if neighbors_md:
        chunk = neighbors_md[:NEIGHBOR_BUDGET]
        parts.append(chunk)
        budget_left -= len(chunk)

    if hazard_warnings_md and budget_left > 100:
        chunk = hazard_warnings_md[:min(HAZARD_BUDGET, budget_left)]
        parts.append(chunk)
        budget_left -= len(chunk)

    if section_md and budget_left > 100:
        chunk = section_md[:min(SECTION_BUDGET, budget_left)]
        parts.append(chunk)
        budget_left -= len(chunk)

    if parts:
        combined = "\n\n".join(parts)
        msg = (
            f"[retrieval-hook] ({elapsed:.1f}s) "
            f"Neighbors + hazard context for this target:\n\n{combined}"
        )
    else:
        msg = f"[retrieval-hook] No similar ported functions above threshold ({elapsed:.1f}s)."

    print(json.dumps({"systemMessage": msg}), flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
