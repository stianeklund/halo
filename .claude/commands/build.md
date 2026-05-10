---
description: Build the project, inspect for warnings or errors
agent: fast
subtask: true
---

Build the project: python3 tools/build/build.py -q
Argument: $ARGUMENTS (unused)

Steps:
1. Build the project using python3 tools/build/build.py -q
2. Report to orchestrator: build status, commit hash, and any warnings
