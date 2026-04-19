---
description: Export a delinked reference object and diff it against the build
agent: deep
subtask: true
---

Use the `halo-verify-debug` skill for delink prerequisites, export guidance,
and mismatch triage rules.

Export a delinked reference object from the live Ghidra session and
structurally diff it against our compiled candidate for a target function. Use
this after a lift when you want structural evidence that the C faithfully
reproduces the original's memory accesses.

Prerequisites (verify and bail early if not met):

- A Ghidra GUI session is open on `cachebeta.xbe` (or `default.xbe`) with the
  delinker plugin enabled. If the live Ghidra bridge is unavailable, stop and
  tell the user to start Ghidra and enable the plugin.
- The project has been built at least once (`cmake --build build`), so the
  target object's `.obj` exists for the lifted source file.

Argument: $ARGUMENTS (function name or `0x...` address; required)

Steps:
1. Resolve the target.
   - If $ARGUMENTS is an address, find the matching `addr` in `kb.json`.
   - If $ARGUMENTS is a name, find the adjacent address and source mapping.
   - Resolve the function body range from Ghidra before exporting.
2. Ensure the output directory exists under `artifacts/delinker/`.
3. Synthesize relocations for the selected range.
4. Export the delinked reference object and verify the file exists and is a
   non-empty COFF object.
5. Locate the candidate object from the `source` field in `kb.json`.
6. Run the structural diff between the reference object and the candidate.
7. Report the reference path, candidate path, whether objdiff ran, and any
   structural mismatches that matter for field offsets or branch shape.

Notes:
- Do not save the Ghidra project after this run.
- Expect some codegen noise; focus on systematic offset or control-flow drift.
