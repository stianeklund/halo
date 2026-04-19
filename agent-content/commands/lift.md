---
description: Two-phase lift — RE analysis by deep agent, then build + verify pipeline
agent: xbox-halo-re-analyst
subtask: true
---

Use `halo-xbox-re` for doctrine and evidence rules, `halo-re-lift` for the
lift workflow, and `halo-verify-debug` for the verification lane expectations.

Two-phase lift: RE analysis + implementation, then build and verify.

Argument: $ARGUMENTS (optional target name or 0x... address)

---

## Phase 1 — Analysis + Implementation

Using the xbox-halo-re-analyst persona (bounded RE worker following
`halo-xbox-re` doctrine), perform a complete lift for the target:

**If $ARGUMENTS is provided:** use it as the target (name or 0x... address).
**If $ARGUMENTS is empty:** run `python3 tools/frontier.py --limit 5` and pick
the top candidate.

Steps:
1. Resolve the target in `kb.json`: address, name, object, and `source_path`.
2. Follow the analysis and ABI checks from `halo-re-lift`.
3. Produce a structurally faithful C implementation.
4. Write the implementation directly to the source file at the correct
   address-ordered position.
5. If the `kb.json` declaration needs updating, update it conservatively.
6. Run `python3 tools/maintain.py <source_file>` to sort and reformat.

Output format follows `halo-xbox-re` (see `docs/references/output-schema.md`).

Report at minimum:
- Target / Confirmed / Inferred / Uncertain
- Proposed code (as written)
- kb.json updates made
- RESOLVED_TARGET: <function_name>

---

## Phase 2 — Build + Verify

After Phase 1 completes:

1. Use the function name from `RESOLVED_TARGET:` above.
2. Run:
   ```
   python3 tools/lift_pipeline.py --target <name> --no-metadata-update --verify-policy auto
   ```
3. Report:
   - Target: name / address / object / source path
   - Phase 1 summary (Confirmed / Inferred / Uncertain)
   - Pipeline stage results (build pass/fail, verify pass/fail)
   - Artifact path from summary.json

Notes:
- If the build fails, fix the error before re-running — do not repeat Phase 1.
- **Prefer XBDM verification on real Xbox** over xemu+ISO whenever a console
  is available. Use `/deploy --xbe-only` then `/xbdm-*` commands to probe.
- Use `/verify-option3` for a fast post-lift lane (xemu fallback).
- Use `/lift-verify` for explicit verify payload runs when you already have
  the lifted function address and extraction outputs.
- Use `/maintain` for a standalone sort + format pass.