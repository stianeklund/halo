---
name: vis-gate-orphan-branch-33967-disasm
description: 2026-06-29 DISASM GROUND-TRUTH of the prop+0x32 (vis) write gate in prop_status_refresh 0x33440. CORRECTS the target the a10 hunt anchored on — at prop+0x24==5 the prop takes the ORPHAN branch (state 4/5), whose visual call is 0x33967 and store is 0x33971, NOT 0x33d45/0x33d6b (that store is the PARENT branch, state 0-3, unreachable for a state-5 prop). Full orphan gate + 0x314f0 geometry inputs + ported-writer table.
metadata:
  type: reference
---

CONFIRMED from raw capstone disasm of halo-patched/cachebeta.xbe (unported = byte-identical both builds).
VA = kb addr directly (repo convention; "base 0x10000" is the image-load base, kb addrs are absolute VAs).
.text raw=0x2000 va=0x12000. Base-pool: ESI=datum_get(prop_data *0x5ab23c, [ebp+0xc]) at 0x334b3 = THE PROP RECORD (verified). EBX = actor rec in prologue (datum_get *0x6325a4 @0x33457) but RELOADED to param_3 (origin buffer) at 0x3363c.

## Master branch on prop+0x24 (perception state), at 0x338c6
  mov ax,[esi+0x24]; cmp ax,4; jl 0x3399d; cmp ax,5; jg 0x3399d
  => state in {4,5}  -> fall through to 0x338e7 = ORPHAN branch
  => state {0,1,2,3} and {>=6} -> 0x3399d = PARENT branch
A player-tracking type-5 orphan/contact prop (prop+0x24==5) takes the ORPHAN branch.

## FIVE distinct `mov word [esi+0x32], ...` stores (decode which branch each is in):
  0x33971  ORPHAN visual store      (state 4/5; fed by 0x314f0 @0x33967) <- THE LIVE ONE for state5
  0x33994  ORPHAN zero store        (state 4/5; skip-gate diverted here)
  0x33bfe  PARENT visual store      (state 0-3; fed by 0x314f0 @0x33bec; gated CMP AX,2 JGE skip)
  0x33c20  PARENT zero store        (state 0-3; [esi+0x133]!=0 path)
  0x33cd4  PARENT literal-0 store   (state 0-3)
  0x33d6b  PARENT visual store      (state 0-3; fed by 0x314f0 @0x33d45) <- user anchored here, WRONG for state5

NOTE the user said "0x33d45 writes prop+0x32" — 0x33d45 is the CALL to 0x314f0; the store is 0x33d6b,
and BOTH are in the PARENT (state 0-3) branch, NOT reachable when prop+0x24==5.

## THE ORPHAN VIS GATE (the deliverable). prop+0x32 = 0x314f0 result IFF:
  (1) prop+0x24 in {4,5}                                          (master branch)
  (2) prop+0x133 == 0       0x33942 test cl,cl / 0x33948 jne 0x33986   (cl=[esi+0x133])
  (3) local_4 ([ebp-4]) == 0  0x3394d test cl / 0x3394f jne 0x33986
  (4) 0x314f0 @0x33967 returns nonzero (needs param_5(los) in {0,1} + cone/dist pass)
  Any of (2)/(3) nonzero -> 0x33986 ZERO store -> prop+0x32 = 0 (and +0x30/+0x34/+0x36 = 0).
  Note: prop+0x38 (LOS, [esi+0x38]) is written from 0x416e0 EAX at 0x33944 BEFORE the gates; the LOS arg
  passed to 0x314f0 is the FRESH 0x416e0 EAX (same value), not a re-read of +0x38.

## Gate-field sources (control-flow gates, distinct from geometry):
- prop+0x133: 0x334ef-0x33503 = bit10 of tag[edi+0x1b4] (edi=FUN_0013d680([esi+0x18],3) tag def);
    FORCED to 1 at 0x3351e if (prop+0x12e!=0 AND FUN_000fff80()==0 AND global byte [0x5ac9c6]!=0).
    Written in-function (unported). Live input to chase = prop+0x12e + global 0x5ac9c6.
- local_4 [ebp-4]: 0x334d0-0x334eb = 1 if (([ebp-0x18] obj && [obj+0x40]) || actor[ebx+0x6a]==1) else 0.
    [ebp-0x18] = datum_get(0x5ab270, actor[ebx+0x34]) or 0. So inputs = actor+0x34, that obj's +0x40, actor+0x6a.

## 0x314f0 actor_visibility_at_point geometry inputs (when reached):
- ENTRY GATE 0x314f6: cx=[ebp+0x18]=param_5=LOS; je continue / cmp 1 / jne 0x31836 RETURN 0.
    So los NOT in {0,1} -> returns 0 -> vis store gets 0 even though branch reached. (los polarity: 0/1=clear.)
- VIEW-CONE BASIS read from EDI = datum_get(actor pool *0x6325a4, param_1=[ebp+8]) = ACTOR RECORD,
    at [edi+0x18c..0x1ac] (0x316f8-0x31753). So actor+0x18c look frame IS on the cone path.
    (CORRECTION to a prior advisor note: cone basis is actor+0x18c via param_1 datum_get, NOT param_2.)
- DIRECTION: dir = param_3(target [ebp+0x10]) - param_2(origin [ebp+0xc]) at 0x315bc.
- ORPHAN call 0x33967 arg map: p1=actor[ebp+8], p2=ebx=param_3 origin buf, p3=edi=[esi+0x104] target,
    p4=[esi+0x120], p5=LOS(fresh 0x416e0 EAX), p6=1, p7=0, p8(knowledge)=literal 2.
    => for the ORPHAN call param_8 knowledge is the constant 2 (NOT FUN_0002f380); friendship prop+0x60
       only affects the 0x416e0 knowledge arg (eax=2 if prop+0x12e&&prop+0x60 else 0) @0x338e7-0x33901.

## ONE-SHOT BISECT for the a10 hunt (lead with this):
In default.xbe, for this state-5 prop: does execution reach 0x33967 (visual) or divert to 0x33986 (zero)?
  - Diverts to 0x33986 -> culprit is prop+0x133 OR local_4 (control gate) -> diff prop+0x12e, actor+0x6a,
    actor+0x34's obj+0x40, tag[+0x1b4] bit10.
  - Reaches 0x33967 but 0x314f0 returns 0 -> culprit is param_5(los) or cone geometry (actor+0x18c basis,
    param_2 origin from 0x31df0, param_3 target=prop+0x104).
PRECONDITION: confirm BOTH builds are at prop+0x24==5 at the compared tick (apples-to-apples). My older
oracle snapshot said cachebeta=state3 there; if so you'd be comparing default-orphan vs cachebeta-parent
(different code) and the real question is promotion lag, not the vis gate.

## PORTED-WRITER table (deliverable #3 — candidate culprits to diff/toggle):
- actor+0x18c..0x1ac look frame: WRITTEN BY PORTED FUN_0003dc20 actor_input_update (actors.c, ported=true).
    On 0x314f0 cone path. (Prior memory facing-frame-writer-3dc20 says byte-faithful vs disasm, but it is
    PORTED so it is a legitimate toggle candidate if the geometry branch is the live one.)
- prop+0x60 friendship: ported writers 0x40010 game_allegiance_apply_change + 0x40280 ai_update_team_status
    (both ported=true, both faithful per prior audit). Only feeds 0x416e0 knowledge arg in orphan branch.
- knowledge FUN_0002f380 (ported=true) — NOT used by the orphan 0x314f0 call (uses literal 2); used by parent.
- UNPORTED on the path: 0x33440 itself, 0x314f0, 0x416e0 (LOS raycast wrapper), 0x31df0 (origin buffer),
    prop+0x12e/+0x133/+0x12a writers in-function. raycast leaf 0x14df70 is ported=true (faithful per audit).
- prop+0x12e and actor+0x6a binary WRITERS not yet confirmed ported/unported — NEXT step to resolve which
    gate field a ported fn could be corrupting; src reads of these are plentiful but the STORE sites need a
    capstone write-scan, not the source greps (which are mostly reads).
