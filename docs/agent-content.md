# Agent Content Maintenance

This repo intentionally keeps `.claude/` and `.opencode/` as separate agent
surfaces.

They should stay separate because:
- each tool expects different command discovery and agent wiring
- frontmatter and command naming can diverge by platform
- repo-local operational guidance still needs to stay aligned

The current state is:
- there is heavy overlap between the two trees
- much of the overlap is deliberate and healthy
- the main risk is silent drift in copied command or skill bodies
- there is at least one naming alias today: `.claude/commands/load-iso.md`
  and `.opencode/commands/load-xemu-with-iso.md`

## Maintenance policy

- Keep `.claude/` and `.opencode/` files separate.
- Do not make one tree import the other.
- Put durable shared doctrine in normal repo docs such as `docs/` or in repo
  tooling under `tools/`.
- Keep agent-specific wrappers thin when possible.
- Prefer matching filenames across both trees unless there is a concrete
  platform-specific naming reason.

## Audit tool

Use:

```bash
python3 tools/audit_agent_content.py
```

Use strict mode in CI or before cleanup commits:

```bash
python3 tools/audit_agent_content.py --strict
```

The audit distinguishes between:
- exact matches
- wrapper-only differences where the body matches but frontmatter or wrapper
  text differs
- real content mismatches or missing files

## Recommended next cleanup steps

1. Normalize command filenames where aliases are no longer needed.
2. Move long shared operational doctrine into `docs/` when it starts to drift.
3. Keep skills as the main place for repeated workflow instructions; keep
   commands short and delegating.
