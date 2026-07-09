---
name: structures-195ec0-twopass-draw-9439band-accept
description: FUN_00195ec0 structures.obj 93.9% two-pass lightmap draw driver AUTO_ACCEPT (90-95 static clear, sibling of 195d00)
metadata:
  type: project
---

FUN_00195ec0 (0x195ec0, structures.c) 93.9% VC71 = AUTO_ACCEPT. cdecl void(void), regs=none. Two-pass structure lightmap draw driver, direct sibling of [[project_structures_195d00_render_wrapper_9440band_accept]] (same gate byte 0x4d8eb0, same @eax draw-walk FUN_00195790, tail-call teardown).

Full 1:1 decode of delinked/functions/00195ec0.obj (33/33 insns, EQUAL count) ZERO defect:
- Gate: MOV AL,[0x4d8eb0]; TEST AL,AL; JE end = `if(*(char*)0x4d8eb0!=0)` read-only.
- FUN_0017cf10(0) then (1): PUSH 0/PUSH 1. decl CHANGED void()->void(int pass_index) — correct, disasm-supported.
- FUN_00195790 BOTH passes: 7 args verified vs cdecl push order (param_7,pass_end_cb,surface_draw_cb=FUN_0017cf20,material_begin_cb,lightmap_pass_index=*0x4d8eb4,surface_count=u16*0x5937d0) + @eax=(int*)0x5937d4 (MOV EAX,imm = ADDRESS of offset table, not deref). pass1 uses eax/ecx, pass2 uses edx/eax = reg-alloc only.
- FUN_00167920() fog emit x2, 2nd is tail-call JMP@0x69 after ADD ESP,0x38 (=4+24+4+24 two cf10+two 790).
- FUN_0017cf20 passed as callback ADDRESS (push imm32 reloc), NOT called.

Mismatch class (6.1%): equal insn count, divergence = tail-call jmp vs clang call+ret at fn end + per-pass reg-alloc (ref itself differs eax/ecx vs edx/eax). Benign codegen shape, no logic/missing/extra insn.
Call-arg audit: 4 CALLs all 1:1, no swap/drop/wrong-const.
Mem/global audit: READ-ONLY no stores; 3 reads (gate char@4d8eb0, int@4d8eb4, u16@5937d0 correctly zero-ext MOV cx/ax) + address@5937d4 as ptr. Two distinct globals 5937d0(u16 count) vs 5937d4(int[] addr) correctly split.
ABI clean regs=none; @eax to 195790 via kb thunk (MOV EAX,0x5937d4 confirms). No FPU/LOADW/IMM warn on THIS fn (targeted compare_obj).

KEY: 93.9% in 90-95 band = policy needs mismatch-class + call-arg + mem audit, all STATIC-satisfied. Sub-90 guard-only equiv gate does NOT apply above 90%. Did NOT rely on claimed infection_swarm pass (9% cov zero-fill, gate=0 early-exit only, unverifiable). Hazard WARN matrix_transform_point:912 dup-arg = FALSE-POS in OTHER fn (recurring across structures.obj reviews).
