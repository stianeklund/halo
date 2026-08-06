---
name: crash-debug
tier: agent
triggers: ["access_violation", "access violation", "page fault", "page-fault", "assert", "hang", "soft deadlock", "deadlock", "freeze", "crash", "eip", "cr2", "trap frame", "register dump", "esp drift", "thunk recursion", "pe export", "symbolize", "crash signal"]
description: >-
  Unified crash and page-fault diagnosis for Halo CE Xbox lifting. Signal table
  (13 patterns), call-stack walk, page-fault ABI investigation, toggle-bisection
  for non-crashing regressions, and deactivation stub diagnosis. Invoke on any
  crash, page fault, ACCESS_VIOLATION, assert, hang, freeze, register dump,
  EIP/CR2 analysis, or toggle-bisect need. For non-crashing visual/behavioral
  bugs (wrong color, invisible geometry), use `lift-silent-bugs` instead.
---

# Crash & Page-Fault Diagnosis

**Auto-triggered** on any message containing `ACCESS_VIOLATION`, `page fault`,
`assert`, `hang`, `crash`, `EIP`, `CR2`, or `trap frame`.

---

## Step 1 — Extract crash registers

Parse the crash output for: EIP, EAX, EBX, ECX, EDX, ESI, EDI, EBP, ESP, CR2.

```bash
python3 tools/xbox/xbdm_debug_txt.py -20       # recent asserts
python3 tools/xbox/xbdm_rdcp.py "halt"
for t in 96 80 32 28; do
  echo "=== Thread $t ==="
  python3 tools/xbox/xbdm_rdcp.py "getcontext thread=$t control int"
done
```

Thread 28 = main game thread. Look for EIP, EBP, ESP, CR2.

---

## Step 2 — Match against signal table

| Pattern | Likely cause | Fix |
|---------|-------------|-----|
| EAX = own compiled addr | Thunk infinite recursion (ported=true, no body) | kb.json: flip `ported: false` |
| EAX=0, `FLD [EAX]`/`MOV [EAX]` | NULL pointer from missing `@<reg>` arg | `check-callee-regs` skill |
| ESI/EDI = float bit-pattern (0x3E–0x3F…) | Loop parameter corruption | Check loop bounds, pointer arithmetic |
| EBP == ESP | Stack exhaustion / infinite recursion | Walk call stack for cycle |
| CR2 = 0, EIP in 0x8001exxx | Soft deadlock (not lift arg bug) | `debug` skill Section C (hang investigation) |
| CR2 = float bit-pattern | Float-as-pointer (register used as address) | `lift-silent-bugs` check 1 |
| EIP in 0x10000–0x1Dxxxx, reg holds datum handle | Deactivation stub re-push (clobbered callee-saved regs) | See deactivation stub section below |
| EIP = `0x4` exactly | `__stdcall` fn-ptr cast missing → ESP drift after RET N | Add `__stdcall` to function pointer cast |
| EBX/ESI/EDI = garbage after call to deactivated fn | Deactivation stub clobbered callee-saved registers | Rebuild with CALL-based stubs |
| Assert `tag_groups.c:3089` (NONE-leaf index) | Collision raycast returned uninitialized leaf | Upstream physics state bug |
| Stack backtrace contains `BinkDoFrame` | **PRE-EXISTING** attract-mode crash — not our regression | Skip. Does NOT repro on real Xbox. |
| EIP in 0x642000+ range | Crash in compiled code. `build/halo.map` is stale. | `tools/xbox/symbolize_exception.py` |
| Assert from `cseries/errors.c` HALT | `system_exit(-1)` macro. | Verify assert uses `system_exit` thunk |

---

## Step 3 — Identify the suspect function

```bash
# Symbolize (never use build/halo.map — it's stale)
EXC=$(mktemp /tmp/halo-exception.XXXXXX)
rtk python3 tools/xbox/symbolize_exception.py --file "$EXC"
rtk python3 tools/xbox/symbolize_exception.py 0x<EIP> 0x<frame0> 0x<frame1>

# Original-code exact lookup
rtk jq '[.. | objects | select(.addr? == "<EIP_hex>")] | .[0] | {name, addr, ported}' kb.json

# Confirm redirect target from live memory
rtk python3 tools/xbox/xbdm_rdcp.py "getmem addr=0x<EIP> length=6"
```

## Step 4 — Walk the call stack

```bash
python3 tools/xbox/xbdm_rdcp.py "getmem addr=<EBP> length=128"
# Little-endian: [EBP+0]=saved_EBP, [EBP+4]=return_addr. Follow chain upward.
```

## Step 5 — Check recent signature changes

```bash
rtk git log --oneline -10 -- kb.json
rtk git diff HEAD~3 -- kb.json | grep -A5 -B5 "<EIP_hex or function_name>"
```

Red flags: recently added `@<reg>`, changed `decl`, changed calling convention.

---

## Page-Fault Deep Investigation

When the signal table points to a signature mismatch (the most common page-fault
cause), follow this procedure.

### Root cause pattern

A function changed in kb.json from a simple declaration to a ported function with
a **different signature**: added `@<reg>` args, changed return type width, changed
parameter count. Callers compiled against the old signature pass garbage.

### Cross-check callers

```bash
rtk jq '.objects[].functions[] | select(.name == "<function_name>") | {name, addr, decl}' kb.json
```

Use Ghidra MCP:
```
ghidra_get_function_callers(name="<function_name>", limit=20)
```

For each caller, verify: register args loaded before CALL, return value used
correctly, stack cleanup matches parameter count.

### Common fix patterns

**A — Wrong @<reg> annotation:** Revert to function_declaration or fix all callers
with disassembly evidence.

**B — Missing HDATA indirection:** Check if function returns a handle needing
`HDATA` dereferencing.

**C — Wrong return type width:** Check `eax` vs `eax:edx` patterns in disassembly.

---

## Toggle-Bisection for Non-Crashing Regressions

VC71 match% does NOT detect stack-layout or buffer-contiguity bugs. The box is
the only oracle for wrong colors, wrong physics, features that do nothing.

### Procedure

**Step 1 — Establish baseline:**
Revert entire suspect cluster (`ported=false` for all) + deploy. Symptom gone →
bug is in our lift. Symptom persists → not our lift (check pre-existing bugs).

**Step 2 — MANDATORY liveness verification gate:**
```bash
rtk python3 tools/xbox/verify_toggles_live.py
```
A stale/cached XBE silently inverts all verdicts. Run BEFORE `git checkout kb.json`.
On `RESULT: FAIL`, redeploy and retry.

**Step 3 — Binary search with dependency-aware subsets:**
- Enabling a callee under a still-original caller is ALWAYS safe
- Disabling a callee under a still-ported caller risks thunk arg-drop — split at
  a plain-cdecl boundary
- Enable half → deploy → test → recurse

**Step 4 — Investigate the isolated function:**
Read the call site (~10 disasm insns), check callee output buffer sizes vs caller
reads. Load `lift-decompiler-traps` for buffer-alias, `lift-silent-bugs` for the
5 silent-bug checks.

```bash
# Toggle a function:
sed -i '/"addr": "0xADDR"/,/ported/ s/"ported": true/"ported": false/' kb.json
./tools/xbox/build_deploy_run.sh -q
rtk python3 tools/xbox/verify_toggles_live.py   # ALWAYS verify after deploy
```

### Common false positives

- **Stale XBE:** verify_toggles_live.py catches this
- **Thunk arg-drop:** Disabling a callee under a ported caller drops `@<reg>` args
- **Multiple interacting bugs:** Binary search assumes one culprit; try pairs

---

## Deactivation Stub Crash

Crash inside **original** code (0x10000–0x1Dxxxx) with a datum handle in a
register that should hold a resolved pointer.

```bash
python3 tools/xbox/xbdm_rdcp.py "getmem addr=<impl_export_addr> length=32"
```
If the stub re-pushes more bytes than the function has stack params (excluding
`@<reg>` slots), the stub is broken. Run `patch.py --test-thunks` to verify fix.

---

## Pre-existing Xbox bugs

Before deep investigation, rule out known pre-existing bugs:
- **BinkDoFrame crash** after 2+ attract cycles → NV2A emulation issue
- **Intermittent collision asserts on specific maps** → test with all ported=false
- **`physical_memory_protect` page fault** → original XBE behavior

---

## Routing

- **Not a crash?** (visual bug, wrong behavior) → `lift-silent-bugs`
- **Need xemu probing?** → `debug-xemu`
- **Need real Xbox probing?** → `halo-xbdm`
- **Build/deploy failure?** → `debug`

## Output

- Fault address, EIP, function at fault
- kb.json signature vs actual caller behavior
- Confirmed / Inferred / Uncertain classification
- Fix applied + validation result
