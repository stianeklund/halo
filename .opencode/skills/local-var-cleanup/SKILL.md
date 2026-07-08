---
name: local-var-cleanup
description: "rename locals, local variable cleanup, local cleanup, uvar, ivar, fvar, decompiler locals, mechanical names: Rename decompiler-style locals (local_NN, uVar3, fVar1, iVar7) into useful MECHANICAL names without inventing unsupported semantics. Rename-only diffs; codegen must be byte-identical. Semantic names require naming-confidence evidence tiers."
---

# Local Variable Cleanup

Ghidra names (`local_44`, `uVar3`, `fVar1`) carry zero information; wrong semantic
names carry negative information. The target is the useful middle: **mechanical names
that describe role, not meaning**.

## Vocabulary (T3 mechanical — allowed without evidence)

| Role | Names |
|---|---|
| Loop counters / indices | `i`, `j`, `elem_index`, `slot` |
| Counts / sizes | `count`, `len`, `size_bytes` |
| Pointers | `elem`, `entry`, `cur`, `out`, `src`, `dst` |
| Accumulators / temporaries | `sum`, `acc`, `tmp_f`, `dot`, `mag_sq` |
| Return / result | `result`, `ok` |
| Flags / conditions | `done`, `found`, `is_valid` (only if the test proves it) |

Semantic names (`player_index`, `damage_scale`) require a T1/T2 citation per
`naming-confidence`. When in doubt, stay mechanical — `scale_q` beats a guessed
`shield_multiplier`.

## Rules

1. **Rename-only diffs.** No type changes, no moved declarations, no merged/split
   variables, no initializer edits in the same commit. C89 declaration blocks stay
   exactly where they are.
2. **Do not merge decompiler duplicates.** Two `local_XX` that "obviously hold the same
   value" may be MSVC's spill/reload pattern that the match depends on
   (lift-score-improve: reload-from-mem lever) — merging is `expr-simplify` territory
   with its own gate, not a rename.
3. **Buffer-alias suspects keep their offset visible.** If a `local_NN` was (or could
   be) an interior view of a buffer (lift-learnings §2/§5 — check
   `EBP_offset = NN−4` against buffer extents before renaming), name it as such
   (`dmg_params_impact /* damage_params+0x48 */`) or leave it — never a fresh
   standalone name that hides the aliasing.
4. **Keep the Ghidra origin when non-trivial.** For frame-mapped locals whose EBP slot
   mattered in analysis, keep a `/* local_44, EBP-0x40 */` breadcrumb so future disasm
   cross-checks (`frame_map.py`, crash triage) still line up.
5. **Consistency across the TU** — same role, same name, matching surrounding code.

## Gate

Renames cannot change codegen. After each file:

```bash
rtk python3 tools/verify/vc71_verify.py src/halo/<path>/<file>.c
rtk python3 tools/verify/vc71_regression.py check --source src/halo/<path>/<file>.c
```

**Any score movement (up or down) means the diff wasn't rename-only** — inspect the
diff for accidental type/init/scope changes and split them out.

Commit per TU, renames-only, message notes "mechanical local renames, codegen-neutral,
match unchanged".
