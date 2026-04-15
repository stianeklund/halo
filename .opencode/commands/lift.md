---
description: Two-phase lift — RE analysis by deep agent, then build + verify pipeline
agent: deep
subtask: true
---

Two-phase lift: RE analysis + implementation, then build and verify.

Argument: $ARGUMENTS (optional target name or 0x... address)

---

## Phase 1 — Analysis + Implementation

Using the xbox-halo-re-analyst persona (expert Xbox/Halo CE reverse engineer),
perform a complete lift for the target:

**If $ARGUMENTS is provided:** use it as the target (name or 0x... address).
**If $ARGUMENTS is empty:** run `python3 tools/frontier.py --limit 5` and pick
the top candidate.

Steps:
1. Resolve the target in kb.json: address, name, object, source_path.
2. Analyze via Ghidra MCP — decompile + disassemble + cross-check operand
   sizes, CALL targets, and register args per the CLAUDE.md procedure.
3. Produce a structurally faithful C implementation following CLAUDE.md rules.
4. Write the implementation directly to the source file at the correct
   address-ordered position.
5. If the kb.json declaration needs updating, update it.
6. Run `python3 tools/maintain.py <source_file>` to sort and reformat.

Output format:
- Target
- Confirmed / Inferred / Uncertain
- Proposed code (as written)
- kb.json updates made
- RESOLVED_TARGET: <function_name>

---

## Phase 2 — Build + Verify

After Phase 1 completes:

1. Use the function name from `RESOLVED_TARGET:` above.
2. Run:
   ```
   python3 tools/lift_pipeline.py --target <name> --no-metadata-update
   ```
3. Report:
   - Target: name / address / object / source path
   - Phase 1 summary (Confirmed / Inferred / Uncertain)
   - Pipeline stage results (build pass/fail, verify pass/fail)
   - Artifact path from summary.json

Notes:
- If the build fails, fix the error before re-running — do not repeat Phase 1.
- Use `/lift-verify` for structural verification with `--verify-auto`.
- Use `/maintain` for a standalone sort + format pass.
