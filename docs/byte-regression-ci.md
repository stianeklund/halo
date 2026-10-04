# Byte regression CI

`vc71-regression.yml` now runs **Raw-XBE Byte Regression** instead of the
mnemonic floor gate. VC71 remains the comparison compiler. The production
clang build, ABI checks and runtime tests remain separate checks.

Each invocation freshly compiles the PR base and GitHub's PR merge revision in
separate checkouts. Both use the candidate's byte evaluator and the same VC71
compiler and RXDK headers. The original debug 2276 XBE must have MD5
`c7869590a1c64ad034e49a5ee0c02465`. No historical score is accepted as a floor.

The metric is `raw_xbe_structural.py`'s **aligned byte lower bound**. Instruction
alignment locates corresponding bytes; mnemonic agreement does not earn byte
credit. Relocation targets must resolve to the original address or literal
contents. Uncertain relocation bytes count against the lower bound. This is
relocation-aware byte accuracy, not literal equality of unlinked COFF and XBE
files, and it does not prove runtime equivalence or validate inferred bounds.

For each function address, neither the matching byte count nor its fraction of
compared bytes may fall. Ratios are compared using integer arithmetic, without
rounding or a tolerance. Increased relocation uncertainty, target mismatches,
unpaired relocations, alignment ambiguity, and loss of literal identity also
fail. Improvements to other functions cannot offset a regression. Renames keep
their comparison by address. Removing or deactivating a measured function fails.

For C-only edits, both revisions compile all ported functions in the affected
translation units. Headers, KB declarations (including their callers), build
settings and measurement tooling changes trigger a full sweep. New ports must
produce a valid byte measurement. Unchanged pre-existing non-comparable results
are listed as coverage gaps in `result.json`; they are not measured passes.
Compilation failures, empty overall coverage and lost measurements fail.

Evidence is valid only for the current invocation: snapshots bind to a unique
run token, exact revision, scope, source/header/KB/discovery input hashes,
generated declarations, evaluator code, compiler binaries, RXDK headers,
Capstone version and pristine XBE hash. Inputs are checked before and after
measurement, and checked again before comparison. Every function record must
agree with its snapshot's source, declaration, bound-table and XBE hashes.
Changes to a function's reference span or compiler options invalidate its
comparison instead of silently replacing its floor. An old timestamp is not
used to decide validity; an old snapshot cannot satisfy the current run token.

The job summary reports compared functions, new functions and existing gaps.
Artifacts contain the exact snapshots, input hashes, byte differences and
compiled objects, including available evidence when the gate fails.

A manual run compares the selected revision to `HEAD^` by default. The
`base_ref` input can select another baseline revision. Local use requires two
clean checkouts, the staged pristine XBE, VC71/RXDK and Python with Capstone:

```sh
python3 tools/verify/byte_regression.py check \
  --base-root /path/to/base-checkout --base-commit BASE_SHA \
  --head-commit HEAD_SHA --output artifacts/byte_regression/local
```

Use a new output directory for every run. The gate copies its evaluator files
and the reference XBE into the separate base checkout; use a disposable checkout.
The workflow file path is retained so its existing badge URL stays valid. If
branch protection requires the old mnemonic job name, update that required
check to **Raw-XBE byte regression** when this workflow lands.

The pre-commit and pre-push hooks use the same gate through `byte_regression.py
local`. Pre-commit captures the index exactly, preserving unstaged changes.
Pre-push captures the pushed commit. Source recovery uses `local --working-tree`
with a private temporary index. These paths create disposable detached worktrees
and unreachable snapshot commit objects; they never move a branch or change the
user's index. There is no mnemonic floor update or auto-staging phase anymore.

`vc71_regression.py check` is a compatibility entry point for raw byte gating.
Its old analysis is explicitly `mnemonic-check`. `score_improve.py` and
`score_structize.py` accept candidates by byte improvements, and reject old
mnemonic baselines. Re-record those baselines before continuing score work.

Dashboard refresh aliases `--vc71` and `--full-vc71` now select byte measurement.
Optional mnemonic analysis is explicitly `--mnemonic` or `--full-mnemonic`.
The dashboard's score endpoint refreshes bytes, and the evidence map defaults
to byte accuracy. The Progress Report action also publishes a fresh byte
snapshot instead of refreshing mnemonic scores. Newly published byte records
validate all input hashes and compiler assets before the dashboard displays
them. Older exploratory records retain their limited source/XBE/bound validation
and are never comparison inputs for the regression gate.

Local byte commands default to one worker and recycle workers between translation units to contain compiler and disassembly caches. CI selects its worker count explicitly.

Each local run leaves a `artifacts/byte_regression/local-XXXXXXXX` output directory (about 230 MB); the gate keeps the newest three and deletes older ones that are more than an hour old. `local-reusable/` (persistent checkouts) and `cache/` are never removed by this pruning.
