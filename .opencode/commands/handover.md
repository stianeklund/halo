---
description: Produce a concise continuation handover for a new agent
subtask: false
---

Use the `handover` skill.

Produce a concise handover document for the current task or goal. Summarize
what we were working on, important changes, current state, validation status,
risks or uncertainties, and what remains. If the work is complete, say that and
summarize the final state instead of inventing continuation steps.

Argument: $ARGUMENTS (optional focus area)

Do not continue implementation work. Do not perform broad repo exploration.
Only use lightweight checks needed to avoid misleading the next agent.
