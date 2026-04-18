Export a delinked reference object from the live Ghidra session and structurally
diff it against our compiled candidate for a target function. Use this after a
lift when you want structural evidence that the C faithfully reproduces the
original's memory accesses — specifically to catch field-rotation / offset-swap
bugs that build clean but violate original invariants.

Prerequisites (skill should verify and bail early if not met):

- A Ghidra GUI session is open on `cachebeta.xbe` (or `default.xbe`) with the
  `Halo Delinker Control` plugin enabled. The RPC listens on
  `127.0.0.1:18080`. Probe it with `mcp__ghidra-live__get_current_program` —
  if this fails, stop and tell the user to start Ghidra + enable the plugin.
- The project has been built at least once (`cmake --build build`), so
  `build/CMakeFiles/halo.dir/src/halo/<area>/<file>.c.obj` exists for the
  target's source file.

Argument: $ARGUMENTS (function name or `0x...` address; required)

Steps the agent must perform:

1. **Resolve the target.**
   - If $ARGUMENTS is an address, grep `kb.json` for the matching `"addr":`
     entry to get the name and source file mapping.
   - If $ARGUMENTS is a name, grep for the `"decl"` line and pick up the
     address from the adjacent `"addr":`.
   - Call `mcp__ghidra__get_function_by_address(addr)` — the result contains
     `Body: <start> - <end>`. Use those as the exact range.
   - Format the range string as `"0x<start>-0x<end>"` (hyphen separator; no
     spaces; lowercase hex).

2. **Ensure output directory.**
   - `mkdir -p /mnt/g/dev/halo/artifacts/delinker/`
   - The Ghidra process runs on Windows, so the export path passed to the
     MCP tool must be a Windows-format path, e.g.
     `G:\\dev\\halo\\artifacts\\delinker\\<name>.o`. Pre-create the dir from
     WSL first; Ghidra won't make intermediate dirs.

3. **Synthesize relocations.**
   - `mcp__ghidra-live__run_relocation_synthesizer(selection_mode="range", range=<range>)`

4. **Export the delinked reference.**
   - `mcp__ghidra-live__export_delinked_object(`
     - `export_path="G:\\dev\\halo\\artifacts\\delinker\\<name>.o",`
     - `selection_mode="range",`
     - `range=<range>,`
     - `run_relocation_synthesizer=false`    *(step 3 already ran it)*
     - `)`
   - Verify the file exists with `ls -la /mnt/g/dev/halo/artifacts/delinker/<name>.o`
     and is a non-empty COFF object (`file <path>` should say "COFF object").

5. **Locate the candidate object.**
   - From the `source` field in `kb.json`, the candidate object is
     `build/CMakeFiles/halo.dir/src/halo/<source>.obj`.
   - Ensure it's current by running `cmake --build build` (idempotent, fast
     when nothing changed). Fail loudly if the build fails.

6. **Run the structural diff.**
   - `python3 tools/lift_pipeline.py --target <name> --no-metadata-update --objdiff-reference artifacts/delinker/<name>.o --objdiff-candidate build/CMakeFiles/halo.dir/src/halo/<source>.obj`
   - If the `objdiff` binary isn't installed, `objdiff_lift.py` will error out —
     report that to the user rather than trying to proceed.

7. **Report.**
   - Reference `.o` path, candidate `.obj` path.
   - Whether objdiff ran and produced output.
   - Summary of structural diffs (basic block count, branch structure,
     memory-access mismatches). Call out any **systematic `[reg+N]`
     store-offset mismatches across adjacent writes with the same source
     registers** — that pattern is the field-rotation signature the bug class
     this skill exists to catch.
   - Artifacts path (under `artifacts/lift_runs/<timestamp>/`).

Notes:

- Do **not** save the Ghidra project after running this — the relocation
  synthesizer can edit the DB, and a pristine DB is easier to re-diff from
  later. This is an analysis-only session.
- Expect noise from clang vs MSVC codegen differences (register allocation,
  instruction scheduling). Field-rotation bugs stand out even through the
  noise because the offsets themselves differ, not just the encoding.
- For very short functions (leaves under ~20 bytes) the diff is usually
  pristine and the signal-to-noise is highest. For 500+ line functions the
  output is noisy enough that the xbox-halo-re-analyst's store-offset table
  is a better first-line check.
- See `docs/ghidra-live-delinker-workflow.md` for the full background and
  the harm analysis.
