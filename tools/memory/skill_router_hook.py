#!/usr/bin/env python3
"""Claude prompt hook: inject skill recommendations from local trigger words.

Reads a prompt from stdin JSON (Claude hook payload) or --prompt, matches Halo
RE/lift trigger phrases, and emits a compact system message. Silence means
"no route". This is the Claude-side counterpart to OpenCode's skill router.
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
STATE_PATH = REPO_ROOT / ".claude" / "agent-memory" / "skill_router_hook_state.json"
MAX_SYMBOLIZER_CHARS = 3600


EXCEPTION_RE = re.compile(
    r"\b(EXCEPTION|ACCESS[_ -]?VIOLATION|trap frame|halt in|EIP\s*[:=]|CR2\s*[:=]|"
    r"tag_groups\.c,#\d+)\b|^\s*\[[0-9]+\]\s*[:=]?\s*(?:0x)?[0-9a-f]{5,8}\b",
    re.IGNORECASE | re.MULTILINE,
)


SKILL_RULES: list[tuple[re.Pattern[str], list[str], str]] = [
    (
        re.compile(
            r"\b(lift|lifting|ported|porting|re[- ]?lift|FUN_[0-9a-f]{8}|"
            r"0x[0-9a-f]{5,}|ghidra|decompil|cachebeta|kb\.json)\b|@<[a-z]+>",
            re.IGNORECASE,
        ),
        ["halo-xbox-re", "halo-re-lift"],
        "Halo RE/lift workflow, binary evidence rules, ABI and kb.json discipline",
    ),
    (
        re.compile(
            r"\b(call[ -]?site|add esp|push|fstp|x87|cross[- ]?product|_ftol2|"
            r"_chkstk|__seh|_allmul|intrinsic|decompiler trap|ghidra.*wrong)\b",
            re.IGNORECASE,
        ),
        ["lift-decompiler-traps", "lift-arg-hazards"],
        "call-site verification, cdecl cleanup tells, FSTP float args, intrinsics, register aliasing",
    ),
    (
        re.compile(
            r"\b(register arg|reg arg|in_eax|in_ecx|in_edx|in_esi|in_edi|"
            r"callee regs?|unported callee|xcall|missing @)\b|@<",
            re.IGNORECASE,
        ),
        ["check-callee-regs", "lift-arg-hazards"],
        "implicit @<reg> ABI hazards and caller register setup checks",
    ),
    (
        re.compile(
            r"\b(_chkstk|stack frame|frame size|buffer size|undersized buffer|"
            r"local_[0-9a-f]+|memset|memcpy|stack alias|buffer alias)\b|&local_",
            re.IGNORECASE,
        ),
        ["lift-frame-hazards", "lift-decompiler-traps"],
        "stack-frame sizing, contiguous buffer rules, local_XX buffer-alias reads",
    ),
    (
        re.compile(
            r"\b(vc71|vc71_verify|low[- ]?match|match percent|structural ceiling|"
            r"objdiff|permuter|permute|permutation campaign)\b|85%|98%",
            re.IGNORECASE,
        ),
        ["halo-verify-debug", "lift-score-improve", "permuter-campaign"],
        "verification lanes, score recovery before declaring a ceiling, safe permuter campaign rules",
    ),
    (
        re.compile(
            r"\b(regression|crash|crashes|crashed|fault|page fault|access[_ -]?violation|"
            r"assert|hang|freeze|deadlock|wrong|broken|bug|visual|tint|color|"
            r"invisible|missing|no[- ]?draw|cull|spawn|position|build failure|"
            r"deploy failure|symbol absent|eip|cr2|trap frame|rasterizer)\b",
            re.IGNORECASE,
        ),
        ["debug", "crash-triage", "lift-crash-signals"],
        "runtime symptom router, crash signal table, toggle-bisect and liveness gates",
    ),
    (
        re.compile(
            r"\b(wrong color|yellow|white tint|invisible|missing geometry|no effect|"
            r"does nothing|wrong position|wrong scalar|silent bug|visual regression|"
            r"behavioral regression)\b",
            re.IGNORECASE,
        ),
        ["lift-silent-bugs", "bug-hunt"],
        "non-crashing correctness checks and automated hazard/silent-bug scans",
    ),
    (
        re.compile(
            r"\b(bug[- ]?hunt|hazard scan|check_lift_hazards|before deploy|"
            r"pre[- ]?deploy|pre[- ]?commit|safety scan|audit)\b",
            re.IGNORECASE,
        ),
        ["bug-hunt"],
        "tiered automated checks after edits, before deploy, and before commit",
    ),
    (
        re.compile(
            r"\b(input replay|deterministic input|capture scenario|halorec|trajectory|"
            r"a/b|ab check|behavior_diff|aa check)\b",
            re.IGNORECASE,
        ),
        ["input-replay-testing", "ab-trajectory-testing"],
        "deterministic input replay and A/B trajectory regression oracle",
    ),
    (
        re.compile(r"\b(xemu|qmp|gdb|screenshot|serial|memory dump|state snapshot)\b", re.IGNORECASE),
        ["debug-xemu"],
        "xemu QMP/GDB/screenshot/live-memory probing workflow",
    ),
]


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
        state = {k: v for k, v in state.items() if isinstance(v, (int, float)) and now - v < 86400}
        STATE_PATH.write_text(json.dumps(state, indent=2), encoding="utf-8")
    except OSError:
        pass
    return False


def matching_rules(prompt: str) -> list[tuple[list[str], str]]:
    matches = []
    for pattern, skills, why in SKILL_RULES:
        if pattern.search(prompt):
            matches.append((skills, why))
            if len(matches) >= 6:
                break
    return matches


def looks_like_exception(prompt: str) -> bool:
    return bool(EXCEPTION_RE.search(prompt))


def run_exception_symbolizer(prompt: str) -> str:
    cmd = [
        sys.executable,
        str(REPO_ROOT / "tools" / "xbox" / "symbolize_exception.py"),
        "--stdin",
    ]
    try:
        proc = subprocess.run(
            cmd,
            cwd=REPO_ROOT,
            input=prompt,
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            timeout=12,
        )
    except (OSError, subprocess.TimeoutExpired):
        return ""
    if proc.returncode != 0:
        return ""
    report = proc.stdout.strip()
    if len(report) > MAX_SYMBOLIZER_CHARS:
        report = report[: MAX_SYMBOLIZER_CHARS - 80].rstrip() + "\n... (symbolizer output truncated)"
    return report


def build_message(matches: list[tuple[list[str], str]], symbolizer_report: str = "") -> str:
    seen = set()
    ordered_skills = []
    route_lines = []
    for skills, why in matches:
        for skill in skills:
            if skill not in seen:
                seen.add(skill)
                ordered_skills.append(skill)
        route_lines.append(f"- {' + '.join(f'`{skill}`' for skill in skills)}: {why}")
    message = (
        "[skill-router] Local Halo trigger words matched. Before acting, load/use these "
        "skills if the Skill tool exposes them; otherwise read "
        "`.claude/skills/<skill>/SKILL.md` and follow its checklist. When delegating "
        "to a subagent, name these skills in the brief.\n\n"
        "Recommended skills: "
        + ", ".join(f"`{skill}`" for skill in ordered_skills)
        + "\n\nMatched routes:\n"
        + "\n".join(route_lines)
    )
    if symbolizer_report:
        message += (
            "\n\n[exception-symbolizer] Pasted exception/backtrace text detected. "
            "Use this fresh PE-export/kb.json symbolization before forming hypotheses; "
            "do not use `build/halo.map` for runtime crashes.\n\n"
            "```text\n"
            f"{symbolizer_report}\n"
            "```"
        )
    return message


def main() -> int:
    parser = argparse.ArgumentParser(description="Emit skill-router context for Halo trigger words.")
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

    if not prompt:
        return 0
    if recently_ran(prompt, args.dedupe_window):
        return 0

    matches = matching_rules(prompt)
    if not matches:
        return 0

    symbolizer_report = run_exception_symbolizer(prompt) if looks_like_exception(prompt) else ""
    message = build_message(matches, symbolizer_report)
    if args.surface == "text":
        print(message)
    else:
        print(json.dumps({"systemMessage": message}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
