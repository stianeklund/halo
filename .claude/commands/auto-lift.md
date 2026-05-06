---
description: Target selection, Ghidra context caching, and lift delegation
agent: fast
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

Safety rules:
1. Code generation is done by `/lift`, which has full agent context and follows CLAUDE.md rules.
2. Never commit from this command. Commits go through the standard lift commit flow.
3. `review` and `promote` are legacy subcommands for old SDK-based batch artifacts.
4. Ask for explicit user approval before running `promote --apply`.

Typical flow (single target):
```bash
rtk python3 tools/llm_auto_lift.py select --limit 10
# pick the top target, then:
rtk python3 tools/llm_auto_lift.py cache-context --target <target>
# then delegate:
/lift <target>
```

`select` is the default subcommand when run with no arguments. It combines
strategic frontier priority with liftability scoring and labels each target as
`auto-lift`, `cache-context`, `manual-lift`, or `defer`.

Report:
- selected target name, address, object, lane, and score
- whether Ghidra context was cached
- then the `/lift` skill handles implementation, validation, and commit
