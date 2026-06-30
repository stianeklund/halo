#!/usr/bin/env python3
"""Stop-hook: intercept "structural ceiling" declarations before they land.

When an agent's last assistant message mentions "structural ceiling" (or a
related phrase) WITHOUT having already applied the §19 rewrite checklist,
this hook blocks the Stop and injects the checklist reminder so the agent
tries the four techniques before giving up.

Hook protocol (Claude Code Stop / SubagentStop):
  - Reads JSON payload from stdin.
  - Payload may include "transcript_path" (JSONL) for transcript scanning.
  - Emits {"decision": "block", "reason": "..."} to prevent stop + inject msg.
  - Emits nothing (exit 0) to allow stop through.

Wired in .claude/settings.json Stop and SubagentStop events.
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

# ---------------------------------------------------------------------------
# Phrase detection
# ---------------------------------------------------------------------------

# Any of these in the last assistant turn triggers the check.
CEILING_PATTERNS = [
    r"structural\s+ceiling",
    r"structural\s+cap",
    r"codegen\s+ceiling",
    r"genuine\s+ceiling",
    r"true\s+ceiling",
    r"permanent\s+ceiling",
    r"permanently\s+capped",
    r"maximum\s+achievable\s+(?:match|score)",
    r"cannot\s+(?:be\s+)?improved\s+further",
    r"hit\s+the\s+ceiling",
    r"at\s+(?:the\s+)?ceiling",
    r"vc71\s+ceiling",
    r"match\s+ceiling",
]

# If any of these are already present, the agent has already applied the
# checklist — let it through to avoid an infinite loop.
ALREADY_CHECKED_PATTERNS = [
    r"lift-score-improve",
    r"§19",        # §19
    r"rewrite\s+checklist",
    r"cos\(\)\s*/\s*sin\(\)",
    r"DEC-chain",
    r"pointer-base\s+aliasing",
    r"early\s+register-?load",
    r"if.*else\s+if.*switch",
    r"structural_ceiling_hook",  # hook already ran this session
]

_CEILING_RE = re.compile(
    "|".join(CEILING_PATTERNS), re.IGNORECASE | re.DOTALL
)
_CHECKED_RE = re.compile(
    "|".join(ALREADY_CHECKED_PATTERNS), re.IGNORECASE | re.DOTALL
)

BLOCK_REASON = """\
STOP — "structural ceiling" detected. Before declaring a ceiling, apply the \
§19 rewrite checklist (lift-score-improve skill / docs/lift-learnings.md §19):

1. cos()/sin() intrinsification — if x87_fcos/x87_fsin helpers present, \
replace with standard cos()/sin() under #if defined(_MSC_VER) && !defined(__clang__).

2. Pointer-base aliasing — if 3+ consecutive stores share the same struct/object, \
declare a single `float *ptr` base and use ptr[0]/ptr[1]/ptr[2].

3. Early register-load hint — if a @<reg> param is first used far from entry, \
save it early: `float v0 = reg_param[0]`.

4. if-else-if → switch — if there's a dense ascending cascade (≥3 constants), \
convert to switch (DEC-chain → CMP-chain). \
Detector: grep -rnE '\\} else if \\([A-Za-z_]\\w* == [0-9]' src/

Run `vc71_verify.py --show-diffs` on the IMPROVED source after each step. \
Only after all four steps fail to improve (or the function is already ≥85%) \
should it be declared a genuine structural ceiling — with the specific \
unmatched instructions documented (FPU stack refs, FUCOMPP vs FCOMPS, \
@<reg> prologue).

Invoke the `lift-score-improve` skill for the full procedure.\
"""


# ---------------------------------------------------------------------------
# Transcript scanning
# ---------------------------------------------------------------------------

def _last_assistant_text(transcript_path: str) -> str:
    """Return the concatenated text of the last assistant turn in the JSONL."""
    path = Path(transcript_path)
    if not path.exists():
        return ""

    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    last_text_parts: list[str] = []
    # Walk backwards to find the last assistant message.
    for raw in reversed(lines):
        raw = raw.strip()
        if not raw:
            continue
        try:
            entry = json.loads(raw)
        except json.JSONDecodeError:
            continue
        role = entry.get("role") or entry.get("type", "")
        if role == "assistant":
            # Content can be a string or a list of content blocks.
            content = entry.get("content", "")
            if isinstance(content, str):
                last_text_parts.append(content)
            elif isinstance(content, list):
                for block in content:
                    if isinstance(block, dict) and block.get("type") == "text":
                        last_text_parts.append(block.get("text", ""))
            break  # only the last turn
        # If we hit a user message before an assistant, stop.
        if role in ("user", "human"):
            break

    return "\n".join(last_text_parts)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> int:
    try:
        payload = json.load(sys.stdin)
    except (json.JSONDecodeError, ValueError):
        return 0

    transcript_path = payload.get("transcript_path", "")
    if not transcript_path:
        return 0

    text = _last_assistant_text(transcript_path)
    if not text:
        return 0

    if not _CEILING_RE.search(text):
        return 0  # no ceiling phrase → pass through

    if _CHECKED_RE.search(text):
        return 0  # checklist already mentioned → pass through

    # Block and inject the reminder.
    print(json.dumps({"decision": "block", "reason": BLOCK_REASON}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
