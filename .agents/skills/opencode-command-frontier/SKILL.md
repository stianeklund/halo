---
name: opencode-command-frontier
description: "Antigravity/Gemini wrapper for OpenCode /frontier. Use when the user asks for /frontier, the frontier command, or says: Show the decompilation frontier"
---

# OpenCode Command: /frontier

This skill ports the existing OpenCode command `.opencode/commands/frontier.md`
to Antigravity/Gemini.

When invoked:

- Treat the user's text after `/frontier` or after the skill request as `$ARGUMENTS`.
- Follow the command prompt below as the task instructions.
- Keep using the repo's normal `AGENTS.md` and skill doctrine.

## Command Prompt

Show the decompilation frontier — what's implemented, what's remaining, and what to work on next. For actionable target selection, prefer the combined selector.

Steps:
1. Run `rtk python3 tools/analysis/fun_pipeline.py status` first. If the output shows more than ~200 functions in `<common>`, recommend running `/fun-pipeline reclassify` before proceeding — frontier scoring is inaccurate until `<common>` is reduced.
2. Run `rtk python3 tools/llm_auto_lift.py select --limit 20` for combined strategic priority and liftability.
2. If the user asks for raw frontier data, run `rtk python3 tools/analysis/frontier.py --limit 20`.
3. Report:
   - Total coverage: implemented / total (percentage)
   - Active TUs: partially-implemented translation units with remaining function count
   - Quick wins: unstarted TUs with few functions (1-3)
   - Recommended next targets with lane: `auto-lift`, `cache-context`, `manual-lift`, or `defer`
4. Keep the output concise — table format preferred.
