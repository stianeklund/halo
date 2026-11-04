# Aim assist on Xbox 2276: reference measurements

This document records how build 2276 applies magnetism and autoaim in split
screen. It also compares that behavior with H1 PB (CEA) with its 33 ms input delay.
Any mention of CEA or PB in this document explicitly means the H1 PB build based off CEA.

The purpose is to answer one question: does the PB input delay change how magnetism and autoaim work?

Short answer: no. PB's input delay uses the same model as the Xbox. The
magnetism search uses the live camera in both games. Autoaim uses the delayed
aim of the unit in both games. One numeric A/B test is still open (see
[Open items](#open-items)).

## Terms

| Term | Meaning |
|------|---------|
| tick | One game update. The Xbox runs 30 ticks per second, so 1 tick is 33.3 ms. |
| camera | The first-person view on the screen. The game calls it the observer camera (`observer_get_camera`). The reticle is at its center. |
| unit aim | The direction that the player's biped aims. The weapon fires along it. The game reads it with `director_camera_deterministic`. |
| action | The record of one tick of player input: facing, trigger, movement and buttons. |
| magnetism | The slowdown of the look turn when the camera is near a target (`local_player_aim_assist`, then the mix in `get_local_player_input_blob`). |
| autoaim | The bend of the projectile toward a target when the weapon fires (`player_aim_projectile`). |
| wu | World unit, the game's unit of distance. |

## How the tests work

All tests ran on xemu using the 2276 NTSC decomp with modifications to allow instrumentation.
Example: `--aim-assist-debug` instrumentation. See `docs/aim-assist-debug.md` for more details on the tooling.

1. A script drives the controller. The hook in `input_get_device_states`
   writes the script values into the XInput state. The game then processes
   them like real controller input.
2. When the script starts, the pose step moves both players. The shooter
   stands 4 wu from the target and faces it. The target is the other player.
   It stands still and does not turn.
3. The trace records each tick: the camera direction, the unit aim, the stick,
   the magnetism value, the look turn before and after magnetism, and each
   shot before and after autoaim.

The pose is necessary. Magnetism only operates when an enemy is in the cone,
so the test puts an enemy in front of the shooter on purpose. Only the
shooter turns, so the target's own aim assist does not affect the results.

Three scripts ran. Each one turns the shooter to the right for 300 ticks:

| Script | Stick | Trigger |
|--------|-------|---------|
| sweep | full right | none |
| taps | 0.8 right | one tap every 7 ticks |
| taps_full | full right | one tap every 7 ticks |

## The game mode is important

The game has three connection modes (`game_connection()`, global `0x46da0c`):

| Value | Mode | How it starts |
|-------|------|---------------|
| 0 | local | `init.txt` with `map_name` boots directly into a game |
| 1 | network client | join a game |
| 2 | network server | split screen or a hosted game from the menu |

Split screen from the menu runs in mode 2. In mode 2, each tick's action goes
through the network game server, also when all players are on one console.
The server uses the action one tick later. Mode 0 does not use this path.

The first tests ran in mode 0, and they showed no delay. The split-screen
behavior that players know needs mode 2. **Start the game from the menu for
any test that must match split screen.** Delete `init.txt` first, because
`deploy_xbox.py` uploads a new one on each deploy.

## Results: split screen, mode 2, default sensitivity

All values are per tick at 30 Hz.

### Look turn

- Full stick starts at 4.0°/tick. Each tick adds 0.267°/tick, to a maximum of
  12°/tick (360°/s).
- The profile sensitivity changes these values. With a raised sensitivity, the
  start was 5.33°/tick and the maximum was 16°/tick. Use the default
  sensitivity for comparisons.

### The camera is one tick ahead of the unit aim

- On all 900 turning ticks in the three runs, the unit aim was exactly one
  tick of turn behind the camera.
- Cause: the camera reads the live `desired_angles` from `player_control`. The
  unit gets the facing from the action, and the action arrives one tick late.

### Magnetism uses the live camera

- `local_player_aim_assist` searches the cone around the camera position and
  the camera forward vector (`observer_get_camera()`, forward at `+0x20`).
- The slowdown applies to the look turn of the same tick. There is no
  additional delay.
- The result: magnetism engages when the reticle crosses the target. The
  unit aim is not part of the search.

Magnetism value against the angle between the camera and the target (4 wu,
target does not move):

| Camera off target | Magnetism |
|-------------------|-----------|
| 0° to about 3.5° | 1.0 |
| 4.3° to 4.7° | 0.80 to 0.87 |
| 5.3° to 6.0° | about 0.5 |
| about 7° | less than 0.1 |
| more than about 7.2° | 0 |

Turn after magnetism, when the target does not move:

    final_turn = look_turn * (1 - 0.6 * magnetism)

At magnetism 1.0, a 12°/tick turn becomes 4.8°/tick. The full formula also
has a target-velocity term:
`+ clamp(target_rate) * 0.7 * magnetism`, with clamps of 6° yaw and 3° pitch
per tick. That term is 0 here, because the target does not move. The values
0.6 and 0.7 come from this weapon's aim-assist constants.

### Shots use the aim of the press tick

- The player presses the trigger in tick T. The shot fires in tick T+1.
- The shot goes along the camera direction that the action of tick T carried.
  That is also the unit aim in tick T+1.
- All 32 shots matched this to 0.01°.
- So the shot goes where the reticle was in the press tick, and it leaves one
  tick later. This is a latency, not an offset.

### Autoaim uses the unit aim

- `player_aim_projectile` searches around `director_camera_deterministic`,
  which is the delayed unit aim.
- Shots near the target bent up to 1.5° toward it.

### Magnetism with a moving target

Test: the target strafes at full stick to the left and right. It changes
direction every 8 ticks. The shooter gives no look input. The shooter moves
the left stick forward and back at 0.3, so the magnetism gate passes. One
trigger tap occurs every 25 ticks.

- The camera stayed 1.4° to 3.9° from the target. Magnetism stayed at 1.0.
- The camera turned at 0.699 × the angular rate of the target (standard
  deviation 0.007). This is the `rate * 0.7 * magnetism` term of the formula.
- The rate term uses the motion of the target in the same tick. It has no
  delay.
- On all 280 ticks, the measured turn agrees with the formula to 0.0001°.
- Autoaim bent the 9 shots by 0.1° to 1.6° toward the target. The target died
  one tick after the 9th shot, so the shots hit.
- 2 of the 11 taps (ticks 131 and 156) fired no shot. A reload is the
  probable cause. The 2 taps are adjacent, and the gap is about 50 to 75
  ticks. This agrees with a pistol reload. The Xbox player fired in earlier
  tests without a respawn, so the magazine was probably almost empty. The
  ammo count was not recorded.

### Unit aim speed limit (high sensitivity only)

- With the raised sensitivity (16°/tick), the unit aim turned at a maximum of
  12.86°/tick. The camera turned at 16°/tick, so the unit aim fell behind by
  up to 90°. The shots followed the unit aim, not the reticle.
- At default sensitivity, the maximum look turn is 12°/tick. This is less than
  12.86°/tick, so the limit did not occur.
- The source is not the unit tag. In the Xbox memory, the tag values
  `aiming_velocity_maximum` and `aiming_acceleration_maximum` are 0 for
  `cyborg_mp` and `cyborg`. With 0 and 0, `unit_update` sets the unit aim
  to the desired aim at once (`src/halo/units/units.c:13476`). The source of
  the 12.86°/tick (386°/s) limit is not known. The value is not a constant
  in the executable.

### Other observations

- Split screen is not deterministic from run to run. Two identical sweeps gave
  different turns after the first contact with magnetism. Analyze each shot
  against the camera and the unit aim of its own run.
- In mode 0 (local), the camera and the unit aim are the same. The magnetism
  values and the turn formula are the same as in mode 2.

## Comparison with PB (CEA) at 33 ms

The CEA setting is `input_pipeline_ms_30tick = 33`
(`src/host/blampc_input_pipeline.c`). It delivers the whole action (facing,
trigger, movement and buttons) one Xbox frame late. The camera continues to
read the live `desired_angles`.

| Behavior | Xbox 2276 split screen | CEA at 33 ms | Same? |
|----------|------------------------|--------------|-------|
| Camera vs unit aim | Camera one tick ahead | Camera live, action 1 frame late | Yes |
| Magnetism search axis | Live camera | Live camera (`local_player_aim_assist.c:57`, `observer_get_camera`) | Yes |
| Pipeline effect on the search | None | None. CEA applies the ring after the repack (`handle_one_player_input.c`). The ring does not change `desired_angles` | Yes |
| Magnetism turn formula | `look * (1 - k0*m) + rate * k4*m` | Same (`get_local_player_input_blob.c:514-569`) | Yes |
| Rate clamps | 6° and 3° per tick | Scaled by the tick rate to the same °/s | Yes (by design) |
| Shot timing and direction | T+1, aim of the press tick | One frame late, aim of the press tick | Yes |
| Autoaim search axis | Delayed unit aim | Delayed unit aim (`player_aim_projectile.c:63`, `director_camera_deterministic`) | Yes |
| Unit aim speed limit | 12.86°/tick seen at high sensitivity. The tag value is 0 | The tag value is 0 | Tag values agree. The 12.86°/tick limit is not tested in PB |

At 120 Hz, CEA runs the magnetism search four times per Xbox frame. Each
search applies the slowdown to a quarter of the turn. For a constant
magnetism value, the total slowdown per frame is the same. Near the edge of
the cone, the magnetism value changes inside one Xbox frame, so CEA
follows the curve more closely than the Xbox. This makes a small difference
near the cone edge. It is not an effect of the input delay.

### Tick rate and ini keys

The PB ini stores one delay value for each tick rate. The 33 ms value applies
only to the rate in its key name:

    tick_rate                 = 120
    input_pipeline_ms_120tick = 0
    input_pipeline_ms_30tick  = 33
    shot_delay_aim_ms_120tick = 17

With these values, a 120 Hz run has no input delay. It uses AIM VECTOR DELAY
(`shot_delay_aim_ms`, `blampc_shotdelay.c`) instead. That mechanism is a
different model:

| Behavior | Xbox split screen | PB 120 Hz, input delay 33 ms | PB 120 Hz, input delay 0, aim delay 17 ms |
|----------|-------------------|-------------------------------|---------------------------------------------|
| Unit aim vs camera | One frame behind | One frame (4 ticks) behind | Same as the camera |
| Shot leaves | One frame after the press | One frame after the press | In the press tick |
| Shot direction | Camera at the press | Camera at the press | Camera 17 ms (2 ticks) before the shot |
| Magnetism search axis | Live camera | Live camera | Live camera |
| Autoaim search axis | Delayed unit aim | Delayed unit aim | Live aim, then the shot turns back 17 ms |

AIM VECTOR DELAY does not change magnetism. It changes the shot: the shot
leaves at once along an older aim, so it lands behind the reticle during a
turn. This is an offset, not a latency. The header of `blampc_shotdelay.c`
calls it a deliberate deviation from the Xbox.

For Xbox behavior at 120 Hz, set `input_pipeline_ms_120tick = 33` and
`shot_delay_aim_ms_120tick = 0`. Keep FIRING INPUT POLLING at 30 Hz. That is
the default value for a player.

**Conclusion.** The 33 ms input delay does not cause magnetism problems at 30 Hz or at 120 Hz.

CEA puts the delay at the same point as the Xbox: the action is late, the
camera is live, magnetism uses the camera, and autoaim uses the delayed unit
aim.

## PB A/B test results

The PB build ran the same scripts as the Xbox: same map (carousel), same
positions, same weapon (pistol), 2 players. A diagnostic trace
(`BLAMPC_AIM_TRACE`, not committed) printed the same values as the Xbox trace.
Each script ran 3 times:

- 30 Hz, input delay 1 tick (33 ms)
- 120 Hz, input delay 4 ticks (33 ms), script ticks × 4
- 30 Hz, input delay 0 (for reference)

FIRING INPUT POLLING was 30 Hz. AIM VECTOR DELAY was 0.

| Measurement | Xbox split screen | PB 30 Hz, 33 ms | PB 120 Hz, 33 ms |
|-------------|-------------------|-----------------|------------------|
| Look turn ramp (with magnetism 1.0) | 1.6, 1.71, 1.81°/tick | 1.6, 1.71, 1.81 | 1.64, 1.75, 1.85 per frame |
| Maximum look turn | 12°/tick | 12 | 12 per frame |
| Magnetism at 5° / 6° / 7.5° off target | 0.75 / 0.43 / 0 | 0.74 / 0.40 / 0 | 0.73 / 0.41 / 0 |
| Turn formula fit | exact | exact | exact |
| Turn / target rate, stick idle | 0.700 | 0.700 | 0.700 |
| Unit aim behind camera | 1 tick | 1 tick | 4 ticks |
| Shot fires | 1 frame after the press | 1 frame after | 1 frame after |
| Shot aims along | camera after the press frame | camera after the press frame | camera after the press tick (1/4 frame) |
| Autoaim bend, strafe shots 1 to 4 | 1.52, 0.75, 1.40, 1.61° | 1.54, 0.77, 1.41, 1.61° | 1.57, 0.80, 1.45, 1.46° |

Results:

- **PB at 30 Hz with 33 ms matches the Xbox.** All values agree within the
  run-to-run noise of the Xbox.
- **PB at 120 Hz with 33 ms also matches.** The small differences come from the
  finer time step. The magnetism search runs 4 times per frame, and the shot
  uses the camera after the press tick, not after the whole frame.
- **The weapon is important for a comparison.** PB scripted runs spawn with the
  plasma pistol (`--gametype stock`) or the assault rifle. These weapons have
  larger cones than the pistol. Assault rifle: autoaim 6°, magnetism 12°.
  Pistol: autoaim 3°, magnetism 6°. The Xbox tags in memory have the same
  values as the PB tags (see the table below). The Xbox results match only
  the pistol. The plasma pistol also fires one tick later than the pistol and the
  assault rifle.
- **One difference is probably the magazine.** In the strafe test, 2 of the
  11 Xbox taps (ticks 131 and 156) fired no shot. PB fired all 11. Each PB
  run starts a new game with a full magazine. The Xbox player probably
  reloaded (see the strafe results above).

Weapon tag values read from the Xbox memory (2276, carousel). They are the
same as the PB tag values for the pistol, assault rifle and plasma pistol.

| Weapon | Autoaim | Magnetism | Range |
|--------|---------|-----------|-------|
| pistol | 3° | 6° | 30 wu |
| assault rifle | 6° | 12° | 25 wu |
| plasma pistol | 5° | 12° | 20 wu |
| plasma rifle | 5° | 12° | 25 wu |
| needler | 6° | 12° | 25 wu |
| shotgun | 6° | 12° | 15 wu |
| sniper rifle | 1° | 2° | 35 wu |
| rocket launcher | 0° | 12° | 35 wu |

## Open items

1. The weapon tag values are confirmed (see the table above). The weapon in
   hand was not read. The pistol is known only from the bend results.
2. The unit tag aim limits are 0 on the Xbox and in PB, so they agree. The
   source of the 12.86°/tick limit at high sensitivity is not known. PB was
   not tested at that sensitivity.
3. The 2 dropped Xbox taps are probably a reload. A rerun in a new game
   (`ssd_strafe2.csv`) fired all 11 taps. This agrees with a reload. But that
   run had different magnetism (0.32 at the target center, not 1.00) and no
   bends. The weapon in hand was probably different. Thus the rerun does not
   prove the cause.
4. Hit counts are not measured. The trace records directions and bends, not
   damage.

## Data

The raw traces and scripts are in `C:\Users\stian\Desktop\xemu-aim-tests\`:

- `splitscreen\ssd_*.csv`: mode 2, default sensitivity (the results above).
- `splitscreen\ssd_strafe.csv`, `strafe.input.txt`, `strafe_an.py`: the moving-target test.
- `splitscreen\ss_*.csv`: mode 2, raised sensitivity.
- `splitscreen\default_sensitivity_analysis.txt` and `ss_an.py`: the
  analysis.
- `pb_ab\`: the PB logs (`pi_*` = pistol, `ar_*` = assault rifle), the
  analysis scripts (`pb_ab.py`, `press_k.py`) and the probe patch
  (`pb_aim_trace_probe.diff`).
- The top folder: the earlier mode 0 (local) tests.

In the CSV files, `search_*` is the camera direction and `facing_*` is the
unit aim.
