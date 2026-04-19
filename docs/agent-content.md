# Agent Content Maintenance

This repo intentionally keeps `.claude/` and `.opencode/` as separate agent
surfaces, but the shared source of truth now lives in `agent-content/`.

They should stay separate because:
- each tool expects different command discovery and agent wiring
- frontmatter and command naming can diverge by platform
- repo-local operational guidance still needs to stay aligned

The current state is:
- the output trees stay separate for tool compatibility
- shared command and skill content is maintained once under `agent-content/`
- `tools/sync_agent_content.py` writes the generated outputs to both trees
- there is one intentional filename alias today:
  `.claude/commands/load-iso.md` and
  `.opencode/commands/load-xemu-with-iso.md`

## Maintenance policy

- Keep `.claude/` and `.opencode/` outputs separate.
- Edit shared command and skill text in `agent-content/`, not in generated
  outputs.
- Regenerate outputs with `tools/sync_agent_content.py` after shared edits.
- Put durable shared doctrine in normal repo docs such as `docs/` or in repo
  tooling under `tools/`.
- Prefer matching filenames across both trees unless there is a concrete
  platform-specific naming reason.

## Workflow

Use:

```bash
python3 tools/sync_agent_content.py
```

This copies shared sources from `agent-content/` into the separate generated
output trees:
- `.claude/`
- `.opencode/`

To verify outputs are current without rewriting files:

```bash
python3 tools/sync_agent_content.py --check
```

To audit the current overlap or spot unexpected drift between the generated
trees themselves:

```bash
python3 tools/audit_agent_content.py --strict
```

## Editing rules

- Treat `agent-content/` as canonical for shared commands and skills.
- Treat `.claude/` and `.opencode/` as generated outputs for shared entries.
- If a command or skill is intentionally platform-specific, keep it only in the
  target tree and document why.
- If a generated file needs a filename alias, keep the alias mapping in
  `tools/sync_agent_content.py`.

## Recommended next cleanup steps

1. Normalize command filenames where aliases are no longer needed.
2. Move long shared operational doctrine into `docs/` when it starts to drift.
3. Keep skills as the main place for repeated workflow instructions; keep
   commands short and delegating.
