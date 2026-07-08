---
name: lift-crash-signals
description: "crash signal, eip, cr2, trap frame, register dump, esp drift, thunk recursion: Xbox runtime crash diagnosis — key register signals, call-stack walk procedure, toggle-bisection for non-crashing regressions and visual bugs. Invoke on any Xbox hang, ACCESS_VIOLATION, assert, silent wrong-behavior report, wrong color, tint, invisible geometry, missing objects, wrong physics, toggle bisect, visual regression, or when you need to localize which ported function caused a regression. For the full toggle-bisect procedure with liveness verification, see the `debug` skill Section D."
---

# Lift Crash and Regression Signals

**Invoke this skill when:**
- The game hangs, crashes, or asserts on the Xbox after a lift
- A visual regression appears (wrong color, missing geometry, wrong behavior)
- You need to localize which ported function caused a regression
- CR2 / EIP signals are ambiguous

Source: `docs/lift-learnings.md` §5 + §9 + §18.

---

## Quick signal table — read EBX/ESI/EDI/EIP/CR2 first

| Signal | Most likely cause |
|--------|------------------|
| EAX = function's own compiled address | Thunk infinite recursion — unimplemented `@<reg>` function (no body but ported=true) |
| EAX = 0, faulting instruction is `FLD [EAX]` or `MOV [EAX]` | NULL pointer — missing `@<reg>` arg passed as 0 (§3) |
| ESI or EDI = float bit-pattern (0x3Exxxxxx, 0x3Fxxxxxx) | Parameter corruption by loop — pointer advanced past buffer end (§4) |
| EBP == ESP | Stack exhaustion (infinite recursion) or frame not set up |
| CR2 = 0, EIP oscillating in kernel idle (≈0x8001exxx) | **Soft deadlock** — NOT a lift arg bug. A thread is blocked on a wait. Toggle-bisect the synchronization path, not the last lifted compute function. |
| CR2 = float bit-pattern (0x3Exxxxxx) | Register arg or float-as-pointer bug — a float was used as a pointer (§6) |
| Fault inside original code (0x10000–0x1Dxxxx), register holds datum handle | Deactivation stub re-pushed register-arg slot (§18) — check stub at `<impl_export>` |

---

## Step 1 — Get crash context

```bash
python3 tools/xbox/xbdm_debug_txt.py -20       # recent asserts
python3 tools/xbox/xbdm_rdcp.py "halt"
for t in 96 80 32 28; do
  echo "=== Thread $t ==="
  python3 tools/xbox/xbdm_rdcp.py "getcontext thread=$t control int"
done
```

Thread 28 = main game thread. Look for EIP, EBP, ESP, CR2.

## Step 2 — Identify the function from EIP

```bash
# Is EIP in original code (0x10000–0x1Dxxxx) or our compiled code (0x642000+)?
rtk python3 tools/xbox/symbolize_exception.py 0x<EIP> 0x<return_addr0> 0x<return_addr1>

# For an original entry-point trampoline, confirm which function is redirected:
rtk python3 tools/xbox/xbdm_rdcp.py "getmem addr=0x<EIP> length=6"
# 68 XX XX XX XX C3 = PUSH <target>; RET — the target is the original function addr.
# Do not use build/halo.map; symbolize compiled frames from build/halo exports.
```

## Step 3 — Walk the call stack

```bash
python3 tools/xbox/xbdm_rdcp.py "getmem addr=<EBP> length=128"
# Little-endian 4-byte values: [EBP+0]=saved_EBP, [EBP+4]=return_addr
# Follow chain upward to get full call stack
```

---

## Non-crashing regressions — toggle bisection (§9)

VC71 match% does NOT detect stack-layout or buffer-contiguity bugs. The box is
the only oracle for wrong colors, wrong physics, features that do nothing.

### Quick procedure (see `debug` skill Section D for the full detailed version)

**Step 1 — Establish baseline:**
Revert entire suspect cluster (`ported=false` for all) + deploy. Symptom gone →
bug is in our lift. Symptom persists → not our lift (check pre-existing bugs).

**Step 2 — MANDATORY liveness verification gate:**
```bash
rtk python3 tools/xbox/verify_toggles_live.py
```
This proves toggles are ACTUALLY live in the running image. A stale/cached XBE
silently inverts all verdicts — a whole session was burned this way. Run BEFORE
`git checkout kb.json`. On `RESULT: FAIL`, redeploy and retry; never record a
verdict against a failing gate.

**Step 3 — Binary search with dependency-aware subsets:**
- Enabling a callee (`ported=true`) under a still-original caller is ALWAYS safe
- Disabling a callee under a still-ported caller risks thunk arg-drop (§1) —
  split at a plain-cdecl boundary
- Enable half → deploy → test → recurse into the half that has the symptom

**Step 4 — Investigate the isolated function:**
Read the call site first (~10 disasm insns), then check callee output buffer
sizes vs what the caller reads back. Load `lift-arg-hazards` if swapped args,
`lift-decompiler-traps` trap 5 if buffer-alias, `lift-silent-bugs` for the
5 silent-bug checks.

```bash
# Toggle a function:
sed -i '/"addr": "0xADDR"/,/ported/ s/"ported": true/"ported": false/' kb.json
./tools/xbox/build_deploy_run.sh -q
rtk python3 tools/xbox/verify_toggles_live.py   # ALWAYS verify after deploy
```

### Common false positives in toggle-bisect

- **Stale XBE image:** Verify_toggles_live.py catches this. If it fails, the
  image on the box doesn't match your kb.json.
- **Thunk arg-drop:** Disabling a callee under a ported caller drops `@<reg>`
  args. The symptom may be the arg-drop, not the callee's lift. Only toggle at
  cdecl boundaries.
- **Multiple interacting bugs:** Two functions may both be needed to reproduce.
  Binary search assumes one culprit — if narrowing to one function doesn't
  reproduce, try pairs.

---

## Deactivation stub crash — specific tell (§18)

Crash inside **original** code (0x10000–0x1Dxxxx) with a datum handle
(e.g. `0xe364001f`) in a register that should hold a resolved pointer.

Check the stub at the compiled impl entry:
```bash
python3 tools/xbox/xbdm_rdcp.py "getmem addr=<impl_export_addr> length=32"
```
If it re-pushes more bytes than the function has stack params (excluding `@<reg>`
slots), the stub is broken. Run `patch.py --test-thunks` to verify fix.
