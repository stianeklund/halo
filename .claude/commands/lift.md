---
description: Two-phase lift — RE analysis by deep agent, then build + verify pipeline
agent: xbox-halo-re-analyst
subtask: true
---

Use `halo-xbox-re` for doctrine and evidence rules, `halo-re-lift` for the
lift workflow, and `halo-verify-debug` for the verification lane expectations.

Two-phase lift: RE analysis + implementation, then build and verify.

Ghidra MCP preflight (required):
- Before any `ghidra`/`ghidra-live` MCP tool call, run
  `rtk python3 tools/audit/check_ghidra_mcp.py`.
- If the preflight fails, or any `ghidra`/`ghidra-live` MCP tool call fails due
  to connection/timeout/unavailable errors, stop immediately and tell the user
  exactly: `You might have forgotten to start tools/shell/mcp-servers.sh or ghidra
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
3b. After the decompile pass, run the deterministic pre-pass on the cached
    Ghidra output before reasoning over it:
    ```
    rtk python3 tools/lift/draft_decompiler.py --json artifacts/auto_lift/context_cache/<NAME>.json > /tmp/lift_draft.c
    rtk python3 tools/lift/buffer_alias_detector.py --json artifacts/auto_lift/context_cache/<NAME>.json > /tmp/lift_hazards.c
    ```
    `draft_decompiler` canonicalizes synthetic Ghidra ops, applies the MSVC
    intrinsic table (CLAUDE.md), and wraps `__SEH_prolog`/`__SEH_epilog`
    pairs in native `__try/__except`. `buffer_alias_detector` flags
    HIGH-RISK `local_XX` reads inside known stack buffers (hazard #5).
    Use `/tmp/lift_draft.c` as the working starting point and resolve any
    HIGH-RISK lines from `/tmp/lift_hazards.c` against disassembly before
    writing the lift.
4. For every callee that takes register args (MOV/LEA into EAX/ECX/ESI/etc
   before a CALL, not PUSHed): add it to `kb.json` with `@<reg>` annotations
   and to `tools/kb_reg_baseline.json` (inside the `"functions"` dict), then call by name from C.
   Do not use inline assembly or raw function pointer casts — the build
   generates thunks automatically. Never remove or change existing `@<reg>`
   slot assignments.
5. Produce a structurally faithful C implementation.
6. Write the implementation directly to the source file at the correct
   address-ordered position.
7. If the `kb.json` declaration needs updating, update it conservatively.
   If the target or any callee has `@<reg>` annotations, also add/update
   `tools/kb_reg_baseline.json` — the pre-commit hook requires these in sync.
8. Run `rtk python3 tools/analysis/maintain.py <source_file>` to sort and reformat.
9. Run `rtk python3 tools/audit/check_lift_hazards.py` after source edits and fix any target-relevant hazards.

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
   rtk python3 tools/lift_pipeline.py --target <name> --no-metadata-update --verify-policy auto \
     --low-match-threshold 90 --low-match-behavior-both-below 85 --low-match-reject-below 80 \
     --abi-caller-disasm-file /tmp/lift_caller_disasm.txt
   ```
   If no caller disassembly was retrieved, omit `--abi-caller-disasm-file`.
4. Report:
    - Target: name / address / object / source path
    - Phase 1 summary (Confirmed / Inferred / Uncertain)
    - Pipeline stage results (build, ABI audit, VC71 verify, low-match policy, behavior/runtime checks)
    - Artifact path from summary.json
5. Optional last-mile: if the VC71 match is in the 85–98% band, suggest
   `/verify permute <target>` for a 60-second permuter pass. Apply a winning
   permutation only after re-running the lift pipeline; never accept a
   permutation that lowers the match.
6. Optional behavioral validation for pure leaves: if the target has no
   unresolved external relocations (e.g. math/serializer/string helpers),
   suggest `/verify equivalence <target>` for a 100-seed Unicorn-Engine
   differential. Useful for FPU-heavy code where byte-match is unreliable.

Notes:
- If the build fails, fix the error before re-running — do not repeat Phase 1.
- **VC71 verify is the primary structural verification.** The lift pipeline runs
  `vc71_verify.py` automatically when a delinked reference exists in `delinked/`
  (mapped via `objdiff.json`). If no delinked reference exists, offer to run
  `/verify delink <target>` after the pipeline.
- The pipeline thresholds above are only a mechanical floor. They are not an
  auto-commit standard: `/auto-lift` must run its separate agent review gate
  before committing any lift below the near-perfect structural range.
- **Prefer XBDM verification on real Xbox** over xemu+ISO whenever a console
  is available. Use `/deploy --xbe-only` then `/xbdm <mode>` commands to probe.
- Use `/verify option3 <target>` only as a runtime/xemu fallback lane, not as primary structural proof.
- Use `/verify structural <target> <new_address>` for explicit verify payload runs with a known lifted function address.
- `/auto-lift` auto-commits on success and reverts+logs on failure. See `artifacts/auto_lift/failures/` for failure records.
- Use `/maintain` for a standalone sort + format pass.
- **After Phase 2 completes**, always run `/build` to catch any remaining compile errors or linter issues that arose post-verification.
