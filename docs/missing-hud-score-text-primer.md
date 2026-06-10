# Missing HUD Score/Flag Text — Investigation Primer

## RESOLVED 2026-06-10 — root cause found and fixed (user-confirmed on xemu)

The renderer/funnel suspects below were all verified faithful. The actual bug
cluster was in `game_show_score_you_ally_enemy` (0xacff0) and its callers:

1. **0xacff0 itself**: original is a **5-parameter** function (`[EBP+0x18]` =
   other-player handle → ESI → EAX of 0xacef0); kb.json/our lift had 4 params,
   and the C body called `game_engine_hud_update_player(iter, msg, param_1)` —
   message in the EAX slot, **datum handle in the EBX/event-type slot** (§10
   class). The switch in 0xac4e0 (and the engine vtable[0x64] callback) got a
   handle as the message type → default → returned false → nothing queued.
   Fixed: 5th param added (kb.json + impl), call corrected to
   `(iter.datum_handle, param_5, msg)`.
2. **CTF capture caller (FUN_000b0000)**: enemy/ally message IDs swapped —
   original 0xb00ac pushes (you=0x1f, enemy=0x21, ally=0x20).
3. **Race lap caller (FUN_000b39a0)**: lift passed `(0,0,0)` and discarded
   `game_engine_get_variant()`; original selects (0x21,0x22) vs (0x1e,0x1f) on
   `variant+0x4c == 2`, ally 0x20 in both.
4. **Oddball carrier-death caller (FUN_000b3470)**: lift mis-evaluated
   Ghidra's `(-(uint)bVar1 & 0xffffffde) + 0x21` as 0x03; it is **-1** (team
   mode, suppress self message) / 0x21 (FFA).

All six 0xacff0 call sites pass the scoring player's own handle as arg5.
Kill/join/funnel paths (0xad0c0, 0xae7e0, 0xaf660, 0xacf90, 0xd51c0, 0xd5070,
0xac4e0 incl. both jump tables, 0xaceb0) were verified instruction-level
faithful — do not re-bisect them.

## Symptom
In multiplayer, "You scored 1 to 0", "Enemy has the flag", kill/death notifications, and similar game-engine HUD messages never appear. This is confirmed on the patched build; the original unpatched XBE shows them correctly.

## What we've ruled out

### 1. The renderer (FUN_000d5350) is NOT the cause
- `FUN_000d5350` (main HUD messaging renderer, `hud_messaging.c:1008`) is **deactivated** (`ported=false`) — the original code at 0xd5350 runs untouched.
- Temporarily reactivating our lift (`ported=true`) still shows NO score text (and adds a position bug). So the text is never **queued**, not just never rendered.
- The original renderer is called from unported `FUN_000d1400` (0xd1400) via `CALL 0xd5350` at VA 0xd1513. No redirect at that address. The renderer works — it has nothing to draw.

### 2. FUN_000aceb0 data flow is correct (despite wrong param names)
- `FUN_000aceb0` (0xaceb0, ported) dispatches player HUD events through vtable[0x64] or falls back to `game_engine_get_score_hud_text`.
- Its `@<ecx>` param is named `player_handle` but actually carries the **buffer pointer** from callers like `FUN_000ae110`. Similarly `@<eax>` carries buffer capacity, not message_type.
- Inside `FUN_000aceb0`, `mov edi,ecx; mov esi,eax` → EDI=buffer, ESI=capacity → the fallback `call 0xac4e0` receives correct register args.
- The C thunk mapping preserves this: the misleading names don't affect the actual values flowing through.

### 3. FUN_000d6660 (nav_point_draw_single) is separately fixed
Stack canary bug was resolved by merging aliased locals into contiguous arrays. This function is now active and working.

## Where the bug likely is

The message queue is empty. The chain that populates it:

```
Scoring event
  → game_engine_player_event (0xad0c0, ported)
    → game_engine_hud_update_player (0xacef0, ported, @<ecx>/@<eax>/@<ebx>)
      → vtable[0x64] callback OR game_engine_get_score_hud_text (0xac4e0, ported, @<edi>/@<esi>)
        → unicode_sprintf into buffer
      → hud_print_message (0xd51c0, ported)
        → hud_find_message_slot (0xd5070, ported, @<esi>)
          → writes message into slot
```

Every function in this chain is **ported**. The bug is somewhere in this pipeline.

## Top suspects (ordered by likelihood)

### A. `game_engine_get_score_hud_text` (0xac4e0) — wrong unicode_sprintf args
- The function formats text via `unicode_sprintf(buffer, buffer_capacity, format_string, ...)`.
- `buffer` and `buffer_capacity` come from EDI/ESI register args.
- Verify: is `unicode_sprintf` being called with correct format string addresses? The format strings are at fixed addresses like 0x26c63c, 0x26c62c, etc.
- Verify: is the `switch(param_2)` hitting the right cases? param_2 is the event type (0=joined, 1=quit, 7-12=scoring, 14-19=personal scoring with score suffix).
- **Key test:** Add a `display_assert` or debug print inside `game_engine_get_score_hud_text` to confirm it's being called and which case it hits.

### B. `game_engine_hud_update_player` (0xacef0) — vtable callback returning true incorrectly
- Lines 1824-1830: the vtable[0x64] callback is tried first. If it returns `true`, the function assumes text was produced and calls `hud_print_message`.
- But if the callback returns `true` without actually writing text to `buffer`, the function prints garbage/empty buffer.
- If the callback returns `false`, the fallback `game_engine_get_score_hud_text` is tried.
- **Key test:** Check if `got_text` at line 1836 is ever true. If `game_engine_get_score_hud_text` always returns false (maybe the switch falls through to default), no text is ever printed.

### C. `hud_print_message` (0xd51c0) — slot write or player index bug
- Line 922: `if (player == -1) return;` — if the local player index is -1, nothing is queued.
- Line 925: `base = (int)player * 0x460 + *(int *)0x46bd18` — wrong player index → writes to wrong memory.
- Line 928: `ustrncpy` with length 0x3f — if the buffer from upstream is empty, an empty string is queued.
- **Key test:** Read the message slots after a kill event: `getmem addr=<0x46bd18_deref + player*0x460> length=0x90`.

### D. `hud_find_message_slot` (0xd5070) — `@<esi>` register arg bug
- Declared `void *hud_find_message_slot(int base, int param2, int tag_handle@<esi>)`.
- Called from `hud_print_message` with `hud_find_message_slot(base, 0, -1)`.
- The thunk puts -1 into ESI. If the thunk is broken, or if ESI isn't actually used for tag_handle, the slot search could fail silently (return a bad slot or NULL).
- **Key test:** Verify the original disasm of 0xd5070 actually reads ESI as the third parameter. Check the first few instructions after the prologue for `mov reg, esi` or `cmp esi, ...`.

## Diagnostic approach

### Quick path: instrument the queue
```bash
# After a kill event, read the HUD message base to see if anything was written
python3 tools/xbox/xbdm_rdcp.py "getmem addr=0x46bd18 length=4"
# → deref to get HUD base address
python3 tools/xbox/xbdm_rdcp.py "getmem addr=<base> length=0x90"
# Check if slot at +0x00 has a non-zero timestamp (game_time_get result)
# Check if slot at +0x04 has wchar_t text (non-zero bytes)
```

### Medium path: bisect the chain
Deactivate functions one at a time from the bottom up:
1. `hud_print_message` (0xd51c0) → `ported=false`. If text appears, our `hud_print_message` is broken.
2. `game_engine_get_score_hud_text` (0xac4e0) → `ported=false`. If text appears, our score formatter is broken.
3. `game_engine_hud_update_player` (0xacef0) → `ported=false`. If text appears, our dispatcher is broken.

**Caution with deactivation:** These are `@<reg>` functions. The deactivation stub now correctly handles mixed reg+stack params (fixed this session), but verify each stub with `patch.py --test-thunks` after flipping.

### Deep path: trace the original
Use capstone on the pristine XBE to trace the original `game_engine_hud_update_player` flow for a specific event type (e.g., type 0 = "player joined"). Verify each step against our C code instruction by instruction.

## Key files
- `src/halo/game/game_engine.c` — game_engine_hud_update_player (line 1811), game_engine_get_score_hud_text (line 1435), game_engine_player_event (line ~2660)
- `src/halo/interface/hud_messaging.c` — hud_print_message (line 917), hud_find_message_slot (line 800), FUN_000d5350 renderer (line 1008)
- `src/halo/objects/objects.c` — FUN_000ae110 (line 317, calls FUN_000aceb0)

## Session context
- Build rev 27410196 (deployed, d5350 reverted to ported=false)
- The deactivation stub re-push bug was fixed this session (§18 in lift-learnings) — all `ported=false` `@<reg>` stubs are now correct for mixed reg+stack params
- FUN_000d6660 (nav points) was fixed this session (stack aliasing → contiguous arrays)
- Five AI flee-path ABI bugs were fixed this session (FUN_00014c10, 00025970, 00024850, 00024890, 00024000)
