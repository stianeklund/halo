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

### Which delinked reference gets used (and why it matters)

A TU can have many references on disk: a whole-object export, narrow range
exports (`<stem>_<hex>_<hex>.obj`), and per-function chunks
(`delinked/functions/<hex8>.obj`). The score is only meaningful if the chosen
one actually bounds the function, and two failure modes are invisible in the
output:

- **Too narrow.** A function with no reference is reported as `DROP`, which
  produces no score line at all — so the loss looks like the file simply having
  fewer functions. Measured 2026-07-29: `objects.c` was scored against a
  2-function `objects_FUN_00084a10.obj` while a 754-function `objects.obj` sat
  in `delinked/` unregistered, and `actor_moving.c` against 1 of 33. Fixed by
  registering the whole-object references in `objdiff.json` and ranking
  candidates by symbol count.
- **Too wide.** A reference slice runs to the next symbol in *its own* object,
  so it absorbs whatever follows: alignment filler (`bipeds.obj`'s
  `FUN_001a0680` slice is 152 instructions where the function is 91) or the
  neighbouring function (`player_queues_new.obj`'s `FUN_000b97b0` is 154 where
  the function is 68). Both score the lift against code it never claimed to
  implement — 61.5% instead of 83.2% in the first case.

Ranking is by symbol count but only among **same-TU** names, because neither
half of that rule is safe alone: preferring the exact stem loses coverage
(`units.obj` has 18 symbols, `units_new.obj` 88), and preferring the widest
picks a *different TU* (`files_windows.obj`, 368 symbols, is a registered
candidate for `files.c`, whose own reference has 17). "Same TU" admits only
address-range and `FUN_<addr>` suffixes — never a bare word, since sibling TUs
share prefixes.

Where a function is available from several references, the **pristine XBE
decides**: whichever slice length is closest to the real instruction count
wins. Nothing else can arbitrate — `kb.json`'s span is the distance to the next
*listed* function and overshoots wherever the listing has a gap (`FUN_000b97b0`
spans 480 bytes for a 196-byte function), and each reference's own bounds are
exactly what is in question.

Not yet done: padding that appears on **both** sides currently contributes free
LCS matches, so every score is slightly inflated. Trimming it symmetrically is
the more complete fix but recalibrates the whole committed baseline (measured:
`files.c` `file_open` 85.2% → 80.6% with its reference unchanged), so it needs a
deliberate repopulate rather than a drive-by change. `_trim_trailing_padding`
exists and is used only to *detect* over-run when choosing between references.

Self-tests: `tools/verify/test_ref_selection.py`.


## Signal 2: FPU risk warnings

`compare_obj.py` also does extra x87/FPU checks and emits `FPU-WARN` style
warnings when operand order looks suspicious. Two sibling presence-census
detectors run in the same lane: `LOADW-WARN` (a field the original narrows to
int16/int8 but our lift reads wider, or vice versa — lift-learnings §24) and
`IMM-WARN` (a large inline constant — float bit-pattern or magic — present on
exactly one side; since both objects are VC71 codegen this is a wrong numeric
literal the LCS % aligns away — lift-learnings §25). Run `--fpu-only`,
`--loadw-only`, or `--imm-only` for focused output.

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
3. **Any math red flags?** (`FPU-WARN`, `LOADW-WARN`, `IMM-WARN`)
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
# Full batch run (sensible defaults: 32 ULP tolerance, auto ESP, CSV):
rtk python3 tools/equivalence/batch_verify.py --seeds 50 --csv

# Quick smoke test (first 20 functions, fewer seeds):
rtk python3 tools/equivalence/batch_verify.py --limit 20 --seeds 10

# List candidates without running:
rtk python3 tools/equivalence/batch_verify.py --dry-run
```

Defaults apply automatically: `--float-tolerance 32` for FPU rounding, ESP delta
checked for leaf functions only (non-leaf stack frames legitimately differ between
MSVC and clang).

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

Typical values: 16 (tight), 32 (moderate, batch default), 256 (long FPU chains).


### Known divergence categories

Not all "fail" results are lift bugs. The batch run produces several expected
failure categories:

- **Pointer returns** — functions returning pointers to globals produce
  different addresses in the emulator vs the XBE. Not fixable without a full
  memory image. (~7 functions)
- **Upper-EAX artifacts** — MSVC leaves stale bits in upper EAX for `int16_t`
  returns. Lower bits match. The `ret_bits` field in kb.json ABI can mask this.
- **Intra-object call chains** — functions that call other functions within the
  same `.obj` now execute deeper (full `.text` section is loaded), but callees
  may themselves hit unmapped memory or missing stubs.
- **ESP delta** — automatically skipped for non-leaf functions. MSVC and clang
  allocate different stack frame sizes for functions with local variables.


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
