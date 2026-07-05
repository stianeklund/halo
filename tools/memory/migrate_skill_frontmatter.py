#!/usr/bin/env python3
"""One-shot authoring tool: seed `tier` + `triggers` into each SKILL.md frontmatter.

After this runs, the frontmatter is the SINGLE SOURCE OF TRUTH for skill routing:
  - `tier: user`  — skills the user invokes with /<name>; NOT auto-routed.
  - `tier: agent` — doctrine the agent self-invokes during work; auto-routed by
    tools/memory/skill_router_hook.py from the `triggers` keyword list.

Idempotent: skips any SKILL.md that already has a `tier:` line (so manual edits to
triggers are never clobbered). Re-run with --force to overwrite.

The CURATED map below is the one authoring step. It is derived from the old
hardcoded SKILL_RULES + each skill's description. It is NOT read at runtime — the
router reads the frontmatter. Keep it only as the migration record.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
SKILLS_DIR = REPO_ROOT / ".claude" / "skills"

# skill dir name -> (tier, [trigger keywords/phrases])
# tier:user skills get no triggers (invoked explicitly by the user).
CATALOG: dict[str, tuple[str, list[str]]] = {
    # ── user commands (invoked by the user, never auto-routed) ──────────────
    "capture-input": ("user", []),
    "clear-cache":   ("user", []),
    "handover":      ("user", []),
    "replay-input":  ("user", []),
    # ── agent doctrine (self-invoked; auto-routed from triggers) ────────────
    "halo-xbox-re": ("agent", [
        "lift", "lifting", "ported", "porting", "ghidra", "decompile", "decompil",
        "cachebeta", "kb.json", "binary evidence", "@<reg>", "reverse engineer",
    ]),
    "halo-re-lift": ("agent", [
        "lift", "lifting", "re-lift", "relift", "abi", "kb.json", "@<reg>", "port function",
    ]),
    "lift-decompiler-traps": ("agent", [
        "call site", "call-site", "add esp", "push", "fstp", "x87", "cross product",
        "cross-product", "_ftol2", "_chkstk", "__seh", "_allmul", "intrinsic",
        "decompiler trap", "ghidra wrong", "struct field rotation",
    ]),
    "lift-arg-hazards": ("agent", [
        "call site", "call-site", "add esp", "push", "fstp", "cdecl", "arg hazard",
        "argument order", "arg order", "operand swap",
    ]),
    "check-callee-regs": ("agent", [
        "register arg", "reg arg", "in_eax", "in_ecx", "in_edx", "in_esi", "in_edi",
        "callee regs", "unported callee", "xcall", "missing @", "@<reg>", "@<",
    ]),
    "lift-frame-hazards": ("agent", [
        "_chkstk", "stack frame", "frame size", "buffer size", "undersized buffer",
        "local_", "memset", "memcpy", "stack alias", "buffer alias", "&local_",
    ]),
    "halo-verify-debug": ("agent", [
        "vc71", "vc71_verify", "low match", "low-match", "match percent", "objdiff",
        "delink", "delinked", "verify lane",
    ]),
    "lift-score-improve": ("agent", [
        "structural ceiling", "vc71", "low match", "score improve", "improve match",
        "65%", "84%", "looks structural",
    ]),
    "permuter-campaign": ("agent", [
        "permuter", "permute", "permutation", "85%", "98%", "last mile match",
    ]),
    "lift-synthetic-equivalence": ("agent", [
        "synthetic equivalence", "state snapshot", "per-branch", "equivalence",
        "capped lift", "snapshot equivalence",
    ]),
    "debug": ("agent", [
        "regression", "crash", "fault", "access violation", "access_violation",
        "broken", "wrong behavior", "build failure", "deploy failure",
        "symbol absent", "rasterizer", "visual bug", "hang", "freeze", "no draw",
        "invisible", "missing", "cull",
    ]),
    "crash-triage": ("agent", [
        "access_violation", "access violation", "page fault", "page-fault", "assert",
        "hang", "soft deadlock", "deadlock", "freeze", "crash", "eip", "cr2",
        "trap frame", "register dump",
    ]),
    "lift-crash-signals": ("agent", [
        "crash signal", "eip", "cr2", "trap frame", "register dump", "esp drift",
        "thunk recursion",
    ]),
    "halo-page-fault": ("agent", [
        "page fault", "page-fault", "access_violation", "trap frame", "cr2",
        "pe export", "symbolize",
    ]),
    "lift-silent-bugs": ("agent", [
        "wrong color", "yellow", "white tint", "invisible", "missing geometry",
        "no effect", "does nothing", "wrong position", "silent bug",
        "visual regression", "behavioral regression",
    ]),
    "bug-hunt": ("agent", [
        "bug hunt", "bug-hunt", "hazard scan", "check_lift_hazards", "before deploy",
        "pre-deploy", "pre-commit", "safety scan", "audit",
    ]),
    "input-replay-testing": ("agent", [
        "input replay", "deterministic input", "capture scenario", "capture_scenario",
        "replay fixture", "input fixture",
    ]),
    "ab-trajectory-testing": ("agent", [
        "halorec", "trajectory", "a/b", "ab check", "ab_check", "behavior_diff",
        "aa check", "trajectory diff", "regression oracle",
    ]),
    "debug-xemu": ("agent", [
        "xemu", "qmp", "gdb", "screenshot", "serial", "memory dump", "state snapshot",
        "live memory",
    ]),
    "halo-build-xemu": ("agent", [
        "build load", "build-load", "xbe deploy", "build_deploy_run", "xemu build",
    ]),
    "halo-deploy-xbdm": ("agent", [
        "xbdm", "deploy", "real xbox", "getmem", "hot patch", "hot-patch",
    ]),
    "halo-xbdm": ("agent", [
        "xbdm", "rdcp", "real xbox", "getmem",
    ]),
}


def to_inline_list(items: list[str]) -> str:
    """Render a Python list as a compact single-line YAML flow sequence."""
    inner = ", ".join('"' + s.replace('"', '\\"') + '"' for s in items)
    return "[" + inner + "]"


def migrate_one(skill_dir: str, tier: str, triggers: list[str], force: bool) -> str:
    path = SKILLS_DIR / skill_dir / "SKILL.md"
    if not path.exists():
        return f"skip (missing): {skill_dir}"
    text = path.read_text(encoding="utf-8")
    if not text.startswith("---"):
        return f"skip (no frontmatter): {skill_dir}"
    # Split frontmatter block.
    end = text.find("\n---", 3)
    if end == -1:
        return f"skip (unterminated frontmatter): {skill_dir}"
    fm = text[3:end]
    rest = text[end:]  # starts with "\n---"
    fm_lines = fm.splitlines()
    has_tier = any(ln.strip().startswith("tier:") for ln in fm_lines)
    if has_tier and not force:
        return f"skip (already migrated): {skill_dir}"
    # Drop any existing tier/triggers lines (force path), then re-insert after name:.
    fm_lines = [ln for ln in fm_lines
                if not ln.strip().startswith(("tier:", "triggers:"))]
    new_lines: list[str] = []
    inserted = False
    for ln in fm_lines:
        new_lines.append(ln)
        if not inserted and ln.strip().startswith("name:"):
            new_lines.append(f"tier: {tier}")
            if triggers:
                new_lines.append(f"triggers: {to_inline_list(triggers)}")
            inserted = True
    if not inserted:
        # No name: line — prepend fields.
        head = [f"tier: {tier}"]
        if triggers:
            head.append(f"triggers: {to_inline_list(triggers)}")
        new_lines = head + new_lines
    new_fm = "\n".join(new_lines)
    path.write_text("---" + new_fm + rest, encoding="utf-8")
    return f"migrated ({tier}, {len(triggers)} triggers): {skill_dir}"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--force", action="store_true", help="overwrite existing tier/triggers")
    ap.add_argument("--check", action="store_true", help="report skills missing from CATALOG and exit")
    args = ap.parse_args()

    present = {p.parent.name for p in SKILLS_DIR.glob("*/SKILL.md")}
    catalogued = set(CATALOG)
    missing = sorted(present - catalogued)
    extra = sorted(catalogued - present)
    if missing:
        print(f"WARNING: SKILL.md dirs not in CATALOG (won't be routed): {missing}", file=sys.stderr)
    if extra:
        print(f"WARNING: CATALOG entries with no SKILL.md: {extra}", file=sys.stderr)
    if args.check:
        return 1 if (missing or extra) else 0

    for skill_dir in sorted(CATALOG):
        tier, triggers = CATALOG[skill_dir]
        print(migrate_one(skill_dir, tier, triggers, args.force))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
