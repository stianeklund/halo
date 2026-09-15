# Batch 1.2B Function Name Recovery Report: Units and Bipeds Subsystems

**Date:** September 14, 2026  
**Target Build:** Halo Xbox Debug Build 2276 (`01.10.12.2276`, Oct 12, 2001)  
**Binary Reference:** `halo-patched/cachebeta.xbe` (MD5: `c7869590a1c64ad034e49a5ee0c02465`)  
**Scope:** Sub-Batch 1.2B (`units.obj` & `bipeds.obj`)  
**Total Functions Recovered:** 80 (63 in `units.obj`, 17 in `bipeds.obj`)  

---

## Executive Summary

Sub-Batch 1.2B recovers 80 authentic Bungie debug symbol names across the player, AI unit, and biped animation/physics subsystems (`units.obj` and `bipeds.obj`). All symbol names are directly verified against Bungie debug map evidence in `halo_2276_functions.txt`.

This recovery replaces 80 legacy placeholders (`FUN_001a03c0` through `FUN_001b3690`) with canonical Bungie symbols and resolves seven critical historical naming collisions across `units.c`, `bipeds.c`, `model_animations.c`, `cseries.c`, and AI subsystems.

All 33 custom register argument functions preserve their exact register pinning with **0 ABI drift across 886 tracked functions** in `tools/kb_reg_baseline.json`, zero hazard scan blockers, and clean compilation linking into `halo.xbe`.

---

## Collision Investigations & Resolutions

Seven structural naming collisions were identified, investigated against binary disassembly, and resolved:

### 1. `0x8dd30` vs `0x8dc30` (`csstrncat` vs `csstrcat`)
- **Prior State:** `0x8dd30` in `cseries.obj` was misnamed `csstrcat`, taking 3 arguments (`destination, source, max_size`), colliding with `0x8dc30` in `units.obj` (`csstrcat(destination, source)`).
- **Binary Evidence:**
  - `halo_2276_functions.txt` line 1554: `_csstrcat .text 0008DC30` (2 params).
  - `halo_2276_functions.txt` line 1556: `_csstrncat .text 0008DD30` (3 params with bounded size).
- **Resolution:**
  - `0x8dd30` renamed to authentic `csstrncat` in `kb.json`, `tools/kb_reg_baseline.json`, `cseries.c`, `console.c`, and `shell.c`.
  - `0x8dc30` retains authentic `csstrcat`.

### 2. `0x121940` vs `0x122a50` (`animation_get_keyframe_scale` vs `overlay_animation_apply_continuous_scaled`)
- **Prior State:** `0x121940` was misnamed `overlay_animation_apply_continuous_scaled` in `model_animations.c`, causing an internal recursive self-call mismatch in `units.c:293`.
- **Binary Evidence:**
  - `halo_2276_functions.txt` line 4723: `_animation_get_keyframe_scale .text 00121940`.
  - `halo_2276_functions.txt` line 4730: `_overlay_animation_apply_continuous_scaled .text 00122A50`.
- **Resolution:**
  - `0x121940` renamed to authentic `animation_get_keyframe_scale`.
  - `0x122a50` renamed to authentic `overlay_animation_apply_continuous_scaled`.
  - Updated call site at `units.c:293` to call `animation_get_keyframe_scale`.

### 3. `0x123e20` vs `0x123470` (`model_get_default_inverse_matrix` vs `animation_get_root_matrix`)
- **Prior State:** `0x123e20` in `model_animations.c` was misnamed `animation_get_root_matrix`, colliding with `0x123470` in `units.obj`.
- **Binary Evidence:**
  - `halo_2276_functions.txt` line 4740: `_model_get_default_inverse_matrix .text 00123E20`.
  - `halo_2276_functions.txt` line 4732: `_animation_get_root_matrix .text 00123470`.
- **Resolution:**
  - `0x123e20` renamed to `model_get_default_inverse_matrix`.
  - `0x123470` renamed to `animation_get_root_matrix`.

### 4. `0x1a9900` vs `0x1a9ec0` (`unit_get_aiming_vector` vs `unit_scripting_unit_driver`)
- **Prior State:** `0x1a9900` was named `unit_scripting_unit_driver(int unit_handle, void *out_aiming)`, colliding with the actual HaloScript command at `0x1a9ec0`.
- **Binary Evidence:**
  - `halo_2276_functions.txt` line 6848: `_unit_get_aiming_vector .text 001A9900`.
  - `halo_2276_functions.txt` line 6858: `_unit_scripting_unit_driver .text 001A9EC0`.
- **Resolution:**
  - `0x1a9900` renamed to `unit_get_aiming_vector`.
  - `0x1a9ec0` renamed to `unit_scripting_unit_driver`.
  - Updated all callers in `actors.c`, `actor_looking.c`, `actor_combat.c`, `hud.c`, and `units.c`.

### 5. `0x1a9930` vs `0x1a9ef0` (`unit_get_looking_vector` vs `unit_scripting_unit_gunner`)
- **Prior State:** `0x1a9930` was named `unit_scripting_unit_gunner(int unit_handle, void *out_looking)`, colliding with the HaloScript command at `0x1a9ef0`.
- **Binary Evidence:**
  - `halo_2276_functions.txt` line 6849: `_unit_get_looking_vector .text 001A9930`.
  - `halo_2276_functions.txt` line 6859: `_unit_scripting_unit_gunner .text 001A9EF0`.
- **Resolution:**
  - `0x1a9930` renamed to `unit_get_looking_vector`.
  - `0x1a9ef0` renamed to `unit_scripting_unit_gunner`.
  - Updated callers in `actors.c` and `units.c`.

### 6. `0x1a9960` vs `0x1aa170` (`unit_get_facing_vector` vs `units_debug_get_closest_unit`)
- **Prior State:** `0x1a9960` was named `units_debug_get_closest_unit(int unit_handle, void *out_facing)`, colliding with the nearest-biped search routine at `0x1aa170`.
- **Binary Evidence:**
  - `halo_2276_functions.txt` line 6850: `_unit_get_facing_vector .text 001A9960`.
  - `halo_2276_functions.txt` line 6862: `_units_debug_get_closest_unit .text 001AA170`.
- **Resolution:**
  - `0x1a9960` renamed to `unit_get_facing_vector`.
  - `0x1aa170` renamed to `units_debug_get_closest_unit`.
  - Updated callers in `actors.c`, `actor_looking.c`, and `units.c`.

### 7. `0x1ab940` vs `0x1adeb0` (`unit_get_weapon` vs `unit_inventory_get_weapon`)
- **Prior State:** Both were named `unit_get_weapon`. `0x1adeb0` takes `(int unit_handle, int16_t weapon_index)` and is called everywhere in game code. `0x1ab940` takes `(int16_t weapon_index@<si>, char *unit_data)`.
- **Binary Evidence:**
  - `halo_2276_functions.txt` line 6867: `_unit_get_weapon .text 001AB940`.
  - `halo_2276_functions.txt` line 6899: `_unit_inventory_get_weapon .text 001ADEB0`.
- **Resolution:**
  - `0x1ab940` renamed to authentic `unit_get_weapon`.
  - `0x1adeb0` renamed to authentic `unit_inventory_get_weapon`.
  - Updated all call sites in `actors.c`, `game_engine.c`, `players.c`, `player_control.c`, `hud.c`, and `units.c`.

---

## Register Argument Functions Preserved (33 Routines)

All 33 functions utilizing custom register conventions preserve their exact bindings with 0 drift:

| Address | Recovered Name | Annotated Signature | Registers |
|---------|----------------|---------------------|-----------|
| `0x1a03c0` | `biped_limp_noodle_adjust_orientations` | `void biped_limp_noodle_adjust_orientations(int unit_handle@<eax>, int node_count, float *positions, void *nodes@<edi>);` | EAX, EDI |
| `0x1a0a40` | `biped_bumped_object` | `void biped_bumped_object(int contact_handle@<edi>, int unit_handle@<ebx>, float *velocity_ptr);` | EDI, EBX |
| `0x1a0be0` | `biped_falling_damage` | `void biped_falling_damage(float vertical_speed, int unit_handle@<edi>);` | EDI |
| `0x1a0e00` | `biped_start_landing` | `void biped_start_landing(float threshold, int unit_handle@<eax>);` | EAX |
| `0x1a0f10` | `biped_make_footstep` | `void biped_make_footstep(int unit_handle, int param_2, short index@<bx>);` | BX |
| `0x1a1a10` | `biped_find_ground_surface` | `int biped_find_ground_surface(float scale, float *out_point, void *out_vec, float *direction@<eax>, int unit_handle@<edi>);` | EAX, EDI |
| `0x1a1fb0` | `biped_vehicle_speech` | `void biped_vehicle_speech(int unit_handle@<eax>);` | EAX |
| `0x1a2290` | `biped_jump` | `char biped_jump(int unit_handle@<edi>);` | EDI |
| `0x1a2440` | `biped_try_to_make_footsteps` | `void biped_try_to_make_footsteps(int unit_handle@<edi>);` | EDI |
| `0x1a2800` | `biped_verify_object_vectors` | `void biped_verify_object_vectors(int unit_handle@<eax>, const char *failure_kind);` | EAX |
| `0x1a2a60` | `biped_update_landing` | `void biped_update_landing(int unit_handle@<edi>, char *state);` | EDI |
| `0x1a2b90` | `biped_update_jumping` | `void biped_update_jumping(int unit_handle@<eax>);` | EAX |
| `0x1a6280` | `biped_update_dead` | `void biped_update_dead(int unit_handle@<edi>, char *state_out@<ebx>);` | EDI, EBX |
| `0x1a6e20` | `unit_lose_speech` | `void unit_lose_speech(int unit_handle@<eax>, void *speech_item@<ecx>, short priority);` | EAX, ECX |
| `0x1a7650` | `unit_find_dialogue_variant` | `int unit_find_dialogue_variant(void *tag_data@<ecx>, int dialogue_type);` | ECX |
| `0x1a7730` | `unit_dialogue_setup` | `void unit_dialogue_setup(int unit_handle@<eax>);` | EAX |
| `0x1a8550` | `unit_euler_axis_doplan` | `char unit_euler_axis_doplan(void *plan@<ecx>, float delta_time, float position, float *out_position, float velocity, float *out_velocity);` | ECX |
| `0x1a86b0` | `unit_animation_state_interruptable` | `char unit_animation_state_interruptable(void *anim_state@<ecx>, int16_t target_state@<edx>);` | ECX, EDX |
| `0x1a8730` | `unit_animation_busy` | `char unit_animation_busy(void *anim_state@<ecx>);` | ECX |
| `0x1a8770` | `unit_animation_overlay_action_loops` | `char unit_animation_overlay_action_loops(void *anim_state@<ecx>);` | ECX |
| `0x1a8790` | `unit_animation_state_loops` | `char unit_animation_state_loops(void *anim_state@<ecx>);` | ECX |
| `0x1a87f0` | `unit_animation_weapon_ik` | `char unit_animation_weapon_ik(void *anim_state@<ecx>);` | ECX |
| `0x1a8850` | `unit_animation_vehicle_ik` | `char unit_animation_vehicle_ik(void *anim_state@<ecx>);` | ECX |
| `0x1a8890` | `unit_animation_aiming_screen` | `char unit_animation_aiming_screen(void *anim_state@<ecx>);` | ECX |
| `0x1a88b0` | `unit_animation_state_get_aiming_screen_index` | `int unit_animation_state_get_aiming_screen_index(int16_t anim_state@<ecx>);` | ECX |
| `0x1a8910` | `unit_animation_state_can_be_entered_without_animation` | `char unit_animation_state_can_be_entered_without_animation(int16_t anim_state@<ecx>);` | ECX |
| `0x1a8950` | `unit_animation_compute_interpolation_frame_count` | `int unit_animation_compute_interpolation_frame_count(int16_t anim_state@<ecx>, int16_t target_state@<edx>);` | ECX, EDX |
| `0x1a8b20` | `unit_animation_start_overlay_action` | `void unit_animation_start_overlay_action(int object_handle@<eax>, int16_t state);` | EAX |
| `0x1ab6e0` | `base_seat_label_get` | `const char *base_seat_label_get(int16_t base_seat_index@<si>);` | SI |
| `0x1ab870` | `unit_animation_update` | `int16_t unit_animation_update(void *animation_state@<ecx>, int animation_graph_tag_index@<edx>, int unit_handle);` | ECX, EDX |
| `0x1ab940` | `unit_get_weapon` | `int unit_get_weapon(int16_t weapon_index@<si>, char *unit_data);` | SI |
| `0x1abd10` | `unit_melee_sound` | `void unit_melee_sound(int16_t material_type@<eax>, int unit_handle@<esi>, int weapon_tag_index@<edi>);` | EAX, ESI, EDI |
| `0x1abd90` | `unit_cause_continuous_melee_damage` | `void unit_cause_continuous_melee_damage(int unit_handle@<edi>);` | EDI |

---

## Complete Table of Recovered Functions (80)

| Address | Placeholder | Authentic Symbol (Build 2276) | Object | Reg Args |
|---------|-------------|--------------------------------|--------|----------|
| `0x8dc30` | `FUN_0008dc30` | `csstrcat` | `units.obj` | No |
| `0x122a50` | `FUN_00122a50` | `overlay_animation_apply_continuous_scaled` | `units.obj` | No |
| `0x122e50` | `FUN_00122e50` | `aiming_screen_apply` | `units.obj` | No |
| `0x123470` | `FUN_00123470` | `animation_get_root_matrix` | `units.obj` | No |
| `0x1234b0` | `FUN_001234b0` | `animation_get_root_velocity` | `units.obj` | No |
| `0x123560` | `FUN_00123560` | `render_model_parts` | `units.obj` | No |
| `0x1a01d0` | `FUN_001a01d0` | `validate_real_vector3d_axes3` | `bipeds.obj` | No |
| `0x1a03c0` | `FUN_001a03c0` | `biped_limp_noodle_adjust_orientations` | `bipeds.obj` | Yes |
| `0x1a0680` | `FUN_001a0680` | `biped_limp_noodle_relax_nodes_onto_environment` | `bipeds.obj` | No |
| `0x1a0a40` | `FUN_001a0a40` | `biped_bumped_object` | `bipeds.obj` | Yes |
| `0x1a0be0` | `FUN_001a0be0` | `biped_falling_damage` | `bipeds.obj` | Yes |
| `0x1a0e00` | `FUN_001a0e00` | `biped_start_landing` | `bipeds.obj` | Yes |
| `0x1a0f10` | `FUN_001a0f10` | `biped_make_footstep` | `bipeds.obj` | Yes |
| `0x1a1a10` | `FUN_001a1a10` | `biped_find_ground_surface` | `bipeds.obj` | Yes |
| `0x1a1fb0` | `FUN_001a1fb0` | `biped_vehicle_speech` | `bipeds.obj` | Yes |
| `0x1a2290` | `FUN_001a2290` | `biped_jump` | `bipeds.obj` | Yes |
| `0x1a2440` | `FUN_001a2440` | `biped_try_to_make_footsteps` | `bipeds.obj` | Yes |
| `0x1a2800` | `FUN_001a2800` | `biped_verify_object_vectors` | `bipeds.obj` | Yes |
| `0x1a2900` | `FUN_001a2900` | `biped_update_airborne` | `bipeds.obj` | No |
| `0x1a2a60` | `FUN_001a2a60` | `biped_update_landing` | `bipeds.obj` | Yes |
| `0x1a2b90` | `FUN_001a2b90` | `biped_update_jumping` | `bipeds.obj` | Yes |
| `0x1a4990` | `FUN_001a4990` | `biped_new` | `bipeds.obj` | No |
| `0x1a4a50` | `FUN_001a4a50` | `biped_preprocess_node_orientations` | `bipeds.obj` | No |
| `0x1a6280` | `FUN_001a6280` | `biped_update_dead` | `units.obj` | Yes |
| `0x1a6350` | `FUN_001a6350` | `biped_update` | `units.obj` | No |
| `0x1a67b0` | `FUN_001a67b0` | `dialogue_get_vocalization_name` | `units.obj` | No |
| `0x1a67e0` | `FUN_001a67e0` | `dialogue_get_vocalization_type_by_name` | `units.obj` | No |
| `0x1a6820` | `FUN_001a6820` | `unit_definition_get_active_hud_index` | `units.obj` | No |
| `0x1a6870` | `FUN_001a6870` | `unit_definition_get_seat_active_hud_index` | `units.obj` | No |
| `0x1a68d0` | `FUN_001a68d0` | `unit_test_speech` | `units.obj` | No |
| `0x1a6bc0` | `FUN_001a6bc0` | `unit_is_speaking` | `units.obj` | No |
| `0x1a6bf0` | `FUN_001a6bf0` | `unit_dialogue_determine_variant` | `units.obj` | No |
| `0x1a6ca0` | `FUN_001a6ca0` | `unit_get_speech_priority_name` | `units.obj` | No |
| `0x1a6cd0` | `FUN_001a6cd0` | `unit_get_speech_priority_by_name` | `units.obj` | No |
| `0x1a6d10` | `FUN_001a6d10` | `unit_describe_speech` | `units.obj` | No |
| `0x1a6e20` | `FUN_001a6e20` | `unit_lose_speech` | `units.obj` | Yes |
| `0x1a6ef0` | `FUN_001a6ef0` | `unit_speak` | `units.obj` | No |
| `0x1a70d0` | `FUN_001a70d0` | `unit_notify_impulse_sound` | `units.obj` | No |
| `0x1a71c0` | `FUN_001a71c0` | `unit_make_damage_sound` | `units.obj` | No |
| `0x1a74d0` | `FUN_001a74d0` | `unit_scream` | `units.obj` | No |
| `0x1a7650` | `FUN_001a7650` | `unit_find_dialogue_variant` | `units.obj` | Yes |
| `0x1a7730` | `FUN_001a7730` | `unit_dialogue_setup` | `units.obj` | Yes |
| `0x1a7790` | `FUN_001a7790` | `unit_dialogue_update` | `units.obj` | No |
| `0x1a7a90` | `FUN_001a7a90` | `unit_scripting_set_maximum_vitality` | `units.obj` | No |
| `0x1a7ad0` | `FUN_001a7ad0` | `units_scripting_set_maximum_vitality` | `units.obj` | No |
| `0x1a7b50` | `FUN_001a7b50` | `unit_scripting_set_current_vitality` | `units.obj` | No |
| `0x1a7c70` | `FUN_001a7c70` | `units_scripting_set_current_vitality` | `units.obj` | No |
| `0x1a7cc0` | `FUN_001a7cc0` | `unit_scripting_get_health` | `units.obj` | No |
| `0x1a7d00` | `FUN_001a7d00` | `unit_scripting_get_shield` | `units.obj` | No |
| `0x1a7d40` | `FUN_001a7d40` | `unit_scripting_get_grenade_count` | `units.obj` | No |
| `0x1a7d80` | `FUN_001a7d80` | `unit_scripting_impervious` | `units.obj` | No |
| `0x1a7df0` | `FUN_001a7df0` | `unit_scripting_start_user_animation_list` | `units.obj` | No |
| `0x1a7e70` | `FUN_001a7e70` | `unit_scripting_has_weapon` | `units.obj` | No |
| `0x1a7ea0` | `FUN_001a7ea0` | `unit_scripting_has_weapon_readied` | `units.obj` | No |
| `0x1a8550` | `FUN_001a8550` | `unit_euler_axis_doplan` | `units.obj` | Yes |
| `0x1a86b0` | `FUN_001a86b0` | `unit_animation_state_interruptable` | `units.obj` | Yes |
| `0x1a8730` | `FUN_001a8730` | `unit_animation_busy` | `units.obj` | Yes |
| `0x1a8770` | `FUN_001a8770` | `unit_animation_overlay_action_loops` | `units.obj` | Yes |
| `0x1a8790` | `FUN_001a8790` | `unit_animation_state_loops` | `units.obj` | Yes |
| `0x1a87f0` | `FUN_001a87f0` | `unit_animation_weapon_ik` | `units.obj` | Yes |
| `0x1a8850` | `FUN_001a8850` | `unit_animation_vehicle_ik` | `units.obj` | Yes |
| `0x1a8890` | `FUN_001a8890` | `unit_animation_aiming_screen` | `units.obj` | Yes |
| `0x1a88b0` | `FUN_001a88b0` | `unit_animation_state_get_aiming_screen_index` | `units.obj` | Yes |
| `0x1a8910` | `FUN_001a8910` | `unit_animation_state_can_be_entered_without_animation` | `units.obj` | Yes |
| `0x1a8950` | `FUN_001a8950` | `unit_animation_compute_interpolation_frame_count` | `units.obj` | Yes |
| `0x1a8b20` | `FUN_001a8b20` | `unit_animation_start_overlay_action` | `units.obj` | Yes |
| `0x1a9ec0` | `FUN_001a9ec0` | `unit_scripting_unit_driver` | `units.obj` | No |
| `0x1a9ef0` | `FUN_001a9ef0` | `unit_scripting_unit_gunner` | `units.obj` | No |
| `0x1aa170` | `FUN_001AA170` | `units_debug_get_closest_unit` | `units.obj` | No |
| `0x1ab6e0` | `FUN_001ab6e0` | `base_seat_label_get` | `units.obj` | Yes |
| `0x1ab870` | `FUN_001ab870` | `unit_animation_update` | `units.obj` | Yes |
| `0x1ab940` | `FUN_001ab940` | `unit_get_weapon` | `units.obj` | Yes |
| `0x1abd10` | `FUN_001abd10` | `unit_melee_sound` | `units.obj` | Yes |
| `0x1abd90` | `FUN_001abd90` | `unit_cause_continuous_melee_damage` | `units.obj` | Yes |
| `0x1ac680` | `FUN_001ac680` | `unit_euler_axis_buildplan` | `units.obj` | No |
| `0x1afd30` | `FUN_001afd30` | `unit_preprocess_node_orientations` | `units.obj` | No |
| `0x1b04b0` | `FUN_001b04b0` | `unit_postprocess_node_matrices` | `units.obj` | No |
| `0x1b0630` | `FUN_001b0630` | `unit_euler_aiming_update` | `units.obj` | No |
| `0x1b1400` | `FUN_001b1400` | `unit_ping_animation` | `units.obj` | No |
| `0x1b3690` | `FUN_001b3690` | `unit_update` | `units.obj` | No |


---

## Verification & Build Gates

1. **Duplicate Check:**
   - Command: `python3 scratch/find_all_dupes_in_kb.py`
   - Result: `Found 0 duplicate function names in kb.json`.
2. **ABI Drift Check:**
   - Command: `python3 tools/audit/extract_reg_args.py --check`
   - Result: `Check results: 886 OK, 0 drift, 0 missing, 0 stale`.
3. **Hazard Audit:**
   - Command: `python3 tools/audit/check_lift_hazards.py --changed-only`
   - Result: `0 blockers`.
4. **Header and Def Regeneration:**
   - Command: `python3 tools/analysis/knowledge.py --gen-header build/generated/decl.h --gen-def build/generated/halo.xbe.def --gen-thunks build/generated/thunks.c`
   - Result: Clean export with 0 errors.
5. **Compilation & Link:**
   - Command: `cmake --build build --target halo`
   - Result: `[ 98%] Built target halo` successful link without undefined symbols or warnings-as-errors.
