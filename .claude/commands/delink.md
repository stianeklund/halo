---
description: Export a delinked reference object and diff it against the build
agent: deep
subtask: true
---

Use the `halo-verify-debug` skill for delink prerequisites, export guidance,
and mismatch triage rules.

Export a delinked reference object from the live Ghidra session and verify it
against our lifted code using the XDK MSVC 7.1 compiler (`tools/verify/xdk_verify.py`).
This is the primary structural verification path — it compiles our C with the
same compiler that built the original XBE, giving 90%+ match rates for correct
lifts vs ~13% with the clang cross-compiler.

Prerequisites (verify and bail early if not met):

- A Ghidra GUI session is open on `cachebeta.xbe` (or `default.xbe`) with the
  delinker plugin enabled. If the live Ghidra bridge is unavailable, stop and
  tell the user to start Ghidra and enable the plugin.
- The project has been built at least once (`cmake --build build`), so the
  target object's `.obj` exists for the lifted source file.

Ghidra MCP preflight (required):
- Before any `ghidra`/`ghidra-live` MCP tool call, run
  `python3 tools/audit/check_ghidra_mcp.py`.
- If the preflight fails, or any `ghidra`/`ghidra-live` MCP tool call fails due
  to connection/timeout/unavailable errors, stop immediately and tell the user
  exactly: `You might have forgotten to start tools/mcp-servers.sh or ghidra
  may not be running?`

Argument: $ARGUMENTS (function name, `0x...` address, or object name; required)

Steps:
1. Resolve the target.
   - If $ARGUMENTS is an address, find the matching `addr` in `kb.json`.
   - If $ARGUMENTS is a name, find the adjacent address and source mapping.
   - If $ARGUMENTS is an object name (e.g. `game_sound.obj`), export the full
     object range.
   - Resolve the function/object body range from Ghidra before exporting.
2. Ensure the delinked output directory exists (`delinked/`).
3. Export the delinked reference object via `ghidra-live` MCP
   `export_delinked_object` with `run_relocation_synthesizer: true`.
4. Verify the exported file exists and is a non-empty COFF object (`file` cmd).
5. Update `objdiff.json` if no entry exists for this object yet.
6. Run `python3 tools/verify/xdk_verify.py <source_file> --function <target_name>
   --show-diffs` to compile with XDK MSVC 7.1 and structurally compare.
   - If XDK compilation fails, fix the issue (usually missing macros in
     `xdk_common.h` or implicit pointer casts that `/TP` C++ mode rejects).
   - Report the match percentage and any diffs.
7. Report: reference path, match %, whether any structural mismatches matter
   for field offsets or branch shape.

Notes:
- Do not save the Ghidra project after this run.
- Match rates above 85% are normal for correct lifts (the remaining diff is
  label placement, NOPs, and minor scheduling differences).
- Match rates below 70% indicate a real structural problem — investigate.
