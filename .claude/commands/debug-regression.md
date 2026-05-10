---
description: Investigate and fix Halo CE XBE regressions via git bisect, Ghidra verification, and xemu probing
agent: deep
subtask: true
---

Use `halo-xbox-re` for doctrine, evidence rules, and the review checklist.
Use `halo-verify-debug` for regression-specific workflow and xemu probing.

Investigate and fix a Halo CE XBE regression. Bisect via git history first,
verify root cause against binary and disassembly, then implement the minimal
safe fix.

Argument: $ARGUMENTS (description of the regression symptom or failing test)

## Investigation priority

**Always start with git history.** Most regressions are introduced by a recent
commit. Static analysis is fast, free, and usually sufficient.

### Phase 1 — Git bisection (always first)

1. `rtk git log --oneline -20` — identify recent commits touching `kb.json`, C
   source, or types.
2. `rtk git diff HEAD~N -- kb.json src/` — inspect what changed.
3. For each suspicious commit, check:
   - Calling convention (cdecl/stdcall/fastcall/thiscall/register)
   - Argument count vs. `PUSH` count and `ADD ESP,N` cleanup
   - Return type vs. caller use of `EAX`
   - Struct size, packing, field offsets, static asserts
   - Side-effect ordering and initialization
   - `kb.json` `HDATA` pointer-vs-array choice (wrong level of indirection)
   - `@<reg>` entry without a corresponding implementation (→ infinite recursion)
   - Empty stub silently replacing a function with required side effects

### Phase 2 — Binary / Ghidra verification

Cross-check any suspect prototype against Ghidra disassembly. The decompiler
lies about operand sizes, interleaved pushes, and register args. Always verify
with the raw disassembly for each candidate.

### Phase 3 — Live probing (last resort)

Only when static analysis leaves the root cause genuinely ambiguous.

**Prefer XBDM on real Xbox** over xemu whenever a console is available:

- Build and deploy: `/deploy --xbe-only`
- Then probe with `/xbdm status`, `/xbdm context`, or `/xbdm mem <addr> <len>`

**Fallback — xemu probing** (only if no Xbox is reachable):

```bash
rtk python3 tools/xbox/xemu_qmp.py --host localhost --port 4444 --screenshot out.png
rtk python3 tools/xbox/xemu_qmp.py --host localhost --port 4444 --serial
rtk python3 tools/xbox/xemu_qmp.py --host localhost --port 4444 --hmp "info registers"
rtk python3 tools/xbox/xemu_qmp.py --host localhost --port 4444 --hmp "x /10x 0x<addr>"
rtk python3 tools/xbox/xemu_qmp.py --host localhost --port 4444 --pause
rtk python3 tools/xbox/xemu_qmp.py --host localhost --port 4444 --resume
```

Useful xemu probes for regressions:
- `info registers` after a crash → EIP tells you where it died
- `x /Nx 0x<addr>` → inspect memory at a suspect global or stack frame
- Serial output often contains `assert_halt` messages with file/line info
- Screenshot confirms whether the game reached a visible state at all

## Common regression classes

- Wrong prototype (arg count, arg type, return type)
- Wrong calling convention → ESP drift → crash at `eip=0x4`
- Wrong signedness or operand width
- Struct size/packing/offset drift
- Incorrect `HDATA` indirection level
- Reordered or missing writes / side effects
- Empty stub replacing a function that had required side effects
- `@<reg>` thunk without implementation → infinite recursion

## Fix process

Once the root cause is established:

1. Patch only what is necessary: code, type, layout, prototype, or `kb.json`.
2. Preserve:
   - calling convention and stack behavior
   - field offsets, packing, and static asserts
   - side-effect ordering
   - initialization and teardown behavior
3. Run the build and test in xemu after the fix.

## Allowed changes

- Narrow code corrections
- Prototype fixes
- Struct field/type corrections
- Restoring required side effects
- Conservative `kb.json` fixes
- Targeted static asserts if they prevent recurrence

## Disallowed (unless explicitly requested)

- Broad refactors or naming sweeps
- Style cleanup
- Multiple independent fixes in one patch
- Redesigning subsystem behavior

## Output format

Follow the `halo-xbox-re` output format. Report:

- **Symptom**: what failed and how it was observed
- **Commits investigated**: which changes were examined and why
- **Root cause**: the single most-supported hypothesis
- **Confirmed / Inferred / Uncertain**: label every claim per evidence policy
- **Fix**: exact change made (prototype, `kb.json` field, struct field, side effect)
- **Why minimal**: what was deliberately not changed
- **Validation**: build result, xemu test, callers/callees checked
- **Follow-up**: any remaining uncertainty or recommended next checks

## Hard rules

- Git history first — don't reach for xemu before reading the diff.
- Do not fix more than the evidence supports.
- Do not repack or reorder structs without binary evidence.
- If the hypothesis is too weak to fix safely, say so and stop.
- Unknown is better than wrong.
