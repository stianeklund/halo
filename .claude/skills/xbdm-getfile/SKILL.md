---
name: xbdm-getfile
tier: agent
triggers: ["core.bin", "core_save", "getfile", "grab the core", "pull the core", "xbox hdd", "xemu hdd", "dump file from xbox", "retrieve file from xbox", "get file off the box"]
description: >-
  Pull a file (e.g. a Halo core_save dump) off the running xemu/Xbox HDD over XBDM
  in ONE command. Use whenever the user says "grab/pull the core.bin", "get the
  dump off the box", or needs any file from the Xbox drive. Do NOT rediscover the
  RDCP getfile wire format or hand-roll a socket loop — the payload is prefixed
  with a 32-bit LE length that must be honored or you truncate the file. Safe to
  run while the game is live (no QMP stop, no gdbstub halt).
---

# XBDM getfile — pull a file off the Xbox/xemu HDD cheaply

**Invoke when** the user asks to grab/pull/retrieve a file (most often a Halo
`core_save` dump `core.bin`) from the running xemu or Xbox HDD, or to list an
Xbox directory.

## One command

```bash
# Fetch the patched-build Halo core dump (default path baked in):
rtk python3 tools/xbox/xbdm_getfile.py --core -o artifacts/equivalence/core_patched.bin

# Any file:
rtk python3 tools/xbox/xbdm_getfile.py 'E:\GAMES\halo-patched\core\core.bin' -o out.bin

# Discover paths + sizes first:
rtk python3 tools/xbox/xbdm_getfile.py --list 'E:\GAMES\halo-patched\core'
```

Host/port default to `localhost:731` (env `XBDM_HOST`/`XBDM_PORT`). It reads the
greeting, sends `getfile name="…"`, honors the length prefix, and writes the
**clean** file (magic at offset 0, complete). Nothing touches QMP (`:4444`) or the
gdbstub (`:1234`), so it will not halt the CPU — run it during live gameplay.

## The one gotcha (why the helper exists)

RDCP `getfile` response after `203- binary response follows\r\n` is:

```
[4-byte LE length N][N bytes of file data]
```

If you read a fixed size instead of that length prefix you get the 4-byte prefix
glued to the front AND the last 4 bytes truncated. The helper reads `N`, then
exactly `N` bytes. Don't hand-roll this again.

## Known paths

| What | Path |
|------|------|
| Patched build core dump | `E:\GAMES\halo-patched\core\core.bin` |
| Core dumps live under | `E:\GAMES\halo-patched\core\` (also `halo`, `derelict` snapshots) |

`core_save` is a Halo debug console command; it writes the game-state pool to
`…\core\core.bin` on the box.

## What a Halo core.bin actually contains (so you don't over-assume)

- Offset 0: magic `0x1fd7a64f`; then scenario tag path (e.g. `levels\c40\c40`
  = **Assault on the Control Room**), then engine build string (`01.10.12.2276`).
- Body: the **game_state pool** (~3.34 MB, `0x345000`) — object datums carry the
  `0x64407440` magic; player/AI/weapon state lives here.
- It is **NOT** the tag cache. Weapon-HUD *tag* data (wphi widget/element
  definitions the rasterizer reads) is loaded separately and is **not** in this
  dump. For a lift diff that needs the actual HUD element bytes, capture the tag
  region via QMP virtual `memsave` during live gameplay (see
  `docs/ab-trajectory-testing.md` / `memsave_snapshot.py`), not from core.bin.

## Do NOT use for memory reads

To read live *memory* (not files), use QMP virtual `memsave` (validated on this
box) — `getmem`/physical reads are unreliable here for game `.data`/heap. This
skill is for **files on the HDD only**.
