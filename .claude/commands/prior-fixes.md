---
description: Search prior fixes, agent memory, commits, and retrieval outcomes before debugging
---

Run a local prior-fix lookup for the symptom or target, then use the returned
matches as hypotheses to confirm or reject.

Argument: $ARGUMENTS (symptom text, target function/address, or subsystem)

Required command:

```bash
rtk python3 tools/memory/prior_fixes.py "$ARGUMENTS"
```

Treat every match as a lead, not proof. Confirm against binary evidence,
disassembly, commit diffs, or runtime oracle before changing code.
