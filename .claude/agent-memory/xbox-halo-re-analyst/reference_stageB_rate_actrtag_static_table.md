---
name: stageB-rate-actrtag-static-table
description: 2026-06-28 Dispatcher 0x355f0 Stage-B perception-accumulator RATE source fully resolved — DAT_00255f30 is a STATIC int16 selector table (no writer); rate floats live in the 'actr' TAG at tag+0x6c/0x70/0x74 (NOT the runtime actor record); only Stage-B variables are prop+0x30 perception and knowledge_type
metadata:
  type: reference
---

CONFIRMED from 0x355f0 disassembly (cachebeta.xbe). Resolves the "actr+0x6c/0x70/0x74 are not
floats in our actor struct" confusion: those offsets are into the 'actr' TAG, not the actor record.

## Rate base = the 'actr' tag, not the runtime actor
Prologue: 0x3560d MOV ECX,[ESI+0x58] (actor+0x58 = actr tag handle); 0x35611 PUSH 0x61637472
('actr'); 0x35619 CALL 0x1ba140 (tag_get) -> EDI = actr tag def ptr; 0x35628 MOV [EBP-0x3c],EDI.
So [EBP-0x3c] (decompiler local_3c/local_40) = the 'actr' TAG definition pointer.

## Stage-B rate selection (disasm 0x35cf2..0x35d4b)
  0x35cf2 MOVSX ECX,word[ESI+0x30]   ; perception (prop+0x30, signed)
  0x35cf6 MOVSX EDI,DI; SHL EDI,2     ; knowledge_type*4
  0x35cfc ADD ECX,EDI                 ; index = perception + knowledge_type*4
  0x35d00 MOV AX,word[ECX*2+0x255f30] ; load int16 SELECTOR from static table
  0x35d0b CMP EBX,4; JA assert; 0x35d13 JMP [EBX*4 + 0x36834]   ; switch(selector)
    sel 0 -> [EBP-0x2c] = 0.0f
    sel 1 -> [EBP-0x2c] = *(float*)(actrTag+0x74)
    sel 2 -> [EBP-0x2c] = *(float*)(actrTag+0x70)
    sel 3 -> [EBP-0x2c] = *(float*)(actrTag+0x6c)
    sel 4 -> [EBP-0x2c] = 1.0f (0x3f800000)
  local_30 ([EBP-0x2c]) is the per-tick accumulator increment added to prop+0x2c.
  Promote when prop+0x2c >= _DAT_002533c8 (=1.0f) (FCOMP 0x35e60, JP guard for ordered <).

## DAT_00255f30 = STATIC const int16 selector table (NO ported writer)
Only xref is the DATA read at 0x35d00. 16 int16 entries; index = perception + knowledge_type*4
(row=knowledge_type 0..3, col=perception 0..3):
                percep0 percep1 percep2 percep3
  knowledge=0:     0       0       1       3
  knowledge=1:     0       1       2       3
  knowledge=2:     0       2       3       4
  knowledge=3:     0       3       4       4
selector->rate: 0=0.0, 1=actrTag+0x74, 2=actrTag+0x70, 3=actrTag+0x6c, 4=1.0.
=> perception==0 ALWAYS selects 0 => rate 0.0 => prop+0x2c never accumulates => NO promotion,
   regardless of knowledge_type. This is exactly the BROKEN "never promotes" mode and confirms
   prop+0x30 (perception) is THE Stage-B variable.

## Implication
The ONLY Stage-B runtime variables are prop+0x30 (perception, written by UNPORTED 0x33440) and
knowledge_type (from PORTED FUN_0002f380, verified faithful 2026-06-28). The rate TABLE is static
const (no writer) and the rate FLOATS come from static 'actr' tag data (loaded from disk, no ported
runtime writer). So Stage B itself cannot be mis-lifted into a stall — the stall must come from
prop+0x30 being wrong, i.e. upstream perception/LOS/unit_effect input divergence. Cross-ref
[[pertick-promotion-gate-355f0-33440]].

## Bonus: actor_compute_prop_target_weight 0x2fd10 (PORTED) current-target block FAITHFUL
Disasm 0x30016..0x3005a vs src/halo/ai/actor_perception.c:541-555: actor+0x270==-1 -> (prop+0x12e!=0
|| clump==actor+0x54) -> local_c=3.0f; else clump==actor+0x270 && actor+0x6e>=3 -> bonus_flag=1;
prop+0x134!=0 -> extra_flag=2. Our C uses `actor+0x6e > 2` which == `>= 3` (0x30041 CMP 0x3;JL signed)
-> arithmetically identical, faithful. All offsets/constants (3.0f=0x40400000, extra=2) match.
