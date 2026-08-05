---
name: const-enum-recovery
description: "magic number, named constant, enum recovery, recover enum, flag bits, sentinel value, magic constant: Convert PROVEN magic numbers, flag bits, modes, and sentinel values into named constants/enums — value fidelity guaranteed bit-for-bit, names gated by naming-confidence tiers, float literals handled with IMM-WARN-aware care. Narrow-field stores use typed defines, not C enums."
---

# Constant & Enum Recovery

Two independent proofs are required: the **value** (what the binary compares/stores —
always provable) and the **meaning** (what the name claims — often not). Never let the
second lag the first silently.

## Evidence for meaning

- Assert/format strings naming the value or state (T1).
- `switch` cases whose handlers have T1/T2-named callees (each case value ↔ behavior).
- Flag bit tested at sites with proven behavior (e.g. object flags bit5 = at-rest, from
  the a30 flashlight investigation).
- Sentinel patterns: datum handles use `0xFFFFFFFF` (NONE) — check the existing name
  in `src/types.h` / kb.json first and reuse it.
- PC/CE symbol mirrors and the PDB corpus.

Value proven but meaning not → mechanical name that encodes the value, e.g.
`UNK_MODE_3`, `FLAG_BIT5_UNKNOWN` — still an improvement (greppable, one definition).

## Rules

1. **Bit-for-bit fidelity.** The named constant must produce the identical immediate.
   Never replace a literal with a computed expression (`(1<<5)|(1<<2)` is fine — the
   compiler folds it; `0.857651889f` → `cosf(0.5236f)` is NOT: that cos(30°) literal
   was a hand-transcribed constant, see the aim_grenade fix). For floats, keep the
   exact decimal that round-trips, and comment the bit pattern:
   `#define COS_30_DEG 0.857651889f /* 0x3F5B8CE2, original literal */`.
2. **Float literal vs const-pool loads.** VC71 emits `MOV [x],0x3f800000` for a literal
   store but `FCOMP [FLOAT_addr]` for compares via const-pool (lift-score-improve step
   3c). Introducing a named constant must not change which form the site compiles to —
   keep the same syntactic shape (`#define`d literal stays a literal).
3. **Narrow fields: no C enums.** C89 enums are `int`; a mode stored in an `int8_t`
   field (byte switch tables — see the VC71 shape recipes) must stay
   `typedef int8_t foo_mode; #define FOO_MODE_IDLE 0` so widths don't drift.
   True `int`-width discriminants may use a real `enum`.
4. **One definition point.** Shared values → `src/types.h` near their struct; TU-local
   → top of the TU. `rtk rg '<value>' src/` first — the constant may already be named;
   never mint a synonym.
5. **Flags document their register of truth**: which field they live in
   (`/* object_header.flags */`) so bit collisions are visible.
6. Optionally mirror into Ghidra (`create_enum`) so decompiles show the names too.

## Gate

```bash
rtk python3 tools/verify/vc71_verify.py src/halo/<path>/<file>.c --imm-only
rtk python3 tools/verify/vc71_regression.py check --source src/halo/<path>/<file>.c
```

Zero score movement and **no new `[IMM-WARN]`** — a new IMM-WARN means the named
constant changed a bit pattern. Commit per category (constants-only diff).
