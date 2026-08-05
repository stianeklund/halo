---
name: halo-xbox-re
tier: agent
triggers: ["lift", "lifting", "ported", "porting", "ghidra", "decompile", "decompil", "cachebeta", "kb.json", "binary evidence", "@<reg>", "reverse engineer"]
description: "Halo CE Xbox reverse engineering, binary evidence, disassembly, Ghidra, ABI, structs, globals, unknowns, output contract: core doctrine for any RE/lift analysis."
---

# Halo Xbox RE Doctrine

Use this skill for any work involving Halo CE Xbox reverse engineering or
binary analysis. It defines the methodology; operational workflows live in
`halo-re-lift`, `halo-verify-debug`, `halo-build-xemu`, and `halo-xbdm`.

## Target Binary Contract

The binary is the source of truth. The target executable is **Halo Xbox debug build 2276** (`halo-patched/cachebeta.xbe`, MD5 `c7869590a1c64ad034e49a5ee0c02465`, version `01.10.12.2276`, dated Oct 12, 2001).
- It is a **debug build** with richer symbols/asserts; do not swap in a retail `default.xbe`.
- All `kb.json` VAs are absolute virtual addresses into THIS file.

## Tooling Standard

- **Ghidra + `ghidra-xbe` extension** (`github.com/XboxDev/ghidra-xbe`) is the community standard for reverse engineering `cachebeta.xbe`.
- Python + `capstone` (x86 32-bit) for quick command-line disassembly reads.

## Ground Rules

- Unknown is better than wrong.
- Inspect both decompilation and disassembly before concluding.
- **Confirm Struct Offsets in `src/types.h`:** ALWAYS confirm field offsets in `src/types.h`. NEVER guess a struct field — a wrong field offset is a wrong decompilation. Name unknown fields `unk_<hexoffset>`.
- **Struct Refinement Doctrine:** Prefer refining `src/types.h` structs (e.g. `game_globals_t`, `players_globals_t`) to replace raw-offset casts (`[ptr+0x24]`) by splitting `unk_N[]` arrays without altering overall struct size.
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
