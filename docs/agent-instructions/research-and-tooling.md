# Research and Tooling Rules

Detail behind AGENTS.md always-on rules 1-8. Do not restate those here.

## Read Discipline

- Files over 300 lines require ranged reads of at most 100 lines.
  `objects.c`, `units.c`, `sound_manager.c` are always ranged.
- Need an already-read fact? Recall it or `rtk rg` the exact string.
- Treat warnings from `tools/audit/token_discipline_hook.py` as authoritative.

## Pre-Edit Research Gate

Target at least four reads or evidence checks per edit. Before editing a
function:

1. `rtk rg '<function_name>' src/` for callers and related symbols.
2. `rtk read <file> -o <start> -l <limit>` for the exact target lines.
3. `rtk jq '<filter>' kb.json` for signature and ABI evidence.
4. `rtk git diff -- src/ kb.json CMakeLists.txt` for current scoped changes.

Do more research if these do not establish behavior and blast radius.
Treat `prior_fixes.py` matches as hypotheses until binary or runtime evidence
confirms them.

## Search Tools

Prefer CodeGraph when the index covers the task. Then `rtk fd` for files,
`rtk rg` for text, `rtk ast-grep` for structure. If `rtk rg` returns empty or
rejects flags, fall back to bare `grep -rn`; do not retry flag variants.

## Ghidra Discipline

Never dump full decompilation or disassembly speculatively. Query callees
first, inspect the smallest useful target, use `read_memory` on exact ranges,
and never redecompile a function that already has ported C. After Ghidra
evidence informs edits, keep first-read line ranges and use later reads of at
most 40 lines.

## Diffs

Scope diffs to relevant source: `rtk git diff -- src/ kb.json CMakeLists.txt`.
Bare diffs include tracked generated artifacts.
