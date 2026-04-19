---
description: Run maintain.py and report the resulting changes
agent: fast
subtask: true
---

Run the maintain script and report any resulting worktree changes.

Steps:
1. If `$ARGUMENTS` is provided, pass it through to `python3 tools/maintain.py`.
2. Run `python3 tools/maintain.py $ARGUMENTS`.
3. Check whether the repository gained new modifications after the run.
4. Report the maintain result and the affected files concisely.
