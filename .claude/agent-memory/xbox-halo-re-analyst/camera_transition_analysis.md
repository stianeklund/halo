---
name: Camera transition analysis
description: Deep analysis of vehicle exit camera transition mechanism - state_253 cascade, camera classifier, director timing
type: project
---

## Vehicle Exit Camera Transition Analysis (2026-04-19)

**Why:** Investigating camera snap on vehicle exit — state_253 cascades from 0x1b to 0x00 within one tick.

### Key Findings

1. The state_253 cascade (0x1b → 0xFF → 0x14 → 0x00) is **correct original behavior**. All functions involved are unported original XBE code. The flag at 0x32d1c8 that controls whether the exit function runs is statically 1 in the .data section and never written by any code.

2. **Game tick order** (from game.c): `objects_update()` → `players_update_after_game()`. Then in main loop: `game_time_update()` (calls `game_tick()` N times) → `director_update()` → `observer_update()`. Camera classifier runs inside `director_update`, AFTER all state machine transitions complete.

3. The **camera classifier** (0x864b0) checks `unit[0x253]` for states 0x1a (entering vehicle) or 0x1b (exiting vehicle), but ONLY while `unit[0xCC]` (vehicle datum) is not -1. After the exit cascade, vehicle datum is cleared by function 0x1411c0 at address 0x14129e.

4. The **transition camera** (0x89cd0) is installed by `camera_internal_reevaluate` (0x86be0) on vehicle ENTRY, not exit. On exit, the camera goes directly from first-person to first-person (the vehicle wrapper is removed).

5. **Smooth exit transitions** in the original game come from the observer_update blend timers (unported, passed via 0x8acb0), NOT from the transition camera function.

### Key Addresses
- 0x864b0: camera_classifier (unit_datum, int16_t *out_mode) → returns transition_active
- 0x86be0: camera_internal_reevaluate (@eax=player, @bl=force_flag) — installs/removes transition camera
- 0x86de0: director_set_player_camera_normal (@eax=player, cdecl: reset, mode_flags) — PORTED
- 0x875f0: director_update (original, float delta_time) — PORTED at 0x875f0
- 0x89270: first-person gameplay camera function
- 0x89cd0: transition camera function
- 0x8acb0: observer commit function (unported)
- 0x1acd70: unit_seat_animation_select — writes 0xFF to unit[0x253]
- 0x1ad260: unit_set_state — final write to unit[0x253]
- 0x1b0d90: unit_state_machine_update
- 0x1b2dd0: unit_exit_vehicle_normal — orchestrates the exit cascade
- 0x1411c0: object detach from vehicle — clears unit[0xCC] to -1
- 0x32d1c8: static flag (always 1, never written), controls whether exit function runs
- 0x865a0: camera_internal_set_camera_fn (@si=player, cdecl: fn, reset_byte)

**How to apply:** If debugging camera transition bugs, focus on the observer blend timers and per-player timer (ps+0), not on state_253 timing. The state cascade within one tick is by design.
