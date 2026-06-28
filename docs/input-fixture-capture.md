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
