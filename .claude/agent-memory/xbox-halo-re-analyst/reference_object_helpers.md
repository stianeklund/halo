---
name: object helper functions not yet in kb.json
description: Ghidra-confirmed object/unit helper addresses used in players.obj
type: reference
---

Object traversal / accessor helpers found via players_respawn_coop analysis that
are not yet in kb.json:

- `0x13d640` — `object_try_and_get_type(int handle, int type_mask)`: returns
  void* (object data ptr) if the object's type bit is set in type_mask, else NULL.
  Calls `FUN_119270` (datum_try_and_get?). Contrast with `object_get_and_verify_type`
  (0x13d680) which asserts.

- `0x13d7f0` — `object_get_ultimate_object(int handle)`: walks the object parent
  chain (via `object_data + 0xcc`) to find the root object. Returns the root handle.
  Iterates while `handle != -1`, uses `FUN_119320` to get data.

- `0xbbb80` — unnamed player helper in players.obj; takes (player_handle, live_unit,
  spawn_pos*): teleports player to the given position near the live unit. Returns bool.

- `0xbbcb0` — unnamed player helper in players.obj; takes (player_handle): triggers
  the full respawn/spawn sequence for a dead player. Returns void.

- `0x425b0` — unnamed AI helper adjacent to `ai_enemies_can_see_player` (0x425a0);
  calls `FUN_42390(1)`. Likely "AI enemies near player" or similar safety check.
  Only caller: players_respawn_coop.

Key offsets confirmed via players_respawn_coop:
- `players_globals + 0x2c` = `int16_t` respawn_failure (0=none, 1=hazard, 2=AI, 3=alive)
- `players_globals + 0x2e` = `char` respawn_pending flag
- `player_data[player + 0x34]` = unit handle (NONE if dead)
- `unit_data + 0x424`, bit 0 = alive/triggered flag (checked with type mask 1)
- `vehicle_data + 0x428` = passenger-count-like field (SETA, compared > 0; type mask 2)
- Object data `+ 0x50` = position/transform used as spawn anchor
