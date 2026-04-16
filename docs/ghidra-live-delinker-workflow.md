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

Procedure for a suspect function:

1. In the Ghidra GUI, select the function's address range.
2. Call `mcp__ghidra-live__run_relocation_synthesizer` on the range.
3. Call `mcp__ghidra-live__export_delinked_object` — produces a COFF `.o`.
4. Build normally (`cmake --build build`).
5. Run `tools/objdiff_lift.py` with the delinked object as
   `--reference` and our corresponding compiled object as `--candidate`.
6. Scan the diff for systematic `[reg+N]` store-offset mismatches across
   adjacent writes with the same source registers — the signature of a
   rotated assignment.

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
