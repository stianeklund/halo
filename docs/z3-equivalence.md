# Z3 + Unicorn Verification Enhancement

## What Changed

The equivalence testing infrastructure (`tools/equivalence/`) was expanded from a pure-leaf-only Unicorn statistical tester into a multi-strategy verification system with formal proofs, coverage-guided seeds, and non-leaf function support.

### Before

- `unicorn_diff.py` ran both oracle and lifted code in Unicorn with random + corner-case seeds
- Only **pure leaf functions** could be tested (any external relocation → rejected)
- Seed generation was random with hardcoded corner cases (INT_CORNERS, FLOAT_CORNERS)
- `leaf_cache.json` had **1 entry** after months of use — the tool was effectively unused
- No formal proofs — 100 random seeds provided statistical confidence only
- Non-leaf functions (the majority of the codebase) had zero behavioral verification

### After

| Capability | Before | After |
|---|---|---|
| Functions classifiable | 1 | **4,635** (1,065 leaf + 806 data_only + 2,763 stubbable) |
| Formal equivalence proofs | None | Z3 proves UNSAT → mathematically equivalent for ALL inputs |
| Seed generation | Random + corners | Z3 solves for branch-covering inputs + random + corners |
| Non-leaf support | Rejected | DIR32 auto-patching + callee stubbing via sentinel addresses |
| Target selector integration | +5 for leaf | +5 leaf, +3 data_only, +3 stubbable, +5 z3_proven |
| Pipeline integration | Unicorn only | Z3 equiv attempted first → falls through to Unicorn on timeout |

### New Files

| File | Purpose |
|---|---|
| `tools/equivalence/x86_to_z3.py` | x86-to-Z3 abstract interpreter. Lifts GPR ops, EFLAGS, x87 FPU (80-bit via FPSort(15,64)), control flow. 79 instruction mnemonics. |
| `tools/equivalence/z3_seeds.py` | CMP/TEST+Jcc pattern extraction → Z3 solver → branch-covering seed vectors |
| `tools/equivalence/z3_equiv.py` | Formal equivalence: lifts oracle + lifted to Z3, asserts output inequality, checks UNSAT |
| `tools/equivalence/stubs.py` | Relocation classification, DIR32 patching, REL32 sentinel-address call interception, StubManager |

### Modified Files

| File | Change |
|---|---|
| `tools/equivalence/coff_loader.py` | `FunctionSlice` now exposes `defined_symbols` for intra-object symbol resolution |
| `tools/equivalence/unicorn_diff.py` | Fixed false-negative leaf classification; added `--z3-equiv`, `--allow-stubs`, `--batch-classify`; auto-maps unmapped memory pages |
| `tools/equivalence/seeds.py` | Accepts `z3_seeds` parameter — Z3 branch seeds injected before random/corner seeds |
| `tools/lift_pipeline.py` | Passes `--z3-equiv --allow-stubs` to unicorn_diff; reports "Z3 PROVEN EQUIVALENT" distinctly |
| `tools/llm_auto_lift.py` | Reads extended `leaf_cache.json` schema; scores data_only +3, stubbable +3, z3_proven +5 |

## Why This Approach

### Z3 formal proofs vs more Unicorn seeds

Random testing can miss bugs that only trigger on specific inputs. A function that passes 10,000 random seeds can still have a bug triggered by `input == 0x7F800000` (infinity). Z3 exhaustively checks all inputs by encoding the problem as satisfiability — if UNSAT, the functions are identical for every possible input. No sampling, no probability.

The tradeoff: Z3 proofs only work on small-to-medium pure-leaf functions (the solver times out on deeply nested FPU expressions). For those functions, though, it's strictly stronger than any amount of statistical testing.

### Lightweight seed extraction vs full symbolic execution

The initial approach (full symbolic lifting → Z3 solver) produced deeply nested Z3 Array expressions that timed out on anything touching memory. The production approach extracts CMP/TEST+Jcc instruction patterns directly and solves for the comparison values — fast (sub-second), works on any function regardless of FPU complexity, and generates seeds that exercise each branch direction.

### DIR32 patching + auto-mapping vs loading XBE data sections

Many functions reference global constants (epsilon values, lookup tables) via DIR32 relocations. Rather than loading the full XBE data section (which would require parsing the XBE format and handling segment relocations), we auto-map zeroed pages on demand. This means the globals contain zeros instead of real values, so the oracle and lifted code may diverge on functions that depend on those values. But it still catches structural bugs (wrong control flow, wrong registers, stack corruption) which are the most common lift errors.

### Why not angr/Triton

- **angr**: Its value is whole-program CFG recovery and symbolic exploration. We already have Ghidra for decompilation, and our verification is post-lift comparison, not path exploration. angr's VEX IR lifter adds dependency weight without clear payoff.
- **Triton** (the concolic engine, not OpenAI's GPU compiler): No pip wheel for Python 3.12, requires building from source with cmake+LLVM. Our manual x86-to-Z3 lifter covers the 79 instruction mnemonics found in leaf functions and is debuggable. If the FPU subset proves too fragile, Triton is the fallback — but the current approach works.

## Usage

```bash
# Classify all delinked functions (run once, or after new delinked exports)
python3 tools/equivalence/unicorn_diff.py --batch-classify

# Test a pure leaf with Z3 proof attempt
python3 tools/equivalence/unicorn_diff.py vector3d_scale_add --z3-equiv

# Test a data_only function with DIR32 patching
python3 tools/equivalence/unicorn_diff.py magnitude3d --allow-stubs

# Both flags (what the pipeline uses)
python3 tools/equivalence/unicorn_diff.py <func> --z3-equiv --allow-stubs

# Pipeline runs this automatically after lift
python3 tools/lift_pipeline.py --target <name_or_addr> --verify-policy auto
```

## Verification Hierarchy

The pipeline tries the strongest verification first and falls through:

1. **Z3 formal proof** (leaf, ≤200 instructions, no transcendentals) → PROVEN EQUIVALENT or counterexample
2. **Unicorn + Z3 seeds** (leaf or data_only or stubbable) → statistical pass/fail with branch-covering inputs
3. **VC71 byte-match** (has delinked reference) → structural similarity percentage
4. **Build + ABI audit** (always) → compile + calling convention check

A Z3 proof is the strongest possible evidence — it supersedes VC71 match and Unicorn testing. Functions with `z3_proven: true` in leaf_cache.json can skip the permuter entirely.
