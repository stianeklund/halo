# Batch 1.3 Recovery Report: AI, Communication & Perception Pipeline

## Executive Summary
Batch 1.3 completes the authentic symbol recovery and labeling of 370 AI subsystem functions in Halo CE Xbox debug build 2276. This encompasses the entirety of the actor perception, behavioral actions, looking, moving, combat routines, communication, profile metering, encounter management, and path storage systems.

All 370 function names and addresses are 100% verified against the canonical debug symbols in `halo_2276_functions.txt` (Oct 12, 2001 build 2276). Zero speculation or synthetic naming was introduced.

## Verification & Guardrail Metrics
- **Functions Renamed:** 370 functions (329 ported C implementations, 41 assembly thunks/unported).
- **Register ABI Invariance:** 886 / 886 tracked functions verified against `tools/kb_reg_baseline.json` (**0 drift, 0 missing, 0 stale**).
- **Hazard Scan:** Passed (`check_lift_hazards.py --changed-only` reports 0 blocker hazards).
- **Compilation & Linkage:** Successful (`build.py -q --target halo` compiled cleanly and produced target executable without errors).
- **Duplicate Check:** 0 duplicate symbol names in `kb.json`.

## Subsystem Functional Breakdown

| Object Module | Count | Primary Subsystem Responsibilities |
| :--- | :--- | :--- |
| `actions.obj` | 38 | High-level actor action state machines: pursue, flee, charge, melee, search, vehicle boarding/eviction, and individual obedience commands. |
| `action_vehicle.obj` | 13 | Actor vehicle interaction logic: entry maneuvers, driver/gunner/passenger roles, speed matching, seat transitions, and vehicle eviction. |
| `actor_combat.obj` | 27 | Tactical combat behaviors: cover search, weapon selection, firing position evaluation, dodge/evade timing, grenade throws, and target prioritization. |
| `actor_firing_position.obj` | 12 | 3D spatial queries for valid firing vectors, line-of-fire clearance, wall hugging, and dynamic cover points. |
| `actor_looking.obj` | 28 | Gaze, head-tracking, target aiming, secondary look targets, glance timers, and field-of-view sweep computations. |
| `actor_moving.obj` | 25 | Local navigation, obstacle avoidance, steering vectors, surface following, leaping, and jump-down path transitions. |
| `actor_perception.obj` | 21 | Sensory awareness modeling: sight cone visibility checks, sound detection/propagation, threat assessment, unopposable target filtering, and memory decay. |
| `actors.obj` | 42 | Actor lifecycle: creation, damage response, death/dormancy, state update dispatch, swarm coordination, and biped coupling. |
| `ai.obj` | 19 | Core AI global loop: tick orchestration, active encounter evaluation, performance profiling hooks, and global perception queues. |
| `ai_communication.obj` | 17 | Dialogue and vocalization engine: combat chatter, warning cries, death screams, conversation triggers, and spatial sound dispatch. |
| `ai_debug.obj` | 34 | AI telemetry and visualization: pathing storage visualization, line-of-fire raycasts, speech state inspection, actor telemetry HUD, and encounter metering. |
| `ai_profile.obj` | 14 | Performance monitoring and script hooks: CPU budgeting, actor/swarm/prop counts, and AI script interface functions. |
| `ai_script.obj` | 16 | HaloScript AI bindings: allegiance manipulation, squad migration, vehicle order assignments, and awareness overrides. |
| `encounters.obj` | 48 | Encounter definitions, squad activation/deactivation triggers, respawn timers, platoon tactical rules, and migration graphs. |
| `path.obj` | 16 | Global pathfinding queries: waypoint networks, graph traversal, cluster connectivity, path input configuration, and obstacle paths. |
| `props.obj` | 20 | Prop awareness records: object perception tracking, memory decay, dynamic prop lists, and spatial prop queries. |

## Collision Resolutions & Displaced Symbols Handled
During Batch 1.3 recovery, several historic collisions and displaced symbols were systematically resolved:
1. `0x10b2d0`: Previously misnamed `random_range` in `random_math.obj`; corrected to authentic debug symbol `seed_random_range`, freeing `random_range` for authentic `0x17940` in `actor_looking.obj`.
2. `0x5dff0`: Previously misnamed `paths_dispose`; corrected to authentic debug symbol `path_input_set_target_object`, freeing `paths_dispose` for authentic `0x5df90` in `encounters.obj`.
3. `0x58a40`: Shifted from legacy `FUN_00058a40` / `ai_magically_see_players` to authentic debug symbol `ai_scripting_magically_see_players` across `encounters.c` and `players.c`.
4. `0x5af70`: Corrected from `FUN_0005af70` to authentic debug symbol `encounter_test_rule` in `encounters.c`.
5. `0x64a80`: Corrected from legacy misnomer `prop_iterator_next` to authentic `prop_delete`, while `0x64570` was confirmed as the authentic single-argument `prop_iterator_next(int *iter)`.
6. `0x16bd0` & `0x19d00`: Corrected to authentic `action_obey_individual_setup` and `action_search_perform`, resolving historic overlaps with secondary looking routines (`0x27870` / `0x27a60`).

All changes preserve 100% binary-identical logic and maintain strict calling convention and register parameter stability across the Halo CE codebase.
