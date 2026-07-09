---
name: structures-196190-shadow-render-9340band-accept
description: FUN_00196190 structures.obj 93.4% render_structure_shadows = AUTO_ACCEPT (90-95 static; full disasm-verified sibling of 195ec0/195d00/198070)
metadata:
  type: project
---

FUN_00196190 (0x196190, structures.c) 93.4% (84/83 insns) = AUTO_ACCEPT. render_structure_shadows: sibling of shadow-render cluster (195ec0/195d00/198070 already accepted). cdecl void(center,radius_x4,bounds6,count,planes6), regs=none.

Full-body 1:1 vs delinked disasm, ZERO logic defect:
- Frame buffer[0x4000] via _chkstk(0x4000); LEA [EBP-0x4000] consistent across all 3 buffer uses.
- 3 profiler byte-reads READ-ONLY at correct width (MOV AL → `*(char*)`): master 0x449ef1, outer-scope 0x32b178, draw-scope 0x32b770. NO global writes.
- 6 call sites all arg-verified: profile_enter_private((void*)0x32b170/0x32b768); FUN_00197e90(9 args) short-return→MOVSX ESI; FUN_001956d0(buffer,0) int→EDI, CMP EDI,-1 skip; FUN_00195790 @eax=buffer + 6 stack (FUN_0017ccf0 lands in surface_draw_cb 5th slot); rasterizer_widget_set_tint_factor@0x17c9a0(pass_index); profile_exit_private@0x8fac0. @eax immutable annotation on 195790 preserved (buffer passed positionally). NO FPU (radius passed as dword pass-through → no FPU-WARN). NO IMM-WARN (file's only IMM-WARN is FUN_00191e90, different fn — 197b00-class IMM bug NOT present).

KEY disasm-reading lesson (ADD ESP accounting): PUSH ESI / PUSH EDI at 0x1961c5-c6 are scheduled IN THE MIDDLE of 197e90's arg pushes but are callee-saved REGISTER SAVES, not args — proven by ADD ESP,0x2c cleaning exactly 11 dwords (9 args of 197e90 + 2 args of 1956d0), while POP EDI/POP ESI at epilogue restore the 2 saves. Do NOT miscount them as 2 extra args.

Mismatch class = benign compiler control-flow shape in the profiler-EXIT epilogue only: original shares one MOV AL,[0x449ef1] load + JZ 0x196280 shortcut; lift emits two nested `&&` guards → 1 extra re-read/branch. Behaviorally identical (case-analysis verified; both fire profile_exit only when both flags set). Corroborated: equiv 100/100 zero-fill high-conf 60.6% cov + 0 stub-arg mismatches + 0-diverge on infection_swarm live snapshot.
