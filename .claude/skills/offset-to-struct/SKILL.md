---
name: offset-to-struct
tier: agent
triggers: ["raw offset", "pointer arithmetic", "offset replacement", "replace offsets", "struct field access", "field access rewrite"]
description: Replace verified raw pointer arithmetic (*(T*)(base+0xNN)) with struct field access, preserving behavior and VC71 match. Requires the struct to exist with co()/cs() asserts first (struct-assert); every rewritten function is gated on zero match drop.
---

# Raw Offset Replacement

Mechanically replace `*(int16_t *)(obj + 0x2A)` with `((object_header *)obj)->field_2A`
— same bytes out of the compiler, radically more readable. This is a *transcription*,
not an interpretation step.

## Preconditions (hard gates)

1. `cleanup-baseline` block exists; target functions have a delinked reference
   (no reference → no byte oracle → do not rewrite offsets in that function).
2. The struct exists with `co()` asserts covering **every offset you will replace**
   (`struct-assert`). No assert for 0xNN → add it first or leave that access raw.
3. Clean tree; one TU per pass.

## Rewrite rules

- **Width must match exactly.** The cast type at the raw site (`int16_t*`, `uint8_t*`,
  `float*`) must equal the struct field's type. A mismatch is a *finding* (either the
  original lift or the struct is wrong — resolve before rewriting; see §24 LOADW).
- **Signedness must match.** `*(short*)` → a `uint16_t` field is not a valid rewrite.
- **Keep the base expression untouched.** Only the `+0xNN` dereference becomes a field
  access; do not "simplify" how `base` is computed in the same edit.
- **Array strides**: `*(float*)(base + i*0x28 + 0x0C)` → `((elem*)base)[i].position[0]`
  only when `cs(elem, 0x28)` is asserted.
- **Grouped consecutive stores caution.** MSVC emits `FSTP [EDI]; FSTP [EDI+4]…` for a
  single-base pointer walk (see lift-score-improve step 2). Struct member access usually
  compiles identically, but if the raw form used an explicit local base pointer
  (`float *up = ...; up[0]=…`), keep that shape — replacing it with three `s->a; s->b;
  s->c` member stores can shift codegen. The score gate below catches it either way.
- **Stack-overlap sites are off-limits.** Accesses flagged by
  `buffer_alias_detector.py` or documented as MSVC layout overlaps (lift-learnings §2,
  the `marker_buf+0x60` class) encode aliasing, not fields — leave raw with a comment.

## Per-function gate

After each function (not each file):

```bash
rtk python3 tools/verify/vc71_verify.py src/halo/<path>/<file>.c
rtk python3 tools/verify/vc71_regression.py check --source src/halo/<path>/<file>.c
rtk python3 tools/audit/check_lift_hazards.py --changed-only
```

- Match must be **≥ baseline, warn-census unchanged**. A drop → revert that function's
  rewrite (keep the rest), record it in the report as `kept-raw: codegen-sensitive`,
  and move on. Do not chase the drop by mutating the struct.
- New hazard WARNs in touched files are blockers, not noise.

## Commit

One commit per TU, offsets-only (Separation rule). Then
`rtk python3 tools/verify/vc71_regression.py update --source <file>` if any score
*improved*, so the floor captures it.
