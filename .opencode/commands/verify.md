---
description: Run lift verification, delink export, hazard scans, and runtime fallback lanes
agent: deep
subtask: true
---

Use `halo-verify-debug` for verification gates, delink triage, and failure
classification.

Consolidated verification command. Prefer this over separate structural,
delink, hazard, or Option 3 commands.

Argument: $ARGUMENTS (`[mode] <target> [extra flags]`)

Modes:
1. `normal <target> [extra lift_pipeline flags]` — default post-lift validation.
2. `structural <target> <new_address> [extra lift_pipeline flags]` — explicit patched-XBE address verification.
3. `hazards` — run the lift hazard scanner.
4. `delink <target>` — export/reference-map a delinked object and run structural comparison.
5. `equivalence <target> [--seeds N] [--allow-stubs] [--float-tolerance N]` — Unicorn-Engine differential test. Runs MSVC oracle and clang candidate with seeded inputs; compares CPU/FPU at RET. Use `--allow-stubs` for non-leaf functions (external calls are stubbed). Use `--float-tolerance N` for FPU-heavy functions (compares float* scratch slots with N ULP tolerance instead of byte-exact).
6. `permute <target> [--time 60]` — last-mile match optimizer for the 85–98% VC71 band. Wraps `tools/permuter/run.py`. Reports best score found; never apply a permutation that lowers the existing match.
7. `option3 <target> [extra verify_option3 flags]` — legacy runtime/xemu fallback.
8. `failure <artifact_dir>` — classify a failed lift pipeline or auto-lift artifact and recommend the next narrow action.

If no mode is supplied, treat the first token as `<target>` and run `normal`.

Commands:
```bash
rtk python3 tools/equivalence/unicorn_diff.py <target> --seeds 100
rtk python3 tools/equivalence/unicorn_diff.py <target> --seeds 100 --allow-stubs --float-tolerance 32
```

Equivalence mode:
- **When to use:** byte-match alone is weak evidence (FPU-heavy math,
  serializers, hashes, small string helpers); VC71 match is structurally
  capped (e.g. SEH wrappers stuck at ~55%); or you need behavioral proof
  beyond structural comparison.
- **Leaf functions:** run bare `unicorn_diff.py <target> --seeds 100`. No
  stubs or globals needed.
- **Non-leaf functions:** add `--allow-stubs` to enable callee stubbing
  (csmemcpy, fabs, _chkstk, etc.) and DIR32 data relocation patching.
  Globals from `_KNOWN_GLOBAL_BYTES` are automatically seeded into the
  patched slots.
- **FPU-heavy functions:** add `--float-tolerance N` to compare float*
  scratch buffers with N ULP tolerance instead of byte-exact.  x87
  rounding differences between MSVC and clang accumulate across loop
  iterations.  Recommended values: 16 (tight), 32 (moderate), 256 (loose
  for long FPU chains).  Auto-detects float* params from the declaration.
- **Typical invocation for a geometry/physics function:**
  `unicorn_diff.py <target> --allow-stubs --float-tolerance 32 --seeds 100`
- **Side effect:** every run records the leaf classification to
  `tools/equivalence/leaf_cache.json`, which boosts that address in
  `llm_auto_lift.py select` (`+5 eq_pure_leaf`).
1. Resolve `<target>` and check if it's leaf or non-leaf (the tool prints
   the classification: leaf / data_only / stubbable / non_leaf).
2. Run with at least 100 seeds. Pass = 0 divergences across all seeds;
   even one divergence is a real bug unless it's a float tolerance issue.
3. Re-run with `--seed <hex>` from the failing report to reproduce.
4. If failures are only in float scratch at small ULP, increase
   `--float-tolerance` or accept the divergence as FPU precision noise.

Permute mode:
- **When to use:** VC71 match is in **[85, 98]%** AND a delinked reference
  exists. The lift is structurally correct but instruction order /
  scheduling differs from MSVC — random AST permutations can close the
  gap.
- **When to skip:** match < 85% (structural bug — fix the lift first;
  permuter cannot recover from real correctness issues), match > 98%
  (diminishing returns, not worth 60s+), or no delinked reference (no
  byte-target to optimize against).
- **Hard rule:** never accept a permutation that lowers the existing VC71
  match. Always re-run the lift pipeline against the new source before
  trusting the new score.
1. Confirm the current VC71 match is in [85, 98] and a delinked reference is
   mapped via `objdiff.json`.
2. Run with a 60s time budget by default. The driver extracts the target,
   sets up a permuter work dir, and reports the best score.
3. Apply a winning permutation ONLY after re-running the lift pipeline
   against the new source — confirm the match improved and no regression
   slipped in.

Delink mode:
1. Resolve `<target>` to object/source in `kb.json` using `rtk jq`.
2. Run Ghidra MCP preflight before any ghidra-live export.
3. Prefer `batch_delink.py --object <object>` for object exports.
4. Verify `objdiff.json` maps the reference and candidate object before trusting XDK/objdiff results.

Failure mode:
1. Inspect the artifact summary and classify the first failing stage.
2. For build failures, fix only the compile error.
3. For ABI failures, inspect `kb.json`, `@<reg>` annotations, and caller setup.
4. For XDK/FPU failures, inspect x87 operand order and push-then-fstp callsites.
5. For low-match failures, inspect branch shape, memory offsets, and missing side effects.
6. For behavior/runtime failures, prefer XBDM state probes before xemu unless no console is reachable.

Report:
- mode and target
- command(s) run
- artifact path or summary path
- pass/fail stage classification
- confidence: high, medium, or low
- next narrow action if anything failed
