---
name: halo-re-lift
description: Repo-specific Halo CE Xbox reverse-engineering and lift workflow
---

# Halo RE Lift

Use this skill for the operational workflow of lifting a function from
cachebeta.xbe or default.xbe. Doctrine and evidence rules live in
`halo-xbox-re`; this skill covers the lift sequence and ABI specifics.

## When to use

- Lifting a function from the XBE
- Reviewing a proposed lift for ABI or layout fidelity
- Resolving prototypes, struct fields, globals, or `@<reg>` thunks
- Updating kb.json after binary-backed analysis

## Lift workflow

1. Resolve target by name or address in kb.json and Ghidra.
2. Gather context: callers, callees, touched globals, strings, imports,
   existing declarations in source and kb.json.
3. Cross-check decompilation against raw disassembly (see `halo-xbox-re`
   review checklist; see `docs/references/abi-and-calling-conventions.md`
   for full ABI rules). **Mandatory call-site verification:** for every CALL in
   the disassembly, trace each PUSH backward and confirm the decompiler mapped
   it to the correct variable. Watch for:
   - Register aliasing: EBX/ESI/EDI set far from the call site
   - Push-then-fstp: `PUSH <dummy>; FSTP [ESP]` replaces arg with float
   - Struct field rotation: MSVC interleaved stores do not imply decompiler offsets
4. Infer the narrowest defensible prototype (see
   `docs/references/prototype-inference.md`).
5. Produce a structurally faithful C lift:
   - preserve control-flow shape
   - preserve side-effect order
   - preserve pointer arithmetic and odd logic unless disproven
6. Write implementation in address-ordered position.
7. Update kb.json conservatively (see
   `docs/references/kb-update-policy.md`).
8. Run `rtk python3 tools/analysis/maintain.py <source_file>`.
9. Run `rtk python3 tools/audit/check_lift_hazards.py` and fix any target-relevant hazards.

## Ghidra MCP availability (required)

- Before the first `ghidra` or `ghidra-live` MCP tool call in a task, run
  `rtk python3 tools/audit/check_ghidra_mcp.py`.
- If the preflight fails, or if any `ghidra`/`ghidra-live` MCP tool call fails
  due to connection/timeout/unavailable errors, stop immediately and do not
  retry in the same response.
- Tell the user exactly: `You might have forgotten to start
  tools/shell/mcp-servers.sh or ghidra may not be running?`

## Token-efficient execution defaults

Use these defaults unless a target requires deeper forensics:

- Prefer existing repo tools over ad-hoc scripts:
  - `rtk python3 tools/analysis/kb_meta.py list --object <obj>` for scoped symbol sets
  - `rtk python3 tools/lift_pipeline.py --target <name_or_addr> ...` for staged lift/verify
  - `rtk python3 tools/llm_auto_lift.py select --limit 20` for combined frontier/liftability target selection
- Avoid inline `python3 -c` snippets for kb queries and address matching.
- Keep MCP passes staged:
  1. Resolve target (`get_function_by_address`).
  2. Pull pseudocode (`decompile_function` or `batch_decompile` for >1 function).
  3. Pre-pass the pseudocode through the deterministic rewriter before reasoning over it:
     ```
     rtk python3 tools/lift/draft_decompiler.py --json artifacts/auto_lift/context_cache/<NAME>.json > /tmp/lift_draft.c
     rtk python3 tools/lift/buffer_alias_detector.py --json artifacts/auto_lift/context_cache/<NAME>.json > /tmp/lift_hazards.c
     ```
     Use `/tmp/lift_draft.c` as the working starting point — it has the
     MSVC intrinsic table (CLAUDE.md) already applied, synthetic types
     canonicalized, and `__try/__except` already wrapped. `/tmp/lift_hazards.c`
     surfaces HIGH-RISK buffer-alias sites (hazard #5) that must be
     resolved before writing the final lift.
  4. If the cached context JSON contains `similar_neighbors`, read them —
     these are the top-3 most semantically similar already-ported functions
     with their final C source. Use as worked examples, not authority.
  5. Pull callers/callees (`get_function_callers`/`get_function_callees`) with bounded limits.
  6. Pull full disassembly only when decompiler output is ambiguous or ABI-critical.
- Prefer one target per run; do not batch unrelated functions in one analysis pass.
- In reports, summarize evidence and include only the minimum assembly needed to justify claims.

## Post-lift follow-ups

After `lift_pipeline.py` reports VC71 results, decide whether to spend
extra cycles on permuter or Unicorn — the default is to do nothing.

- Match in **[85, 98]%** with a delinked reference → `/verify permute`
  (60s last-mile optimizer). Do NOT permute below 85% (fix the lift) or
  above 98% (diminishing returns). Never accept a permutation that lowers
  the existing match; always re-run the lift pipeline before trusting it.
- **Pure leaf** (no calls out, no globals) AND FPU-heavy or structurally
  capped (e.g. SEH wrappers stuck near 55%) → `/verify equivalence` for a
  100-seed Unicorn behavioral diff. Skip if the function calls `FUN_xxx`
  or references DAT_/globals — Unicorn rejects non-leaves.
- Equivalence runs populate `tools/equivalence/leaf_cache.json`, which
  rewards leaves in future `select` runs (`+5 eq_pure_leaf`).

See `halo-verify-debug` for the full lane decision tree.

## Auto-lift candidates

- `tools/llm_auto_lift.py` is an untrusted candidate generator and validation runner.
- Use `select` as the normal entrypoint; it combines frontier priority with automation safety.
- It may produce `auto_accept`, `needs_review`, or `reject` artifacts, but it must not commit.
- Run `promote` as a dry run first and ask the user before `promote --apply`.
- Do not use generated code without the same binary evidence, hazard checks, and validation gates as a manual lift.

## ABI cautions

Key reminders (full rules in `docs/references/abi-and-calling-conventions.md`):

- cdecl: the first PUSH is the last C argument.
- For `@<reg>` or reverse-thunked paths, audit which registers the original
  caller expects preserved.
- Lifted C may legitimately clobber caller-saved EAX, ECX, EDX.
- `@<reg>` annotations are immutable. Never remove or change slot assignments.
- When calling an original XBE function that takes register args, add it to
  kb.json with `@<reg>` and call by name. Do not use raw casts or inline asm.
  New `@<reg>` entries must also be added to `tools/kb_reg_baseline.json`
  **inside the `"functions"` dict** (not at the top level). Key format: `"0xABCDEF"`.
- Do not reorder or repack structs without binary evidence and matching asserts.

## Commit discipline

- Do not write freeform lift commit messages.
- After staging lift changes, run `rtk python3 tools/audit/generate_lift_commit.py --batch-name "<short description>" > /tmp/commit_msg.txt`.
- Commit with `rtk git commit -F /tmp/commit_msg.txt` only after reviewing the generated message.

## Output expectations

Follow the output format from `halo-xbox-re`
(see `docs/references/output-schema.md` for full detail).
