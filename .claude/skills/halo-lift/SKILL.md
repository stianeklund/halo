---
name: halo-lift
tier: agent
triggers: ["lift", "lifting", "re-lift", "relift", "ported", "porting", "ghidra", "decompile", "decompil", "cachebeta", "kb.json", "binary evidence", "@<reg>", "reverse engineer", "abi", "port function"]
description: >-
  Halo CE Xbox reverse engineering doctrine and function lifting workflow — from
  binary evidence through C implementation and verification. Covers RE methodology,
  evidence policy, ABI rules, Ghidra MCP usage, lift sequence, token-efficient
  defaults, and commit discipline. The single skill for any RE/lift analysis.
---

# Halo RE & Lift

Use this skill for any work involving Halo CE Xbox reverse engineering, binary
analysis, or function lifting. Operational workflows for verification live in
`halo-verify-debug`; build/deploy in `halo-build-xemu` and `halo-xbdm`.

---

## Target Binary Contract

The binary is the source of truth. The target is **Halo Xbox debug build 2276**
(`halo-patched/cachebeta.xbe`, MD5 `c7869590a1c64ad034e49a5ee0c02465`, version
`01.10.12.2276`, Oct 12, 2001). It is a **debug build** with richer symbols/asserts;
do not swap in a retail `default.xbe`. All `kb.json` VAs are absolute virtual
addresses into THIS file.

## Ground Rules

- Unknown is better than wrong.
- Inspect both decompilation and disassembly before concluding.
- **Confirm Struct Offsets in `src/types.h`:** NEVER guess a struct field.
- **Unknown-field naming:**

  | Name | Means |
  |------|-------|
  | `field_<hex>` | offset IS accessed, meaning unproven |
  | `pad_<hex>[n]` | never observed accessed (a read is a recovery bug) |

  Lowercase hex, no `0x`, 2 digits min. `unk_<hex>` is legacy — convert when
  the struct is already being edited, not as a standalone campaign.
- Preserve ABI, stack behavior, field offsets, packing, side-effect order.
- Do not add empty stubs.
- Reuse existing project and Xbox types before inventing new ones.

## Evidence Policy

Every claim must carry a label:
- **Confirmed** — binary-backed (disassembly, callsites, register behavior, operand widths, strings)
- **Inferred** — best narrow interpretation with supporting evidence
- **Uncertain** — unresolved possibilities, conflicts, or weak guesses

## Efficiency Guardrails

- Prefer bounded, evidence-first pulls over broad dumps.
- One strong artifact per claim, not redundant copies of all three views.
- Full-function disassembly only when needed to resolve uncertainty.
- Batch APIs for multiple related functions.

---

## Lift Workflow

### Worktree context (CRITICAL)

At the start of every lift session:
```bash
rtk git rev-parse --show-toplevel
```
All edits target **that path**, not a hardcoded checkout path.

### Sequence

1. **Pick a frontier target.** Select an un-implemented function called by an
   already-implemented function.
2. Resolve target by name or address in `kb.json` and Ghidra.
3. **Gather target context & recover literals.** Callers, callees, globals,
   strings, imports, and existing declarations. Recover string/constant
   literals pushed by address from `cachebeta.xbe`. Do not use external or cross-build material (other
   builds, PDB-derived corpora, or their bodies) as source-shape, naming,
   prototype, or layout evidence. Historical external-source notes may identify
   a claim that needs rechecking, but the claim remains unknown until 2276
   evidence independently proves it. Follow `PROVENANCE.md` and
   `naming-confidence`.
4. **Cross-check decompilation against raw disassembly.** Mandatory call-site
   verification: for every CALL, trace each PUSH backward. Watch for register
   aliasing, push-then-fstp, struct field rotation. Use `lift-decompiler-traps`
   for the full hazard checklist.
5. **Infer the narrowest defensible prototype, TYPES INCLUDED** (see
   `docs/references/prototype-inference.md`). The prototype is an *input* to
   the lift, not a cleanup of it: a `float` param declared `int` compiles to a
   `PUSH` where the original does `FSTP`, and the VC71 official score is a
   mnemonic-only LCS that will not notice. Read the type off the call sites:
   - a param slot filled by `fstp dword ptr [esp+K]` is `float`; `qword` is
     `double` (K/4 plus the pushes between that store and the CALL gives the
     slot index)
   - `ADD ESP,N` after the CALL gives the stack slot count
   - callers doing `test al,al` / `movzx r,al` want an 8-bit return; `test
     eax,eax` a 32-bit one (see `reference_callback_return_width_must_match_test_insn`)
   - callers consuming ST(0) with no intervening `fld` mean a `float` return

   `rtk python3 tools/audit/check_param_types.py --callee 0x<addr>` does all
   four mechanically against the pristine XBE. Run it for the target AND for
   any callee whose decl you are about to rely on. Record what it confirmed
   under **Types recovered** and what you assumed under **Types assumed**.
6. **Pre-implementation pattern check** — scan for crash classes
   `check_lift_hazards.py` does NOT flag:
   - XCALLs to targets being ported
   - `&local_XX` args to callees that index `param[N]` (stack aliasing)
   - Loops advancing a parameter pointer when original uses a copy register
   - `(float)(int)` float-as-pointer smuggling
7. **Produce structurally faithful C lift:**
   - Preserve control-flow shape and side-effect order
   - **Use the struct field where a struct exists for that base.** Do not ship
     `*(int *)(p + 0x1b8)` when `p`'s type has a field there —
     `&g->players[i]` and `(char *)g + i*0x40 + 0x10` compile identically, so
     the recovered spelling is free. If no struct exists for the base and the
     function touches 3+ distinct offsets off it, define or extend one now
     (`type-recovery`), with `field_<hex>` for offsets you see accessed and
     `pad_<hex>[n]` for the gaps. Partial is fine and expected: it grows as more
     of the object is lifted.
   - **Type the producer, not the site.** A raw offset is usually not a missing
     field — it is a producer whose kb.json return decl is `void *`/`char *`.
     `rtk python3 tools/audit/check_readability.py --untyped-producer` ranks
     them; 7042 deref sites trace to 36 producers. Typing one decl types every
     caller and is codegen-neutral (a pointer return is EAX either way). The
     pre-commit hook blocks a NEWLY-ADDED deref on a known producer.
   - Generic accessors (`datum_get`, `tag_get`) genuinely return `void *` —
     the type depends on the pool. Those need a typed wrapper, not a changed
     decl; leave them and say so rather than inventing a type.
   - **Write in Bungie code style** (binding: `lift-implementation.md` →
     *Bungie Code Style*). This covers: `lower_snake` names, `k_` constants,
     `_enum_member` / `_x_bit` enums, `MAXIMUM_`/`NUMBER_OF_` bounds,
     `*_index` handles, `NONE`, `TEST_FLAG`, cseries types, typed `<element>_get`
     datum macros over kb-named pools, and `DATUM_INDEX_TO_*`. Style changes
     spelling only. If a style rule would change bytes or call count, keep the
     original codegen.
   - Preserve engine idioms: `real`/`boolean` types, `cseries` macros, typed
     tag/object accessors, named enum switch cases (see `halo-xbox-re`)
   - Asserts: `assert_halt(cond)`
   - Compiler: `-Wall -Werror -target i386-pc-win32 -march=pentium3`
   - Non-void functions MUST return a value. Cast pointer↔int explicitly.
8. Write implementation in address-ordered position.
9. **Verify `src/CMakeLists.txt` registration (CRITICAL).** Unregistered files
   silently fail to compile!
10. Update kb.json conservatively (see `docs/references/kb-update-policy.md`).
11. Run `rtk python3 tools/analysis/maintain.py <source_file>`.
12. Build and verify: `llvm-objdump -dr --disassemble-symbols=_<fn> <obj>`.
13. Run `rtk python3 tools/audit/check_lift_hazards.py` — fix target-relevant hazards.
    Use `lift-silent-bugs` before deploying to Xbox.
13b. Run `rtk python3 tools/audit/check_param_types.py --check`. A new ERROR
    means a decl you touched contradicts the call sites — a silent truncation
    bug VC71 cannot see. Fix the decl; only record it with `--update-baseline`
    if you can say why the disassembly evidence is wrong.
14. **Post-verify score routing:**
    - Check `artifacts/score_context/<func>.json` first
    - Score 65–84% and "structural" → `lift-score-improve` skill
    - Crash/hang → `crash-debug` skill
    - Wrong visual output → `lift-silent-bugs` + toggle-bisect in `crash-debug`

## Ghidra MCP Availability

Before the first `ghidra`/`ghidra-live` MCP tool call, run:
```bash
rtk python3 tools/audit/check_ghidra_mcp.py
```
If it fails, stop and tell the user.

## Token-Efficient Defaults

- `rtk python3 tools/analysis/kb_meta.py list --object <obj>` for scoped symbols
- `rtk python3 tools/lift/lift_pipeline.py --target <name_or_addr> ...` for staged verify
- `rtk python3 tools/lift/llm_auto_lift.py select --limit 20` for target selection
- Keep MCP passes staged: resolve → decompile → callers/callees → disassembly only if needed
- One target per run; summarize evidence minimally

## ABI Cautions

- cdecl: first PUSH is the last C argument
- `@<reg>` annotations are immutable — never remove or change slot assignments
- Register-arg callees must be added to kb.json with `@<reg>` and called by name
- Do not use raw casts or inline asm for register-arg calls
- New `@<reg>` entries must also be in `tools/kb_reg_baseline.json`

## Commit Discipline

Generate the message into a **`mktemp` path**:
```bash
MSG=$(mktemp /tmp/halo-commit-msg.XXXXXX)
rtk python3 tools/audit/generate_lift_commit.py --batch-name "<short description>" > "$MSG"
rtk git commit -F "$MSG" && rm -f "$MSG"
```
**Never a fixed path** — concurrent agents clobber shared paths.

## Review Checklist

1. Resolve target in kb.json and Ghidra
2. Gather context: callers, callees, globals, strings, imports
3. Cross-check decompilation against disassembly
4. Infer narrowest defensible prototype
5. Produce structurally faithful C
6. Write in address-ordered position
7. Update kb.json conservatively
8. Run `maintain.py`, build, verify, hazard scan

## Output Format

Report under: Target, Scope, Confirmed, Inferred, Uncertain, Evidence,
Proposed code, Proposed kb deltas, Validation, Open questions.
(See `docs/references/output-schema.md` for detail.)

## Detailed References

| Concern | Reference |
|---|---|
| ABI, calling conventions | `docs/references/abi-and-calling-conventions.md` |
| Prototype inference | `docs/references/prototype-inference.md` |
| kb.json update rules | `docs/references/kb-update-policy.md` |
| Output schema | `docs/references/output-schema.md` |

## Commit Message File Safety (moved from AGENTS.md, 2026-09-02)

The standard recipe is:

```bash
MSG=$(mktemp /tmp/halo-commit-msg.XXXXXX)
rtk python3 tools/audit/generate_lift_commit.py --batch-name "<short description>" > "$MSG"
rtk git commit -F "$MSG" && rm -f "$MSG"
```

**Never use a fixed path such as `/tmp/commit_msg.txt`.** It is shared by every
concurrent agent, cron job, and worktree on the box, and they all follow this
same recipe. A second actor overwriting the file between your write and your
`git commit -F` silently commits YOUR staged changes under THEIR message — no
hook catches it, and the commit looks legitimate. Observed 2026-07-31: commit
d6caee6b landed a `game_engine.c` fix titled "Port draw_string_get_string
(draw_string.obj)". Always `mktemp`.
