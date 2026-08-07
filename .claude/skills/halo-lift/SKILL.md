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
All edits target **that path**, not a hardcoded `/mnt/g/dev/halo`.

### Sequence

1. **Pick a frontier target.** Select an un-implemented function called by an
   already-implemented function.
2. Resolve target by name or address in `kb.json` and Ghidra.
3. **Gather context & recover literals.** Callers, callees, globals, strings,
   imports, existing declarations. Recover string/constant literals pushed by
   address from `cachebeta.xbe`.
4. **Cross-check decompilation against raw disassembly.** Mandatory call-site
   verification: for every CALL, trace each PUSH backward. Watch for register
   aliasing, push-then-fstp, struct field rotation. Use `lift-decompiler-traps`
   for the full hazard checklist.
5. Infer the narrowest defensible prototype (see
   `docs/references/prototype-inference.md`).
6. **Pre-implementation pattern check** — scan for crash classes
   `check_lift_hazards.py` does NOT flag:
   - XCALLs to targets being ported
   - `&local_XX` args to callees that index `param[N]` (stack aliasing)
   - Loops advancing a parameter pointer when original uses a copy register
   - `(float)(int)` float-as-pointer smuggling
7. **Produce structurally faithful C lift:**
   - Preserve control-flow shape, side-effect order, pointer arithmetic
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
- `rtk python3 tools/lift_pipeline.py --target <name_or_addr> ...` for staged verify
- `rtk python3 tools/llm_auto_lift.py select --limit 20` for target selection
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
