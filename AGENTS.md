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

A hook (`tools/audit/token_discipline_hook.py`, wired in `.claude/settings.json`) backs this up with measurement: it records every Read/Edit/Write per session in `.claude/agent-memory/token_discipline/`, warns on duplicate `(path, offset, limit)` reads, warns on reads of banned dirs (`build/`, `*.log`, etc.), warns when the read/edit ratio falls below 4:1 after at least 5 edits, and emits a final summary on Stop. Treat hook warnings as authoritative — they observe the actual tool calls, not your recollection.

## Workflow

### 1. Research & Analysis
- **Scope-first:** Start tasks with exact path(s), symbol(s), and line range(s).
- **Token Discipline:** See the [Token Discipline](#token-discipline) section above. Use line-ranged reads (`rtk read -o <start> -l <limit>`). Never re-read a file after a successful edit.
- **Large files:** For files >300 lines, always read with `rtk read -o <start> -l <limit>`. Cap at 100 lines per read. See the File-Specific Bans & Caps table.
- **kb.json:** `rtk jq` ONLY — never use the Read tool. See the File-Specific Bans & Caps table.
- **JSON Mastery:** ALWAYS use `rtk jq` for querying/parsing `kb.json`. Never use `python -c`.
- **Pre-edit research:** Before editing any function, run `rtk rg '<function_name>' src/` to find all callers and related symbols.
- **Ghidra Pre-flight:** Before using any `ghidra` or `ghidra-live` MCP tool, run `python3 tools/audit/check_ghidra_mcp.py`. If it fails, alert the user and stop.
- **Ghidra Token Discipline:** Never dump full decompile/disassembly "just in case." Use `get_function_callees` first (tiny output) to map the call graph. When you already have ported C, don't re-decompile the same function — diff only the specific section. For disassembly verification, use `read_memory` at a specific address range, not `disassemble_function`. Decompile the smallest/most-targeted function first (leaf helpers, not 400-line handlers).
- **Tooling:** Always prefix with `rtk`. Use `rtk fd` (files), `rtk rg` (text), `rtk ast-grep` (structure), `rtk fzf` (selecting). If `rtk rg` returns empty or errors with flags, fall back to bare `grep -rn` immediately — don't retry with different flag permutations.

### 2. Implementation & kb.json Discipline
- **C89 only.** The original binary was compiled with MSVC 7.1, which is a C89 compiler. All lifted code must be valid C89: declare all variables at the top of their block scope, before any statements. No mixed declarations (C99). This is enforced by VC71 verify. We will stay C89 until the game is fully reimplemented.
- **No Speculation:** Do not invent behavior or names without binary evidence.
- **ABI Stability:** `@<reg>` annotations in `kb.json` are **immutable**. Never remove or change register assignments.
- **New Symbols:** Register-arg callees must be added to `kb.json` with `@<reg>` and called by name.
- **No Inline ASM:** The build system handles thunks via `kb.json`. Do not use inline assembly in C.
- **`ported` is a real toggle.** Setting `"ported": false` on a function in `kb.json` deactivates the patch at build time: `tools/build/patch.py` skips the redirect at the original address AND writes a JMP at the impl entry that tail-calls the original. Both original code and our lifted C code reach original behavior. The C implementation stays in source and in the binary as dead code. Use this for **bisecting** which lift introduced a regression — flip ports off one at a time and rebuild. Re-activate by setting `"ported": true`. The pre-commit hook `pre-commit-ported-deactivations.sh` warns (does not block) when a commit contains active deactivations so you don't ship them by accident; `tools/audit/check_ported_deactivations.py --check` exits non-zero for CI gating.
- **Separation:** Keep logic changes separate from cleanup/formatting.
- **Auto-lift delegates to `/lift`:** `tools/llm_auto_lift.py` provides target selection, liftability scoring, and Ghidra context caching. Code generation is delegated to `/lift` which has full agent context. Legacy `review`/`promote` subcommands exist for old batch artifacts.
- **Never transcribe MSVC intrinsics as C function calls.** Ghidra shows them as regular calls but they have non-standard ABIs that corrupt the stack or registers when called from C. Use the equivalent C idiom — the compiler generates the intrinsic automatically:

  | Address | Intrinsic | Refs | Ghidra shows | Write in C instead |
  |---------|-----------|------|--------------|-------------------|
  | 0x1d90e0 | `_chkstk` | 71 | `regparm(1)` call | declare locals normally (or `static` for large buffers) |
  | 0x1d9068 | `_ftol2` | 228 | `_ftol2(var)` or cast | `(int)float_expr` |
  | 0x1dd5c8 | `__SEH_prolog` | 74 | mangled params | `__try/__except` — clang supports this natively on `-target i386-pc-win32`; see `docs/seh-handling.md` |
  | 0x1dd601 | `__SEH_epilog` | 73 | mangled return | (paired with prolog — handled automatically by `__try/__except`) |
  | 0x1dd620 | `_allmul` | 10 | 4-arg call | `(int64_t)a * b` |
  | 0x1dd660 | `_aullshr` | 1 | register call | `(uint64_t)val >> shift` |
  | 0x1dd680 | `_aullrem` | 1 | 4-arg call | `(uint64_t)a % b` |
  | 0x1dd770 | `_aulldiv` | 1 | 4-arg call | `(uint64_t)a / b` |

  Never add these to kb.json. They are compiler runtime, not game functions.

  **SEH functions specifically:** All 74 `__SEH_prolog` callers are LIBCMT/XAPILIB CRT helpers. Use `__try { <body> } __except(1) { return 0; }` (or the appropriate error return). The SEH is a safety net — in normal execution the `__try` body runs to completion. `vc71_verify` will report ~55% match because the frame shape differs (clang emits an inline SEH frame; the original uses compact thunks). This is expected and accepted for these CRT wrappers. New source files go in `src/halo/cseries/xbox_crt.c`; register the object as `XAPILIB:xbox_crt.obj` in `kb.json`.

- **MSVC-style `__asm` is broken under clang.** Our `-target i386-pc-win32` flag makes clang define `_MSC_VER`, so `#ifdef _MSC_VER` guards select MSVC-style `__asm {}` blocks. Unlike GCC-style `asm volatile`, MSVC-style `__asm` does **not** communicate register clobbers to the optimizer. If a GPR (EAX, EDX, ECX, etc.) holds a live value before the `__asm` block, and the asm instruction overwrites that register, the optimizer will reuse the stale value after the block — causing silent corruption or ACCESS_VIOLATION. **Rule:** Always guard MSVC-only asm with `#if defined(_MSC_VER) && !defined(__clang__)`, and provide a GCC-style `asm volatile` alternative in the `#else` branch with proper output constraints and clobber lists. **Known dangerous instructions:** `RDTSC` (clobbers EAX:EDX), `CPUID` (clobbers EAX/EBX/ECX/EDX), `MUL`/`DIV` (implicit EAX/EDX). FPU-only asm (`FLD`, `FMUL`, `FPATAN`, etc.) is safe since it doesn't touch GPRs.

- **Verify callee buffer sizes.** Ghidra may under-size local buffers. When a lifted function passes a stack buffer to a callee, check the callee's `memset`/init size in disassembly — it reveals the true required size. Example: `FUN_0013fc20` (object placement init) writes 0x88 bytes; Ghidra showed the caller's buffer as 0x30, causing a stack overflow.

- **MSVC stack layout overlap hazard.** When a lifted function calls an **unlifted** function by pointer (vtable, callback, function table), the callee may read from offsets within a local array that MSVC placed overlapping with other local variables. Our clang build uses a different stack layout, so those offsets contain garbage instead of the expected data. **Detection:** decompile the unlifted callee and check every offset it reads from the array parameter. Cross-reference with the original function's `[EBP±N]` disassembly to see if those offsets land on other locals. **Fix:** explicitly copy the required data into the array at the expected offsets before the call. Example: `FUN_0009fd30` passes `marker_buf` to creation physics (vtable `0x26ab10`). The callee reads position from `marker_buf+0x60`, which in the original MSVC layout overlaps with `local_position`. Fix was to copy `local_position` into `marker_buf+0x60` explicitly.

- **Verify every call site against disassembly.** The decompiler is a draft. Four known pitfalls:
  1. **Register aliasing:** Ghidra loses track of EBX/ESI/EDI in long functions and substitutes the wrong variable. Trace each PUSH backward from the CALL instruction.
  2. **Push-then-fstp:** MSVC passes floats via `PUSH <dummy>; FSTP [ESP]`. Ghidra reports the dummy (often a pointer) as the argument instead of the FPU-computed float.
  3. **Struct field rotation:** MSVC reorders stores for scheduling. Ghidra reassembles them in instruction order, producing wrong struct offsets. Derive offsets from `MOV [EBP±N]` in disassembly, not the decompiler.
  4. **Cross-product operand swap:** `cross(A, B)` and `cross(B, A)` look nearly identical in the decompiler — the FLD/FMUL order before FSUBP differs but the components look the same. Always verify the subtraction order against disassembly: `cross(A,B)[0] = A[1]*B[2] - A[2]*B[1]`. Getting it backwards negates the vector, which can cause invisible geometry, flipped UV mapping, or reflected projections.
  5. **Buffer-alias confusion:** Ghidra names every stack offset as an independent `local_XX` variable, even when the offset falls inside a local buffer. After any call that takes a buffer pointer, check whether subsequent `local_XX` reads are buffer fields — compute `EBP_offset - buffer_base_EBP_offset`. If the result is within `[0, buffer_size)`, the read is from the buffer, not a separate variable. Example: `FUN_000f90d0` had `damage_params` at EBP-0x8C (0xac bytes). After `FUN_00137d20(damage_params, ...)`, Ghidra showed `local_44` (EBP-0x44 = damage_params+0x48) and `local_40` (EBP-0x40 = damage_params+0x4C) as independent locals. The lift incorrectly read these from `col_result` instead of `damage_params`, causing wrong impact effects on all objects.

### 3. Build & Verification
- **Lift Pipeline:** Use `rtk python3 tools/lift_pipeline.py --target <name_or_addr> --no-metadata-update --verify-policy auto` as the primary post-lift validation orchestrator. It runs build, ABI audit, VC71 verify when a delinked reference is mapped, optional behavior/runtime checks, and low-match policy gates.
- **Hazard Scan:** Run `rtk python3 tools/audit/check_lift_hazards.py` after source edits or when reviewing auto-lift output. Treat intrinsic calls, undersized buffers, suspicious duplicate arguments, and pointer-as-float warnings as blockers until investigated.
- **Golden Master Test Harness:** A specialized test harness intercepts the engine boot in `src/halo/shell_xbox.c`. It lets you run functions inside the engine context and verify their side-effects/return values against the exact Xbox ASM output. 
  - *Usage:* Add tests to `src/halo/test_harness.c`. Ensure your function is unmapped in `kb.json` (`"ported": false`), run `rtk python3 tools/verify/run_golden_tests.py` to capture the original FPU hex values. Then map your function (`"ported": true`) and press Enter to verify your C implementation.
  - *Use cases:* FPU math functions, struct/object initializers, and complex isolated state transitions.
- **Live Memory Capture + State Replay:** For Unicorn equivalence that under-covers live engine paths, use `tools/equivalence/dump_xemu_memory.py` to capture full Xbox memory and build snapshots, then replay with `unicorn_diff.py --state-snapshot <path>`. Workflow:
  1. Get into the desired game state in xemu (e.g. MP match with players).
  2. Dump full memory: pause xemu, run `pmemsave 0 134217728 "G:\dev\halo\artifacts\xbox_full_memory.bin"` via QMP monitor.
  3. Build a compact snapshot: `rtk python3 tools/equivalence/dump_xemu_memory.py snapshot --dump artifacts/xbox_full_memory.bin --target <func> --full -o artifacts/snapshot_<func>.json`
  4. Add `--arg <name> <value>` to pin function arguments to real handles (e.g. `--arg dead_handle 0xec700000`).
  5. Run equivalence: `rtk python3 tools/equivalence/unicorn_diff.py <target> --seeds 50 --allow-stubs --mem-trace --state-snapshot artifacts/snapshot_<func>.json`
  - For real Xbox: `rtk python3 tools/equivalence/dump_xemu_memory.py dump --xbdm --xbox-ip <IP>` dumps via XBDM `getmem` (virtual memory, works on real hardware).
  - The `--full` flag maps all non-zero pages (includes code segments callees need); without it, only targeted data regions are extracted.
  - Do not use QEMU `savevm`/`loadvm` for oracle tests — those restore old loaded-XBE code pages.
- **Dual-Oracle Runtime Harness:** For high-value stateful targets, prefer a same-process harness case over two separate emulator runs. Clone inputs, call the original implementation, restore inputs, call the candidate implementation, then compare return values, mutated buffers, selected globals, and structured debug records in one initialized engine state.
- **RTK Build:** Use `rtk python3 tools/build/build.py -q --target halo` (warnings/errors only).
- **Build error triage — undeclared/renamed symbols:** When the compiler reports `call to undeclared function 'FUN_XXXXXXXX'` or `wrong argument count`, do NOT read source files first. Use two shell commands:
  ```bash
  rtk rg "FUN_XXXXXXXX" build/generated/decl.h   # → not there = renamed
  rtk jq '[.. | objects | select(.addr? == "0xXXXXX")] | .[0] | {name, decl}' kb.json  # → real name + signature
  ```
  The function was renamed in kb.json; update the call site to use the new name. Source reads are only needed if the signature itself is wrong.
- **Build error triage — `ported=true` symbol absent from EXE exports:** When `patched_xbe` fails with "symbol absent from EXE exports", the **first** step is always:
  ```bash
  grep -rn "FUN_XXXXXXXX" src/
  ```
  - **Implementation found** → the source file has compile errors or is missing from `CMakeLists.txt`. Fix the errors; do NOT set `ported=false`.
  - **No implementation found** → the function was marked ported without an implementation being written. Set `ported=false` until a proper lift is done.
- **VC71 Verify:** After lifting FPU-heavy functions (geometry, math, projections), run `rtk python3 tools/verify/vc71_verify.py src/path/to/file.c` to compile with Visual C++ 7.1 and compare against the delinked reference. Review any `[FPU-WARN]` output — it flags potential operand-order bugs. Requires a delinked reference in `delinked/` (export via `ghidra-live` MCP).
- **Delinked-reference precondition (REQUIRED before VC71 verify):** Before running `vc71_verify.py` for any newly lifted target, confirm the function's symbol exists in the delinked reference:
  ```
  objdump -t delinked/<object>.obj | grep -E "FUN_<addr_no_0x>|<funcname>"
  ```
  If the target's symbol is missing, you **MUST** export a new delinked reference via `mcp__ghidra-live__export_delinked_object` covering the correct address range (start at the earliest unported function in the TU, end past the last function) before running VC71 verify. Committing a lift without a delinked reference for the target is not allowed — there is no byte-match evidence without one.
- **Permuter (`/verify permute`):** Last-mile match optimizer. Use ONLY when VC71 match is in **[85, 98]%** with a mapped delinked reference. Below 85% the lift has a structural bug — fix it first; permuter cannot recover from real correctness issues. Above 98% it is not worth the cycles. Never accept a permutation that lowers the existing match; always re-run the lift pipeline against the new source.
- **Equivalence (`/verify equivalence`):** Unicorn-Engine behavioral differential with seeded inputs, coverage tracking, and concolic feedback. Use when byte-match is weak evidence: FPU-heavy code, hashes/serializers, or structurally capped lifts (e.g. SEH wrappers stuck at ~55%). Works for both leaf and non-leaf functions:
  - **Pure leaves:** Run `rtk python3 tools/equivalence/unicorn_diff.py <target> --seeds 100`
  - **Non-leaf or FPU-heavy:** Use `rtk python3 tools/equivalence/unicorn_diff.py <target> --seeds 100 --allow-stubs --float-tolerance 32`. `--allow-stubs` stubs known callees (csmemcpy, fabs, _chkstk, etc.) and seeds known XBE globals. `--float-tolerance N` compares float* scratch buffers with N ULP tolerance instead of byte-exact (recommended: 16–32 for typical geometry, up to 256 for long chains), accounting for x87 rounding differences across compiler versions.
  - **Coverage & confidence:** Each run reports code coverage % and a confidence tier (high/moderate/weak). If coverage < 60%, a **concolic Phase 2** automatically injects non-zero values into zero-filled global memory to reach untested branches. The confidence tier and coverage are persisted to `leaf_cache.json` and shown in pipeline output.
  - **Memory-trace differential:** `--mem-trace` (enabled by default in pipeline/batch) compares all non-stack memory writes between oracle and candidate, catching side-effect bugs (wrong struct offset, missing writes) that return-value comparison misses.
  - **State snapshots:** `--state-snapshot path.json` loads real game-state memory (captured from xemu) instead of zero-fill. Use for functions that depend on complex runtime state (linked lists, hash tables). Capture with `tools/equivalence/state_snapshot.py`.
  - Each run records leaf classification and confidence to `tools/equivalence/leaf_cache.json`, boosting pure leaves by `+5 eq_pure_leaf` and high-confidence results by `+3 eq_high_conf` in future selector runs.
  - **Verification decision:** Match ≥99% → done (byte-match sufficient); [85, 98]% with delinked ref → try `/verify permute` first; [85, 98]% pure leaf FPU-heavy → `/verify equivalence` first; <85% → investigate lift (don't permute); structurally capped (~55%) → equivalence to prove behavior. **Interpret confidence:** `high` = strong evidence; `moderate` = concolic improved coverage but returns monotonic; `weak` = only early-exit path tested, needs investigation or live memory replay.
- **Auto-Lift Selector:** Use `rtk python3 tools/llm_auto_lift.py select` for target selection and `cache-context` for Ghidra context caching. Code generation is delegated to `/lift`.
- **Auto-Build After Lift:** `/lift` and `/auto-lift` automatically trigger `/build` on completion to catch compile errors and linter issues early. This is enforced by `.claude/settings.json` hooks.
- **Validation:** Run the narrowest meaningful validation first.
- **XBDM Priority:** Prefer real Xbox XBDM verification over xemu when available.
- **Failure Policy:** If an edit fails, re-read only affected ranges before retrying.

### 4. Commit Discipline
- **Use `/lift` for all new function ports.** Do not manually implement and commit lift work without going through the `/lift` skill. It runs ABI audit, build, and verification stages that catch real bugs (calling convention mismatches, register-arg errors). Bypassing it has caused page faults and silent regressions.
- **No Freeform Messages:** Never write freeform lift commit messages.
- **Auto-Lift Commit Policy:** `/auto-lift` auto-commits on pipeline success (via `generate_lift_commit.py`) and reverts+logs on failure (to `artifacts/auto_lift/failures/`). Legacy `review`/`promote` artifacts must not be committed directly.
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
- Need to reduce FUN_ backlog or improve frontier accuracy: `rtk python3 tools/analysis/fun_pipeline.py status` → `reclassify [--apply]` → `prioritize --priority P0`.
- Need manual implementation: `/lift <target>`.
- Need auto-lift candidate: `/auto-lift` to select + cache context, then `/lift <target>`.
- Need validation, delink, hazards, or failure triage: `/verify <mode> ...`.
- Need live-state equivalence: dump memory with `rtk python3 tools/equivalence/dump_xemu_memory.py` (QMP or `--xbdm`), build snapshot with `snapshot --dump <path> --target <func> --full`, then run `/verify equivalence <target> --state-snapshot <path>`.
- Need runtime oracle coverage: `/verify golden <target>` or `/verify dual-oracle <target>` when a same-process harness case exists.
- Need real Xbox probing: `/deploy --xbe-only`, then `/xbdm <mode>`.
- Need xemu build/load: `/build` or `/xemu build-load`.
- Need regression investigation: `/debug-regression <symptom>`.

## Analysis Tools
- **`tools/analysis/frontier.py`** — Decompilation frontier scoring and target recommendations.
- **`tools/analysis/fun_pipeline.py`** — FUN_ function naming pipeline. Four stages: `reclassify` moves `<common>` FUN_ functions to named objects (feeds frontier.py accuracy); `prioritize` tiers remaining FUN_ functions by evidence quality (P0 = attributed + signature, P3 = unclassified); `propose` outputs a Ghidra work queue for decompile-based naming; `apply` writes proposed names back to kb.json. Run `status` first to gauge naming debt. Prerequisite for accurate frontier scoring when `<common>` is large.
- **`tools/llm_auto_lift.py`** — Target selection, liftability scoring, and Ghidra context caching. Use `select` for combined frontier/liftability target choice; `cache-context` to pre-cache Ghidra output; code generation delegated to `/lift`.
- **`tools/analysis/classify_common.py`** — Analyze `<common>` functions for reclassification into proper objects. Uses delinker exports and XBE `__FILE__` strings as evidence. Run with `--delinker-analyze` for full binary-evidence analysis (requires Ghidra), or `--summary` for a quick static overview.
- **`tools/audit/batch_delink.py`** — Batch-export delinked reference objects for all kb.json objects.
- **`tools/audit/check_lift_hazards.py`** — Build-time hazard scan for common Ghidra/MSVC lifting pitfalls.
- **`tools/lift_pipeline.py`** — Primary lift validation orchestrator for build, ABI audit, VC71 verify, behavior/runtime checks, and low-match policy.
- **`tools/analysis/maintain.py`** — Source file organization and function placement checks.
- **`tools/verify/objdiff_lift.py`** — Structural object diff helper used by the pipeline when reference and candidate objects are available.
- **`tools/verify/vc71_verify.py`** — Compile a source file with Visual C++ 7.1 (`RXDK/xbox/bin/vc71/CL.Exe`) and compare against the delinked reference. Flags FPU operand-order differences that indicate cross-product swaps, subtraction direction errors, etc. Use `--fpu-only` for focused output. SQLite-backed cache (`tools/verify/vc71_cache.py`) skips compile+diff on unchanged inputs; use `--no-cache` to force recompute or `tools/verify/cache_stats.py` to inspect hit rate.
- **`tools/verify/compare_obj.py`** — LCS-based instruction comparison between two COFF objects. Used by `vc71_verify.py`.
- **`tools/lift/draft_decompiler.py`** — Stage 1+2+4 Ghidra pseudocode rewriter. Run as a pre-pass on raw Ghidra output before lifting: canonicalizes synthetic types/ops (undefined4, CONCAT44, ZEXT/SEXT, etc.), rewrites MSVC intrinsics per the table above, and wraps `__SEH_prolog/epilog`-bracketed bodies in native `__try/__except`. Each rewrite emits an `/* AUTO: ... */` annotation.
- **`tools/lift/buffer_alias_detector.py`** — Pre-pass detector for hazard #5 (buffer-alias confusion). Annotates every suspect `local_YY` read with a HIGH-RISK or verify comment based on whether a qualifying CALL preceded it. Exits 1 on HIGH-RISK hits for pipeline gating.
- **`tools/audit/extract_reg_args.py`** — kb.json ↔ kb_reg_baseline.json drift detector for `@<reg>` annotations. Run `--check` after kb.json edits. Both `pre-commit-reg-baseline-drift.sh` and `generate_lift_commit.py` hard-fail on detected drift (use `--apply` to merge missing entries; `--no-verify` / `--skip-abi-audit` to bypass).
- **`tools/permuter/run.py`** — decomp-permuter wrapper for VC71/x86/MSVC. Last-mile match optimizer for functions in the 85–98% VC71 band. Random AST permutations + LCS scoring; reuse `tools/permuter/compile.sh` and `score.py` adapters. Upstream lives at `third_party/decomp-permuter/` (gitignored; clone per `docs/permuter-adapter.md`). **Always pass `-q`** to suppress diagnostic noise and the permuter's own progress stream — only the final summary and NEW BEST candidates are printed in that mode.
- **`tools/equivalence/unicorn_diff.py`** — Unicorn-Engine differential tester. Runs MSVC-delinked oracle and clang-built candidate `.obj` in two emulators with seeded inputs; compares CPU/FPU state at RET. Three-phase testing: Phase 1 (random/corner seeds), automatic concolic Phase 2 (injects non-zero globals to reach untested branches when coverage < 60%), and memory-trace differential (compares all non-stack writes). Reports coverage %, confidence tier (high/moderate/weak), and persists both to `leaf_cache.json`. Key flags: `--allow-stubs` (non-leaf), `--float-tolerance N` (FPU), `--mem-trace` (write comparison), `--state-snapshot PATH` (inject real game state), `--no-concolic` (disable Phase 2).
- **`tools/equivalence/concolic.py`** — Coverage-guided concolic feedback for unicorn_diff. Disassembles oracle code, finds untaken branches, determines what memory values would reach them, and generates injection overrides. Called automatically when Phase 1 coverage < 60%.
- **`tools/equivalence/state_snapshot.py`** — State snapshot capture and replay. `load_snapshot(path)` loads JSON into memory overrides for Unicorn. Low-level building block; prefer `dump_xemu_memory.py` for full-memory workflows.
- **`tools/equivalence/dump_xemu_memory.py`** — Full Xbox memory dump and snapshot builder. `dump` subcommand captures 128MB via xemu QMP (`pmemsave`, physical) or XBDM (`getmem`, virtual — works on real Xbox hardware). `snapshot --dump <path> --target <func>` extracts targeted regions; `--full` maps all non-zero pages (needed when callees aren't stubbed). `--arg <name> <value>` pins function arguments to real datum handles. Output is a `state_snapshot.json` compatible with `unicorn_diff.py --state-snapshot`.
- **`tools/analysis/punpckhdq_import.py`** — Imports PDB-derived TU/symbol corpus from punpckhdq/halo (debug-build PDB) and proposes real names for our `FUN_<addr>` placeholders. `tools/analysis/apply_punpckhdq_renames.py` performs textual renames across kb.json/baseline/src after dry-run review.

## Architecture and Skills
- `halo-xbox-re`: RE doctrine and evidence rules.
- `halo-re-lift`: Lift workflow and ABI-specific execution.
- `halo-verify-debug`: Verification lanes, delink comparison, and regression debugging.
- `halo-build-xemu`: Build and XBE deployment workflow.
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
