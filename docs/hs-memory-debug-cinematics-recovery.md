# HaloScript Evaluators Recovery: Batch 7 (Memory, Profiler, Radiosity, AI Debug, and Cinematics)

## Overview
- **Binary Target**: Halo CE Xbox debug build 2276 (`halo-patched/cachebeta.xbe`, MD5 `c7869590a1c64ad034e49a5ee0c02465`).
- **Object**: `hs.obj` (addresses `0xc1ee0`–`0xc24c0`, table indices 269–299).
- **Scope**: 31 HaloScript evaluator functions recovering memory tracking, cache flushes, performance profiler metrics/graphs, radiosity lightmapping controls, AI debugging tools, and cinematic cutscene orchestration.
- **Decompilation Oracle**: Verified byte-for-byte against pristine assembly decompiled with `kuna` (`/data/data/com.termux/files/home/bin/kuna`).
- **Standards**: Strict C89 compliance, explicit terminal `return;` on all void functions, uniform `(int16_t function_index, int thread_datum, char init)` signature, and zero collateral register drift.

## Recovered Functions Summary

| Address | Function Name | HS Builtin Name | Table Index | Return Type | Description |
|---|---|---|---|---|---|
| `0xc1ee0` | `hs_evaluate_sound_cache_flush` | `sound_cache_flush` | 269 | void | Unconditionally flushes audio cache via `sound_cache_flush` (`0x1be490`). |
| `0xc1f00` | `hs_evaluate_debug_memory` | `debug_memory` | 270 | void | Dumps memory debug state via `FUN_0008f1e0`. |
| `0xc1f20` | `hs_evaluate_debug_memory_by_file` | `debug_memory_by_file` | 271 | void | Dumps file-aggregated memory debug stats via `FUN_0008f210`. |
| `0xc1f40` | `hs_evaluate_debug_memory_for_file` | `debug_memory_for_file` | 272 | void | Dumps memory allocated by specific file via `FUN_0008f240`. |
| `0xc1f80` | `hs_evaluate_debug_tags` | `debug_tags` | 273 | void | Dumps tag allocation debug information via `tag_files_dump_allocation_stats` (`0x110280`). |
| `0xc1fa0` | `hs_evaluate_profile_reset` | `profile_reset` | 274 | void | Resets performance profiler counters via `profile_reset` (`0x8eb30`). |
| `0xc1fc0` | `hs_evaluate_profile_dump` | `profile_dump` | 275 | void | Dumps performance profiler logs/metrics via `profile_dump` (`0x8eb40`). |
| `0xc2000` | `hs_evaluate_profile_activate` | `profile_activate` | 276 | void | Activates profiler sampling channel via `profile_activate` (`0x8ee80`). |
| `0xc2040` | `hs_evaluate_profile_deactivate` | `profile_deactivate` | 277 | void | Deactivates profiler sampling channel via `profile_deactivate` (`0x8eea0`). |
| `0xc2080` | `hs_evaluate_profile_graph_toggle` | `profile_graph` | 278 | void | Toggles profile visual telemetry graph via `profile_graph_toggle` (`0x8eed0`). |
| `0xc20c0` | `hs_evaluate_debug_pvs` | `debug_pvs` | 279 | void | Toggles PVS (potentially visible set) debug rendering via `debug_pvs_toggle` (`0x18e7e0`). |
| `0xc2100` | `hs_evaluate_radiosity_start` | `radiosity_start` | 280 | void | Starts lightmap radiosity calculation solver via `radiosity_start` (`0x19a0a0`). |
| `0xc2120` | `hs_evaluate_radiosity_save` | `radiosity_save` | 281 | void | Saves computed radiosity solution via `radiosity_save` (`0x19a0b0`). |
| `0xc2140` | `hs_evaluate_radiosity_debug_point` | `radiosity_debug_point` | 282 | void | Samples radiosity debug point probe via `radiosity_debug_point` (`0x19a0c0`). |
| `0xc2160` | `hs_evaluate_ai_lines` | `ai_lines` | 283 | void | Toggles AI navigation & behavior debug lines via `ai_debug_toggle_lines` (`0x163a0`). |
| `0xc2180` | `hs_evaluate_ai_debug_sound_point_set` | `ai_sound` | 284 | void | Toggles AI sound point debug visualization via `ai_debug_sound_point_set` (`0x163b0`). |
| `0xc21a0` | `hs_evaluate_ai_debug_vocalize` | `ai_vocalize` | 285 | void | Triggers AI debug vocalization line via `ai_debug_vocalize` (`0x165c0`). |
| `0xc21e0` | `hs_evaluate_ai_debug_teleport_to` | `ai_teleport_to` | 286 | void | Teleports AI actor to destination point via `ai_debug_teleport_to` (`0x16860`). |
| `0xc2220` | `hs_evaluate_ai_debug_speak` | `ai_speak` | 287 | void | Triggers AI debug speech utterance via `ai_debug_speak` (`0x16600`). |
| `0xc2260` | `hs_evaluate_ai_debug_speak_list` | `ai_speak_list` | 288 | void | Queues speech sequence from AI dialogue list via `ai_debug_speak_list` (`0x16630`). |
| `0xc22a0` | `hs_evaluate_fade_in` | `fade_in` | 289 | void | Triggers screen fade-in transition via `player_effect_screen_fade_in` (`0xa2970`). |
| `0xc22f0` | `hs_evaluate_fade_out` | `fade_out` | 290 | void | Triggers screen fade-out transition via `player_effect_screen_fade_out` (`0xa29c0`). |
| `0xc2340` | `hs_evaluate_cinematic_start` | `cinematic_start` | 291 | void | Begins cutscene cinematic playback mode via `cinematic_start` (`0x92e20`). |
| `0xc2360` | `hs_evaluate_cinematic_stop` | `cinematic_stop` | 292 | void | Halts active cutscene cinematic playback via `cinematic_stop` (`0x93050`). |
| `0xc2380` | `hs_evaluate_cinematic_skip_start_internal` | `cinematic_skip_start_internal` | 293 | void | Arms cinematic skip window trigger via `cinematic_skip_start` (`0x92e70`). |
| `0xc23a0` | `hs_evaluate_cinematic_skip_stop_internal` | `cinematic_skip_stop_internal` | 294 | void | Disarms cinematic skip window trigger via `cinematic_skip_stop` (`0x92e80`). |
| `0xc23c0` | `hs_evaluate_cinematic_show_letterbox` | `cinematic_show_letterbox` | 295 | void | Toggles cinematic aspect-ratio letterboxing bars via `cinematic_show_letterbox` (`0x92e90`). |
| `0xc2400` | `hs_evaluate_cinematic_set_title` | `cinematic_set_title` | 296 | void | Displays cinematic cutscene title chapter name via `FUN_00093640` (`0x93640`). |
| `0xc2440` | `hs_evaluate_cinematic_set_title_delayed` | `cinematic_set_title_delayed` | 297 | void | Displays cinematic title chapter after delay in seconds via `cinematic_set_title_delayed` (`0x930b0`). |
| `0xc2480` | `hs_evaluate_cinematic_suppress_bsp_object_creation` | `cinematic_suppress_bsp_object_creation` | 298 | void | Suppresses automatic BSP object instantiation during cinematics via `cinematic_suppress_bsp_object_creation` (`0x93030`). |
| `0xc24c0` | `hs_evaluate_attract_mode_start` | `attract_mode_start` | 299 | void | Starts title attract mode loop via `event_manager_tab_process` (`0xdc140`). |

## Key Technical Details
1. **Float & Argument-Slot Recovery (`0xc22a0`, `0xc22f0`, `0xc2440`)**:
   - Kuna disassembly revealed that `0xc22a0` and `0xc22f0` pass four arguments to `player_effect_screen_fade_in` and `player_effect_screen_fade_out` (including two float parameters loaded via `FLD` and stored via `FSTP` into stack slots reserved by dummy `PUSH` instructions).
   - In `0xc2440`, `cinematic_set_title_delayed` takes two parameters: a 16-bit title identifier and a single-precision float delay.
2. **Terminal Explicit Returns**:
   - Verified that every lifted void handler terminates with an explicit `return;`.
