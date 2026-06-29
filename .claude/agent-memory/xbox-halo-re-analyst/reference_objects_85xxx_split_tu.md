---
name: objects-85xxx-split-tu
description: objects.obj owns a non-contiguous 0x85180-0x853c0 sub-range INTERLEAVED with camera/bored_camera.c; check kb source_path per-address before lifting any 0x85xxx target into objects.c
metadata:
  type: reference
---

The objects.c TU is NOT address-contiguous. It owns a small 0x85xxx island (0x85180, 0x85260, 0x85280, 0x85350, 0x853a0, 0x853c0 — cutscene-camera config + frame-time helpers) that is INTERLEAVED in kb.json with `camera/bored_camera.c` functions (0x84fe0, 0x85000, 0x850d0, 0x85110, 0x85150).

**Gotcha:** an address being in 0x85xxx does NOT mean it's bored_camera; and being objects.c does NOT mean it's in the 0x13xxxx-0x145xxx main range. Always read the per-entry `source_path` field in kb.json before lifting a 0x85xxx target into objects.c. Example resolved 2026-06-21: 0x853a0 (`(int)(camera_time * 30.0f)` tick conversion) IS objects.c (source_path confirmed, VC71 100%); 0x85000 (`tag_get('antr')`+`__stricmp`, animation-graph name lookup) has NO source_path but is sandwiched in bored_camera entries → belongs to bored_camera.c, do NOT lift into objects.c (wrong delinked ref + maintain.py violation).

Shared globals across the split: 0x2ee5a8 (cutscene camera time = ticks/30), 0x2ee5a1..0x2ee5d8 (camera state block), 0x46f084 (object_globals ptr, +0x90 PVS state machine 1/2, +0x94 handle-or-cluster). FUN_00085180 (objects.c) writes 0x2ee5a8; FUN_000853a0 reads it back.

**Method:** `grep -n '"0xADDR",' kb.json` to find the line, then read ±4 lines for the `source_path`. Or per-entry `source_path` query. The recursive `[.. | objects? | select(.addr?==$a)]` jq works; the nested-array `arrays` variant does not.
