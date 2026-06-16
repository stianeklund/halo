---
name: debug
description: >-
  Universal debugging entry point for Halo CE Xbox — invoke on ANY runtime
  problem: crash, page fault, ACCESS_VIOLATION, hang, freeze, deadlock, assert,
  wrong color, tint, invisible geometry, missing spawn, wrong position, wrong
  behavior, broken feature, visual regression, build failure, deploy failure,
  register dump, EIP, CR2, trap frame, soft-deadlock, or incorrect output.
  Routes to the correct specialized skill and provides inline procedures for
  hangs, asserts, toggle-bisect, build errors, and deploy failures.
---

# Debug — Universal Dispatch and Cookbook

**This is the single entry point for ALL debugging.** If you're unsure which
debug skill to use, start here.

Argument: `$ARGUMENTS` (symptom description)

---

## A — Symptom Router

Read the symptom, then follow the matching row:

| Symptom | Action |
|---------|--------|
| ACCESS_VIOLATION / page fault / CR2 nonzero | → Load `crash-triage` skill (signal table + root cause). If page fault, follow up with `halo-page-fault`. |
| Assert in debug.txt (e.g. `tag_groups.c:3089`) | → Section B below (Assert Triage) |
| Hang / freeze / soft-deadlock / CR2=0 / no output | → Section C below (Hang Investigation) |
| Wrong visual: color, tint, culling, invisible geometry | → Section D below (Toggle-Bisect), then `lift-silent-bugs` |
| Feature does nothing / wrong values / wrong positions | → `lift-silent-bugs` checks 1–5, then `/bug-hunt --full` |
| Build failure (compile error, linker error) | → Section E below (Build Error Triage) |
| Deploy failure (symbol absent, stale image) | → Section F below (Deploy Triage) |
| Register dump available but cause unclear | → Load `crash-triage` (signal table match) |
| Regression after recent commit | → Load `debug-regression` command (git bisect + verify) |
| xemu-specific probing needed | → Load `debug-xemu` skill (QMP, GDB, screenshots) |

**Before deep investigation:** Check if the symptom matches a known pre-existing
Xbox bug (Section G).

---

## B — Assert Triage

### Step 1 — Fetch the assert

```bash
rtk python3 tools/xbox/xbdm_debug_txt.py -20
```

Or via xemu serial:
```
mcp__xemu__xemu_read_serial()
```

### Step 2 — Parse the assert format

Asserts follow: `<source_file>:<line_number> <condition_or_message>`

### Step 3 — Match against known assert patterns

| Assert Pattern | Root Cause | Fix |
|----------------|-----------|-----|
| `tag_groups.c:3089` with NONE index `#2147483647` | Collision raycast returned uninitialized leaf (NONE=-1). Upstream ported function produces bad physics state that feeds `scenario_location_from_point`. | Check collision input path. Guard NONE-leaf with `(field & 0x7fffffff) == 0x7fffffff` demote-to-miss. |
| `"place < MAXIMUM_PLAYERS"` or bounds assert in consumer | Builder-returns-count ignored (lift-learnings §11). Builder fills buffer AND returns count; caller uses hard upper bound instead. | Replace `i < MAXIMUM_PLAYERS` with the builder's returned count. See `lift-silent-bugs` check 3. |
| `decals.c:479` or `effects.c` assert | Often pre-existing. Check if it reproduces with ALL ported functions disabled (ported=false). | If pre-existing, document and guard. If ours, toggle-bisect to isolate. |
| `cseries/errors.c` HALT macro | `system_exit(-1)` call. Verify the assert macro calls `system_exit` thunk with -1, not `halt_and_catch_fire()`. | Use `system_exit(-1)` in assert implementations. |
| Unknown file/line | Symbolize via `rtk jq` against kb.json source file mappings. Cross-reference Ghidra for the address. | Disassemble the assert location to understand the condition. |

### Step 4 — Determine if it's our bug or pre-existing

```bash
# Disable all recently ported functions:
# Toggle ported=false, rebuild, redeploy, reproduce
# If assert persists with everything disabled → pre-existing (Section G)
# If assert gone → toggle-bisect to isolate (Section D)
```

---

## C — Hang / Soft-Deadlock Investigation

**Signature:** Game stops responding. No assert in debug.txt. XBDM still
reachable. CR2=0 if you can read it. Threads alive but blocked.

**Key insight:** CR2=0 is NOT a lift argument bug. It indicates a thread blocked
on a kernel wait — investigate the synchronization path, not the last lifted
compute function.

### Step 1 — Confirm it's a hang (not a slow operation)

```bash
rtk python3 tools/xbox/xbdm_rdcp.py "halt"
# Get context for all game threads:
for t in 28 32 80 96; do
  echo "=== Thread $t ==="
  rtk python3 tools/xbox/xbdm_rdcp.py "getcontext thread=$t control int"
done
```

Thread 28 = main game thread. If EIP oscillates in kernel idle range (0x8001exxx),
the thread is blocked on a wait/signal.

### Step 2 — Identify what the blocked thread was doing

Read EBP chain to get the call stack (see `lift-crash-signals` Step 3). The
return addresses above the kernel idle frame tell you which game function
entered the wait.

### Step 3 — Check state machine phase

Many hangs are state machine deadlocks. Known phase globals:

| Global | Purpose |
|--------|---------|
| `0x5aa730` | Game engine postgame phase (0=inactive, 2=countdown, 3=score/postgame) |
| `0x5aa728` | Postgame countdown timer |
| `0x5aa72c` | Postgame tick counter |

```bash
rtk python3 tools/xbox/xbdm_rdcp.py "getmem addr=0x5aa728 length=16"
```

### Step 4 — Toggle-bisect the sync path

Hangs are caused by state transitions, not compute. Toggle recently ported
functions that manage state machines, network sync, or thread signaling. Follow
the full toggle-bisect procedure in Section D.

---

## D — Toggle-Bisect (Full Procedure)

The runtime ported-flag toggle is the oracle for visual regressions, wrong
behavior, and behavioral bugs that VC71/instruction-diff cannot detect.

### Step 1 — Establish baseline

Revert the ENTIRE suspect cluster to `ported=false`:

```bash
# For a single function:
sed -i '/"addr": "0xADDR"/,/ported/ s/"ported": true/"ported": false/' kb.json
# Or for a batch, use artifacts/toggle_ported.py if available
```

Build, deploy, and confirm the symptom is **resolved**:
```bash
./tools/xbox/build_deploy_run.sh -q
```

If symptom **persists** with everything disabled → not our lift. Check Section G.

### Step 2 — MANDATORY liveness verification gate

**Run this BEFORE recording ANY verdict:**

```bash
rtk python3 tools/xbox/verify_toggles_live.py
```

This QMP-reads each function's VA and confirms:
- Toggled-off functions run ORIGINAL bytes (`55 8B EC...` prologue)
- A sampled positive control still shows redirects (`68 <impl> C3`)

**On `RESULT: FAIL`:** The running image is stale/cached. Redeploy and retry.
**Never record an in-game verdict against a failing gate.** A whole debugging
session was once burned by this — the stale image silently inverted every result.

**Run it BEFORE `git checkout kb.json`** — it compares the live image against
the working-tree kb.json.

### Step 3 — Binary search with dependency-aware subsets

**Safe rule:** Enabling a callee (ported=true) under a still-original caller is
ALWAYS safe. Disabling a callee under a still-ported caller risks thunk
arg-drop (lift-learnings §1) — split at a plain-cdecl boundary.

1. Enable half the suspect functions → deploy → test
2. If symptom returns, the bug is in the enabled half
3. If symptom stays gone, enable the other half
4. Recurse until isolated to a single function

### Step 4 — Investigate the isolated function

Once narrowed to one function:
1. Read the call site disassembly (~10 instructions around each CALL)
2. Check callee output buffer sizes vs what the caller reads back
3. Look for register/buffer swap (load `lift-arg-hazards` if swapped args)
4. Look for buffer-alias confusion (load `lift-decompiler-traps` trap 5)
5. Check for silent-bug patterns (load `lift-silent-bugs` checks 1–5)

### Step 5 — Verify the fix

After fixing, re-run liveness gate + visual/behavioral confirmation:
```bash
./tools/xbox/build_deploy_run.sh -q
rtk python3 tools/xbox/verify_toggles_live.py
# Then visually confirm the symptom is gone
```

---

## E — Build Error Triage

### `call to undeclared function 'FUN_XXXXXXXX'`

The function was renamed in kb.json. Do NOT read source files first:

```bash
rtk rg "FUN_XXXXXXXX" build/generated/decl.h          # not there = renamed
rtk jq '[.. | objects | select(.addr? == "0xXXXXX")] | .[0] | {name, decl}' kb.json
```

Update the call site to use the new name from kb.json.

### `ported=true symbol absent from EXE exports`

```bash
grep -rn "FUN_XXXXXXXX" src/
```

- **Implementation found** → source file has compile errors OR is missing from
  `CMakeLists.txt`. Fix errors; do NOT set `ported=false`.
- **No implementation found** → function marked ported without a body. Set
  `ported=false` until a proper lift is done.

### Undeclared type

Check `build/generated/` headers. If a type was renamed or moved, update the
`#include` or the type reference in source.

### Linker error (undefined symbol)

```bash
rtk rg "symbol_name" src/CMakeLists.txt
```

If the source file isn't listed in `CMakeLists.txt`, add it. New TUs pass VC71
verify + build.py but never link into the EXE (the "silent unlinked TU" bug).

---

## F — Deploy Triage

### "symbol absent from EXE exports"

Source file not registered in CMakeLists.txt. See Section E linker error.

### Stale halo.map symbolization

**build.py never regenerates `build/halo.map`.** If you symbolize a crash from
`halo.map`, you WILL get wrong function names.

**Always symbolize from the fresh PE export table** (base 0x642000):

```bash
rtk python3 -c "
import pefile
pe = pefile.PE('build/halo-patched/default.xbe')
for exp in pe.DIRECTORY_ENTRY_EXPORT.symbols:
    if exp.name: print(f'0x{exp.address:08x} {exp.name.decode()}')" | grep <partial_name>
```

### Deploy self-verification failure

`deploy_xbox.py` matches the running `DECOMP BUILD <date>` token against the
local XBE. If it fails:
- The XBE on the box is from a different build
- Redeploy: `./tools/xbox/build_deploy_run.sh -q`
- Trust the gate — never use file size/mtime as proof

---

## G — Pre-Existing Xbox Bug Checklist

Before spending hours debugging, check if the symptom matches a known
pre-existing bug in the original `cachebeta.xbe`:

| Symptom | Bug | Evidence |
|---------|-----|----------|
| ACCESS_VIOLATION in `BinkDoFrame` after 2+ attract cycles | NV2A emulation / `physical_memory_protect` page cycling | Confirmed in both patched and original XBE. Does NOT reproduce on real Xbox hardware. |
| Intermittent collision assert on specific maps | Map geometry edge cases in original collision BSP | Check if assert reproduces with ALL ported functions disabled. |
| Texture corruption in specific lighting conditions | Original Xbox GPU precision issues | Compare against known retail behavior. |

**Test:** Disable all recently ported functions (`ported=false`), rebuild, and
reproduce. If the bug persists → it's pre-existing, not our regression.

---

## Cross-Reference: Specialized Debug Skills

| Skill | Use When |
|-------|----------|
| `crash-triage` | Have register dump, need signal-table match and root cause |
| `halo-page-fault` | Page fault specifically, need deep ABI/signature investigation |
| `lift-crash-signals` | Xbox runtime signals, call-stack walk, deactivation stub diagnosis |
| `lift-silent-bugs` | Non-crashing correctness bugs (5 specific check patterns) |
| `lift-arg-hazards` | Argument passing errors (cdecl mis-group, NULL regs, swap) |
| `lift-decompiler-traps` | Ghidra decompiler pitfalls at call sites (6 traps) |
| `lift-frame-hazards` | Buffer sizing, stack aliasing, _chkstk frames |
| `check-callee-regs` | Missing `@<reg>` annotations on unported callees |
| `bug-hunt` | Automated tiered scanning (edit-time through pre-deploy) |
| `halo-verify-debug` | Verification ladder, delink comparison, regression debugging |
| `debug-xemu` | xemu-specific probing: QMP, GDB, screenshots, memory dumps |
| `halo-xbdm` | Real Xbox XBDM/RDCP commands |
| `debug-regression` | Git bisect driven regression investigation |
