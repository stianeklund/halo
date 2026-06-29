# Deterministic Per-Level Input Test Fixtures

`tools/xbox/capture_scenario.py` records your controller input on the real engine
and stores it — together with the checkpoint it started from and a one-shot deploy
recipe — as a **named, self-contained per-level fixture**. Replay the fixture to
drive the *exact same input every run*, so a code change is the only variable.
This is the record→replay oracle: replay the same input on the unpatched
`cachebeta.xbe` and a patched `default.xbe`, then diff the result.

It automates the full manual flow (arm → record → close → download → trim → store →
co-locate) that otherwise takes ~8 fiddly XBDM steps per scenario.

> **Host-only / copyright.** The whole `input-recordings/` tree is gitignored.
> `state.data` is controller input, but `core.bin` is a **game-state heap snapshot
> — copyrighted Halo runtime memory** (same class as the XBE). Fixtures live on
> this machine only; never commit these bytes.

See also: `docs/xbox-pad.md` (native recorder internals + sentinel files) and
`docs/boot-init-and-checkpoints.md` (the core/checkpoint mechanism, and the §4
caveat about script-driven AI).

## Fixture layout

```
input-recordings/levels/<level>/<scenario>/
  state.data      trimmed controller input (4 gamepads * 0x28 bytes per tick)
  core.bin        the checkpoint this scenario starts from   (start=core only)
  init.txt        boot recipe: "map_name <level>" + "core_load_at_startup"
  read.xts        empty playback sentinel
  recording.json  metadata + sha256 (written by input_recordings.py)
  REPLAY.md       per-fixture deploy recipe + determinism caveat
```

`<level>` is the map key (`a10`, `a30`, …); `<scenario>` is any folder/id you
pick (`cryo-bay-exit`, `first-doorway-grunts`). Each scenario is independent and
carries its own `core.bin` + input — different scenarios start from different
checkpoints, so the core travels with the recording.

## Prerequisites

- xemu (or a real Xbox) up and reachable over XBDM — the same transport
  `deploy_xbox.py` uses. Verify: `python3 tools/xbox/xbdm_rdcp.py 'dirlist name="E:\GAMES\halo-patched"'`.
- The debug build running (it reads `d:\init.txt` and the `*.xts` sentinels).
- For `--start core`: a saved core. Play to the spot, open the console (`~`) and
  run `core_save` first. The tool verifies `d:\core\core.bin` exists and aborts
  with a clear message if not.

## Start modes

| `--start` | init.txt | Captures | Use when | Caveat |
|-----------|----------|----------|----------|--------|
| `core` (default) | `map_name <lvl>` + `core_load_at_startup` | gameplay from your checkpoint | tight, fast-to-action fixtures | core does **not** re-run the scenario script thread — scripted squad orders won't re-issue (`boot-init-and-checkpoints.md` §4). Fine for movement/physics/weapons; wrong for scripted-AI-intro behavior. |
| `menu` | empty (boot to menu) | menu navigation **+** gameplay | script-faithful AI (real New Game) | long dead head (menu + cutscene); replay must start from the identical menu. |

(`map_name`-without-core is intentionally **not** offered — it was never validated
on this box, and the repo's rule is no unproven modes.)

## Commands

```bash
# Interactive one-shot (run in YOUR terminal — it waits for you to play):
python3 tools/xbox/capture_scenario.py record --level a10 --scenario cryo-bay-exit
#   preflight (verify core) -> arm + reboot -> "press Enter when done" ->
#   close -> download -> auto-trim idle tail -> store + co-locate -> replay to
#   validate -> sets known_good on your "y".

# Crash-safe split — play at your own pace between the two halves:
python3 tools/xbox/capture_scenario.py arm      --level a10 --scenario cryo-bay-exit
#   ...play the scenario from the checkpoint...
python3 tools/xbox/capture_scenario.py finalize --level a10 --scenario cryo-bay-exit

# Redeploy + boot an existing fixture (no capture):
python3 tools/xbox/capture_scenario.py replay --level a10 --scenario a10-checkpoint-5s-action
#   add --xbe default.xbe to drive the PATCHED build with the same input.

# Validate the trim/analyze math against on-disk recordings (no hardware):
python3 tools/xbox/capture_scenario.py selftest
```

Common options: `--start {core,menu}`, `--map <name>` (default = level),
`--xbe <file>` (default `cachebeta.xbe`), `--tail-pad <ticks>` (idle kept past
last input, default 36), `--title` / `--purpose` / `--difficulty` / `--build`
(metadata), `--force` (overwrite a `known_good` fixture), `--no-validate`
(skip the post-store replay), `--host` (XBDM host).

`arm`/`finalize` are the load-bearing pair; `record` is sugar over them. The box
state (sentinels + core) plus the args fully determine `finalize`, so a hiccup
during a multi-minute playthrough never loses the run.

## How trimming works

Recording starts at **boot**, so the head of `state.data` is the unavoidable
time-to-control (level load, and for `menu` the cutscene). The tool keeps that
head intact — replay must re-load the level at the same pace, so trimming the head
would desync playback — and trims only the **idle tail** after your last input
(`last_input_tick + --tail-pad`). If it finds no input in slot 0 it stores the file
untrimmed, warns, and leaves `known_good = false`.

## Determinism model (read before trusting a diff)

Input playback is **open-loop**: a fixed, tick-locked stream replayed regardless
of game state. That is exactly why it is an oracle — a behavior divergence
mid-replay is the *signal*, not a problem. The one confound: the meaningful presses
land at fixed **tick numbers**, after the time-to-control head. If a build change
shifts how many ticks it takes to reach player control, those presses land at a
different game moment (a desync that looks like a bug but isn't). For faithful lifts
the head is identical; still, the **first** time you reuse a fixture against a
patched `default.xbe`, confirm the action lands in the same place. The same core
loads into either build because we preserve heap layout.

The engine itself is deterministic by design (fixed RNG seed `0xDEADBEEF` reset per
map load, x87 precision reset, no wall-clock entropy in the RNG path), which is what
makes identical input yield identical simulation.

## Worked example

```bash
# 1. In-game on a10, at the spot you want the test to begin, open console (~):
core_save
# 2. From repo root:
python3 tools/xbox/capture_scenario.py record --level a10 --scenario cryo-bay-exit \
    --title "a10 cryo bay exit" --purpose "movement regression from cryo checkpoint"
# 3. Play your route when it drops you at the checkpoint; press Enter; answer "y"
#    when the replay reproduces it. Fixture lands at
#    input-recordings/levels/a10/cryo-bay-exit/ with known_good=true.
```

## Loading / replaying a saved recording

This is the "reset to the start state" button — every replay reboots fresh into the
core, so you never rely on in-game death/respawn.

```bash
# list what's stored for a level
ls input-recordings/levels/a10/
python3 tools/xbox/input_recordings.py ls --level a10        # with metadata

# load + boot an existing fixture on the unpatched build
python3 tools/xbox/capture_scenario.py replay --level a10 --scenario a10-checkpoint-5s-action

# run the SAME input against the patched build (the diff oracle)
python3 tools/xbox/capture_scenario.py replay --level a10 --scenario a10-checkpoint-5s-action --xbe default.xbe
```

`replay` uploads the fixture (core + `state.data` + `init.txt` + `read.xts`),
releases any handle a prior playback left open, and magicboots into playback.

### Choosing the build (cachebeta vs default)

`--xbe` selects which title replays the input, and the tool **verifies the
requested build is actually running** before declaring success (retrying if the
box auto-boots the wrong title after a reset):

| `--xbe` | Build | Use for |
|---------|-------|---------|
| `cachebeta.xbe` (default) | unpatched original | capturing/replaying the faithful reference |
| `default.xbe` | patched (our lifted C) | running the same input against your changes |

The same `core.bin` loads into either build (heap layout is preserved), so the
diff oracle is: `replay … --xbe cachebeta.xbe` then `replay … --xbe default.xbe`
and compare. `record`/`arm` honor `--xbe` the same way, so you can capture on the
unpatched build and replay on the patched one.

## Why death goes to the level start, not the core

A **core is a debug heap snapshot, not a campaign checkpoint.** When the player
dies, the engine (`main.c:2274–2283`) sets `byte_46DA3B`, waits ~90 ticks, then
calls `game_state_revert()` — which reloads the **campaign autosave** (the scripted
checkpoint, i.e. the level start), *not* your `core.bin`. This is by design and is
the same family as the `boot-init-and-checkpoints.md` §4 caveat.

For replay-based testing this does not matter: you reset by re-running `replay`
(reboot into the core), not by dying. Only tests that *involve* dying and want to
land back on the core need the hook below.

## Die-to-core debug hook (IMPLEMENTED & runtime-verified — commit 102a89df)

Goal: optionally make a player death reload the fixture core instead of the
campaign checkpoint, so a death-involving test loops without a reboot.

**Feasibility: small — and now verified against the lifted code.** The engine
already has the core-load machinery in the same main-loop iteration as the death
revert. Both blocks live in the main game-loop function (`while (1)` at
`main.c:2267`); both `if`s run the same iteration:

| Path | Location | Effect |
|------|----------|--------|
| death revert | `main.c:2274–2283`: after `byte_46DA3B` + ~90-tick (`word_46DA4C`) fade, calls `game_state_revert()` | reload campaign autosave (level start) |
| core load | `main.c:2335–2337`: `if (game_state_load_core_pending) game_state_load_core(core_name)` | reload `core_name` heap |

`game_state_load_core` (`game_state.c:319`) and `game_state_revert` run the same
structure (validate header → restore the `0x345000` heap at `*0x4ea994` → run the
13 init-for-new-map callbacks at `0x32eaa8`), so swapping one for the other at the
death dispatch is structurally safe.

**Verified mechanism — what `core_name` actually is (this is the load-bearing fact).**
`core_name` at `main.c:2332/2336` is a **global** (`HDATA char core_name[]` @
`0x46dd55`, `build/generated/decl.h`), *not* a local. Its only writers are three
**unlifted** hs handlers declared in `kb.json`:

- `main_load_core_name_at_startup` — the `core_load_at_startup` /
  `core_load_name_at_startup` init.txt directive; sets the global `core_name` and
  the startup pending byte `0x46da3f`.
- `main_save_core_name` / `main_load_core_name` — the console `core_save` /
  `core_load` commands.

On a new map, `main_new_map` (`main.c:436–453`) copies `0x46da3f` →
`game_state_load_core_pending` (`0x46da3e`), so the next loop iteration runs
`game_state_load_core(core_name)` at 2336. **Nothing in the per-frame path writes
the global `core_name`** — verified from the binary, not inference: Ghidra xrefs to
`0x46dd55` are the *complete* reference set (8 refs / 7 functions, covering unlifted
code). The only per-frame function that touches it is the main loop
(`FUN_00102e40`, body `0x102e40–0x1034a9`), which references it exactly twice — both
as the `const char *name` argument to `game_state_save_core` / `game_state_load_core`
(reads, not writes). The six writers (`0x1003b0–0x1004e1`) are each reached only via
an hs-command wrapper (`FUN_000c27xx`) — console / startup directive dispatch, never
per-frame. (The `char core_name[256]` local at `main.c:880` is a *shadow* inside
`main_frame_rate_debug` for slow-frame diagnostic dumps; it never touches the
global.) Therefore, after a fixture boots via `core_load_at_startup`, the global
`core_name` still points at the fixture core at the moment of death. The hook needs
**no extra name wiring**.

**Implementation (shipped in 102a89df — gated, off by default):**
- A `DECOMP_CUSTOM` `main_loop` local `die_to_core_enabled`, armed in the existing
  startup banner block via `file_get_full_attributes("d:\\die_to_core.xts") != -1`
  — the same file-attribute probe the recorder uses for `write/read/loop.xts`. No
  global plumbing: the startup arm and the death dispatch share the `main_loop`
  frame.
- In the death-revert block at `main.c:2280`, the bare `game_state_revert();` is
  now a gated branch:

  ```c
  #ifdef DECOMP_CUSTOM
      if (die_to_core_enabled && core_name[0]) {
        game_state_load_core_pending = 1;   /* reload fixture core, not the checkpoint */
      } else
  #endif
        game_state_revert();
  ```

  Setting the existing `game_state_load_core_pending` makes the core-load block at
  2335 fire later in the **same iteration** — the faithful core-load path, reused.

**Risks / edge cases (all benign):**
- If the global `core_name` is empty (booted without any `core_load*` directive),
  `core_name[0]` is `0` → falls through to `game_state_revert()`. The `&& core_name[0]`
  guard makes that automatic.
- If the player runs console `core_save`/`core_load` mid-session, the global is
  repointed — expected; not part of a death-loop fixture run.
- When the sentinel is absent (`g_die_to_core_enabled == 0`) the branch compiles to
  the original `game_state_revert()` call — **zero behavioral change**, faithful.

**Status:** implemented in commit 102a89df and **runtime-verified on xemu
(2026-06-28), including repeated deaths.** With `die_to_core.xts` armed and a core
saved, **three consecutive deaths each fired the hook and reloaded `core.bin`** —
no clearing of `core_name`, no re-entrancy crash, no reboot (confirmed with
temporary `error(2,"DTC: …")` markers, now stashed). The earlier "works only once"
report was **not a bug**: those retry-deaths had no core saved, so `core_name[0]==0`
and they fell through to the faithful revert.

**Operational prerequisite — `core_save` first.** The hook reloads whatever
`core_save` (or `core_load`) stored in the global `core_name`. With no core saved,
`core_name` is empty and a death does the normal level-start revert. The correct
loop is: in-game → `core_save` → die → reloads the saved spot, repeatably.

**Verdict signal (for a future verify script):** `error(2,…)` *does* reach
`debug.txt` (the boot banner and the `DTC:` markers both landed there), so an
instrumented `error(2)` marker in the hook is a grep-able verdict. The engine's own
`game_state_load_core` line uses `error(0,"loaded '%s'")`, which did **not** appear
in `debug.txt` — so don't rely on the stock `loaded` line; add an `error(2)` marker
or check respawn position.

**Caveat:** it remains *new* debug code (a deliberate deviation), kept **off by
default and sentinel-gated**, exactly like the recorder — faithful behavior is
preserved whenever `die_to_core.xts` is absent (and in cachebeta, which omits
`DECOMP_CUSTOM` entirely).

**Boot note:** drive this from a **menu start** (boot to the main menu, start the
level, then `core_save`), not a direct `core_load_at_startup` boot — the latter was
observed to stall after the startup banner on 2026-06-28.

## Self-running replay loop (`core_loop.xts`) — no death needed

For a deterministic loop that does **not** depend on the player dying, use the
EOF-triggered variant. When recorded-input playback reaches the **end** of
`state.data`, the engine reloads the core and rewinds playback to packet 0 — so the
stored input re-executes against the same core state, indefinitely. **Loop period =
the recording's length.**

Mechanism (DECOMP_CUSTOM, `default.xbe` only):
- `d:\core_loop.xts` at boot → input playback mode 4 + `core_loop_enabled` (`input_xbox.c`).
- At input EOF (mode 4) with a core loaded → set `game_state_load_core_pending` (`input_xbox.c`).
- The load-core dispatch in `main_loop` reloads the core and `SetFilePointer(handle,0,…)`
  rewinds the input (`main.c`); the rewind also fires for `die_to_core` and the startup load.

Drive it with the tool (`--loop` stages `core_loop.xts` instead of `read.xts`):
```bash
python3 tools/xbox/capture_scenario.py replay --level a10 \
    --scenario a10-checkpoint-5s-action --xbe default.xbe --loop
```

Verified on xemu 2026-06-28: an a10 fixture looped every **~13 s** (= the recording
length, 400 ticks) with **zero deaths and zero reboots**.

Notes:
- The loop resets exactly at the recording's EOF, so a short capture loops early —
  record a touch longer, or raise `--tail-pad`.
- Combine with `die_to_core.xts` to also reset the loop on a player death.
- `default.xbe` only (DECOMP_CUSTOM); `cachebeta.xbe` ignores the sentinel.

## Troubleshooting

- **"core not found"** — you didn't `core_save`, or saved to a different slot/map.
  Save a core on the same map first.
- **"no controller input detected in slot 0"** — `write.xts` wasn't present at
  boot (recording is armed only at input-init), or no controller was bound in
  xemu. Re-`arm` and confirm you played after the reboot.
- **finalize hangs at "rebooting to close"** — the recording handle is opened
  exclusively; the tool waits until a 4-byte probe read succeeds (handle released).
  If it times out, the box didn't reboot — re-run `finalize`.
- **Replay desyncs only on the patched build** — the time-to-control head shifted;
  see the determinism model above.
