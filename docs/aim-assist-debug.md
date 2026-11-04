# Aim-assist diagnostics

Diagnostic instrumentation, not recovered game code. It is the Xbox
counterpart of halo-cea-jukkis's DISPLAY AUTOAIM ANGLE / DISPLAY MAGNETISM ANGLE
(`src/host/blampc_aimcones.c`). It also prints every term of the magnetism mix.
The purpose is to measure how build 2276 applies magnetism, so that the PC
port's input-delay emulation can be compared against it.

The instrumentation is compiled only when `HALO_AIM_ASSIST_DEBUG` is defined:

    python3 tools/build/build.py --aim-assist-debug
    ./tools/xbox/build_deploy_run.sh -q --aim-assist-debug

When the flag is off, `player_control.c`, `weapons.c`, `render_debug.c` and
`input_xbox.c` compile to objects that are identical to HEAD in every section. Each inserted
block is followed by a `#line` directive that restores the HEAD line numbering.
In that build, `aim_assist_debug.c` compiles to an empty object.

## What is drawn

The rings are drawn in the render-debug flush (`FUN_0018ac50`), once per player
window, as immediate debug lines around `render.camera`:

| ring | color | half-angle |
|------|-------|-----------|
| magnetism | cyan | `unit_get_aim_assist_parameters(..., player_control_get_zoom_level)[2]` |
| autoaim | orange | `unit_get_aim_assist_parameters(..., unit_get_zoom_level)[0]` |

Each consumer uses its own zoom source, the same one the game passes:
`local_player_aim_assist` uses the player-control zoom, and
`player_aim_projectile` uses the unit zoom. The angles therefore already include
the `1/magnification` scaling. A weapon that has no assist at the current zoom
draws no ring. The rings are the true cone edges projected through the camera.
Each vertex is `camera + 0.5 * dir` on the cone. A ring is therefore as wide on
screen as the cone is in the world, at any FOV.

Like jukkis, each ring is centered on the axis its own search uses. Each ring
is projected from the render camera:

- **magnetism**: centered on the observer-camera forward
  (`observer_get_camera()+0x20`) that `local_player_aim_assist` handed to the
  cone search on the last `get_local_player_input_blob` tick. If magnetism tests
  a view older than the one on screen, this ring trails the reticle while you
  turn.
- **autoaim**: centered on `director_camera_deterministic`'s forward. In first
  person that forward is the unit's facing, `unit+0x1ec`. `player_aim_projectile`
  searches around that axis when the shot fires. The call is read-only.

The text block prints both offsets from the view in degrees. A per-shot
`facing-vs-view` value is also printed. It is the angle between the
deterministic forward that `player_aim_projectile` searched around and the
observer forward at the moment of the shot.

The targets themselves can be shown with the stock globals:
`debug_objects true` together with `debug_objects_biped_autoaim_pills true`.
These map to `0x5054fd`, as resolved from the external-globals table at
`0x2f3708`. In the same table, `player_magnetism` is `0x2f0291` and
`player_autoaim` is `0x2f0290`.

## Telemetry block

The telemetry is drawn as top-left debug text. It shows the last
`get_local_player_input_blob` tick for the rendered window's local player:

- `target`, `autoaim_level`, `magnetism` (`control->field_0x30`). These are the
  outputs of `local_player_aim_assist`.
- `aim_angles` and `aim_velocity`. `aim_velocity` is `local_player_aim_assist`'s
  `rates[0..1]`, the angular-rate term computed from the target's offset from
  the unit. It is shown in degrees. Its time unit is not proven.
- The stick vector and the look delta before magnetism.
- `MIX`. This line appears only when the original's gate passes:
  `player_magnetism && magnetism > 0 && (stick or move input > 0.0001)`.
  It shows:
  - the game speed (halved under `game_players_are_double_speed`)
  - `input_scale = 1 - magnetism * clamp(constants[0])`
  - `magnetism_scale = magnetism * clamp(constants[4])`
  - the velocity after the game-speed scale, clamped to ±6° yaw and ±3° pitch
  - `mixed = velocity * magnetism_scale + look * input_scale`
- `final look`, which is `mixed * delta_time * 30`. That is the value written to
  `input->look_*_delta`.

## Console lines

`console_printf` prints one line in each of these cases:

- The target changes.
- Magnetism engages (goes from 0 to above 0) or releases (goes from above 0
  to 0).
- An autoaim shot is fired. This line includes the shot's target, its
  `aim_level`, and the facing-vs-view angle.

## Runtime toggles and the trace

`halo_aim_assist_debug` is a set of 4 toggle bytes: `rings`, `text`,
`console` and `trace`. The defaults are 1, 1, 0, 1. `text` shows a single
summary line. `trace` records one 128-byte record per local player per input
tick, plus one record per player shot, into `halo_aim_assist_trace`. That
buffer holds 4096 records, which is about 2¼ minutes for one player.

    HALO_WINDOWS_REEXEC=1 python3 tools/xbox/aim_assist_debug.py --host 10.0.0.21            # show
    HALO_WINDOWS_REEXEC=1 python3 tools/xbox/aim_assist_debug.py --host 10.0.0.21 --dump t.csv
    python3 tools/xbox/aim_assist_debug.py --csv t.csv --tail 50                            # readable

## Scripted pad input

`halo_aim_test_script` replays a halo-cea-jukkis `--input` script, using the
same `tick port lx ly rx ry buttons(hex)` lines, on the Xbox pads. It is used
for repeatable aim tests such as sweep, flick, strafe and orbit. The script
starts on the first controller poll after it is armed. Each scripted port then
takes its last keyframe with `tick <= game_time - start_tick`. The hook in
`input_get_device_states` writes the keyframe into the XInput state before the
game processes it, so deadzone, button hold counters, look acceleration and
magnetism all run as they would with a real pad. Ports not in the script keep
their real controller. After `end_tick` the script is done and the real pads
come back.

Buttons use the engine's gamepad button order. Bit N is button N:

| bits | buttons |
|------|---------|
| 0–7 (analog) | A, B, X, Y, black, white, left trigger, right trigger (`0080`) |
| 8–15 (digital) | d-pad up, down, left, right, start, back, left thumb, right thumb |

    HALO_WINDOWS_REEXEC=1 python3 tools/xbox/aim_assist_debug.py --host 10.0.0.21 --port 731 \
        --script flick.input.txt --tick-scale 4    # a 120 Hz PC script
    HALO_WINDOWS_REEXEC=1 python3 tools/xbox/aim_assist_debug.py --host 10.0.0.21 --port 731 --script-status

`--tick-scale 4` converts the PC's 120 Hz ticks to the Xbox's 30 Hz ticks by
dividing each tick by 4 and rounding. A keyframe that rounds onto the same
tick as the previous keyframe for its port moves 1 tick later, so a press of
less than 4 PC ticks is not lost. Trace records written while a script runs
have the `scripted` column set. Every record also holds the unit's world
position (`pos_x`, `pos_y`, `pos_z`).

`--pose LP,X,Y,Z,YAW[,PITCH]` (degrees, repeatable) places local player LP's
unit when the script starts, using `object_set_position` and
`player_control_set_facing`. `--meet DIST` places local player 0 DIST world
units in front of local player 1's pose, facing it. This is jukkis's
TEST_STAND/TEST_MEET. Use points where a unit has already stood (the trace's
`pos_*` columns), because a pose over a hole falls out of the map. On
carousel, this pair is 4 wu apart on floor:

    --pose 1,9.983,-1.199,-3.258,131 --pose 0,7.359,1.822,-2.86,-49

The trace version is now 2. A tool that expects version 1 refuses the dump.

Two players: install `tools/xbox/presets/carousel-2p/init.txt` as
`E:\GAMES\halo-patched\init.txt` and restart the title. This boots
split-screen Slayer on carousel. `deploy_xbox.py` uploads the repo-root
`init.txt` on every deploy, so install the preset again after each deploy.

## Hook sites

| site | hook |
|------|------|
| `get_local_player_input_blob`, after `local_player_aim_assist` | `aim_assist_debug_note_search` |
| `get_local_player_input_blob`, after the magnetism mix | `aim_assist_debug_note_mix` |
| `trigger_create_projectiles`, around its `player_aim_projectile` call | `aim_assist_debug_note_shot_begin` / `aim_assist_debug_note_shot` |
| `FUN_0018ac50`, after `players_debug_render` | `aim_assist_debug_render` |
| `input_get_device_states`, after `XInputGetState` | `aim_assist_debug_script_pad` |

The render hook only reads game state. It does not call
`director_get_perspective`, because that function writes director state.

`player_aim_projectile`, `local_player_aim_assist`, `FUN_000a5f00`,
`FUN_000a5ac0` and `object_compute_autoaim_target` are still
`ported: false`, so the original code runs for them. A hook placed inside them
never runs. For that reason the shot hook sits in the ported caller. That
caller never sees `aim_level` or the cone axis. A SHOT record therefore holds:

- the projectile forward before and after the call (the bend)
- the target
- the `director_camera_deterministic` forward, recomputed read-only
- the observer forward
