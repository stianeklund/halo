Two-phase lift: RE analysis + implementation by the xbox-halo-re-analyst agent,
then build and verify via the pipeline.

Argument: $ARGUMENTS (optional target name or 0x... address)

---

## Phase 1 — Analysis + Implementation

Spawn an Agent with `subagent_type: "xbox-halo-re-analyst"` with the following
prompt (substitute $ARGUMENTS literally — if it is empty, tell the agent to
auto-pick from the frontier):

> **Target:** $ARGUMENTS (if empty, run `python3 tools/frontier.py --limit 5`
> and pick the top candidate)
>
> Perform a complete lift:
>
> 1. Resolve the target in kb.json: address, name, object, source_path.
> 2. Analyze via Ghidra MCP — decompile + disassemble + cross-check operand
>    sizes, CALL targets, and register args per the CLAUDE.md procedure.
> 3. Produce a structurally faithful C implementation following CLAUDE.md rules.
> 4. Write the implementation directly to the source file using the Edit or
>    Write tool at the correct address-ordered position. Do not write to a temp
>    file — write to the real source file.
> 5. If the kb.json declaration needs updating (prototype, register args, new
>    symbol), update it.
> 6. Run `python3 tools/maintain.py <source_file>` to sort and reformat.
> 7. End your response with exactly one line: `RESOLVED_TARGET: <function_name>`

---

## Phase 2 — Build + Verify

After the agent returns:

1. Parse the `RESOLVED_TARGET: <name>` line from the agent's response to get
   the canonical function name.
2. Run:
   ```
   python3 tools/lift_pipeline.py --target <name> --no-metadata-update
   ```
   No `--candidate` is needed — the agent already wrote to the source file.
3. Report:
   - Target: name / address / object / source path
   - Agent summary: Confirmed / Inferred / Uncertain items
   - Pipeline stage results (build pass/fail, verify pass/fail)
   - Artifact path from summary.json

Notes:
- If Phase 2 build fails, investigate and fix the build error before re-running.
  Do not re-run Phase 1 unless the implementation itself is wrong.
- Use `/lift-verify` for structural verification with `--verify-auto` after a
  successful build.
- Use `/maintain` if ordering or formatting needs a standalone cleanup pass.
