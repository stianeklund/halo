# Known VC71 Codegen Pattern Taxonomy

Apply these before running the permuter. Each pattern is a structural source fix
that closes a known gap between MSVC 7.1 codegen and what our clang build produces.
Patterns here that aren't applicable to a function are safe to skip.

---

## 1. FPU Comparison Direction

MSVC 7.1 uses `FCOM`/`FUCOMPP` followed by `FNSTSW AX` and `TEST AH, <flags>`.
The exact test constant depends on the comparison operator used in C.

| C operator | AH mask | Jump | Notes |
|-----------|---------|------|-------|
| `<`  (lt) | `0x05`  | `JP` followed by `JNE` | Both flags checked |
| `>=` (ge) | `0x01`  | `JNE` only | |
| `>`  (gt) | `0x41`  | `JP` or `JBE` | |
| `<=` (le) | `0x05`  | different sequence | |
| `==` (eq) | `0x44`  | `JNP`+`JE` | |

**How to spot:** VC71 verify reports large unmatched instruction blocks around
TEST/JNE sequences. The decompiler will show a comparison that looks correct
semantically but produces the wrong AH mask when compiled.

**Fix pattern:** Flip the comparison operator and/or swap operands.
```c
// Wrong (produces AH=0x01 path, MSVC wanted AH=0x05)
if (a < b) { ... }
// Correct
if (b > a) { ... }  // or negate the branch
```

**Branchless bool-gated buffers:** When MSVC emits `SETNZ` + `DEC` + `AND` to
conditionally zero a buffer, write:
```c
int mask = -(int)(condition != 0);  /* all-ones if true, 0 if false */
value &= mask;
```

---

## 2. Goto-Merge Returns

MSVC consolidates multiple return paths into a single epilog via `goto`. Clang
may generate separate epilogs, changing regalloc and instruction count.

**How to spot:** VC71 verify shows extra `LEAVE`/`MOV EAX,N` at the end of
several non-last basic blocks. The decompiled function has multiple `return`
statements that could share cleanup code.

**Fix pattern:** Introduce a goto to share return logic:
```c
int result;
// ... body using result ...
result = some_value;
goto done;
// ...
result = other_value;
done:
return result;
```

Prefer this when two or more return paths set the same variable and/or do the
same cleanup (e.g., freeing a resource, resetting a global).

---

## 3. Struct Field Rotation / Store Scheduling

MSVC reorders stores for scheduling. Ghidra reassembles them in instruction
order, which may produce wrong-looking struct initializer ordering in the
decompile.

**How to spot:** Struct member initialization order in the decompile does not
match disassembly `MOV [EBP±N], <val>` store order.

**Fix:** Derive the correct order from disassembly. Lay out stores in exactly
the order they appear in the disassembly:
```c
/* Wrong (decompiler-reordered): */
obj->field_a = x;
obj->field_b = y;
obj->field_c = z;

/* Correct (disassembly-ordered): */
obj->field_c = z;
obj->field_a = x;
obj->field_b = y;
```

---

## 4. Combined `&&` Condition Hoisting

MSVC sometimes hoists the simpler sub-expression of an `&&` condition to avoid
repeated evaluation, changing the basic block structure.

**How to spot:** VC71 verify shows an extra block containing the second operand
before the branch that VC71 expects, or the sequence of CMP+JNE instructions
is in a different order than the decompiler's `if (a && b)` suggests.

**Fix:** Decompose the combined test:
```c
/* May produce wrong block order: */
if (a && b) { do_thing(); }

/* Produces correct block layout: */
if (a) {
    if (b) {
        do_thing();
    }
}
```

---

## 5. `volatile` Local for Forced Reload

When MSVC keeps re-reading a value that our optimizer caches in a register, a
`volatile` cast forces the reload:

```c
int val = *(volatile int *)&some_local;
```

Use sparingly — only when the disassembly shows a `MOV EAX, [EBP-N]` inside
a loop that our code optimizes away.

---

## 6. Condition Swap / Else-If Inversion

MSVC sometimes inverts if/else ordering compared to what the decompiler shows.
This happens when the false branch is shorter and MSVC puts the shorter path
first for branch prediction.

**How to spot:** The true/false block bodies are swapped in the VC71 disassembly
relative to the decompiler's reading.

**Fix:** Negate the condition and swap the branches:
```c
/* Decompiler version: */
if (x > 0) { A; } else { B; }

/* MSVC-layout version: */
if (x <= 0) { B; } else { A; }
```

---

## 7. `int16_t` / `movswl` Chains

When a value flows through a `short` cast before being used in an expression,
MSVC emits `MOVSWL` (sign-extend to 32-bit). The decompiler often loses the
intermediate cast.

**How to spot:** `[FPU-WARN]` output or mismatched `MOVSWL` instructions in
the vc71 diff. The decompile may show `(int)some_field` where it should be
`(short)some_field`.

**Fix:** Add the explicit `(short)` or `int16_t` cast at the point of truncation.

---

## 8. `noinline` Attribute for Small Helpers

MSVC sometimes does NOT inline a short helper even when it looks inlineable.
If a small function call shows up in the decompile but the disassembly shows
a CALL to a separate address that matches an existing lifted function, add
`__attribute__((noinline))` to force a call site rather than inlining.

---

## How to Apply Patterns Before Permuting

1. Read the target function's current source.
2. Run `rtk python3 tools/verify/vc71_verify.py <source_file> --function <name> --show-diffs`
   to see the exact instruction-level diff.
3. Identify which pattern(s) account for the largest diffs.
4. Apply the pattern fix to the source.
5. Re-run vc71_verify to confirm the score improved.
6. Then run the permuter for final last-mile adjustments.

**Pattern fixes go through the same accept/revert gate as permuter candidates.**
A pattern fix that drops any TU neighbor's score must be reverted.
