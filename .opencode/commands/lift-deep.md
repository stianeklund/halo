---
description: High-reasoning lift — deep agent RE analysis + build + verify pipeline
agent: deep
subtask: true
---

Use `halo-xbox-re` for doctrine and evidence rules, `halo-re-lift` for the
lift workflow, and `halo-verify-debug` for the verification lane expectations.

Two-phase lift: RE analysis + implementation, then build and verify.

Argument: $ARGUMENTS (optional target name or 0x... address)

Ghidra MCP preflight (required):
- Before any `ghidra`/`ghidra-live` MCP tool call, run
  `python3 tools/audit/check_ghidra_mcp.py`.
- If the preflight fails, or any `ghidra`/`ghidra-live` MCP tool call fails due
  to connection/timeout/unavailable errors, stop immediately and tell the user
  exactly: `You might have forgotten to start tools/mcp-servers.sh or ghidra
  may not be running?`

Scope/read-budget guardrails:
- Start from the narrowest file/function range for the resolved target.
- Avoid re-reading the same file/range unless the file changed or ambiguity remains.
- Before reading outside scope, report `NEED <path>:<line-range> because <reason>`.

---

## Phase 1 — Analysis + Implementation

Adopt the persona of the `xbox-halo-re-analyst` (bounded RE worker for Halo CE
Xbox, cachebeta.xbe, v01.10.12.2276). Follow the `halo-xbox-re` skill for
methodology, evidence rules, prototype inference, kb policy, and output format.

**If $ARGUMENTS is provided:** use it as the target (name or 0x... address).
**If $ARGUMENTS is empty:** run `python3 tools/analysis/frontier.py --limit 5` and pick
the top candidate.

Steps:
1. Resolve the target in `kb.json`: address, name, object, and `source_path`.
2. Follow the analysis and ABI checks from `halo-re-lift`.
3. Apply token-efficient defaults from `halo-re-lift`:
   - no ad-hoc inline `python3 -c` for kb lookups
   - stage MCP requests (resolve -> decompile -> callers/callees -> disasm if needed)
   - prefer `batch_decompile` when comparing related helpers
4. For every callee that takes register args (MOV/LEA into EAX/ECX/ESI/etc
   before a CALL, not PUSHed): add it to `kb.json` with `@<reg>` annotations
   and to `tools/kb_reg_baseline.json`, then call by name from C.
   Do not use inline assembly or raw function pointer casts — the build
   generates thunks automatically. Never remove or change existing `@<reg>`
   slot assignments.
5. Produce a structurally faithful C implementation.
6. Write the implementation directly to the source file at the correct
   address-ordered position.
7. If the `kb.json` declaration needs updating, update it conservatively.
8. Run `python3 tools/analysis/maintain.py <source_file>` to sort and reformat.

Output format follows `halo-xbox-re` (see `docs/references/output-schema.md`).

Report at minimum:
- Target / Confirmed / Inferred / Uncertain
- Proposed code (as written)
- kb.json updates made
- RESOLVED_TARGET: <function_name>
- A short `Tool efficiency` note (MCP calls used, and why any full disassembly was needed)

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
