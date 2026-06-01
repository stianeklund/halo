# MP End-Game Freeze

**Status:** Open  
**Symptom:** Silent soft-deadlock when a multiplayer match ends (time/score limit reached, or game_over trigger). The postgame fade-in fires, then the game hangs indefinitely — no assertion in debug.txt, threads alive, Xbox still responds to XBDM.  
**Distinct from:** The "Loading level…" freeze (CR2 ≈ float-as-pointer page fault, fixed 9d8b8dc0) and the quit-crash (place<MULTIPLAYER_MAXIMUM_PLAYERS assert, fixed this session).

---

## Signature

- **CR2 = 0** — not a page fault. A thread is spinning or waiting on a signal that never fires.
- Last debug.txt line before the hang: player quit or score-limit line, then silence.
- `xbdm threads` / `getcontext` shows main game thread with EIP stuck in a kernel wait loop (≈ 0x8001exxx), barely moving across multiple samples.
- Contrast with the quit-crash: that produced an explicit `EXCEPTION halt ... place<MULTIPLAYER_MAXIMUM_PLAYERS` line. This produces nothing.

---

## Code location

The postgame sequence is driven by **`game_engine_update_non_deterministic` (0xacdd0, ported=true)**, a phase state machine on global `*(int*)0x5aa730`:

| Phase | Meaning | Advances when |
|-------|---------|---------------|
| 0 | In-game | game ends → phase 1 |
| 1 | Postgame overlay (win/lose/draw shown) | countdown at `0x5aa728` reaches 0 (~5 s) → phase 2? |
| 2 | Score/postgame screen | polls "done" buttons via `game_engine_check_input_button` (@edi = button index) → begins map-cycle or network reset |

A soft-deadlock = a phase that never advances: counter never reaches its target, a "done" poll returns false forever, or a network shutdown call blocks.

**Network postgame path:** `network_client_switch_to_postgame` (0x125610, ported=true).

**Callees of 0xacdd0** (all ported=true, all candidates for a lift regression):
- `FUN_000ab420` (0xab420)
- `FUN_000b9d60` (0xb9d60)
- `FUN_0012eca0` (0x12eca0)
- `FUN_0012a1d0` (0x12a1d0) — ported=false (original)
- `FUN_0012a780` (0x12a780) — ported=false (original)

The 0x12xxxx callees are the networking shutdown/reset path.

---

## First steps

### 1. Regression check
Does the **original unpatched XBE** also hang on MP game-end? Boot from the clean ISO without our patch applied. If it hangs too → emulation/engine issue, not our lift. If it returns to lobby → it's our lift, proceed.

### 2. Toggle-bisect
Per `docs/lift-learnings.md §9`: flip the ported end-game functions to `ported=false` one subset at a time, deploy, end a match.

Deactivate all five at once first to confirm the symptom disappears:
```bash
# In kb.json, set "ported": false for:
# 0xacdd0, 0x125610, 0x12eca0, 0xab420, 0xb9d60
./tools/xbox/build_deploy_run.sh -q
# End a match → should return to lobby
```
If symptom is gone: re-enable in halves to localise. Suggested split:
- A: `0xacdd0` alone (the state machine itself)
- B: `0x125610 + 0x12eca0` (network path)
- C: `0xab420 + 0xb9d60` (helpers)

### 3. Capture the stuck thread
With the box hung:
```bash
rtk python3 tools/xbox/xbdm_rdcp.py --json "threads"
# for each thread ID:
rtk python3 tools/xbox/xbdm_rdcp.py --json "getcontext thread=N"
```
Sample 2–3 times. The spinning/blocked thread's EIP is the deadlock site. A thread blocked on a wait/event during shutdown points at the network reset path (0x125610 / 0x12eca0) not signalling completion. A thread looping with a counter that never changes points at the phase countdown or a "done" poll stuck at false.

---

## What we already know

- `datum_get` / `datum_absolute_index_to_index` both return non-zero for quitting players (salt unchanged) — any guard using these will NOT filter out players absent from `data_iterator_next`. This is why the quit-crash fix had to be at the search-bound level, not the guard level. Relevant if the freeze is related to player teardown.
- The score overlay and kill notifications are now working correctly — those code paths are no longer confounders.
- The freeze is **not** caused by `FUN_000abf50` / `FUN_000ae920` / the score overlay chain (all fixed this session).
