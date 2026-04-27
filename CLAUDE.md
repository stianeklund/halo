# Agent Instructions

These rules apply to any coding agent used in this repo (Claude, OpenCode, subagents). Keep `AGENTS.md` and `CLAUDE.md` aligned.

## Mission

Recover Halo CE Xbox behavior faithfully and incrementally.

- **Binary is source of truth:** Evidence-based lifting only.
- **Explicit Unknowns:** Prefer `unknown` or `field_XX` over plausible guesses.
- **Small Changes:** Make small, reviewable commits.
- **Preserve Shape:** Maintain original ABI, layout, side effects, and control-flow.

## Workflow

### 1. Research & Analysis
- **Scope-first:** Start tasks with exact path(s), symbol(s), and line range(s).
- **Token Discipline:** Minimize redundant reads. Use line-ranged reads and `rtk read`. Never re-read a file after a successful edit — the Edit tool confirms success. Only re-read affected line ranges on failure.
- **Large files:** For files >300 lines (objects.c, units.c, etc.), always read with `offset`+`limit`. Never read the full file.
- **kb.json — NEVER use Read tool on it.** It is 6000+ lines. ALWAYS use `rtk jq '<filter>' kb.json` for all lookups and verification. Only edit kb.json directly; never read it to understand structure — query for the specific key you need.
- **JSON Mastery:** ALWAYS use `rtk jq` for querying/parsing `kb.json`. Never use `python -c`.
- **Pre-edit research:** Before editing any function, run `rtk rg '<function_name>' src/` to find all callers and related symbols.
- **Ghidra Pre-flight:** Before using any `ghidra` or `ghidra-live` MCP tool, run `python3 tools/check_ghidra_mcp.py`. If it fails, alert the user and stop.
- **Tooling:** Always prefix with `rtk`. Use `rtk fd` (files), `rtk rg` (text), `rtk ast-grep` (structure), `rtk fzf` (selecting).

### 2. Implementation & kb.json Discipline
- **No Speculation:** Do not invent behavior or names without binary evidence.
- **ABI Stability:** `@<reg>` annotations in `kb.json` are **immutable**. Never remove or change register assignments.
- **New Symbols:** Register-arg callees must be added to `kb.json` with `@<reg>` and called by name.
- **No Inline ASM:** The build system handles thunks via `kb.json`. Do not use inline assembly in C.
- **Separation:** Keep logic changes separate from cleanup/formatting.
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

- **Verify every call site against disassembly.** The decompiler is a draft. Three known pitfalls:
  1. **Register aliasing:** Ghidra loses track of EBX/ESI/EDI in long functions and substitutes the wrong variable. Trace each PUSH backward from the CALL instruction.
  2. **Push-then-fstp:** MSVC passes floats via `PUSH <dummy>; FSTP [ESP]`. Ghidra reports the dummy (often a pointer) as the argument instead of the FPU-computed float.
  3. **Struct field rotation:** MSVC reorders stores for scheduling. Ghidra reassembles them in instruction order, producing wrong struct offsets. Derive offsets from `MOV [EBP±N]` in disassembly, not the decompiler.
  4. **Cross-product operand swap:** `cross(A, B)` and `cross(B, A)` look nearly identical in the decompiler — the FLD/FMUL order before FSUBP differs but the components look the same. Always verify the subtraction order against disassembly: `cross(A,B)[0] = A[1]*B[2] - A[2]*B[1]`. Getting it backwards negates the vector, which can cause invisible geometry, flipped UV mapping, or reflected projections.

### 3. Build & Verification
- **RTK Build:** Use `rtk cmake --build build`.
- **XDK Verify:** After lifting FPU-heavy functions (geometry, math, projections), run `rtk python3 tools/xdk_verify.py src/path/to/file.c` to compile with the original MSVC 7.1 compiler and compare against the delinked reference. Review any `[FPU-WARN]` output — it flags potential operand-order bugs. Requires a delinked reference in `delinked/` (export via `ghidra-live` MCP).
- **Validation:** Run the narrowest meaningful validation first.
- **XBDM Priority:** Prefer real Xbox XBDM verification over xemu when available.
- **Failure Policy:** If an edit fails, re-read only affected ranges before retrying.

### 4. Commit Discipline
- **Use `/lift` for all new function ports.** Do not manually implement and commit lift work without going through the `/lift` skill. It runs ABI audit, build, and verification stages that catch real bugs (calling convention mismatches, register-arg errors). Bypassing it has caused page faults and silent regressions.
- **No Freeform Messages:** Never write freeform lift commit messages.
- **Standard Command:** After staging changes, run:
  ```bash
  rtk python3 tools/generate_lift_commit.py --batch-name "<short description>" > /tmp/commit_msg.txt
  rtk git commit -F /tmp/commit_msg.txt
  ```
- **ABI audit gate:** `generate_lift_commit.py` runs `audit_reg_abi.py` on all newly ported functions. If any fail, no commit message is generated. Use `--skip-abi-audit` only for emergencies.
- **Pre-commit hook:** The git pre-commit hook runs both the baseline guard and lift ABI audit on staged changes. `--no-verify` is the emergency bypass.

## Repo Guardrails
- **Noisy Dirs:** Never read or search inside `build/`, `build_debug/`, `node_modules/`, `.git/`, `halo-patched/` unless explicitly asked. These are generated artifacts.
- **No Log Files:** Never read `build.log`, `build_output.log`, or any `.log` file. Run the build or command instead.
- **Scoped Diffs:** When checking changes, scope git diffs to source: `rtk git diff -- src/ kb.json CMakeLists.txt`. Bare `git diff` includes tracked build artifacts.
- **RTK Always:** Prefix ALL shell commands with `rtk` (e.g., `rtk git status`, `rtk pytest`).
- **Output Schema:** For non-trivial work, report: Target, Confirmed, Inferred, Uncertain, Proposed Code, kb.json updates.

## Analysis Tools
- **`tools/frontier.py`** — Decompilation frontier scoring and target recommendations.
- **`tools/classify_common.py`** — Analyze `<common>` functions for reclassification into proper objects. Uses delinker exports and XBE `__FILE__` strings as evidence. Run with `--delinker-analyze` for full binary-evidence analysis (requires Ghidra), or `--summary` for a quick static overview.
- **`tools/batch_delink.py`** — Batch-export delinked reference objects for all kb.json objects.
- **`tools/maintain.py`** — Source file organization and function placement checks.
- **`tools/xdk_verify.py`** — Compile a source file with the original XDK MSVC 7.1 compiler (`RXDK/xbox/bin/vc71/CL.Exe`) and compare against the delinked reference. Flags FPU operand-order differences that indicate cross-product swaps, subtraction direction errors, etc. Use `--fpu-only` for focused output.
- **`tools/compare_obj.py`** — LCS-based instruction comparison between two COFF objects. Used by `xdk_verify.py`.

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
| **Ghidra**| 70-90% | `rtk python3 tools/check_ghidra_mcp.py` |

Use `rtk gain` to view savings statistics.
<!-- /rtk-instructions -->
