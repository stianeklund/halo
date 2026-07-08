---
name: control-flow-cleanup
tier: agent
triggers: ["control flow cleanup", "simplify goto", "goto cleanup", "restructure loop", "loop restructure", "simplify branches", "flatten nesting"]
description: Simplify gotos, loop forms, branch nesting, and early exits in lifted code — the HIGHEST-risk cleanup category, allowed only behind behavioral gates (equivalence/golden harness) on top of the VC71 gate, because structure edits legitimately change codegen. Default posture is Preserve Shape; this skill bounds the exceptions.
---

# Control Flow Cleanup

Mission doctrine is **Preserve Shape** — original control flow is evidence. This skill
exists to bound the rare justified exceptions (decompiler-artifact gotos, duplicated
exit blocks), not to make restructuring routine. If the function is readable enough,
stop here.

## Preconditions (strictest in the family)

1. `cleanup-baseline` block exists AND at least one **behavioral oracle** is available:
   - equivalence: `unicorn_diff.py` viable for the target (leaf, or `--allow-stubs`
     with adequate coverage/confidence), or
   - golden harness case in `src/halo/test_harness.c`, or
   - runtime fixture (`ab_check.py`) covering the code path.
   No behavioral oracle → **category blocked** (record as a `cleanup-gap-audit` gap).
2. All lower-risk cleanup for the TU already committed (don't mix).
3. The candidate rewrite is written down first: which goto/loop, what replaces it, why
   the orders of side effects, early exits, and short-circuit evaluation are identical.

## Known traps

- **Ghidra while-loop inversion**: the decompiler's `do/goto` form has an inverted
  condition vs the natural `while`; converting requires negating it
  (feedback: ghidra-while-loop-condition). Verify against disassembly, not intuition.
- **Early exits ARE the NaN/degenerate path.** Guard clauses like `if (!(m2 >= 1e-8f))`
  are NaN-inclusive by construction (§23); restructuring into `if (m2 < 1e-8f)`
  changes NaN behavior.
- **Merged exit blocks**: VC71 cross-jump tail merging means one exit label in the
  reference may be several logical exits; splitting/merging them shifts the match
  (lift-score-improve step 3b).
- **`switch` shape**: dense value sets compile to jump tables, sparse to compare
  chains; converting between `switch` and `if/else` chains changes which one you get.
- **Loop-carried state**: watch §4 (parameter corruption by loop) and §8 (accumulator
  misread) when hoisting anything across an iteration boundary.

## Gates (both, in order)

1. **Byte gate**: `vc71_verify.py` + `vc71_regression.py check --source <file>`.
   Unlike other cleanup categories a small drop is *possible* here — but the default
   policy is still zero: accept a drop only if the user explicitly opted into
   readability-over-match for this TU, and record old→new in the report.
2. **Behavior gate** (mandatory, even at zero drop):
   ```bash
   rtk python3 tools/equivalence/unicorn_diff.py <target> --seeds 100 --allow-stubs --mem-trace
   # and/or the golden harness / ab_check lane recorded in the baseline
   ```
   Any divergence → revert. Park the attempt
   (`artifacts/parked/patches/`, per the park-never-discard policy) with the diff and
   the divergence, so the analysis isn't lost.

Commit per function (not per TU) for this category, message stating the oracle used
and its result. Runtime misbehavior discovered later → `cleanup-regression-triage`,
then the `debug` family.
