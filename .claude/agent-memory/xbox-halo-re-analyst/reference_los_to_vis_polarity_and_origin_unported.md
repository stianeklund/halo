---
name: los-to-vis-polarity-and-origin-unported
description: 2026-06-28 DEFINITIVE los->vis gate analysis. (1) POLARITY CORRECTION — prop+0x38 LOS value 0/1 = clear/partial (vision OK), 2/3/4 = OCCLUDED; the task/older notes labeling "los=3 = LOS ESTABLISHED" are INVERTED. los=3 = ray graded OCCLUDED, so vis=0 is FAITHFUL. (2) The visual writer 0x314f0 entry gate `if(param_5!=0 && param_5!=1) return 0` is faithful (must be, or working builds couldn't accumulate vis either). (3) The LOS origin is param_3 of 0x33440 = a caller stack buffer filled by UNPORTED prop_position_refresh 0x31df0 — NOT the datum_get actor record. ENTIRE los->vis path is unported; NO ported gate-flip exists on it.
metadata:
  type: reference
---

CONFIRMED from cachebeta.xbe Ghidra decompile + disasm (full image VAs).
Answers the focused question "why does los=3 not convert to vis>0 for a10 door grunts".

## HEADLINE: prop+0x38 (LOS) polarity is INVERTED in prior notes/task
ai_test_line_of_sight FUN_000416e0 (UNPORTED) returns (low 16 bits = uVar9):
  0 = clear LOS (param_2._3_1_ path)        -> vision allowed
  1 = partial/edge clear (LAB_000419fa)     -> vision allowed
  2 = occluded (dist*frac < thresh)          -> BLOCKED
  3 = occluded (occlusion-factor band)       -> BLOCKED
  4 = fully blocked (default fall-through)    -> BLOCKED
=> SMALLER value = CLEARER sight. los=3 means the raycast (FUN_0014df70) graded the
   ray as OCCLUDED. The live "los=3" reading is NOT "LOS established" — it is "ray blocked".
   These are a10 DOOR grunts; los=3 may be a LITERAL door occluding the ray — possibly not a
   defect on this path at all.

## The los->vis gate (visual writer FUN_000314f0, UNPORTED) is FAITHFUL
Entry: `if ((param_5 != 0) && (param_5 != 1)) return 0;`  param_5 = LOS result (prop+0x38).
So los in {2,3,4} -> early return 0 -> vis (prop+0x32) = 0. This MUST be faithful — if it were a
bug, working builds couldn't accumulate vis with los=3 either. Returns: param_5==0 -> 2/3 (highest);
param_5==1 -> 1; else 0. The view-cone (reads param_2+0x18c..0x1ac orientation) and distance gate
(`local_c*local_c <= local_10`) are DOWNSTREAM of this entry gate and DO NOT EXECUTE when los=3.

## Parent (state 0-3) state-reset = faithful consequence (matches captured parent +0x24=0/+0x30=0)
0x33440 parent branch (state 0-3, taken via `CMP[ESI+0x24],4 JL/JG 0x3399d`): after visual call
@0x33bec returns AX, at 0x33bf4 `CMP AX,0x2; JGE 0x33c08` (skip store if vis>=2); else stores
prop+0x30=AX, prop+0x32=AX, and **prop+0x24=0** (state reset). With los=3 -> vis=0 -> prop+0x30=0,
prop+0x32=0, prop+0x24=0. EXACTLY the captured broken parent state. Faithful.
(Orphan path state 4/5 uses origin=param_3=EBX, visual @0x33967, no AX<2 gate.)

## ABI / origin identity (the key trap that resolved the hunt)
FUN_00033440(param_1=actor handle [EBP+8], param_2=prop handle [EBP+0xc], param_3=ORIGIN struct
ptr [EBP+0x10]). EBX is the actor record ONLY in the prologue; at 0x3363c `MOV EBX,[EBP+0x10]`
EBX is RELOADED with param_3 and stays param_3 through both LOS pushes (0x33933 / 0x339ed) and the
orphan visual push. Decompiler confirms: FUN_000416e0(param_3, *(param_3+0x28), prop+0x104, ...)
and FUN_000314f0(param_1, param_3, prop+0x104, ...). So:
  - LOS ray ORIGIN = *(float*)(param_3+0x0); ray TARGET = prop+0x104 (datum_get(0x5ab23c)=prop rec).
  - param_3+0x28 = bsp cluster; param_3+0x2c = a position used in projection dot (line 112, separate
    from the +0x0 ray origin); param_3+0x18c.. = orientation basis (view-cone, downstream of gate).

## param_3 (origin) is a CALLER STACK BUFFER filled by UNPORTED 0x31df0 (dump_caller_regsetup)
All 3 callers of 0x33440 are ported=false (deactivated) and push param_3 = &local, filled by
FUN_00031df0 (kb name prop_position_refresh, UNPORTED, no src impl) immediately before:
  - 0x34970 @0x34b3a: push EAX = LEA[EBP-0x44]; preceded by call 0x31df0(...,&[EBP-0x44],...)
  - 0x355f0 @0x35b7e: push ECX = LEA[EBP-0xBC]; preceded by call 0x31df0(...,&[EBP-0xBC],...)
  - 0x64b40 @0x64c90: push ECX = LEA[EBP-0x4C]; preceded by call 0x31df0(ebx,edi,&[EBP-0x4C],0)
=> The eye/origin transform is NOT read from a ported-written actor field; it is computed by
   UNPORTED prop_position_refresh into a stack buffer. NO ported function lies on the los->vis path.

## VERDICT for the ported-culprit hunt: NEGATIVE (honest)
Every node on los->vis is unported: origin producer 0x31df0, raycast 0x416e0+0x14df70, perception
writer 0x314f0 + status refresh 0x33440, sound 0x31850, knowledge 0x2f380(ported but faithful),
all 3 dispatchers (0x34970/0x355f0/0x64b40, ported=false). FUN_0003dc20 actor_input_update (PORTED,
actors.c:6938) writes actor+0x18c looking-vector + up/side basis — VERIFIED byte-FAITHFUL vs disasm
0x3e2e2-0x3e37a (cross-product order matches) — but is (a) the wrong record (caller passes a stack
buffer, not the datum_get actor record) and (b) downstream of the los=3 early-return anyway.
CONCLUSION: los=3 -> vis=0 is faithful occlusion behavior. Root, if a real regression, is an INPUT
divergence in the unported origin/target feed (0x31df0 origin OR prop+0x104 target, bulk-written by
unported 0x647c0 family) OR a genuine door. DECISIVE TEST (unchanged): live float-diff of origin
(param_3+0x0) and target (prop+0x104) into FUN_0014df70 at the 0x416e0 call site, patched vs pristine.
Supersedes the "los=3=established" framing in [[los-input-field-audit-33440]] and
[[pertick-promotion-gate-355f0-33440]] (their mechanism is otherwise correct).
