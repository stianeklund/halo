# Halo CE Re-Implementation Status

This document is a snapshot of the current checkout, based on the repo's analysis tools rather than older hand-written estimates.

Primary sources used for this update:
- `python3 tools/analysis/progress.py --json`
- `python3 tools/analysis/frontier.py`
- `python3 tools/analysis/maintain.py`

## Executive Summary

The repository is well past the "tiny bootstrap" stage, but it is still an incremental reimplementation layered onto the original Xbox binary rather than a fully standalone game engine.

Measured progress from the current checkout:
- `tools/analysis/progress.py` reports `1,254 / 7,388` formally ported functions in the `.text` cache: `16.97%` by function count.
- The same report shows `266,797 / 1,919,436` `.text` bytes ported: `13.90%` by code size.
- `tools/analysis/frontier.py` reports `1,260 / 1,527` tracked functions implemented across mapped translation units: `82.5%`.
- `tools/analysis/maintain.py` reports `1,589` symbols identified with objects, `57` not yet identified, and `118 / 160` object files with known source mapping.

The short version: the repo contains a large and growing body of C code, several major translation units are close to complete, but broad coverage of the original game binary is still incomplete and many systems still rely on original code.

## Current Measured Status

| Metric | Current value | Source |
|--------|---------------|--------|
| `.text` size covered by Ghidra cache | `1,919,436` bytes | `progress.py` |
| Formally ported bytes | `266,797` bytes (`13.90%`) | `progress.py` |
| Total functions in `.text` cache | `7,388` | `progress.py` |
| Formally ported functions | `1,254` (`16.97%`) | `progress.py` |
| Functions declared in `kb.json` and counted by progress | `1,448` | `progress.py` |
| Functions listed across `kb.json` objects | `1,529` | `jq '.objects | map(.functions | length) | add' kb.json` |
| Tracked functions implemented in mapped TUs | `1,260 / 1,527` (`82.5%`) | `frontier.py` |
| Symbols identified with objects | `1,589` | `maintain.py` |
| Symbols not identified with objects | `57` | `maintain.py` |
| Object files with known source mapping | `118 / 160` | `maintain.py` |

## Repository Size

Current source footprint under `src/halo`:
- `126` C source files
- `59,978` total C source lines

Some of the largest current translation units are:
- `src/halo/objects/objects.c`: `5,159` lines
- `src/halo/hs/hs_runtime.c`: `2,570` lines
- `src/halo/sound/sound_manager.c`: `2,544` lines
- `src/halo/units/units.c`: `2,504` lines
- `src/halo/main/main.c`: `2,400` lines
- `src/halo/game/players.c`: `2,358` lines
- `src/halo/ai/actors.c`: `2,355` lines
- `src/halo/game/game_engine.c`: `2,170` lines
- `src/halo/effects/decals.c`: `2,026` lines
- `src/halo/interface/ui_widget.c`: `1,942` lines

This is a substantial codebase now; the old "~29K lines across 95 files" estimate is no longer accurate.

## Strongest Areas

Based on `frontier.py`, the strongest currently mapped translation units include:
- `objects.obj`: `80 / 81`
- `units.obj`: `45 / 46`
- `sound_manager.obj`: `41 / 41`
- `ui_widget.obj`: `42 / 42`
- `game_time.obj`: `20 / 20`
- `scenario.obj`: `19 / 19`
- `data.obj`: `16 / 16`
- `real_math.obj`: `18 / 18`
- `random_math.obj`: `11 / 11`
- `tags.obj`: `4 / 4`
- `cseries.obj`: `12 / 12`

In practical terms, the repo currently has especially strong implementation density in:
- Object lifecycle and object-adjacent systems
- Units and unit support code
- UI widget infrastructure
- Sound manager infrastructure
- Core data/memory helpers
- Game time and several math/support translation units

## Active But Incomplete Areas

The following areas have meaningful implementation, but are still clearly incomplete in current analysis:
- `hs_runtime.obj`: `36 / 41`
- `actors.obj`: `24 / 35`
- `vector_math.obj`: `1 / 3`
- `<common>` bucket: `13 / 202`

This means the repo has moved beyond simple stubs in several important subsystems, but still has notable frontier gaps in AI, scripting, utility/common code, and unmapped/shared functions.

## Areas Still Thin Or Uncertain

Some subsystems still look sparse or only partially recovered when judged by current source coverage and mapping status:
- Networking: source exists, but some files are still very small and the system is not close to complete end-to-end.
- Physics: current direct implementation footprint is small; the physics directory is still thin.
- Large unmapped/shared code: the `<common>` frontier remains the single biggest open bucket with `189` remaining functions.
- Unmapped object files: `42` object files still do not have known source mapping according to `maintain.py`.

There are also places where source exists but automated coverage is still uncertain. For example:
- `src/halo/bink/bink_playback.c` is sizable, but `frontier.py` still shows `bink.obj` as `0 / 11` with unknown source mapping.

So the repo should not be described as "fully implemented" in those subsystems just because a file exists or is large.

## Recommended Frontier Targets

Current high-value follow-up targets from `frontier.py` are:
- `<common>`: `13 / 202`, `189` remaining
- `objects.obj`: `80 / 81`, `1` remaining
- `actors.obj`: `24 / 35`, `11` remaining
- `units.obj`: `45 / 46`, `1` remaining
- `vector_math.obj`: `1 / 3`, `2` remaining

This matches the current repo shape well: finish nearly-complete TUs where possible, while continuing to drain the large `<common>` backlog.

## What This Repo Is Today

Accurate high-level description for the current checkout:
- It is a substantial incremental reimplementation of Halo CE Xbox with roughly `14%` code-size coverage and roughly `17%` formal function coverage in the Ghidra `.text` cache.
- It has much deeper recovery in mapped translation units than those topline percentages alone suggest.
- Several major object-backed source files are close to complete.
- The project still depends heavily on the original binary for unported behavior.
- Source mapping and shared/common-function recovery are still major ongoing tasks.

## Outdated Claims Removed

The following older statements are no longer accurate and should not be reused:
- "11 functions formally marked as ported"
- "909 total functions documented"
- "~29,076 lines of C source code across 95 files"
- Broad subsystem claims such as "fully implemented" where current tool output only supports partial or translation-unit-local completion

## Refresh Procedure

To refresh this document after future lift work:
1. Regenerate `build/function_sizes.json` from Ghidra if needed.
2. Run `python3 tools/analysis/progress.py --json`.
3. Run `python3 tools/analysis/frontier.py`.
4. Run `python3 tools/analysis/maintain.py`.
5. Update the numbers here from those tool outputs rather than from old narrative estimates.
