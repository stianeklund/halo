#!/usr/bin/env python3
"""PreToolUse hook: route doctrine skills off the ACTION being taken, not the
user's phrasing.

The prompt-side router (skill_router_hook.py) fires on trigger words in the
user's message. But the moment `lift-silent-bugs` / `bug-hunt` matter most is
when the assistant is about to DEPLOY or COMMIT a lift — which has nothing to do
with how the user phrased anything. This hook watches Bash commands and injects a
terse reminder at exactly that moment.

Non-blocking: emits {"systemMessage": ...} and exits 0 (never denies the tool).
Deduped so the same action doesn't nag repeatedly. Silent for everything else.
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

# (category, command regex, skills, reminder)
ACTIONS: list[tuple[str, re.Pattern[str], str]] = [
    (
        "deploy",
        re.compile(r"build_deploy_run|--xbe-only|xbdm.*(send|setmem)|\bdeploy\b|/deploy", re.IGNORECASE),
        "[skill-router:deploy] About to deploy to Xbox. The box is the only oracle for "
        "SILENT (non-crashing) lift bugs — wrong color/tint, invisible geometry, no-op "
        "effects. Apply `lift-silent-bugs` (its §6/§8/§11/§16/§17 checklist) and confirm "
        "`bug-hunt` is clean on your changed files BEFORE this deploy.",
    ),
    (
        "commit",
        re.compile(r"git\s+commit", re.IGNORECASE),
        "[skill-router:commit] Committing. If this includes a lift, ensure `bug-hunt` "
        "(hazard + ABI + reg-arg scan) ran clean on the changed files; amend if it "
        "surfaces anything.",
    ),
    (
        # The lift-* hazard skills are keyed on mid-analysis jargon (`add esp`,
        # `fstp`, `&local_`) that never appears in an opening prompt, so the
        # prompt-side router almost never surfaces them. Verifying a lift is the
        # action-time moment they matter — when VC71 comes back low, the fix is a
        # call-site / arg / buffer-frame audit. Surface them here as a backstop.
        "lift-verify",
        re.compile(r"vc71_verify|lift_pipeline\.py|objdiff_lift|/verify\b", re.IGNORECASE),
        "[skill-router:lift-verify] Verifying a lift. If the match is low or a call "
        "site looks off, apply the hazard family before declaring a ceiling: "
        "`lift-arg-hazards` (cdecl mis-group / ADD ESP / @<reg> order), "
        "`lift-decompiler-traps` (register aliasing, push-then-fstp, struct rotation, "
        "cross-product swap, buffer-alias), and `lift-frame-hazards` (_chkstk buffer "
        "sizing, stack aliasing).",
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

    for category, pattern, message in ACTIONS:
        if pattern.search(command):
            if _recently_ran(category):
                return 0
            print(json.dumps({"systemMessage": message}))
            return 0
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
