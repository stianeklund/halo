---
name: biped-430-clobber-fun-1a5300
description: CORRECTED — biped+0x430 (last_good_surface) clobber to 0x30000 is written by FUN_001a5300 (BSP/move update, UNPORTED/original), which ALSO sets +0x434=-1; the prior "+0x434=-1 proves surface-update didn't run" inference is WRONG
metadata:
  type: reference
---

# biped object +0x430 clobber — corrected root cause (2026-06-20)

Supersedes the "residual / biped_new never ran" conclusion in
[[object-type-def-dispatch]]. That memory's "Hardware writers of obj+0x430"
census missed that FUN_001a5300's +0x430 store sits at 0x1a5edf (offset 0xbdf,
beyond a naive 0x800 window) and that it co-writes +0x434=-1.

## The authoritative +0x430 writer census (function-aligned capstone over pristine XBE)
IMPORTANT: a LINEAR capstone sweep of the whole .text desyncs and silently
misses real stores (it failed to decode the known 0x1a0874 store). ALWAYS
disassemble function-aligned from kb.json addrs. Extract all 7612 func addrs by
walking kb.json (top-level keys + every `addr` field), use addr[i+1] as the
bound. XBE map: .text VMA 0x12000, raw 0x2000 → file = vma-0x12000+0x2000.

DWORD literal stores `mov [reg+0x430], r32`:
- 0x1a0874 biped_disconnect (0x1a0860) → -1 ; also writes +0x434=-1
- 0x1a240d FUN_001a2290 jump → -1 ; does NOT write +0x434
- 0x1a279e FUN_001a25e0 surface → ecx (surface idx) ; does NOT write +0x434
- 0x1a49ed biped_new (0x1a4990) → ebx(-1) ; also writes +0x434=-1
- **0x1a5edf FUN_001a5300 → eax = [ebp-0x40] ; ALSO writes +0x434=-1 @0x1a5efc**
- 0x1b57ba vehicle_reset (0x1b5770) → ebx ; also writes +0x434 ; NOT biped

No indexed `[reg+reg*4+disp]` store anywhere can reach 0x430 onto a unit base
(only hit: 0x1480e8 `[eax+ecx*4+0x408]` but eax=[esi+0x14] = a BSP/cluster
accumulator list, count<0x100, NOT the unit). No `lea/add base+0x430` exists in
game .text. So the clobber is NOT an overrun/indexed/computed-pointer bug — it
is a normal `mov [edi+0x430], eax` in FUN_001a5300.

## Root cause framing correction
FUN_001a5300(unit_handle, state) is the biped MOVE/BSP-update step. It:
- edi = object_get_and_verify_type(unit_handle, 1) (biped)
- stores `[ebp-0x40]` → biped+0x430, the +0x438 vec3 (cached_position), and
  -1 → biped+0x434.
- `[ebp-0x40]` is NEVER written by a plain mov; it is filled as an OUT-PARAM by
  a surface/structure-collision query earlier in the body (lea [ebp-0x40];push).
  So +0x430 receives a STRUCTURE-BSP reference (cluster/leaf/surface ref), which
  can legitimately be 0x30000-shaped (packed ref), not a datum handle.

Because FUN_001a5300 sets +0x434=-1 itself, the runtime observation
"+0x434 still -1" does NOT prove the surface-update never ran. It ran on the
first biped update tick after spawn and wrote +0x430.

## Ported status (who is OUR code)
- FUN_001a5300 = **ported=false (ORIGINAL runs)**. The +0x430 store is original
  behavior. The 0x30000 value is the original surface query's output given the
  freshly-spawned biped's BSP state.
- FUN_001a6350 (0x1a6350, units.c:13716 `char FUN_001a6350(int unit_handle)`)
  = **ported=true (OUR lift)** — the biped update dispatcher; sole caller of
  FUN_001a5300 at units.c:13852 with `unsigned char state_pair[2]`. Verified
  FUN_001a5300 reads only state[0]/state[1] (writes state[0]), so the 2-byte
  buffer is correctly sized — NO §2 stack-aliasing bug at this call.

## Where the real bug must be (ranked)
1. UPSTREAM SPAWN STATE: original FUN_001a5300 computes +0x430 from a surface
   query that depends on the biped's position/BSP-connection set during spawn.
   If our ported spawn/activation path (FUN_0003f030 → object_new → biped_new,
   actor_customize_unit, actor_attach_unit, FUN_001a6350 pre-steps) leaves the
   biped's collision/BSP-leaf state wrong, the query returns 0x30000 that
   biped_find_pathfinding_surface_index (0x1a1bc0) later rejects. NOT a single
   store bug — a state-setup bug feeding original code.
2. The dispatcher FUN_001a6350 itself (ported) or a pre-`FUN_001a5300` sub-step
   it calls (FUN_001a4c50 turning, FUN_001a2800, etc.) mutating the biped's
   position/orientation incorrectly before the move step.

## Verified clean (NOT the culprit) on the direct spawn path
- actor_customize_unit (0x3c7c0, ported) change-color loop: offsets
  (i*3+0x4e)*4 / (i*3+0x5a)*4, guarded i<4 → max 0x18c. Disasm-matched.
- unit_enter_seat: unk_680[0x2A8]/unk_696[0x2B8] indexed by seat 0..3 → ≤0x2C4.
- actor_attach_unit: writes unit+0x1a4 (actor_idx) / +0x68 (team) only.
- biped_reset (0x1a09xx) csmemset(obj+0x424,0,0x5c) zeroes 0x424..0x480 — would
  ZERO the whole cache block (incl +0x434) → contradicts +0x434=-1, so it did
  NOT run on these bipeds.
