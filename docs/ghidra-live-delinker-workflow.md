# Live Ghidra Delinker Workflow

Use the live Ghidra MCP integration as an optional validation lane, not the main
decompilation path.

## Purpose

The live-session delinker workflow exports small relocatable reference objects
from the current Ghidra GUI session so they can be compared against the
project's compiled candidate objects.

Use it to validate structural closeness:

- control flow shape
- relocation recovery
- symbol coverage
- section boundaries
- object-level drift visible in `objdiff`

## Recommended usage

1. Analyze and implement a small function or helper cluster as usual.
2. In the live Ghidra GUI, select one function or a tight address range.
3. Use the MCP server to:
   - `run_relocation_synthesizer`
   - `export_delinked_object`
4. Build the project normally.
5. Compare the delinked reference object with the compiled candidate object
   using the existing objdiff tooling.
6. Use the diff results to guide small fidelity fixes.

## Best fit

This is most useful for:

- frontier helper clusters
- nearly complete `.obj` groups
- difficult functions where structural validation is helpful
- targeted verification before landing a reimplementation

## Guidelines

- Prefer small coherent ranges over large speculative exports.
- Treat delinker output and `objdiff` as validation signals, not as stronger
  evidence than the binary or runtime behavior.
- Keep the normal source/build workflow unchanged.
- Do not require this for every small change; use it when closeness matters.

## Suggested placement in the project workflow

Use the live delinker lane after initial lifting and before or during objdiff
validation.

```text
binary analysis -> implementation -> live delinker export -> objdiff -> fidelity fixes
```

## Targeted use: catching field-rotation / offset-swap bugs

The live delinker is well-suited to the specific bug class where our C has
the right *memory accesses* but writes them to the *wrong struct fields* —
the kind of mistake that passes build and looks correct in review but
silently violates an original invariant. Commit `8ea4afa` (ctl2 rotation
in `players_update_before_game`) is the canonical example; it wasn't
visible in the decompiler, passed `-Wall -Werror`, and only surfaced as
a page fault inside `stack_walk` several frames downstream.

Procedure for a suspect function (**fully automated, no GUI interaction**):

1. Resolve the function's body range from Ghidra:
   `mcp__ghidra__get_function_by_address(addr)` returns `Body: <start> - <end>`.
   Or take `addr` from `kb.json` and the end from the next address in the
   sorted listing. Format the range as `"0x<start>-0x<end>"` (hyphen
   separator).
2. `mcp__ghidra-live__run_relocation_synthesizer(selection_mode="range", range=<range>)` —
   synthesize relocations for the range.
3. `mcp__ghidra-live__export_delinked_object(export_path=<out.o>, selection_mode="range", range=<range>, run_relocation_synthesizer=false)` —
   produces a COFF `.o`. Pass `run_relocation_synthesizer=false` since
   step 2 already ran it.
4. Build normally (`cmake --build build`).
5. Run `tools/objdiff_lift.py` with the delinked object as `--reference`
   and our corresponding compiled object as `--candidate`.
6. Scan the diff for systematic `[reg+N]` store-offset mismatches across
   adjacent writes with the same source registers — the signature of a
   rotated assignment.

### Path gotcha

Ghidra runs on Windows even when the repo lives under WSL. `export_path`
is opened on the Ghidra process side, so it must be a **Windows-format
path**, e.g. `G:\dev\halo\artifacts\delinker\foo.o`, not
`/mnt/g/dev/halo/artifacts/delinker/foo.o`. Pre-create the directory
from WSL side (`mkdir -p /mnt/g/dev/halo/artifacts/delinker/`) since
Ghidra won't create intermediate directories for you.

### Tested example

Verified end-to-end against `FUN_000930a0` (`cinematic_in_progress`,
9 bytes):

```
run_relocation_synthesizer(
  selection_mode="range",
  range="0x930a0-0x930a8"
)
export_delinked_object(
  export_path="G:\\dev\\halo\\artifacts\\delinker\\cinematic_in_progress.o",
  selection_mode="range",
  range="0x930a0-0x930a8",
  run_relocation_synthesizer=false
)
```

Produced a valid COFF object with one `dir32` relocation against
`DAT_0044df00`, exactly what `objdump -r -t` of a clean extract should
show.

The only human prerequisite is that a Ghidra GUI session with the
`Halo Delinker Control` plugin enabled (RPC on `127.0.0.1:18080`) is
running. Once that's up, the whole loop above is agent-driveable.

### Harm analysis

| Risk                        | Severity | Notes                                                                                                                        |
| --------------------------- | -------- | ---------------------------------------------------------------------------------------------------------------------------- |
| Ghidra database mutation    | Low      | `run_relocation_synthesizer` edits the relocation table and may touch symbol attrs. Persists only if the project is saved.   |
| GUI thread lag              | None     | Analysis runs on Ghidra's internal threads. No data risk.                                                                    |
| File-lock conflict          | None     | This is the headless-path problem. Live MCP runs inside the GUI process; no lock.                                            |
| `cachebeta.xbe` corruption  | None     | The binary is read-only to the tool. Only Ghidra's analysis DB of it changes.                                                |
| Runtime side effects        | None     | Pure analysis/export. Doesn't touch xemu, the patched XBE, or the build tree.                                                |
| Environment fragility       | Low      | Requires the Ghidra plugin installed + enabled and the MCP SDK present. If misconfigured, calls fail cleanly; no damage.     |
| Incidental rework           | Low      | Re-running after the analyzer has already synthesized is idempotent but wastes a few seconds.                                |

### Caveats

- Clang vs MSVC scheduling produces structural noise in objdiff even when
  the lift is correct. Look for *systematic* `[reg+N]` mismatches across
  several adjacent stores, not a single unexplained diff.
- For long functions (hundreds of asm lines), per-store review is slow.
  Prefer the xbox-halo-re-analyst agent's store-offset table as the
  first-line check and run objdiff only when the table looks suspicious
  or the function is ABI-critical.
- Don't save the Ghidra project after a delinker run unless you intend to
  keep the synthesized relocations in the DB permanently.
