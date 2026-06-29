# Boot Control: init.txt and Saved States (Cores)

This document covers three things:

1. What `init.txt` is and how the deploy uses it.
2. How to boot a map fresh from its start.
3. **How to boot straight to a saved in-game checkpoint** — the "core" mechanism. This is the main reason to read this doc.

> ⚠️ **Critical caveat for AI / encounter bugs:** a restored core **and** a console `map_name` boot do **NOT**
> faithfully reproduce script-driven squad behavior (advance / engage / target orders). Those are issued by the
> scenario **main script thread**, which runs only on a real **New Game**. If you are debugging "grunts won't
> advance/aim"-type bugs, read **§4** first — a degenerate repro will make *every* build (patched or pristine)
> look broken and waste days.

---

## 1. What is `init.txt`?

`init.txt` is a text file the debug build reads and executes at startup.

- Source: `src/halo/main/console.c`, `console_startup()` (~line 326).
- The build opens `d:\init.txt` (or `editor_d:\init.txt` in editor mode).
- It reads each line and evaluates it as a HaloScript console command, the same as if you typed it in the in-game console (`~` key).
- Lines starting with `;` are treated as comments (silently skipped).

### Where `d:\` maps on Xbox

On the Xbox devkit, `d:\` is the running title's own directory. For our deploy that is:

```
E:\GAMES\halo-patched\
```

So:

| Path in HaloScript | Physical location on Xbox |
|--------------------|--------------------------|
| `d:\init.txt` | `E:\GAMES\halo-patched\init.txt` |
| `d:\core\core.bin` | `E:\GAMES\halo-patched\core\core.bin` |
| `d:\core\<name>` | `E:\GAMES\halo-patched\core\<name>` |

### How the deploy handles it

`tools/xbox/deploy_xbox.py`, function `deploy_init_txt()` (~line 819):

- If `/mnt/g/dev/halo/init.txt` **exists**: uploads it to the Xbox on every deploy.
- If `/mnt/g/dev/halo/init.txt` **does not exist**: deletes the remote one.

**You control boot behavior simply by editing `/mnt/g/dev/halo/init.txt`.** No other config is needed — just save the file and run the next deploy.

---

## 2. Booting a Map Fresh

To load a level from its start, put `map_name <level>` in `init.txt`. The level names match the file names in `maps/`.

**Example `init.txt` — boot the Pillar of Autumn at Legendary:**

```
game_difficulty_set impossible
map_name a10
```

Common level names: `a10`, `a30`, `a50`, `b30`, `b40`, `c10`, `c20`, `c40`, `d20`, `d40`.

**`game_difficulty_set` values:** `easy`, `normal`, `hard`, `impossible` (Legendary).

If you omit `game_difficulty_set`, the build uses whatever difficulty was last set.

---

## 3. Booting to a Saved Checkpoint (the Core Mechanism)

This is the primary workflow for iterating on a build at a specific in-game moment.

### What is a "core"?

A core (`core.bin` or a named `.bin` file) is a **snapshot of the game-state heap only — not the XBE code**. This matters:

- You can rebuild and redeploy the patched XBE without touching the core file.
- The new code runs on the restored state.
- You can reproduce the exact same in-game scenario (enemies in the same positions, same health, same encounter state) every boot, without replaying the level.

The core is saved to and loaded from `d:\core\` on the Xbox.

### The recipe: boot straight to your checkpoint

1. **Play to the spot you want to re-test.** Get to the encounter, cutscene, or game state you care about.

2. **Open the console** (`~` key) and save a core:
   - Default slot: `core_save` — saves to `d:\core\core.bin`.
   - Named slot (recommended if you want to keep it): `core_save_name <name>` — saves to `d:\core\<name>`.

3. **Edit `/mnt/g/dev/halo/init.txt`** to load the core on next boot.

4. **Deploy** — every subsequent deploy will boot that map and overlay your saved state, dropping you straight to your checkpoint.

### Default slot (`core.bin`)

```
game_difficulty_set impossible
map_name a10
core_load_at_startup
```

### Named slot

```
game_difficulty_set impossible
map_name a10
core_load_name_at_startup a10_flood_encounter.bin
```

### Why `core_load_at_startup` must come AFTER `map_name`

A core is game-state data. The map must be fully initialized before the core can be overlaid onto it. `core_load_at_startup` (and `core_load_name_at_startup`) defer the load until after the map finishes initializing. If you put the `core_load_at_startup` line **before** `map_name`, the map will not be loaded yet and the command will have nothing to load into.

**Correct order:**
```
map_name a10           ← load the map first
core_load_at_startup   ← then overlay the saved state
```

**Wrong order — will boot a fresh map, not the saved state:**
```
core_load_at_startup   ← no map loaded yet, does nothing
map_name a10
```

### Core commands reference

| Command | What it does |
|---------|-------------|
| `core_save` | Saves game state to `d:\core\core.bin` |
| `core_save_name <name>` | Saves game state to `d:\core\<name>` |
| `core_load` | Loads `d:\core\core.bin` immediately (use in-console) |
| `core_load_name <name>` | Loads `d:\core\<name>` immediately (use in-console) |
| `core_load_at_startup` | Loads `d:\core\core.bin` after map initializes (use in init.txt) |
| `core_load_name_at_startup <name>` | Loads `d:\core\<name>` after map initializes (use in init.txt) |
| `game_revert` | Reverts to the last in-session campaign autosave (no core needed) |

### Troubleshooting

**The map loads fresh instead of my saved state:**
- Check that `core_load_at_startup` (or `core_load_name_at_startup`) appears **after** `map_name` in `init.txt`.
- Check that the core file exists on the Xbox at `E:\GAMES\halo-patched\core\core.bin` (or the named path). If you saved it in a previous session on a different Xbox partition or with a different name, it will not be there.
- Confirm that `init.txt` was actually uploaded — the deploy script prints `init.txt (N bytes)` when it uploads. If it prints `deleting init.txt...`, the local file is missing.

**The core was saved at a different difficulty / the wrong map loads:**
- `core_load_at_startup` restores the map that was active when `core_save` was called. If `map_name` in `init.txt` names a different map, the core load will not match and the behavior is undefined. Use the same map name as when you saved.

**The engine generated a `d:\a10_init.txt` (slow-frame core):**
- When the engine detects sustained slow frames, it auto-saves a core and writes `d:\<map>_init.txt` containing `map_name <map>` and a commented `;core_load_name_at_startup <name>` line. This is for diagnostic replay, not normal use. Copy the relevant lines to `/mnt/g/dev/halo/init.txt` if you want to boot from it.

---

## 4. Faithful repro for script-driven AI behavior (a10 grunt advance, etc.)

Some bugs are about **scripted squad behavior** — e.g. the a10 first-doorway grunts that should advance + aim +
fire. That behavior is issued by the scenario **main script thread**, which runs only on a genuine **New Game**.
Two common shortcuts silently break the repro and make *every* build look broken:

- **Console `map_name a10`** loads the BSP and statically spawns the encounters but does **not** run the script
  thread → grunts shuffle to static firing posts, never acquire the player, never aim (only return fire when shot).
- **A restored core** snapshots the heap but not live script-thread state. A core saved "just before the trigger"
  does not re-issue the advance/engage orders on reload → the same false-passive, in patched *and* pristine builds.

**Faithful method:** empty `init.txt` → boot to the main menu → **New Game → Pillar of Autumn → Impossible** →
play to the doorway. Confirmed 2026-06-27: pristine-unpatched grunts advance + aim + fire + cycle guard↔fight here;
the degenerate core/console repros showed passive grunts in *both* builds.

### Deploying the PRISTINE (unpatched) original for an A/B

`deploy_xbox.py --xbe-only` re-runs the `patched_xbe` target whenever the build ELF is newer
(`deploy_xbox.py:901-908`), so `cp cachebeta.xbe default.xbe && deploy` silently uploads the **patched** XBE.
Upload the original **directly**, bypassing the patch step:

```
python3 tools/xbox/xbdm_rdcp.py --sendfile halo-patched/cachebeta.xbe 'E:\GAMES\halo-patched\default.xbe'
```

then magicboot it: send `magicboot title=E:\GAMES\halo-patched\default.xbe debug` over XBDM (port 731).

### Verify which build is actually running (it flip-flops — always check)

- **XBE header section count** @ VA `0x1011C`: **24 = unpatched**, **30 = patched**. Readable even at the menu
  (header magic `XBEH` is live at `0x10000`).
- **In-game byte** @ a ported function (e.g. `0x2cdb0`, AI pages load only in-game): `55 8b ec` = unpatched
  prologue; `68 .. .. .. 00 c3` (`push <impl>; ret`) = patched redirect.

See `.claude/agent-memory/xbox-halo-re-analyst/project_a10_FAITHFUL_REPRO_and_gold_signature.md` for the full
field-offset table and the gold behavior signature.

---

## See also

- `docs/debug-commands-keyboard.md` — full HaloScript command reference, console keys, cheats.
- `docs/xbdm.md` — XBDM/RDCP workflow, deploy commands, real Xbox probing.
- `.claude/skills/halo-deploy-xbdm/SKILL.md` — deploy skill, covers `init.txt` upload in context of the full deploy flow.
