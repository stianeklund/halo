---
name: los-input-field-audit-33440
description: 2026-06-28 INPUT-field audit of the unported per-tick perception chain (0x33440 prop_status_refresh -> 0x416e0 LOS / 0x314f0 visual / 0x31850 sound). Enumerates every actor/prop field the chain READS and each field's WRITER+obj+ported. Only TWO ported ai.obj writers of any LOS input exist (0x40010/0x40280, both write prop+0x60 friendship, both FAITHFUL). LOS target/origin are bulk-written/internal -> ai.obj bisect cannot have altered 0x416e0 (it is UNPORTED).
metadata:
  type: reference
---

CONFIRMED from cachebeta.xbe disasm (XBE base 0x10000, sec0 VA 0x12000; full VAs).
Complements [[pertick-promotion-gate-355f0-33440]] / [[stageB-rate-actrtag-static-table]] /
[[a10-ai-obj-pertick-writeset-clean]]: the prior write-set audit swept OUTPUT offsets
{0x38,0x66,0x63,0x26,0x6a,0x6e,0x268,0x9e,0x4e,0x270,0x610,0x60c,0x9c}; this audits the INPUT
offsets the perception chain READS, which were never tested against ai.obj.

## The two LOS calls inside 0x33440 (prop_status_refresh, UNPORTED). ESI=prop(*0x5ab23c), EBX=actor(0x6325a4).
1st LOS @0x33934 (visual, prop type NOT in [4,5]): cdecl 8-arg, add esp,0x20. Args (arg0..7):
  arg0=EBX actor record; arg1=word[EBX+0x28] actor+0x28; arg2=&prop+0x104 (target, float*);
  arg3=word[prop+0x100]; arg4=(prop+0x12e!=0 && prop+0x60!=0)?2:0; arg5=0; arg6=dword[prop+0x110];
  arg7=(dword[actor+0x158]!=-1). Return AX -> stored prop+0x38 @0x33944 (the LOS-RESULT field).
2nd LOS @0x339ee (type 4/5 contact path): same shape; arg2=&prop+0x104, arg6=prop+0x110,
  arg7=actor+0x158!=-1, arg3=prop+0x100, arg1=actor+0x28. Return -> prop+0x38 @0x339f9.
Then visual FUN_000314f0 @0x33967 -> prop+0x32; max merged to prop+0x30 @0x3397d.
Sound FUN_00031850 @0x33e15 (gated prop+0x66==1||2 ->3 else call) -> prop+0x34.
Extra: prop+0x66==0 ->prop+0x36=3 else 0.

## INPUT-FIELD -> WRITER table (writer obj by narrowest objects.csv range; ported from kb.json)
| input | reader | writer (addr/name) | obj | ported |
|---|---|---|---|---|
| prop+0x104/108/10c (LOS target pos, float*) | 0x416e0 via 0x33440/0x14f8b | NO direct/lea store in entire .text; bulk-written by unported prop create/update (0x647c0 family) | actor_perception/unported | false |
| prop+0x100 (target idx/handle) | 0x33440 arg3 | only store 0x40f44 in 0x40700 | ai.obj | **false (0x40700 unported)** |
| prop+0x110 (vehicle handle) | 0x33440 arg6 | 0x407be `[esi+0x110]=-1` in 0x40700; ported reader-only ai.c/actions.c/actor_perception.c | ai.obj | **false (0x40700 unported)** |
| prop+0x12e (look-active byte) | 0x33440 arg4 gate | no AI-cluster store (unported refresh sets prop+0x120@0x339fd) | actor_perception/unported | false |
| prop+0x60 (friendship/hostility byte) | 0x33440 arg4 gate | **0x40010 game_allegiance_apply_change @0x400ed/0x40210; 0x40280 ai_update_team_status @0x402f8** | **ai.obj** | **TRUE** |
| prop+0x38 (LOS result) | written by 0x33440; read by visual via prop+0x32 chain & 0x33dee | 0x33440(unported)@0x33944/0x339f9; ALSO 0x40e6d in 0x40700 | actor_perception/ai.obj | 0x40700 unported |
| prop+0x30 (perception max) | Stage A/B gate (0x355f0) | 0x33440(unported)@0x3397d; ALSO 0x40e6a in 0x40700 | unported | false |
| actor+0x28 (bsp/cluster idx) | 0x33440 arg1 | no AI-cluster store (object/bsp subsystem, outside AI) | objects/bsp | likely false |
| actor+0x158 (a handle, !=-1 sets arg7) | 0x33440 arg7 | no AI-cluster store found | outside AI | unknown |

## SUMMARY
(A) Ported ai.obj writers of a LOS input: EXACTLY TWO, both write ONLY prop+0x60 (friendship):
    0x40010 game_allegiance_apply_change (event-driven, allegiance change) and 0x40280
    ai_update_team_status (PER-TICK). 0x40280 disasm = its ai.c source byte-faithful
    ([esi+0x12]=team,[esi+0x60]=0xa7a30(actorteam,unitteam),[esi+0x61]=0xa7a90,[esi+0xa4]=0x2fc20,
    [esi+0x50]=fstp 0x2fd10). prop+0x60 for an ENEMY (player) = is_friendly = 0 -> LOS arg4=0
    (correct enemy path). No divergence mechanism via prop+0x60 unless allegiance tables differ.
(B) EVERY other LOS input is written by UNPORTED code: the LOS TARGET prop+0x104 (bulk/unported
    create), prop+0x100/0x110/0x38/0x30 (unported 0x40700 + unported 0x33440), prop+0x12e
    (unported), and the LOS ORIGIN is computed INSIDE 0x416e0 from arg0=actor record (the grunt
    eye/head transform = ported biped-aim TU OUTSIDE ai.obj, documented bug history). 0x416e0
    itself is ported=null -> UNPORTED -> identical in BOTH bisect arms -> an ai.obj-only bisect
    CANNOT have changed LOS behavior. So bisect "real ai.obj" survivor reduces to: 0x3f5f0
    handle/gate (faithful) + prop+0x60 via 0x40280 (faithful). => bisect most likely non-live OR
    coarse; PIVOT to actor_looking.obj / biped-aim origin producer is the strongest path.
(C) Live XBDM reads on a6 (slot 6) + parent prop (orphan+0xc) at aw>=1:
    - prop+0x32 (visual) vs prop+0x34 (sound) vs prop+0x36 (extra): which channel is dead. If
      +0x32==0 broken but >0 stock => VISUAL/LOS is the discriminator (expected; +0x66=-1 both
      builds already kills +0x34/+0x36 as discriminators).
    - prop+0x38 (LOS result): is the ray succeeding at all (0=blocked/miss).
    - prop+0x104..0x110 (target pos+handle): if = player and sane, failure is on ORIGIN side.
    - DECISIVE (unchanged from [[reference_a10_prop_deletion_path_faithful]] line 51): live-diff
      the float origin/direction passed to 0x14df70 at the 0x416e0 call site, patched vs pristine.
CORRECTION to older notes: the chain's LOS-result field is prop+0x38 (NOT actor+0x9e; +0x9e is a
scripted-look countdown in actor_looking.c ~1189/1234). "actor+0x9e LOS=0 both builds" was mislabeled.
