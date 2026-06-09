---
name: biped-branch-drivability
description: Branch-drivability map for dormant bipeds 0x1a0680/0x1a1e70/0x1a0e00 — which branches a dual-oracle MATCH can witness on a live biped, and the single caller that supplies each function's gating input
metadata:
  type: reference
---

Measured 2026-06-08 (worktree scalable-coalescing-lobster). Pure analysis; no
source/kb edits. Decides whether a dual-oracle MATCH is conclusive (witnesses
behaviorally-distinct branches) or vacuous (only the common path runs).

## Sole callers (the activation context — read these to drive branches)
- **0x1a0680** ← FUN_001a6280 (`post-limp-noodle` death state). Calls 0680 ONLY
  when `obj+0x47c (step ctr) < obj+0x47d (max)` AND global `DAT_004e4cf3==0`.
  So in the real call path the counter is BELOW max → 0680's own early-exit
  (0x76b, fires when `0x47c >= 0x47d`) does NOT fire; the node-copy loop path
  runs. FUN_001a6280 itself is ported=NULL (inactive) → activation caller-safe.
- **0x1a0e00** ← FUN_001a5300 @0x1a5fa1. Gate: `FLD [EBP-0x20]; FCOMP 0.0;
  TEST AH,0x41; JNZ skip` → call fires iff `threshold(local_20) > 0.0`. Arg
  setup: `MOV EAX,ESI` where **ESI = unit HANDLE** (prologue 0x1a530a
  `MOV ESI,[EBP+8]`; EDI = obj ptr). So @<eax> = unit handle (matches kb decl
  and 0e00's own `object_get_and_verify_type(EAX,1)`). `threshold` = per-tick
  movement/turn magnitude produced inside FUN_001a2f40 (physics step) earlier
  in the frame; same value reused by `FUN_001a0be0(local_20)` at 0x1a61d1.
- **0x1a1e70** ← FUN_001a6350 (dying/recovery dispatcher, sibling of 6280).

## 0x1a0e00 branch math (constants confirmed)
`_DAT_002546a4 = 0x3d088988 = 1/30` (per-tick scale). `0x253394 = 30.0`.
`FLOAT_002533c0 = 0.0`. fVar1 = tag[0x3dc]/30, fVar2 = tag[0x3e0]/30 (ordered
phase boundaries). Reach the obj writes only if `fVar1 <= threshold` AND
`0.0 < spread`. Then **CX=1** (`word[obj+0x460]=1`) iff `fVar2 <= threshold`
(inner, MOV ECX,1 @0x1a0e8a); **CX=0** iff `fVar1 <= threshold < fVar2`
(XOR ECX,ECX @0x1a0e71). Also writes byte[obj+0x428]=0 and byte[obj+0x429]=
_ftol2(...). The two tag fields bracket the per-tick threshold → branch select
is data-driven by the bipd tag's two scaled boundaries.
COMBAT-CAPTURE NOTE: prior equivalence (snapshot memory) witnessed 0e00
PRODUCT-EQUIVALENT but with word[obj+0x460]=0 → only the CX=0 leg was driven.
CX=1 requires threshold >= fVar2, NOT yet witnessed → that's the gating branch.

## 0x1a1e70 recovery gate (constants confirmed)
Long AND-chain of biped-state guards (obj+0xb6&4, tag+0x2f4&0x84, obj+0x1b4
AH&0x10, obj+0x1a4!=-1, obj+0x253!=0x1d, obj+0x459>0x1e, obj+0x450 timer).
Then look-ahead ray `FUN_001a1a10(6.0, &local_1c, 0, dir@<eax>=*0x31fc50,
unit@<edi>)`. Recovery `FUN_001a74d0(unit,0)` fires iff `ray==-1` (no ground
found within 6.0) OR (`obj+0x20 vertical-vel <= 0.0` AND fall-clearance test:
`(iVar4+0x94)^2 <= obj+0x20^2 + 2*(local_8-local_14)*gravity`, gravity
0x0032512c=0.00357). 1a1a10 (ported=false) is the on-work-path sibling →
SIBLING-WALLED under stubs; collision-MISS path returns -1 (recovery fires);
HIT path needs non-stubbed collision_bsp_test_vector. 1a74d0 ported=null.

## Drive-first verdict
0e00 is the most deterministic to drive: single cdecl-ish caller, branch select
is a pure data comparison of one float (threshold) against two tag fields we can
read/bracket; both legs reachable by choosing threshold relative to fVar1/fVar2.
0680 = node-loop always runs in its real caller (counter<max) but output buffer
is global-scratch DISJOINT across oracle/candidate bases → product unwitnessable
(stays vacuous). 1e70 recovery branch needs live collision (HIT path undrivable
under stubs; MISS path drivable but only proves the ray==-1 leg).
