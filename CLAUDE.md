# Agent Instructions

These rules apply to any coding agent used in this repo (Claude, OpenCode, subagents). Keep `AGENTS.md` and `CLAUDE.md` aligned.

## Mission

Recover Halo CE Xbox behavior faithfully and incrementally.

- **Binary is source of truth:** Evidence-based lifting only.
- **Explicit Unknowns:** Prefer `unknown` or `field_XX` over plausible guesses.
- **Small Changes:** Make small, reviewable commits.
- **Preserve Shape:** Maintain original ABI, layout, side effects, and control-flow.

## Token Discipline

CodeBurn analysis: **~1.7M tokens wasted in 7 days** (~$0.64). The root causes are fixable with strict discipline. These rules are non-negotiable.

### Read-Once Rule
- **Never read the same file twice in one task.** Before reading, ask: "Have I already read this file in this conversation?"
- **The Edit tool confirms success.** Do not re-read to verify your edit worked.
- **Failure exception only:** If an edit fails, re-read *only* the failing line range (≤20 lines), never the full file.

### Research Ratio Gate (4:1 minimum)
- **Target: 4+ reads per edit.** CodeBurn measured 2.2:1 (1,958 reads vs. 878 edits), causing ~932K wasted tokens from edit-without-research retries.
- Before editing any function, perform at least these research steps:
  1. `rtk rg '<function_name>' src/` — find all callers and related symbols.
  2. `rtk read <file> -o <start> -l <limit>` — read the exact target lines only.
  3. `rtk jq '<filter>' kb.json` — verify signatures and ABI.
  4. `rtk git diff -- src/ kb.json` — check current uncommitted state.
- If you have not done 4 reads, do more research before editing.

### File-Specific Bans & Caps
| File / Directory | Action | Max Lines | Why |
|------------------|--------|-----------|-----|
| `kb.json` | `rtk jq` ONLY | **0** | 6,000+ lines; 239 redundant reads = ~143K tokens |
| `objects.c`, `units.c`, `sound_manager.c` | `rtk read -o -l` | **100** | 100+ redundant reads each = ~300K tokens |
| `build/`, `build_debug/`, `node_modules/`, `.git/`, `halo-patched/`, `__pycache__/`, `dist/` | **NEVER** | **0** | Generated artifacts; 7 reads = ~4K tokens for zero value |
| Any `*.log` | **NEVER** | **0** | Run the command instead of reading output |

### Session Memory
Maintain a mental ledger of files already read in this conversation. If you need a fact from a file you've already read, recall it from context or `rtk rg` for the specific string — do not re-read the file.

## Workflow

### 1. Research & Analysis
- **Scope-first:** Start tasks with exact path(s), symbol(s), and line range(s).
- **Token Discipline:** See the [Token Discipline](#token-discipline) section above. Use line-ranged reads (`rtk read -o <start> -l <limit>`). Never re-read a file after a successful edit.
- **Large files:** For files >300 lines, always read with `rtk read -o <start> -l <limit>`. Cap at 100 lines per read. See the File-Specific Bans & Caps table.
- **kb.json:** `rtk jq` ONLY — never use the Read tool. See the File-Specific Bans & Caps table.
- **JSON Mastery:** ALWAYS use `rtk jq` for querying/parsing `kb.json`. Never use `python -c`.
- **Pre-edit research:** Before editing any function, run `rtk rg '<function_name>' src/` to find all callers and related symbols.
- **Ghidra Pre-flight:** Before using any `ghidra` or `ghidra-live` MCP tool, run `python3 tools/audit/check_ghidra_mcp.py`. If it fails, alert the user and stop.
- **Tooling:** Always prefix with `rtk`. Use `rtk fd` (files), `rtk rg` (text), `rtk ast-grep` (structure), `rtk fzf` (selecting).

### 2. Implementation & kb.json Discipline
- **C89 only.** The original binary was compiled with MSVC 7.1, which is a C89 compiler. All lifted code must be valid C89: declare all variables at the top of their block scope, before any statements. No mixed declarations (C99). This is enforced by VC71 verify. We will stay C89 until the game is fully reimplemented.
- **No Speculation:** Do not invent behavior or names without binary evidence.
- **ABI Stability:** `@<reg>` annotations in `kb.json` are **immutable**. Never remove or change register assignments.
- **New Symbols:** Register-arg callees must be added to `kb.json` with `@<reg>` and called by name.
- **No Inline ASM:** The build system handles thunks via `kb.json`. Do not use inline assembly in C.
- **Separation:** Keep logic changes separate from cleanup/formatting.
- **Auto-lift is untrusted:** `tools/llm_auto_lift.py` may generate and validate candidates, but generated code is untrusted until reviewed against binary evidence and validation artifacts. Run `promote` dry first, require explicit user approval for `promote --apply`, and never auto-commit auto-lift results.
- **Never transcribe MSVC intrinsics as C function calls.** Ghidra shows them as regular calls but they have non-standard ABIs that corrupt the stack or registers when called from C. Use the equivalent C idiom — the compiler generates the intrinsic automatically:

  | Address | Intrinsic | Refs | Ghidra shows | Write in C instead |
  |---------|-----------|------|--------------|-------------------|
  | 0x1d90e0 | `_chkstk` | 71 | `regparm(1)` call | declare locals normally (or `static` for large buffers) |
  | 0x1d9068 | `_ftol2` | 228 | `_ftol2(var)` or cast | `(int)float_expr` |
  | 0x1dd5c8 | `__SEH_prolog` | 74 | mangled params | `__try/__except` (or skip SEH functions) |
  | 0x1dd601 | `__SEH_epilog` | 73 | mangled return | (paired with prolog) |
  | 0x1dd620 | `_allmul` | 10 | 4-arg call | `(int64_t)a * b` |
  | 0x1dd660 | `_aullshr` | 1 | register call | `(uint64_t)val >> shift` |
  | 0x1dd680 | `_aullrem` | 1 | 4-arg call | `(uint64_t)a % b` |
  | 0x1dd770 | `_aulldiv` | 1 | 4-arg call | `(uint64_t)a / b` |

  Never add these to kb.json. They are compiler runtime, not game functions.

- **Verify callee buffer sizes.** Ghidra may under-size local buffers. When a lifted function passes a stack buffer to a callee, check the callee's `memset`/init size in disassembly — it reveals the true required size. Example: `FUN_0013fc20` (object placement init) writes 0x88 bytes; Ghidra showed the caller's buffer as 0x30, causing a stack overflow.

- **MSVC stack layout overlap hazard.** When a lifted function calls an **unlifted** function by pointer (vtable, callback, function table), the callee may read from offsets within a local array that MSVC placed overlapping with other local variables. Our clang build uses a different stack layout, so those offsets contain garbage instead of the expected data. **Detection:** decompile the unlifted callee and check every offset it reads from the array parameter. Cross-reference with the original function's `[EBP±N]` disassembly to see if those offsets land on other locals. **Fix:** explicitly copy the required data into the array at the expected offsets before the call. Example: `FUN_0009fd30` passes `marker_buf` to creation physics (vtable `0x26ab10`). The callee reads position from `marker_buf+0x60`, which in the original MSVC layout overlaps with `local_position`. Fix was to copy `local_position` into `marker_buf+0x60` explicitly.

- **Verify every call site against disassembly.** The decompiler is a draft. Three known pitfalls:
  1. **Register aliasing:** Ghidra loses track of EBX/ESI/EDI in long functions and substitutes the wrong variable. Trace each PUSH backward from the CALL instruction.
  2. **Push-then-fstp:** MSVC passes floats via `PUSH <dummy>; FSTP [ESP]`. Ghidra reports the dummy (often a pointer) as the argument instead of the FPU-computed float.
  3. **Struct field rotation:** MSVC reorders stores for scheduling. Ghidra reassembles them in instruction order, producing wrong struct offsets. Derive offsets from `MOV [EBP±N]` in disassembly, not the decompiler.
  4. **Cross-product operand swap:** `cross(A, B)` and `cross(B, A)` look nearly identical in the decompiler — the FLD/FMUL order before FSUBP differs but the components look the same. Always verify the subtraction order against disassembly: `cross(A,B)[0] = A[1]*B[2] - A[2]*B[1]`. Getting it backwards negates the vector, which can cause invisible geometry, flipped UV mapping, or reflected projections.

### 3. Build & Verification
- **Lift Pipeline:** Use `rtk python3 tools/lift_pipeline.py --target <name_or_addr> --no-metadata-update --verify-policy auto` as the primary post-lift validation orchestrator. It runs build, ABI audit, VC71 verify when a delinked reference is mapped, optional behavior/runtime checks, and low-match policy gates.
- **Hazard Scan:** Run `rtk python3 tools/audit/check_lift_hazards.py` after source edits or when reviewing auto-lift output. Treat intrinsic calls, undersized buffers, suspicious duplicate arguments, and pointer-as-float warnings as blockers until investigated.
- **Golden Master Test Harness:** A specialized test harness intercepts the engine boot in `src/halo/shell_xbox.c`. It lets you run functions inside the engine context and verify their side-effects/return values against the exact Xbox ASM output. 
  - *Usage:* Add tests to `src/halo/test_harness.c`. Ensure your function is unmapped in `kb.json` (`"ported": false`), run `rtk python3 tools/verify/run_golden_tests.py` to capture the original FPU hex values. Then map your function (`"ported": true`) and press Enter to verify your C implementation.
  - *Use cases:* FPU math functions, struct/object initializers, and complex isolated state transitions.
- **RTK Build:** Use `rtk python3 tools/build/build.py -q --target halo` (warnings/errors only).
- **VC71 Verify:** After lifting FPU-heavy functions (geometry, math, projections), run `rtk python3 tools/verify/vc71_verify.py src/path/to/file.c` to compile with Visual C++ 7.1 and compare against the delinked reference. Review any `[FPU-WARN]` output — it flags potential operand-order bugs. Requires a delinked reference in `delinked/` (export via `ghidra-live` MCP).
- **Auto-Lift Harness:** Use `rtk python3 tools/llm_auto_lift.py score`, `cache-context`, `generate`, and `review` for review-queue candidate generation. Passing validation reduces risk but is not proof of behavioral equivalence without strong delink, golden, or runtime coverage.
- **Validation:** Run the narrowest meaningful validation first.
- **XBDM Priority:** Prefer real Xbox XBDM verification over xemu when available.
- **Failure Policy:** If an edit fails, re-read only affected ranges before retrying.

### 4. Commit Discipline
- **Use `/lift` for all new function ports.** Do not manually implement and commit lift work without going through the `/lift` skill. It runs ABI audit, build, and verification stages that catch real bugs (calling convention mismatches, register-arg errors). Bypassing it has caused page faults and silent regressions.
- **No Freeform Messages:** Never write freeform lift commit messages.
- **No Auto-Lift Commits:** `tools/llm_auto_lift.py` results must not be committed directly or automatically. Promote only after review, then use the standard lift commit generator if the user requested a commit.
- **Standard Command:** After staging changes, run:
  ```bash
  rtk python3 tools/audit/generate_lift_commit.py --batch-name "<short description>" > /tmp/commit_msg.txt
  rtk git commit -F /tmp/commit_msg.txt
  ```
- **ABI audit gate:** `generate_lift_commit.py` runs `audit_reg_abi.py` on all newly ported functions. If any fail, no commit message is generated. Use `--skip-abi-audit` only for emergencies.
- **Pre-commit hook:** The git pre-commit hook runs both the baseline guard and lift ABI audit on staged changes. `--no-verify` is the emergency bypass.

## Repo Guardrails
- **Noisy Dirs:** Never read or search inside `build/`, `build_debug/`, `node_modules/`, `.git/`, `halo-patched/`, `__pycache__/`, `dist/` unless explicitly asked. These are generated artifacts. 7 reads = ~4K tokens for zero value. See the File-Specific Bans & Caps table.
- **No Log Files:** Never read `build.log`, `build_output.log`, or any `.log` file. Run the build or command instead.
- **Scoped Diffs:** When checking changes, scope git diffs to source: `rtk git diff -- src/ kb.json CMakeLists.txt`. Bare `git diff` includes tracked build artifacts.
- **RTK Always:** Prefix ALL shell commands with `rtk` (e.g., `rtk git status`, `rtk pytest`).
- **Output Schema:** For non-trivial work, report: Target, Confirmed, Inferred, Uncertain, Proposed Code, kb.json updates.

## Command Decision Tree
- Need next target: `/frontier` or `rtk python3 tools/llm_auto_lift.py select --limit 20`.
- Need manual implementation: `/lift <target>`.
- Need auto-lift candidate: `/auto-lift select`, then `cache-context`, `generate`, and `review`.
- Need validation, delink, hazards, or failure triage: `/verify <mode> ...`.
- Need real Xbox probing: `/deploy --xbe-only`, then `/xbdm <mode>`.
- Need xemu build/load: `/build` or `/xemu build-load`.
- Need regression investigation: `/debug-regression <symptom>`.

## Analysis Tools
- **`tools/analysis/frontier.py`** — Decompilation frontier scoring and target recommendations.
- **`tools/llm_auto_lift.py`** — Untrusted LLM candidate generator, context cache, validation runner, and review queue. Use `select` for combined frontier/liftability target choice; promotion requires review and explicit approval for apply.
- **`tools/analysis/classify_common.py`** — Analyze `<common>` functions for reclassification into proper objects. Uses delinker exports and XBE `__FILE__` strings as evidence. Run with `--delinker-analyze` for full binary-evidence analysis (requires Ghidra), or `--summary` for a quick static overview.
- **`tools/audit/batch_delink.py`** — Batch-export delinked reference objects for all kb.json objects.
- **`tools/audit/check_lift_hazards.py`** — Build-time hazard scan for common Ghidra/MSVC lifting pitfalls.
- **`tools/lift_pipeline.py`** — Primary lift validation orchestrator for build, ABI audit, VC71 verify, behavior/runtime checks, and low-match policy.
- **`tools/analysis/maintain.py`** — Source file organization and function placement checks.
- **`tools/verify/objdiff_lift.py`** — Structural object diff helper used by the pipeline when reference and candidate objects are available.
- **`tools/verify/vc71_verify.py`** — Compile a source file with Visual C++ 7.1 (`RXDK/xbox/bin/vc71/CL.Exe`) and compare against the delinked reference. Flags FPU operand-order differences that indicate cross-product swaps, subtraction direction errors, etc. Use `--fpu-only` for focused output.
- **`tools/verify/compare_obj.py`** — LCS-based instruction comparison between two COFF objects. Used by `vc71_verify.py`.

## Architecture and Skills
- `halo-xbox-re`: RE doctrine and evidence rules.
- `halo-re-lift`: Lift workflow and ABI-specific execution.
- `halo-verify-debug`: Verification lanes, delink comparison, and regression debugging.
- `halo-build-xemu`: Build/ISO/xemu workflow.
- `halo-xbdm`: RDCP/XBDM workflow for real Xbox.

<!-- rtk-instructions v2 -->
# RTK (Rust Token Killer) - Token-Optimized Commands

## Golden Rule

**Always prefix commands with `rtk`**. Even in command chains with `&&`:
`rtk git add . && rtk git commit -m "msg" && rtk git push`

## RTK Commands by Workflow

| Category | Typical Savings | Examples |
|----------|-----------------|----------|
| **Build** | 70-90% | `rtk cmake`, `rtk tsc`, `rtk lint` |
| **Test** | 90-99% | `rtk pytest`, `rtk cargo test`, `rtk vitest` |
| **Git** | 60-80% | `rtk git status`, `rtk git diff`, `rtk git log` |
| **Files** | 60-75% | `rtk read`, `rtk grep`, `rtk find`, `rtk ls` |
| **Ghidra**| 70-90% | `rtk python3 tools/audit/check_ghidra_mcp.py` |

Use `rtk gain` to view savings statistics.
<!-- /rtk-instructions -->
