---
name: decals-158df0-hcf-vs-systemexit-reject
description: FUN_00158df0 (rasterizer scene render begin) 89.4% REJECT — 4 assert terminals lifted as halt_and_catch_fire() where binary=system_exit(-1); 2nd instance of the decals hcf anti-pattern
metadata:
  type: project
---

FUN_00158df0 @0x158df0 (rasterizer_decals.obj, "rasterizer scene render begin"), 89.4% VC71 = **REJECT**. Second confirmed instance of the [[project_decals_15c680_hcf_vs_systemexit_reject]] anti-pattern in the SAME object.

Body is byte-faithful 1:1 everywhere I audited: memcpy 0x96 dwords=0x258 to mirror 0x5a5bc0; same-target OR EDI,-1 / dual CMP-to-0xffff logic; 5-call same_target==0 block; render-begin chain with deferred ADD ESP,0x10 (4 dword args, 16f880 pushes none); clear-color CMP word[0x3256bc],1; FUN_00158140(param[0],0,color,(byte+5==0),1) arg order EXACT; z_near FCOMP==0.0 equality idiom; rasterizer_set_frustum_z(-1,-1); neg/sbb/add fillmode ternary 0x1b01/0x1b02; D3DDevice_SetRenderState_FillMode __stdcall. ALL correct.

THE BUG (×4): every assert terminal in the binary is `PUSH -1` (or `PUSH EDI` where EDI still = -1 from the earlier OR EDI,0xffffffff at 0x158e54) then `CALL 0x8e2f0` = `system_exit(-1)` (void __noreturn system_exit(int)). Lift wrote `halt_and_catch_fire()` = CALL 0x1029a0 — WRONG CALLEE (bluescreen renderer, void(void)) + DROPPED -1 arg. Sites: 0x158e13/e15, 0x158e3c/e3e, 0x158f56/f57 (shared by unsupported-target + z_near asserts).

TELL: the code comments rationalize it as `/* reloc FUN_001029a0, push -1 not emittable (void decl) */`. This rationale is FALSE — system_exit(-1) is an ordinary call statement, wholly independent of the enclosing fn's void return. Treat "push -1 not emittable (void decl)" as a red-flag string.

Fix = replace 4 hcf() with system_exit(-1); per prior precedent this also clears the score >90.

Secondary defect: kb.json source_path = src/halo/rasterizer/xbox/rasterizer_xbox.c but impl actually lives in rasterizer_xbox_decals.c:3406. Wrong source_path will mislead future VC71/cleanup runs that compile source_path. (Sibling decals fns build fine — object is rasterizer_decals.obj.)

KEY reaffirmed: grep `CALL 0x8e2f0` / grep `halt_and_catch_fire` on EVERY sub-98 rasterizer_decals lift.
