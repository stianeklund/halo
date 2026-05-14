#!/usr/bin/env python3
"""Token-discipline hook for Claude Code.

Wires the rules in CLAUDE.md ("Token Discipline" section) into runtime
feedback so the agent sees its read/edit ratio and gets warned about
repeat reads instead of relying on "track mentally."

Hook protocol (Claude Code):
- Reads JSON payload from stdin: {tool_name, tool_input, session_id, hook_event_name, ...}
- Writes JSON to stdout: {"systemMessage": "..."} when there's something to say.

Wired in `.claude/settings.json` to:
- PostToolUse on Read|Edit|Write: update counts, emit ratio warning on
  drift, emit duplicate-read warning when (path, offset, limit) repeats.
- Stop: emit final session summary.

State file: `.claude/agent-memory/token_discipline/<session_id>.json`.
"""

from __future__ import annotations

import json
import os
import sys
from datetime import datetime
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
STATE_DIR = REPO_ROOT / ".claude" / "agent-memory" / "token_discipline"

# Tunables. CLAUDE.md targets a 4:1 read/edit ratio; warn below this after a
# minimum number of edits so the first few hits are not noise.
TARGET_RATIO = 4.0
MIN_EDITS_BEFORE_RATIO_WARN = 5
RATIO_WARN_COOLDOWN = 5

# Files we never want to count as "research reads" — generated artifacts,
# logs, and binaries that CLAUDE.md already bans. Reading them is the
# problem, so they should not credit the read/edit ratio.
NOISY_PATH_FRAGMENTS = (
    "/build/", "/build_debug/", "/node_modules/", "/.git/",
    "/halo-patched/", "/__pycache__/", "/dist/",
)


def _state_path(session_id: str) -> Path:
    safe = "".join(c if c.isalnum() or c in ("-", "_") else "_" for c in session_id)
    if not safe:
        safe = "default"
    return STATE_DIR / f"{safe}.json"


def _load_state(session_id: str) -> dict:
    p = _state_path(session_id)
    if p.exists():
        try:
            return json.loads(p.read_text(encoding="utf-8"))
        except json.JSONDecodeError:
            pass
    return {
        "session_id": session_id,
        "started": datetime.now().isoformat(timespec="seconds"),
        "reads": 0,
        "noisy_reads": 0,
        "edits": 0,
        "writes": 0,
        "repeat_reads": 0,
        "ranges": {},  # "<path>:<offset>:<limit>" -> count
        "files_read": {},  # path -> count
        "last_event": "",
        "last_ratio_warn_at": 0,
    }


def _save_state(session_id: str, state: dict) -> None:
    STATE_DIR.mkdir(parents=True, exist_ok=True)
    p = _state_path(session_id)
    tmp = p.with_suffix(".tmp")
    tmp.write_text(json.dumps(state, indent=2) + "\n", encoding="utf-8")
    tmp.replace(p)


def _is_noisy(path: str) -> bool:
    norm = path.replace("\\", "/")
    return any(frag in norm for frag in NOISY_PATH_FRAGMENTS) or norm.endswith(".log")


def _ratio_message(state: dict) -> str | None:
    research_reads = state["reads"] - state["noisy_reads"]
    total_writes = state["edits"] + state["writes"]
    if total_writes < MIN_EDITS_BEFORE_RATIO_WARN:
        return None
    ratio = research_reads / total_writes if total_writes else 0.0
    if ratio < TARGET_RATIO:
        last = state.get("last_ratio_warn_at", 0)
        if total_writes - last < RATIO_WARN_COOLDOWN:
            return None
        state["last_ratio_warn_at"] = total_writes
        return (
            f"Token-discipline: read/edit ratio {ratio:.1f}:1 "
            f"({research_reads} research reads / {total_writes} edits) — "
            f"below {TARGET_RATIO:.0f}:1 target. CLAUDE.md: do more research "
            f"before editing (rg/jq for callers + read narrow line ranges)."
        )
    return None


def _record_read(state: dict, file_path: str, offset, limit) -> str | None:
    state["reads"] += 1
    if _is_noisy(file_path):
        state["noisy_reads"] += 1
        return (
            f"Token-discipline: read of noisy path {file_path}. CLAUDE.md "
            f"bans reads in build/log/generated dirs — run the command or "
            f"grep instead."
        )

    norm = file_path.replace("\\", "/")
    if norm.endswith("/kb.json") or norm == "kb.json":
        state["noisy_reads"] += 1
        return (
            f"Token-discipline: direct Read of kb.json. CLAUDE.md: use "
            f"`rtk jq` ONLY for kb.json queries (6000+ lines, historically "
            f"239 redundant reads = ~143K wasted tokens)."
        )

    state["files_read"][file_path] = state["files_read"].get(file_path, 0) + 1

    key = f"{file_path}:{offset}:{limit}"
    state["ranges"][key] = state["ranges"].get(key, 0) + 1
    if state["ranges"][key] > 1:
        state["repeat_reads"] += 1
        prior = state["ranges"][key] - 1
        # If an Edit happened on this file since the last read, the file may
        # have changed — but CLAUDE.md still says don't re-read to verify.
        return (
            f"Token-discipline: re-reading {file_path} "
            f"(offset={offset}, limit={limit}) — already read {prior}x this "
            f"session. CLAUDE.md Read-Once Rule: the Edit tool confirms "
            f"success. Recall from context or rg for the specific string."
        )
    return None


def _record_edit_or_write(state: dict, tool_name: str) -> None:
    if tool_name == "Edit":
        state["edits"] += 1
    elif tool_name == "Write":
        state["writes"] += 1


def _summary(state: dict) -> str:
    research_reads = state["reads"] - state["noisy_reads"]
    total_writes = state["edits"] + state["writes"]
    ratio = research_reads / total_writes if total_writes else 0.0
    lines = [
        f"Token-discipline summary (session {state['session_id'][:8]}):",
        f"  reads:        {state['reads']} ({research_reads} research, "
        f"{state['noisy_reads']} noisy)",
        f"  edits/writes: {state['edits']} / {state['writes']}",
        f"  repeat reads: {state['repeat_reads']}",
        f"  ratio:        {ratio:.1f}:1 (target {TARGET_RATIO:.0f}:1)",
    ]
    return "\n".join(lines)


def main() -> int:
    try:
        payload = json.load(sys.stdin)
    except (json.JSONDecodeError, ValueError):
        return 0

    session_id = payload.get("session_id") or "default"
    hook_event = payload.get("hook_event_name", "")
    tool_name = payload.get("tool_name", "")
    tool_input = payload.get("tool_input") or {}

    state = _load_state(session_id)
    state["last_event"] = hook_event

    msgs: list[str] = []

    if hook_event == "PostToolUse":
        if tool_name == "Read":
            file_path = tool_input.get("file_path", "")
            offset = tool_input.get("offset", 0)
            limit = tool_input.get("limit", 0)
            if file_path:
                m = _record_read(state, file_path, offset, limit)
                if m:
                    msgs.append(m)
        elif tool_name in ("Edit", "Write"):
            _record_edit_or_write(state, tool_name)
            m = _ratio_message(state)
            if m:
                msgs.append(m)

    elif hook_event in ("Stop", "SubagentStop"):
        if state["reads"] + state["edits"] + state["writes"] > 0:
            msgs.append(_summary(state))

    _save_state(session_id, state)

    if msgs:
        print(json.dumps({"systemMessage": "\n".join(msgs)}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
