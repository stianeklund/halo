# System link regression: `transport_error_connection_lost`

Investigation notes for the networking regression observed when one xemu
instance runs our patched `default.xbe` and the other runs the unpatched
`cachebeta.xbe` (2276 beta). Unpatched ↔ unpatched works; any mix involving
our patched build fails shortly after connect.

## Observed error (from in-game log)

```
client call to write_endpoint() returned error 'transport_error_cor<...>
network_connection_idle_client_reliable_endpoint failed
the game host went down
```

The first line is truncated. It can only resolve to one of two values from
the transport error enum:

- `_transport_error_connect_failed` (0xfff0)
- `_transport_error_connection_lost` (0xfffd) — most likely given the
  "host went down" line immediately after

## Strings and callers (from Ghidra on unpatched `cachebeta.xbe`)

| Address    | String                                                           |
|------------|------------------------------------------------------------------|
| `0x266170` | enum table starts: `_transport_error_poll_error` …               |
| `0x266438` | `_transport_error_none`                                          |
| `0x28b768` | `the game host went down`                                        |
| `0x294d6c` | `client call to write_endpoint() returned error '%s'`            |
| `0x2950cc` | `blocked in network_connection_idle_client_reliable_endpoint`    |
| `0x2954cc` | `network_connection_idle_client_reliable_endpoint failed`        |

### Call chain (all addresses are in the unpatched binary, none ported)

- `FUN_00128e00` — `network_connection_write` (source path string points to
  `c:\halo\SOURCE\networking\network_connection.c:0x170`). Calls
  `FUN_00082f50` (`write_endpoint`). On a non-recoverable negative return
  (`!= -4`) it formats the error via `FUN_00081c80` and prints
  `client call to write_endpoint() returned error '%s'`.
- `FUN_00129cf0` — `network_connection_idle`. Calls
  `FUN_001294d0` (`network_connection_idle_client_reliable_endpoint`).
  On failure prints `network_connection_idle_client_reliable_endpoint failed`.
- `FUN_00081c80` — transport-error code → string converter. Relevant cases:
  - `0xfff0` → `_transport_error_connect_failed`
  - `0xfffd` → `_transport_error_connection_lost`

None of `0x128e00`, `0x129cf0`, `0x1294d0`, `0x82f50`, `0x82e50`, `0x83040`,
`0x84740`, `0x81c80`, `0x118e90`, `0x118ec0` are in `kb.json` or ported.
They thunk implicitly to original bytes, so on the face of it our build
should execute the same code as the unpatched one on this path.

## What we've actually touched in the networking area

- `src/halo/bungie_net/network/transport_endpoint_set_winsock.c`
  - `transport_dispose` @ `0x822d0` — 7-line reimplementation, shutdown only
  - `transport_initialize` @ `0x82130` — declared in `kb.json`, not
    implemented in C (thunked to original)
- `src/halo/networking/network_messages.c`
  - `initialize_network_game_packets` — single-line passthrough to
    `0x11A930`; behavior identical to unpatched
- Call sites in `src/halo/game/game.c`:
  - `game_initialize` (line 37) calls `transport_initialize`
  - `game_dispose` (line 84) calls `transport_dispose`
- `game_initialize` / `game_dispose` call ordering was compared against
  `FUN_000a6bf0` / `FUN_000a6d00` in the unpatched binary and matches.

Struct sizes also match:

- `game_globals_t` = `0x114`
- `game_variant_t` = `0x68`
- `game_options_t` = `0x10c`

## Why the unpatched ↔ unpatched evidence matters

User tested two xemu instances running the unpatched `cachebeta.xbe`
against each other — system link works. Any xemu-level networking
limitation would fail identically there, so the regression is introduced
by *our patched image*.

That leaves three classes of candidate cause:

1. A ported function that runs during active gameplay and subtly alters
   tick cadence, network-state timing, or memory.
2. A `kb.json` declaration with the wrong indirection level or argument
   count corrupting state the networking path reads.
3. A struct layout / field-offset mismatch in a struct the networking
   code reads (less likely — networking structs are internal to unported
   code).

## Most suspicious recent commits (runs during gameplay)

| Commit    | Area                                                                   |
|-----------|------------------------------------------------------------------------|
| `0176fd2` | `game_time fixes` — adds a pregame bootstrap branch to `game_time_update` that calls `((void (*)(int, bool))0x12e1d0)(server, false)` (`network_server_manager` timeout-tracker reset). |
| `3fdb9cd` | `sound_manager`, `widgets_update`, "fix empty stub crash" — `widgets_update` runs every frame. |
| `55649e5` | `recorded_animations_update` helpers — runs every tick.                |
| `e8dc1a5` | `hs` / `hs_runtime_dispose` — `hs_update` runs every tick.             |
| `9cc4c2f` | `observer.obj` — observer init/dispose/get_camera.                     |
| `d73868a` | `director.obj` — director init/dispose.                                |

### Top hypothesis: `game_time.c:227-383` (`game_time_update`) pregame branch

Added at `0176fd2`. When `game_connection() == 2` and the server is still
in pregame state (`server_state == 0` with `server_min_time == 0`, or
`server_min_time == 0xffffffff`), we now call:

```c
((void (*)(int, bool))0x12e1d0)(server, false);
```

`FUN_0012e1d0` in the unpatched binary is
`network_server_manager.c:0x59e`-area timeout enforcement. Called with
`false`, it zeroes the 4 per-client timeout-start timestamps at offsets
`server+0x444`, `+0x454`, `+0x464`, `+0x474`. Called every frame during
the bootstrap window, this continuously resets the timeout tracking.
Stock 2276 beta is reported to not hit the original drift assert in that
window, but whether it calls `0x12e1d0(server, false)` in that same
window is not verified — this is the asymmetry most likely to shape
observable network-state differences between our build and unpatched.

## FUN_0012e1d0 reference (abridged)

```c
// Called with param_2 == false: clear per-client timeout timestamps
puVar7 = (int*)(param_1 + 0x444);
iVar8 = 4;
do { *puVar7 = 0; puVar7 += 4; iVar8--; } while (iVar8 != 0);

// Called with param_2 == true: scan clients, escalate or forcibly remove
// after ~2s of no progress via FUN_0012df50.
```

## Recommended bisect

Run patched-host ↔ unpatched-client (or the reverse) after each revert
until connect + in-game session is stable for more than a few seconds:

```
git revert --no-edit 0176fd2   # game_time fixes
# if still broken:
git revert --no-edit 3fdb9cd   # widgets_update, sound_manager
# if still broken:
git revert --no-edit 55649e5   # recorded_animations helpers
git revert --no-edit e8dc1a5   # hs / hs_runtime_dispose
git revert --no-edit 9cc4c2f   # observer init/dispose
git revert --no-edit d73868a   # director init/dispose
```

If `0176fd2` is the culprit, the fix is to tighten the pregame branch so
it does not call `0x12e1d0(server, false)` — either skip that call
entirely in the bootstrap window, or match the unpatched binary's exact
behavior in that state (TBD: disassemble `FUN_000b6020` on unpatched and
verify what it does when `game_connection() == 2` and the server is in
pregame state 0).

## Next steps for a fresh session

1. Reproduce the failure: one xemu with `halo-patched/default.xbe`, one
   with unpatched `cachebeta.xbe`, try Create Game → Join.
2. Confirm which side emits `transport_error_cor<...>` — host or client.
3. Run the bisect above.
4. Before merging any revert, capture the full error code by temporarily
   re-implementing `FUN_00128e00` (`network_connection_write`) at
   `0x128e00` so we can log `iVar2` before the `FUN_00081c80` call.
5. Once the bad commit is identified, compare `FUN_000b6020`
   (`game_time_update`) disassembly on unpatched against our
   `src/halo/game/game_time.c:227` line-by-line — do not assume the
   decompiler in commit `0176fd2` produced a faithful port.

## Files consulted

- `src/halo/bungie_net/network/transport_endpoint_set_winsock.c`
- `src/halo/networking/network_messages.c`
- `src/halo/game/game.c`
- `src/halo/game/game_time.c`
- `src/types.h`
- `kb.json` (entries for `transport_endpoint_set_winsock.obj`,
  `network_game_globals.obj`, `network_messages.obj`, `game.obj`)

## Addresses quick reference

| Address    | Symbol (inferred / decompiler)                                   |
|------------|------------------------------------------------------------------|
| `0x82130`  | `transport_initialize` (declared, thunked)                       |
| `0x822d0`  | `transport_dispose` (ported)                                     |
| `0x81c80`  | transport error-code → string                                    |
| `0x82f50`  | `write_endpoint` (reliable)                                      |
| `0x82e50`  | `read_endpoint` (reliable)                                       |
| `0x84520`  | datagram `read_endpoint`                                         |
| `0x84740`  | datagram `write_endpoint`                                        |
| `0x128e00` | `network_connection_write`                                       |
| `0x1294d0` | `network_connection_idle_client_reliable_endpoint`               |
| `0x129cf0` | `network_connection_idle`                                        |
| `0x12a1d0` | server manager accessor (pregame / server object getter)         |
| `0x12d5b0` | returns `server_min_time` (min of per-client sync times)         |
| `0x12e1d0` | `network_server_manager` timeout tracker                         |
| `0x118e90` | circular queue: bytes available                                  |
| `0x118ec0` | circular queue: enqueue                                          |
| `0x1d0581` | `system_milliseconds` / reads `KeTickCount`                      |
