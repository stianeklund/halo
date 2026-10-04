# Carousel split-screen visual comparison

For captures at a shared simulation tick and render event with exact player
poses, use [the frame capture procedure](carousel-frame-capture.md). The earlier
near-simultaneous scanout captures in these notes did not establish that alignment.
The new alignment procedure changes runtime poses and weapon presentation state;
the earlier read-only memory-inspection description does not cover that procedure.

Tested on 2026-10-03 with Halo Xbox debug build 2276. This starts two
**independent debug map launches**, each with Slayer rules and two local players.
It does not initialize a normal split-screen multiplayer session or join a
System Link session. The two instances can remain configured for
bridged networking so XBDM remains reachable.

## Normal split-screen menus and Slayer Pro

The direct map-launch recipe below bypasses the multiplayer lobby. On both
tested guests, the network game server/client pointers at `0x46e8bc` and
`0x46e8c0` were zero and the connection state at `0x46da0c` was zero. The pause
menu checks these session pointers rather than the Slayer engine selection.
With two players and no session it tries the solo/co-op split-screen pause
widget, producing `failed to load split screen pause game window` when that
widget cannot load. This recipe is therefore unsuitable when you need normal
multiplayer pause menus or the menu's Slayer Pro variant.

To return to the normal startup flow, save the current `init.txt`, replace it
on both guests with the following complete file, and restart the same XBEs
using the commands below:

```text
player_spawn_count 1
game_variant none
```

There is no `map_name` command in this file. Start the game through
**Multiplayer → Split Screen**, join two players, and choose **Slayer Pro** and
Carousel in the lobby. The lobby initializes the local multiplayer server and
client; the preset `game_variant slayer` is not equivalent to selecting Slayer
Pro in that lobby. Capture the resulting variant and weapon inventory before
claiming its loadout has been reproduced automatically.

The startup files installed immediately before the menu correction are backed
up under `tmp/carousel-menu-20261003/10.0.0.21-init-before.txt` and
`tmp/carousel-menu-20261003/10.0.0.24-init-before.txt`. The replacement file is
`tmp/carousel-menu-20261003/menu-init.txt`. Only `init.txt` is deployed; the XBEs
are preserved and no game-memory settings are written.

Source evidence: `ui_check_for_pause_game()` in
`src/halo/interface/ui_widget.c` and `split_screen_game_initialize()` in
`src/halo/interface/ui_widget_event_handler_functions.c`.

## Files deployed

The complete startup file is checked in at
[`tools/xbox/presets/carousel-2p/init.txt`](../tools/xbox/presets/carousel-2p/init.txt):

```text
player_spawn_count 2
game_variant slayer
map_name levels\test\carousel\carousel
```

Upload this file to `E:\GAMES\halo-patched\init.txt` on both guests. Halo opens
that title-relative file as `d:\init.txt` at startup. `player_spawn_count`
controls how many local players `create_local_players()` creates. `game_variant`
selects the multiplayer preset, and `map_name` requests Carousel.

**Only `init.txt` was uploaded for the successful two-player Slayer boot.** No
XBE was built or uploaded. No `cheats.txt`, `camera.txt`, core, input recording,
or replay sentinel was required. XBDM memory access was read-only: it verified
the map, player pool, unit handles, positions, facing, and weapon tags. It did
not place players or change their facing.

## Tested instances

| XBDM address | Executable used |
|---|---|
| `10.0.0.21:731` | `E:\GAMES\halo-patched\standalone_seed.xbe` |
| `10.0.0.24:731` | `E:\GAMES\halo-patched\cachebeta.xbe` |

These were the executables already running. Preserve the build you intend to
test; do not silently substitute `default.xbe` for `standalone_seed.xbe`.
The addresses are guest **debug** addresses, not necessarily title-network
addresses. Both guests already had `maps\carousel.map`.

## Reproduce from WSL

Run from `/mnt/g/dev/halo`. These bridged guests were reachable from WSL;
native Windows XBDM access is unreliable with this npcap setup.
`HALO_WINDOWS_REEXEC=1` prevents the RDCP tool from handing itself to Windows
Python. Explicit port `731` is required here; port `730` refused connections.

Before replacing an existing `init.txt`, save it with `xbdm_getfile.py`. A
missing file is normal; the host had none before this session.

```bash
HALO_WINDOWS_REEXEC=1 rtk python3 tools/xbox/xbdm_getfile.py --host 10.0.0.21 --port 731 'E:\GAMES\halo-patched\init.txt' -o tmp/carousel-init-before-client.txt
```

Upload the checked-in preset to each guest:

```bash
HALO_WINDOWS_REEXEC=1 rtk python3 tools/xbox/xbdm_rdcp.py --host 10.0.0.21 --port 731 --timeout 30 --sendfile /mnt/g/dev/halo/tools/xbox/presets/carousel-2p/init.txt 'E:\GAMES\halo-patched\init.txt'
HALO_WINDOWS_REEXEC=1 rtk python3 tools/xbox/xbdm_rdcp.py --host 10.0.0.24 --port 731 --timeout 30 --sendfile /mnt/g/dev/halo/tools/xbox/presets/carousel-2p/init.txt 'E:\GAMES\halo-patched\init.txt'
```

Restart the existing titles; this is a title restart, not a rebuild or upload:

```bash
HALO_WINDOWS_REEXEC=1 rtk python3 tools/xbox/xbdm_rdcp.py --host 10.0.0.21 --port 731 --timeout 30 'magicboot title=E:\GAMES\halo-patched\standalone_seed.xbe debug'
HALO_WINDOWS_REEXEC=1 rtk python3 tools/xbox/xbdm_rdcp.py --host 10.0.0.24 --port 731 --timeout 30 'magicboot title=E:\GAMES\halo-patched\cachebeta.xbe debug'
```

Allow the map to load, then verify both displays have two viewports. Connect
matching controller configurations and avoid movement/look input during
comparison. An accepted `magicboot` response alone does not prove a map loaded.
Leave the preset installed if you want subsequent title starts to repeat it.

## What was actually verified

Both instances reported Carousel, `player_spawn_count == 2`, Slayer engine
record `0x2f00f8`, two live local player units, valid player-pool magic
`0x64407440`, and a clear assert/halt byte. Game ticks advanced.

Their corresponding player slots initially selected the same spawn locations
and exactly the same yaw/pitch. This happened through normal engine spawning;
no spawn-location override or teleport was used.

| Local slot | Position sampled on the client | Yaw (radians) | Pitch |
|---|---|---|---|
| 0 | `(-4.6619034, -11.3691101, -0.8562036)` | `1.1448506` | `0` |
| 1 | `(5.8109126, 3.6055307, -2.7239900)` | `0.7155910` | `0` |

Slot 0's sampled position matched exactly. Slot 1's settled host position
differed by approximately `(0.00778, 0.00676, -0.00275)` world units. Therefore
this is a tested convenient starting setup, **not a guarantee of exact camera
or pixel equality**. The samples were at different game ticks. Exact visual
regression tests also need frame/tick alignment and matching rendering settings.

Carousel's loaded scenario contained 63 player starting locations, but zero
cutscene flags, cutscene camera points, and player starting-equipment profiles.
Consequently the proposed flag-based `object_teleport` commands and named-profile
`player_add_equipment` commands cannot be used on this map as shipped.

## Starting weapons

For the next comparison run, the latest host capture is bundled with the
earlier comparison camera targets in
[`tools/xbox/presets/carousel-slayer-pro-2p/preset.json`](../tools/xbox/presets/carousel-slayer-pro-2p/preset.json).
The accompanying README distinguishes the old target poses from the new
game's actual player positions. This bundle is a reference payload; automatic
variant loading and pose restoration still require a verified helper.

The later normal split-screen test identified **Slayer Pro** by its UTF-16LE
variant name in memory. Both instances initially had the same 104-byte active
record, and all four players had an assault rifle and pistol. The original
host's active, requested, UI-selected, and playlist copies also matched. The
complete record is saved in
[`tools/xbox/presets/slayer-pro-2276/variant.json`](../tools/xbox/presets/slayer-pro-2276/variant.json),
with reuse instructions in that directory's README. Its flags at `+0x20` are
`0x23`, versus `0x03` in the debug Slayer preset; the extra `0x20` bit skips
the map-specific starting-equipment path. No memory-write experiment was used.

There is no native `game_variant slayer_pro` alias in 2276. Keep the normal
menu startup for now and select Slayer Pro through the split-screen lobby.
The captured payload is ready for a future verified custom-variant loader;
uploading the payload alone does not load it or initialize a session.

The tested `game_variant slayer` debug preset gave both players a **plasma
pistol**, verified by the live weapon tag path. It does not establish a human
pistol/assault-rifle loadout. A temporary CTF preset test gave plasma pistol and
plasma rifle; CTF was rejected and the host was restored to Slayer. Do not use
that test as the recommended recipe.

## Restore previous startup behavior

Restore your saved `init.txt`, or delete the test file if no file existed:

```bash
HALO_WINDOWS_REEXEC=1 rtk python3 tools/xbox/xbdm_rdcp.py --host 10.0.0.24 --port 731 --timeout 30 'delete name=E:\GAMES\halo-patched\init.txt'
```

For this session, the original client file and per-instance backup manifests
were saved locally under `tmp/carousel-debug-20261003/10.0.0.21/` and
`tmp/carousel-debug-20261003/10.0.0.24/`. These are local recovery artifacts,
not required deployable files.

## Implementation evidence

- `src/halo/main/console.c`: `console_startup()` evaluates `d:\init.txt` lines.
- `src/halo/main/main.c`: `create_local_players()` loops over `player_spawn_count`.
- `src/halo/game/game.c`: `game_set_game_variant_from_name()` selects the preset.
- `src/halo/game/players.c`: `find_best_starting_location_index()` chooses normal spawns.
- `docs/xemu-bridged-deploy.md`: WSL-native XBDM access to these bridged guests.

The existing `boot_gametype.py` stages only `game_variant` and `map_name`; it
does not include `player_spawn_count 2`. Use the complete preset above for this
two-player setup.
