# How Lift Verification Works (Beginner Guide)

This document explains, in plain language, how we decide whether a lifted
function is "close enough" to the original Xbox behavior.

If you are new to this repo, this is the short version:

- We do **not** trust one signal.
- We do **not** rely on raw byte equality alone.
- We combine structural similarity, ABI/build safety, and behavior checks.


## Why this is harder than a byte compare

Even when two functions do the same thing, compiled machine code bytes can differ
because of instruction scheduling, register allocation, or encoding choices.

So "different bytes" does not always mean "wrong behavior".

On the other hand, high similarity alone can still hide bugs (especially x87/FPU
operand order issues).


## The main verification command

Most of the time we run:

```bash
rtk python3 tools/lift_pipeline.py --target <FUNC> --no-metadata-update --verify-policy auto
```

This orchestrates the checks and writes a run summary to:

- `artifacts/lift_runs/<run-id>/summary.json`


## Signal 1: Structural match ("shape" of the function)

Primary tools:

- `tools/verify/vc71_verify.py`
- `tools/verify/compare_obj.py`

What happens:

1. `vc71_verify.py` compiles the lifted source with **VC71** (old MSVC, close to
   the original compiler family).
2. `compare_obj.py` disassembles both objects (lifted and delinked reference).
3. It extracts instruction mnemonic sequences (`mov`, `call`, `fld`, `fsubp`, ...).
4. It computes similarity using Python `SequenceMatcher(...).ratio()` and reports
   a match percentage.

In plain terms: this is a **sequence/structure similarity score**, not a strict
byte-equality score.


## Signal 2: FPU risk warnings

`compare_obj.py` also does extra x87/FPU checks and emits `FPU-WARN` style
warnings when operand order looks suspicious.

This matters because two code paths can look similar while still flipping math
sign/direction (for example with subtraction or cross-product order).


## Signal 3: Optional objdiff structural match

Tool:

- `tools/verify/objdiff_lift.py`

This is another structural signal. It extracts instruction mnemonics from objdiff
JSON output and computes a sequence similarity percentage.

In `lift_pipeline.py`, this can be used as an additional structural source.


## Signal 4: Behavioral equivalence (state compare)

Tool:

- `tools/equivalence/unicorn_diff.py`

This runs oracle and lifted code in emulation with the same seeds and compares
CPU/FPU/scratch outcomes at return.

- Default: scratch is byte-exact.
- FPU-heavy paths can use ULP tolerance for float scratch slots.

This is a behavior-oriented signal, not just shape.


## How signals are combined in the pipeline

`lift_pipeline.py` does **rule-based gating**, not one big weighted score.

High-level logic:

1. Build and ABI audit must pass first.
2. Structural match is taken from the best available structural source
   (`vc71` and/or `objdiff`).
3. Low-match policy applies thresholds:
   - below reject floor: fail
   - mid-low band: require stronger behavior evidence
   - near-threshold band: require at least one behavior signal
4. FPU warnings can fail the run unless a stronger behavior proof lane passes.

Default threshold values currently come from `lift_pipeline.py`:

- low-match threshold: `50.0`
- stricter behavior-required band: below `40.0`
- hard reject floor: below `25.0`


## Quick mental model

Think of verification as four questions:

1. **Can it build safely?** (build + ABI)
2. **Does it look structurally similar?** (sequence match)
3. **Any math red flags?** (`FPU-WARN`)
4. **Does behavior agree under tests?** (equivalence/runtime/behavior checks)

A lift is accepted when the relevant gates say "yes" together.


## Common misconception

"We compare bytes." Not exactly.

What we primarily compare for structural scoring is instruction sequence shape
(mnemonic order), then we add behavior and risk checks on top.


## Batch verification

Tool:

- `tools/equivalence/batch_verify.py`

Single-function verification (above) checks one lift at a time. Batch
verification runs the Unicorn differential test across **all** ported functions
that have both a delinked oracle `.obj` and a built candidate `.obj`.

For each function it:

1. Loads the **oracle** (original MSVC-compiled `.obj` extracted from the Xbox
   binary via the Ghidra delinker) and the **candidate** (our clang-compiled
   `.obj` from lifted C source).
2. Runs both in separate Unicorn x86 emulators with identical random inputs
   (default: 50 seeds per function).
3. Compares CPU state at function return — EAX, EDX, ST0, and output buffer
   contents.
4. Reports **pass** (identical output), **fail** (divergence — our lift has a
   bug), **error** (emulation crashed, e.g. unmapped memory), or **N/A**
   (function can't be tested).

```bash
# Full batch run with FPU tolerance and CSV output:
rtk python3 tools/equivalence/batch_verify.py --seeds 50 --float-tolerance 32 --csv

# Quick smoke test (first 20 functions, fewer seeds):
rtk python3 tools/equivalence/batch_verify.py --limit 20 --seeds 10

# List candidates without running:
rtk python3 tools/equivalence/batch_verify.py --dry-run
```

Results go to `artifacts/batch_verify/`:

- `summary.json` — aggregate pass/fail/error counts, failure list, Z3 proofs
- `<function>.json` — per-function detailed result
- `results.csv` — tabular output (with `--csv` flag)


### Global data seeding

The emulator needs to read the same global data as the real Xbox binary.
`tools/equivalence/extract_globals.py` scans all delinked `.obj` files for
`DIR32` relocations (absolute address references), reads the corresponding bytes
from the XBE, and writes them to `tools/equivalence/known_globals.json`.
`unicorn_diff.py` loads this file at startup to seed the emulator's memory.

```bash
# Regenerate after new delinked exports:
rtk python3 tools/equivalence/extract_globals.py --json
```


### FPU tolerance

x87 floating-point rounding can differ between MSVC and clang even when the
logic is identical. `--float-tolerance N` allows up to N ULP (Unit in the Last
Place) difference for:

- Float pointer output buffers (scratch slots)
- ST0 return values (80-bit x87 extended precision)

Typical values: 16 (tight), 32 (moderate), 256 (long FPU chains).


### Leaf cache and function classification

`unicorn_diff.py --batch-classify` scans all delinked `.obj` files and
classifies each function as:

- **leaf** — no external calls or data references (pure computation)
- **data_only** — references global data but makes no external calls
- **stubbable** — calls known stubs (csmemcpy, fabs, etc.)
- **non_leaf** — calls unknown functions (can't emulate yet)

Results are cached in `tools/equivalence/leaf_cache.json`. `batch_verify.py`
uses this cache to select testable candidates.


## Where to read next

- `docs/lift_pipeline.md` - pipeline stages and flags
- `docs/verification_policy.md` - acceptance policy for low match
- `docs/z3-equivalence.md` - Z3 formal equivalence proofs
- `tools/verify/compare_obj.py` - sequence matcher and FPU warning implementation
- `tools/equivalence/unicorn_diff.py` - behavioral state comparison
- `tools/equivalence/batch_verify.py` - batch differential testing
