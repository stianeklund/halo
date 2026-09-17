# Batch 4.1: HaloScript Core & Evaluator Recovery Report

## Executive Summary

- **Batch Target:** Batch 4.1: Complete HaloScript Core and Evaluator Decompilation (Phase 4).
- **Functions Recovered:** 36 functions across `hs.obj`, `hs_compile.obj`, and `hs_runtime.obj`.
- **Total Code Recovered:** 3,834 bytes of lifted C89 engine logic.
- **Verification Status:**
  - `extract_reg_args.py --check`: **PASS** (917 OK, 0 drift, 0 missing, 0 stale).
  - `check_param_types.py --check`: **PASS** (0 new type mismatches, 0 new errors).
  - `check_lift_hazards.py --changed-only`: **PASS** (zero unverified hazards; investigated duplicate argument warnings in `0xcbb40` and confirmed identical in-place transform logic in the original binary).
  - `patched_xbe` build: **PASS** (clean compilation and linking with MSVC / Clang + LLD into `default.xbe`).

---

## Detailed Function Inventory

### 1. `src/halo/hs/hs.c` (16 functions, 1,061 bytes)

| Address | Size (Bytes) | Name | Signature | Description |
|---|---|---|---|---|
| `0x0c3c40` | 8 | `hs_recompile` | `void hs_recompile(void);` | Recompilation flag trigger (`byte_005aa699 = 1`). |
| `0x0c4240` | 36 | `hs_enumerate_script_names` | `void hs_enumerate_script_names(void);` | Enumerator callback for scenario scripts. |
| `0x0c4270` | 161 | `hs_enumerate_variable_names` | `void hs_enumerate_variable_names(void);` | Enumerator for global script variables and scenario variables with type names. |
| `0x0c4320` | 39 | `hs_enumerate_ai_names` | `void hs_enumerate_ai_names(void);` | Enumerator for AI encounters and squad groups. |
| `0x0c4350` | 36 | `hs_enumerate_ai_command_list_names` | `void hs_enumerate_ai_command_list_names(void);` | Enumerator for AI script command lists. |
| `0x0c4380` | 36 | `hs_enumerate_starting_profile_names` | `void hs_enumerate_starting_profile_names(void);` | Enumerator for player starting profiles. |
| `0x0c43b0` | 36 | `hs_enumerate_conversation_names` | `void hs_enumerate_conversation_names(void);` | Enumerator for scenario AI conversations. |
| `0x0c43e0` | 36 | `hs_enumerate_object_names` | `void hs_enumerate_object_names(void);` | Enumerator for scenario object names. |
| `0x0c4410` | 36 | `hs_enumerate_trigger_volume_names` | `void hs_enumerate_trigger_volume_names(void);` | Enumerator for scenario trigger volumes. |
| `0x0c4440` | 36 | `hs_enumerate_cutscene_flag_names` | `void hs_enumerate_cutscene_flag_names(void);` | Enumerator for scenario cutscene flags. |
| `0x0c4470` | 36 | `hs_enumerate_cutscene_camera_point_names` | `void hs_enumerate_cutscene_camera_point_names(void);` | Enumerator for cutscene camera points. |
| `0x0c44a0` | 36 | `hs_enumerate_cutscene_title_names` | `void hs_enumerate_cutscene_title_names(void);` | Enumerator for cutscene chapter titles. |
| `0x0c44d0` | 36 | `hs_enumerate_cutscene_recording_names` | `void hs_enumerate_cutscene_recording_names(void);` | Enumerator for cutscene recordings. |
| `0x0c4500` | 54 | `hs_enumerate_navpoints` | `void hs_enumerate_navpoints(void);` | Enumerator for scenario HUD navpoints. |
| `0x0c4540` | 54 | `hs_enumerate_hud_messages` | `void hs_enumerate_hud_messages(void);` | Enumerator for scenario HUD message entries. |
| `0x0c4f90` | 81 | `hs_hack` | `void hs_hack(void);` | Debug scenario script compilation helper. |

### 2. `src/halo/hs/hs_compile.c` (1 function, 30 bytes)

| Address | Size (Bytes) | Name | Signature | Description |
|---|---|---|---|---|
| `0x0c5820` | 30 | `character_in_list` | `boolean character_in_list(char c, const char *list, int16_t count);` | Character set membership test helper for tokenizer. |

### 3. `src/halo/hs/hs_runtime.c` (19 functions, 2,743 bytes)

| Address | Size (Bytes) | Name | Signature | Description |
|---|---|---|---|---|
| `0x0c8e00` | 191 | `hs_parse_inspect` | `bool hs_parse_inspect(int function_index, int expression_index);` | Parser/type-checker for inspect expressions. |
| `0x0c8ec0` | 119 | `hs_parse_object_cast_up` | `bool hs_parse_object_cast_up(int function_index, int expression_index);` | Parser/type-checker for object hierarchy up-casts. |
| `0x0c97f0` | 70 | `hs_unit_can_see_flag` | `boolean hs_unit_can_see_flag(int unit_handle, int16_t flag_index, real angle);` | Line-of-sight test from unit to cutscene flag. |
| `0x0ca010` | 29 | `FUN_000ca010` | `real FUN_000ca010(const char *sound_name);` | Sound playback parameter lookup/getter. |
| `0x0ca030` | 24 | `FUN_000ca030` | `void FUN_000ca030(const char *sound_name, real value);` | Sound playback parameter setter. |
| `0x0ca140` | 23 | `FUN_000ca140` | `void FUN_000ca140(const char *substring);` | HUD message string search and display trigger. |
| `0x0ca160` | 654 | `FUN_000ca160` | `void FUN_000ca160(int object_handle@<ebx>, int param_2, char param_3, char param_4);` | Cutscene flag teleportation & camera positioning for units/players. |
| `0x0ca410` | 26 | `FUN_000ca410` | `void FUN_000ca410(int a, int b);` | Debug camera point forwarder. |
| `0x0ca4b0` | 39 | `hs_syntax_nth` | `int hs_syntax_nth(int node_index@<eax>, int16_t count@<cx>);` | Traverses the Nth sibling in an HS syntax tree. |
| `0x0ca880` | 13 | `hs_runtime_dispose` | `void hs_runtime_dispose(void);` | Clears runtime thread data pointers. |
| `0x0cacf0` | 175 | `FUN_000cacf0` | `void FUN_000cacf0(int thread_handle@<edi>);` | Disposes an HS execution thread and frees allocated structures. |
| `0x0cae00` | 113 | `FUN_000cae00` | `int FUN_000cae00(const char *script_name@<edi>);` | Resolves script name to scenario script index. |
| `0x0caec0` | 32 | `hs_string_to_boolean` | `int hs_string_to_boolean(const char *string);` | Parses boolean string literal ("true"/"false"/"0"/"1"). |
| `0x0caee0` | 3 | `hs_data_to_void` | `int hs_data_to_void(void);` | No-op type coercion returning 0. |
| `0x0caf70` | 16 | `hs_long_to_short` | `int hs_long_to_short(int value);` | Safe signed 32-bit to 16-bit integer clamp/cast. |
| `0x0cb940` | 63 | `script_error` | `boolean script_error(int thread_handle, const char *reason, const char *details);` | Emits formatted script runtime error message. |
| `0x0cb9c0` | 376 | `render_debug_scripting` | `void render_debug_scripting(void);` | Renders on-screen debug text for running HS threads. |
| `0x0cbb40` | 1080 | `render_debug_trigger_volumes` | `void render_debug_trigger_volumes(void);` | Draws 3D trigger volume wireframes and hulls with camera occlusion. |
| `0x0ce1b0` | 1 | `FUN_000ce1b0` | `void FUN_000ce1b0(void);` | Empty stub function. |

---

## Architectural & ABI Insights

1. **Register Calling Conventions & Baseline Immutability:**
   - 4 functions strictly adhere to the `@<reg>` register ABI baseline:
     - `0xca160`: `void FUN_000ca160(int object_handle@<ebx>, int param_2, char param_3, char param_4);`
     - `0xca4b0`: `int hs_syntax_nth(int node_index@<eax>, int16_t count@<cx>);`
     - `0xcacf0`: `void FUN_000cacf0(int thread_handle@<edi>);`
     - `0xcae00`: `int FUN_000cae00(const char *script_name@<edi>);`
   - Verification via `extract_reg_args.py --check` confirms 0 drift against `kb_reg_baseline.json`.

2. **In-Place Vector Transformations:**
   - In `render_debug_trigger_volumes` (`0xcbb40`), `matrix_scale_transform_vector` is called with identical source and destination pointers (`(float *)&v1, (float *)&v1`). Disassembly of pristine binary confirmed MSVC generated:
     ```asm
     0xcbda0: call 0x109610 ; matrix_scale_transform_vector(transform, &v1, &v1)
     0xcbdb4: call 0x109610 ; matrix_scale_transform_vector(transform, &v2, &v2)
     ```
   - This validates the intentional in-place transformation semantic in Bungie's trigger volume debug rendering.

3. **Authentic Bungie Assertion Paths:**
   - Exact source paths and line numbers recovered:
     - Cutscene flags: `"c:\\halo\\SOURCE\\hs\\hs_library_external.c"`, lines 460 (`0x1cc`), 479 (`0x1df`).
     - Script compilation: `"c:\\halo\\SOURCE\\hs\\hs_library_internal_compile.h"`, lines 605 (`0x25d`), 653 (`0x28d`), 666 (`0x29a`).
     - Trigger volume rendering: `"c:\\halo\\SOURCE\\hs\\hs_runtime.c"`, line 531 (`0x213`).
     - Variable enumeration: `"c:\\halo\\SOURCE\\hs\\hs.c"`, line 576 (`0x240`).

4. **Producer / Consumer Interface Typing:**
   - Synchronized `FUN_000ca010` and `FUN_000ca030` (`const char *sound_name`) with call sites in `src/halo/game/players.c` (`FUN_000be5e0` and `FUN_000be620`), ensuring seamless type alignment without implicit integer-to-pointer warnings.
