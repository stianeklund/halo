---
name: lift-silent-bugs
description: Checklist for non-crashing silent correctness bugs — wrong colors, features doing nothing, wrong positions, wrong scalars. Run after writing any lift before deploying to Xbox. Covers float-as-pointer (§6), accumulator misread (§8), builder-count ignored (§11), void-EAX implicit return (§16), address-offset mis-read as value (§17).
---

# Silent Bug Checklist

**Invoke this skill when:**
- A lift looks correct from VC71/instruction diff but you suspect silent wrong behavior
- A visual feature is wrong but not crashed (wrong color, wrong position, wrong scale)
- A game feature does nothing with no error (no items, no score, no nav point)
- You are reviewing a lift before activating it on a real Xbox

These bugs do NOT trigger assertions and do NOT change VC71 match. The box is
the only oracle. Run this checklist BEFORE deploying.

Source: `docs/lift-learnings.md` §6, §8, §11, §16, §17.

---

## Check 1 — Float-as-pointer bit smuggling (§6)

**Symptom:** Persistent wrong colors (yellow/white tint), wrong scale factors,
lighting that doesn't respond to distance.

**Detection grep:**
```bash
grep -n '(float)(int)' src/<file>.c
grep -n '(int)(float' src/<file>.c
```

Any `(float)(int)var` where `var` was assigned via `(T*)(int)(float_expr)` is
a bug. The cast chain `(float)(int)` numerically truncates (0.75 → 0 → 0.0)
instead of preserving the IEEE 754 bit pattern.

**Fix:** Use a proper `float` variable:
```c
/* Wrong: truncates the float to integer then back */
FUN_00139e50(..., (float)(int)psVar3, ...);

/* Right: pass the float directly */
FUN_00139e50(..., fVar_distance_scale, ...);
```

**Note:** `*(type*)&local_XX` is ambiguous. Check whether the two uses are the
same logical value (genuine type-punning → use `memcpy`/union) or unrelated
variables sharing the same EBP slot (lifetime reuse → just declare two separate
locals; no union needed).

---

## Check 2 — Running accumulator misread as constant (§8)

**Symptom:** A loop-based selector always returns the same result or -1.
Features that never trigger (no spawns, no selections).

**Detection:** In the decompile, look for `(float)0` or `0` inside a loop body
that is also **written** inside the loop:

```bash
# In disassembly: search for slot that is both loaded AND stored in loop body
# FILD [EBP-N] + (later) MOV [EBP-N], EAX = running accumulator, not constant
```

Tell: the slot has both a LOAD (`FILD [EBP-N]`, `MOV EAX,[EBP-N]`) and a STORE
(`MOV [EBP-N],EAX`, `FISTP [EBP-N]`) inside the loop. If the decompile shows
`(float)0` as the value, Ghidra lost the data flow — the slot is seeded BEFORE
the loop and updated each iteration.

**Fix:**
```c
/* Wrong: accumulator lost, always tests 0 */
for (i = 0; i < count; i++) {
    if ((float)0 - weight[i] < 0) return item[i];
}

/* Right: seed from RNG before loop, update each iteration */
int accum = random_range(0, total_weight);
for (i = 0; i < count; i++) {
    accum = (int)((float)accum - (float)weight[i]);
    if (accum < 0) return item[i];
}
```

---

## Check 3 — Builder returns count; consumer ignores it (§11)

**Symptom:** Assert `"place < MAXIMUM_PLAYERS"` or similar bounds assertion in
a search loop. The crash is in the *consumer*, not the builder.

**Detection:** When a function calls a builder that fills a buffer AND calls it
with `(void)` or discards the return:

```bash
grep -n '(void).*(' src/<file>.c        # discarded return values
grep -n 'data_iterator_next\|sorted.*build' src/<file>.c
```

If the builder returns a count and the caller searches with a hard upper bound
(`i < 16`, `i < MAXIMUM_PLAYERS`), replace the hard bound with the returned
count. The datum can pass `datum_get` but still be absent from the iterator's
output (active bit = 0); the count is the only safe upper bound.

---

## Check 4 — Void-return function that implicitly returns EAX (§16)

**Symptom:** Wrong positions, wrong coordinates, wrong handle — the callee wrote
the correct data but the caller read it from a garbage pointer.

**Detection:**
```bash
# In the callee's disassembly tail — does it end with:
# MOV EAX,[EBP+8]  (return first param pointer)
# LEA EAX,[EBP-N]  (return local buffer address)
# before RET?

# In unported callers — does the call site assign the return value to a pointer?
grep -n 'puVar.*FUN_\|pVar.*FUN_\|= (.*\*)FUN_' src/<file>.c
```

If yes: the function must NOT be declared `void`. It must return the pointer
in EAX to match `MOV EAX,[EBP+8]; RET` in the original:

```c
/* Wrong */
void game_engine_get_goal_position(int *out, short idx) { ... }

/* Right */
int *game_engine_get_goal_position(int *out, short idx) {
    ...
    return out;   /* matches MOV EAX,[EBP+8] at original RET */
}
```

Also add the address to `VOID_BUT_RETURNS_EAX` in `check_lift_hazards.py`.

---

## Check 5 — Address offset mis-read as value addition (§17)

**Symptom:** Wrong scalar — wrong width, wrong aspect ratio, Y computed from X
field, etc. No crash.

**Detection grep:**
```bash
grep -nE '\*\([a-zA-Z_]+ \*\)0x[0-9a-f]+ \+ [0-9]' src/<file>.c
grep -nE '\*\([a-zA-Z_]+ \*\)\([a-zA-Z_]+ \+ [0-9]+\) \+ [0-9]' src/<file>.c
```

For every hit: check whether a sibling expression reads `BASE+N` as an address
directly. If so, the `+N` belongs INSIDE the cast:

```c
/* Wrong: +2 is a value addition, reads wrong field */
*(short*)0x506588 + 2

/* Right: +2 is an address offset, reads adjacent field */
*(short*)(0x506588 + 2)
/* or */
*(short*)0x50658a
```

Cross-confirm in disasm: a single `movswl [0x50658a]` with no trailing `add`
proves the constant is part of the address.
