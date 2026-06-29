# Halo 1 Scripting

## Summary

HaloScript is Halo 1's scenario scripting language. It controls mission structure, cinematics, AI placement, encounters, checkpoints, HUD state, camera control, damage/effects, and many debug/runtime toggles. Multiplayer scripts can drive some effects, but AI is not synchronized in network games.

For the full Reclaimers-derived HSC function/global listing, see [scripting-reference.md](scripting-reference.md). This page is the concise Xbox-focused overview.

## Runtime Behavior

- Script source files use the `.hsc` extension and are associated with a scenario's `data\...\<level>\scripts` folder before being compiled into the processed `scenario` tag.
- Legacy HEK workflows compile scripts through Sapien before they affect the scenario. Newer H1A tools can compile scripts automatically when loading or building a level, but that behavior is not an original Xbox assumption.
- The simulation tick rate is 30 ticks per second. `continuous` scripts run every tick, and `sleep`/`sleep_until` delays are expressed in ticks.
- `startup` scripts begin at level load and are commonly used for mission setup, alliances, intro cinematics, and high-level mission orchestration.
- `dormant` scripts do not run until `wake` is called. Once a dormant script completes, another `wake` does not restart it.
- `static` scripts execute when called by another script and run on the caller's thread. Original Halo 1-style static scripts do not take parameters; H1A parameter support is a later extension.
- `stub` scripts predeclare a static script signature so other scripts can call it before the implementation appears in another script file.
- Globals are level-lifetime script variables declared as `(global <value type> <name> <initial value>)`. Stock campaign scripts commonly split constants/state into `base_*.hsc` and use them from mission or cinematic script files, but the language does not require that layout.
- Script and global counts are limited by the engine. The Reclaimers page states the counts are limited but does not provide numeric values on the main scripting page.

## Value Types

| Type                  | Notes                                                                                                                                                                                                                                                                                 |
|-----------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `void`                | No value; used by functions or static scripts that do not return a value.                                                                                                                                                                                                             |
| `passthrough`         | Returns the type/value of an inner expression, used by forms such as `set`, `if`, and `begin`.                                                                                                                                                                                        |
| `boolean`             | `true`/`false`; official scripts also commonly use `1`/`0`, `on`/`off`.                                                                                                                                                                                                               |
| `real`                | Floating-point value, documented range `3.4E +/- 38` with 6 digits.                                                                                                                                                                                                                   |
| `short`               | 16-bit signed integer, `-32768` to `32767`.                                                                                                                                                                                                                                           |
| `long`                | 32-bit signed integer, `-2147483648` to `2147483647`.                                                                                                                                                                                                                                 |
| `string`              | Quoted string, maximum 32 characters.                                                                                                                                                                                                                                                 |
| Scenario references   | `trigger_volume`, `cutscene_flag`, `cutscene_camera_point`, `cutscene_title`, `device_group`, `ai`, `ai_command_list`, `starting_profile`, `conversation`, `object_name`, `unit_name`, `vehicle_name`, `weapon_name`, `device_name`, and `scenery_name` refer to scenario-tag blocks. |
| Tag/object references | `sound`, `effect`, `damage`, `looping_sound`, `animation_graph`, `actor_variant`, `damage_effect`, `object_definition`, `object`, `unit`, `vehicle`, `weapon`, `device`, and `scenery` refer to runtime objects or tag-derived definitions.                                           |
| Enums                 | Examples include `game_difficulty`, `team`, `ai_default_state`, `actor_type`, and `hud_corner`.                                                                                                                                                                                       |
| `object_list`         | Semi-fixed runtime object lists. Dead units remain counted by `list_count` but not `list_count_not_dead`; deleted objects are removed and free list space.                                                                                                                            |

## Core Function Families

| Family                       | Relevant entries and behavior                                                                                                                                                                                                                                   |
|------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Control                      | `begin`, `begin_random`, `cond`, `if`, `sleep`, `sleep_until`, `thread_sleep`, and `wake` drive script ordering and scheduling. `begin_random` supports up to 32 expressions. Division asserts if the denominator is zero or near zero (`-0.0001` to `0.0001`). |
| Math and logic               | Arithmetic functions operate on numeric expressions. Boolean `and` and `or` are short-circuiting. Bitwise functions listed by Reclaimers are H1A-only and should not be assumed present in the original Xbox executable.                                        |
| AI                           | `ai_place`, `ai_erase`, `ai_living_count`, `ai_conversation`, `ai_follow_*`, `ai_migrate*`, `ai_magically_see_*`, `ai_set_*`, and related functions bridge scripts into the encounter, squad, actor, conversation, and command-list systems.                    |
| Camera and cinematics        | `camera_control`, `camera_set`, `camera_set_relative`, `camera_time`, `cinematic_start`, `cinematic_stop`, letterbox, screen-effect, and title functions control interruptive cutscene state.                                                                   |
| Objects, damage, and effects | Functions such as `object_destroy`, `damage_new`, `damage_object`, object-list functions, and effect/sound functions mutate or query live game-state datums.                                                                                                    |
| Checkpoints and core saves   | `checkpoint_save`, `checkpoint_load`, `core_save`, and `core_load*` interact with saved or debug game state.                                                                                                                                                    |
| Debug/developer console      | Many script functions and external globals are usable from the developer console in debug-enabled builds. The page includes 506 non-server gameplay/debug functions.                                                                                            |
| Server functions             | `sv_*` functions apply to dedicated or locally hosted Custom Edition/PC retail servers, not the original Xbox runtime target.                                                                                                                                   |

The source page contains 506 non-server gameplay/debug function entries and 499 external global entries. Those entries are preserved for local search in [scripting-reference.md](scripting-reference.md).

## Xbox-Specific Notes

- AI is not synchronized in multiplayer. Scripts that rely on AI state should be treated as campaign/local runtime behavior for Xbox reverse-engineering purposes.
- `attract_mode_start` is documented as Xbox-only behavior for playing attract videos after the main menu is idle.
- H1A-only script features, including parameterized static scripts and bitwise functions, should not be treated as evidence for original Xbox Halo CE behavior unless independently verified in the Xbox binary.
- Server-only `sv_*` functions describe PC/Custom Edition hosting behavior and are generally out of scope for Halo CE Xbox runtime lifting.
- Script globals are stored in game state; the Reclaimers game-state page lists `hs globals` with a legacy limit of 1024 and `hs thread` with a legacy limit of 256.

## Reverse-Engineering Relevance

- Search for `hs`, script thread, script global, expression evaluation, and scenario block access when mapping the scripting VM.
- Scenario-linked script value types imply lookups through scenario blocks such as trigger volumes, cutscene flags, device groups, encounters, command lists, AI conversations, object names, and placed object palettes.
- `object_list` behavior matters for datum lifetime: object-list headers are reference-counted and periodically garbage-collected, while deleted objects shrink existing lists.
- Script control functions are useful probes for scheduler state: `sleep -1` parks a thread until another thread wakes or reschedules it.
- Runtime functions often mutate global systems rather than returning values; do not infer meaningful return values for void script handlers without binary evidence.

## Open Questions

- Numeric limits for declared scripts and source-file size are not stated on the main Reclaimers scripting page and should be validated against Xbox data or tool behavior before documenting as constants.
- H1A, OpenSauce, PC retail, Custom Edition, and server-only functions must be separated from original Xbox functions before using the page as a lift target list.
- The exact in-memory layout of script threads, script globals, expression nodes, and compiled script bytecode remains a binary-reverse-engineering task.

## Provenance

Adapted from the Reclaimers Library Halo 1 scripting page. Retrieved 2026-06-25. Text is retained locally as concise notes so this page is readable and searchable without copying the full 506-entry function table. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
