---
name: halo-verify-debug
description: Verification ladder, delink comparison, and regression debugging workflow
---

# Halo Verify And Debug

Use this skill whenever work involves lift verification, Option 3 validation,
delinked object comparison, or regression investigation.

## Verification lanes

### Structural verification via lift pipeline

Run:

`python3 tools/lift_pipeline.py --target <target> --verify-auto --verify-new-address <new_address> --no-metadata-update <extra_flags>`

Report:

- verify payload path
- `verify_lift` stage result
- summary path under `artifacts/lift_runs/.../summary.json`

### Option 3 ladder

Run:

`python3 tools/verify_option3.py --target <target> <extra_flags>`

Report:

- stage results for `build`, `build_iso`, `objdiff`, `xemu_load_reset`, and `assert_tripwire`
- PASS or FAIL verdict
- summary path under `artifacts/verify_option3/.../summary.json`

Notes:

- Add `--objdiff-reference <path>` and `--objdiff-candidate <path>` when a
  delinked reference object exists.
- Add `--load-into-xemu` to hot-load and reset via `tools/xemu_qmp.py`.
- Use `--skip-build` or `--skip-iso` for quick reruns when artifacts already exist.

## Delink workflow

Before running delink comparison, verify:

- a live Ghidra GUI session is open on `cachebeta.xbe` or `default.xbe`
- the delinker plugin is enabled
- the project has been built so the candidate `.obj` exists

Then:

1. Resolve the target and function body range.
2. Export the delinked reference object under `artifacts/delinker/`.
3. Compare it against the built candidate object.
4. Focus on structural mismatches that affect field offsets, branch shape, and
   memory access behavior.

Do not save the Ghidra project after a delink export run.

## Regression debugging workflow

1. Start with git history before live probing.
2. Inspect recent commits touching `kb.json`, `src/`, types, or prototypes.
3. Check likely regression classes:
   - wrong calling convention or arg count
   - wrong return type or operand width
   - struct size, packing, or field offset drift
   - wrong `HDATA` indirection level
   - missing side effects or empty stubs
   - `@<reg>` thunk without implementation
4. Cross-check suspects in Ghidra disassembly.
5. Use xemu probing only when static evidence is insufficient.

Useful xemu probes:

- screenshot for visible state
- serial output for assertions
- `hmp "info registers"`
- `hmp "x /Nx 0x<addr>"`

## Debugging guardrails

- Fix only what the evidence supports.
- Prefer narrow changes over refactors.
- Do not repack or reorder structs without binary proof.
- If the hypothesis is too weak to fix safely, stop and say so.

## Output expectations

Report:

- symptom
- commits investigated
- root cause with Confirmed, Inferred, and Uncertain labels
- exact fix made
- validation performed
- remaining risk or follow-up
