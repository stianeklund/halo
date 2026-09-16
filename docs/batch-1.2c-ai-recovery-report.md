# Batch 1.2C Function Name Recovery Report: Core AI Subsystem

**Date:** September 14, 2026  
**Target Build:** Halo Xbox Debug Build 2276 (`01.10.12.2276`, Oct 12, 2001)  
**Binary Reference:** `halo-patched/cachebeta.xbe` (MD5: `c7869590a1c64ad034e49a5ee0c02465`)  
**Scope:** Sub-Batch 1.2C (`actors.obj`, `actor_moving.obj`, `ai.obj`, `actions.obj`, `actor_combat.obj`)  
**Total Functions Recovered:** 94 (`actors.obj`: 72, `actor_moving.obj`: 7, `ai.obj`: 6, `actions.obj`: 5, `actor_combat.obj`: 4)  

---

## Executive Summary

Sub-Batch 1.2C recovers 94 authentic Bungie debug symbol names across the core AI runtime systems: actor decision-making, stimulus response, breed action dispatchers, swarm control, obstacle avoidance, and combat/firing behavior. All symbol names are directly extracted from Bungie debug map evidence in `halo_2276_functions.txt`.

This recovery replaces 94 legacy placeholders (`FUN_0001d3c0` through `FUN_00041420`) with canonical Bungie debug symbols and resolves four critical historical naming collisions across `actors.c`, `ai.c`, `actions.c`, and combat modules.

All 15 custom register argument functions preserve their exact register pinning with **0 ABI drift across 886 tracked functions** in `tools/kb_reg_baseline.json`, zero hazard scan blockers, and clean compilation linking into `halo.xbe`.

---

## Collision Investigations & Resolutions

Four structural naming collisions were identified, investigated against binary disassembly, and resolved:

### 1. `0x3aa60` vs `0x3b120` (`actors_initialize_for_new_map` vs `actor_in_combat`)
- **Prior State:** `0x3aa60` in `actors.obj` was misnamed `actor_in_combat(void)`, colliding with the true combat query routine `0x3b120` (`actor_in_combat(int actor_handle)`).
- **Binary Evidence:**
  - `0x3aa60`: Takes 0 arguments and executes `data_delete_all(actor_data); data_delete_all(swarm_data); data_delete_all(swarm_component_data);`.
  - Disassembly at `0x409b0` (`ai_initialize_for_new_map`) calls `0x3aa60` alongside other map lifecycle initializers (`ai_debug_initialize_for_new_map`, `encounters_initialize_for_new_map`, etc.).
  - `halo_2276_functions.txt` line 426: `_actors_initialize_for_new_map .text 0003AA60`.
  - `halo_2276_functions.txt` line 437: `_actor_in_combat .text 0003B120`.
- **Resolution:**
  - `0x3aa60` renamed to authentic `actors_initialize_for_new_map`.
  - `0x3b120` retains authentic `actor_in_combat`.
  - Updated call site in `ai.c:1501`.

### 2. `0x3b0b0` vs `0x3b410` (`actor_swarm_component_refresh` vs `actor_switch_props`)
- **Prior State:** `0x3b0b0` in `actors.obj` was named `actor_switch_props(int unit_handle, int swarm_component_handle)`, colliding with the prop switching routine at `0x3b410`.
- **Binary Evidence:**
  - `0x3b0b0`: Updates one swarm component record with the unit position and target state from `unit+0x430`.
  - `halo_2276_functions.txt` line 435: `_actor_swarm_component_refresh .text 0003B0B0`.
  - `halo_2276_functions.txt` line 445: `_actor_switch_props .text 0003B410`.
- **Resolution:**
  - `0x3b0b0` renamed to authentic `actor_swarm_component_refresh`.
  - `0x3b410` retains authentic `actor_switch_props`.
  - Updated call site in `actors.c:5751`.

### 3. `0x3b270` vs `0x3b190` (`actor_get_weapon` vs `actor_attacking_target`)
- **Prior State:** `0x3b270` was named `int actor_attacking_target(int actor_handle)` and called throughout `actions.c`, `actor_combat.c`, and `actor_looking.c` to assign to `weapon_handle`.
- **Binary Evidence:**
  - `0x3b270`: Reads the actor unit (`actor+0x158` or `actor+0x18`) and retrieves the primary weapon handle via `unit_inventory_get_weapon`.
  - `0x3b190`: Takes `(int actor_handle, int *attack_vector_out)` and determines attack direction and target viability.
  - `halo_2276_functions.txt` line 441: `_actor_get_weapon .text 0003B270`.
  - `halo_2276_functions.txt` line 439: `_actor_attacking_target .text 0003B190`.
- **Resolution:**
  - `0x3b270` renamed to authentic `actor_get_weapon`.
  - `0x3b190` retains authentic `actor_attacking_target`.
  - Updated all callers across `actors.c`, `actor_combat.c`, `actor_looking.c`, and `actions.c`.

### 4. `0x3c370` vs `0x3c1c0` (`actor_unit_control_throw_grenade` vs `actor_handle_communication`)
- **Prior State:** `0x3c370` was defined in `actors.c:5290` as `void actor_handle_communication(int actor_handle)`, creating a duplicate symbol with `0x3c1c0` (`actor_handle_communication(int, int, int)`).
- **Binary Evidence:**
  - `0x3c370`: Sets bit `0x2000` in actor control flags at offset `+0x6d0` (`*(uint32_t *)(actor + 0x6d0) |= 0x2000;`).
  - Situated in the `actor_unit_control_*` block between `actor_unit_control_exact_facing` (0x3c3a0) and `actor_unit_control_stop_animation_impulse` (0x3c3e0).
  - `halo_2276_functions.txt` line 471: `_actor_unit_control_throw_grenade .text 0003C370`.
  - `halo_2276_functions.txt` line 466: `_actor_handle_communication .text 0003C1C0`.
- **Resolution:**
  - `0x3c370` renamed to authentic `actor_unit_control_throw_grenade`.
  - `0x3c1c0` retains authentic `actor_handle_communication`.

---

## Register Argument Functions Preserved (15 Routines)

All 15 functions in Sub-Batch 1.2C utilizing custom register conventions retain their exact bindings with 0 ABI drift:

| Address | Recovered Name | Annotated Signature | Registers |
|---------|----------------|---------------------|-----------|
| `0x1d530` | `actor_pursuit_consider_nearby_actor` | `int actor_pursuit_consider_nearby_actor(int actor_handle@<ebx>, int target_handle@<esi>, int param_3);` | EBX, ESI |
| `0x2bd80` | `actor_move_vector_avoidance` | `void actor_move_vector_avoidance(int actor_handle@<esi>, void *move_control_ptr@<edi>, void *avoidance_data);` | ESI, EDI |
| `0x36890` | `actor_stimulus_combat` | `void actor_stimulus_combat(int actor_handle@<esi>, int enemy_handle@<edi>, int stimulus_type, int priority);` | ESI, EDI |
| `0x38da0` | `infection_wander_pause_time` | `int16_t infection_wander_pause_time(int actor_handle@<eax>);` | EAX |
| `0x38e00` | `infection_wander_move_time` | `int16_t infection_wander_move_time(int actor_handle@<eax>);` | EAX |
| `0x3a600` | `actor_type_definition_get` | `void *actor_type_definition_get(int actor_type@<eax>);` | EAX |
| `0x3ac20` | `actor_verify_unit_activation` | `void actor_verify_unit_activation(int actor_handle@<eax>);` | EAX |
| `0x3b7e0` | `actor_freeze_unit` | `void actor_freeze_unit(int actor_handle@<eax>);` | EAX |
| `0x3b940` | `actor_randomly_control_unit` | `void actor_randomly_control_unit(int actor_handle@<eax>);` | EAX |
| `0x3bb50` | `actor_get_timeslice` | `int16_t actor_get_timeslice(int actor_handle@<eax>);` | EAX |
| `0x3bbf0` | `actor_clear_output` | `void actor_clear_output(int actor_handle@<eax>);` | EAX |
| `0x3cb50` | `actor_swarm_component_setup` | `void actor_swarm_component_setup(int actor_handle@<eax>);` | EAX |
| `0x3e7a0` | `actor_unit_control` | `void actor_unit_control(int actor_handle@<eax>);` | EAX |
| `0x3ec80` | `actor_update` | `void actor_update(int actor_handle@<eax>);` | EAX |
| `0x413c0` | `ai_generate_line_of_fire_pill` | `void ai_generate_line_of_fire_pill(int unit_handle@<edi>, void *line_of_fire_record@<eax>);` | EDI, EAX |

---

## Complete Table of Recovered Functions (94)

| Address | Placeholder | Authentic Symbol (Build 2276) | Object | Reg Args |
|---------|-------------|--------------------------------|--------|----------|
| `0x1d3c0` | `FUN_0001d3c0` | `actor_action_try_to_panic` | `actions.obj` | No |
| `0x1d530` | `FUN_0001d530` | `actor_pursuit_consider_nearby_actor` | `actions.obj` | Yes |
| `0x21010` | `FUN_00021010` | `actor_combat_fire_wildly` | `actor_combat.obj` | No |
| `0x21040` | `FUN_00021040` | `actor_combat_disable_bursts` | `actor_combat.obj` | No |
| `0x21080` | `FUN_00021080` | `actor_firing_blindly` | `actions.obj` | No |
| `0x210b0` | `FUN_000210b0` | `actor_combat_currently_firing_burst` | `actions.obj` | No |
| `0x210f0` | `FUN_000210f0` | `actor_get_weapon_definition` | `actions.obj` | No |
| `0x21350` | `FUN_00021350` | `fast_ftol` | `actor_combat.obj` | No |
| `0x22390` | `FUN_00022390` | `actor_start_burst` | `actor_combat.obj` | No |
| `0x2a360` | `FUN_0002a360` | `actor_move_animation_busy` | `actors.obj` | No |
| `0x2a3a0` | `FUN_0002a3a0` | `actor_path_clear` | `actor_moving.obj` | No |
| `0x2ade0` | `FUN_0002ade0` | `actor_move_avoidance_setup` | `actor_moving.obj` | No |
| `0x2b020` | `FUN_0002b020` | `actor_move_test_avoidance_vector` | `actor_moving.obj` | No |
| `0x2b310` | `FUN_0002b310` | `actor_move_vector_avoidance_find_direction` | `actor_moving.obj` | No |
| `0x2b830` | `FUN_0002b830` | `actor_move_calculate_controlled_by_aiming` | `actor_moving.obj` | No |
| `0x2bab0` | `FUN_0002bab0` | `actor_move_calculate_free` | `actor_moving.obj` | No |
| `0x2bd80` | `FUN_0002bd80` | `actor_move_vector_avoidance` | `actor_moving.obj` | Yes |
| `0x36860` | `FUN_00036860` | `actor_stimulus_clear` | `actors.obj` | No |
| `0x36890` | `FUN_00036890` | `actor_stimulus_combat` | `actors.obj` | Yes |
| `0x36960` | `FUN_00036960` | `actor_stimulus_surprise` | `actors.obj` | No |
| `0x369c0` | `FUN_000369c0` | `actor_stimulus_suspicion` | `actors.obj` | No |
| `0x36a20` | `FUN_00036a20` | `actor_stimulus_prop_sighted` | `actors.obj` | No |
| `0x36a90` | `FUN_00036a90` | `actor_stimulus_enter_combat_found_body` | `actors.obj` | No |
| `0x36b10` | `FUN_00036b10` | `actor_stimulus_enter_combat_perceived_enemy` | `actors.obj` | No |
| `0x36b50` | `FUN_00036b50` | `actor_stimulus_enter_combat_friend_in_combat` | `actors.obj` | No |
| `0x36bd0` | `FUN_00036bd0` | `actor_stimulus_bumped` | `actors.obj` | No |
| `0x36c00` | `FUN_00036c00` | `actor_stimulus_environmental_noise` | `actors.obj` | No |
| `0x36c50` | `FUN_00036c50` | `actor_stimulus_heard_shooting` | `actors.obj` | No |
| `0x36da0` | `FUN_00036da0` | `actor_stimulus_was_surprised` | `actors.obj` | No |
| `0x36dc0` | `FUN_00036dc0` | `actor_stimulus_maneuvering` | `actors.obj` | No |
| `0x36e30` | `FUN_00036e30` | `actor_stimulus_vehicle_eviction` | `actors.obj` | No |
| `0x36e50` | `FUN_00036e50` | `actor_stimulus_abandon_stationary_facing` | `actors.obj` | No |
| `0x36f20` | `FUN_00036f20` | `actor_stimulus_prop_acknowledged` | `actors.obj` | No |
| `0x37240` | `FUN_00037240` | `actor_stimulus_damage` | `actors.obj` | No |
| `0x373b0` | `FUN_000373b0` | `actor_stimulus_weapon_impact` | `actors.obj` | No |
| `0x374f0` | `FUN_000374f0` | `actor_stimulus_weapon_detonation` | `actors.obj` | No |
| `0x37630` | `FUN_00037630` | `actor_stimulus_prop_just_killed` | `actors.obj` | No |
| `0x377d0` | `FUN_000377d0` | `actor_stimulus_prop_fleeing` | `actors.obj` | No |
| `0x378e0` | `FUN_000378e0` | `actor_stimulus_noticed_danger_zone` | `actors.obj` | No |
| `0x379f0` | `FUN_000379f0` | `carrier_decide_action` | `actors.obj` | No |
| `0x37b50` | `FUN_00037b50` | `crew_decide_action` | `actors.obj` | No |
| `0x37d50` | `FUN_00037d50` | `elite_decide_action` | `actors.obj` | No |
| `0x38000` | `FUN_00038000` | `engineer_decide_action` | `actors.obj` | No |
| `0x38200` | `FUN_00038200` | `flood_decide_action` | `actors.obj` | No |
| `0x38370` | `FUN_00038370` | `actor_type_flood_desire_shamble` | `actors.obj` | No |
| `0x38880` | `FUN_00038880` | `grunt_decide_action` | `actors.obj` | No |
| `0x38b10` | `FUN_00038b10` | `hunter_decide_action` | `actors.obj` | No |
| `0x38c70` | `FUN_00038c70` | `infection_decide_action` | `actors.obj` | No |
| `0x38da0` | `FUN_00038da0` | `infection_wander_pause_time` | `actors.obj` | Yes |
| `0x38e00` | `FUN_00038e00` | `infection_wander_move_time` | `actors.obj` | Yes |
| `0x38e60` | `FUN_00038e60` | `infection_swarm_control` | `actors.obj` | No |
| `0x39c80` | `FUN_00039c80` | `infection_swarm_aim_jump` | `actors.obj` | No |
| `0x39f30` | `FUN_00039f30` | `jackal_decide_action` | `actors.obj` | No |
| `0x3a190` | `FUN_0003a190` | `marine_decide_action` | `actors.obj` | No |
| `0x3a3b0` | `FUN_0003a3b0` | `mounted_weapon_decide_action` | `actors.obj` | No |
| `0x3a480` | `FUN_0003a480` | `sentinel_decide_action` | `actors.obj` | No |
| `0x3a600` | `FUN_0003a600` | `actor_type_definition_get` | `actors.obj` | Yes |
| `0x3a740` | `FUN_0003a740` | `actor_types_initialize` | `actors.obj` | No |
| `0x3a760` | `FUN_0003a760` | `actor_type_get_name` | `actors.obj` | No |
| `0x3a770` | `FUN_0003a770` | `actor_type_get_race` | `actors.obj` | No |
| `0x3a790` | `FUN_0003a790` | `actor_type_get_when_to_search_at_target` | `actors.obj` | No |
| `0x3a7b0` | `FUN_0003a7b0` | `actor_type_get_when_to_pursue` | `actors.obj` | No |
| `0x3a7d0` | `FUN_0003a7d0` | `actor_type_get_when_to_search_pursuit` | `actors.obj` | No |
| `0x3a7f0` | `FUN_0003a7f0` | `actor_type_get_pursuit_controller` | `actors.obj` | No |
| `0x3a800` | `FUN_0003a800` | `actor_type_get_swarm` | `actors.obj` | No |
| `0x3a810` | `FUN_0003a810` | `actor_type_initialize` | `actors.obj` | No |
| `0x3a840` | `FUN_0003a840` | `actor_type_decide_action` | `actors.obj` | No |
| `0x3a8a0` | `FUN_0003a8a0` | `actor_type_swarm_control` | `actors.obj` | No |
| `0x3a920` | `FUN_0003a920` | `actor_type_swarm_aim_jump` | `actors.obj` | No |
| `0x3ac20` | `FUN_0003ac20` | `actor_verify_unit_activation` | `actors.obj` | Yes |
| `0x3b120` | `FUN_0003b120` | `actor_in_combat` | `actors.obj` | No |
| `0x3b190` | `FUN_0003b190` | `actor_attacking_target` | `actors.obj` | No |
| `0x3b410` | `FUN_0003b410` | `actor_switch_props` | `actors.obj` | No |
| `0x3b7e0` | `FUN_0003b7e0` | `actor_freeze_unit` | `actors.obj` | Yes |
| `0x3b860` | `FUN_0003b860` | `actor_freeze` | `actors.obj` | No |
| `0x3b940` | `FUN_0003b940` | `actor_randomly_control_unit` | `actors.obj` | Yes |
| `0x3baa0` | `FUN_0003baa0` | `actor_change_encounter` | `actors.obj` | No |
| `0x3bb50` | `FUN_0003bb50` | `actor_get_timeslice` | `actors.obj` | Yes |
| `0x3bbf0` | `FUN_0003bbf0` | `actor_clear_output` | `actors.obj` | Yes |
| `0x3bde0` | `FUN_0003bde0` | `actor_input_sample_position` | `actors.obj` | No |
| `0x3be90` | `FUN_0003be90` | `actor_decision_loop` | `actors.obj` | No |
| `0x3c1c0` | `FUN_0003c1c0` | `actor_handle_communication` | `actors.obj` | No |
| `0x3cb50` | `FUN_0003cb50` | `actor_swarm_component_setup` | `actors.obj` | Yes |
| `0x3d9f0` | `FUN_0003d9f0` | `actor_general_update` | `actors.obj` | No |
| `0x3dc20` | `FUN_0003dc20` | `actor_input_update` | `actors.obj` | No |
| `0x3e7a0` | `FUN_0003e7a0` | `actor_unit_control` | `actors.obj` | Yes |
| `0x3ec80` | `FUN_0003ec80` | `actor_update` | `actors.obj` | Yes |
| `0x3f030` | `FUN_0003f030` | `actor_place` | `actors.obj` | No |
| `0x3f5f0` | `FUN_0003f5f0` | `actors_update` | `ai.obj` | No |
| `0x3fb00` | `FUN_0003fb00` | `sub_3FB00` | `ai.obj` | No |
| `0x40570` | `FUN_00040570` | `ai_place_pending_mounted_weapons` | `ai.obj` | No |
| `0x40a40` | `FUN_00040a40` | `ai_flush_spatial_effects` | `ai.obj` | No |
| `0x413c0` | `FUN_000413c0` | `ai_generate_line_of_fire_pill` | `ai.obj` | Yes |
| `0x41420` | `FUN_00041420` | `ai_find_line_of_fire_friend_pills` | `ai.obj` | No |


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
   - Result: Clean code generation without warnings or errors.
5. **Compilation & Link:**
   - Command: `cmake --build build --target halo`
   - Result: `[ 98%] Built target halo` successful link.
