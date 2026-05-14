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
5. `option3 <target> [extra verify_option3 flags]` — legacy runtime/xemu fallback.
6. `failure <artifact_dir>` — classify a failed lift pipeline or auto-lift artifact and recommend the next narrow action.

If no mode is supplied, treat the first token as `<target>` and run `normal`.

Commands:
```bash
rtk python3 tools/lift_pipeline.py --target <target> --no-metadata-update --verify-policy auto <extra_flags>
rtk python3 tools/lift_pipeline.py --target <target> --verify-auto --verify-new-address <new_address> --no-metadata-update <extra_flags>
rtk python3 tools/audit/check_lift_hazards.py
rtk python3 tools/audit/batch_delink.py --object <object>
rtk python3 tools/verify/verify_option3.py --target <target> <extra_flags>
```

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
