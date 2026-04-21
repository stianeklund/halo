# Agent Instructions

These rules apply to any coding agent used in this repo (Claude, OpenCode,
subagents). Keep `AGENTS.md` and `CLAUDE.md` aligned.

## Mission

Recover Halo CE Xbox behavior faithfully and incrementally.

- Binary is source of truth.
- Prefer explicit unknowns over plausible guesses.
- Make small, reviewable changes.
- Preserve ABI, layout, side effects, and control-flow shape.

## Scope-first requests

When possible, start tasks with:

- Exact file path(s)
- Function/symbol name(s)
- Line range(s)
- Expected validation command(s)

Prompt example:

`In src/halo/main/main.c:120-210, analyze foo_bar(). Validate with: cmake --build build`

## Context and token discipline (required)

Goal: minimize redundant reads without losing accuracy.

1. Start with the narrowest scope that can answer the task.
2. Prefer line-ranged reads and symbol lookups over full-file reads.
3. Do not re-read the same file/range in the same task unless one is true:
   - file changed since last read
   - prior slice was too small to resolve ambiguity
   - a new task phase requires different nearby context
4. Before expanding scope, state exactly what is missing:
   - `NEED <path>:<line-range> because <type/symbol/call-site>`
5. For repeat-heavy files (for example `kb.json`, `main.c`), read only the
   required section unless a full-file pass is explicitly justified.
6. MUST prefer `jq` over inline Python for querying JSON files (especially
   `kb.json`) to minimize token usage and boilerplate.
7. Prefer one target/hypothesis per pass for RE-heavy work.

## Research before edit

Before editing:

1. Read target function/file context.
2. Identify callers/callees and touched globals/types.
3. Make a minimal patch plan scoped to the request.

If an edit fails, re-read only affected ranges before retrying.

## Repo safety rules

- Do not invent behavior, names, or types without binary evidence.
- Keep unresolved semantics visible (`unknown`, `field_XX`, `TODO`).
- Reuse existing types and declarations before creating new ones.
- Do not reorder/repad structs without matching `cs`/`co` checks.
- Do not hand-edit generated files in `build/generated/`.
- Keep behavior changes separate from cleanup/formatting whenever possible.

## kb.json discipline

Treat `kb.json` as link/runtime-critical.

- Verify prototypes from disassembly evidence.
- Keep updates conservative and auditable.
- Prefer hardcoded addresses over speculative global entries when uncertain.
- Use `jq` for all inspections and filtering.
- Build and verify after each meaningful `kb.json` change.
- Protected `@<reg>` ABI entries are pinned by `tools/kb_reg_baseline.json`:
  - Any mismatch against the baseline is a hard build failure.
  - Do not remove or move `@<reg>` slots in routine lift work.
  - Baseline edits are explicit policy changes and must be justified separately.

## Build and verification

- Use the repo toolchain and existing scripts.
- Run the narrowest meaningful validation first, then broaden if needed.
- Prefer real Xbox XBDM verification over xemu when available.

## MCP servers

Expected local Ghidra MCP SSE endpoints:

- `ghidra` at `http://127.0.0.1:8090/sse`
- `ghidra-live` at `http://127.0.0.1:8091/sse`

Start them with:

- `bash tools/mcp-servers.sh`

Ghidra MCP failure policy (required):

- Before first use of any `ghidra` or `ghidra-live` MCP tool in a task, run
  `python3 tools/check_ghidra_mcp.py`.
- If that preflight fails, or if any `ghidra`/`ghidra-live` MCP tool call fails
  due to connection/timeout/unavailable errors, bail out immediately (no
  retries in the same response).
- Tell the user exactly: `You might have forgotten to start
  tools/mcp-servers.sh or ghidra may not be running?`

Useful commands:

- `python3 tools/check_ghidra_mcp.py`
- `python3 tools/frontier.py --limit 5`
- `python3 tools/maintain.py <source_file>`
- `python3 tools/lift_pipeline.py --target <name_or_addr> --no-metadata-update --verify-policy auto`
- `jq '.symbols[] | select(.name | contains("foo"))' kb.json`

## Avoid noisy directories

Do not read/search these unless explicitly requested:

- `build/`
- `build_debug/`
- `node_modules/`
- `.git/`
- `dist/`
- `__pycache__/`
- `halo-patched/`

## Output discipline

- Prefer focused output over broad logs.
- Use narrow paths and targeted commands.
- For non-trivial RE/lift work, report:
  - Target
  - Confirmed
  - Inferred
  - Uncertain
  - Proposed code
  - kb.json updates
  - Validation
  - Next steps

## Architecture and skills

- `halo-xbox-re`: RE doctrine and evidence rules (source of truth)
- `halo-re-lift`: lift workflow and ABI-specific execution
- `halo-verify-debug`: verification lanes and regression debugging
- `halo-build-xemu`: build/ISO/xemu workflow
- `halo-xbdm`: RDCP/XBDM workflow for real Xbox
- `xbox-halo-re-analyst`: bounded one-target worker following doctrine

Deep references live in `docs/references/`.
