#!/usr/bin/env python3
"""PreToolUse hook: route doctrine skills off the ACTION being taken, not the
user's phrasing.

The prompt-side router (skill_router_hook.py) fires on trigger words in the
user's message. But the moment `lift-silent-bugs` / `bug-hunt` matter most is
when the assistant is about to DEPLOY or COMMIT a lift — which has nothing to do
with how the user phrased anything. This hook watches Bash commands and injects a
terse reminder at exactly that moment.

Two modes per action:
  "block"  — deny-once-with-reason: the FIRST matching call inside the dedupe
             window is denied with the gate checklist as the reason; the retry
             passes (the deny records the dedupe stamp). Guarantees the model
             actually sees the gate — a plain systemMessage is droppable by
             output-filtering wrappers, a permissionDecision is not.
  "advise" — non-blocking {"systemMessage": ...} as before.

Deduped so the same action doesn't nag repeatedly. Silent for everything else.
NOTE: hook commands in .claude/settings.json must NOT be wrapped in `rtk` —
the proxy filters stdout, which silently ate every hook message (2026-08-08).
"""

from __future__ import annotations

import hashlib
import json
import re
import sys
import time
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
STATE_PATH = REPO_ROOT / ".claude" / "agent-memory" / "action_skill_router_state.json"
DEDUPE_WINDOW = 300  # seconds

# (category, mode, command regex, reminder)
ACTIONS: list[tuple[str, str, re.Pattern[str], str]] = [
    (
        "deploy",
        "block",
        # Match actual deploy invocations only — a bare \bdeploy\b also matched
        # the word inside commit messages / echoed text (live false positive
        # 2026-08-08) and denied unrelated commands.
        re.compile(r"build_deploy_run|--xbe-only|xbdm.*(send|setmem)|deploy\.(py|sh)|(^|\s)/deploy\b", re.IGNORECASE),
        "[skill-router:deploy] GATE (deny-once; rerun the same command to proceed). "
        "The box is the only oracle for SILENT (non-crashing) lift bugs — wrong "
        "color/tint, invisible geometry, no-op effects. Before rerunning: apply "
        "`lift-silent-bugs` (§6/§8/§11/§16/§17 checklist) and confirm `bug-hunt` is "
        "clean on your changed files.",
    ),
    (
        "commit",
        "block",
        re.compile(r"git\s+commit", re.IGNORECASE),
        "[skill-router:commit] GATE (deny-once; rerun the same command to proceed). "
        "If this commit includes lift/score work, confirm before rerunning: (1) "
        "check_lift_hazards --staged-only clean, (2) VC71/score gate ran "
        "(lift_pipeline or score_improve check), (3) for new lifts the /lift or "
        "/auto-lift route was used and xbox-halo-lift-reviewer has reviewed the "
        "result. If all already done, just rerun the command.",
    ),
    (
        # The lift-* hazard skills are keyed on mid-analysis jargon (`add esp`,
        # `fstp`, `&local_`) that never appears in an opening prompt, so the
        # prompt-side router almost never surfaces them. Verifying a lift is the
        # action-time moment they matter — when VC71 comes back low, the fix is a
        # call-site / arg / buffer-frame audit. Surface them here as a backstop.
        "lift-verify",
        "advise",
        re.compile(r"vc71_verify|lift_pipeline\.py|objdiff_lift|/verify\b", re.IGNORECASE),
        "[skill-router:lift-verify] Verifying a lift. If the match is low or a call "
        "site looks off, apply `lift-decompiler-traps` (covering register aliasing, "
        "push-then-fstp, struct rotation, cross-product swap, cdecl ADD ESP mis-grouping, "
        "NULL @<reg> args, caller-site register swaps, and _chkstk frame sizing) "
        "before declaring a ceiling.",
    ),
]


def _recently_ran(key: str) -> bool:
    digest = hashlib.sha256(key.encode("utf-8", "replace")).hexdigest()[:16]
    try:
        state = json.loads(STATE_PATH.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        state = {}
    now = time.time()
    last = state.get(digest)
    if isinstance(last, (int, float)) and now - last < DEDUPE_WINDOW:
        return True
    try:
        STATE_PATH.parent.mkdir(parents=True, exist_ok=True)
        state[digest] = now
        state = {k: v for k, v in state.items() if isinstance(v, (int, float)) and now - v < 86400}
        STATE_PATH.write_text(json.dumps(state, indent=2), encoding="utf-8")
    except OSError:
        pass
    return False


def main() -> int:
    try:
        payload = json.loads(sys.stdin.read() or "{}")
    except json.JSONDecodeError:
        return 0
    if payload.get("tool_name") != "Bash":
        return 0
    command = ""
    ti = payload.get("tool_input")
    if isinstance(ti, dict):
        command = str(ti.get("command", ""))
    if not command:
        return 0

    for category, mode, pattern, message in ACTIONS:
        if pattern.search(command):
            if _recently_ran(category):
                return 0
            if mode == "block":
                print(json.dumps({
                    "hookSpecificOutput": {
                        "hookEventName": "PreToolUse",
                        "permissionDecision": "deny",
                        "permissionDecisionReason": message,
                    }
                }))
            else:
                print(json.dumps({"systemMessage": message}))
            return 0
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
