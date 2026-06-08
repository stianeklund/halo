---
name: biped-update-dispatcher
description: FUN_001a6350 / FUN_001a5300 are the biped per-tick sub-step dispatchers that fix the calling conventions and shared object-field offsets for the 0x1a24xx-0x1a2bxx biped update cluster
metadata:
  type: reference
---

The biped per-tick update is split across small sub-step functions in
bipeds.obj (0x1a24xx-0x1a2bxx). Two dispatchers establish their ABI; read the
dispatcher disasm to resolve any of these callees' signatures rather than
guessing from the (void) placeholder decls.

## FUN_001a6350 (state-machine dispatcher)
Prologue: `MOV EDI,[EBP+0x8]` once -> EDI = unit_handle, reused for EVERY
sub-step call. Passes a `char* state` = `&local_4` (a 2-byte update-state
buffer at EBP-0x4). Call conventions it uses:
- FUN_001a2900 `(int unit_handle, char* state)` -- PUSHes EDI (cdecl, NOT @edi)
- FUN_001a2a60 `(int unit_handle@<edi>, char* state)` -- EDI live + 1 push
- FUN_001a2b10 `(int unit_handle@<edi>)` -- EDI live, pushed state UNUSED
- FUN_001a2440 `(int unit_handle@<edi>)` -- EDI live, NO stack args
- FUN_001a2800 `(int unit_handle@<eax>, const char*)` -- EAX + str (timing marks)
Also calls 0x1a2290 `@<edi>`, 0x1a1e70, 0x1a0b30, etc.

## FUN_001a5300 (movement/orientation dispatcher)
Prologue: `MOV ESI,[EBP+0x8]` -> ESI = unit_handle; `char* state` = `[EBP+0xc]`.
- FUN_001a2b90 `(int unit_handle@<eax>)` -- `MOV EAX,ESI; PUSH state; CALL` --
  EAX + 1 push; the pushed state is UNUSED by the callee.
- FUN_001a0e00 `(float, int@<eax>)`, FUN_001a2f40, FUN_001a0a40, FUN_001a0be0.

## Shared biped-object field offsets (object = object_get_and_verify_type(h,1))
- +0x18..0x20  linear velocity vec3 (x,y,z)
- +0x24..0x38  aim/forward vectors (0x24 aim, 0x30 facing dir, 0x38 timer)
- +0x1ec..0x1f4 desired-aim vec3 (paired with +0x18 in FUN_001a2b90 dot)
- +0x253       movement-state byte (switch values: walking 2-3, fast 4-7,
               crouch==2 at +0x257)
- +0x424       biped flags (bit0 = airborne); also addressed as object[0x109]
- +0x428/0x429 landing-frame counter / frame count
- +0x42a       slip/recovery mode (0,1,default) gating object+0x45b counter
- +0x45a       slip counter; +0x45b recovery counter; +0x45c airborne counter
- +0x460       landing animation index (NONE=0xffff)
- +0x1b8       control flags; +0x1c8 controlling-player datum handle
Tag: 'bipd' (0x62697064) via tag_get(0x62697064, *object); animation graph
'antr' (0x616e7472) via object+0x7c, element stride 0xb4 at antr+0x74.

## Keystone-ceiling note
These sub-steps take unit_handle in a register (@<edi>/@<eax>) AND call other
register-arg callees (FUN_001a0f10's index@<bx>, FUN_001a2800's @<eax>). Under
VC71 the register routing is thunk-mediated and invisible, so small wrappers
cap ~84-87% (residual = EDI/EAX prologue load + per-call BX/EAX materialization
vs stack pushes). See [[reg-arg-keystone-vc71-ceiling]]. Larger bodies with
real logic (FUN_001a2900 96.9%, FUN_001a2b90 96.2%, FUN_001a2440 88.4%) clear
88% because the keystone gap is a small fraction of the instruction count.
