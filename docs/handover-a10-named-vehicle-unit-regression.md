# Handover

## Objective
Find and faithfully fix the patched direct-a10 `unit doesn't exist` regression for named vehicles, using binary and runtime evidence before any implementation.

## Current State
- Root cause is not yet isolated; do not implement or use Luna until the first divergent ported function is proven.
- Temporary probes in `objects.c`, `players.c`, and `hs_runtime.c` were removed.
- A normal all-ported `default.xbe` was rebuilt, deployed, and launch-verified last.
- `kb.json` diagnostic toggles are restored to `ported: true`.
- Unrelated worktree changes exist in `README.md` and `artifacts/batch_verify/`; preserve them.
- `src/halo/hs/hs_runtime.c` has only an EOF-newline diff left from removed probes.

## Confirmed
- Clean direct-a10 baseline:
  - patched `default.xbe` logs repeated `unit doesn't exist` around 12 seconds;
  - pristine `cachebeta.xbe` has no fresh matching logs after explicit `debug.txt` deletion.
- The string is used only by `recorded_animation_play_internal` at `0x95330`.
  It emits when its implicit EAX unit handle is `-1`.
- The active caller is HS function index 66 (`recording_play`), dispatched by
  `player_rumble_set_effect` at `0xBE770`. Binary call-site inspection confirms
  it passes `record[0]` as the unit and `record[1]` low word as animation.
- Runtime probes showed `mission_a10` calls `recording_play` with `record[0] == -1`
  and animation 30. The owner script is `mission_a10`.
- Failed scenario-name values have low 16-bit indices 238--241 and 248--249
  (for example, `-65298` maps to 238).
- `hs_can_cast` at `0xCB170` matches its binary behavior and resolves these
  scenario-name values via `object_name_list_get_handle`.
- The live patched name table had slot 238 equal to `0xffffffff`; the object
  table datum magic was valid (`0x64407440`).
- Forced index-238 placement reaches `object_new_from_scenario` but is skipped
  while `object_globals->object_is_being_placed` is set and placement flag bit 0
  is set. Pristine reaches the same early state, so that guard is expected.
- Targeted runtime trace showed no `object_new_by_name` / `object_create` call
  for indices 238--241 or 248--249 during the first two patched minutes.
  Recorded-animation errors occur before observed creation warnings.
- Binary/source comparisons found no discrepancy in:
  - `object_new_from_scenario` (`0x144770`), `objects_place` (`0x13F060`),
    `FUN_0013CDD0`, or `FUN_0013CB80`;
  - `hs_macro_function_evaluate` (`0xCC560`), `hs_can_cast` (`0xCB170`), or
    `hs_evaluate_sleep` (`0xCD0E0`);
  - `hs_runtime_initialize_for_new_map` (`0xCDB30`) control flow.
- `actor_action_can_stop_conversing` is correctly declared `char`; generated
  declarations were regenerated before this investigation. Do not re-attribute
  this issue to its former stale declaration without new evidence.

## Important Changes
- No intentional source or `kb.json` changes remain.
- Last normal deployment command:
  `rtk ./tools/xbox/build_deploy_run.sh -q`
  It launch-verified `DECOMP BUILD 39bd3eeb`.

## Validation
- Passed: `rtk python3 tools/audit/check_ghidra_mcp.py`.
- Runtime probes used `rtk ./tools/xbox/build_deploy_run.sh -q`,
  `rtk python3 tools/xbox/xbdm_debug_txt.py -100`, and virtual xemu memory reads.
- Pristine baseline was explicitly reset with:
  `rtk python3 tools/xbox/xbdm_rdcp.py "delete name=E:\\GAMES\\halo-patched\\debug.txt"`
  followed by `magicboot` of `cachebeta.xbe` and a 120-second wait.
- No VC71, equivalence, or lift-pipeline validation applies: no fix was made.

## Uncertain / Risks
- The defect is an early `mission_a10` control-flow or scheduling divergence,
  not yet a proven source discrepancy.
- `hs_runtime_initialize_for_new_map` (`0xCDB30`) and
  `hs_load_scenario_scripts` (`0xC4C10`) remain candidates only because their
  runtime output has not been compared event-for-event; do not change them on
  that basis alone.
- Do not suppress the recorded-animation error or force-create the named units:
  either masks the timing divergence and is not faithful.
- Core replay is not a valid initialization-toggle oracle. `mapreset` logs the
  symptom but did not exercise the prior index-238 probe.

## Next Steps
1. Capture equivalent pristine and patched event timelines at `0x95640`
   (recorded-animation wrapper), `0xC9990` (`object_create`), `0x144940`
   (`object_new_by_name`), and `0x144770` (`object_new_from_scenario`). Record
   game time (`*(uint32_t *)(*(uint32_t *)0x45708C + 0xC)`) and name slots
   238--241/248--249.
2. Interpret the first divergence:
   - original creates before playback but patched never reaches `0xC9990`:
     inspect `0xCDB30` / `0xC4C10` initialization output;
   - patched reaches creation but no name-table write: investigate the actual
     scripted `0x144770` result/guard;
   - patched writes then clears: inspect `object_remove_from_name_list`;
   - both write before playback but patched passes `-1`: revisit evaluator
     result-record construction.
3. Only after the first divergent ported function is proven, delegate a
   constrained fix to Luna and independently review it against the binary.

## Resume Prompt
Continue the a10 named-vehicle `unit doesn't exist` investigation from
`docs/handover-a10-named-vehicle-unit-regression.md`. Do not implement or use
Luna yet. Establish the pristine-versus-patched runtime timeline at `0x95640`,
`0xC9990`, `0x144940`, and `0x144770`, then identify the first divergent
ported function before proposing a fix.
