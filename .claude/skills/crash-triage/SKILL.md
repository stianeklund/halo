---
name: crash-triage
description: Automated crash and bug diagnosis for Halo CE Xbox lifting. Invoke whenever debugging any crash, page fault, ACCESS_VIOLATION, assert, hang, deadlock, freeze, regression, wrong behavior, broken feature, incorrect output, or runtime problem. Parses crash registers, matches signal table, identifies root cause, proposes fix. Use before halo-page-fault for first-response triage.
---

# Crash Triage — Automated First-Response Diagnosis

**Auto-triggered** on any message containing `ACCESS_VIOLATION`, `page fault`,
`assert`, `hang`, or `soft deadlock`.

Delegates detailed investigation to `halo-page-fault` and `lift-crash-signals`.

---

## Step 1 — Extract crash registers

Parse the crash output for: EIP, EAX, EBX, ECX, EDX, ESI, EDI, EBP, ESP, CR2.

---

## Step 2 — Match against signal table

| Pattern | Likely cause | Route to |
|---------|-------------|----------|
| EAX = own compiled addr | Thunk infinite recursion (ported=true, no body) | kb.json: flip `ported: false` |
| EAX=0, `FLD [EAX]`/`MOV [EAX]` | NULL pointer from missing `@<reg>` arg | `check-callee-regs` skill |
| ESI/EDI = float bit-pattern (0x3E–0x3F…) | Loop parameter corruption | Check loop bounds, pointer arithmetic |
| EBP == ESP | Stack exhaustion / infinite recursion | Walk call stack for cycle |
| CR2 = 0, EIP in 0x8001exxx | Soft deadlock (not lift arg bug) | Toggle-bisect sync path |
| CR2 = float bit-pattern | Float-as-pointer (register used as address) | `lift-silent-bugs` check 1 |
| EIP in 0x10000–0x1Dxxxx | Deactivation stub re-push | `patch.py --test-thunks` |

---

## Step 3 — Identify the suspect function

```bash
# Is EIP in original code or compiled code?
rtk jq '[.. | objects | select(.addr? == "<EIP_hex>")] | .[0] | {name, addr, ported}' kb.json

# For compiled code, find the original via thunk trampoline:
rtk python3 tools/xbox/xbdm_rdcp.py "getmem addr=0x<EIP> length=6"
# PUSH <target>; RET → target is original function address
```

---

## Step 4 — Check recent signature changes

```bash
rtk git log --oneline -10 -- kb.json
rtk git diff HEAD~3 -- kb.json | grep -A5 -B5 "<EIP_hex or function_name>"
```

Red flags: recently added `@<reg>`, changed `decl`, changed calling convention.

---

## Step 5 — Propose fix path

Based on signal match + signature check, output one:

- **Confirmed:** [root cause] → [specific fix command]
- **Inferred:** [likely cause, N% confidence] → [verification step to confirm]
- **Uncertain:** [symptoms only] → load `halo-page-fault` skill for full investigation

When uncertain, do NOT guess. Load `halo-page-fault` and follow its step-by-step workflow.
