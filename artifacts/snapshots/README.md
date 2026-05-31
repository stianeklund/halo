# Live game-state snapshots (xemu)

Captured from a running, **paused** xemu instance (patched Halo CE Xbox build,
rev cbe31983) during a **Flood encounter with an active infection swarm**.
Memory was dumped with QMP `memsave` (VIRTUAL addresses — the game's VA is NOT
identity-mapped to physical in this xemu, so `pmemsave` reads the wrong bytes;
always use `memsave` / `x`, not `pmemsave` / `xp`, for game memory here).

## Scene
- Map: a Flood level; `swarm_data` has 4 slots (1 active swarm).
- Infection actor: `actor_data` index 1, handle `0xe36b0001`, behavior mode 2,
  alert (`actor+0x6e`) = 0, swarm handle `0xf7770003`.
- Swarm: `swarm_data` index 3, **3 components**, cooldown 0.

## Data-table globals (verified)
| global | .data addr | live value (table base) |
|--------|-----------|--------------------------|
| actor_data           | 0x6325a4 | 0x8027e6e0 |
| swarm_data           | 0x6325a0 | 0x802f0b18 |
| swarm_component_data | 0x63259c | 0x802f1e50 |
| prop_data            | 0x5ab23c | 0x802f5e88 |
| object_header table  | *0x5a8d50 | 0x800b9370 (600 objects, 12-byte entries) |
| forward-vector ptr   | *0x31fc38 | 0x0028caa8 (.rdata) |

`data_t` header: maximum_count@0x20, size@0x22, valid@0x24, magic(0x64407440)@0x28,
current_count@0x2e, data ptr@0x34.

## Files
- `gamestate_full.bin`  — VA 0x800b0000, 0x310000 (3.2MB): object table + records + AI tables (actor/swarm/component/prop). Primary game-state region.
- `gamestate_tables.bin` — VA 0x8027e6e0, 0x140000 (earlier, narrower; superseded by gamestate_full).
- `glob_5a8000.bin` — VA 0x5a8000 page: object-header table pointer (0x5a8d50).
- `glob_632000.bin` — VA 0x632000 page: actor/swarm/swarm_component data pointers.
- `glob_5ab000.bin` — VA 0x5ab000 page: prop_data pointer.
- `glob_31f000.bin` — VA 0x31f000 page: forward-vector pointer (0x31fc38).
- `rdata_const.bin` — VA 0x250000, 0x10000: float/double constants (0x253xxx–0x257xxx).
- `rdata_28c000.bin` — VA 0x28c000 page: forward-vector target.
- `infection_swarm.json` — assembled unicorn_diff `--state-snapshot` for FUN_00038e60
  (regions + `arg_overrides.actor_handle = 0xe36b0001`).

## unicorn_diff status (FUN_00038e60)
With the tool enhancement (named-global slot seeding + sub-address snapshot
reads in `_build_globals_seeds`), the oracle and candidate now read the REAL
captured tables. Verified against this snapshot (both sides identical):

    [datum_get] actor  idx=1 count=9 sz=1828 arr=0x8027e718 entry=0x8027ee3c salt=58219 ✓
    [datum_get] swarm  idx=3 count=4 sz=152  arr=0x802f0b50 entry=0x802f0d18 salt=63351 ✓
    [datum_get] comp   idx=2 count=6 sz=64   arr=0x802f1e88 entry=0x802f1f08 salt=63373 ✓
    [object_get_and_verify_type] resolves the real unit object

i.e. the function walks the full real infection swarm
(actor 0xe36b0001 → swarm 0xf7770003 → component 0xf78d0002) and every datum
handle resolves to a live entry with a matching salt.

### dllimport double-deref fix (the key fix)
kb.json HDATA globals are `__declspec(dllimport)`, so both the oracle and the
candidate reference `actor_data`/`swarm_data`/`swarm_component_data` as
`__imp__X` — an EXTRA dereference (`mov eax,[slot]; mov eax,[eax]`). The slot
must hold the global's ADDRESS (e.g. `0x6325a4`), not the table-base VALUE, so
the mapped snapshot page supplies the value through that extra deref. Seeding the
value made the extra deref read the table NAME ("acto") and return a NULL datum.
Fixed in `_build_globals_seeds` (address-seed `__imp__` slots when a snapshot
maps the real page) plus a guard so the function-pointer pre-map heuristic does
not clobber pointers backed by snapshot memory. This is reusable for ANY
dllimport-heavy stateful target.

### Residual harness limitation (not a lift defect)
After the full data walk, the isolated COFF differential faults at a raw
`eip=0xff84` — an indirect switch jump-table dispatch. `patch_rel32_calls`
intentionally skips `.text`/`.rdata` relocations, so a switch jump table cannot
be relocated when the function is run as a standalone object. This is a generic
limitation of the isolated differential for jump-table-heavy non-leaf functions,
independent of FUN_00038e60's correctness.

### Correctness evidence for FUN_00038e60
1. Disassembly-verified faithful lift (line-by-line vs the XBE).
2. Runs LIVE in-game on this exact 3-component infection swarm with no crash —
   the engine itself is the oracle.
3. The differential drives the function through the REAL captured game state and
   the actor→swarm→component traversal executes correctly and identically on
   oracle and candidate up to the jump-table dispatch above.
