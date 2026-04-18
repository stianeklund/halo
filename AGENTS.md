# Agent Instructions

These rules apply to any coding agent used in this repo.

## Scope-first requests

- Include exact files/functions and line ranges when known.
- Provide expected validation commands up front.

## Research before edit

Before editing:

1. Read target file/function context.
2. Find callers/callees and touched globals/types.
3. Make a minimal, scoped patch plan.

If an edit attempt fails, re-read affected files before retrying.

## Avoid noisy directories

Do not read/search these paths unless explicitly requested:

- `build/`
- `build_debug/`
- `node_modules/`
- `.git/`
- `dist/`
- `__pycache__/`
- `halo-patched/`

## Output discipline

- Prefer focused command output over broad logs.
- Use narrow paths and targeted commands.
