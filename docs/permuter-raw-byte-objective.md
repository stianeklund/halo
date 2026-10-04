# Guide: switch the permuter search objective to raw aligned bytes

## Status (2026-10-06): implemented

- `--objective raw` is now the default in `tools/permuter/run.py`.
- Prerequisites 1-2 were solved by splicing, not by fixing extraction.
  `tools/permuter/splice_compile.py` compiles each candidate body inside a copy
  of the real TU through `vc71_verify.compile_vc71`, using the gate's
  per-function options. On RGBToColor, `base.o` now scores 50/96, the same as
  the gate. The extracted `base.c` can still be wrong, but the baseline guard
  (exit 4) catches it.
- Prerequisite 3 (varargs) is untested under splicing.
- There is no size cap. A whole-TU compile takes about 1 s and dominates the
  audit cost. The measured rate was 5.7 candidates/s with `-j4` on a small TU.
- Penalties are 20 units per unit of gate-counter increase or lost matching
  bytes. They are not infinite, so the search can still move through those
  candidates.
- Validation 1: the first RGBToColor raw winner (50/92) moved the struct store
  above `result.rgba[3] = 0`, which made it a semantic mutation, and
  `audit_candidate` reported it as OK. Validation 2 (FUN_00108fa0, LCS 100%,
  raw 81/82): baseline parity held, and 150 s found no improvement.

Goal: rank permuter candidates by the same metric the byte gate enforces
(`aligned_byte_match.byte_accuracy`), not mnemonic-LCS. Do the prerequisites
first, or the search optimises a number the gate never measures.

## Why

- LCS (`third_party/decomp-permuter/src/scorer.py:103-133`, `_score_lcs`) compares
  mnemonic names only. About 100K same-mnemonic differences (register, operand,
  branch target, stack offset, immediate) are invisible to it.
- On RGBToColor, 4 of 4 reported LCS "improvements" (65-70% vs 47% base) scored
  lower on raw bytes (28-48% vs 48.5%). The gate would reject all of them.
- `--best-only` (`run.py:1173`) discards candidates that help bytes but not LCS
  before any rerank, so a rerank-only hybrid is not enough.

## Prerequisites (do first, about 1-2 days)

1. **Compile flags.** `tools/permuter/compile.sh:147` hard-codes `/O2 /Oy-`.
   The gate uses per-function options (`_function_opt`,
   `tools/verify/raw_xbe_structural.py:1628`). Pass them from `run.py` via
   `VC71_OPT` / `VC71_FP`.
2. **Baseline parity.** The permuter's `base.o` raw score must equal a fresh gate
   measurement of the real source file. On RGBToColor it was 48.5% (permuter)
   vs 52.1% (gate). Probable cause: the extracted `base.c` differs from the real
   TU. Fix the extraction.
3. **Varargs targets.** `base.c` fails to compile for varargs wrappers
   (e.g. TIFFGetField 0x65e90). Fix or skip with a clear message.
4. **Baseline guard.** Add a run-time check (exit code 4) that fails when the
   `base.o` raw score differs from the fresh gate measurement. The existing
   "iterations ran" guard does not catch a broken metric.

## Implementation (about 1 day)

1. `scorer.py` (near line 144): add `score_algorithm = "raw_aligned"`. Call
   `raw_xbe_structural.audit(candidate_obj, function, address)` (`:1293`). It
   takes one COFF file and needs no link step.
   - Score = unmatched bytes (`compared - matching`), plus large penalties for any
     increase over base in: mismatched bytes, unpaired instructions, uncertain
     relocations, `alignment_ambiguous_steps`. These are the gate's hard-fail
     conditions (`docs/byte-regression-ci.md:19-23`).
   - Tie-break with the old LCS score.
   - If `audit()` raises, score the candidate as rejected and count the failures.
     Abort the run if every candidate fails (otherwise all score infinity and the
     run looks healthy).
2. `run.py`:
   - Pass the function address (`_target_address`) to the scorer through
     `settings.toml`.
   - Set `score_algorithm="raw_aligned"` where it currently sets `"lcs"`
     (`:1118-1135`).
   - Switch the baseline (`:1145`) and the final rerank (`:1266-1296`) to the same
     objective. Add raw columns to `lcs_results.txt`.
3. Large functions: `audit()` is O(n*m) pure Python (0.68 s at 1737 instructions).
   Add a size cap and fall back to LCS above it.
4. Keep `audit_candidate` (`run.py:1319`). Byte scoring catches changed constants
   but not use-before-init hoists, so the semantic-mutation audit stays required.

## Cost reference

Compile about 0.34 s per candidate; scoring 8 ms (38 instructions) to 0.68 s
(1737 instructions). Negligible for the [85, 98]% target band.

## Validate

1. Re-run RGBToColor (`src/halo/bitmaps/libtiff/tif_write.c`). The 4 old LCS
   winners must now rank below base, and the baseline guard must pass.
2. Run a target that is below 100% on raw bytes but 100% LCS. The permuter should
   now find a mutation, where before it had nothing to climb.
3. Confirm any winner with `rtk python3 tools/verify/byte_regression.py local`
   before committing.
4. Record the change in `.claude/skills/permuter-campaign` and the permuter memory
   hub (the old "LCS objective" notes become stale).

## Watch for

- Alignment-ambiguity tie-breaks can make scores jitter between runs.
- Never accept a winner on score alone. Always run with `--keep --output-dir`
  and audit the diff.
