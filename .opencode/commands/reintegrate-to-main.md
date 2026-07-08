---
description: Safely reintegrate a lift/session worktree branch into main
---

Use `reintegrate-to-main`.

Request: $ARGUMENTS

Follow the safe branch integration workflow: inspect branch/worktree state, bring the branch up to date, run the required kb.json partition/build/no-drop gates, and fast-forward main only when evidence supports it. Do not use destructive git commands.
