# Slayer Pro: captured Halo Xbox 2276 variant

`variant.json` preserves the exact 104-byte variant captured from the user's
normal split-screen game on 2026-10-03. The reference was `cachebeta.xbe` on
`10.0.0.24`, running Carousel. Both instances' active records initially matched,
and all four initial players had an assault rifle and pistol. The host's active,
requested, UI-selected, and playlist variant copies also matched.

The name is the first 24 bytes interpreted as UTF-16LE: **Slayer Pro**. Engine
type at `+0x18` is 2 (Slayer), team play at `+0x1c` is 0, and the score limit at
`+0x40` is 25. Retain unnamed fields and trailing bytes exactly as captured;
the JSON labels unknown scalars by offset rather than assigning guessed names.

The useful loadout difference from the native debug Slayer preset is the flag
word at `+0x20`: **0x23 instead of 0x03**. In
`game_engine_postspawn_player_update()` (`0xad3e0`), bit `0x20` skips
`FUN_000ad2b0()`, which applies the map's starting equipment. The observed Pro
loadout retains the assault rifle and pistol. This identifies the relevant
equipment path; no isolated one-bit runtime experiment was performed.

## Use on the next deployment

Keep the normal menu startup file until a custom-variant loading path has been
implemented and validated:

```text
player_spawn_count 1
game_variant none
```

Start **Multiplayer → Split Screen**, join two players, and select **Slayer Pro**
and Carousel. This is the tested way to reproduce both the selected variant and
normal multiplayer session initialization.

The 2276 debug command recognizes only its fixed list of preset names; neither
`game_variant slayer_pro` nor `game_variant "Slayer Pro"` selects this record.
Uploading `variant.bin` by itself has no effect: Halo has no verified startup
loader for this file. A future automatic deployment must explicitly load this
variant through a verified path and initialize the multiplayer session before
map startup. Do not overwrite the active variant after players have spawned
and assume it will retroactively replace their equipment.

`variant.bin` is a local binary copy of `variant_hex` in the JSON. Its size is
104 bytes and SHA-256 is
`77a9cc559a0dc4e9f10e6833d5a263d541e830a5dbee2cc7f259843c5bdd81ce`.
The JSON is the portable source of truth if binary files are ignored by Git.

All capture operations were reads. No new XBE, init file, or guest memory
change was deployed during this identification.
