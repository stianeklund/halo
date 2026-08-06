---
name: name-cleanup
description: "rename locals, local variable cleanup, uvar, ivar, fvar, decompiler locals, mechanical names, magic number, named constant, enum recovery, flag bits, sentinel value: Rename decompiler-style locals and convert proven magic numbers into named constants/enums — both rename-class edits gated on byte-identical codegen."
---

# Name Cleanup — Locals & Constants

Two rename-class cleanup categories with identical gates (byte-identical codegen,
naming-confidence tiers). Both produce rename-only diffs.

---

## Part 1 — Local Variable Renaming

Ghidra names (`local_44`, `uVar3`, `fVar1`) carry zero information; wrong semantic
names carry negative information. The target: **mechanical names that describe role,
not meaning**.

### Vocabulary (T3 mechanical — allowed without evidence)

| Role | Names |
|---|---|
| Loop counters / indices | `i`, `j`, `elem_index`, `slot` |
| Counts / sizes | `count`, `len`, `size_bytes` |
| Pointers | `elem`, `entry`, `cur`, `out`, `src`, `dst` |
| Accumulators / temporaries | `sum`, `acc`, `tmp_f`, `dot`, `mag_sq` |
| Return / result | `result`, `ok` |
| Flags / conditions | `done`, `found`, `is_valid` (only if the test proves it) |

Semantic names (`player_index`, `damage_scale`) require a T1/T2 citation per
`naming-confidence`.

### Rules

1. **Rename-only diffs.** No type changes, moved declarations, merged/split
   variables, or initializer edits in the same commit.
2. **Do not merge decompiler duplicates.** Two `local_XX` that "obviously hold the
   same value" may be MSVC's spill/reload pattern — merging is `expr-simplify`
   territory with its own gate.
3. **Buffer-alias suspects keep their offset visible.** If `local_NN` could be an
   interior view of a buffer (check `EBP_offset = NN-4` against buffer extents),
   name it with the alias (`dmg_params_impact /* damage_params+0x48 */`) or leave it.
4. **Keep Ghidra origin when non-trivial.** For frame-mapped locals, keep a
   `/* local_44, EBP-0x40 */` breadcrumb.
5. **Consistency across the TU** — same role, same name.

---

## Part 2 — Constant & Enum Recovery

Two independent proofs: the **value** (what the binary compares/stores — always
provable) and the **meaning** (what the name claims — often not).

### Evidence for meaning

- Assert/format strings naming the value or state (T1)
- `switch` cases whose handlers have T1/T2-named callees
- Flag bit tested at sites with proven behavior
- Sentinel patterns: datum handles use `0xFFFFFFFF` (NONE)
- PC/CE symbol mirrors and PDB corpus

Value proven but meaning not → mechanical name: `UNK_MODE_3`, `FLAG_BIT5_UNKNOWN`.

### Rules

1. **Bit-for-bit fidelity.** The named constant must produce the identical immediate.
   Never replace a literal with a computed expression that changes bits (e.g.
   `cosf(0.5236f)` is NOT the same as the hand-transcribed constant `0.857651889f`).
2. **Float literal vs const-pool loads.** VC71 emits `MOV [x],0x3f800000` for literal
   stores but `FCOMP [FLOAT_addr]` for const-pool compares. A named constant must not
   change which form the site compiles to — keep the same syntactic shape.
3. **Narrow fields: no C enums.** C89 enums are `int`; a mode in `int8_t` must stay
   `typedef int8_t foo_mode; #define FOO_MODE_IDLE 0`.
4. **One definition point.** Shared → `src/types.h` near struct; TU-local → top of TU.
   `rtk rg '<value>' src/` first — never mint a synonym.
5. **Flags document their register of truth** (`/* object_header.flags */`).

---

## Gate (shared for both categories)

```bash
rtk python3 tools/verify/vc71_verify.py src/halo/<path>/<file>.c
rtk python3 tools/verify/vc71_regression.py check --source src/halo/<path>/<file>.c
```

- **Local renames:** any score movement means the diff wasn't rename-only — inspect
  for accidental type/init/scope changes.
- **Constants:** zero score movement AND **no new `[IMM-WARN]`** — a new IMM-WARN
  means the named constant changed a bit pattern.

Commit per TU, per category (renames-only or constants-only diff).
