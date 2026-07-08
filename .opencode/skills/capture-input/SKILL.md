---
name: capture-input
description: "Interactive wizard to capture (record) or replay a deterministic controller-input fixture for a Halo CE Xbox level. Invoke when the user wants to record gameplay, capture a scenario / playthrough, build an input fixture, or replay an existing one — and doesn't want to remember the capture_scenario.py flags. Prompts for level, scenario, start mode, build, difficulty, tail-pad, title, and purpose, then drives the capture. Wraps tools/xbox/capture_scenario.py; deeper doctrine in the input-replay-testing skill and docs/input-fixture-capture.md."
---

# Capture Input — guided record/replay wizard

**Invoke this skill when the user wants to:**
- Record a gameplay sequence into a reusable per-level fixture ("record a10 from
  the start", "capture a playthrough", "make an input fixture").
- Replay an existing fixture (optionally against the patched build to diff).
- …without remembering the `capture_scenario.py` flags.

This skill **asks for every argument** (with sensible defaults), assembles the
command, and runs it for the user. The underlying tool and the *why* live in the
`input-replay-testing` skill and `docs/input-fixture-capture.md`.

---

## Step 0 — the interactivity constraint (read first)

`capture_scenario.py record` is **interactive**: it arms, blocks on
`input("press Enter when done")`, then on a `[y/N]` validation prompt. Those
`input()` calls hit EOF under a non-interactive shell and crash. **Never run
`record` (or a validating `finalize`) through the Bash tool.**

Instead drive the load-bearing split across conversation turns:
**`arm` → (user plays) → `finalize --no-validate` → `replay` → bless.** Both
`arm` and `finalize --no-validate` are non-interactive and safe to run.

---

## Step 1 — collect the arguments

Collect the enumerable arguments with **AskUserQuestion**. `AskUserQuestion`
caps at **4 questions per call**, so a Record/Arm flow needs **two calls in
sequence** — Round 1 then Round 2. **Both rounds are mandatory** for Record new /
Arm only; do not skip Round 2 or wait for the user to remind you. Don't re-ask
values the user already gave in their message. For free-text fields, propose a
default and let the user accept or override.

**Round 1 — first AskUserQuestion call (4 choices):**

| Question | Header | Options |
|----------|--------|---------|
| What do you want to do? | Action | **Record new fixture** (recommended) · Replay existing · Arm only · Finalize only |
| Which level? | Level | a10 (Pillar of Autumn) · a30 (Halo) · b30 (Silent Cartographer) · *Other* (type any key: a50 b40 c10 c20 c40 d20 d40 …) |
| Start mode? | Start | **mapreset** (recommended) — boots straight into the level via `map_name`+`map_reset`, fresh from the start, **no menu nav**, immune to any stale savegame · **core** — boot a saved checkpoint (needs `core_save` first) · **menu** — genuine New Game *including* menu navigation (use only if you specifically want the menu captured) |
| Which build? | Build | **cachebeta.xbe** — unpatched / faithful (recommended) · default.xbe — patched (your lifted C) |

**Round 2 — REQUIRED second AskUserQuestion call (Record new / Arm only).**
After Round 1 returns, immediately make a second AskUserQuestion call with these
enumerable questions (do not narrate first, do not assume defaults — ask):

| Question | Header | Options | Applies when |
|----------|--------|---------|--------------|
| Which **difficulty** should the level run at? | Difficulty | **impossible** (Legendary) · **hard** (Heroic) · **normal** · **easy** | **mapreset** (pinned via `game_difficulty_set`). `menu` → user picks in-game; `core` → baked into the core. Omit the question only for `menu`/`core`. |
| How much **tail-pad** (idle ticks kept past the last input)? | Tail-pad | **90** (long playthrough, recommended) · **36** (default, short scenario) · *Other* | always |
| **Overwrite** the existing `known_good` fixture? | Overwrite | Yes (adds `--force`) · No | **only** if a `known_good` fixture already exists at that level/scenario |

> The difficulty question is the load-bearing one — for a `mapreset` capture it
> is **always asked** and pins `game_difficulty_set` in `init.txt`, so the fixture
> records the difficulty it was captured at. Never silently default it.

**Free-text fields (no AskUserQuestion needed — propose a default in prose and
let the user override):**
- **scenario id** — default `<level>-play-from-start`. Folder name under
  `input-recordings/levels/<level>/`.
- **title** / **purpose** — default both to `"<level> playthrough"`. Override if
  the user described something specific.

**For Replay**, list what exists first so the user picks a real one:
`rtk ls input-recordings/levels/<level>/`. Then ask build, and whether to
`--loop` (self-running loop; `default.xbe` only).

---

## Step 2 — run it

Build the common arg string once: `--level L --scenario S --start MODE --xbe XBE
--tail-pad N --title "T" --purpose "P" [--difficulty D] [--force]`. Pass
`--difficulty` for `mapreset` (it pins `game_difficulty_set`); omit it for
`menu`/`core`. Always prefix with `rtk`.

### Record new (the usual path)

1. **Arm** (non-interactive):
   ```bash
   rtk python3 tools/xbox/capture_scenario.py arm <common args>
   ```
   - `start=mapreset` (recommended): boots **straight into the level**, fresh from
     the start (`map_name`+`map_reset` overrides any savegame). No save-clearing
     needed; the level loads directly into record mode.
   - `start=core`: preflights `d:\core\core.bin` and aborts if missing → tell the
     user to play to the spot and run `core_save` in the console first.
   - `start=menu`: best-effort save-clear runs first; note that `savegame.bin` is
     locked while the title runs, so if it warns the save survived, a New Game may
     resume into it — prefer `mapreset`.

2. **Hand off to the user.** Tell them plainly:
   - mapreset/core: *"Recording is armed — the level is loading in record mode.
     Play your scenario now (\<purpose>). Tell me when you've finished."*
   - menu: *"…play from the main menu (New Game → level → difficulty), then play
     your scenario. Tell me when you've finished."*

   Then wait for their reply. Do not poll.

3. **Finalize** when they say done — use `--no-validate` (avoids the interactive
   confirm) and a longer Bash timeout (it waits up to ~150 s for the record
   handle to close after the reboot):
   ```bash
   rtk python3 tools/xbox/capture_scenario.py finalize <common args> --no-validate
   ```
   Run with `timeout: 200000`. Report the tick stats it prints (slot0 bytes,
   input tick range, trimmed tick count). If it warns "no controller input in
   slot 0", the recording didn't arm in time — re-`arm` and re-play.

4. **Validate by replay** (non-interactive — no prompt when run standalone):
   ```bash
   rtk python3 tools/xbox/capture_scenario.py replay <common args>
   ```
   Ask the user (AskUserQuestion yes/no): *"Did it reproduce your run on
   screen?"*

5. **Bless** on "yes" (the fixture is stored `known_good=false` until then):
   ```bash
   rtk python3 -c "import sys; sys.path.insert(0,'tools/xbox'); import capture_scenario as c; c._set_known_good('L','S',True)"
   ```
   On "no", leave it false and note the likely cause (play route differed, or
   the time-to-control head shifted — see the determinism note in the doc).

### Arm only / Finalize only
Run just that half with the common args (finalize → add `--no-validate`, then
optionally do the replay + bless as above).

### Replay existing
```bash
rtk python3 tools/xbox/capture_scenario.py replay --level L --scenario S --xbe XBE [--loop]
```
Diff oracle: replay once with `--xbe cachebeta.xbe`, once with `--xbe
default.xbe`, and compare on screen.

---

## Notes
- **Clean slate: `mapreset` solves it in-engine.** `map_reset` restarts the level
  from the beginning, overriding any campaign savegame — so a mapreset capture is
  fresh every time with no file deletion (verified on the box: it ignores even a
  freshly-saved checkpoint). This is why mapreset is the recommended mode.
- **`menu` start can't fully clear the save.** `savegame.bin` is held open (locked,
  XBDM 414) while the title runs, so `arm`/`replay` delete only the unlocked
  pointer files (best-effort) and *warn* if the save survives — a menu New Game may
  then resume into it. Prefer `mapreset`; `--no-clear-saves` skips the attempt.
- **Difficulty is per-recording (mapreset).** `--difficulty` pins
  `game_difficulty_set` in `init.txt` and is stored in the fixture metadata, so a
  replay always runs at the captured difficulty. (menu picks it in-game; core
  keeps the core's.)
- **Host-only.** `input-recordings/` is gitignored; `core.bin` is copyrighted
  game memory — never commit fixtures.
- **Prereq:** xemu / Xbox up and reachable over XBDM (same transport as deploy).
- Pass `--host <ip>` through if the user targets a non-default box.
