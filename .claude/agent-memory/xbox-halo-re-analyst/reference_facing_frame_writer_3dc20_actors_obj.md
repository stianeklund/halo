---
name: facing-frame-writer-3dc20-actors-obj
description: 2026-06-29 DISASM-CONFIRMED writer of the a6 actor facing/aim frame (actor record +0x174..0x1ac) = FUN_0003dc20 actor_input_update in actors.obj; copies aiming<-biped+0x1ec, looking<-biped+0x210, derives up/right; toggle target for the a10 "won't fire" wrong-facing oracle is actors.obj (FUN_0003dc20 ported=false), 2nd pick = biped aim producers (units/biped)
metadata:
  type: reference
---

# a10 grunt "won't fire" — facing-frame writer localization (disasm-anchored)

Bug: default.xbe a6 grunt facing frame at ACTOR record +0x180..0x1ac is rotated
~31.6deg vs cachebeta (faithful) -> player falls outside vision cone -> vis
(prop+0x32)=0 -> prop never promotes prop+0x24 5->3 -> stays passive.
ACTOR record = 0x724-stride pool, base = *(*0x6325A4 + 0x34), element = base+6*0x724.

## WRITER = FUN_0003dc20 ("actor_input_update"), object = actors.obj (src/halo/ai/actors.c), ported=true
Disasm cachebeta.xbe (VMA=file_offset+0x10000), ESI = actor record:
- Prologue 0x3dc26-0x3dc38: `mov eax,[0x6325a4]`(actor_data); `mov ebx,[ebp+8]`(handle);
  `push ebx; push eax; call 0x119320`(datum_get); `mov esi,eax` => ESI = 0x724 actor rec.
  NOT object_data, NOT a 0x44 look-spec. Base-pool trap cleared.
- biped = object_get_and_verify_type(actor+0x18, 3) (type 3 = biped); EBX reloaded to biped
  from [ebp-8] before the orientation copies.
- Field map (assert strings in actors.c are AUTHORITATIVE):
  - actor+0x174..0x17c = input.facing_vector  (units_debug_get_closest_unit / default fwd 0x31fc3c; .z forced 0 on foot)
  - actor+0x180..0x188 = input.aiming_vector  <- biped+0x1ec  (store 0x3e2c6-0x3e2df, [esi+0x180])
  - actor+0x18c..0x194 = input.looking_vector <- biped+0x210  (store 0x3e2e2-0x3e2fd, [esi+0x18c])
  - actor+0x198..0x1a0 = up    = cross(looking, world_up[0x31fc44]); normalize3d 0x13010  (store 0x3e30a-0x3e337)
  - actor+0x1a4..0x1ac = right = cross(looking, up)                                         (store 0x3e341-0x3e37a)
- The WHOLE diverged frame is written here. up/right are DERIVED from looking(biped+0x210),
  so a wrong looking/aiming vector rotates the entire frame. Source actors.c:7271-7335 matches
  disasm exactly (was lifted from raw disasm; Ghidra DECOMPILE shuffles operands and is misleading).

## INPUT SOURCE = biped object (controlled unit aim)
- unit+0x1ec = aiming_vector, unit+0x210 = looking_vector (units.c:2094/2111 field names confirmed).
- units.c:5314-5322 seeds all 5 dir slots from object forward at unit INIT only.
- Per-tick aim/look is written by the biped aiming/euler update. Historically divergent aim
  producer flagged in memory: biped euler aim FUN_001b0630 / FUN_001b3690, biped update FUN_001a5300
  (units/biped obj). See [[biped-aim-up-euler-nan]] / project_biped_aim_up_euler_nan.

## VIS/CONE TEST = FUN_000314f0 ("actor_visibility_at_point"), actor_perception.obj, UNPORTED
- Sets prop+0x32 (visual channel). CASE A reads the BASIS actor+0x18c..0x1ac (the looking/up/right
  frame written by 0x3dc20) and does elevation fpatan + cone gate [_DAT_00256138.._0025613c].
  Reference direction = origin(param_2)->target(param_3) i.e. a6->player. See
  [[vis-writer-314f0-input-chain-faithful]].
- actor_looking.c:9030 reads actor+0x180 (aiming) for SNAP-TO-LOOK — a DIFFERENT consumer, not the
  vis test. Don't toggle on that read.

## TOGGLE TARGETS (per-function ported=false in kb.json)
1. BEST: FUN_0003dc20 (actors.obj). Reverts the facing-frame writer alone.
   - If facing reverts to (0.998,..) -> bug IS in 0x3dc20 (re-audit; prior "faithful" wrong).
   - If facing stays diverged -> 0x3dc20 faithfully copies an upstream-wrong biped aim -> go to #2.
2. SECOND: biped aim producers — FUN_001a5300 (biped update) / FUN_001b0630 / FUN_001b3690 (euler aim),
   units/biped obj. These write biped+0x1ec/+0x210 that 0x3dc20 copies.
