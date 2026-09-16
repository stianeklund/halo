# Batch 1.2A Function Name Recovery Report: Objects Subsystem

**Date:** September 14, 2026  
**Target Build:** Halo Xbox Debug Build 2276 (`01.10.12.2276`, Oct 12, 2001)  
**Binary Reference:** `halo-patched/cachebeta.xbe` (MD5: `c7869590a1c64ad034e49a5ee0c02465`)  
**Scope:** Sub-Batch 1.2A (`objects.obj` & `object_lights.obj`)  
**Total Functions Recovered:** 104 (102 in `objects.obj`, 2 in `object_lights.obj`)  

---

## Executive Summary

Sub-Batch 1.2A recovers 104 authentic Bungie debug symbol names across the core object management and lighting subsystems (`objects.obj` and `object_lights.obj`). All symbol names are directly extracted from Bungie debug map evidence in `halo_2276_functions.txt`.

This recovery resolves long-standing placeholders (`FUN_0013xxxx` through `FUN_0014xxxx`) and disambiguates three critical naming collisions that previously affected `objects.c`, `damage.c`, `widgets.c`, and `players.c`.

All ABI register annotations have been verified against `tools/kb_reg_baseline.json` with **0 drift across 886 tracked functions**, zero hazard scan blockers, and clean compilation linking into `halo.xbe`.

---

## Collision Investigations & Resolutions

During the recovery of the 104 functions in Sub-Batch 1.2A, three structural naming collisions were identified and resolved:

### 1. `0x13df70` vs `0x13e1a0` (`object_header_delete` vs `object_postprocess_node_matrices`)
- **Prior State:** `0x13df70` was incorrectly labeled `object_postprocess_node_matrices` in `kb.json`, while `0x13e1a0` remained an unnamed placeholder (`FUN_0013e1a0`).
- **Binary Evidence:**
  - `0x13df70`: Takes `data_t *data, int object_handle@<ebx>`. Disassembly and call-site analysis in `objects.c` confirm it performs the datum deletion and header cleanup for an object datum in the global objects data table (`_object_header_delete`).
  - `0x13e1a0`: Takes `int object_handle@<edi>`. Reads model animation block headers and dispatches node matrix postprocessing passes. This corresponds to the authentic symbol `_object_postprocess_node_matrices`.
- **Resolution:**
  - `0x13df70` renamed to `object_header_delete` (`data_t *data, int object_handle@<ebx>`).
  - `0x13e1a0` renamed to authentic Bungie symbol `object_postprocess_node_matrices` (`int object_handle@<edi>`).
  - Updated all call sites in `objects.c` and updated `kb.json` and `tools/kb_reg_baseline.json`.

### 2. `0x136580`–`0x1365b0` vs `0x135f90`–`0x136150` (`damage_*` vs `widgets_*`)
- **Prior State:** In `kb.json` and `src/halo/objects/widgets/widgets.c`, routines `0x136580`, `0x1365a0`, and `0x1365b0` were misnamed `widgets_initialize`, `widgets_initialize_for_new_map`, and `widgets_dispose_from_old_map`.
- **Binary Evidence:**
  - In `objects.c`, the lifecycle functions (`objects_initialize`, `objects_initialize_for_new_map`, `objects_dispose_from_old_map`, `objects_dispose`) invoke sub-modules in sequence:
    1. `widgets_*` (`0x135f90`, `0x136040`, `0x1360a0`, `0x136100`) in `widgets.obj`.
    2. `damage_*` (`0x136580`, `0x1365a0`, `0x1365b0`, `0x1365c0`) in `damage.obj`.
    3. `object_types_*` (`0x13c2e0`, `0x13c3d0`, `0x13c400`, `0x13c3a0`) in `objects.obj`.
    4. `lights_*` (`0x1391e0`, `0x1392b0`, `0x1392c0`, `0x1392a0`) in `object_lights.obj`.
- **Resolution:**
  - Reassigned `0x136580` -> `damage_initialize`, `0x1365a0` -> `damage_initialize_for_new_map`, `0x1365b0` -> `damage_dispose_from_old_map`.
  - Maintained true widget lifecycle symbols at `0x135f90` (`widgets_initialize`), `0x136040` (`widgets_initialize_for_new_map`), `0x1360a0` (`widgets_dispose_from_old_map`), and `0x136100` (`widgets_dispose`).

### 3. `0x136930` vs `0x1366b0` (`object_cannot_take_damage` vs `object_get_maximum_body_vitality`)
- **Prior State:** `0x136930` in `src/halo/objects/damage.c` and `kb.json` was misnamed `object_get_maximum_body_vitality(int player_handle)`. Meanwhile, `0x1366b0` in `objects.c` was the actual vitality getter `object_get_maximum_body_vitality(int object_handle, char use_raw_max)`.
- **Binary Evidence:**
  - `halo_2276_functions.txt` line 5082: `_object_cannot_take_damage .text 00136930`.
  - `halo_2276_functions.txt` line 5074: `_object_get_maximum_body_vitality .text 001366B0`.
  - `0x136930` sets bit 3 of `object+0xb7` (`*(unsigned char *)(obj + 0xb7) |= 0x8;`), making the object immune to damage (god-mode). It is the complement of `0x1368e0` (`object_can_take_damage`), which clears bit 3 (`&= 0xf7`).
  - In `players.c`, `FUN_000beab0` and `FUN_000beaf0` are the HaloScript trampolines for `object_cannot_take_damage` and `object_can_take_damage`.
- **Resolution:**
  - In `kb.json`: Replaced `0x136930` declaration with `void object_cannot_take_damage(int player_handle);`.
  - In `src/halo/objects/damage.c`: Renamed function definition and updated documentation comment.
  - In `src/halo/game/players.c`: Updated caller at line 5363 to call `object_cannot_take_damage(*record);`.
  - In `tools/kb_reg_baseline.json`: Updated baseline to `object_cannot_take_damage`.

---

## Register Argument Functions Preserved (8 Routines)

All 8 functions in Sub-Batch 1.2A utilizing custom register-calling conventions retain their exact annotations with 0 ABI drift:

| Address | Recovered Name | Annotated Signature | Registers |
|---------|----------------|---------------------|-----------|
| `0x134070` | `glow_normal_particle_update_position` | `void glow_normal_particle_update_position(int particle_ptr@<esi>, int glow_widget_ptr@<edi>, int object_handle, float delta, float ratio);` | ESI, EDI |
| `0x1342a0` | `glow_particles_initialize` | `void glow_particles_initialize(int glow_widget_ptr@<esi>);` | ESI |
| `0x134c40` | `light_volume_interpolate_frames` | `void *light_volume_interpolate_frames(int definition_ptr@<ebx>, int object_handle);` | EBX |
| `0x139350` | `light_build_cluster_array` | `int16_t light_build_cluster_array(int light_handle@<eax>, int16_t *out_buffer@<ebx>, int16_t max_count@<di>);` | EAX, EBX, DI |
| `0x139810` | `brighten_real_rgb_color` | `void brighten_real_rgb_color(float *color@<ecx>, float scale);` | ECX |
| `0x139e50` | `build_distant_lights` | `void build_distant_lights(unsigned int param_1, float *param_2, float *param_3, float param_4, float *color_ptr@<ebx>, float *output_ptr@<esi>, float *intensity_ptr@<edi>);` | EBX, ESI, EDI |
| `0x13a250` | `light_compute_bounding_sphere` | `void light_compute_bounding_sphere(int light_handle@<eax>, float *out_position@<edi>, float *out_radius@<ebx>, char param_1, char param_2, char param_3);` | EAX, EDI, EBX |
| `0x13e1a0` | `object_postprocess_node_matrices` | `void object_postprocess_node_matrices(int object_handle@<edi>);` | EDI |

---

## Complete Table of Recovered Functions (104)

| Address | Placeholder / Prior Name | Authentic Symbol (Build 2276) | Object | Reg Args |
|---------|--------------------------|--------------------------------|--------|----------|
| `0x84ae0` | `FUN_84ae0` | `bored_camera_update` | `objects.obj` | No |
| `0x84fe0` | `FUN_84fe0` | `scripted_camera_enable` | `objects.obj` | No |
| `0x85000` | `FUN_85000` | `scripted_camera_set_animation` | `objects.obj` | No |
| `0x850d0` | `FUN_850d0` | `scripted_camera_set_first_person` | `objects.obj` | No |
| `0x85110` | `FUN_85110` | `scripted_camera_set_dead` | `objects.obj` | No |
| `0x85150` | `FUN_85150` | `scripted_camera_object_is_first_person_camera` | `objects.obj` | No |
| `0x85180` | `FUN_85180` | `scripted_camera_set` | `objects.obj` | No |
| `0x85260` | `FUN_85260` | `scripted_camera_set_absolute` | `objects.obj` | No |
| `0x85280` | `FUN_85280` | `scripted_camera_set_camera_point_relative` | `objects.obj` | No |
| `0x85350` | `FUN_85350` | `scripted_camera_set_camera_point_absolute` | `objects.obj` | No |
| `0x853a0` | `FUN_853a0` | `scripted_camera_time` | `objects.obj` | No |
| `0x853c0` | `FUN_853c0` | `scripted_camera_update` | `objects.obj` | No |
| `0x9eb40` | `FUN_9eb40` | `effect_new_looping` | `objects.obj` | No |
| `0x9ec30` | `FUN_9ec30` | `effect_new_from_object` | `objects.obj` | No |
| `0xadf70` | `FUN_adf70` | `game_engine_remap_equipment` | `objects.obj` | No |
| `0xae0a0` | `FUN_ae0a0` | `game_engine_remap_object_definition` | `objects.obj` | No |
| `0xae110` | `FUN_ae110` | `game_engine_get_state_message` | `objects.obj` | No |
| `0xae250` | `FUN_ae250` | `game_engine_did_player_win_default` | `objects.obj` | No |
| `0x1330a0` | `FUN_1330a0` | `glow_delete` | `objects.obj` | No |
| `0x133520` | `FUN_133520` | `glow_render` | `objects.obj` | No |
| `0x1336a0` | `FUN_1336a0` | `nonuniform_cubic_spline_vector3d` | `objects.obj` | No |
| `0x134070` | `FUN_134070` | `glow_normal_particle_update_position` | `objects.obj` | Yes |
| `0x1342a0` | `FUN_1342a0` | `glow_particles_initialize` | `objects.obj` | Yes |
| `0x134ae0` | `FUN_134ae0` | `glow_submit` | `objects.obj` | No |
| `0x134be0` | `FUN_134be0` | `light_volume_new` | `objects.obj` | No |
| `0x134c20` | `FUN_134c20` | `light_volume_delete` | `objects.obj` | No |
| `0x134c40` | `FUN_134c40` | `light_volume_interpolate_frames` | `objects.obj` | Yes |
| `0x134e50` | `FUN_134e50` | `pow1` | `objects.obj` | No |
| `0x134e80` | `FUN_134e80` | `light_volume_render` | `objects.obj` | No |
| `0x135210` | `FUN_135210` | `light_volume_submit` | `objects.obj` | No |
| `0x1353b0` | `FUN_1353b0` | `lightning_new` | `objects.obj` | No |
| `0x1353f0` | `FUN_1353f0` | `lightning_delete` | `objects.obj` | No |
| `0x135510` | `FUN_135510` | `lightning_submit` | `objects.obj` | No |
| `0x135f20` | `FUN_135f20` | `tag_group_to_widget_type` | `objects.obj` | No |
| `0x135f90` | `FUN_135f90` | `widgets_initialize` | `objects.obj` | No |
| `0x136040` | `FUN_136040` | `widgets_initialize_for_new_map` | `objects.obj` | No |
| `0x1360a0` | `FUN_1360a0` | `widgets_dispose_from_old_map` | `objects.obj` | No |
| `0x136100` | `FUN_136100` | `widgets_dispose` | `objects.obj` | No |
| `0x136150` | `FUN_136150` | `widgets_new` | `objects.obj` | No |
| `0x1362d0` | `FUN_1362d0` | `widgets_delete` | `objects.obj` | No |
| `0x1363d0` | `FUN_1363d0` | `widgets_need_lighting` | `objects.obj` | No |
| `0x1365d0` | `FUN_1365d0` | `object_initialize_vitality` | `objects.obj` | No |
| `0x1366b0` | `FUN_1366b0` | `object_get_maximum_body_vitality` | `objects.obj` | No |
| `0x139350` | `FUN_139350` | `light_build_cluster_array` | `objects.obj` | Yes |
| `0x139480` | `FUN_139480` | `light_particle` | `objects.obj` | No |
| `0x1397f0` | `FUN_1397f0` | `light_attenuation` | `objects.obj` | No |
| `0x139810` | `FUN_139810` | `brighten_real_rgb_color` | `objects.obj` | Yes |
| `0x1398b0` | `FUN_1398b0` | `cluster_get_first_light` | `objects.obj` | No |
| `0x1398d0` | `FUN_1398d0` | `cluster_get_next_light` | `objects.obj` | No |
| `0x139930` | `FUN_139930` | `light_unmarked` | `objects.obj` | No |
| `0x139990` | `FUN_139990` | `light_mark` | `objects.obj` | No |
| `0x139a30` | `FUN_139a30` | `render_debug_light` | `objects.obj` | No |
| `0x139b40` | `FUN_139b40` | `lights_queue_lens_flare` | `objects.obj` | No |
| `0x139c20` | `FUN_139c20` | `find_point_lights_for_object_in_cluster` | `objects.obj` | No |
| `0x139e50` | `FUN_139e50` | `build_distant_lights` | `objects.obj` | Yes |
| `0x13a250` | `FUN_13a250` | `light_compute_bounding_sphere` | `objects.obj` | Yes |
| `0x13a340` | `FUN_13a340` | `light_get_bounding_sphere` | `objects.obj` | No |
| `0x13a420` | `FUN_13a420` | `lights_render_diffuse` | `objects.obj` | No |
| `0x13a5f0` | `FUN_13a5f0` | `lights_render_specular` | `objects.obj` | No |
| `0x13a740` | `FUN_13a740` | `lights_illumination_at_point` | `objects.obj` | No |
| `0x13aa10` | `FUN_13aa10` | `lights_prepare_for_object_dynamic` | `objects.obj` | No |
| `0x13ab20` | `FUN_13ab20` | `lights_distant_lighting_at_point` | `objects.obj` | No |
| `0x13b150` | `FUN_13b150` | `lights_reconnect_to_structure_bsp` | `objects.obj` | No |
| `0x13b1b0` | `FUN_13b1b0` | `light_new` | `objects.obj` | No |
| `0x13b290` | `FUN_13b290` | `light_new_unattached` | `objects.obj` | No |
| `0x13b380` | `FUN_13b380` | `lights_preprocess_scene` | `objects.obj` | No |
| `0x13bce0` | `FUN_13bce0` | `lights_prepare_for_object_static` | `objects.obj` | No |
| `0x13c030` | `FUN_13c030` | `build_family_shadow` | `objects.obj` | No |
| `0x13c080` | `FUN_13c080` | `object_build_shadow` | `objects.obj` | No |
| `0x13c100` | `FUN_13c100` | `object_type_definition_get` | `objects.obj` | No |
| `0x13c1b0` | `FUN_13c1b0` | `object_type_get_datum_size` | `objects.obj` | No |
| `0x13c250` | `FUN_13c250` | `object_type_get_name` | `objects.obj` | No |
| `0x13c2e0` | `FUN_13c2e0` | `object_types_initialize` | `objects.obj` | No |
| `0x13c3a0` | `FUN_13c3a0` | `object_types_dispose` | `objects.obj` | No |
| `0x13c3d0` | `FUN_13c3d0` | `object_types_initialize_for_new_map` | `objects.obj` | No |
| `0x13c400` | `FUN_13c400` | `object_types_dispose_from_old_map` | `objects.obj` | No |
| `0x13c430` | `FUN_13c430` | `object_type_adjust_placement` | `objects.obj` | No |
| `0x13c490` | `FUN_13c490` | `object_type_new` | `objects.obj` | No |
| `0x13c500` | `FUN_13c500` | `object_type_place` | `objects.obj` | No |
| `0x13c560` | `FUN_13c560` | `object_type_delete` | `objects.obj` | No |
| `0x13c5c0` | `FUN_13c5c0` | `object_type_update` | `objects.obj` | No |
| `0x13c620` | `FUN_13c620` | `object_type_export_function_values` | `objects.obj` | No |
| `0x13c680` | `FUN_13c680` | `object_type_handle_deleted_object` | `objects.obj` | No |
| `0x13c6e0` | `FUN_13c6e0` | `object_type_handle_region_destroyed` | `objects.obj` | No |
| `0x13c740` | `FUN_13c740` | `object_type_handle_parent_destroyed` | `objects.obj` | No |
| `0x13c7a0` | `FUN_13c7a0` | `object_type_preprocess_node_orientations` | `objects.obj` | No |
| `0x13c800` | `FUN_13c800` | `object_type_postprocess_node_matrices` | `objects.obj` | No |
| `0x13c860` | `FUN_13c860` | `object_type_reset` | `objects.obj` | No |
| `0x13c8c0` | `FUN_13c8c0` | `object_type_disconnect_from_structure_bsp` | `objects.obj` | No |
| `0x13c920` | `FUN_13c920` | `object_type_render_debug` | `objects.obj` | No |
| `0x13c980` | `FUN_13c980` | `object_type_notify_impulse_sound` | `objects.obj` | No |
| `0x13c9e0` | `FUN_13c9e0` | `object_definition_index_to_object_type` | `objects.obj` | No |
| `0x13ca30` | `FUN_13ca30` | `scenario_get_object_type_scenario_datums` | `objects.obj` | No |
| `0x13cab0` | `FUN_13cab0` | `scenario_get_object_type_scenario_palette` | `objects.obj` | No |
| `0x13cb30` | `FUN_13cb30` | `object_types_disconnect_from_structure_bsp` | `objects.obj` | No |
| `0x13cb80` | `FUN_13cb80` | `object_types_place_objects` | `objects.obj` | No |
| `0x13cdd0` | `FUN_13cdd0` | `object_types_place_all` | `objects.obj` | No |
| `0x13ce90` | `FUN_13ce90` | `object_names_postprocess` | `objects.obj` | No |
| `0x13cf50` | `FUN_13cf50` | `object_type_synchronize` | `objects.obj` | No |
| `0x13e1a0` | `FUN_13e1a0` | `object_postprocess_node_matrices` | `objects.obj` | Yes |
| `0x141900` | `FUN_141900` | `objects_paparazzi` | `objects.obj` | No |
| `0x141970` | `FUN_141970` | `object_export_function_values` | `objects.obj` | No |
| `0x145490` | `FUN_145490` | `objects_memory_compact` | `objects.obj` | No |
| `0x1a9520` | `FUN_1a9520` | `unit_get_center_of_mass` | `objects.obj` | No |


---

## Verification & Build Gates

1. **Duplicate Check:**
   - Script: `scratch/find_all_dupes_in_kb.py`
   - Result: `Found 0 duplicate function names in kb.json`.
2. **ABI Drift Check:**
   - Command: `python3 tools/audit/extract_reg_args.py --check`
   - Result: `Check results: 886 OK, 0 drift, 0 missing, 0 stale`.
3. **Hazard Audit:**
   - Command: `python3 tools/audit/check_lift_hazards.py --changed-only`
   - Result: `0 blockers` (clean on all touched files).
4. **Header and Def Regeneration:**
   - Command: `python3 tools/analysis/knowledge.py --gen-header build/generated/decl.h --gen-def build/generated/halo.xbe.def --gen-thunks build/generated/thunks.c`
   - Result: Clean export of header declarations, def exports, and thunks.
5. **Compilation & Link:**
   - Command: `cmake --build build --target halo`
   - Result: `[ 98%] Built target halo` successful link without undefined symbols or errors.
