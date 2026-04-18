---
name: maintain.py invocation
description: How to correctly invoke maintain.py to avoid emptying source files
type: feedback
---

Always invoke maintain.py with a RELATIVE path (from the project root) and with
both the project root and tools/ on PYTHONPATH:

```
PYTHONPATH=/mnt/g/dev/halo:/mnt/g/dev/halo/tools python3 -m tools.maintain src/halo/game/players.c
```

**Why:** When you pass an absolute path (e.g. `/mnt/g/dev/halo/src/halo/game/players.c`),
maintain.py treats it as distinct from the relative path `src/halo/game/players.c` that
kb.json uses. It "moves" functions from the absolute-path version to the relative-path
version — which, from the project root, is the same file — causing the file to be emptied.

**How to apply:** Always use a relative path argument (matching the `source` field in
kb.json). Never pass an absolute path to maintain.py.
