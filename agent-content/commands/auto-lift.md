---
description: Generate and validate untrusted LLM lift candidates without committing
agent: fast
subtask: true
---

Use `halo-re-lift` for lift rules and `halo-verify-debug` for validation gates.

Run the LLM auto-lift harness as an untrusted candidate generator and review
queue. It may generate, build, and validate candidate code, but it must not
commit automatically.

Argument: $ARGUMENTS (optional: `auto|select|score|cache-context|generate|review|promote ...`)

Default behavior with no arguments:
1. Run the combined selector.
2. Pick the best `auto-lift` target, falling back to `cache-context` if needed.
3. Cache Ghidra context for that target.
4. Generate and validate one candidate with `--max-retries 2`.
5. Show the latest review summary.
6. Stop before `promote --apply` or commit.

Safety rules:
1. Treat every generated candidate as untrusted until validation artifacts prove otherwise.
2. Prefer no-argument `/auto-lift` for the streamlined safe path, or `select`, `cache-context`, `generate`, and `review` for manual control; run `promote` without `--apply` first.
3. Ask for explicit user approval before running `promote --apply`.
4. Never commit from this command. If a promoted result should be committed, use the normal lift commit flow with `tools/audit/generate_lift_commit.py`.
5. Do not use `--skip-vc71` unless the user explicitly requests a lower-confidence run.

Typical flow:
```bash
rtk python3 tools/llm_auto_lift.py
```

Manual flow:
```bash
rtk python3 tools/llm_auto_lift.py select --limit 20
rtk python3 tools/llm_auto_lift.py cache-context --target <target>
rtk python3 tools/llm_auto_lift.py generate --target <target> --max-retries 2
rtk python3 tools/llm_auto_lift.py review
rtk python3 tools/llm_auto_lift.py promote --batch <batch>
```

`select` is the recommended entrypoint. It combines strategic frontier priority
with auto-lift safety/liftability and labels each target as `auto-lift`,
`cache-context`, `manual-lift`, or `defer`.

`auto` is the no-argument default and runs the safe selection -> context ->
generation -> review sequence for one target.

Batch mode is allowed only for low-risk review queues:
```bash
rtk python3 tools/llm_auto_lift.py generate --batch 3 --min-score 30 --max-retries 2
```

Batch `cache-context` and `generate` use the same combined selector by default.

Report:
- batch directory under `artifacts/auto_lift/`
- each target state: `auto_accept`, `needs_review`, or `reject`
- build, ABI, XDK, and low-match results from validation
- whether promotion was dry-run or applied
- use `/verify failure <artifact_dir>` for failed or ambiguous validation artifacts

Remember: passing validation reduces risk, but it is not proof of behavioral equivalence unless the target also has strong delink, golden, or runtime coverage.
