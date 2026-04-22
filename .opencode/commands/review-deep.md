---
description: Run a deep code review on the deep agent
agent: deep
subtask: true
---

Perform a code review with a primary focus on bugs, regressions, risky assumptions, missing validation, and testing gaps.

Argument: $ARGUMENTS (optional file paths, symbols, or review scope)

Behavior:
1. If arguments are provided, focus the review on that scope.
2. If no arguments are provided, review the current uncommitted changes in the repository.
3. Findings must come first and be ordered by severity.
4. Include file and line references where possible.
5. Keep summaries brief and only after enumerating findings.
6. If no findings are discovered, say so explicitly and mention residual risks or testing gaps.
7. Do not re-read the same file/range unless scope changed or the first slice was insufficient.
8. Before reading outside the initial scope, report `NEED <path>:<line-range> because <reason>.`
