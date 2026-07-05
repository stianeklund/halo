#!/usr/bin/env python3
"""Claude prompt hook: inject skill recommendations from local trigger words.

Reads a prompt from stdin JSON (Claude hook payload) or --prompt, matches Halo
RE/lift trigger phrases, and emits a compact system message. Silence means
"no route". This is the Claude-side counterpart to OpenCode's skill router.

Routing rules are the SINGLE SOURCE OF TRUTH in each skill's own frontmatter:
`.claude/skills/<name>/SKILL.md` declares `tier:` (user|agent) and, for agent
skills, `triggers: [...]`. Only `tier: agent` skills are auto-routed. Compiled
rules are cached (keyed on the newest SKILL.md mtime) so the hook stays fast and
adding a skill needs no edit here. Seed frontmatter with
tools/memory/migrate_skill_frontmatter.py.
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
SKILLS_DIR = REPO_ROOT / ".claude" / "skills"
STATE_PATH = REPO_ROOT / ".claude" / "agent-memory" / "skill_router_hook_state.json"
RULES_CACHE_PATH = REPO_ROOT / ".claude" / "agent-memory" / "skill_router_rules_cache.json"
MAX_SYMBOLIZER_CHARS = 3600
MAX_ROUTES = 6


EXCEPTION_RE = re.compile(
    r"\b(EXCEPTION|ACCESS[_ -]?VIOLATION|trap frame|halt in|EIP\s*[:=]|CR2\s*[:=]|"
    r"tag_groups\.c,#\d+)\b|^\s*\[[0-9]+\]\s*[:=]?\s*(?:0x)?[0-9a-f]{5,8}\b",
    re.IGNORECASE | re.MULTILINE,
)


# ── Frontmatter-driven rule loading ─────────────────────────────────────────

def _kw_to_regex(kw: str) -> str:
    """Escape a trigger keyword, adding \\b only where an edge is a word char.

    Leading/trailing '_' and symbols (e.g. `_ftol2`, `@<`, `local_`, `65%`) get
    no boundary on that side, so prefix matches like `local_` -> `local_44` work.
    """
    esc = re.escape(kw)
    pre = r"\b" if kw[:1].isalnum() else ""
    post = r"\b" if kw[-1:].isalnum() else ""
    return pre + esc + post


def _parse_frontmatter(text: str) -> dict:
    """Extract tier/triggers/description from a SKILL.md frontmatter block.

    Only single-line `tier:` and `triggers:` are needed (the migration writes them
    that way). `description` may be inline or a folded (`>-`) block; we grab a short
    one-liner for the route hint.
    """
    if not text.startswith("---"):
        return {}
    end = text.find("\n---", 3)
    if end == -1:
        return {}
    lines = text[3:end].splitlines()
    out: dict = {"tier": "", "triggers": [], "description": ""}
    for i, ln in enumerate(lines):
        s = ln.strip()
        if s.startswith("tier:"):
            out["tier"] = s[len("tier:"):].strip()
        elif s.startswith("triggers:"):
            val = s[len("triggers:"):].strip()
            try:
                parsed = json.loads(val)
                if isinstance(parsed, list):
                    out["triggers"] = [str(x) for x in parsed]
            except (json.JSONDecodeError, ValueError):
                pass
        elif s.startswith("description:") and not out["description"]:
            val = s[len("description:"):].strip()
            if val and val not in (">", ">-", "|", "|-"):
                out["description"] = val
            else:
                # folded/blocked — take the first non-empty continuation line
                for cont in lines[i + 1:]:
                    c = cont.strip()
                    if c:
                        out["description"] = c
                        break
    return out


def _short_why(description: str) -> str:
    """First clause of the description, capped — keeps the injected message lean."""
    d = description.strip()
    for sep in (". ", " — ", "; "):
        if sep in d:
            d = d.split(sep, 1)[0]
            break
    return (d[:90].rstrip() + "…") if len(d) > 90 else d


def _skill_mtimes() -> tuple[float, list[Path]]:
    paths = sorted(SKILLS_DIR.glob("*/SKILL.md"))
    newest = max((p.stat().st_mtime for p in paths), default=0.0)
    return newest, paths


def _build_rules(paths: list[Path]) -> list[dict]:
    rules: list[dict] = []
    for p in paths:
        try:
            fm = _parse_frontmatter(p.read_text(encoding="utf-8"))
        except OSError:
            continue
        if fm.get("tier") != "agent":
            continue
        triggers = fm.get("triggers") or []
        if not triggers:
            continue
        pattern = "|".join(_kw_to_regex(k) for k in triggers if k)
        if not pattern:
            continue
        rules.append({
            "skill": p.parent.name,
            "pattern": pattern,
            "why": _short_why(fm.get("description", "")),
        })
    return rules


def load_rules() -> list[tuple[re.Pattern[str], str, str]]:
    """Return [(compiled_pattern, skill, why)], rebuilding the cache if stale."""
    newest, paths = _skill_mtimes()
    raw_rules: list[dict] | None = None
    try:
        cached = json.loads(RULES_CACHE_PATH.read_text(encoding="utf-8"))
        if isinstance(cached, dict) and cached.get("mtime") == newest:
            raw_rules = cached.get("rules")
    except (OSError, json.JSONDecodeError):
        raw_rules = None
    if raw_rules is None:
        raw_rules = _build_rules(paths)
        try:
            RULES_CACHE_PATH.parent.mkdir(parents=True, exist_ok=True)
            RULES_CACHE_PATH.write_text(
                json.dumps({"mtime": newest, "rules": raw_rules}, indent=2),
                encoding="utf-8",
            )
        except OSError:
            pass
    compiled: list[tuple[re.Pattern[str], str, str]] = []
    for r in raw_rules:
        try:
            compiled.append((re.compile(r["pattern"], re.IGNORECASE), r["skill"], r.get("why", "")))
        except (re.error, KeyError):
            continue
    return compiled


# ── Prompt extraction / dedupe ──────────────────────────────────────────────

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


def matching_rules(prompt: str) -> list[tuple[str, str]]:
    matches: list[tuple[str, str]] = []
    for pattern, skill, why in load_rules():
        if pattern.search(prompt):
            matches.append((skill, why))
            if len(matches) >= MAX_ROUTES:
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


def build_message(matches: list[tuple[str, str]], symbolizer_report: str = "") -> str:
    skills = [skill for skill, _ in matches]
    route_lines = [f"- `{skill}`: {why}" if why else f"- `{skill}`" for skill, why in matches]
    message = (
        "[skill-router] Local Halo trigger words matched. These are agent doctrine "
        "you self-invoke — load/use them via the Skill tool if surfaced, else read "
        "`.claude/skills/<skill>/SKILL.md` and follow its checklist. Name them in any "
        "subagent brief.\n\n"
        "Recommended: " + ", ".join(f"`{s}`" for s in skills)
        + "\n\nWhy:\n" + "\n".join(route_lines)
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
    parser.add_argument("--self-test", action="store_true", help="run built-in checks and exit")
    args = parser.parse_args()

    if args.self_test:
        return _self_test()

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


def _self_test() -> int:
    rules = load_rules()
    ok = True

    def check(cond: bool, msg: str) -> None:
        nonlocal ok
        ok = ok and cond
        print(("PASS" if cond else "FAIL") + ": " + msg)

    check(len(rules) >= 15, f"loaded {len(rules)} agent rules from frontmatter")
    skills = {s for _, s, _ in rules}
    check("crash-triage" in skills, "crash-triage routed")
    check("check-callee-regs" in skills, "check-callee-regs routed")
    # user-tier skills must NOT be routed
    check("handover" not in skills, "handover (user tier) not routed")
    check("replay-input" not in skills, "replay-input (user tier) not routed")

    def routes(prompt: str) -> set[str]:
        return {s for s, _ in matching_rules(prompt)}

    check("check-callee-regs" in routes("the decompile shows in_EAX for the callee"),
          "in_EAX -> check-callee-regs")
    check("lift-frame-hazards" in routes("passing &local_44 into the callee"),
          "local_44 -> lift-frame-hazards")
    check("crash-triage" in routes("got an ACCESS_VIOLATION at boot"),
          "ACCESS_VIOLATION -> crash-triage")
    check("permuter-campaign" in routes("stuck at 92% VC71, run the permuter"),
          "permuter -> permuter-campaign")
    check(routes("please refactor the readme wording") == set(),
          "unrelated prompt routes nothing")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
