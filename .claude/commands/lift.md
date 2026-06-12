---
description: Two-phase lift — RE analysis then build + verify pipeline
---

Use `halo-xbox-re` for doctrine and evidence rules, `halo-re-lift` for the
lift workflow, and `halo-verify-debug` for the verification lane expectations.

Two-phase lift: RE analysis + implementation, then build and verify.

Ghidra MCP preflight (required):
- Before any `ghidra`/`ghidra-live` MCP tool call, run
  `rtk python3 tools/audit/check_ghidra_mcp.py`.
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
**If $ARGUMENTS is empty:** run `rtk python3 tools/analysis/frontier.py --limit 5` and pick
the top candidate.

Steps:
1. Resolve the target in `kb.json`: address, name, object, and `source_path`.
2. Follow the analysis and ABI checks from `halo-re-lift`.
3. Apply token-efficient defaults from `halo-re-lift`:
   - no ad-hoc inline `python3 -c` for JSON parsing; use `rtk jq` for any JSON
     lookup or filter
   - stage MCP requests (resolve -> decompile -> callers/callees -> disasm if needed)
   - prefer `batch_decompile` when comparing related helpers
4. **Retrieval neighbor injection** — fires automatically via hook the moment the
   Ghidra decompile completes. Similar already-ported functions will appear in
   your context as a `[retrieval-hook]` system message. Use them as worked
   examples when writing the C implementation. If no neighbors are above
   threshold the hook reports that; proceed without.
   **Context-enrichment injection** — a second hook fires simultaneously and
   checks whether `cache-context` was run for this target beforehand. If a
   cache file exists, pre-computed enrichment appears as a `[context-enrichment]`
   system message containing: a callee signature table (ported status, @reg
   annotations), FPU/FSTP argument hazards, verified struct field offsets for
   ESI/EDI/EBX, HIGH-RISK buffer-alias reads, and undersized-buffer warnings.
   This data is evidence-derived from disassembly; verify against disasm before
   relying on it, but treat HIGH-RISK buffer-alias and FSTP hazards as
   confirmed blockers requiring investigation. If no `[context-enrichment]`
   block appears, no cache exists for this target — proceed with live Ghidra
   calls as normal.
   **Doc-hazard lookup (optional, complements the code retrieval above)** — the
   hooks above retrieve similar *code*; the doc index retrieves the *bug-class
   playbook*. If the `qmd` MCP tools are available, query `docs/lift-learnings.md`
   for the hazard classes this function is prone to given its character (FPU /
   geometry → operand order & cross-product swap; passes a stack buffer to a
   callee → buffer aliasing & undersized buffers; packed 16/32-bit field stores →
   CONCAT decomposition; builds vertex/quad data → producer-vs-consumer stride).
   `mcp__plugin_qmd_qmd__query` with a `vec` line describing the function plus a
   `lex` line of the hazard keywords, `collections: ["halo-docs"]`. Apply the
   documented prevention checks while writing the C; skip if qmd is unavailable.
5. For every callee that takes register args (MOV/LEA into EAX/ECX/ESI/etc
   before a CALL, not PUSHed): add it to `kb.json` with `@<reg>` annotations
   and to `tools/kb_reg_baseline.json`, then call by name from C.
   Do not use inline assembly or raw function pointer casts — the build
   generates thunks automatically. Never remove or change existing `@<reg>`
   slot assignments.
6. Produce a structurally faithful C implementation.
7. Write the implementation directly to the source file at the correct
   address-ordered position.
8. If the `kb.json` declaration needs updating, update it conservatively.
   If the target or any callee has `@<reg>` annotations, also add/update
   `tools/kb_reg_baseline.json` — the pre-commit hook requires these in sync.
9. Run `rtk python3 tools/analysis/maintain.py <source_file>` to sort and reformat.
10. Run `rtk python3 tools/audit/check_lift_hazards.py` after source edits and fix any target-relevant hazards.

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
3. **Ensure a delinked reference exists** — VC71 verify is skipped without one,
   costing a full extra pipeline pass. Check and export now if missing:
   ```bash
   # Does a per-function delinked obj exist?
   rtk jq '[.units[] | select(.name | contains("<ADDR>"))] | length' objdiff.json
   ```
   If the result is `0`, export it before running the pipeline:
   - Get the function body range: `mcp__ghidra__get_function_by_address` at target address
   - **Verify end > start before exporting.** Also check `ls delinked/ | grep <ADDR>` — skip if a file already exists.
   - Export: `mcp__ghidra-live__export_delinked_object` to
     `G:\dev\halo\delinked\<obj>_FUN_<ADDR>.obj` with `selection_mode=range`
   - Add entry to `objdiff.json` under `.units`:
     ```json
     {
       "name": "halo/<obj>_FUN_<ADDR>",
       "target_path": "build/CMakeFiles/halo.dir/src/<source_path>.obj",
       "base_path": "delinked/<obj>_FUN_<ADDR>.obj",
       "metadata": { "source_path": "<source_path>" }
     }
     ```
   - `rtk git add delinked/<obj>_FUN_<ADDR>.obj objdiff.json`
4. Run:
   ```
   rtk python3 tools/lift_pipeline.py --target <name> --no-metadata-update --verify-policy auto \
     --abi-caller-disasm-file /tmp/lift_caller_disasm.txt
   ```
   If no caller disassembly was retrieved, omit `--abi-caller-disasm-file`.
4. Report:
    - Target: name / address / object / source path
    - Phase 1 summary (Confirmed / Inferred / Uncertain)
    - Pipeline stage results (build, ABI audit, VC71 verify, low-match policy, behavior/runtime checks)
    - Artifact path from summary.json

Notes:
- If the build fails, fix the error before re-running — do not repeat Phase 1.
- **VC71 verify is the primary structural verification.** The lift pipeline runs
  `vc71_verify.py` automatically when a delinked reference exists in `delinked/`
  (mapped via `objdiff.json`). Step 3 above ensures this is done before the
  pipeline runs — never skip it and then offer a second pass.
- **Prefer XBDM verification on real Xbox** whenever a console is available.
  Use `/deploy --xbe-only` then `/xbdm <mode>` commands to probe.
- Use `/verify option3 <target>` only as a runtime/xemu fallback lane, not as primary structural proof.
- Use `/verify structural <target> <new_address>` for explicit verify payload runs with a known lifted function address.
- `/auto-lift` auto-commits on success and reverts+logs on failure. See `artifacts/auto_lift/failures/` for failure records.
- Use `/maintain` for a standalone sort + format pass.
- **After a successful commit**, update the retrieval index so future lifts benefit:
  `rtk python3 tools/retrieval/build_index.py extract && rtk python3 tools/retrieval/build_index.py embed`
