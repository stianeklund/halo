# Batch 1.2D Function Name Recovery Report: Weapons, Items, Projectiles, Devices & First-Person Weapons

**Date:** September 15, 2026  
**Target Build:** Halo Xbox Debug Build 2276 (`01.10.12.2276`, Oct 12, 2001)  
**Binary Reference:** `halo-patched/cachebeta.xbe` (MD5: `c7869590a1c64ad034e49a5ee0c02465`)  
**Total Functions Recovered:** 96  
**Scope:** Sub-Batch 1.2D (`weapons.obj`, `items.obj`, `projectiles.obj`, `devices.obj`, `first_person_weapons.obj`)  

---

## Executive Summary

Sub-Batch 1.2D recovers **96 authentic Bungie debug symbol names** across the weapons, items, projectiles, devices, and first-person weapon subsystems. All names were directly verified against Bungie debug map evidence in `halo_2276_functions.txt`.

This recovery resolves critical historical naming collisions across `devices.c`, `items.c`, and `weapons.c` (e.g. `device_effect_new` vs `device_group_new`, `item_new` vs `garbage_update`), aligns `kb.json` with implementations in `first_person_weapons.c`, and preserves 100% ABI stability across all 22 custom `@<reg>` calling conventions with **0 ABI drift across 886 tracked functions** in `tools/kb_reg_baseline.json`.

Compilation, linking, and hazard validation completed cleanly with zero blockers (`[ 98%] Built target halo`).

---

## Subsystem Breakdown


| Object File | Total Recovered | Ported | Unported | Register Functions (`@<reg>`) |
|-------------|----------------:|-------:|---------:|-------------------------------:|
| `devices.obj` | 3 | 3 | 0 | 0 |
| `first_person_weapons.obj` | 18 | 12 | 6 | 8 |
| `items.obj` | 28 | 22 | 6 | 3 |
| `projectiles.obj` | 12 | 12 | 0 | 6 |
| `weapons.obj` | 35 | 10 | 25 | 5 |
| **Total** | **96** | **59** | **37** | **22** |

---

## Key Collision Investigations & Resolutions

### 1. `0x967a0` vs `0x96850` (`device_effect_new` vs `device_group_new`)
- **Prior State:** `0x96850` was mistakenly named `device_effect_new(float initial_value, short flags)`, forcing the actual device effect creation routine at `0x967a0` to retain the placeholder `FUN_000967a0`.
- **Binary Evidence:**
  - `0x967a0` takes `(int object_handle, int tag_index)` and instantiates sound/particle effects (`'effe'` and `'snd!'`) on device objects.
  - `0x96850` allocates a device group header from the `device_group` pool (`0x5aa8c8`).
  - `halo_2276_functions.txt` line 1259: `_device_effect_new .text 000967A0`.
  - `halo_2276_functions.txt` line 1260: `_device_group_new .text 00096850`.
- **Resolution:** `0x967a0` renamed to authentic `device_effect_new`; `0x96850` renamed to authentic `device_group_new`. Call sites in `recorded_animations.c` and `devices.c` aligned.

### 2. `0xf6820` vs `0xf6910` (`garbage_update` vs `item_new`)
- **Prior State:** `0xf6820` was misnamed `item_new(int object_handle)`, while `0xf6910` was misnamed `item_activate(int item_handle)`.
- **Binary Evidence:**
  - `0xf6820` updates countdown timers and garbage collection despawn ticks for items.
  - `0xf6910` instantiates a new item object, setting item initialization flags and timestamp.
  - `halo_2276_functions.txt` line 2697: `_garbage_update .text 000F6820`.
  - `halo_2276_functions.txt` line 2702: `_item_new .text 000F6910`.
- **Resolution:** `0xf6820` renamed to authentic `garbage_update`; `0xf6910` renamed to authentic `item_new`.

### 3. `0xdd580` vs `0xde560` (`first_person_weapon_build_node_matrices` vs `first_person_weapon_update`)
- **Prior State:** `0xdd580` was named `first_person_weapon_update`, leaving the actual per-tick weapon update function `0xde560` as `FUN_000de560`.
- **Binary Evidence:**
  - `0xdd580` calculates bone transform matrices for the first person viewmodel.
  - `0xde560` runs the state machine update loop for first-person weapons.
  - `halo_2276_functions.txt` line 2307: `_first_person_weapon_build_node_matrices .text 000DD580`.
  - `halo_2276_functions.txt` line 2317: `_first_person_weapon_update .text 000DE560`.
- **Resolution:** `0xdd580` renamed to `first_person_weapon_build_node_matrices`; `0xde560` renamed to authentic `first_person_weapon_update`.

### 4. `0xfb880` vs `0xfe790` (`weapon_trigger_change_state` vs `weapon_trigger_release_charge`)
- **Prior State:** `0xfb880` was named `weapon_trigger_release_charge`, leaving `0xfe790` as `FUN_000fe790`.
- **Binary Evidence:**
  - `0xfb880` transitions trigger state between idle, charging, and firing.
  - `0xfe790` handles overcharge/release mechanics.
  - `halo_2276_functions.txt` line 2810: `_weapon_trigger_change_state .text 000FB880`.
  - `halo_2276_functions.txt` line 2862: `_weapon_trigger_release_charge .text 000FE790`.
- **Resolution:** `0xfb880` renamed to `weapon_trigger_change_state`; `0xfe790` renamed to `weapon_trigger_release_charge`.

---

## Complete List of Recovered Functions (96 Functions)


| Address | Object | Previous Name | Recovered Bungie Symbol | Ported | Register Args |
|---------|--------|---------------|-------------------------|:------:|:-------------:|
| `0x966d0` | `devices.obj` | `device_group_set_real` | `device_touched` | ✅ Yes | No |
| `0x967a0` | `devices.obj` | `FUN_000967a0` | `device_effect_new` | ✅ Yes | No |
| `0x96850` | `devices.obj` | `device_effect_new` | `device_group_new` | ✅ Yes | No |
| `0xdc7a0` | `first_person_weapons.obj` | `FUN_000dc7a0` | `first_person_weapons_initialize_for_new_map` | ✅ Yes | No |
| `0xdc8c0` | `first_person_weapons.obj` | `FUN_000dc8c0` | `first_person_animation_type_from_weapon_state` | ✅ Yes | Yes (`@<reg>`) |
| `0xdc9d0` | `first_person_weapons.obj` | `FUN_000dc9d0` | `weapon_play_first_person_weapon_sound` | ✅ Yes | Yes (`@<reg>`) |
| `0xdcaf0` | `first_person_weapons.obj` | `FUN_000dcaf0` | `first_person_weapon_get` | ❌ No | No |
| `0xdcb30` | `first_person_weapons.obj` | `FUN_000dcb30` | `first_person_weapon_set_visibility` | ✅ Yes | Yes (`@<reg>`) |
| `0xdcbd0` | `first_person_weapons.obj` | `fp_anim_apply_node_remap` | `model_remap_node_matrices_to_match_animation_graph` | ✅ Yes | Yes (`@<reg>`) |
| `0xdcc80` | `first_person_weapons.obj` | `FUN_000dcc80` | `model_build_remapping_table_for_animation_graph` | ✅ Yes | Yes (`@<reg>`) |
| `0xdcd60` | `first_person_weapons.obj` | `FUN_000dcd60` | `first_person_weapon_index_from_weapon_index` | ✅ Yes | Yes (`@<reg>`) |
| `0xdcdc0` | `first_person_weapons.obj` | `FUN_000dcdc0` | `first_person_weapon_index_from_unit_index` | ❌ No | No |
| `0xdce00` | `first_person_weapons.obj` | `FUN_000dce00` | `first_person_weapon_predict` | ✅ Yes | Yes (`@<reg>`) |
| `0xdd4d0` | `first_person_weapons.obj` | `FUN_000dd4d0` | `first_person_weapon_start_interpolation` | ✅ Yes | Yes (`@<reg>`) |
| `0xdd580` | `first_person_weapons.obj` | `first_person_weapon_update` | `first_person_weapon_build_node_matrices` | ❌ No | No |
| `0xddbd0` | `first_person_weapons.obj` | `FUN_000ddbd0` | `first_person_weapon_set_state` | ✅ Yes | No |
| `0xdde80` | `first_person_weapons.obj` | `FUN_000dde80` | `first_person_weapon_switch_weapons` | ✅ Yes | No |
| `0xde0e0` | `first_person_weapons.obj` | `FUN_000de0e0` | `first_person_weapon_new_unit` | ❌ No | No |
| `0xde140` | `first_person_weapons.obj` | `FUN_000de140` | `first_person_weapon_message` | ✅ Yes | No |
| `0xde3f0` | `first_person_weapons.obj` | `FUN_000de3f0` | `first_person_weapon_next_state` | ❌ No | No |
| `0xde560` | `first_person_weapons.obj` | `FUN_000de560` | `first_person_weapon_update` | ❌ No | No |
| `0xf5500` | `items.obj` | `virtual_keyboard_set_validation` | `virtual_keyboard_launch` | ✅ Yes | No |
| `0xf5640` | `items.obj` | `FUN_000f5640` | `virtual_keyboard_active` | ✅ Yes | No |
| `0xf5650` | `items.obj` | `FUN_000f5650` | `virtual_keyboard_last_exit_saved_text` | ✅ Yes | No |
| `0xf5660` | `items.obj` | `FUN_000f5660` | `virtual_keyboard_tab_left` | ❌ No | No |
| `0xf56b0` | `items.obj` | `FUN_000f56b0` | `virtual_keyboard_tab_right` | ✅ Yes | No |
| `0xf5700` | `items.obj` | `FUN_000f5700` | `virtual_keyboard_tab_up` | ✅ Yes | No |
| `0xf5750` | `items.obj` | `FUN_000f5750` | `virtual_keyboard_tab_down` | ✅ Yes | No |
| `0xf57a0` | `items.obj` | `FUN_000f57a0` | `virtual_keyboard_cancel` | ✅ Yes | No |
| `0xf5800` | `items.obj` | `FUN_000f5800` | `virtual_keyboard_get_character` | ❌ No | Yes (`@<reg>`) |
| `0xf5900` | `items.obj` | `virtual_keyboard_tab_left` | `virtual_keyboard_render_internal` | ❌ No | No |
| `0xf5f10` | `items.obj` | `FUN_000f5f10` | `virtual_keyboard_free_space_in_text_buffer` | ✅ Yes | No |
| `0xf5f30` | `items.obj` | `FUN_000f5f30` | `virtual_keyboard_backspace` | ✅ Yes | No |
| `0xf5f90` | `items.obj` | `items_initialize` | `virtual_keyboard_close` | ❌ No | No |
| `0xf5fa0` | `items.obj` | `items_initialize_for_new_map` | `virtual_keyboard_render` | ❌ No | No |
| `0xf5fb0` | `items.obj` | `FUN_000f5fb0` | `virtual_keyboard_select` | ✅ Yes | No |
| `0xf63f0` | `items.obj` | `virtual_keyboard_process_input` | `virtual_keyboard_process_internal` | ✅ Yes | No |
| `0xf6740` | `items.obj` | `items_dispose_from_old_map` | `virtual_keyboard_process` | ✅ Yes | No |
| `0xf6750` | `items.obj` | `FUN_000f6750` | `equipment_place` | ✅ Yes | No |
| `0xf67b0` | `items.obj` | `item_activate_equipment_effect` | `equipment_handle_pickup` | ✅ Yes | No |
| `0xf67f0` | `items.obj` | `FUN_000f67f0` | `equipment_definition_handle_pickup` | ✅ Yes | No |
| `0xf6820` | `items.obj` | `item_new` | `garbage_update` | ✅ Yes | No |
| `0xf6860` | `items.obj` | `item_begin_garbage_collection` | `garbage_new` | ✅ Yes | No |
| `0xf68b0` | `items.obj` | `FUN_000f68b0` | `object_get_type` | ✅ Yes | No |
| `0xf6910` | `items.obj` | `item_activate` | `item_new` | ✅ Yes | No |
| `0xf69c0` | `items.obj` | `item_attach_to_unit` | `item_in_unit_inventory` | ✅ Yes | No |
| `0xf6b80` | `items.obj` | `FUN_000f6b80` | `item_adjust_for_angular_velocity_change` | ✅ Yes | Yes (`@<reg>`) |
| `0xf6d60` | `items.obj` | `item_set_position` | `item_accelerate` | ✅ Yes | No |
| `0xf7110` | `items.obj` | `FUN_000f7110` | `item_align_to_normal_and_point` | ❌ No | Yes (`@<reg>`) |
| `0xf7e40` | `projectiles.obj` | `FUN_000f7e40` | `projectile_set_action` | ✅ Yes | Yes (`@<reg>`) |
| `0xf7e60` | `projectiles.obj` | `FUN_000f7e60` | `projectile_effect_new` | ✅ Yes | No |
| `0xf7fa0` | `projectiles.obj` | `FUN_000f7fa0` | `projectile_calculate_deceleration_from_distances` | ✅ Yes | Yes (`@<reg>`) |
| `0xf8590` | `projectiles.obj` | `FUN_000f8590` | `projectile_adjust_for_angular_velocity_change` | ✅ Yes | Yes (`@<reg>`) |
| `0xf8640` | `projectiles.obj` | `FUN_000f8640` | `projectile_calculate_deceleration` | ✅ Yes | Yes (`@<reg>`) |
| `0xf8720` | `projectiles.obj` | `FUN_000f8720` | `projectile_collision_test_line` | ✅ Yes | Yes (`@<reg>`) |
| `0xf8920` | `projectiles.obj` | `FUN_000f8920` | `projectile_detonate` | ✅ Yes | No |
| `0xf90d0` | `projectiles.obj` | `FUN_000f90d0` | `projectile_collision` | ✅ Yes | Yes (`@<reg>`) |
| `0xf9c40` | `projectiles.obj` | `FUN_000f9c40` | `projectile_update` | ✅ Yes | No |
| `0xfac20` | `projectiles.obj` | `FUN_000fac20` | `weapon_definition_get_damage_potential` | ✅ Yes | No |
| `0xface0` | `projectiles.obj` | `FUN_000face0` | `animation_update` | ✅ Yes | No |
| `0xfad00` | `projectiles.obj` | `FUN_000fad00` | `animation_choose_random_permutation` | ✅ Yes | No |
| `0xfb140` | `weapons.obj` | `weapon_get_animation_frame` | `weapon_get_first_person_animation_time` | ✅ Yes | No |
| `0xfb320` | `weapons.obj` | `FUN_000fb320` | `weapon_trigger_get` | ❌ No | No |
| `0xfb370` | `weapons.obj` | `FUN_000fb370` | `weapon_magazine_get` | ✅ Yes | Yes (`@<reg>`) |
| `0xfb3c0` | `weapons.obj` | `weapon_has_activity` | `weapon_busy` | ✅ Yes | Yes (`@<reg>`) |
| `0xfb510` | `weapons.obj` | `FUN_000fb510` | `weapon_trigger_get_charged_fraction` | ❌ No | No |
| `0xfb5a0` | `weapons.obj` | `FUN_000fb5a0` | `weapon_trigger_can_fire_again` | ❌ No | No |
| `0xfb690` | `weapons.obj` | `FUN_000fb690` | `weapon_magazine_idle` | ❌ No | No |
| `0xfb6e0` | `weapons.obj` | `weapon_start_effect` | `weapon_effect_new` | ✅ Yes | Yes (`@<reg>`) |
| `0xfb7d0` | `weapons.obj` | `FUN_000fb7d0` | `weapon_effect_looping_new` | ❌ No | No |
| `0xfb880` | `weapons.obj` | `weapon_trigger_release_charge` | `weapon_trigger_change_state` | ❌ No | No |
| `0xfb910` | `weapons.obj` | `FUN_000fb910` | `weapon_trigger_start_ejection_port` | ❌ No | No |
| `0xfb990` | `weapons.obj` | `FUN_000fb990` | `weapon_state_key_frame` | ❌ No | No |
| `0xfba00` | `weapons.obj` | `FUN_000fba00` | `weapon_state_interruptable` | ❌ No | No |
| `0xfba20` | `weapons.obj` | `weapon_set_animation_state` | `weapon_set_state` | ✅ Yes | Yes (`@<reg>`) |
| `0xfbcf0` | `weapons.obj` | `FUN_000fbcf0` | `power` | ❌ No | No |
| `0xfc990` | `weapons.obj` | `FUN_000fc990` | `weapon_magazine_start_reload` | ✅ Yes | Yes (`@<reg>`) |
| `0xfcaf0` | `weapons.obj` | `FUN_000fcaf0` | `weapon_magazine_finish_reload` | ✅ Yes | No |
| `0xfcbd0` | `weapons.obj` | `FUN_000fcbd0` | `weapon_magazine_start_chamber` | ❌ No | No |
| `0xfcc90` | `weapons.obj` | `FUN_000fcc90` | `weapon_magazine_finish_chamber` | ❌ No | No |
| `0xfcd10` | `weapons.obj` | `FUN_000fcd10` | `weapon_trigger_fully_charged` | ❌ No | No |
| `0xfcdd0` | `weapons.obj` | `FUN_000fcdd0` | `weapon_trigger_idle` | ❌ No | No |
| `0xfce60` | `weapons.obj` | `FUN_000fce60` | `weapon_trigger_locked` | ❌ No | No |
| `0xfcec0` | `weapons.obj` | `FUN_000fcec0` | `weapon_trigger_recover` | ❌ No | No |
| `0xfcf20` | `weapons.obj` | `weapon_reset_state` | `weapon_reset` | ✅ Yes | No |
| `0xfd0b0` | `weapons.obj` | `FUN_000fd0b0` | `projectile_distribute` | ❌ No | No |
| `0xfd150` | `weapons.obj` | `FUN_000fd150` | `weapon_state_next` | ❌ No | No |
| `0xfd2e0` | `weapons.obj` | `weapon_activate` | `weapon_ready` | ✅ Yes | No |
| `0xfd360` | `weapons.obj` | `weapon_try_place` | `weapon_put_away` | ✅ Yes | No |
| `0xfd520` | `weapons.obj` | `FUN_000fd520` | `weapon_trigger_finish_tracking` | ❌ No | No |
| `0xfd570` | `weapons.obj` | `FUN_000fd570` | `trigger_create_projectiles` | ❌ No | No |
| `0xfdc90` | `weapons.obj` | `FUN_000fdc90` | `weapon_trigger_fire` | ❌ No | No |
| `0xfe450` | `weapons.obj` | `FUN_000fe450` | `weapon_trigger_begin_firing` | ❌ No | No |
| `0xfe6c0` | `weapons.obj` | `FUN_000fe6c0` | `weapon_trigger_overload` | ❌ No | No |
| `0xfe790` | `weapons.obj` | `FUN_000fe790` | `weapon_trigger_release_charge` | ❌ No | No |
| `0xfe890` | `weapons.obj` | `FUN_000fe890` | `weapon_trigger_overcharged` | ❌ No | No |
