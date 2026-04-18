# Agent Workflow

Use this template when starting a task with Claude, OpenCode, Codex, or any
other coding agent.

## Task brief template

```text
Task: <goal>
Primary files: <path1>, <path2>
Read first:
- <file>:<line>
- <file>:<line>
Touch only: <file list>
Avoid searching: build/, build_debug/, node_modules/, .git/, dist/, __pycache__/, halo-patched/
Validation: <tests/commands>
```

## In-session guardrails

1. Read target code before editing.
2. Check callers/callees and related globals/types before changing function behavior.
3. Keep edits minimal and scoped to the task.
4. If first edit fails, re-read affected files before retrying.
5. Prefer narrow command output over broad logs.

## Session handoff note

Use this structure in `notes/agent-context.md` for long-running work:

```text
Task:
Files already read:
Key findings:
Decisions made:
Open questions:
Next smallest step:
```

## Option 3 quick lane

Use this when you want fast structural + runtime sanity checks after a lift:

```bash
python3 tools/verify_option3.py --target <function_name_or_0xaddr>
```

Common add-ons:

- Add objdiff: `--objdiff-reference <ref.o> --objdiff-candidate <candidate.obj>`
- Load/reset xemu: `--load-into-xemu`
- Fast re-run: `--skip-build --skip-iso`

Artifacts are written under `artifacts/verify_option3/<run-id>/summary.json`.
