---
name: halo-xbox-re
description: Halo CE Xbox reverse-engineering doctrine, evidence rules, and output contract
---

# Halo Xbox RE Doctrine

Use this skill for any work involving Halo CE Xbox reverse engineering or
binary analysis. It defines the methodology; operational workflows live in
`halo-re-lift`, `halo-verify-debug`, `halo-build-xemu`, and `halo-xbdm`.

## Ground rules

- The binary is the source of truth.
- Unknown is better than wrong.
- Inspect both decompilation and disassembly before concluding.
- Reuse existing project and Xbox types before inventing new ones.
- Do not add empty stubs.
- Preserve ABI, stack behavior, field offsets, packing, and side-effect order.

## Efficiency guardrails

- Prefer bounded, evidence-first pulls over broad dumps.
- Use one strong artifact per claim (decompile snippet, disasm instruction block,
  or callsite), not redundant copies of all three.
- Request full-function disassembly only when needed to resolve uncertainty.
- When touching multiple related functions, prefer batch APIs where available.

## Evidence policy

Every claim in output must carry an evidence label:

- **Confirmed** — binary-backed facts only (disassembly, callsites, register
  behavior, operand widths, string references).
- **Inferred** — the best narrow interpretation with supporting evidence.
- **Uncertain** — unresolved possibilities, conflicts, or weak guesses.

If evidence is weak or conflicting, say so explicitly. If a fact cannot be
checked from available binary evidence, mark it Uncertain.

## Review checklist

Before finalizing any RE finding:

1. Resolve target in kb.json and Ghidra.
2. Gather context: callers, callees, globals, strings, imports, existing
   declarations.
3. Cross-check decompilation against raw disassembly (operand sizes, CALL
   targets, register args, interleaved MSVC pre-pushes).
4. Infer the narrowest defensible prototype.
5. Produce structurally faithful C (preserve control flow, side-effect order,
   pointer arithmetic).
6. Write implementation in address-ordered position.
7. Update kb.json conservatively when evidence supports it.
8. Run `python3 tools/analysis/maintain.py <source>` after source edits.

## Output format

Report findings under these sections (see `docs/references/output-schema.md`
for full detail):

- Target
- Scope
- Confirmed
- Inferred
- Uncertain
- Evidence
- Proposed code
- Proposed kb deltas
- Validation
- Open questions

## Detailed references

Load these when you need deep rules or edge cases:

| Concern | Reference |
|---|---|
| ABI, calling conventions, cdecl push order | `docs/references/abi-and-calling-conventions.md` |
| Prototype inference, parameter narrowing | `docs/references/prototype-inference.md` |
| kb.json update rules | `docs/references/kb-update-policy.md` |
| Full output schema | `docs/references/output-schema.md` |
| Memory save/forget rules | `docs/references/memory-policy.md` |
| Work selection priorities | `docs/references/work-selection.md` |
