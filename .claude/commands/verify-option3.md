---
description: Run the Option 3 verification ladder for a lift target
agent: fast
subtask: true
---

Use the `halo-verify-debug` skill for the Option 3 ladder and reporting rules.

Run the practical Option 3 verification ladder. The primary structural
verification is XDK MSVC 7.1 compilation via `tools/xdk_verify.py` — it
compiles with the same compiler that built the original XBE.

Argument: $ARGUMENTS (`<target> [extra flags]`)

Steps:
1. Parse the first token as `<target>` (function name or `0x...` address).
2. Resolve the source file and check if a delinked reference exists in
   `delinked/` (lookup via `objdiff.json`).
3. **XDK verify (primary):** If a delinked reference exists, run:
   ```
   python3 tools/xdk_verify.py <source_file> --function <target> --show-diffs
   ```
   Report the match percentage. Above 85% = normal. Below 70% = investigate.
4. **Build + ISO (optional):** Run `tools/verify_option3.py` with remaining
   tokens as extra flags for build/ISO/xemu verification.
5. If no delinked reference exists, offer to run `/delink` to export one.

Notes:
- Add `--load-into-xemu` to hot-load the ISO and reset the VM.
- Use `--skip-build` / `--skip-iso` for quick re-runs.
- XDK verify is the meaningful structural comparison. The clang-vs-delinked
  objdiff path gives ~13% match due to compiler differences and is not useful.
