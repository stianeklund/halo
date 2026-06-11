---
name: lift-crash-signals
description: Xbox runtime crash diagnosis — key register signals, call-stack walk procedure, and toggle-bisection for non-crashing regressions. Invoke on any Xbox hang, ACCESS_VIOLATION, assert, or silent wrong-behavior report.
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
# For compiled code, find which original function was redirected there:
python3 tools/xbox/xbdm_rdcp.py "getmem addr=0x<EIP> length=6"
# 68 XX XX XX XX C3 = PUSH <target>; RET — the target is the original function addr
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

1. **Revert entire suspect cluster** (`ported=false` for all) + deploy. Symptom
   gone → the bug is in our lift. Symptom persists → not our lift.
2. **Re-enable in dependency-complete subsets.** Re-enabling a callee is always
   safe. Toggling a callee OFF under a still-ported caller risks thunk arg-drop
   (§1) — split at a plain-cdecl edge.
3. **Once localized** to one function: read the call site first (~10 disasm insns),
   then check callee output buffer sizes vs what the caller reads back.

```bash
# Toggle a function without rebuilding:
sed -i 's/"ported": true/"ported": false/' ... # or vice versa, then:
rtk python3 tools/build/build.py -q --target halo && ./tools/xbox/build_deploy_run.sh -q
```

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
