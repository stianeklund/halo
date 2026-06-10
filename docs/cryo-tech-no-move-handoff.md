# Handoff: Cryo tech NPC won't run to the door (a10 / `cryo_explosion_1`)

**Status:** Open. Root cause narrowed to a 2-way fork; the decisive capture is staged but not yet run.
**Date:** 2026-06-10. **Build:** clean `main`, rev `a562bea3` (no instrumentation).
**Companion notes:** `artifacts/cryo_tech_investigation.md` (same content, terser).

This is a self-contained handoff for a fresh agent. Read the "Traps" section before doing any
runtime work — several hours were lost to post-abort state reads and GDB harness issues.

---

## 1. Symptom

- Level **a10** (Pillar of Autumn). Loaded via **New Game, skipping the opening cinematic**
  (no saved game). NOTE: internally the map string is `levels\a10\a10`; the user sometimes
  calls it "a50" — it is a10.
- The **cryo technician** NPC speaks voice lines and turns to face the player (generic look
  behavior — works), but **never runs to the door / never executes its scripted movement.**
- Its command list **`cryo_explosion_1`** loops and aborts; debug log:
  `cryo_tech/tech crewman: command list cryo_explosion_1 is stuck looping (aborting on loop #5)`
- The user also expects an elite/covenant to attack/kill a crewman; **no elite appears, no
  other AI present.** This is almost certainly **downstream**: the tech never reaches the door,
  so the next scripted beat (spawn/kill) never fires. Do not chase the elite as a separate root.

### A SECOND, INDEPENDENT bug (do not conflate)
Letting the opening cinematic **play** (not skipping) crashes on the same clean build:
```
EXCEPTION halt in c:\halo\SOURCE\render\render_sky.c,#38:
  !render.visible_sky_model || scenario_get_sky(render.visible_sky_index)
backtrace: 0066f238 0018ca82 00184f7c 007bb673 006de203 006c14dd 006c1b90 00700aa1 001d43a1 800224ac
```
Symbolized: `display_assert <- [orig 0x18ca82 render_sky] <- [orig 0x184f7c] <- render_scene+0x23
<- render_frame+0x123 <- main_game_render+0x26d <- main_loop+0x690 <- main+0xc1`.
The scenario has **2 skies** (healthy); sky-lookup fns `FUN_0018e770`/`FUN_0018e7d0`(scenario_get_sky)/
`FUN_0018f3e0` are **byte-faithful** (VC71 100/91/91%). So `visible_sky_index` is being set ≥2
by its **setter** (not found) or by genuine cinematic data — NOT by these leaf fns. This is a
separate render bug; scenario load is healthy, so it is not a shared root with the tech bug.

---

## 2. TL;DR of the diagnosis

The tech **does** build a real path to a real destination (the door, 6.8 world units away) but
**never moves**. The path-builder chain and the locomotion orchestrator are either byte-faithful
or original code, so the bug is in one of exactly two places:

1. **`FUN_0005ff70`** (the pathfinder, `0x5ff70`, `ported=true`, **83.3% VC71** — the *only*
   non-faithful ported function in the path-production chain and the sole producer of the tech's
   paths) builds a subtly **wrong / unwalkable** path; OR
2. the path is fine but a **movement-type / flag** stalls the walk (downstream original code, or
   a flag set wrong upstream of the original locomotion).

The decisive discriminator is a **during-sequence** dump of the actual path waypoints
(`path_active=1`). Script staged at `/tmp/gdb_cryo6.txt` (contents in §7). A post-abort read
**cannot** distinguish these — see Traps.

---

## 3. Confirmed findings (with evidence)

| # | Finding | Evidence |
|---|---------|----------|
| 1 | **Path-builder chain is byte-faithful — NOT the bug.** | `FUN_0005e0d0` & `path_state_new` matched store-for-store vs `delinked/path.obj` disasm; VC71: `path_state_build_path` 383/385, `FUN_0005ff70` 84/84. Earlier low VC71 %s (68/71) were reference-boundary artifacts (vc71 ran past `ret` into unnamed sibling fns). |
| 2 | **Orchestrators are original code (`ported=false`).** | `actor_path_refresh` (0x2cdb0), `actor_destination_update` (0x2d350) — both `ported=false` in kb.json. The per-tick locomotion brain is the original binary's. |
| 3 | **Scenario load is healthy.** | Live: scenario @ `0x8083baf8`; cmd-list block @+0x438 = **124 lists**; sky block @+0x30 = **2 skies**, valid ptr. Rules out scenario-load corruption as a root. |
| 4 | **Command list IS processing; pathfinding IS attempted.** | GDB run, all hits = tech handle `0xe3630002`: `FUN_000169a0` (cmd-list advance) **277×**, `actor_path_refresh` **269×** (called from `actor_destination_update`+0x42, retaddr 0x2d392). |
| 5 | **The tech builds a real path 3×.** | `FUN_0005ff70` **3×** and `path_state_build_path` **3×**, both via apr's on-foot branch (retaddrs 0x2d26b / 0x2d285 are *inside* apr; apr was only ever entered for the tech). The other 266 apr calls coast on the fast-path (`actor_test_destination` = "already navigating to this destination"). |
| 6 | **Destination is real (the door), not degenerate.** | Live tech actor[2] (salt 0xe363): pos `(-55.18,-0.05,0.00)`, dest(+0x488) `(-50.53,-5.01,0.05)`, `|dest-pos| = 6.80`. |
| 7 | **Tech never moves** despite 1, 4, 5, 6. | User-confirmed "no attempt"; live position static across reads. |

### GDB firing pattern (run4, 200s window, handle 0xe3630002)
```
FUN_000169a0 (cmd-list advance):  277 hits
actor_path_refresh (0x2cdb0):     269 hits
FUN_0005ff70 (pathfinder 0x5ff70):  3 hits   (retaddr 0x2d26b, inside apr)
path_state_build_path (0x5eae0):    3 hits   (retaddr 0x2d285, inside apr)
```
Interpretation: per tick, `actor_destination_update` → `actor_path_refresh`. apr mostly takes
the `actor_test_destination` fast-path (returns "already navigating", no re-path). 3× it did a
full on-foot re-path and built a path. So pathfinding is neither blocked nor failing — it
succeeds — yet the actor doesn't walk.

### Live tech actor state (POST-ABORT — see Traps before trusting)
```
actor[2] salt=0xe363 action=2 movement_type(0x4c)=0 moving(0x484)=1 path_active(0x4a8)=0
  pos (0x12c)  = (-55.18, -0.05,  0.00)
  dest(0x488)  = (-50.53, -5.01,  0.05)   |dest-pos| = 6.80
  step_cnt(0x4c1) = 4   step_idx(0x4c2) = 0
```
`step_cnt=4` ⇒ non-trivial path was built (CAVEAT: post-abort, possibly stale). `path_active=0`
and `mtype=0` are post-abort cleared values — **do not** conclude "tech isn't in far-movement
mode" from this; that is a stale-state artifact.

---

## 4. What was tried, in order (incl. dead-ends — don't repeat)

1. **Prior-session diagnosis: "pathfinding builds no path (path_active=0)."** OVERTURNED. The
   `path_active=0` reading was taken at `action=2` (not obey) — i.e. *after* the list aborted to
   idle. Post-abort artifact. (This trap recurred; see Traps.)
2. **VC71-verified the whole `path.c` chain** → byte-faithful. Dead end as a bug source.
3. **Checked scenario sky/cmd-list blocks live** → healthy. Ruled out scenario-load root and the
   "stalled global script" hypothesis (the list demonstrably advances, run4).
4. **Symbolized + analyzed the cinematic sky-assert** → separate bug; sky-lookup fns faithful.
5. **Live GDB** (mechanism validated: `render_frame` 465 hits/14s): captured the run4 firing
   pattern above. Two early runs got 0 hits because the sequence wasn't triggered in-window
   (NOT a real negative — confirmed once the user ran it).
6. **Live actor read** post-abort: real destination, step_cnt=4, but stale path_active/mtype.
7. **Verified `actor_destination_update` (0x2d350) is original** → locomotion brain is not our lift.
8. **Looked at `FUN_0005ff70` VC71** = 83.3% (84/84 insns; ordering/scheduling delta). Did not get
   instruction-level operand diff yet; behavioral equivalence is weak without nav-mesh state.

---

## 5. Leading hypotheses & how to test each

### H1 — `FUN_0005ff70` builds a wrong/unwalkable path (operand-order bug)
- It's the only non-faithful ported fn producing the tech's paths (83.3% VC71, 84/84 insns).
- **Test A (cheapest, decisive):** during-sequence node dump (§7) — do the built waypoints lead
  from pos `(-55,0)` toward dest `(-50.5,-5.0)`? If they cluster at a wrong point / don't progress
  → bad path → it's `FUN_0005ff70`.
- **Test B:** get the instruction-level VC71 diff and judge if the 16.7% is a real operand swap
  (cross-product / subtraction-direction — see CLAUDE.md hazard #4) vs benign x87 scheduling.
  `tools/verify/compare_obj.py` against `delinked/path.obj` FUN_0005ff70; or
  `tools/verify/vc71_verify.py src/halo/ai/path.c --fpu-only` (note: terse — may need compare_obj
  for the raw instruction alignment).
- **Test C (non-interactive, end-game):** `tools/equivalence/unicorn_diff.py` on `0x5ff70` — but
  it needs a **nav-mesh state snapshot** (zero-fill seeds early-exit; coverage will be ~0). Capture
  via `tools/equivalence/dump_xemu_memory.py` during live gameplay (read CLAUDE.md "Live Memory
  Capture" — capture is finicky on this xemu; verify the dump before trusting).

### H2 — path is fine; the WALK / movement-type stalls
- `actor_destination_update` (0x2d350, original) has 3 branches keyed on a "movement type" field
  (comment says `== 4` = far-movement seek/flee; `!= 4` = reset path & return). The exact selector
  offset is **unverified** (line 926 reads `0x4c` only as a *gate flag*; the branch selector may be
  a different field). If the tech's go_to should set movement-type 4 but it's not set, the original
  locomotion clears the path instead of walking it.
- **Test:** in the during-sequence capture, also confirm (live, NOT post-abort) the movement-type
  field and whether branch-1 (`path_active != 0`) waypoint-walk runs and advances `step_idx`.
- Find **who sets the movement type / actor+0x488 destination** for a command-list `go_to` atom
  (the atom-execution / "start atom" path, e.g. `FUN_00017ab0`, and the go_to target resolution).
  If that's a ported fn with a bug, it's upstream of the original locomotion.
- The downstream biped physics consumes the computed movement direction (`actor+0x518`); check
  whether that's ported (bipeds.obj was recently worked — see memory `project_bipeds_*`).

---

## 6. Environment, addresses, and offsets (reusable)

### Target / tooling
- xemu runs our patched XBE as a **Windows .exe** (WSL2 host). It is a **debug build** (asserts +
  `error()`/debug.txt logging active).
- **XBDM** (live memory, works): `python3 tools/xbox/xbdm_rdcp.py "getmem addr=0x<va> length=0x<n>"`.
  It re-execs on Windows; `XBDM_HOST` defaults to `localhost`. Decode little-endian carefully.
- **GDB stub:** `localhost:1234` (reachable from WSL2 via mirrored networking). `/usr/bin/gdb`,
  `set architecture i386`. The xemu MCP (`mcp__xemu__*`) is unreliable here (reports
  `xemu_running:false` even when alive) — probe the stub directly instead.
- **Symbolization:** runtime VAs **equal kb.json addresses** for original code (XBE is based so
  e.g. `0x2cdb0`, `0x5064e4` are absolute). **Our lifted impls** live at **`0x642000 + RVA`**;
  `build/halo` is a PE — symbolize via its export table:
  ```python
  import pefile; pe=pefile.PE("build/halo", fast_load=True)
  pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_EXPORT']])
  # exp = sorted((s.address, s.name) ...); runtime = 0x642000 + RVA
  ```
  `build/halo.debug` has runtime VMAs but **no DWARF/symbols** (`nm` empty) — addr2line/nm won't work.

### Globals
- scenario ptr: `*0x5064e4`. sky block: `scenario+0x30` `{count,ptr}`. cmd-list block:
  `scenario+0x438` `{count,ptr}` (entry 0x60 bytes; atoms block @+0x30 `{count,ptr}`, atom 0x20 bytes,
  type = int16 @0).
- actor array: `*0x6325a4`. Header: esize `u16 @+0x22`, count `u16 @+0x2e`, data `u32 @+0x34`.
  element = `data + index*esize`. salt = `u16 @element[0]` (full handle = (salt<<16)|index).
  Current scene: count=3, esize=0x724; tech = index 2, salt 0xe363, handle `0xe3630002`.

### Actor struct offsets (verified unless noted)
```
+0x0c  position (game logic)        +0x12c position (3 floats) [used by locomotion]
+0x18  unit_handle                  +0x174/+0x178 facing x/y (used in segment dot tests)
+0x4c  movement gate flag (apr line-926 gate; nonzero during move)
+0x58  actr tag index               +0x484 is_moving (u8)
+0x488 destination (3 floats)       +0x4a8 path_active (u8) + path block base
+0x4a4 path-computed-this-tick (u8) +0x4bc "was moving" distance (float)
+0x4c1 step_cnt (signed char)       +0x4c2 step_idx (signed char)
+0x50c movement target (3f)         +0x518 movement direction = target - pos (3f)
+0x6c  current action (u16; 0xb=obey, observed 2 post-abort)
+0x9c  scripted-look prop index (NOT the obey cmd-list state)
waypoints: node k = (actor+0x4a8) + (step_idx+k+2)*0x10, x@+0 y@+4 z@+8
           [stride/offset UNVERIFIED — post-abort parse looked clustered; verify live]
```

### Key functions
```
0x2d350  actor_destination_update  ported=false  per-tick locomotion brain (3 branches)
0x2cdb0  actor_path_refresh        ported=false  path gate (mounted/override/on-foot)
0x169a0  FUN_000169a0              (actor_looking.c) cmd-list advance/loop handler.
                                   param_5(@esi)=cmd state: [0]=atom idx, [1]=loop count
                                   (aborts when loop count > 9; prints atom idx → "loop #5"
                                    = the loop atom at index 5). cryo_explosion_1 atoms:
                                    5,26,1(go_to),19,24(wait),20(loop→4),1,1,19.
0x5ff70  FUN_0005ff70              ported=true 83.3% VC71  THE PATHFINDER (prime suspect)
0x5eae0  path_state_build_path     ported=true faithful   extracts path → actor+0x4a8
0x5e0d0  FUN_0005e0d0              ported=true faithful   set destination in path-build state
0x5e090  path_state_new            ported=true faithful   init path-build state
0x2a580  actor_test_destination    ported=true            fast-path "already navigating"
0x18b90  FUN_00018b90              ported=true            obey/go_to atom validator
0x2f1a0  FUN_0002f1a0              (go_to cleanup, called when leaving the atom)
```
Source: `src/halo/ai/actor_moving.c` (actor_path_refresh @ line 499, actor_destination_update
@ line 902), `src/halo/ai/path.c`, `src/halo/ai/actor_looking.c` (FUN_000169a0 @ line 2276,
abort logger @ line 2363).

---

## 7. The staged decisive capture (run this next)

GDB command file (`/tmp/gdb_cryo6.txt`) — breakpoint the buildpath call site (path_active=1,
`$esi`=actor), dump the actual nodes across the 3 builds:
```gdb
set pagination off
set confirm off
set architecture i386
set tcp connect-timeout 10
target remote localhost:1234
break *0x2d285
commands
  silent
  printf "[BUILD] salt=0x%x step_cnt=%d step_idx=%d pos=(%.2f,%.2f) dest=(%.2f,%.2f) wp0=(%.2f,%.2f) wp1=(%.2f,%.2f) wp2=(%.2f,%.2f) pa=%d\n", *(unsigned short*)$esi, *(signed char*)($esi+0x4c1), *(signed char*)($esi+0x4c2), *(float*)($esi+0x12c), *(float*)($esi+0x130), *(float*)($esi+0x488), *(float*)($esi+0x48c), *(float*)($esi+0x4c8), *(float*)($esi+0x4cc), *(float*)($esi+0x4d8), *(float*)($esi+0x4dc), *(float*)($esi+0x4e8), *(float*)($esi+0x4ec), *(unsigned char*)($esi+0x4a8)
  continue
end
printf "=== ARMED6 buildpath-node-dump (bp 0x2d285) ===\n"
continue
```
Launch (see Traps for why this exact form):
```bash
rm -f /tmp/gdb_cryo6.log
setsid gdb -nx -batch -x /tmp/gdb_cryo6.txt >/tmp/gdb_cryo6.log 2>&1 </dev/null &
# confirm "ARMED6" in the log, THEN have the user New-Game and run the cryo sequence.
# stop cleanly when done: pkill -INT -f "gdb -nx -batch -x /tmp/gdb_cryo6.txt"
```
**Discriminator:**
- waypoints progress toward dest `(-50.5,-5.0)` AND pos changes across the 3 builds → **real path,
  being walked** → pivot to H2 (movement-type/flag stall; the path must be getting reset). Then
  capture the movement-type field + branch taken in `actor_destination_update`, **live**.
- waypoints cluster / don't lead to the door, or pos static with step_cnt>1 → **bad path** →
  it's H1 → fix `FUN_0005ff70` (chase the operand-order delta; CLAUDE.md hazards #4/#1).

To validate `$esi`=actor at 0x2d285: `salt` should print `0xe363` (the tech) and pos should be
near `(-55,0)`. If salt/pos look like garbage, the register/offset assumption is wrong — re-derive
from `actor_path_refresh` disasm (apr body uses ESI=actor throughout; comments in
`src/halo/ai/actor_moving.c` map the offsets).

---

## 8. Traps & gotchas (READ THIS)

- **Post-abort actor reads are stale.** When `cryo_explosion_1` aborts (loop #5), the tech goes
  idle: `path_active`, the movement gate (`0x4c`), and movement-type clear; action flips to 2.
  The path *node block* (step_cnt/waypoints) is abandoned, not necessarily cleared — values may be
  stale. **Always capture DURING the live sequence** (gdb at path_active=1). This single trap
  produced two overturned diagnoses across the session.
- **GDB harness:** launch `setsid gdb -nx -batch -x cmds </dev/null &`. Do **NOT** wrap in
  `timeout -s INT` — a stray/orphan INT killed a run mid-arm. The xemu stub **auto-removes
  software breakpoints on disconnect** (verified: byte at bp site read clean afterward, no 0xCC),
  so an abrupt gdb exit does not corrupt the game — but prefer a clean `pkill -INT` to be safe.
- **Confirm the sequence actually ran:** `FUN_000169a0` (0x169a0) firing proves the cmd list is
  processing. 0 hits = sequence not triggered in-window (NOT a real negative). Validate the bp
  mechanism with a positive control (`render_frame` 0x6de0e0 fires every frame).
- **gdb convenience-var counters** (`set $n=$n+1`) printed nonsense (`#-48`) in `-batch`; trust
  `grep -c` totals, not inline counters.
- **Endianness:** decode XBDM bytes as explicit little-endian u32/float; manual hex grouping
  caused two misreads (e.g. scenario ptr).
- **VC71 reference-boundary artifact:** small functions sandwiched between named symbols get a
  deflated % because vc71 extends the reference past `ret` into unnamed sibling fns. Check the
  delinked disasm boundaries before trusting a low %.
- The user loads via **New Game skipping the cinematic**; the cinematic path itself crashes (the
  separate sky bug), so "just don't skip it" is not a workaround.

---

## 9. Suggested order of attack for the next agent

1. Run §7 capture (one gdb-armed New Game). This forks H1 vs H2 in a single cycle.
2. If H1: get the `FUN_0005ff70` instruction diff (compare_obj vs delinked/path.obj); look for an
   operand-order swap; fix per CLAUDE.md hazards; re-verify VC71 + rebuild + retest.
3. If H2: find the movement-type setter for command-list go_to (atom-start path) and verify the
   `actor_destination_update` branch live; check whether the consuming biped physics is ported.
4. Keep the cinematic sky-assert (§1) as a separate ticket: find the setter of
   `render.visible_sky_index` (the sky-lookup leaves are already cleared).
