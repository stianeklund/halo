# Halo CE Xbox Build 2276 Function Recovery: Comprehensive Methodology & Subsystem Catalog

**Target Build:** Halo Xbox Debug Build 2276 (`01.10.12.2276`, Oct 12, 2001)  
**Binary Reference:** `halo-patched/cachebeta.xbe` (MD5: `c7869590a1c64ad034e49a5ee0c02465`)  
**Evidence Source:** `halo_2276_functions.txt` (11,125 symbols from Bungie debug map)  

---

## 1. Provenance & Evidence Methodology

### 1.1 The Source of Truth
The authentic Bungie function names in this project are not speculative guesses or modern re-creations. They are recovered directly from the **Xbox Debug Build 2276** binary and its corresponding debug map (`halo_2276_functions.txt`). Debug build 2276 contains complete internal symbol tables, rich assertion strings with authentic original C file paths (e.g. `c:\halo\SOURCE\interface\virtual_keyboard.c`), line numbers, and explicit variable names preserved in `.rdata` strings.

Every entry in `halo_2276_functions.txt` provides:
1. **Mangled / Underscored Symbol Name:** e.g. `_actor_action_try_to_panic`
2. **PE Section:** `.text` (Halo game code), `.rdata`, or SDK libraries
3. **Hexadecimal Address:** e.g. `0001D3C0` (corresponds directly to VA `0x1d3c0` in `cachebeta.xbe`)
4. **Function Code Size:** In bytes (e.g. `00000056` = 86 bytes)
5. **Stack Frame Information:** Stack locals and stack argument sizes

### 1.2 Demangling and Address Normalization
- Addresses from `halo_2276_functions.txt` are normalized to lowercase hexadecimal with a `0x` prefix (e.g. `0001D3C0` $\rightarrow$ `0x1d3c0`), matching the canonical key format in `kb.json`.
- Standard C cdecl symbols have their MSVC leading underscore stripped (e.g. `_first_person_weapons_update` $\rightarrow$ `first_person_weapons_update`).
- Fastcall/register symbols or C++ class methods (e.g. `??0...`) are preserved with their exact signatures.

### 1.3 Disassembly & Assert Cross-Verification
Before renaming any function, three independent sources of truth are corroborated:
1. **Address & Size Match:** The function's start address and instruction length in `halo_2276_functions.txt` must match `kb.json` and the disassembly bounds in `function_bounds.json`.
2. **Inline Assert Verification:** Assertions in the disassembly or lifted C source code are checked for original source filenames and function names (e.g., `display_assert("first_person_weapons", "c:\\halo\\SOURCE\\interface\\first_person_weapons.c", 0xf0, 1)` confirms `first_person_weapons.c`).
3. **Call Site Semantics:** Callers of the function are analyzed to ensure arguments, return types, and side-effects match the semantic purpose of the Bungie symbol.

---

## 2. Collision Investigation & Resolution Doctrine

A frequent challenge in legacy decompilation codebases is **historical naming drift and displacement**:
- Early reversers named a function based on external behavior (e.g., calling `0x96850` `device_effect_new`).
- When another reverser encountered the *real* `device_effect_new` at `0x967a0`, they found the name was already taken in `kb.json` and fell back to generating a placeholder (`FUN_000967a0`).
- This created a chain of displaced functions across the knowledge base.

### Disambiguation Process
1. **Global Collision Scanning:** Every proposed rename is simulated against the entire 9,293-entry knowledge base using automated collision detectors.
2. **Simultaneous Swapping:** When two functions have swapped names (e.g. $A \rightarrow B$ and $B \rightarrow C$), both are renamed simultaneously using two-stage unique replacement tokens (`___HALO_TOK_XXX___`). This prevents sequential regex passes from chaining $A \rightarrow B \rightarrow C$.
3. **Disassembly Ground Truth:** In all collision cases, the binary disassembly in `cachebeta.xbe` is the ultimate arbiter.

---

## 3. ABI Register Invariance (`@<reg>`)

Halo CE on the original Xbox was compiled with Microsoft Visual C++ 7.1 (`cl.exe` version 13.10.3077). The MSVC optimizer frequently emitted custom register-passed calling conventions for non-leaf functions (passing arguments in `EAX`, `ECX`, `EDX`, `EBX`, `ESI`, `EDI`, `BL`, etc.).

### Zero-Drift Policy
- In `kb.json`, all register parameters are annotated with `@<reg>` (e.g. `int actor_handle@<ebx>`).
- Register annotations are tracked across **886 functions** in `tools/kb_reg_baseline.json`.
- When a function is renamed, its declaration in **both** sections of `tools/kb_reg_baseline.json` (the top-level dictionary and the `"functions"` object) is updated with the exact same updated identifier while preserving 100% of argument types, parameter names, and register tags.
- Verified before every commit using `tools/audit/extract_reg_args.py --check`.

---

## 4. Completed Batches: Work & Functional Catalog

### Batch 1.1: HaloScript Evaluators & Dispatch (112 Functions)
* **Status:** Complete — [PR #10](https://github.com/stianeklund/halo/pull/10) (Commit `a104c82e`)
* **Subsystem:** `src/halo/hs/hs.c`, `src/halo/hs/hs_compile.c`
* **What the functions do:**
  - Evaluator routines for the HaloScript runtime engine (compiled bytecode expressions).
  - Arithmetic evaluators: `hs_evaluate_addition`, `hs_evaluate_subtraction`, `hs_evaluate_multiplication`, `hs_evaluate_division`.
  - Logic & Flow Control: `hs_evaluate_and`, `hs_evaluate_or`, `hs_evaluate_not`, `hs_evaluate_cond`, `hs_evaluate_begin`, `hs_evaluate_begin_random`.
  - Game State Queries: `hs_evaluate_unit_health`, `hs_evaluate_player_count`, `hs_evaluate_game_time`, `hs_evaluate_objects_distance`.
  - Scripted Cinematic Commands: `hs_evaluate_camera_set`, `hs_evaluate_cinematic_start`, `hs_evaluate_fade_in`, `hs_evaluate_fade_out`.

### Batch 1.2A: Objects & Object Lights (104 Functions)
* **Status:** Complete — [PR #11](https://github.com/stianeklund/halo/pull/11) (Commit `4cd48f65`)
* **Subsystem:** `src/halo/objects/objects.c`, `src/halo/objects/object_lights.c`
* **What the functions do:**
  - Object memory allocation from the master `object_data` pool (`0x5a8d50`).
  - Spatial partitioning and cluster attachment (`object_attach_to_cluster`, `object_detach_from_cluster`).
  - Hierarchical bone matrix computation (`object_compute_node_matrices`, `object_get_node_matrix`).
  - Dynamic object lighting calculation: ambient, directional, spherical harmonics, and point light occlusion across scenario objects.

### Batch 1.2B: Units & Bipeds (80 Functions)
* **Status:** Complete — [PR #11](https://github.com/stianeklund/halo/pull/11) (Commit `30db1c8e`)
* **Subsystem:** `src/halo/units/units.c`, `src/halo/units/bipeds.c`
* **What the functions do:**
  - Biped ragdoll / "limp noodle" physical relaxation onto environment geometry (`biped_limp_noodle_relax_nodes_onto_environment`).
  - Surface impact and footstep sound/effect triggers (`biped_make_footstep`, `biped_find_ground_surface`).
  - Unit speech, dialogue selection, and priority vocalization (`unit_speak`, `unit_dialogue_determine_variant`, `unit_scream`).
  - Weapon inventory management (`unit_inventory_get_weapon`, `unit_detach_weapon`).

### Batch 1.2C: Core AI Subsystem (94 Functions)
* **Status:** Complete — [PR #11](https://github.com/stianeklund/halo/pull/11) (Commit `c2a67080`)
* **Subsystem:** `src/halo/ai/actors.c`, `src/halo/ai/actor_moving.c`, `src/halo/ai/ai.c`, `src/halo/ai/actions.c`, `src/halo/ai/actor_combat.c`
* **What the functions do:**
  - Decision loop pump: evaluates current combat state, alertness, and target threats (`actor_decision_loop`).
  - Stimulus response dispatchers: acoustic perception, visual sighting, weapon detonation, damage events (`actor_stimulus_combat`, `actor_stimulus_damage`, `actor_stimulus_weapon_impact`).
  - Breed action dispatchers: individual AI decision logic for Grunt (`grunt_decide_action`), Elite (`elite_decide_action`), Hunter (`hunter_decide_action`), Marine (`marine_decide_action`), Flood Carrier/Infection (`carrier_decide_action`, `infection_swarm_control`), and Sentinel (`sentinel_decide_action`).
  - Vector obstacle avoidance and steering calculations (`actor_move_vector_avoidance_find_direction`).

### Batch 1.2D: Weapons, Items, Devices & Projectiles (96 Functions)
* **Status:** Complete — [PR #11](https://github.com/stianeklund/halo/pull/11) (Commit `eff89bec`)
* **Subsystem:** `src/halo/items/weapons.c`, `src/halo/items/items.c`, `src/halo/items/projectiles.c`, `src/halo/devices/devices.c`, `src/halo/interface/first_person_weapons.c`
* **What the functions do:**
  - Ballistic aiming trajectories with gravity arc prediction (`projectile_aim_ballistic`, `projectile_aim_linear`).
  - Projectile line-of-fire collision detection, bounce physics, and detonation triggers (`projectile_collision_test_line`, `projectile_detonate`).
  - Weapon firing state machine: charging, burst firing, cooldown, reload cycles (`weapon_trigger_change_state`, `weapon_magazine_start_reload`, `weapon_busy`).
  - First-person viewmodel animation interpolation, flashlight alignment, and bone remapping (`first_person_weapon_build_node_matrices`, `first_person_weapon_start_interpolation`).
  - Device power, group values, and position controllers (`device_effect_new`, `device_group_new`, `device_touched`).
  - On-screen virtual keyboard text entry state machine (`virtual_keyboard_launch`, `virtual_keyboard_tab_left`, `virtual_keyboard_process_internal`).

### Batch 1.2E: Effects, Particles, Decals & Environment (78 Functions)
* **Status:** Complete — [PR #11](https://github.com/stianeklund/halo/pull/11) (Commit `f4e06518`)
* **Subsystem:** `src/halo/effects/effects.c`, `src/halo/effects/particles.c`, `src/halo/effects/particle_systems.c`, `src/halo/effects/weather_particle_systems.c`, `src/halo/effects/contrails.c`, `src/halo/effects/decals.c`, `src/halo/scenario/wind.c`, `src/halo/objects/widgets/glow.c`
* **What the functions do:**
  - Contrail point list creation, lifetime fading, and attachment to parent objects (`contrail_update_points`, `contrail_owner_collision`).
  - Decal projection mapping onto BSP geometry and object bounding boxes (`decal_projection_create`, `decal_clip_to_surface`).
  - Particle systems spawning, particle physics updates, lifetime decay, and rendering dispatch (`particle_system_update`, `particle_systems_render`, `particle_system_new_particle_explosion`).
  - Weather precipitation systems: camera-relative bounding box wrapping, clipping plane construction, and velocity physics (`weather_particle_system_wrap_point`, `weather_particle_system_build_clipping_planes`, `weather_particle_update_physics`).
  - Global wind variance calculations and object widget glow updates (`wind_variance_get`, `glow_update`).

---

## 5. Upcoming Batches Roadmap

### Batch 1.3: AI Looking, Perception, Encounters, Profiles & Communication (~337 Functions)
- Scope: `actor_looking.obj`, `encounters.obj`, `ai_debug.obj`, `ai_profile.obj`, `props.obj`, `ai_communication.obj`, `actor_firing_position.obj`, `actor_perception.obj`, `ai_script.obj`.
- Core Focus: Actor aim/look targeting, squad encounter management, dialogue communication channels, perception filters, and combat firing positions.

### Batch 1.4: Network, HUD, UI & Player Subsystems (~145 Functions)
- Scope: `network_*.obj`, `hud*.obj`, `player_*.obj`, `players.obj`, `ui_widget*.obj`, `interface.obj`.
- Core Focus: Multiplayer packet serialization, client/server message dispatch, HUD meters/reticles, player controller queues and rumble, menu UI hierarchy.

### Batch 1.5: Rasterizer & Hardware Rendering Pipeline (~371 Functions)
- Scope: `rasterizer*.obj`, `render*.obj`, `structures*.obj`, `bitmaps*.obj`.
- Core Focus: Xbox D3D pushbuffers, vertex/pixel shader initialization, dynamic lighting passes, environment fog, transparent geometry groups, and detail surface rendering.
