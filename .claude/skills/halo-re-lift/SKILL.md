---
name: halo-re-lift
description: Repo-specific Halo CE Xbox reverse-engineering and lift workflow
---

# Halo RE Lift

Use this skill for the operational workflow of lifting a function from
cachebeta.xbe or default.xbe. Doctrine and evidence rules live in
`halo-xbox-re`; this skill covers the lift sequence and ABI specifics.

## Worktree context (CRITICAL — read first)

At the start of every lift session, determine the actual repo root:

```bash
rtk git rev-parse --show-toplevel
```

All file edits, `rtk git` commands, and tool invocations must target **that path**, not a hardcoded `/mnt/g/dev/halo`. When running as a subagent spawned from a worktree, the "Primary working directory" in your system prompt tells you where you are — trust it over any hardcoded path. Never commit or stage changes to a path outside your working directory.

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
5. **Pre-implementation pattern check** — before writing C, scan for crash classes
   `check_lift_hazards.py` does NOT flag (full detail in `docs/lift-learnings.md`):
   - XCALLs to targets being ported: `grep -oP 'XCALL\(0x\K[0-9a-f]+' src/<file>.c`
   - `&local_XX` args to callees that index `param[N]` (stack aliasing → must be contiguous buffer)
   - Loops that advance a parameter pointer when original uses a copy register post-loop
   - After writing C: `grep -n '(float)(int)' src/<file>.c` (float-as-pointer smuggling)
   - After writing C: `grep -n '(float \*)0\|(void \*)0\|(int \*)0' src/<file>.c` (NULL @<reg> args)
   - **→ Use `lift-arg-hazards` skill** for ADD ESP mismatch suspicion, 0-arg getter patterns, or @<reg> order questions
   - **→ Use `lift-frame-hazards` skill** when the decompile has `_chkstk`, you need to size a buffer from the frame, or you see `&local` passed to an indexing callee
6. Produce a structurally faithful C lift:
   - preserve control-flow shape
   - preserve side-effect order
   - preserve pointer arithmetic and odd logic unless disproven
7. Write implementation in address-ordered position.
8. Update kb.json conservatively (see
   `docs/references/kb-update-policy.md`).
9. Run `rtk python3 tools/analysis/maintain.py <source_file>`.
10. Run `rtk python3 tools/audit/check_lift_hazards.py` and fix any target-relevant hazards.
11. **Post-verify score routing:**
    - Score 65–84% and gap described as "structural" → **invoke `lift-score-improve` skill first** before reverting or escalating
    - Xbox crash / hang / ACCESS_VIOLATION → **invoke `lift-crash-signals` skill**
    - Wrong visual output / silent wrong behavior → **invoke `lift-crash-signals` skill** (toggle-bisect section)

## Ghidra MCP availability (required)

- Before the first `ghidra` or `ghidra-live` MCP tool call in a task, run
  `rtk python3 tools/audit/check_ghidra_mcp.py`.
- If the preflight fails, or if any `ghidra`/`ghidra-live` MCP tool call fails
  due to connection/timeout/unavailable errors, stop immediately and do not
  retry in the same response.
- Tell the user exactly: `You might have forgotten to start
  tools/mcp-servers.sh or ghidra may not be running?`

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
  3. Pull callers/callees (`get_function_callers`/`get_function_callees`) with bounded limits.
  4. Pull full disassembly only when decompiler output is ambiguous or ABI-critical.
- Prefer one target per run; do not batch unrelated functions in one analysis pass.
- In reports, summarize evidence and include only the minimum assembly needed to justify claims.

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
  New `@<reg>` entries must also be added to `tools/kb_reg_baseline.json`.
- Do not reorder or repack structs without binary evidence and matching asserts.

## Commit discipline

- Do not write freeform lift commit messages.
- After staging lift changes, run `rtk python3 tools/audit/generate_lift_commit.py --batch-name "<short description>" > /tmp/commit_msg.txt`.
- Commit with `rtk git commit -F /tmp/commit_msg.txt` only after reviewing the generated message.

## Output expectations

Follow the output format from `halo-xbox-re`
(see `docs/references/output-schema.md` for full detail).
