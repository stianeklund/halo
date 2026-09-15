# Batch 1.2E Function Name Recovery Report: Effects, Particles, Decals & Environment

**Date:** September 15, 2026  
**Target Build:** Halo Xbox Debug Build 2276 (`01.10.12.2276`, Oct 12, 2001)  
**Binary Reference:** `halo-patched/cachebeta.xbe` (MD5: `c7869590a1c64ad034e49a5ee0c02465`)  
**Total Functions Recovered:** 78  
**Scope:** Sub-Batch 1.2E (`effects.obj`, `particles.obj`, `particle_systems.obj`, `weather_particle_systems.obj`, `contrails.obj`, `decals.obj`, `wind.obj`, `glow.obj`)  

---

## Executive Summary

Sub-Batch 1.2E completes **Batch 1.2: Objects, Units, AI, and Weapons Subsystem Function Name Recovery** by recovering **78 authentic Bungie debug symbol names** across the game's visual and physical environmental effects subsystems: contrails, gameplay decals, dynamic effects, particle systems, local particles, weather particle systems, wind variance, and object widget glow.

All symbols were directly verified against Bungie debug map evidence in `halo_2276_functions.txt`.

This recovery resolves key naming collisions across particle systems and effects (such as `effects_information_get` vs `effect_random_angular_velocity`, and `particle_systems_update` vs `particle_systems_render`), while preserving 100% ABI stability across all 44 custom `@<reg>` calling conventions with **0 ABI drift across 886 tracked functions** in `tools/kb_reg_baseline.json`.

Compilation, linking, and hazard validation completed cleanly with zero blockers (`[ 98%] Built target halo`).

---

## Subsystem Breakdown


| Object File | Total Recovered | Ported | Register Functions (`@<reg>`) |
|-------------|----------------:|-------:|-------------------------------:|
| `contrails.obj` | 7 | 7 | 4 |
| `decals.obj` | 11 | 11 | 7 |
| `effects.obj` | 21 | 21 | 14 |
| `glow.obj` | 1 | 1 | 1 |
| `particle_systems.obj` | 15 | 15 | 5 |
| `particles.obj` | 10 | 10 | 4 |
| `weather_particle_systems.obj` | 12 | 12 | 8 |
| `wind.obj` | 1 | 1 | 1 |
| **Total** | **78** | **78** | **44** |

---

## Key Collision Investigations & Resolutions

### 1. `0x9c910` vs `0x9ca70` (`effects_information_get` vs `effect_random_angular_velocity`)
- **Prior State:** `0x9ca70` was mistakenly named `effects_information_get`, forcing the actual effect information getter at `0x9c910` to retain the placeholder `FUN_0009c910`.
- **Binary Evidence:**
  - `0x9c910` populates the effect information structure from the effect definition tag.
  - `0x9ca70` computes a random 3D angular velocity vector bounded by effect definition flags.
  - `halo_2276_functions.txt` line 1445: `_effects_information_get .text 0009C910`.
  - `halo_2276_functions.txt` line 1447: `_effect_random_angular_velocity .text 0009CA70`.
- **Resolution:** `0x9c910` renamed to authentic `effects_information_get`; `0x9ca70` renamed to authentic `effect_random_angular_velocity`.

### 2. `0xa0180` vs `0xa1170` (`particle_system_update` vs `particle_systems_render`)
- **Prior State:** `0xa1170` was misnamed `particle_system_update`, leaving the actual per-instance update routine `0xa0180` as `FUN_000a0180`.
- **Binary Evidence:**
  - `0xa0180` updates physics, lifetime, and particle states for a particle system instance.
  - `0xa1170` renders all active particle systems in the world.
  - `halo_2276_functions.txt` line 1555: `_particle_system_update .text 000A0180`.
  - `halo_2276_functions.txt` line 1561: `_particle_systems_render .text 000A1170`.
- **Resolution:** `0xa0180` renamed to authentic `particle_system_update`; `0xa1170` renamed to authentic `particle_systems_render`.

### 3. Decals Gameplay vs Rasterizer Scope Boundary
- Gameplay decal management (`decals.c`, `0x98970` to `0x99b70`, 11 functions) handles decal lifetime, attachment to objects, clipping, and decal buffer management.
- Low-level vertex and pixel pipeline decals (`0x17ca80` onwards) are part of the low-level rendering hardware pipeline and properly deferred to Batch 1.5 (Rasterizer & Rendering).

---

## Complete List of Recovered Functions (78 Functions)


| Address | Object | Previous Name | Recovered Bungie Symbol | Ported | Register Args |
|---------|--------|---------------|-------------------------|:------:|:-------------:|
| `0x97a50` | `contrails.obj` | `FUN_00097a50` | `contrail_compute_new_point_count` | ✅ Yes | Yes (`@<reg>`) |
| `0x97ae0` | `contrails.obj` | `FUN_00097ae0` | `contrail_verify` | ✅ Yes | Yes (`@<reg>`) |
| `0x97db0` | `contrails.obj` | `FUN_00097db0` | `contrail_next_frame` | ✅ Yes | Yes (`@<reg>`) |
| `0x97e40` | `contrails.obj` | `FUN_00097e40` | `contrail_add_points` | ✅ Yes | Yes (`@<reg>`) |
| `0x98200` | `contrails.obj` | `FUN_00098200` | `contrail_update_points` | ✅ Yes | No |
| `0x986d0` | `contrails.obj` | `contrail_set_state_for_object` | `contrail_owner_collision` | ✅ Yes | No |
| `0x9f6e0` | `contrails.obj` | `FUN_0009f6e0` | `particle_system_orphan` | ✅ Yes | No |
| `0x98970` | `decals.obj` | `FUN_00098970` | `decal_check` | ✅ Yes | Yes (`@<reg>`) |
| `0x98aa0` | `decals.obj` | `FUN_00098aa0` | `decal_set_first_decal_index` | ✅ Yes | Yes (`@<reg>`) |
| `0x98b20` | `decals.obj` | `FUN_00098b20` | `decal_sprite_get_bounds` | ✅ Yes | Yes (`@<reg>`) |
| `0x98e70` | `decals.obj` | `decals_update_for_new_map` | `decals_unlock` | ✅ Yes | No |
| `0x98fe0` | `decals.obj` | `FUN_00098fe0` | `decal_get_first_decal_index` | ✅ Yes | No |
| `0x99490` | `decals.obj` | `FUN_00099490` | `plane3d_from_point_and_normal` | ✅ Yes | No |
| `0x99840` | `decals.obj` | `FUN_00099840` | `decal_reinsert` | ✅ Yes | Yes (`@<reg>`) |
| `0x998b0` | `decals.obj` | `FUN_000998b0` | `decal_insert` | ✅ Yes | Yes (`@<reg>`) |
| `0x9a300` | `decals.obj` | `FUN_0009a300` | `decal_projection_create` | ✅ Yes | Yes (`@<reg>`) |
| `0x9a5a0` | `decals.obj` | `FUN_0009a5a0` | `decal_clip_to_surface` | ✅ Yes | Yes (`@<reg>`) |
| `0x9c4b0` | `decals.obj` | `FUN_0009c4b0` | `decal_new` | ✅ Yes | No |
| `0x9c700` | `effects.obj` | `FUN_0009c700` | `effects_object_is_corpse` | ✅ Yes | Yes (`@<reg>`) |
| `0x9c910` | `effects.obj` | `FUN_0009c910` | `effects_information_get` | ✅ Yes | No |
| `0x9c9a0` | `effects.obj` | `FUN_0009c9a0` | `effect_scale` | ✅ Yes | Yes (`@<reg>`) |
| `0x9c9f0` | `effects.obj` | `FUN_0009c9f0` | `effect_real_random_range` | ✅ Yes | Yes (`@<reg>`) |
| `0x9ca70` | `effects.obj` | `effects_information_get` | `effect_random_angular_velocity` | ✅ Yes | Yes (`@<reg>`) |
| `0x9caf0` | `effects.obj` | `FUN_0009caf0` | `effect_allowed_by_environment` | ✅ Yes | Yes (`@<reg>`) |
| `0x9cb90` | `effects.obj` | `FUN_0009cb90` | `effect_set_event` | ✅ Yes | Yes (`@<reg>`) |
| `0x9cc20` | `effects.obj` | `FUN_0009cc20` | `effect_build_location` | ✅ Yes | Yes (`@<reg>`) |
| `0x9cca0` | `effects.obj` | `FUN_0009cca0` | `effect_location_get_next_instance` | ✅ Yes | No |
| `0x9cdd0` | `effects.obj` | `FUN_0009cdd0` | `effect_evaluate_function_integral` | ✅ Yes | No |
| `0x9d1f0` | `effects.obj` | `FUN_0009d1f0` | `effect_random_translational_velocity` | ✅ Yes | Yes (`@<reg>`) |
| `0x9d2d0` | `effects.obj` | `FUN_0009d2d0` | `effect_allocate` | ✅ Yes | Yes (`@<reg>`) |
| `0x9d430` | `effects.obj` | `FUN_0009d430` | `impulse_effect_initialize` | ✅ Yes | Yes (`@<reg>`) |
| `0x9d4e0` | `effects.obj` | `FUN_0009d4e0` | `effect_build_locations` | ✅ Yes | Yes (`@<reg>`) |
| `0x9d590` | `effects.obj` | `FUN_0009d590` | `effect_generate_particles` | ✅ Yes | Yes (`@<reg>`) |
| `0x9dcf0` | `effects.obj` | `FUN_0009dcf0` | `effect_generate_part` | ✅ Yes | No |
| `0x9e180` | `effects.obj` | `FUN_0009e180` | `effect_marker_list_get_marker` | ✅ Yes | Yes (`@<reg>`) |
| `0x9e310` | `effects.obj` | `FUN_0009e310` | `effect_generate_parts` | ✅ Yes | Yes (`@<reg>`) |
| `0x9e560` | `effects.obj` | `FUN_0009e560` | `effect_marker_list_get_markers_by_name` | ✅ Yes | No |
| `0x9f3b0` | `effects.obj` | `FUN_0009f3b0` | `material_effect_visible` | ✅ Yes | No |
| `0x9f430` | `effects.obj` | `FUN_0009f430` | `material_effect_new` | ✅ Yes | No |
| `0x1345b0` | `glow.obj` | `FUN_001345b0` | `glow_update` | ✅ Yes | Yes (`@<reg>`) |
| `0x9f570` | `particle_systems.obj` | `FUN_0009f570` | `material_effect_new_from_point` | ✅ Yes | No |
| `0x9f920` | `particle_systems.obj` | `FUN_0009f920` | `particle_system_next_type_state_index` | ✅ Yes | Yes (`@<reg>`) |
| `0x9f9d0` | `particle_systems.obj` | `FUN_0009f9d0` | `particle_system_next_particle_state_index` | ✅ Yes | Yes (`@<reg>`) |
| `0x9fa60` | `particle_systems.obj` | `FUN_0009fa60` | `particle_system_update_default` | ✅ Yes | No |
| `0x9fad0` | `particle_systems.obj` | `FUN_0009fad0` | `particle_system_new_particle_default` | ✅ Yes | No |
| `0x9fb10` | `particle_systems.obj` | `FUN_0009fb10` | `particle_system_update_particle_default` | ✅ Yes | No |
| `0x9fca0` | `particle_systems.obj` | `FUN_0009fca0` | `particle_system_update_explosion` | ✅ Yes | No |
| `0x9fd30` | `particle_systems.obj` | `FUN_0009fd30` | `particle_system_new_particles` | ✅ Yes | Yes (`@<reg>`) |
| `0xa0080` | `particle_systems.obj` | `FUN_000a0080` | `randomize_particle_variables` | ✅ Yes | Yes (`@<reg>`) |
| `0xa0180` | `particle_systems.obj` | `FUN_000a0180` | `particle_system_update` | ✅ Yes | No |
| `0xa0800` | `particle_systems.obj` | `FUN_000a0800` | `particle_system_render` | ✅ Yes | Yes (`@<reg>`) |
| `0xa0d50` | `particle_systems.obj` | `FUN_000a0d50` | `particle_system_new_particle_explosion` | ✅ Yes | No |
| `0xa0e60` | `particle_systems.obj` | `FUN_000a0e60` | `particle_system_new_particle_jet` | ✅ Yes | No |
| `0xa0fd0` | `particle_systems.obj` | `FUN_000a0fd0` | `particle_system_initialize` | ✅ Yes | No |
| `0xa1170` | `particle_systems.obj` | `particle_system_update` | `particle_systems_render` | ✅ Yes | No |
| `0xa1210` | `particles.obj` | `FUN_000a1210` | `particle_system_new_unattached` | ✅ Yes | No |
| `0xa12e0` | `particles.obj` | `FUN_000a12e0` | `particle_system_new_attached` | ✅ Yes | No |
| `0xa1510` | `particles.obj` | `FUN_000a1510` | `particles_stop_on_first_person_weapon` | ✅ Yes | No |
| `0xa1590` | `particles.obj` | `FUN_000a1590` | `particles_reconnect_to_structure_bsp` | ✅ Yes | No |
| `0xa1770` | `particles.obj` | `FUN_000a1770` | `particle_effect_new` | ✅ Yes | Yes (`@<reg>`) |
| `0xa18c0` | `particles.obj` | `FUN_000a18c0` | `particle_die` | ✅ Yes | Yes (`@<reg>`) |
| `0xa1910` | `particles.obj` | `FUN_000a1910` | `particle_next_sequence` | ✅ Yes | No |
| `0xa1a90` | `particles.obj` | `FUN_000a1a90` | `particle_next_frame` | ✅ Yes | Yes (`@<reg>`) |
| `0xa1b60` | `particles.obj` | `FUN_000a1b60` | `particle_update_frame_time` | ✅ Yes | Yes (`@<reg>`) |
| `0xa1c30` | `particles.obj` | `FUN_000a1c30` | `particle_update_physics` | ✅ Yes | No |
| `0xa3e60` | `weather_particle_systems.obj` | `FUN_000a3e60` | `weather_particle_system_get` | ✅ Yes | Yes (`@<reg>`) |
| `0xa3ea0` | `weather_particle_systems.obj` | `FUN_000a3ea0` | `weather_particle_system_get_type` | ✅ Yes | Yes (`@<reg>`) |
| `0xa4000` | `weather_particle_systems.obj` | `FUN_000a4000` | `weather_particle_system_wrap_point` | ✅ Yes | Yes (`@<reg>`) |
| `0xa40a0` | `weather_particle_systems.obj` | `FUN_000a40a0` | `weather_particle_system_new` | ✅ Yes | No |
| `0xa4200` | `weather_particle_systems.obj` | `FUN_000a4200` | `weather_particle_system_delete` | ✅ Yes | No |
| `0xa4310` | `weather_particle_systems.obj` | `FUN_000a4310` | `weather_particle_system_new_particle` | ✅ Yes | Yes (`@<reg>`) |
| `0xa45d0` | `weather_particle_systems.obj` | `FUN_000a45d0` | `weather_particle_system_box_offset_from_point3d` | ✅ Yes | Yes (`@<reg>`) |
| `0xa4610` | `weather_particle_systems.obj` | `FUN_000a4610` | `weather_particle_update_physics` | ✅ Yes | Yes (`@<reg>`) |
| `0xa48c0` | `weather_particle_systems.obj` | `FUN_000a48c0` | `weather_particle_system_build_clipping_planes` | ✅ Yes | Yes (`@<reg>`) |
| `0xa4a00` | `weather_particle_systems.obj` | `FUN_000a4a00` | `weather_polyhedra_find` | ✅ Yes | No |
| `0xa4ab0` | `weather_particle_systems.obj` | `FUN_000a4ab0` | `weather_particle_system_update_particle_count` | ✅ Yes | Yes (`@<reg>`) |
| `0xa4be0` | `weather_particle_systems.obj` | `FUN_000a4be0` | `weather_particle_system_update` | ✅ Yes | No |
| `0x18ff00` | `wind.obj` | `FUN_0018ff00` | `wind_variance_get` | ✅ Yes | Yes (`@<reg>`) |
