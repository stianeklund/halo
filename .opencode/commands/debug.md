---
description: Universal debugging entry point for crash, hang, assert, visual regression, build failure, deploy failure, or wrong behavior
agent: deep
---

Use the `debug` skill for symptom routing and inline debugging procedures.

Argument: $ARGUMENTS (describe the symptom: crash output, assert text, visual
regression, build error, or behavior description)

First run prior-fix lookup:

```bash
rtk python3 tools/memory/prior_fixes.py "$ARGUMENTS"
```

Then load the recommended skill(s) from the lookup output and follow the debug
skill's symptom router. Treat matches as hypotheses only; confirm against
binary/disassembly/runtime evidence before fixing code.
