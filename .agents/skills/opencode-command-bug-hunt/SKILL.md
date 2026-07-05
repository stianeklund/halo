---
name: opencode-command-bug-hunt
description: "Antigravity/Gemini wrapper for OpenCode /bug-hunt. Use when the user asks for /bug-hunt, the bug-hunt command, or says: Run the Halo lift bug-hunt router: hazards, silent bugs, ABI drift, and pre-deploy checks"
---

# OpenCode Command: /bug-hunt

This skill ports the existing OpenCode command `.opencode/commands/bug-hunt.md`
to Antigravity/Gemini.

When invoked:

- Treat the user's text after `/bug-hunt` or after the skill request as `$ARGUMENTS`.
- Follow the command prompt below as the task instructions.
- Keep using the repo's normal `AGENTS.md` and skill doctrine.

## Command Prompt

Load the `bug-hunt` skill and follow its tiered decision tree.

Argument: $ARGUMENTS (`--all`, `--full`, `--changed-only`, or a short symptom)

Routing:
- Default / `--all`: run Tier 0 and Tier 1 from `bug-hunt`.
- `--full`: run Tier 0, Tier 1, and Tier 3.
- Symptom includes crash, page fault, assert, hang, wrong behavior, wrong color, invisible geometry, or regression: load `debug` and `crash-triage` first, then return to `bug-hunt` for scans.
- Any WARN/HIGH/ERROR in touched files: load the detailed skill named by `bug-hunt` (`lift-silent-bugs`, `lift-arg-hazards`, `lift-decompiler-traps`, or `lift-frame-hazards`).

Report commands run, blocking findings, and the next narrow fix.
