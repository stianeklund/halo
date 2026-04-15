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
