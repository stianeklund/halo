# Batch 4.2: Weapon Subsystem Recovery Report

## Executive Summary

- **Batch Target:** Batch 4.2: Weapon Subsystem and First-Person Weapon Interface Recovery (Phase 4).
- **Functions Recovered:** 40 functions across `weapons.obj` and `first_person_weapons.obj`.
- **Total Code Recovered:** 15,027 bytes of lifted C89 engine logic.
- **Verification Status:**
  - `extract_reg_args.py --check`: **PASS** (917 OK, 0 drift, 0 missing, 0 stale).
  - `check_param_types.py --check`: **PASS** (0 new type mismatches, 0 new errors).
  - `check_lift_hazards.py --changed-only`: **PASS** (0 errors; verified `random_direction3d` duplicate `proj_forward` vector parameter represents in-place 3D vector rotation matching pristine XBE at `0xfda95–0xfdac6`).
  - `patched_xbe` build: **PASS** (exit code 0, generating valid `halo-patched/default.xbe`).

---

## Detailed Function Inventory

### 1. `src/halo/items/weapons.c` (31 functions, 10,369 bytes)

| Address | Size (Bytes) | Name | Signature | Description |
|---|---|---|---|---|
| `0x0fad40` | 1 | `weapon_stop_fire_empty_effect` | `void weapon_stop_fire_empty_effect(int weapon_handle);` | Stub/reset empty-fire effect callback. |
| `0x0fad50` | 1 | `weapon_stop_charging_effect` | `void weapon_stop_charging_effect(int weapon_handle);` | Stub/reset charge effect callback. |
| `0x0faf50` | 132 | `FUN_000faf50` | `char FUN_000faf50(int weapon_handle);` | Weapon state predicate checking ready status and parent unit conditions. |
| `0x0fafe0` | 44 | `FUN_000fafe0` | `int FUN_000fafe0(int weapon_handle);` | Weapon magazine index query. |
| `0x0fb010` | 108 | `FUN_000fb010` | `int FUN_000fb010(int weapon_handle);` | Secondary magazine index query and state check. |
| `0x0fb450` | 33 | `weapon_magazine_is_full` | `char weapon_magazine_is_full(int weapon_handle, short magazine_index);` | Checks whether weapon magazine is at capacity. |
| `0x0fb480` | 58 | `weapon_magazine_is_empty` | `char weapon_magazine_is_empty(int weapon_handle, short magazine_index);` | Checks whether weapon magazine has zero remaining rounds. |
| `0x0fb4c0` | 71 | `weapon_get_rounds_loaded` | `int16_t weapon_get_rounds_loaded(int weapon_handle, short magazine_index);` | Retrieves count of loaded rounds in magazine. |
| `0x0fc780` | 342 | `weapon_reload` | `char weapon_reload(int weapon_handle, short magazine_index);` | Initiates reload sequence for specified weapon magazine. |
| `0x0fc8e0` | 77 | `FUN_000fc8e0` | `void FUN_000fc8e0(int weapon_handle, short magazine_index);` | Cancels pending reload and clears magazine reload state. |
| `0x0fb080` | 1 | `weapon_ready` | `void weapon_ready(int weapon_handle);` | Marks weapon as readied for active combat. |
| `0x0fb410` | 50 | `weapon_get_total_rounds` | `int16_t weapon_get_total_rounds(int weapon_handle, short magazine_index);` | Retrieves total available ammunition (inventory + chamber). |
| `0x0fb5a0` | 234 | `weapon_reset` | `void weapon_reset(int weapon_handle);` | Resets weapon state, animation timers, and trigger counters. |
| `0x0fb690` | 70 | `weapon_idle` | `void weapon_idle(int weapon_handle);` | Transitions weapon to idle animation/state. |
| `0x0fb840` | 49 | `FUN_000fb840` | `void FUN_000fb840(int weapon_handle);` | Updates weapon zoom magnification factor. |
| `0x0fb990` | 74 | `weapon_drop` | `void weapon_drop(int weapon_handle);` | Handles weapon drop, clearing owner and resetting inputs. |
| `0x0fb9e0` | 18 | `FUN_000fb9e0` | `void FUN_000fb9e0(int weapon_handle);` | Secondary weapon drop cleanup helper. |
| `0x0fba00` | 32 | `FUN_000fba00` | `void FUN_000fba00(int weapon_handle);` | Weapon owner detachment notification. |
| `0x0fbcf0` | 15 | `FUN_000fbcf0` | `void FUN_000fbcf0(int weapon_handle);` | Clears weapon heat dissipation state. |
| `0x0fbd00` | 15 | `FUN_000fbd00` | `void FUN_000fbd00(int weapon_handle);` | Resets weapon recoil accumulator. |
| `0x0fbd10` | 388 | `FUN_000fbd10` | `void FUN_000fbd10(int weapon_handle, float dt);` | Computes heat dissipation, cooling rates, and overheat triggers. |
| `0x0fbf00` | 835 | `weapon_export_function_values` | `void weapon_export_function_values(int weapon_handle);` | Exports runtime function values (heat, ammo fraction, charge) to shaders. |
| `0x0fcc90` | 122 | `weapon_magazine_finish_chamber` | `void weapon_magazine_finish_chamber(int16_t magazine_index, int weapon_handle);` | Completes round chambering transition. |
| `0x0fcec0` | 83 | `FUN_000fcec0` | `void FUN_000fcec0(int trigger_index@<eax>, int weapon_handle@<ebx>);` | Clears weapon trigger tracking and state. |
| `0x0fd0b0` | 149 | `projectile_distribute` | `void projectile_distribute(int weapon_handle, short trigger_index);` | Calculates projectile spread cone and distribution angles. |
| `0x0fd520` | 79 | `weapon_trigger_finish_tracking` | `void weapon_trigger_finish_tracking(int weapon_handle, int trigger_index);` | Concludes tracking and resets target lock datum. |
| `0x0fd570` | 1,822 | `trigger_create_projectiles` | `void trigger_create_projectiles(int weapon_handle, int16_t trigger_index);` | Spawns projectiles, calculates error cones, and triggers fire events. |
| `0x0fdc90` | 1,970 | `FUN_000fdc90` | `void FUN_000fdc90(int weapon_handle, int trigger_index);` | Handles trigger firing mechanics, heat accumulation, burst timers, and haptics. |
| `0x0fe450` | 620 | `weapon_trigger_begin_firing` | `void weapon_trigger_begin_firing(int weapon_handle, int16_t trigger_index, char flag);` | Initiates trigger firing sequence, charge-up, and animation changes. |
| `0x0fe890` | 127 | `FUN_000fe890` | `void FUN_000fe890(int trigger_index@<eax>, int weapon_handle@<ebx>);` | Resets charging effects and triggers discharge if overcharged. |
| `0x0fe910` | 2,749 | `weapon_update` | `boolean weapon_update(int weapon_handle);` | Master weapon subsystem per-frame update loop. |

---

### 2. `src/halo/interface/first_person_weapons.c` (9 functions, 4,658 bytes)

| Address | Size (Bytes) | Name | Signature | Description |
|---|---|---|---|---|
| `0x0dc790` | 1 | `first_person_weapons_dispose` | `void first_person_weapons_dispose(void);` | Shuts down first-person weapons system on map unload. |
| `0x0dce80` | 642 | `first_person_weapon_draw` | `void first_person_weapon_draw(void);` | Renders first-person weapon model, hands, and equipment meshes. |
| `0x0dd190` | 204 | `first_person_weapon_get_marker_by_name` | `int16_t first_person_weapon_get_marker_by_name(int object_handle, void *marker_name, void *out_markers, int max_count);` | Resolves first-person model markers by tag and animation matrices. |
| `0x0dd260` | 216 | `first_person_weapon_center_flashlight` | `void first_person_weapon_center_flashlight(int object_handle, float *out_position, float *out_forward, void *out_up);` | Positions flashlight beam source relative to first-person weapon model. |
| `0x0dd340` | 203 | `first_person_weapon_adjust_light` | `char first_person_weapon_adjust_light(int object_handle, int marker_result, void *out_position, void *out_forward, void *out_up);` | Adjusts dynamic weapon light positions for local viewport. |
| `0x0dd580` | 1,368 | `first_person_weapon_update` | `void first_person_weapon_update(int16_t local_player_index);` | Per-frame animation, interpolation, and state progression for local player weapon. |
| `0x0ddae0` | 173 | `first_person_weapon_render_update` | `void first_person_weapon_render_update(void);` | Updates viewmodels for all active local viewports prior to scene render. |
| `0x0de3f0` | 318 | `first_person_weapon_next_state` | `void first_person_weapon_next_state(int16_t local_player_index);` | Determines next first-person state upon animation sequence completion. |
| `0x0de560` | 1,533 | `FUN_000de560` | `void FUN_000de560(int16_t local_player_index);` | Core first-person weapon state machine, physics dampening, and motion sway. |

---

## Architectural Notes

1. **Strict C89 Compliance:**
   - Block scope variable declarations were enforced across large functions (`FUN_000fdc90`, `weapon_update`, `FUN_000de560`).
   - Clean separation of declarations from statements ensures compatibility with MSVC / Clang toolchains.

2. **Register ABI Preservation:**
   - Register argument invariants (`0x0fcec0` and `0x0fe890` with `@<eax>` and `@<ebx>`) remain strictly preserved in `kb.json` and generated thunks.
   - `extract_reg_args.py --check` confirms 0 drift across all 917 registered ABI functions.

3. **Hazard Audit:**
   - Duplicate argument scan warning on `random_direction3d` confirmed as benign in-place 3D vector randomization matching original binary behavior.
