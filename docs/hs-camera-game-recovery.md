# HaloScript Camera, Game, Map & BSP Dispatch Evaluator Recovery Report
**Target:** Halo: Combat Evolved (Original Xbox Debug Build 01.10.12.2276, Oct 12 2001)  
**Binary:** `cachebeta.xbe` (MD5 `c7869590a1c64ad034e49a5ee0c02465`)  
**Branch:** `camera-game-evaluators`  
**Date:** September 13, 2026  
**Author:** Nick Arcade / Antigravity Pair  

---

## Executive Summary

This document details the recovery, technical analysis, and semantic labeling of **24 consecutive Camera, Game, Map, and BSP HaloScript dispatch evaluators** in [`src/halo/hs/hs.c`](../src/halo/hs/hs.c) and [`kb.json`](../kb.json) (`0xc1640`–`0xc1900` and `0xc1cf0`–`0xc1ec0`).

Every single recovered name is backed by **Tier 1 direct target binary evidence** extracted from the static `hs_function_table` at virtual address `0x002f1588` (file offset `0x002eb0c8`, 418 entries) in the authentic Xbox debug binary `cachebeta.xbe`.

Zero semantic or algorithmic alterations were introduced: the change consists exclusively of replacing placeholder symbols (`FUN_000c1xxx`) with canonical Bungie identifiers, verified via byte-matching and disassembly analysis.

---

## 1. How the Names Were Recovered (Binary Evidence)

In Halo CE Xbox debug build 2276 (`cachebeta.xbe`), all HaloScript functions are registered in a static descriptor table located at VA `0x002f1588`. Each table entry has the following structure:

```c
struct hs_function_definition {
    int16_t return_type;
    int16_t flags;
    const char *name;             // Pointer to script command string in .rdata
    void (*exec_proc)(int16_t fn_idx, int thread_datum, char init); // Dispatch evaluator
    const char *help_string;      // Pointer to developer help string in .rdata
    int16_t param_count;
    int16_t param_types[8];
};
```

Each entry's `exec_proc` function pointer points directly to the compiled dispatch handler in `hs.obj`.
Because Bungie compiled both the command names (`name`) and the developer documentation strings (`help_string`) into `.rdata`, resolving each entry's `exec_proc` yields **100% authentic, authoritatively named Bungie symbols** without any speculative naming.

---

## 2. Complete Catalogue of 24 Recovered Evaluators

The 24 functions span table entries 245 through 268 in `hs_function_table`:

| VA | Table Index | Script Command Name | Recovered C Identifier | Bungie Help / Documentation String |
| :--- | :---: | :--- | :--- | :--- |
| `0xc1640` | 245 | `camera_control` | `hs_evaluate_camera_control` | toggles script control of the camera. |
| `0xc1680` | 246 | `camera_set` | `hs_evaluate_camera_set` | moves the camera to the specified camera point over the specified number of ticks. |
| `0xc16c0` | 247 | `camera_set_relative` | `hs_evaluate_camera_set_relative` | moves the camera to the specified camera point over the specified number of ticks (position is relative to the specified object) |
| `0xc1700` | 248 | `camera_set_animation` | `hs_evaluate_camera_set_animation` | begins a prerecorded camera animation. |
| `0xc1740` | 249 | `camera_set_first_person` | `hs_evaluate_camera_set_first_person` | makes the scripted camera follow a unit. |
| `0xc1780` | 250 | `camera_set_dead` | `hs_evaluate_camera_set_dead` | makes the scripted camera zoom out around a unit as if it were dead. |
| `0xc17c0` | 251 | `camera_time` | `hs_evaluate_camera_time` | returns the number of ticks remaining in the current camera interpolation. |
| `0xc17f0` | 253 | `debug_camera_save` | `hs_evaluate_debug_camera_save` | saves the camera position and facing. |
| `0xc1810` | 252 | `debug_camera_load` | `hs_evaluate_debug_camera_load` | loads the saved camera position and facing. |
| `0xc1830` | 254 | `game_speed` | `hs_evaluate_game_speed` | changes the game speed. |
| `0xc1870` | 256 | `game_variant` | `hs_evaluate_game_variant` | set the game engine |
| `0xc18b0` | 255 | `game_time` | `hs_evaluate_game_time` | gets ticks elapsed since the start of the game. |
| `0xc18d0` | 257 | `game_difficulty_get` | `hs_evaluate_game_difficulty_get` | returns the current difficulty setting, but lies to you and will never return easy, instead returning normal |
| `0xc1900` | 258 | `game_difficulty_get_real` | `hs_evaluate_game_difficulty_get_real` | returns the actual current difficulty setting without lying |
| `0xc1cf0` | 259 | `map_reset` | `hs_evaluate_map_reset` | starts the map from the beginning. |
| `0xc1d10` | 260 | `map_name` | `hs_evaluate_map_name` | changes the name of the solo player map. |
| `0xc1d50` | 261 | `multiplayer_map_name` | `hs_evaluate_multiplayer_map_name` | changes the name of the multiplayer map |
| `0xc1d90` | 262 | `game_difficulty_set` | `hs_evaluate_game_difficulty_set` | changes the difficulty setting for the next map to be loaded. |
| `0xc1dd0` | 263 | `crash` | `hs_evaluate_crash` | crashes (for debugging). |
| `0xc1e10` | 264 | `switch_bsp` | `hs_evaluate_switch_bsp` | takes off your condom and changes to a different structure bsp |
| `0xc1e50` | 265 | `structure_bsp_index` | `hs_evaluate_structure_bsp_index` | returns the current structure bsp index |
| `0xc1e80` | 266 | `version` | `hs_evaluate_version` | prints the build version. |
| `0xc1ea0` | 267 | `playback` | `hs_evaluate_playback` | starts game in film playback mode |
| `0xc1ec0` | 268 | `texture_cache_flush` | `hs_evaluate_texture_cache_flush` | don't make me kick your ass |

---

## 3. Technical Breakdown & Functional Analysis

All 24 functions implement the standard HaloScript dispatch evaluator ABI:
```c
void hs_evaluate_<name>(int16_t function_index, int thread_datum, char init);
```
Parameter arguments passed to scripts are unpacked via `hs_macro_function_evaluate(function_index, thread_datum, init)` returning a pointer to the evaluated argument buffer, or directly invoke engine subroutines when parameterless. Values are returned to the running script thread via `hs_return(thread_datum, value)`.

### Group 1: Director Camera Script Control (`0xc1640`–`0xc17c0`)
- **`hs_evaluate_camera_control` (`0xc1640`)**: Unpacks boolean argument `result[0]` and invokes `director_script_camera(result[0])` (`0x86cb0`) to grant or revoke script authority over the active viewpoint.
- **`hs_evaluate_camera_set` (`0xc1680`)**: Unpacks camera point index `result[0]` and duration ticks `result[2]`, calling `director_script_camera_set` (`0x85260`) to smoothly interpolate the camera transform.
- **`hs_evaluate_camera_set_relative` (`0xc16c0`)**: Unpacks camera point index `result[0]`, interpolation ticks `result[2]`, and parent object datum handle `result[4]`, calling `FUN_00085180` (`0x85180`) to position the camera relative to an moving object.
- **`hs_evaluate_camera_set_animation` (`0xc1700`)**: Unpacks animation reference identifiers `result[0]` and `result[1]`, calling `FUN_00085150` (`0x85150`) to trigger recorded camera path animations.
- **`hs_evaluate_camera_set_first_person` (`0xc1740`)**: Unpacks target unit object handle `result[0]` and calls `FUN_000850d0` (`0x850d0`) to attach the camera directly to a unit's first-person node.
- **`hs_evaluate_camera_set_dead` (`0xc1780`)**: Unpacks target unit object handle `result[0]` and calls `FUN_00085110` (`0x85110`) to initiate death-cam orbit/zoom mechanics.
- **`hs_evaluate_camera_time` (`0xc17c0`)**: Queries interpolation subsystem via `FUN_000853a0()` (`0x853a0`) and returns remaining camera interpolation ticks to the HS thread.

### Group 2: Debug Camera Persistence (`0xc17f0`–`0xc1810`)
- **`hs_evaluate_debug_camera_save` (`0xc17f0`)**: Invokes `director_save_camera()` (`0x86360`) to store the current free camera position and orientation into global debug state.
- **`hs_evaluate_debug_camera_load` (`0xc1810`)**: Invokes `director_load_camera()` (`0x86900`) to restore the saved free camera transform.

### Group 3: Game Simulation Time & Variant Control (`0xc1830`–`0xc18b0`)
- **`hs_evaluate_game_speed` (`0xc1830`)**: Unpacks float multiplier `result[0]` and invokes `game_time_set_speed(speed)` (`0xb5d00`) to adjust game tick simulation rate.
- **`hs_evaluate_game_variant` (`0xc1870`)**: Unpacks string pointer `result[0]` and invokes `game_set_game_variant_from_name(name)` (`0xa78e0`) to configure active multiplayer game rules.
- **`hs_evaluate_game_time` (`0xc18b0`)**: Calls `game_time_get()` (`0xb5aa0`) and returns elapsed simulation ticks to the script thread.

### Group 4: Difficulty Query & Configuration (`0xc18d0`–`0xc1900`, `0xc1d90`)
- **`hs_evaluate_game_difficulty_get` (`0xc18d0`)**: Invokes `game_difficulty_level_get_ignore_easy()` (`0xa7480`), which maps "Easy" difficulty up to "Normal" (a classic Bungie Easter egg behavior noted in the help string). Preserves zero-extension of the 16-bit return value into 32-bit `value` slot.
- **`hs_evaluate_game_difficulty_get_real` (`0xc1900`)**: Invokes `game_difficulty_level_get()` (`0xa7460`), returning the unmasked authentic difficulty level (0=Easy, 1=Normal, 2=Heroic, 3=Legendary). Zero-extended into 32 bits.
- **`hs_evaluate_game_difficulty_set` (`0xc1d90`)**: Unpacks target difficulty short `*(short *)result` and invokes `main_set_difficulty(diff)` (`0x1003d0`) for subsequent map loads.

### Group 5: Map Management & Engine Crash (`0xc1cf0`–`0xc1dd0`)
- **`hs_evaluate_map_reset` (`0xc1cf0`)**: Invokes `main_reset_map()` (`0x1005b0`) to restart current map state from the initial spawn definition.
- **`hs_evaluate_map_name` (`0xc1d10`)**: Unpacks single-player map scenario string pointer `result[0]` and calls `main_set_map_name(name)` (`0x100370`).
- **`hs_evaluate_multiplayer_map_name` (`0xc1d50`)**: Unpacks multiplayer map scenario string pointer `result[0]` and calls `main_set_multiplayer_map_name(name)` (`0x1003a0`).
- **`hs_evaluate_crash` (`0xc1dd0`)**: Unpacks debug reason string pointer `*result` and passes it to `main_crash(reason)` (`0x101cb0`), deliberately triggering an unhandled exception for script debugging.

### Group 6: Structure BSP Operations (`0xc1e10`–`0xc1e50`)
- **`hs_evaluate_switch_bsp` (`0xc1e10`)**: Unpacks target structure BSP index `*(short *)result` and calls `scenario_switch_structure_bsp(index)` (`0x18eb40`) to stream in the requested environment cluster.
- **`hs_evaluate_structure_bsp_index` (`0xc1e50`)**: Invokes `global_structure_bsp_index_get()` (`0x18f080`) and returns the zero-extended 16-bit BSP cluster index to the script thread.

### Group 7: Engine Diagnostics, Playback & Cache (`0xc1e80`–`0xc1ec0`)
- **`hs_evaluate_version` (`0xc1e80`)**: Calls `main_print_version()` (`0x101cc0`) to emit the engine build identifier and version banner to console output.
- **`hs_evaluate_playback` (`0xc1ea0`)**: Calls `main_set_game_connection_to_film_playback()` (`0x1006e0`) to configure network transport to read recorded film inputs.
- **`hs_evaluate_texture_cache_flush` (`0xc1ec0`)**: Calls `texture_cache_flush()` (`0x1bed30`) to purge all cached GPU texture allocations.

---

## 4. Verification & Byte-Matching Evidence

1. **ABI Rigidity & Zero Drift:**
   Running `python3 tools/audit/extract_reg_args.py --check` confirms **886 OK, 0 drift, 0 missing, 0 stale**. Register conventions are immutable.
2. **Deactivation Policy Gate:**
   Running `python3 tools/audit/check_ported_deactivations.py --check` confirms all 46 active deactivations match `deactivation_allowlist.json` with 0 unallowlisted regressions.
3. **Disassembly & Codegen Invariance:**
   Disassembly verification via `llvm-objdump -d` on `build/CMakeFiles/halo.dir/src/halo/hs/hs.c.obj`:
   - Functions preserving zero-extension (`hs_evaluate_game_difficulty_get`, `hs_evaluate_game_difficulty_get_real`, `hs_evaluate_structure_bsp_index`) emit identical `movzwl %ax, %eax` zero-extension operations without sign corruption.
   - Dual-call wrappers preserve MSVC coalesced stack adjustment `add esp, 0x8` or `add esp, 0xc`.
4. **Linker Export Verification:**
   All 24 functions resolve cleanly as global text symbols in `hs.c.obj` (`llvm-nm` confirmed 24 `T` exports).
5. **Zero Collateral Diffs:**
   `git diff --stat -- src/ kb.json CMakeLists.txt` confirms exactly 48 lines modified across `hs.c` and `kb.json` (24 renames each), with zero unrelated formatting modifications.
