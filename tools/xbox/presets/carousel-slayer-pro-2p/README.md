# Carousel / Slayer Pro / two-player comparison reference

`preset.json` combines the full live host variant with the target positions and
facing from the earlier debug comparison. It was read from `10.0.0.24`, running
`E:\GAMES\halo-patched\cachebeta.xbe`, on 2026-10-03. Capture changed no guest
memory, files, or execution state.

Confirmed configuration:

- Loaded map: `levels\test\carousel\carousel`.
- Variant name: **Slayer Pro**, Slayer engine 2, team play disabled, score limit 25.
- Both local players carry an assault rifle and pistol.
- Active variant (`0x456af8`), requested variant (`0x5aa820`), UI selection
  (`0x46bfcc`), and playlist (`0x5aa7a0`) match byte for byte.
- A normal multiplayer server/client session is initialized.

The map was read from `game_globals->game_options.map_name`. The console's
`map_name` startup string remains `levels\b40\b40` on this host and does not
describe the map selected through the multiplayer lobby. Likewise,
`player_spawn_count == 1` is the menu startup setting, not the count of players
joined through the lobby; the player pool contains two live local players.

## Pose targets for the next comparison

Use `comparison_targets_from_previous_debug_run` for the earlier comparison
views. Use `current_host_player_sample` only as evidence of this new game's
positions and inventory. The two sets of positions are different.

Map the targets to the first and second local viewports on each instance,
resolving their current controller slots and live unit handles. The host's
sampled controller slots are 1 and 2; the old debug startup used slots 0 and 1.
Never reuse the captured object addresses or handles as write destinations in
a later run.

The full 104-byte variant is embedded in `variant.hex`; `variant.bin` contains
the same bytes. Its SHA-256 is
`77a9cc559a0dc4e9f10e6833d5a263d541e830a5dbee2cc7f259843c5bdd81ce`.
The JSON remains usable if Git ignores binary files.

## Restore status

This is a captured reference, not an installed startup loader. A future
automatic test run must load the full variant before spawning, initialize the
multiplayer session when normal pause menus are needed, and separately match
or restore the two camera poses. The native `game_variant slayer` command
loads a different preset; `game_variant slayer_pro` is unsupported in 2276.

Carousel has no named cutscene flags or player starting-equipment profiles,
so the previous proposed flag/profile script commands do not provide the
missing restore path. A pose-restoration helper still needs implementation
and runtime validation. Matching positions/facing also does not align game
ticks or weapon animation phases automatically.
