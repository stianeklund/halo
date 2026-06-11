---
name: lift-score-improve
description: Checklist for recovering VC71 match before declaring a structural ceiling. Invoke when score is 65–84% and the gap looks "structural".
---

# Lift Score Improvement Checklist

**Invoke this skill when:**
- VC71 score is 65–84% and you are about to write "structural ceiling"
- Permuter returned no improvement (may be vacuous — see §12 of lift-learnings)
- A function with `static` large buffers has a suspiciously low score

Source: `docs/lift-learnings.md` §19 + §20.

---

## Step 0 — Check for the static-buffer ceiling first (§20)

This is the highest-leverage check and takes 5 seconds:

```bash
grep -n 'static.*avoid.*_chkstk\|static.*_chkstk\|chkstk linker' src/halo/ai/actor_looking.c
grep -rn 'static.*avoid.*_chkstk\|static.*_chkstk' src/
```

If any hit — convert that `static` declaration to a plain stack declaration.
`_chkstk` is now a no-op stub in `xbox_crt.c`; the linker error is gone.
Rerun VC71. **Score impact observed: +10pp** (77.3% → 87.1% on FUN_00025c10).

---

## Step 1 — cos()/sin() intrinsification (§19, technique 1)

Does the function use `x87_fcos`, `x87_fsin`, or `x87_fcos_mul`?

```bash
grep -n 'x87_fcos\|x87_fsin' src/<file>.c
```

If yes — wrap the call sites with:
```c
#if defined(_MSC_VER) && !defined(__clang__)
  result = (float)cos((double)x);   /* VC71 /Oi inlines as FCOS; shares ST0 */
#else
  result = x87_fcos(x);
#endif
```

When the same variable feeds both `cos` and `sin`, MSVC shares it on the FPU
stack as `FLD ST0` — the `x87_*` helpers each do their own `FLD [mem]`, so the
patterns diverge. **Recovered ~5 instructions on FUN_001a2160.**

---

## Step 2 — Pointer-base aliasing for consecutive stores (§19, technique 2)

Search for 3+ stores to consecutive offsets through the same base:

```bash
grep -n '\*(float\*)(.* + 0x[0-9a-f]\+)' src/<file>.c | head -20
```

Pattern that hurts:
```c
*(float *)(obj + 0x30) = a;
*(float *)(obj + 0x34) = b;
*(float *)(obj + 0x38) = c;
```

MSVC generates `FSTP [EDI]; FSTP [EDI+4]; FSTP [EDI+8]` only when it sees a
single base pointer. Fix:
```c
float *up = (float *)(obj + 0x30);
up[0] = a; up[1] = b; up[2] = c;
```
Applies to both read and write sides. **Recovered ~10 instructions on FUN_001a2160.**

---

## Step 3 — Early register-load hint for @<reg> params (§19, technique 3)

Check: does any `@<reg>` parameter have its first **use** far from the function
entry (many lines down)?

```bash
grep -n '@<' kb.json | grep "$(basename <file> .c)"  # find the @<reg> params
```

If a register param is first used deep in the function, MSVC may spill it and
reload later — producing extra load instructions. Fix:
```c
/* Force early register load to match MSVC's register flow */
float dir0 = direction[0];
float dir1 = direction[1];
float dir2 = direction[2];
```

Add these right after variable declarations, before any other logic.
**Recovered +11.5pp on FUN_001a1a10 (80% → 91.5%).**

---

## Step 4 — Run the permuter on the IMPROVED source

Only run the permuter AFTER applying techniques 0–3. The search space is much
smaller on improved source.

```bash
rtk python3 tools/permuter/run.py <function_name> -q --limit 120
```

**Verify the run is real** (not vacuous — §12): run WITHOUT `-q` first and
confirm it prints accruing iteration counts (hundreds), not an instant exit.

---

## Decision after all four steps

| After applying all four | Action |
|------------------------|--------|
| Score ≥ 90% | Commit |
| Score 85–89% | Commit; note permuter recommendation |
| Score < 85% | NOW it's a genuine structural ceiling — document which specific unmatched instructions remain (FPU comparison idiom, `@<reg>` prologue preamble, FLD ST(1) depth ref) so future sessions don't re-investigate |
