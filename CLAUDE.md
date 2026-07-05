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

### Wording and language
- Be brief and concise. Don't use words as "This is the smoking gun", "Honestly", "I'll be honest".
- Use natural normal concise language.

### Research Ratio Gate (4:1 minimum)
- **Target: 4+ reads per edit.** CodeBurn measured 2.2:1 (1,958 reads vs. 878 edits), causing ~932K wasted tokens from edit-without-research retries.
- Before editing any function, perform at least these research steps:
  1. `rtk rg '<function_name>' src/` — find all callers and related symbols.
  2. `rtk read <file> -o <start> -l <limit>` — read the exact target lines only.
  3. `rtk jq '<filter>' kb.json` — verify signatures and ABI.
  4. `rtk git diff -- src/ kb.json` — check current uncommitted state.
- If you have not done 4 reads, do more research before editing.

### File-Specific Bans & Caps
| File / Directory                                                                             | Action           | Max Lines | Why                                                      |
|----------------------------------------------------------------------------------------------|------------------|-----------|----------------------------------------------------------|
| `kb.json`                                                                                    | `rtk jq` ONLY    | **0**     | 6,000+ lines; 239 redundant reads = ~143K tokens         |
| `objects.c`, `units.c`, `sound_manager.c`                                                    | `rtk read -o -l` | **100**   | 100+ redundant reads each = ~300K tokens                 |
| `build/`, `build_debug/`, `node_modules/`, `.git/`, `halo-patched/`, `__pycache__/`, `dist/` | **NEVER**        | **0**     | Generated artifacts; 7 reads = ~4K tokens for zero value |
| Any `*.log`                                                                                  | **NEVER**        | **0**     | Run the command instead of reading output                |

### Session Memory
Maintain a mental ledger of files already read in this conversation. If you need a fact from a file you've already read, recall it from context or `rtk rg` for the specific string — do not re-read the file.

A hook (`tools/audit/token_discipline_hook.py`, wired in `.claude/settings.json`) backs this up with measurement: it records every Read/Edit/Write per session in `.claude/agent-memory/token_discipline/`, warns on duplicate `(path, offset, limit)` reads, warns on reads of banned dirs (`build/`, `*.log`, etc.), warns when the read/edit ratio falls below 4:1 after at least 5 edits, and emits a final summary on Stop. Treat hook warnings as authoritative — they observe the actual tool calls, not your recollection.

## Workflow

### 1. Research & Analysis
- **Scope-first:** Start tasks with exact path(s), symbol(s), and line range(s).
- **Prior-fix lookup:** Before debugging any regression, crash, hang, assert, visual bug, wrong behavior, or build/deploy failure, run `rtk python3 tools/memory/prior_fixes.py "<symptom or target>"`. Treat matches as hypotheses only; confirm against binary/disassembly/runtime evidence before fixing code.
- **Token Discipline:** See the [Token Discipline](#token-discipline) section above. Use line-ranged reads (`rtk read -o <start> -l <limit>`). Never re-read a file after a successful edit.
- **Large files:** For files >300 lines, always read with `rtk read -o <start> -l <limit>`. Cap at 100 lines per read. See the File-Specific Bans & Caps table.
- **kb.json:** `rtk jq` ONLY — never use the Read tool. See the File-Specific Bans & Caps table.
- **JSON Mastery:** ALWAYS use `rtk jq` for querying/parsing `kb.json`. Never use `python -c`.
- **Pre-edit research:** Before editing any function, run `rtk rg '<function_name>' src/` to find all callers and related symbols.
- **Ghidra Pre-flight:** Before using any `ghidra` or `ghidra-live` MCP tool, run `python3 tools/audit/check_ghidra_mcp.py`. If it fails, alert the user and stop.
- **Ghidra Token Discipline:** Never dump full decompile/disassembly "just in case." Use `get_function_callees` first (tiny output) to map the call graph. When you already have ported C, don't re-decompile the same function — diff only the specific section. For disassembly verification, use `read_memory` at a specific address range, not `disassemble_function`. Decompile the smallest/most-targeted function first (leaf helpers, not 400-line handlers). **After a Ghidra decompile/callers call that informs N edits to one source file, record the target line ranges from the first read. All subsequent edits must use `rtk read -o <line> -l 40` — never re-read the file from offset 0.**
- **Tooling:** Always prefix with `rtk`. Use `rtk fd` (files), `rtk rg` (text), `rtk ast-grep` (structure), `rtk fzf` (selecting). If `rtk rg` returns empty or errors with flags, fall back to bare `grep -rn` immediately — don't retry with different flag permutations.

### 2. Implementation & kb.json Discipline
- **C89 only.** The original binary was compiled with MSVC 7.1, which is a C89 compiler. All lifted code must be valid C89: declare all variables at the top of their block scope, before any statements. No mixed declarations (C99). This is enforced by VC71 verify. We will stay C89 until the game is fully reimplemented.
- **No Speculation:** Do not invent behavior or names without binary evidence.
- **ABI Stability:** `@<reg>` annotations in `kb.json` are **immutable**. Never remove or change register assignments.
- **New Symbols:** Register-arg callees must be added to `kb.json` with `@<reg>` and called by name.
- **No Inline ASM:** The build system handles thunks via `kb.json`. Do not use inline assembly in C.
- **`ported` is a real toggle.** Setting `"ported": false` on a function in `kb.json` deactivates the patch at build time: `tools/build/patch.py` skips the redirect at the original address AND writes a JMP at the impl entry that tail-calls the original. Both original code and our lifted C code reach original behavior. The C implementation stays in source and in the binary as dead code. Use this for **bisecting** which lift introduced a regression — flip ports off one at a time and rebuild. Re-activate by setting `"ported": true`. The pre-commit hook `pre-commit-ported-deactivations.sh` now **BLOCKS** commits that introduce deactivations not in the allowlist at `tools/audit/deactivation_allowlist.json`; bypass with `HALO_ALLOW_DEACTIVATIONS=1 git commit` or `--no-verify`. CI also gates via the `DeactivationGate` job in `main.yml`. The allowlist is seeded from the long-standing dormant/bisect deactivations present on 2026-06-10; add new entries to the allowlist only with a `"reason"` field and run `--update-allowlist` to regenerate. `tools/audit/check_ported_deactivations.py --check` exits non-zero only on non-allowlisted deactivations.
- **Separation:** Keep logic changes separate from cleanup/formatting.
- **Auto-lift delegates to `/lift`:** `tools/llm_auto_lift.py` provides target selection, liftability scoring, and Ghidra context caching. Code generation is delegated to `/lift` which has full agent context. Legacy `review`/`promote` subcommands exist for old batch artifacts.
- **Never transcribe MSVC intrinsics as C function calls.** Ghidra shows them as regular calls but they have non-standard ABIs that corrupt the stack or registers when called from C. Use the equivalent C idiom — the compiler generates the intrinsic automatically:

  | Address  | Intrinsic      | Refs | Ghidra shows          | Write in C instead                                                                                     |
  |----------|----------------|------|-----------------------|--------------------------------------------------------------------------------------------------------|
  | 0x1d90e0 | `_chkstk`      | 71   | `regparm(1)` call     | declare locals normally (or `static` for large buffers)                                                |
  | 0x1d9068 | `_ftol2`       | 228  | `_ftol2(var)` or cast | `(int)float_expr`                                                                                      |
  | 0x1dd5c8 | `__SEH_prolog` | 74   | mangled params        | `__try/__except` — clang supports this natively on `-target i386-pc-win32`; see `docs/seh-handling.md` |
  | 0x1dd601 | `__SEH_epilog` | 73   | mangled return        | (paired with prolog — handled automatically by `__try/__except`)                                       |
  | 0x1dd620 | `_allmul`      | 10   | 4-arg call            | `(int64_t)a * b`                                                                                       |
  | 0x1dd660 | `_aullshr`     | 1    | register call         | `(uint64_t)val >> shift`                                                                               |
  | 0x1dd680 | `_aullrem`     | 1    | 4-arg call            | `(uint64_t)a % b`                                                                                      |
  | 0x1dd770 | `_aulldiv`     | 1    | 4-arg call            | `(uint64_t)a / b`                                                                                      |

  Never add these to kb.json. They are compiler runtime, not game functions.

  **SEH functions specifically:** All 74 `__SEH_prolog` callers are LIBCMT/XAPILIB CRT helpers. Use `__try { <body> } __except(1) { return 0; }` (or the appropriate error return). The SEH is a safety net — in normal execution the `__try` body runs to completion. `vc71_verify` will report ~55% match because the frame shape differs (clang emits an inline SEH frame; the original uses compact thunks). This is expected and accepted for these CRT wrappers. New source files go in `src/halo/cseries/xbox_crt.c`; register the object as `XAPILIB:xbox_crt.obj` in `kb.json`.

- **MSVC-style `__asm` is broken under clang.** Our `-target i386-pc-win32` flag makes clang define `_MSC_VER`, so `#ifdef _MSC_VER` guards select MSVC-style `__asm {}` blocks. Unlike GCC-style `asm volatile`, MSVC-style `__asm` does **not** communicate register clobbers to the optimizer. If a GPR (EAX, EDX, ECX, etc.) holds a live value before the `__asm` block, and the asm instruction overwrites that register, the optimizer will reuse the stale value after the block — causing silent corruption or ACCESS_VIOLATION. **Rule:** Always guard MSVC-only asm with `#if defined(_MSC_VER) && !defined(__clang__)`, and provide a GCC-style `asm volatile` alternative in the `#else` branch with proper output constraints and clobber lists. **Known dangerous instructions:** `RDTSC` (clobbers EAX:EDX), `CPUID` (clobbers EAX/EBX/ECX/EDX), `MUL`/`DIV` (implicit EAX/EDX). FPU-only asm (`FLD`, `FMUL`, `FPATAN`, etc.) is safe since it doesn't touch GPRs.

- **Audit XCALL return and param types against kb.json.** XCALL raw function-pointer casts silently use the wrong register or push convention when the cast type doesn't match the real function signature. On x86: `float` return reads ST(0), `int`/`uint32_t` return reads EAX — wrong type = garbage from wrong register. `float` param is pushed via FLD+FSTP[ESP]; `int` param is pushed via PUSH — `(int)float_var` truncates before pushing. **Run `rtk python3 tools/audit/check_xcall_types.py` after adding or modifying any XCALL macro.** ERROR-level (float↔int) mismatches are blockers. Example: `real_a_rgb_color_to_pixel32` (0x99530) returns `uint32_t` in EAX; an XCALL cast `float(*)` read ST(0) garbage instead — the plasma-pistol overcharge orb was invisible for weeks.

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
- **Hazard Scan:** Run `rtk python3 tools/audit/check_lift_hazards.py` after source edits or when reviewing auto-lift output. Treat intrinsic calls, undersized buffers, suspicious duplicate arguments, pointer-as-float warnings, and CONCAT survival as blockers until investigated.  Checks and their lift-learnings sections: XCALL (§1), buffer-alias (§2), intrinsics (table), duplicate-args (§3), pointer-as-float (§4), frame-size (§2 stack), callee-output-size (projection §5), x87-math, void-EAX (§16), CONCAT (§13, ERROR), float-smuggling (§6), addr-value-add (§17), param-loop-corruption (§4), discarded-result (§8/§11). Use `--changed-only` to scan the union of staged, unstaged-tracked, and untracked files you have touched; use `--staged-only` (what the pre-commit hook uses) for staged files only. **WARN-level findings in files you touched are review items, not ignorable noise** — an fmod/FPREM1 warning in `hud_messaging.c` was ignored as pre-existing noise and shipped a visible HUD rendering bug (2026-06-10).
- **Learnings-must-ship-a-detector rule:** Every new `docs/lift-learnings.md` section **must ship, in the same commit**, either (a) a check wired into `check_lift_hazards.py` / `draft_decompiler.py` / `buffer_alias_detector.py` / the call-site audit, or (b) a one-line `Automation: not mechanically detectable because …` justification in the section.  A documented grep/regex counts as (a) only when it is actually implemented as a check.
- **XCALL Type Audit:** Run `rtk python3 tools/audit/check_xcall_types.py` after adding or modifying XCALL macros. ERROR-level (float↔int return/param) mismatches are blockers — they silently read the wrong register (ST0 vs EAX) or truncate float args.
- **Golden Master Test Harness:** A specialized test harness intercepts the engine boot in `src/halo/shell_xbox.c`. It lets you run functions inside the engine context and verify their side-effects/return values against the exact Xbox ASM output. 
  - *Usage:* Add tests to `src/halo/test_harness.c`. Ensure your function is unmapped in `kb.json` (`"ported": false`), run `rtk python3 tools/verify/run_golden_tests.py` to capture the original FPU hex values. Then map your function (`"ported": true`) and press Enter to verify your C implementation.
  - *Use cases:* FPU math functions, struct/object initializers, and complex isolated state transitions.
- **Live Memory Capture + State Replay:** For Unicorn equivalence that under-covers live engine paths, capture live game state and replay it into `unicorn_diff.py --state-snapshot <path>` (or `--from-halorec`). **Use the proven virtual-memory paths — never physical `pmemsave`:**
  - `tools/equivalence/memsave_snapshot.py` — reloc-driven virtual QMP `memsave`; validated byte-exact against `known_globals.json`. Flow: `plan --target <func> -o artifacts/plan_<func>.json` → `capture --plan artifacts/plan_<func>.json -o artifacts/snapshot_<func>.json` (`--follow` dereferences `*_ptr` regions; `--anchors` adds player/game-engine anchors). **Coverage caveat:** reloc-driven capture grabs only the *data* windows the target's relocations reference — NOT callee **code** pages. For a **non-leaf** target whose callees you do not stub, either run `unicorn_diff.py --allow-stubs` (stubs the callees — the usual path) or replay a `.halorec` frame that carries the code segments. (The old `dump_xemu_memory.py --full` mapped all non-zero pages incl. callee code, but only via the broken `pmemsave` default — on real hardware use `dump --method xbdm` + `snapshot --full`.)
  - `tools/equivalence/qmp_capture.py` — atomic `stop`→`memsave`→`cont` live capture that verifies the datum magic for you; the basis of the `.halorec` lineage (`capture_trajectory.py` → `hmrc.py` → `halorec_to_snapshot.py`), whose regions map 1:1 to `--from-halorec`.
  - **Do NOT use `tools/equivalence/dump_xemu_memory.py`'s default QMP `pmemsave` (physical) path.** Cerbios does not identity-map game VA on this dev box, so virt-to-phys reads the wrong bytes — the tool's own header documents every capture method as broken here (2026-06-07). Only its `--xbdm` getmem path is kept, and only for real hardware.

  Workflow:
  1. Get into the desired game state in xemu (e.g. MP match with players).
  2. **VERIFY EVERY CAPTURE before building a snapshot:** read a known global and confirm it is sane/non-zero — fwd-vector ptr @VA `0x31fc38` ≈ `0x0028xxxx`, and the object table (`*0x5a8d50` → `~0x80xxxxxx`) should contain datum magic `0x64407440`. If those read `0`, the capture is unusable — wrong method/context (e.g. a menu/idle pause instead of an **active-gameplay paused state** like a Flood encounter, where the paused CPU context has the game's user address space live). Fix the capture, do not proceed. `qmp_capture.py` performs this magic check itself.
  3. Build the snapshot with `memsave_snapshot.py plan`+`capture` (above), or reuse a `.halorec` frame.
  4. Run equivalence: `rtk python3 tools/equivalence/unicorn_diff.py <target> --seeds 50 --allow-stubs --mem-trace --state-snapshot artifacts/snapshot_<func>.json`.
  - Do not use QEMU `savevm`/`loadvm` for oracle tests — those restore old loaded-XBE code pages.
  - **xemu MCP daemon may be unreliable:** the `mcp__xemu__*` tools (xemu_connect/query_state/screenshot) can fail with `connection reset by peer` even when QMP is healthy (stale daemon attachment; QMP is single-client). Verify with a raw QMP probe (`socket → recv greeting → {"execute":"qmp_capabilities"}` → expect `return`); if raw works, **bypass the MCP**: use `tools/xbox/xbdm_screenshot.py --png` for screenshots and `dump_xemu_memory.py --xbdm` for memory. The MCP's launch path is also env-driven (`XEMU_PATH`); the real binary is `/mnt/g/dev/xemu/dist/xemu.exe` (WSL2 → xemu runs as a Windows `.exe`), and the HDD is opened `locked=on` so only one instance can run at a time.
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
- **VC71 Verify:** After lifting FPU-heavy functions (geometry, math, projections), run `rtk python3 tools/verify/vc71_verify.py src/path/to/file.c` to compile with Visual C++ 7.1 and compare against the delinked reference. Review any `[FPU-WARN]` (operand-order bugs), `[LOADW-WARN]` (int vs int16/int8 field-width bugs; `--loadw-only`), and `[IMM-WARN]` (wrong float/magic numeric literal; `--imm-only`) output. Requires a delinked reference in `delinked/` (export via `ghidra-live` MCP).
- **Delinked-reference precondition (REQUIRED before VC71 verify):** Before running `vc71_verify.py` for any newly lifted target, confirm the function's symbol exists in the delinked reference:
  ```
  objdump -t delinked/<object>.obj | grep -E "FUN_<addr_no_0x>|<funcname>"
  ```
  If the target's symbol is missing, you **MUST** export a new delinked reference via `mcp__ghidra-live__export_delinked_object` covering the correct address range (start at the earliest unported function in the TU, end past the last function) before running VC71 verify. Committing a lift without a delinked reference for the target is not allowed — there is no byte-match evidence without one.
- **Permuter (`/verify permute`):** Last-mile match optimizer. Use ONLY when VC71 match is in **[85, 98]%** with a mapped delinked reference. Below 85% the lift has a structural bug — fix it first; permuter cannot recover from real correctness issues. Above 98% it is not worth the cycles. Never accept a permutation that lowers the existing match; always re-run the lift pipeline against the new source.
- **Equivalence (`/verify equivalence`):** Unicorn-Engine behavioral differential with seeded inputs, coverage tracking, and concolic feedback. Use when byte-match is weak evidence: FPU-heavy code, hashes/serializers, or structurally capped lifts (e.g. SEH wrappers stuck at ~55%). Works for both leaf and non-leaf functions:
  - **Pure leaves:** Run `rtk python3 tools/equivalence/unicorn_diff.py <target> --seeds 100`
  - **Non-leaf or FPU-heavy:** Use `rtk python3 tools/equivalence/unicorn_diff.py <target> --seeds 100 --allow-stubs --float-tolerance 32`. `--allow-stubs` stubs known callees (csmemcpy, fabs, _chkstk, etc.) and seeds known XBE globals. `--float-tolerance N` compares float* scratch buffers with N ULP tolerance instead of byte-exact (recommended: 16–32 for typical geometry, up to 256 for long chains), accounting for x87 rounding differences across compiler versions. With `--allow-stubs`, a stub-argument differential records each stubbed callee's register and stack args in oracle and candidate and fails seeds on mismatch, catching swapped, dropped, or wrong-constant call arguments that return-value and mem-trace comparison miss; disable with `--no-stub-arg-trace`.
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
- Need live-state equivalence: capture with `rtk python3 tools/equivalence/memsave_snapshot.py plan`+`capture` (virtual memsave), or reuse a `.halorec` frame, then run `/verify equivalence <target> --state-snapshot <path>`. On **real hardware only**, `dump_xemu_memory.py --xbdm` (getmem) is the fallback — never its physical `pmemsave` default.
- Need runtime oracle coverage: `/verify golden <target>` or `/verify dual-oracle <target>` when a same-process harness case exists.
- Need real Xbox probing: `/deploy --xbe-only`, then `/xbdm <mode>`.
- Need xemu build/load: `/build` or `/xemu build-load`.
- Need regression investigation: `/debug-regression <symptom>`.
- Need to replay an existing fixture (quick path — no flags to remember): `/replay-input` — picks level/scenario/build interactively, fires `capture_scenario.py replay`.
- Need deterministic input testing (capture gameplay, replay the exact same input repeatedly, or diff patched vs unpatched build on identical input): `rtk python3 tools/xbox/capture_scenario.py replay --level <lvl> --scenario <name> [--xbe cachebeta.xbe|default.xbe]` — skill `input-replay-testing`, doc `docs/input-fixture-capture.md`.
- Need an A/B regression test (does the patched build behave like the original on the same input): `rtk python3 tools/equivalence/ab_check.py --level <lvl> --scenario <name>` — one command: **builds+deploys your candidate and gates build-liveness first** (`verify_toggles_live --all-off`, so it tests your local build not a stale on-box one; `--no-deploy` to skip), then replays the fixture on cachebeta (golden) + default (candidate), captures both trajectories, runs the tolerant `behavior_diff`, and prints the localized divergence (exit 0 clean / 3 divergent / 2 inconclusive = deploy or gate failed). Add `--golden <frozen>.halorec` to reuse a frozen faithful golden (CI tripwire), `--aa-first` to self-check determinism. Skill `ab-trajectory-testing`, doc `docs/ab-trajectory-testing.md`.

## Analysis Tools
- **`tools/xbox/capture_scenario.py`** — Deterministic controller-input record/replay test fixtures (per-level, host-only). `record`/`arm`/`finalize`/`replay`/`selftest`; `--xbe` selects and verifies the build (`cachebeta.xbe`=unpatched faithful, `default.xbe`=patched). Capture gameplay once, replay identical input on demand or in a loop; diff builds on the same input. Skill `input-replay-testing`; doc `docs/input-fixture-capture.md`.
- **`tools/equivalence/qmp_capture.py`** — Atomic live-state capture engine for A/B trajectory testing. Wraps raw QMP (`:4444`) `stop`→`memsave`(VIRTUAL — never `pmemsave`, which is broken on this Cerbios/kernel-irqchip=off box)→`cont` so one frame is read coherently; resolves the data_t pools (objects/players/actors/props), reads the game tick (`*0x45708C+0xC`), and verifies the objtable magic (`0x64407440`) before trusting a capture. Never touch the gdbstub (`:1234` halts the CPU). Doc `docs/ab-trajectory-testing.md`.
- **`tools/equivalence/hmrc.py`** — `.halorec`/HMRC writer. Encodes captured frames into the halo-memory-viewer format (gzip; round-trips with `halorec_to_snapshot.py`), so trajectories are both Python-diffable and viewer-playable. Output is literal game memory — host-only, gitignored, never commit.
- **`tools/xbox/capture_trajectory.py`** — Captures a game-state trajectory to a `.halorec` while a deterministic fixture replays. Waits for gameplay, anchors on that engine event, captures the full pool set every K **relative** game ticks (fixed bucket grid, so two runs of the same fixture align). Run it alongside `capture_scenario.py replay`. Skill `ab-trajectory-testing`.
- **`tools/equivalence/trajectory_diff.py`** — STRICT byte-diff of two `.halorec` trajectories at the exact tick. This is the **A/A determinism check only** (masks the known sub-frame phase counters actor+0x4a/+0x7c, prop+0x26); it is the wrong tool for A/B because it flags benign x87/phase drift. Exit 0 clean / 3 divergent.
- **`tools/equivalence/behavior_diff.py`** — TOLERANT behavioral diff = the **A/B regression oracle**. Aligns by nearest tick within ±W, matches entities by datum SLOT (salts differ across builds), reports a discrete-field divergence only when SUSTAINED (≥`min_run` samples), compares positions at a value eps and handles by LIVENESS, and checks aggregate invariants. The earliest sustained onset names the time+field+entity to investigate. Built-in watch-list targets a10 AI; override with `--config`. Exit 0 clean / 3 divergent.
- **`tools/equivalence/aa_check.py`** — Orchestrates the A/A determinism check: replays one fixture twice on the same build (cachebeta) and runs `trajectory_diff`. Must be CLEAN before trusting any A/B result. Skill `ab-trajectory-testing`.
- **`tools/equivalence/ab_check.py`** — The one-command A/B regression check (primary entry point). By default **builds+deploys the candidate (`build_deploy_run.sh -q`, which always re-patches via the `patched_xbe` ALL target) and gates build-liveness (`verify_toggles_live --all-off`) before capturing** — so it tests your local build, not a stale on-box one; either step failing → exit 2 INCONCLUSIVE, no diff. Then replays the fixture on cachebeta (golden) + default (candidate), captures both, runs `behavior_diff`, prints the localized divergence + verdict (exit 0 clean / 3 divergent). `--no-deploy` skips the rebuild (verdict flagged UNVERIFIED unless `--verify-live`); `--golden <path>` reuses/`--freeze` writes a frozen faithful golden (CI tripwire — capture cachebeta once, reuse); `--aa-first` self-checks determinism before the A/B; `--reuse` diffs existing captures. Skill `ab-trajectory-testing`.
- **`tools/analysis/frontier.py`** — Decompilation frontier scoring and target recommendations.
- **`tools/analysis/fun_pipeline.py`** — FUN_ function naming pipeline. Four stages: `reclassify` moves `<common>` FUN_ functions to named objects (feeds frontier.py accuracy); `prioritize` tiers remaining FUN_ functions by evidence quality (P0 = attributed + signature, P3 = unclassified); `propose` outputs a Ghidra work queue for decompile-based naming; `apply` writes proposed names back to kb.json. Run `status` first to gauge naming debt. Prerequisite for accurate frontier scoring when `<common>` is large.
- **`tools/llm_auto_lift.py`** — Target selection, liftability scoring, and Ghidra context caching. Use `select` for combined frontier/liftability target choice; `cache-context` to pre-cache Ghidra output; code generation delegated to `/lift`.
- **`tools/analysis/classify_common.py`** — Analyze `<common>` functions for reclassification into proper objects. Uses delinker exports and XBE `__FILE__` strings as evidence. Run with `--delinker-analyze` for full binary-evidence analysis (requires Ghidra), or `--summary` for a quick static overview.
- **`tools/audit/batch_delink.py`** — Batch-export delinked reference objects for all kb.json objects.
- **`tools/audit/check_lift_hazards.py`** — Build-time hazard scan for common Ghidra/MSVC lifting pitfalls.
- **`tools/lift_pipeline.py`** — Primary lift validation orchestrator for build, ABI audit, VC71 verify, behavior/runtime checks, and low-match policy.
- **`tools/analysis/maintain.py`** — Source file organization and function placement checks.
- **`tools/verify/objdiff_lift.py`** — Structural object diff helper used by the pipeline when reference and candidate objects are available.
- **`tools/verify/vc71_verify.py`** — Compile a source file with Visual C++ 7.1 (`RXDK/xbox/bin/vc71/CL.Exe`) and compare against the delinked reference. Flags FPU operand-order differences (`--fpu-only`), narrow-field load-width mismatches (`--loadw-only`, int vs int16/int8), and wrong float/magic numeric literals (`--imm-only`, `[IMM-WARN]` — a large inline constant differing between our lift and the original; since both sides are VC71 codegen this is a source-literal mismatch that the LCS % aligns away). SQLite-backed cache (`tools/verify/vc71_cache.py`) skips compile+diff on unchanged inputs; use `--no-cache` to force recompute or `tools/verify/cache_stats.py` to inspect hit rate.
- **`tools/verify/compare_obj.py`** — LCS-based instruction comparison between two COFF objects. Used by `vc71_verify.py`. Also hosts the presence-census detectors that the aggregate LCS % hides: `compare_fpu_blocks` (`[FPU-WARN]`), `compare_load_widths` (`[LOADW-WARN]`, §24), and `compare_immediates` (`[IMM-WARN]`, §25 — large inline constants present on exactly one side). Self-test: `--self-test`.
- **`tools/lift/draft_decompiler.py`** — Stage 1+2+4 Ghidra pseudocode rewriter. Run as a pre-pass on raw Ghidra output before lifting: canonicalizes synthetic types/ops (undefined4, CONCAT44, ZEXT/SEXT, etc.), rewrites MSVC intrinsics per the table above, and wraps `__SEH_prolog/epilog`-bracketed bodies in native `__try/__except`. Each rewrite emits an `/* AUTO: ... */` annotation.
- **`tools/lift/buffer_alias_detector.py`** — Pre-pass detector for hazard #5 (buffer-alias confusion). Annotates every suspect `local_YY` read with a HIGH-RISK or verify comment based on whether a qualifying CALL preceded it. Exits 1 on HIGH-RISK hits for pipeline gating.
- **`tools/audit/extract_reg_args.py`** — kb.json ↔ kb_reg_baseline.json drift detector for `@<reg>` annotations. Run `--check` after kb.json edits. Both `pre-commit-reg-baseline-drift.sh` and `generate_lift_commit.py` hard-fail on detected drift (use `--apply` to merge missing entries; `--no-verify` / `--skip-abi-audit` to bypass).
- **`tools/audit/check_arg_counts.py`** — cross-checks kb.json decl stack-arg counts against `ADD ESP,N` cleanup at original call sites in the pristine XBE (capstone sweep; ~5s). `--self-test` validates internal state; `--callee 0xADDR` checks a single callee; `--check` exits non-zero on HIGH findings. Report written to `artifacts/audit/arg_count_report.txt`. NOTE: as of 2026-06-10 this file no longer parses cached Ghidra disasm — it sweeps the raw XBE directly.
- **`tools/audit/dump_caller_regsetup.py`** — dumps every original CALL site of a callee with last-writer register loads and PUSH sequence; the mechanized §10 caller audit. When fixing a §10 argument-swap on one caller, run this tool and audit ALL callers of that callee before committing.
- **`tools/permuter/run.py`** — decomp-permuter wrapper for VC71/x86/MSVC. Last-mile match optimizer for functions in the 85–98% VC71 band. Random AST permutations + LCS scoring; reuse `tools/permuter/compile.sh` and `score.py` adapters. Upstream lives at `third_party/decomp-permuter/` (gitignored; clone per `docs/permuter-adapter.md`). **Always pass `-q`** to suppress diagnostic noise and the permuter's own progress stream — only the final summary and NEW BEST candidates are printed in that mode.
- **`tools/equivalence/unicorn_diff.py`** — Unicorn-Engine differential tester. Runs MSVC-delinked oracle and clang-built candidate `.obj` in two emulators with seeded inputs; compares CPU/FPU state at RET. Three-phase testing: Phase 1 (random/corner seeds), automatic concolic Phase 2 (injects non-zero globals to reach untested branches when coverage < 60%), and memory-trace differential (compares all non-stack writes). Reports coverage %, confidence tier (high/moderate/weak), and persists both to `leaf_cache.json`. Key flags: `--allow-stubs` (non-leaf), `--float-tolerance N` (FPU), `--mem-trace` (write comparison), `--state-snapshot PATH` (inject real game state), `--no-concolic` (disable Phase 2). With `--allow-stubs`, a stub-argument differential records each stubbed callee's register and stack args in both oracle and candidate and fails seeds where they diverge — catching swapped, dropped, or wrong-constant call arguments that return-value and mem-trace comparison miss; disable with `--no-stub-arg-trace`.
- **`tools/equivalence/concolic.py`** — Coverage-guided concolic feedback for unicorn_diff. Disassembles oracle code, finds untaken branches, determines what memory values would reach them, and generates injection overrides. Called automatically when Phase 1 coverage < 60%.
- **`tools/equivalence/state_snapshot.py`** — State snapshot capture and replay. `load_snapshot(path)` loads JSON into memory overrides for Unicorn. Low-level building block; prefer `memsave_snapshot.py` (virtual memsave) for capture workflows.
- **`tools/equivalence/dump_xemu_memory.py`** — LEGACY full-memory dump/snapshot builder. Its default `dump` path uses QMP `pmemsave` (physical), which is **broken on this dev box** (Cerbios does not identity-map game VA → reads wrong bytes; see the tool's own header). Prefer `memsave_snapshot.py`/`qmp_capture.py` (virtual memsave) for capture. The one kept use is `dump --method xbdm` (`getmem`, virtual) on **real Xbox hardware**. Given a verified dump, `snapshot --dump <path> --target <func> [--full] [--arg <name> <value>]` still assembles a `unicorn_diff.py --state-snapshot`-compatible JSON.
- **`tools/analysis/punpckhdq_import.py`** — Imports PDB-derived TU/symbol corpus from punpckhdq/halo (debug-build PDB) and proposes real names for our `FUN_<addr>` placeholders. `tools/analysis/apply_punpckhdq_renames.py` performs textual renames across kb.json/baseline/src after dry-run review.

## Architecture and Skills

**Skills are agent doctrine the assistant self-invokes — not a menu the user
picks from.** `tools/memory/skill_router_hook.py` surfaces the relevant skill(s)
from each `SKILL.md`'s `triggers` every prompt; treat those `[skill-router]`
picks — and trigger words (`crash`, `page fault`, `@<reg>`, `ADD ESP`, `_chkstk`,
`VC71`, `low match`, `permuter`, `wrong color`, `trajectory`, `xemu`) — as an
instruction to load and apply the named skill before acting. Before any lift,
score-recovery, call-site, hazard, or crash/regression work, apply the matching
skill via the Skill tool when surfaced, else read `.claude/skills/<skill>/SKILL.md`
and follow its checklist (`lift-*` are often unsurfaced — not a reason to skip).
Name the skill(s) in any subagent brief.

Full two-tier catalogue (~4 user `/<name>` commands vs ~22 auto-applied doctrine
skills, with triggers): `.claude/skills/SKILLS.md`, generated from frontmatter by
`tools/memory/gen_skills_index.py` (on-demand — not loaded each session). To add
or retier a skill: edit its `SKILL.md` frontmatter (`tier:` + `triggers:`), then
re-run `migrate_skill_frontmatter.py` + `gen_skills_index.py`.

<!-- rtk-instructions v2 -->
# RTK (Rust Token Killer) - Token-Optimized Commands

## Golden Rule

**Always prefix commands with `rtk`**. Even in command chains with `&&`:
`rtk git add . && rtk git commit -m "msg" && rtk git push`

## RTK Commands by Workflow

| Category   | Typical Savings | Examples                                        |
|------------|-----------------|-------------------------------------------------|
| **Build**  | 70-90%          | `rtk cmake`, `rtk tsc`, `rtk lint`              |
| **Test**   | 90-99%          | `rtk pytest`, `rtk cargo test`, `rtk vitest`    |
| **Git**    | 60-80%          | `rtk git status`, `rtk git diff`, `rtk git log` |
| **Files**  | 60-75%          | `rtk read`, `rtk grep`, `rtk find`, `rtk ls`    |
| **Ghidra** | 70-90%          | `rtk python3 tools/audit/check_ghidra_mcp.py`   |

Use `rtk gain` to view savings statistics.
<!-- /rtk-instructions -->
