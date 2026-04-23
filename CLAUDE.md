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
6. MUST prefer `jq` over inline Python for querying or parsing JSON files
   anywhere in this repo; do not use `python -c` for JSON parsing when `jq`
   can do the job.
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
- Do not use inline assembly in lifted C code. The build system generates
  forward thunks (C → original XBE) and reverse thunks (original XBE → C)
  automatically via kb.json `@<reg>` entries. If you think you need inline
  asm, you are missing a kb.json entry.
- Keep behavior changes separate from cleanup/formatting whenever possible.

## kb.json discipline

Treat `kb.json` as link/runtime-critical.

- Verify prototypes from disassembly evidence.
- Keep updates conservative and auditable.
- Prefer hardcoded addresses over speculative global entries when uncertain.
- Use `jq` for all inspections and filtering.
- Build and verify after each meaningful `kb.json` change.
- `@<reg>` annotations are **immutable** — they describe the original XBE ABI,
  not the C implementation. See `docs/references/abi-and-calling-conventions.md`.
  - Never remove or change `@<reg>` slot assignments. Renaming and retyping are fine.
  - `tools/kb_reg_baseline.json` enforces this: any mismatch is a hard build failure.
  - When you encounter a register-arg callee, add it to kb.json with `@<reg>` and
    call it by name. Do not use raw function pointer casts or inline assembly.
  - New `@<reg>` entries must also be added to the baseline.

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

<!-- rtk-instructions v2 -->
# RTK (Rust Token Killer) - Token-Optimized Commands

## Golden Rule

**Always prefix commands with `rtk`**. If RTK has a dedicated filter, it uses it. If not, it passes through unchanged. This means RTK is always safe to use.

**Important**: Even in command chains with `&&`, use `rtk`:
```bash
# ❌ Wrong
git add . && git commit -m "msg" && git push

# ✅ Correct
rtk git add . && rtk git commit -m "msg" && rtk git push
```

## RTK Commands by Workflow

### Build & Compile (80-90% savings)
```bash
rtk cargo build         # Cargo build output
rtk cargo check         # Cargo check output
rtk cargo clippy        # Clippy warnings grouped by file (80%)
rtk tsc                 # TypeScript errors grouped by file/code (83%)
rtk lint                # ESLint/Biome violations grouped (84%)
rtk prettier --check    # Files needing format only (70%)
rtk next build          # Next.js build with route metrics (87%)
```

### Test (60-99% savings)
```bash
rtk cargo test          # Cargo test failures only (90%)
rtk go test             # Go test failures only (90%)
rtk jest                # Jest failures only (99.5%)
rtk vitest              # Vitest failures only (99.5%)
rtk playwright test     # Playwright failures only (94%)
rtk pytest              # Python test failures only (90%)
rtk rake test           # Ruby test failures only (90%)
rtk rspec               # RSpec test failures only (60%)
rtk test <cmd>          # Generic test wrapper - failures only
```

### Git (59-80% savings)
```bash
rtk git status          # Compact status
rtk git log             # Compact log (works with all git flags)
rtk git diff            # Compact diff (80%)
rtk git show            # Compact show (80%)
rtk git add             # Ultra-compact confirmations (59%)
rtk git commit          # Ultra-compact confirmations (59%)
rtk git push            # Ultra-compact confirmations
rtk git pull            # Ultra-compact confirmations
rtk git branch          # Compact branch list
rtk git fetch           # Compact fetch
rtk git stash           # Compact stash
rtk git worktree        # Compact worktree
```

Note: Git passthrough works for ALL subcommands, even those not explicitly listed.

### GitHub (26-87% savings)
```bash
rtk gh pr view <num>    # Compact PR view (87%)
rtk gh pr checks        # Compact PR checks (79%)
rtk gh run list         # Compact workflow runs (82%)
rtk gh issue list       # Compact issue list (80%)
rtk gh api              # Compact API responses (26%)
```

### JavaScript/TypeScript Tooling (70-90% savings)
```bash
rtk pnpm list           # Compact dependency tree (70%)
rtk pnpm outdated       # Compact outdated packages (80%)
rtk pnpm install        # Compact install output (90%)
rtk npm run <script>    # Compact npm script output
rtk npx <cmd>           # Compact npx command output
rtk prisma              # Prisma without ASCII art (88%)
```

### Files & Search (60-75% savings)
```bash
rtk ls <path>           # Tree format, compact (65%)
rtk read <file>         # Code reading with filtering (60%)
rtk grep <pattern>      # Search grouped by file (75%)
rtk find <pattern>      # Find grouped by directory (70%)
```

### Analysis & Debug (70-90% savings)
```bash
rtk err <cmd>           # Filter errors only from any command
rtk log <file>          # Deduplicated logs with counts
rtk json <file>         # JSON structure without values
rtk deps                # Dependency overview
rtk env                 # Environment variables compact
rtk summary <cmd>       # Smart summary of command output
rtk diff                # Ultra-compact diffs
```

### Infrastructure (85% savings)
```bash
rtk docker ps           # Compact container list
rtk docker images       # Compact image list
rtk docker logs <c>     # Deduplicated logs
rtk kubectl get         # Compact resource list
rtk kubectl logs        # Deduplicated pod logs
```

### Network (65-70% savings)
```bash
rtk curl <url>          # Compact HTTP responses (70%)
rtk wget <url>          # Compact download output (65%)
```

### Meta Commands
```bash
rtk gain                # View token savings statistics
rtk gain --history      # View command history with savings
rtk discover            # Analyze Claude Code sessions for missed RTK usage
rtk proxy <cmd>         # Run command without filtering (for debugging)
rtk init                # Add RTK instructions to CLAUDE.md
rtk init --global       # Add RTK to ~/.claude/CLAUDE.md
```

## Token Savings Overview

| Category | Commands | Typical Savings |
|----------|----------|-----------------|
| Tests | vitest, playwright, cargo test | 90-99% |
| Build | next, tsc, lint, prettier | 70-87% |
| Git | status, log, diff, add, commit | 59-80% |
| GitHub | gh pr, gh run, gh issue | 26-87% |
| Package Managers | pnpm, npm, npx | 70-90% |
| Files | ls, read, grep, find | 60-75% |
| Infrastructure | docker, kubectl | 85% |
| Network | curl, wget | 65-70% |

Overall average: **60-90% token reduction** on common development operations.
<!-- /rtk-instructions -->