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
   for full ABI rules).
4. Infer the narrowest defensible prototype (see
   `docs/references/prototype-inference.md`).
5. Produce a structurally faithful C lift:
   - preserve control-flow shape
   - preserve side-effect order
   - preserve pointer arithmetic and odd logic unless disproven
6. Write implementation in address-ordered position.
7. Update kb.json conservatively (see
   `docs/references/kb-update-policy.md`).
8. Run `python3 tools/maintain.py <source_file>`.

## ABI cautions

Key reminders (full rules in `docs/references/abi-and-calling-conventions.md`):

- cdecl: the first PUSH is the last C argument.
- For `@<reg>` or reverse-thunked paths, audit which registers the original
  caller expects preserved.
- Lifted C may legitimately clobber caller-saved EAX, ECX, EDX.
- Do not add `kb.json` `@<reg>` entries unless the implementation exists.
- Do not reorder or repack structs without binary evidence and matching asserts.

## Output expectations

Follow the output format from `halo-xbox-re`
(see `docs/references/output-schema.md` for full detail).