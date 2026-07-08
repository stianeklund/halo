---
name: expr-simplify
description: "simplify expression, expression simplification, redundant cast, simplify cast, collapse temporaries, simplify arithmetic: Carefully simplify casts, temporaries, and arithmetic in lifted code — with hard bans around float reassociation, signedness/width changes, aliasing, global re-reads, and evaluation order. Per-expression VC71 gate; anything that moves the match gets reverted, not argued with."
---

# Expression Simplification

The decompiler output is full of noise (`(int)(uint)(ushort)x`, chained temporaries)
— but some of that "noise" is load-bearing codegen shape. Simplify only what is
provably inert, one unit at a time, and let VC71 arbitrate.

## Hard bans (never simplify these)

1. **Float arithmetic order/association.** No `a*b + a*c → a*(b+c)`, no reordering
   operands, no folding through intermediates. x87 (`-mno-sse`) rounds at each step;
   the shape *is* the semantics. Cross products keep their exact subtraction order
   (lift-learnings, decompiler-traps §4).
2. **Comparison direction with floats.** `x <= 1.0f` and `!(1.0f < x)` differ under
   NaN (§21/§23; the actor_looking NaN guard). Never flip a compare to "read better".
3. **Cast width/signedness.** `(int)(short)x` encodes a MOVSX; `(uint16_t)` vs
   `(int16_t)` changes the instruction (§24). Only delete a cast you can prove is a
   no-op at the exact operand width — and the LOADW census must agree.
4. **Global loads in loops.** Re-reads of globals/pointers match the original's
   volatile-like behavior (feedback: re-read globals in loops). Never hoist or CSE them.
5. **`(int)float_expr` stays a cast** — it compiles to the `_ftol2` intrinsic; don't
   "clean it up" into anything else.
6. **Spill/reload temporaries.** A temporary that looks redundant may force the
   store+reload MSVC emitted (lift-score-improve: reload-from-mem, accumulator-variable
   levers). If deleting a temp changes the score, it was structure, not noise.
7. **Aliased writes.** Between a store through one pointer and a read through another
   (or a `local_XX` that is a buffer interior, §2/§5), no motion of any kind.

## Safe simplifications (still gated)

- Deleting genuinely redundant same-width casts (`(int)an_int_expr`).
- Splitting an unreadable mega-expression into named C89 temporaries **without changing
  evaluation order or rounding points** — introducing temps is usually safer than
  removing them.
- De-duplicating identical sub-expressions ONLY for integer/pointer values whose
  operands provably cannot change between uses (no intervening calls or stores).
- Replacing decompiler artifacts already covered by `draft_decompiler.py` rewrites
  (CONCAT/ZEXT forms) — run that tool's patterns rather than hand-rolling.

## Procedure & gate

Work in units of ONE expression/function. After each unit:

```bash
rtk python3 tools/verify/vc71_verify.py src/halo/<path>/<file>.c
rtk python3 tools/verify/vc71_regression.py check --source src/halo/<path>/<file>.c
rtk python3 tools/audit/check_lift_hazards.py --changed-only
```

Score moved (either direction) → revert the unit. **Do not iterate toward the score**;
a simplification that needs tuning is a rewrite in disguise. FPU-heavy functions
additionally deserve `--fpu-only` inspection, and if a delinked ref is missing the
function is off-limits (see `cleanup-baseline`).

Commit per TU, simplification-only. Anything reverted gets a one-line
`kept-as-is: codegen-sensitive` note for the report.
