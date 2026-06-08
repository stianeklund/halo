#!/usr/bin/env python3
"""PostToolUse hook: inject retrieval neighbors after Ghidra decompile.

Wired in .claude/settings.json on mcp__ghidra__decompile_function and
mcp__ghidra__batch_decompile.

When the agent decompiles a function, this hook immediately embeds the output,
queries the index for similar already-ported functions, and returns them as a
systemMessage. The neighbors appear directly in the agent's context — no
manual query step required and no way to sidestep.

Exit codes: 0 always (errors are logged, not fatal).
Output: {"systemMessage": "..."} on stdout when neighbors are found,
        {"systemMessage": "..."} with a short "none found" note otherwise.
"""

from __future__ import annotations

import json
import sys
import time
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
QUERY_SCRIPT = REPO_ROOT / "tools" / "retrieval" / "query.py"
DECOMP_TMP = Path("/tmp/lift_target_decomp.txt")
NEIGHBORS_TMP = Path("/tmp/lift_retrieval_neighbors.md")
SOCK_PATH = Path("/tmp/retrieval_server.sock")

# Minimum decompile length to bother querying (noise filter)
MIN_DECOMP_CHARS = 80
# Max chars of neighbor markdown to inject into systemMessage
MAX_INJECT_CHARS = 4000


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
        # Some MCP servers wrap in {"content": [...]} or {"text": "..."}
        inner = response.get("content") or response.get("text", "")
        return _extract_text(inner) if inner != response else ""
    return ""


def _query_via_server(text: str, top_k: int = 3) -> str | None:
    """Query the persistent server via Unix socket.

    Returns the markdown string (possibly empty if no neighbors), or None if
    the server is not running.
    """
    import socket as _socket
    if not SOCK_PATH.exists():
        return None
    try:
        sock = _socket.socket(_socket.AF_UNIX, _socket.SOCK_STREAM)
        sock.settimeout(10)
        sock.connect(str(SOCK_PATH))
        req = json.dumps({"text": text, "top_k": top_k}).encode() + b"\n"
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
            return resp.get("markdown", "")
        return ""
    except (OSError, json.JSONDecodeError, KeyError):
        return None


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

    # Extract decompile text from tool response
    response = (
        payload.get("tool_response")
        or payload.get("output")
        or payload.get("result")
        or {}
    )
    decomp_text = _extract_text(response)

    if not decomp_text or len(decomp_text) < MIN_DECOMP_CHARS:
        return 0

    DECOMP_TMP.write_text(decomp_text, encoding="utf-8")

    t0 = time.monotonic()
    neighbors_md = _query_via_server(decomp_text)
    if neighbors_md is None:
        # Server not running — start it in background (auto-rebuilds if stale)
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
    NEIGHBORS_TMP.write_text(neighbors_md, encoding="utf-8")

    if neighbors_md:
        inject = neighbors_md[:MAX_INJECT_CHARS]
        if len(neighbors_md) > MAX_INJECT_CHARS:
            inject += f"\n\n... (truncated; full output at {NEIGHBORS_TMP})"
        msg = (
            f"[retrieval-hook] Similar already-ported functions ({elapsed:.1f}s). "
            f"Use these as worked examples for the C implementation:\n\n{inject}"
        )
    else:
        msg = f"[retrieval-hook] No similar ported functions above threshold ({elapsed:.1f}s)."

    print(json.dumps({"systemMessage": msg}), flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
