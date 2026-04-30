# lift_pipeline automation lane

This document defines the input/output contract for `tools/lift_pipeline.py` and
how to plug in extraction tooling so decompile -> reimplementation -> verify can
run with one command.

## Goals

- Keep the workflow conservative (assist-first, fail-fast, explicit artifacts).
- Make every stage machine-readable and resumable from files.
- Gate metadata updates on successful build/verification.

## Pipeline stages

`lift_pipeline.py` runs these stages in order:

1. `target_pick`
2. `ghidra_extract` (optional via `--extract-cmd`)
3. `source_update` (optional via `--candidate`)
4. `build` (optional via `--skip-build`)
5. `verify_policy` (always runs; computes risk and verify decision)
6. `verify_payload` (optional via `--verify-auto` or policy-driven auto-verify)
7. `verify_lift` (optional via `--verify-input` or generated payload)
8. `objdiff` (optional via `--objdiff-reference` + `--objdiff-candidate`)
9. `runtime_check` (optional via `--with-runtime`)
10. `metadata_update` (unless `--no-metadata-update`)

Per-run output goes to:

`artifacts/lift_runs/<run-id>/summary.json`

and stage-specific logs in the same directory.

## Extraction command contract

`--extract-cmd` is a shell command template executed by:

`bash -lc "<rendered command>"`

Supported template placeholders:

- `{target_addr}`
- `{target_name}`
- `{target_object}`
- `{artifact_dir}`

Expected behavior for extraction tools:

1. Exit with code `0` on success, non-zero on failure.
2. Write extraction outputs under `{artifact_dir}`.
3. At minimum, produce decompile text files that can be used for verification:
   - `{artifact_dir}/orig_decompile.txt`
   - `{artifact_dir}/new_decompile.txt`
4. Optionally produce callee lists:
   - `{artifact_dir}/orig_callees.txt`
   - `{artifact_dir}/new_callees.txt`

If these default files are not used, pass explicit paths/commands to
`--orig-decompile-*`, `--new-decompile-*`, `--orig-callees-*`, and
`--new-callees-*`.

## verify payload contract

`tools/verify/verify_lift.py` expects a JSON payload with:

- `name`
- `orig_address`
- `new_address`
- `orig_decompile`
- `new_decompile`
- `orig_callees` (optional)
- `new_callees` (optional)

`tools/verify/collect_verify_payload.py` creates this payload and accepts inputs via:

- `--*-text` (inline)
- `--*-file`
- `--*-cmd` (shell command output)

Only one input mode per field is allowed.

## Verification policy

`lift_pipeline.py` now supports policy-driven structural verification decisions:

- `--verify-policy manual`
  - Never auto-enables verify stages.
  - Only runs `verify_lift` when `--verify-input` or `--verify-auto` is passed.
- `--verify-policy auto` (default)
  - Computes a risk score from the target function body and declaration metadata.
  - If risk is high, it auto-enables `--verify-auto` behavior when inputs exist.
  - If inputs are missing, it records the reason and continues.
- `--verify-policy strict`
  - Same risk detection as `auto`.
  - Fails the run when a risky function cannot be verified due to missing inputs.

Tune the score threshold with `--verify-risk-threshold` (default: `3`).

Policy-driven auto-verify still needs the same payload prerequisites as
`--verify-auto` (notably `--verify-new-address` plus decompile inputs from
files or commands).

`verify_policy` stage details include:

- computed score/threshold
- key metrics (branches, calls, pointer writes, offset access, register args)
- trigger list and final decision

## Metadata gating

Without `--no-metadata-update`:

- after successful build: set status to `ported`
- after successful verification (structural/runtime enabled and passing): set
  status to `verified`

This keeps `kb_meta.json` updates aligned with what was actually validated.

## Common command patterns

Target from frontier, no edits/build:

```bash
python3 tools/lift_pipeline.py --skip-build --no-metadata-update
```

Insert candidate and build:

```bash
python3 tools/lift_pipeline.py \
  --target 0x17c7e0 \
  --candidate /tmp/rasterizer_frame_begin.c \
  --source src/halo/rasterizer/rasterizer.c
```

Auto-generate verify payload from files emitted by extraction:

```bash
python3 tools/lift_pipeline.py \
  --target 0x3f670 \
  --extract-cmd "python3 tools/my_extract.py --addr {target_addr} --out {artifact_dir}" \
  --verify-new-address 0x600000
```

Enforce verification on risky functions (fail if inputs are missing):

```bash
python3 tools/lift_pipeline.py \
  --target ai_initialize \
  --extract-cmd "python3 tools/my_extract.py --addr {target_addr} --out {artifact_dir}" \
  --verify-policy strict \
  --verify-new-address 0x600000
```

Auto-generate verify payload from commands:

```bash
python3 tools/lift_pipeline.py \
  --target ai_initialize \
  --verify-auto \
  --verify-new-address 0x600000 \
  --orig-decompile-cmd "python3 tools/dump_decompile.py --program cachebeta.xbe --addr {target_addr}" \
  --new-decompile-cmd "python3 tools/dump_decompile.py --program default.xbe --addr 0x600000" \
  --orig-callees-cmd "python3 tools/dump_callees.py --program cachebeta.xbe --addr {target_addr}" \
  --new-callees-cmd "python3 tools/dump_callees.py --program default.xbe --addr 0x600000" \
  --skip-build --no-metadata-update
```

## Recommended next integration

Implement a small extractor wrapper (for your chosen Ghidra lane) that always
writes the default files into `{artifact_dir}`:

- `orig_decompile.txt`
- `new_decompile.txt`
- `orig_callees.txt`
- `new_callees.txt`

That makes `--verify-auto` mostly zero-config for day-to-day use.
