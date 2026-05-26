# Agent Content Maintenance

This repo uses `.claude/` as the source of truth for shared agent commands and
skills. `.opencode/` is kept in sync from it manually.

## Policy

- **Edit in `.claude/`** — it is the canonical working copy.
- **Copy to `.opencode/`** after editing shared commands/skills.
- Platform-specific commands or skills stay only in the target tree.
- Put durable shared doctrine in `docs/`, not in agent command trees.

## Sync workflow

```bash
# Copy all shared commands from claude to opencode
cp .claude/commands/*.md .opencode/commands/

# Copy shared skills
cp -r .claude/skills/<skill>/ .opencode/skills/
```

## Verification

```bash
diff -rq .claude/commands/ .opencode/commands/ | grep -v '~'
```
