---
name: opencode-command-auto-lift
description: "Use when /auto-lift is invoked. Runs guarded OpenCode function-lift automation."
---

# OpenCode Command: /auto-lift

Run the repository-native guarded driver instead of reproducing an automation
loop in prompt prose:

```bash
rtk python3 tools/auto_lift_opencode.py $ARGUMENTS
```

The driver selects candidates, uses one OpenCode agent per target, requires the
`goal90` pipeline gate before a commit, and stops if protected paths become
dirty. Relay its summary. Do not independently lift another target.

Arguments: `--goal N`, `--stop-on-fail N`, `--objects obj1,obj2`, `--criteria TEXT`,
`--dry-run`, `--allow-main`, `--agent NAME`, `--model PROVIDER/MODEL`.
