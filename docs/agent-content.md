# Agent Content Maintenance

This repo uses `.claude/` as the source of truth for shared agent commands and
skills. `.opencode/` is the OpenCode runtime copy. `.agents/skills/` is the
workspace Antigravity/Gemini runtime copy generated from `.opencode/`.

## Policy

- **Edit in `.claude/`** — it is the canonical working copy.
- **Copy to `.opencode/`** after editing shared commands/skills.
- **Generate `.agents/skills/`** after `.opencode/` changes. Antigravity/Gemini
  only discover skills, so OpenCode slash commands are exposed as
  `opencode-command-<name>` wrapper skills and OpenCode agents are exposed as
  `opencode-agent-<name>` persona skills.
- Platform-specific commands or skills stay only in the target tree.
- Put durable shared doctrine in `docs/`, not in agent command trees.

## Sync workflow

```bash
# Copy all shared commands from claude to opencode
cp .claude/commands/*.md .opencode/commands/

# Copy shared skills
cp -r .claude/skills/<skill>/ .opencode/skills/

# Generate workspace Antigravity/Gemini skills from OpenCode content
rtk python3 tools/memory/sync_agent_content.py

# Optional: install the same generated skills globally/shared for Gemini users
rtk python3 tools/memory/sync_agent_content.py --target global
rtk python3 tools/memory/sync_agent_content.py --target shared
```

## Verification

```bash
diff -rq .claude/commands/ .opencode/commands/ | grep -v '~'
rtk python3 tools/memory/sync_agent_content.py
```
