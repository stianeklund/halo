---
description: Target selection, Ghidra context caching, and lift delegation
model: sonnet
subtask: true
---

Use `halo-re-lift` for lift rules and `halo-verify-debug` for validation gates.

Selects the best unported function to lift next, optionally caches Ghidra
context, then delegates code generation to `/lift`.

Argument: $ARGUMENTS (optional: `select|score|cache-context|review|promote ...`)

Default behavior with no arguments:
1. Run `rtk python3 tools/llm_auto_lift.py select --limit 10` to pick the best targets.
2. Pick the top `auto-lift` or `cache-context` lane target.
3. If Ghidra MCP is available and no cached context exists, run `cache-context --target <target>`.
4. Delegate to `/lift <target>` for the actual implementation (includes `maintain.py` via lift_pipeline).
5. **On success** (pipeline passes): auto-commit via the standard lift commit flow:
   ```bash
   rtk git add -- src/ kb.json
   rtk python3 tools/audit/generate_lift_commit.py --batch-name "<target_name>" > /tmp/commit_msg.txt
   rtk git commit -F /tmp/commit_msg.txt
   ```
6. **On failure** (build error, ABI failure, low VC71 match, etc.): revert all
   uncommitted changes and log the failure:
   ```bash
   rtk git checkout -- src/ kb.json
   ```
   Then write a failure record to `artifacts/auto_lift/failures/<target_name>.json`
   with: target name, address, object, timestamp, failure stage, error summary,
   and pipeline output. This lets the user review what failed and why.

Safety rules:
1. Code generation is done by `/lift`, which has full agent context and follows CLAUDE.md rules.
2. Auto-commit only after the full pipeline passes (build + ABI audit + VC71 when available).
3. Always revert on failure — never leave broken lift state in the working tree.
4. `review` and `promote` are legacy subcommands for old SDK-based batch artifacts.
5. Ask for explicit user approval before running `promote --apply`.

Failure log format (`artifacts/auto_lift/failures/<target_name>.json`):
```json
{
  "target": "<name>",
  "addr": "<0x...>",
  "object": "<object_name>",
  "timestamp": "<ISO 8601>",
  "failure_stage": "<build|abi|vc71|hazard|other>",
  "error_summary": "<one-line description>",
  "pipeline_output": "<full pipeline stderr/stdout>"
}
```

Typical flow (single target):
```bash
rtk python3 tools/llm_auto_lift.py select --limit 10
# pick the top target, then:
rtk python3 tools/llm_auto_lift.py cache-context --target <target>
# then delegate:
/lift <target>
# auto-commits on success, reverts+logs on failure
```

`select` is the default subcommand when run with no arguments. It combines
strategic frontier priority with liftability scoring and labels each target as
`auto-lift`, `cache-context`, `manual-lift`, or `defer`.

When used with `/loop`, each iteration picks the next best target, lifts it,
and either commits or reverts+logs. Check `artifacts/auto_lift/failures/` to
review any targets that failed.

Report:
- selected target name, address, object, lane, and score
- whether Ghidra context was cached
- lift result: committed (with commit hash) or reverted (with failure log path)
