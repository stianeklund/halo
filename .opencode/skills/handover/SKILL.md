---
name: handover
description: Create a concise continuation handover for Halo CE Xbox RE/lift work when the user runs /handover or asks to transfer context to a new agent.
---

# Handover

Use this skill when the user invokes `/handover` or asks for a handover,
continuation brief, session summary, or bootstrap document for a new LLM agent.

The output is a short, evidence-aware document that helps the next agent resume
without rereading the whole repo or repeating risky reverse-engineering work.

## Scope

This repo does evidence-based Halo CE Xbox reverse engineering and C89 lifting.
The handover must preserve that discipline:

- binary-backed facts are separated from inferences
- uncertain points stay explicit
- validation status is reported exactly
- unrelated worktree changes are not claimed as yours
- next steps are specific and small

Do not do new investigation unless the user explicitly asks. A handover should
summarize the current session state, not continue the task.

## Before Writing

Use only lightweight checks needed to avoid misleading the next agent:

1. Check the current objective or goal if available.
2. Check concise git status and scoped diffs for likely touched files.
3. Use remembered context from the current conversation first.
4. Do not read `kb.json`; if a kb fact is needed, use `rtk jq` only.
5. Do not read build artifacts, logs, generated dirs, or broad source files.

If the current task involved Ghidra, XBDM, xemu, VC71 verify, equivalence,
or lift pipeline runs, report only the exact commands/results already observed.
Do not invent verification evidence.

## Output Format

Keep the handover concise. Prefer bullets over prose. Use this structure:

```markdown
# Handover

## Objective
<one sentence describing what we were trying to accomplish>

## Current State
- <what is done or partially done>
- <important files/symbols/functions touched>
- <repo/worktree state if relevant>

## Confirmed
- <facts backed by tool output, binary evidence, successful edits, or tests>

## Important Changes
- `<path>`: <brief change and why it matters>

## Validation
- <commands run and pass/fail status>
- <commands not run, if important>

## Uncertain / Risks
- <unknowns, assumptions, possible hazards, or unverified areas>

## Next Steps
1. <smallest concrete next action>
2. <next validation or investigation step>

## Resume Prompt
<brief prompt the user can paste into a new agent>
```

If the task is complete, say so in `Current State`, keep `Next Steps` to
optional follow-ups, and make the `Resume Prompt` a brief context summary rather
than an instruction to continue.

## Style Rules

- Keep it brief enough to paste into a new session.
- Include exact function names, addresses, paths, and commands when known.
- Distinguish `Confirmed`, `Inferred`, and `Uncertain` when reverse-engineering
  evidence matters.
- Do not include chain-of-thought or hidden reasoning.
- Do not over-summarize validation; missing validation is useful context.
- Do not mark work complete unless completion was verified.
