---
description: >-
  Universal debugging entry point — crash, hang, assert, visual regression,
  build failure, deploy failure, or any runtime problem. Routes to the correct
  specialized skill.
---

Use the `debug` skill for symptom routing and inline debugging procedures.

Argument: $ARGUMENTS (describe the symptom — crash output, assert text, visual
regression, build error, or behavior description)

First run prior-fix lookup:

```bash
rtk python3 tools/memory/prior_fixes.py "$ARGUMENTS"
```

Then start with the symptom router (Section A) to identify the correct debugging
path, and load the recommended specialized skill(s). Treat lookup matches as
hypotheses only until confirmed against binary/disassembly/runtime evidence.
