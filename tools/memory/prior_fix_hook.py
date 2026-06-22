#!/usr/bin/env python3
"""Claude/OpenCode prior-fix hook helper.

Reads a prompt from stdin JSON (Claude hook payload) or --prompt, detects
debug/regression-like tasks, runs prior_fixes.py, and emits a compact system
message.  Silence means "not a debug task".
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
import sys
import time
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent.parent.parent
STATE_PATH = REPO_ROOT / ".claude" / "agent-memory" / "prior_fix_hook_state.json"
MAX_MESSAGE_CHARS = 4200

DEBUG_RE = re.compile(
    r"\b("
    r"regression|crash|crashes|crashed|fault|page fault|access[_ -]?violation|"
    r"assert|hang|freeze|deadlock|wrong|broken|bug|visual|tint|color|"
    r"invisible|missing|no[- ]?draw|cull|spawn|position|build failure|"
    r"deploy failure|symbol absent|eip|cr2|trap frame|rasterizer"
    r")\b",
    re.IGNORECASE,
)


def extract_prompt(payload: dict) -> str:
    for key in ("prompt", "message", "text", "input"):
        value = payload.get(key)
        if isinstance(value, str) and value.strip():
            return value.strip()
    if isinstance(payload.get("messages"), list):
        for msg in reversed(payload["messages"]):
            if not isinstance(msg, dict):
                continue
            if msg.get("role") not in ("user", "human"):
                continue
            text = message_text(msg)
            if text:
                return text
    return ""


def message_text(message: dict) -> str:
    content = message.get("content")
    if isinstance(content, str):
        return content.strip()
    if isinstance(content, list):
        parts = []
        for part in content:
            if isinstance(part, str):
                parts.append(part)
            elif isinstance(part, dict):
                text = part.get("text") or part.get("content")
                if isinstance(text, str):
                    parts.append(text)
        return "\n".join(parts).strip()
    parts = message.get("parts")
    if isinstance(parts, list):
        chunks = []
        for part in parts:
            if isinstance(part, dict):
                text = part.get("text") or part.get("content")
                if isinstance(text, str):
                    chunks.append(text)
        return "\n".join(chunks).strip()
    return ""


def recently_ran(prompt: str, window_seconds: int) -> bool:
    digest = hashlib.sha256(prompt.encode("utf-8", errors="replace")).hexdigest()[:16]
    try:
        state = json.loads(STATE_PATH.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        state = {}
    now = time.time()
    last = state.get(digest)
    if isinstance(last, (int, float)) and now - last < window_seconds:
        return True
    try:
        STATE_PATH.parent.mkdir(parents=True, exist_ok=True)
        state[digest] = now
        # Keep the state tiny and naturally self-cleaning.
        state = {k: v for k, v in state.items() if isinstance(v, (int, float)) and now - v < 86400}
        STATE_PATH.write_text(json.dumps(state, indent=2), encoding="utf-8")
    except OSError:
        pass
    return False


def run_prior_fixes(prompt: str) -> str:
    cmd = [
        sys.executable,
        str(REPO_ROOT / "tools" / "memory" / "prior_fixes.py"),
        prompt,
        "--limit", "6",
        "--max-commits", "160",
    ]
    try:
        proc = subprocess.run(
            cmd,
            cwd=REPO_ROOT,
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            timeout=18,
        )
    except (OSError, subprocess.TimeoutExpired):
        return ""
    if proc.returncode != 0:
        return ""
    return proc.stdout.strip()


def hook_message(report: str) -> str:
    if len(report) > MAX_MESSAGE_CHARS:
        report = report[:MAX_MESSAGE_CHARS - 80].rstrip() + "\n... (prior-fix lookup truncated)"
    return (
        "[prior-fix-router] This looks like a debug/regression task. "
        "Use these local leads before investigating. Matches are hypotheses only; "
        "confirm against binary/disassembly/runtime evidence before fixing.\n\n"
        f"{report}"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="Emit prior-fix context for debug-like prompts.")
    parser.add_argument("--prompt", help="prompt text; otherwise read hook JSON from stdin")
    parser.add_argument("--surface", default="claude", help="claude or text")
    parser.add_argument("--dedupe-window", type=int, default=180)
    args = parser.parse_args()

    prompt = args.prompt or ""
    if not prompt:
        try:
            payload = json.loads(sys.stdin.read() or "{}")
        except json.JSONDecodeError:
            payload = {}
        prompt = extract_prompt(payload)

    if not prompt or not DEBUG_RE.search(prompt):
        return 0
    if recently_ran(prompt, args.dedupe_window):
        return 0

    report = run_prior_fixes(prompt)
    if not report:
        return 0
    message = hook_message(report)
    if args.surface == "text":
        print(message)
    else:
        print(json.dumps({"systemMessage": message}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
