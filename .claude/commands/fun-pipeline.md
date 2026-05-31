---
description: Classify and name FUN_ functions; run before /frontier when <common> backlog is large
subtask: false
---

Manage the FUN_ function naming pipeline. Reduces the unnamed `FUN_` backlog and improves `frontier.py` accuracy by moving `<common>` functions into proper objects and generating Ghidra-based name proposals.

Usage: `/fun-pipeline [status|reclassify|prioritize|propose|apply] [options]`

## Default (no argument or "status")

Run `rtk python3 tools/analysis/fun_pipeline.py status` and report:
- Total / ported / named / FUN_ counts
- Priority tier breakdown (P0/P2/P3)
- Suggested next action from the tool's output

## Subcommands

**`reclassify [--apply]`**
Run `rtk python3 tools/analysis/fun_pipeline.py reclassify`.
- Without `--apply`: dry-run — show proposed moves, do not write.
- With `--apply`: write moves to kb.json; report how many functions moved and top target objects.
- Always run `status` after `--apply` to show updated counts.

**`prioritize [--priority P0|P2|P3]`**
Run `rtk python3 tools/analysis/fun_pipeline.py prioritize [--priority ...]`.
- Show tier table and top objects per tier.
- P0 = attributed + has signature; P2 = attributed but void(void); P3 = in `<common>`.

**`propose [--limit N] [--object OBJ] [--priority P0|P2]`**
Run `rtk python3 tools/analysis/fun_pipeline.py propose $ARGUMENTS`.
- Outputs a JSON work queue of FUN_ functions with Ghidra actions for each.
- Summarize the number of proposals and the top objects represented.
- Remind the user to run Ghidra decompile/callers/strings for each entry before naming.

**`apply --input FILE [--dry-run]`**
Run `rtk python3 tools/analysis/fun_pipeline.py apply --input $FILE [--dry-run]`.
- Apply name proposals from a JSON file back to kb.json.
- With `--dry-run`: show what would change without writing.
- After applying, run `rtk python3 tools/audit/extract_reg_args.py --check` to catch any ABI drift.

## Recommended workflow

If `$ARGUMENTS` is empty or "status", just run status.
Otherwise, pass `$ARGUMENTS` directly to `fun_pipeline.py` and report the result.

The typical progression is:
1. `status` — gauge naming debt
2. `reclassify` (dry-run first, then `--apply`) — move `<common>` FUN_ to named objects
3. `prioritize --priority P0` — see what has real signatures
4. `propose --priority P0 --limit 50` — generate Ghidra work queue
5. (human does Ghidra work, saves proposals JSON)
6. `apply --input proposals.json --dry-run` then without `--dry-run`
