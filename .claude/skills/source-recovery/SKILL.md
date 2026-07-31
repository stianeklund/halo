---
name: source-recovery
tier: user
description: Manifest-driven faithful source recovery for an existing Halo Xbox source file — /recover-source scopes debt, records evidence, applies only gated neutral or corrective changes, and reports every parked failure.
---

# /recover-source — Faithful Source Recovery

Use this skill as the sole end-to-end workflow for recovering readability and
source structure in an existing Halo Xbox translation unit. Start with a
manifest created by `tools/recovery/source_recovery.py`; never use an
untracked baseline or silently rewrite a source file.

## Required sequence

1. **Scope and baseline.** Confirm the exact `.c` path, inspect unrelated
   worktree changes without modifying them, create a manifest, and capture the
   candidate COFF and assertion metadata snapshots.
2. **Debt inventory.** Use the manifest's line-numbered inventory to select
   small items. Do not treat every numeric literal as an address or infer a
   semantic name from a pattern alone.
3. **Binary evidence and contracts.** Obtain disassembly, call-site, layout,
   and runtime evidence before changing behavior. Evidence-backed `kb.json`
   names, prototypes, and globals are permitted; `@<reg>` assignments are
   immutable. Keep unknowns explicit.
4. **Header placement.** Put recovered types, constants, and inline helpers in
   the proven Bungie header/TU location, not a convenient catch-all header.
5. **Struct, enum, and type recovery.** Recover widths, offsets, signedness,
   and padding from evidence, then prove layout with assertions. Names follow
   `naming-confidence`.
6. **Raw rewrites.** Replace raw calls, globals, and offsets only when the
   callee contract or field offset is evidence-backed. Preserve ABI, stack
   shape, evaluation order, side effects, and control flow.
7. **Mechanical names and comments.** Apply conservative local names and
   evidence comments. Mechanical readability changes must remain neutral.
8. **Exact and strict gates.** Run the manifest check, COFF neutrality guard,
   assertion metadata guard, strict VC71 regression check, and the relevant
   hazard/readability checks after each small change.
9. **Report.** Update item status, park unresolved work with a reason, and
   produce the manifest report.

## Fidelity rules

- Distinguish **neutral changes** (source/readability only, exact output
  unchanged) from **corrective fidelity improvements** (a proven mismatch in
  the current lift). Corrective changes require binary evidence and their own
  verification; do not call them cleanup.
- `naming-confidence` is cross-cutting. Use semantic names only when evidence
  supports them; use mechanical names or `unknown`/`field_XX` otherwise.
- Preserve Shape. Control-flow cleanup, restructuring, and speculative
  simplification are excluded from this workflow.
- If an item fails a gate, **park that item with a reason and continue** with
  independent items. Never weaken or bypass a strict gate to make progress.
- A skipped VC71 reference is reported as skipped, never as passed.
