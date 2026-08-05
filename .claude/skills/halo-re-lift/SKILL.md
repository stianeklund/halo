---
name: halo-re-lift
tier: agent
triggers: ["lift", "lifting", "re-lift", "relift", "abi", "kb.json", "@<reg>", "port function"]
description: "Lift, port, re-lift, FUN_, Ghidra decompile, kb.json, @<reg>, ABI, source_path: repo-specific Halo CE Xbox function lifting workflow from binary evidence through C implementation and verification."
---

# Halo RE Lift

Use this skill for the operational workflow of lifting a function from
cachebeta.xbe or default.xbe. Doctrine and evidence rules live in
`halo-xbox-re`; this skill covers the lift sequence and ABI specifics.

## Worktree context (CRITICAL — read first)

At the start of every lift session, determine the actual repo root:

```bash
rtk git rev-parse --show-toplevel
```

All file edits, `rtk git` commands, and tool invocations must target **that path**, not a hardcoded `/mnt/g/dev/halo`. When running as a subagent spawned from a worktree, the "Primary working directory" in your system prompt tells you where you are — trust it over any hardcoded path. Never commit or stage changes to a path outside your working directory.

## When to use

- Lifting a function from the XBE
- Reviewing a proposed lift for ABI or layout fidelity
- Resolving prototypes, struct fields, globals, or `@<reg>` thunks
- Updating kb.json after binary-backed analysis

## Lift workflow

1. **Pick a frontier target:** Select an un-implemented function that is *called by* an already-implemented function.
2. Resolve target by name or address in `kb.json` and Ghidra (or Python capstone x86 32-bit for quick disassembly).
3. **Gather context & recover literals:** Collect callers, callees, touched globals, strings, imports, existing declarations in source and `kb.json`. Recover string and constant literals pushed by address from `cachebeta.xbe` (e.g., `name` argument to `game_state_malloc`).
4. Cross-check decompilation against raw disassembly (see `halo-xbox-re`
   review checklist; see `docs/references/abi-and-calling-conventions.md`
   for full ABI rules). **Mandatory call-site verification:** for every CALL in
   the disassembly, trace each PUSH backward and confirm the decompiler mapped
   it to the correct variable. Watch for:
   - Register aliasing: EBX/ESI/EDI set far from the call site
   - Push-then-fstp: `PUSH <dummy>; FSTP [ESP]` replaces arg with float
   - Struct field rotation: MSVC interleaved stores do not imply decompiler offsets
   - **→ Use `lift-decompiler-traps` skill** for full guidance on these + cross-product swap, buffer-alias confusion, and MSVC intrinsics
5. Infer the narrowest defensible prototype (see
   `docs/references/prototype-inference.md`).
6. **Pre-implementation pattern check** — before writing C, scan for crash classes
   `check_lift_hazards.py` does NOT flag (full detail in `docs/lift-learnings.md`):
   - XCALLs to targets being ported: `grep -oP 'XCALL\(0x\K[0-9a-f]+' src/<file>.c`
   - `&local_XX` args to callees that index `param[N]` (stack aliasing → must be contiguous buffer)
   - Loops that advance a parameter pointer when original uses a copy register post-loop
   - After writing C: `grep -n '(float)(int)' src/<file>.c` (float-as-pointer smuggling)
   - **→ Use `lift-decompiler-traps` skill** for ADD ESP mismatch suspicion, 0-arg getter patterns, @<reg> order questions, `_chkstk` frame sizing, stack aliasing, or `&local_XX` passed to an indexing callee
7. Produce a structurally faithful C lift:
   - preserve control-flow shape
   - preserve side-effect order
   - preserve pointer arithmetic and odd logic unless disproven
   - Asserts: `assert_halt(cond)`
   - Compiler gotchas: Flags are `-Wall -Werror -target i386-pc-win32 -march=pentium3 -nostdlib -ffreestanding -fno-builtin -fno-exceptions -include src/common.h`. Note that `-Wall` does NOT include `-Wunused-parameter`. Non-void functions MUST return a value or `-Werror` breaks the build. Explicitly cast pointer <-> int assignments (e.g. `dword_50548c = (int)game_state_malloc(...)`).
8. Write implementation in address-ordered position.
9. **Verify `src/CMakeLists.txt` registration (CRITICAL):** Confirm the `.c` file containing your lift is listed in `src/CMakeLists.txt`. Unregistered files silently fail to compile and patch redirects!
10. Update kb.json conservatively (see
    `docs/references/kb-update-policy.md`). Declare every global the function touches
    in the owning object's `data` list (or `<common>` if unattributed) — narrowest
    proven type, real name where evidence supports one; see that doc's **Data global
    declarations** section for the accepted shapes.
11. Run `rtk python3 tools/analysis/maintain.py <source_file>`.
12. Build and verify instruction stream: `llvm-objdump -dr --disassemble-symbols=_<fn> <obj>` to compare the compiled instruction stream against original binary disassembly.
13. Run `rtk python3 tools/audit/check_lift_hazards.py` and fix any target-relevant hazards.
    - **→ Use `lift-silent-bugs` skill** before deploying to Xbox — catches float-as-pointer, accumulator misread, builder-count ignored, void-EAX, address-offset bugs that `check_lift_hazards.py` does NOT detect
14. **Post-verify score routing:**
    - Any score below 100% → first check `artifacts/score_context/<func_name>.json`
      (`rtk jq '{scores, frame, classification}' ...`). `vc71_verify.py` writes this
      pack on every scored run with pre-classified `classification[]` entries
      (`rule`/`evidence`/`action`) plus frame-size and warning fields — read it
      before manually re-deriving the same diagnosis from `--show-diffs`.
    - Before any source-level score-recovery experiment, run
      `score_improve.py baseline` for the source file. Apply one evidence-backed
      lever, then run `score_improve.py check` for the target; retain the edit only
      on PASS. `lift-score-improve` supplies the exact commands and categories.
    - Score 65–84% and gap described as "structural" → **invoke `lift-score-improve` skill first** before reverting or escalating
    - Xbox crash / hang / ACCESS_VIOLATION → **invoke `lift-crash-signals` skill**
    - Wrong visual output / silent wrong behavior → **invoke `lift-crash-signals` skill** (toggle-bisect section)

## Ghidra MCP availability (required)

- Before the first `ghidra` or `ghidra-live` MCP tool call in a task, run
  `rtk python3 tools/audit/check_ghidra_mcp.py`.
- If the preflight fails, or if any `ghidra`/`ghidra-live` MCP tool call fails
  due to connection/timeout/unavailable errors, stop immediately and do not
  retry in the same response.
- Tell the user exactly: `You might have forgotten to start
  tools/mcp-servers.sh or ghidra may not be running?`

## Token-efficient execution defaults

Use these defaults unless a target requires deeper forensics:

- Prefer existing repo tools over ad-hoc scripts:
  - `rtk python3 tools/analysis/kb_meta.py list --object <obj>` for scoped symbol sets
  - `rtk python3 tools/lift_pipeline.py --target <name_or_addr> ...` for staged lift/verify
  - `rtk python3 tools/llm_auto_lift.py select --limit 20` for combined frontier/liftability target selection
- Avoid inline `python3 -c` snippets for kb queries and address matching.
- Keep MCP passes staged:
  1. Resolve target (`get_function_by_address`).
  2. Pull pseudocode (`decompile_function` or `batch_decompile` for >1 function).
  3. Pull callers/callees (`get_function_callers`/`get_function_callees`) with bounded limits.
  4. Pull full disassembly only when decompiler output is ambiguous or ABI-critical.
- Prefer one target per run; do not batch unrelated functions in one analysis pass.
- In reports, summarize evidence and include only the minimum assembly needed to justify claims.

## Auto-lift candidates

- `tools/llm_auto_lift.py` is an untrusted candidate generator and validation runner.
- Use `select` as the normal entrypoint; it combines frontier priority with automation safety.
- It may produce `auto_accept`, `needs_review`, or `reject` artifacts, but it must not commit.
- Run `promote` as a dry run first and ask the user before `promote --apply`.
- Do not use generated code without the same binary evidence, hazard checks, and validation gates as a manual lift.

## ABI cautions

Key reminders (full rules in `docs/references/abi-and-calling-conventions.md`):

- cdecl: the first PUSH is the last C argument.
- For `@<reg>` or reverse-thunked paths, audit which registers the original
  caller expects preserved.
- Lifted C may legitimately clobber caller-saved EAX, ECX, EDX.
- `@<reg>` annotations are immutable. Never remove or change slot assignments.
- When calling an original XBE function that takes register args, add it to
  kb.json with `@<reg>` and call by name. Do not use raw casts or inline asm.
  New `@<reg>` entries must also be added to `tools/kb_reg_baseline.json`.
- Do not reorder or repack structs without binary evidence and matching asserts.

## Commit discipline

- Do not write freeform lift commit messages.
- Generate the message into a **`mktemp` path**, review it, then commit:
  ```bash
  MSG=$(mktemp /tmp/halo-commit-msg.XXXXXX)
  rtk python3 tools/audit/generate_lift_commit.py --batch-name "<short description>" > "$MSG"
  rtk git commit -F "$MSG" && rm -f "$MSG"
  ```
- **Never a fixed path such as `/tmp/commit_msg.txt`.** Every concurrent agent, cron
  job, and worktree on this box follows the same recipe. A second writer landing
  between your write and your `git commit -F` silently commits YOUR staged changes
  under THEIR message — no hook catches it and the commit looks legitimate. Observed
  2026-07-31: commit `d6caee6b` landed a `game_engine.c` fix titled "Port
  draw_string_get_string (draw_string.obj)".

## Output expectations

Follow the output format from `halo-xbox-re`
(see `docs/references/output-schema.md` for full detail).
