---
name: halo-re-lift
description: Repo-specific Halo CE Xbox reverse-engineering and lift workflow
---

# Halo RE Lift

Use this skill whenever work involves Halo CE Xbox reverse engineering, binary
verification in Ghidra, faithful C lifts, or conservative `kb.json` updates.

## Use this skill for

- Lifting a function from `cachebeta.xbe` or `default.xbe`
- Reviewing a proposed lift for ABI or layout fidelity
- Resolving prototypes, struct fields, globals, or `@<reg>` thunks
- Updating `kb.json` or related metadata after binary-backed analysis

## Ground rules

- The binary is the source of truth.
- Inspect both decompilation and disassembly.
- Unknown is better than wrong.
- Preserve ABI, stack behavior, field offsets, packing, and side-effect order.
- Reuse existing project and Xbox types before inventing new ones.
- Do not add empty stubs.
- Do not add `kb.json` `@<reg>` entries unless the implementation exists.
- Do not reorder or repad structs without binary evidence and matching asserts.

## Analysis checklist

1. Resolve the target by name or address in `kb.json` and Ghidra.
2. Gather context before editing:
   - callers and callees
   - touched globals and strings
   - imports and likely subsystem or object grouping
   - any existing declarations in source and `kb.json`
3. Cross-check decompilation against raw disassembly:
   - operand sizes
   - raw `CALL` targets
   - register-passed arguments
   - interleaved MSVC pre-pushes
4. Infer the narrowest defensible prototype from:
   - `PUSH` count and cleanup
   - caller use of `EAX`
   - calling convention evidence
5. Produce a structurally faithful C lift:
   - preserve control-flow shape
   - preserve side-effect order
   - preserve pointer arithmetic and odd logic unless disproven
6. Write the implementation in address-ordered position.
7. Update `kb.json` conservatively when evidence supports it.
8. Run `python3 tools/maintain.py <source_file>` after source edits.

## ABI cautions

- Remember cdecl push order: the first `PUSH` is the last C argument.
- For `@<reg>` or reverse-thunked code paths, audit which registers the original
  caller expects preserved.
- Lifted C may legitimately clobber caller-saved `EAX`, `ECX`, and `EDX`; do
  not rely on them surviving unless the ABI requires it.
- Return addresses and other critical state must remain stack-safe.

## Output expectations

Report findings under:

- Target
- Confirmed
- Inferred
- Uncertain
- Proposed code or exact change made
- `kb.json` updates
- Validation
