---
description: Two-phase lift — RE analysis by deep agent, then build + verify pipeline
agent: xbox-halo-re-analyst
subtask: true
agent: fast
---

Use `halo-xbox-re` for doctrine and evidence rules, `halo-re-lift` for the
lift workflow, and `halo-verify-debug` for the verification lane expectations.

Two-phase lift: RE analysis + implementation, then build and verify.

Ghidra MCP preflight (required):
- Before any `ghidra`/`ghidra-live` MCP tool call, run
  `python3 tools/check_ghidra_mcp.py`.
- If the preflight fails, or any `ghidra`/`ghidra-live` MCP tool call fails due
  to connection/timeout/unavailable errors, stop immediately and tell the user
  exactly: `You might have forgotten to start tools/mcp-servers.sh or ghidra
  may not be running?`

Scope/read-budget guardrails:
- Start from the narrowest file/function range for the resolved target.
- Avoid re-reading the same file/range unless the file changed or ambiguity remains.
- Before reading outside scope, report `NEED <path>:<line-range> because <reason>.`

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
3. Apply token-efficient defaults from `halo-re-lift`:
   - no ad-hoc inline `python3 -c` for JSON parsing; use `jq` for any JSON
     lookup or filter
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
8. Run `python3 tools/maintain.py <source_file>` to sort and reformat.

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
2. If Phase 1 retrieved caller disassembly from Ghidra (callers of the target
   function showing register setup before CALL instructions), save it to
   `/tmp/lift_caller_disasm.txt`.
3. Run:
   ```
   python3 tools/lift_pipeline.py --target <name> --no-metadata-update --verify-policy auto \
     --abi-caller-disasm-file /tmp/lift_caller_disasm.txt
   ```
   If no caller disassembly was retrieved, omit `--abi-caller-disasm-file` (the
   pipeline still runs ABI audit against kb.json declarations alone).
4. Report:
   - Target: name / address / object / source path
   - Phase 1 summary (Confirmed / Inferred / Uncertain)
   - Pipeline stage results (build pass/fail, ABI audit pass/fail, verify pass/fail)
   - Artifact path from summary.json

Notes:
- If the build fails, fix the error before re-running — do not repeat Phase 1.
- **XDK verify is the primary structural verification.** The lift pipeline runs
  `xdk_verify.py` automatically when a delinked reference exists in `delinked/`
  (mapped via `objdiff.json`). It compiles with the same MSVC 7.1 compiler that
  built the original XBE, giving 90%+ match for correct lifts. If no delinked
  reference exists, offer to run `/delink` to export one after the pipeline.
- **Prefer XBDM verification on real Xbox** over xemu+ISO whenever a console
  is available. Use `/deploy --xbe-only` then `/xbdm-*` commands to probe.
- Use `/verify-option3` for a fast post-lift lane (xemu fallback).
- Use `/lift-verify` for explicit verify runs with xdk_verify.
- Use `/maintain` for a standalone sort + format pass.
