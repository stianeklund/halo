---
name: structures-195d00-render-wrapper-9440band-accept
description: FUN_00195d00 structures.obj 94.4% = AUTO_ACCEPT (90-95 static clear; render-orchestration wrapper, tail-call teardown shape, @eax draw-walk, dark-body equiv is NOT gate-required in-band)
metadata:
  type: project
---

FUN_00195d00 @0x195d00 structures.c, void(void), 94.4% VC71 = AUTO_ACCEPT via full STATIC clear in the 90-95 band (runtime NOT gate-required in-band).

19-insn sibling of FUN_00195d40/FUN_00195b10 render-orchestration wrappers. Full 1:1 decode zero-defect:
- Gate: MOV AL,[0x4d8eb0];TEST AL,AL;JZ 0x195d3b => `if(*(char*)0x4d8eb0!=0)`. Read-only.
- Brackets scope enter/exit (FUN_0017ce40/FUN_0017ce60, both void(void)) around draw walk FUN_00195790.
- FUN_00195790 has `@<eax>` on arg1 (surface_material_offsets) in kb decl. C passes (int*)0x5937d4 as arg1 => MOV EAX,0x5937d4 (ADDRESS not deref) ✓ thunk-handled. Reg-baseline extract_reg_args --check = 616 OK, 0 drift.
- Full 7-arg call audit 1:1 vs PUSH order (first push=last C arg): param_7=0, pass_end_cb=0, surface_draw_cb=0x17ce50(=&FUN_0017ce50 by address), material_begin_cb=0, lightmap_pass_index=*(int*)0x4d8eb4(PUSH EAX loaded @0x195d0e), surface_count=*(u16*)0x5937d0(XOR ECX;MOV CX=zero-ext PUSH ECX). ADD ESP,0x18=6 stack dwords (the @eax arg not on stack) ✓.
- Two DISTINCT globals: uint16 count @0x5937d0 vs int[] offsets @0x5937d4 (adjacent, not confused) ✓.
- Thunk decode confirmed: 0x17ce40 E9->0x160bd0; 0x17ce60 E9->0x160be0; 0x17ce50=PUSH EBP;MOV EBP,ESP;POP EBP;JMP 0x160690 (real callback entry, address-taken).

MISMATCH CLASS (the ~5.6%): teardown reached via tail-call `JMP 0x0017ce60` in original vs our C call+ret + scope/epilogue frame shape. Pure control-flow/codegen shape, no logic delta.

KEY band discriminator: 94.4% is ABOVE the sub-90 guard-only-equiv NEEDS_RUNTIME zone. The equiv here was pass1 zero-fill weak (16.7% cov, gate byte 0x4d8eb0=0 so only no-op early-return exercised, draw body DARK) — but 90-95 policy requires mismatch-class + call-arg audit + memory/global audit + no Uncertain, NOT runtime. All satisfied statically. Did NOT rely on the acceptance path's claimed "infection_swarm live-snapshot 0-divergence pass" (per bsp3d 146be0 pattern that boilerplate can narrate a run that never happened) — static clear alone carries it.

Hazard scan: sole WARN is matrix_transform_point dup-arg @structures.c:912 = KNOWN FALSE-POSITIVE faithful in-place, DIFFERENT function outside this lift. Non-blocking.
