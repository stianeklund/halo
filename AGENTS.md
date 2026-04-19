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

## Architecture

### Skill: halo-xbox-re (doctrine)

The single source of truth for RE methodology. Carries:

- Ground rules (binary-first, unknown over wrong)
- Evidence policy (Confirmed / Inferred / Uncertain labels)
- Review checklist (the step-by-step RE process)
- Output format contract
- Pointers to `docs/references/` for deep detail on ABI, prototypes,
  kb policy, memory, work selection, and output schema

### Agent: xbox-halo-re-analyst (bounded worker)

A thin, cheap Codex agent. Each invocation handles exactly one target or
hypothesis and produces one output artifact. It delegates all methodology to
the `halo-xbox-re` skill and does not embed doctrine itself.

### Other skills (operational workflows)

- `halo-re-lift` — lift-specific workflow, references `halo-xbox-re` for
  doctrine and `docs/references/` for deep ABI and prototype rules
- `halo-verify-debug` — verification lanes, delink workflow, regression
  debugging; references `halo-xbox-re` for evidence labeling
- `halo-build-xemu` — build, ISO, and xemu control procedures
- `halo-xbdm` — XBDM/RDCP command handling for real Xbox debugging