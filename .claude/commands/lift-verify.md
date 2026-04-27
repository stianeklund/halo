---
description: Run lift verification on the deep agent
agent: deep
subtask: true
---

Use the `halo-verify-debug` skill for structural verification rules and report
shape.

Run verification for a lifted function. The primary path is XDK MSVC 7.1
structural comparison via `tools/xdk_verify.py`. Fallback to
`tools/lift_pipeline.py` structural verify when no delinked reference exists.

Argument: $ARGUMENTS (`<target> [extra flags]`)

Behavior:
1. Parse the first token as `<target>` (function name or `0x...` address).
2. Resolve the source file from kb.json.
3. **XDK verify (primary):** Check if a delinked reference exists in `delinked/`
   (via `objdiff.json`). If so, run:
   ```
   python3 tools/xdk_verify.py <source_file> --function <target> --show-diffs
   ```
   Report match percentage. Above 85% = correct. Below 70% = investigate.
4. **Lift pipeline verify (fallback):** If no delinked reference exists, run
   `tools/lift_pipeline.py` with `--verify-auto` and any extra flags from
   $ARGUMENTS for decompile-based structural verification.
5. Treat any remaining tokens as extra flags forwarded to the chosen tool.

Notes:
- If extraction wrote defaults into `{artifact_dir}` (`orig_decompile.txt`,
  `new_decompile.txt`), the lift pipeline fallback needs no extra flags.
- The XDK path is strongly preferred — it catches real offset/argument bugs
  that decompile-diff misses, and gives a concrete match percentage.
- If no delinked reference exists, offer to run `/delink` to export one.
