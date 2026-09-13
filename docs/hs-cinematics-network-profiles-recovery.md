# HaloScript Evaluators Recovery: Batch 6 (Cinematics, Screen Effects, Network, Profiles & Settings)

## Overview
- **Binary Target**: Halo CE Xbox debug build 2276 (`halo-patched/cachebeta.xbe`, MD5 `c7869590a1c64ad034e49a5ee0c02465`).
- **Object**: `hs.obj` (addresses `0xc3620`–`0xc3a10`, table indices 399–417).
- **Scope**: 19 HaloScript evaluator functions recovering cinematic screen effects, near clip planes, memory unit enumeration, save game deletion, fast network server setup, campaign level unlocks, player 0 input/look controls, UI debug paths, scenario help dialogs, and Xbox machine name configuration.
- **Decompilation Oracle**: Verified byte-for-byte against pristine assembly decompiled with `kuna` (`/data/data/com.termux/files/home/bin/kuna`).
- **Standards**: Strict C89 compliance, explicit terminal `return;` on all void functions, uniform `(int16_t function_index, int thread_datum, char init)` signature, and zero collateral register drift.

## Recovered Functions Summary

| Address | Function Name | HS Builtin Name | Table Index | Return Type | Description |
|---|---|---|---|---|---|
| `0xc3620` | `hs_evaluate_script_screen_effect_set_value` | `script_screen_effect_set_value` | 399 | void | Sets scripted screen effect scalar parameters via `FUN_0017d9a0`. |
| `0xc3660` | `hs_evaluate_cinematic_screen_effect_start` | `cinematic_screen_effect_start` | 400 | void | Starts cinematic screen effect sprite pass via `FUN_0017da00`. |
| `0xc36a0` | `hs_evaluate_cinematic_screen_effect_set_convolution` | `cinematic_screen_effect_set_convolution` | 401 | void | Sets convolution filter weight, scale, and offset via `FUN_0017da40`. |
| `0xc3700` | `hs_evaluate_cinematic_screen_effect_set_filter` | `cinematic_screen_effect_set_filter` | 402 | void | Sets full cinematic filter parameters (gamma, tint, desaturation) via `FUN_0017dab0`. |
| `0xc3760` | `hs_evaluate_cinematic_screen_effect_set_filter_desaturation_tint` | `cinematic_screen_effect_set_filter_desaturation_tint` | 403 | void | Sets cinematic filter desaturation tint rgb via `FUN_0017db20`. |
| `0xc37b0` | `hs_evaluate_cinematic_screen_effect_set_video` | `cinematic_screen_effect_set_video` | 404 | void | Sets video playback screen effect parameters via `rasterizer_screen_effect_set_video`. |
| `0xc37f0` | `hs_evaluate_cinematic_screen_effect_stop` | `cinematic_screen_effect_stop` | 405 | void | Stops active cinematic screen effect via `FUN_0017dc60`. |
| `0xc3810` | `hs_evaluate_cinematic_set_near_clip_distance` | `cinematic_set_near_clip_distance` | 406 | void | Adjusts camera near clipping plane distance via `FUN_0017dec0`. |
| `0xc3850` | `hs_evaluate_enumerate_memory_units` | `enumerate_memory_units` | 407 | void | Enumerates connected Xbox memory units via `FUN_001c58f0`. |
| `0xc3870` | `hs_evaluate_delete_save_game_files` | `delete_save_game_files` | 408 | void | Deletes save game files via `FUN_001c4f30`. |
| `0xc3890` | `hs_evaluate_fast_setup_network_server` | `fast_setup_network_server` | 409 | void | Quickly configures and boots network host server via `player_ui_fast_setup_network_server`. |
| `0xc38b0` | `hs_evaluate_profile_unlock_solo_levels` | `profile_unlock_solo_levels` | 410 | void | Unlocks all solo campaign mission levels in player profile via `player_ui_activate_all_solo_levels`. |
| `0xc38d0` | `hs_evaluate_player0_look_invert_pitch` | `player0_look_invert_pitch` | 411 | void | Sets pitch inversion toggle for player 0 look controls via `FUN_000e1770`. |
| `0xc3910` | `hs_evaluate_player0_look_pitch_is_inverted` | `player0_look_pitch_is_inverted` | 412 | boolean | Queries whether player 0 look pitch is currently inverted via `player0_look_pitch_is_inverted`. |
| `0xc3940` | `hs_evaluate_player0_joystick_set_is_normal` | `player0_joystick_set_is_normal` | 413 | boolean | Queries whether player 0 joystick layout is set to default/normal via `FUN_000e1060`. |
| `0xc3970` | `hs_evaluate_ui_widget_show_path` | `ui_widget_show_path` | 414 | void | Enables widget hierarchy path display debugging via `ui_widget_debug_show_path`. |
| `0xc39b0` | `hs_evaluate_display_scenario_help` | `display_scenario_help` | 415 | void | Pops scenario mission help dialog on screen via `ui_widget_display_scenario_help`. |
| `0xc39f0` | `hs_evaluate_network_game_start_now` | `network_game_start_now` | 416 | void | Starts pending network multiplayer game immediately via `FUN_0012a7a0`. |
| `0xc3a10` | `hs_evaluate_xbox_set_machine_name` | `xbox_set_machine_name` | 417 | void | Configures Xbox console machine/host name via `xbox_set_machine_name`. |

## Key Technical Details
1. **Boolean Widening / Zero-Extension Idioms (`0xc3910`, `0xc3940`)**:
   - Kuna disassembly proves MSVC initializes the 4-byte stack slot to zero (`mov [ebp-4], 0`) before writing the returned byte into the low byte (`mov [ebp-4], al`).
   - Reconstructed faithfully using zero-initialized local dword with byte cast write `*(unsigned char *)&result = predicate(); hs_return(thread_datum, result);` to guarantee correct zero-extension without emitting erroneous `movsx`.
2. **Terminal Explicit Returns**:
   - Verified that every lifted void handler terminates with an explicit `return;`.
