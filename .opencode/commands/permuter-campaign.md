---
description: Run or plan a VC71 permuter campaign for 85-98% low-match lifted functions
agent: Xbox-Halo-Lift-Reviewer
---

Load the `permuter-campaign` skill and follow it exactly. If the Skill tool does not expose it, read `.opencode/skills/permuter-campaign/SKILL.md` first.

Argument: $ARGUMENTS (optional shortlist, object, source file, or campaign limit)

Use this command when the user says `permuter`, `batch permute`, `low match`, `VC71 85-98%`, or `push stuck functions toward 100%`.

Before running workers, confirm the target is eligible: already ported, VC71 score in `[85, 98]%`, delinked reference exists, no known structural ceiling, source parses for the permuter, and the build is clean.

Report the candidate list, skipped targets with reasons, worker results, gate results, and any committed/reverted improvements.
