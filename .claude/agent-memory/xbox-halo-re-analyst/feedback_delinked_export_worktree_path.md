---
name: delinked-export-worktree-path
description: ghidra-live export_delinked_object writes to the MAIN repo delinked/, not the active worktree; copy the obj into the worktree before VC71 verify
metadata:
  type: feedback
---

`mcp__ghidra-live__export_delinked_object` takes an absolute Windows path
(e.g. `G:\dev\halo\delinked\foo.obj`) and writes to the **main repo**
`delinked/` directory. When working inside a git worktree under
`.claude/worktrees/<name>/`, the worktree has its own real (non-symlink)
`delinked/` directory, and `vc71_verify.py` resolves
`ref=delinked/<obj>` relative to the worktree root.

**Why:** The export lands in main-repo `delinked/` but VC71 looks in the
worktree's `delinked/`, so `--list` shows `[MISSING]` and verify fails with
"No usable objdiff.json unit found" even though the export succeeded.

**How to apply:** After exporting a delinked reference while in a worktree,
copy it into the worktree before running VC71:
`cp /mnt/g/dev/halo/delinked/<obj>.obj <worktree>/delinked/<obj>.obj`
Then `objdump -t <worktree>/delinked/<obj>.obj | grep <FUN_addr>` to confirm
the target symbol is present. Related: [[delink-before-verify]].
