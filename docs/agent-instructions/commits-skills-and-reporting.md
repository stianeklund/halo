# Commits, Skills, and Reporting

Load this module for commits, auto-lift, skill routing, subagent delegation, or
final reports.

## Lift Commit Discipline

- Never write freeform lift commit messages (`/lift` owns ABI audit, build,
  verification, and the message).
- `/auto-lift` commits successful pipeline results through
  `generate_lift_commit.py` and reverts/logs failures under
  `artifacts/auto_lift/failures/`. Do not commit legacy review/promote artifacts
  directly.
- Lift subjects must be one of:
  `Port <func> (<obj>) (<score>)`,
  `Port <func1>, <func2> (<obj>) (<score>)`, or
  `Port <N> functions from <obj> (<score>)`.
- Use one deduplicated `Functions ported:` section with address, object, and
  score per function; consolidate `Callee decls corrected:` and
  `Functions renamed:` sections; report only net coverage change as
  `Coverage: <start>% -> <end>% (<count>/<total> symbols)`.
- Remove squash artifacts, duplicate section headers, and concatenated subjects.
  `tools/audit/consolidate_squash_msg.py --in-place <msg_file>` can normalize a
  squash message.

After staging, generate the message through a unique temporary file:

```bash
MSG=$(mktemp /tmp/halo-commit-msg.XXXXXX)
rtk python3 tools/audit/generate_lift_commit.py --batch-name "<short description>" > "$MSG"
rtk git commit -F "$MSG" && rtk rm -f "$MSG"
```

Never use a fixed shared path such as `/tmp/commit_msg.txt`.
`generate_lift_commit.py` and the pre-commit hook run register-ABI and baseline
guards. `--skip-abi-audit` and `--no-verify` are emergency bypasses only.

## Readability and Fix Commits

- Subjects: `Types: …`, `Names: …`, `Readability: …`, `Fix …`. One logical bulk
  per category per TU or cluster, not one commit per wave or per function.
- Each commit passes the gates in skill `readability-ladder` §9 on its own.
- Plain description of what changed. No attribution or co-author lines. No
  mention of external reference projects (AGENTS.md rule 13).
- Stash generated noise (`tools/equivalence/leaf_cache.json`,
  `tools/xbox/aim_ab/`) around commits; never sweep another session's WIP.
- Never push without explicit approval.

## Skill Routing

Skills are doctrine agents invoke themselves, not a menu users must choose.
`tools/memory/skill_router_hook.py` maps prompt triggers from skill frontmatter.
Treat routed skills as required. Before lift, score recovery, call-site, hazard,
crash, or regression work, load the matching skill even if the router omitted
it. Name all relevant skills in subagent briefs.

Full catalogue: `.claude/skills/SKILLS.md`. To add or retier a skill, edit its
frontmatter (`tier` and `triggers`), then run the migration and index generators
documented there. Command/script selection belongs to
`.claude/skills/tool-reference/SKILL.md`.

Shared agent command and skill maintenance follows `docs/agent-content.md`:
`.claude/` is canonical, `.opencode/` is its runtime copy, and
`.agents/skills/` is generated from OpenCode content.

## Reporting

For lift and RE reports, use these fields when applicable:

- Target
- Confirmed
- Inferred
- Uncertain
- Proposed Code
- `kb.json` updates

Keep reports concise and distinguish binary/runtime evidence from inference.
