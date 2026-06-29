# Halo 1 Reference: CTF

## Source Page

- Source: `https://c20.reclaimers.net/h1/engine/game-modes/`
- Local path: `engine/game-modes/readme.md`
- Scope: Reclaimers-derived Halo 1 reference content retained for local QMD search; verify Xbox runtime behavior against the binary before using as lift evidence.

## Content

There are 5 base **game modes**, also called "game engines", which implement multiplayer game rules and scoring:

| Engine  | `game_variant` names                                                                                                                                                       |
|---------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| King    | `king`, `team_king`, `crazy_king`, `classic_king`, `classic_king_pro`, `classic_crazy_king`, `classic_team_king`                                                           |
| Oddball | `oddball`, `team_oddball`, `classic_oddball`, `classic_team_oddball`, `classic_reverse_tag`, `classic_accumulation`, `juggernaut`, `classic_juggernaut`, `classic_stalker` |
| Race    | `race`, `classic_race`, `classic_rally`, `classic_team_race`, `classic_team_rally`, `team_race`                                                                            |
| Slayer  | `slayer`, `team_slayer`, `classic_slayer`, `classic_slayer_pro`, `classic_phantoms`, `classic_endurance`, `classic_rockets`, `classic_snipers`, `classic_team_slayer`      |
| CTF     | `ctf`, `assault`, `classic_ctf`, `classic_ctf_pro`, `classic_invasion`, `classic_iron_ctf`,                                                                                |

All gametypes are customizations of these base modes. To test game modes in Standalone, use one of the names above with the game_ variant function:

```
game_variant ctf
map_name levels\test\bloodgulch\bloodgulch
```

The game mode will be active for all subsequent uses of `map_name`. If you have previously set a game variant and want to return to singleplayer, use `game_variant none` before loading a scenario.

Objectives like the CTF flags, oddball spawns, and hill boundaries depend on scenario netgame flags placed using Sapien.

The UI localizations for these game variants are found in `ui\default_multiplayer_game_setting_names.unicode_string_list`.

### CTF

The CTF engine is responsible for controlling CTF and Assault games.

During initialization of a CTF game, all player spawn points are validated. If a spawn point is closer to the enemy team's flag than its own team's flag (or vice versa for Assault), its team index is set to `3` which prevents it from being used. If a level artist accidentally places the CTF flags in the wrong bases it can result in all spawn points being disabled and players being forever stuck "waiting for space to clear" rather than spawning.

### King of the Hill

Hills must be defined of a single, or 3-8 hill flags. If defined by just 2, you will encounter the following exception in Standalone:

`EXCEPTION halt in c:\mcc\main\h1\code\h1a2\sources\game\game_engine_king.c,#402: king_globals.hill_point_count != 0`
In Crazy King, the initial hill will be the neural hill with _team index_`0`, then each time the hill moves a random hill is chosen. The game is not guaranteed to cycle through all hill locations.

When testing your hill placements in Crazy King, you can increase game speed with `game_speed_value 4` for example so you don't have to wait so long for the hill to move (normally 1 minute).

### Acknowledgements

Thanks to the following individuals for their research or contributions to this topic:

*   Conscars _(Reversing game variant names)_

## Provenance

Adapted from the Reclaimers Library Halo 1 page `https://c20.reclaimers.net/h1/engine/game-modes/`. Retrieved 2026-06-25. Text is retained locally as a cleaned reference extract for search and reverse-engineering context. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
