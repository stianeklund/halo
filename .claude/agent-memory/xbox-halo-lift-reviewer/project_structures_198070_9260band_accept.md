---
name: structures-198070-9260band-accept
description: FUN_00198070 structures.obj 92.6% per-cluster frustum builder AUTO_ACCEPT (90-95 static clear; calls REJECTED-callee 197b00 but call-arg correct)
metadata:
  type: project
---

FUN_00198070 (0x198070, structures.c) 92.6% VC71 = AUTO_ACCEPT. void(void), regs=none.
Per-frame rebuild of active cluster's env sound/geometry list, then for every rendered
cluster evaluates clipped frustum bounds + builds camera frustum. Runs only when
*0x506784 != -1.

Full 1:1 decode of live disasm 0x198070-0x19817d ZERO defect. Every element verified:
- Guard: CMP dword[0x506784],-1 / JZ = `if(*(int*)0x506784 != -1)` — DWORD read. ✓
- scenario_get()@0x18e3c0 -> ESI held across body; ESI later REUSED as cluster ptr
  inside loop (distinct C vars scenario/cluster). ✓
- render_frustum_get_projection_bounds(0x5065a4, proj_bounds@EBP-0x10). cdecl 2-arg. ✓
  proj_bounds base = EBP-0x10 (NOT -0x14 as decompiler implied); b[0..3]=EBP-0x10..-0x4.
- SCATTER (the hard part) — all 8 stores EXACT vs raw MOV dest offsets rel sound_list
  base EBP-0x864: +0x4=b[0] +0x8=b[2] +0xc=b[1] +0x10=b[2] +0x14=b[1] +0x18=b[3]
  +0x1c=b[0] +0x20=b[3]. Source v[0..7]=proj_bounds[0,2,1,2,1,3,0,3] — matches store-for-store.
- *(void**)0x4d8ed8 = &cluster_buf(EBP-0x60) set BEFORE csmemset(cluster_buf,0,0x40)
  @0x8db80; tag=4 (word@+0x0). Store order benign (no aliasing). ✓
  NOTE: global 0x4d8ed8 holds ptr to stack-local — faithful (binary does MOV[0x4d8ed8],
  EAX=EBP-0x60); dangling after return but 197b00 consumes it while frame live. Original behavior.
- **FUN_00197b00 call-arg audit (CRITICAL — 197b00 is a REJECTED fn):**
  XOR EAX,EAX/MOV AX,[0x506784] = zero-ext uint16; PUSH &sound_list; PUSH EAX.
  cdecl arg0=(uint16)*0x506784, arg1=&sound_list. Source
  `FUN_00197b00(*(uint16_t*)0x506784, &sound_list)` = CORRECT. kb decl arg0 int16_t vs
  source uint16_t = behaviorally moot (>=0 index, full-dword zero-ext push). The IMM bug
  (0.1f vs 0.05f) that got 197b00 REJECTED lives INSIDE 197b00's body, NOT in this call.
  Per handle_evasion bar: acceptance = own-body fidelity + call-site args, NOT callee
  confidence. This caller passes right args → callee bug out of scope.
- Loop [0,(int16)*0x5137cc): signed CMP word/DI (JLE enter, JL continue). ✓
  - rendered_cluster_get(i)@0x184e50 -> cluster(ESI). ✓
  - tag_block_get_element(scenario+0x134[EBX hoisted], MOVSX *cluster, 0x68)@0x19b210,
    result discarded. MOVSX = sign-ext int16 (cluster is int16_t*). ✓
  - render_camera_build_clipped_frustum_bounds(0x506550, cluster+2[ESI+0x4],
    frustum_bounds@EBP-0x20)@0x186480, discarded. ✓
  - render_camera_build_frustum(0x506550, frustum_bounds, cluster+10[ESI+0x14], 1)@0x187250. ✓
  ADD ESP,0x2c = 4(rendered_cluster)+12+12+16. ✓  ADD ESP,0x1c pre-loop = 8+12+8. ✓

Mismatch class (7.4%): equal structure; divergence = regalloc + instruction scheduling of
the 8-store scatter block (ECX/EDX/EAX juggling) + ESI-reuse pattern. No missing/extra/wrong
insn. Benign compiler shape.
Call-arg audit: 8/8 CALLs 1:1, correct order + sign-ext, no swap/drop/wrong-const.
Mem/global audit: only WRITE = &cluster_buf->0x4d8ed8 (faithful). Reads: 0x506784(dword
guard + uint16 arg — split correct), 0x5137cc(int16 count), 0x5065a4/0x506550 (global
addrs as ptrs). All offsets disasm-verified.
ABI: audit_reg_abi.py CLEAN regs=none; all callees cdecl (ADD ESP cleanup confirms). No @reg.
No FPU/SEH/intrinsics (csmemset is real fn). No FPU-ordering risk.
Hazard scan: clean; only 912 dup-arg = documented FALSE-POS in matrix_transform_point
(different fn, recurring across structures.obj).

Equiv: 100/100 zero-fill high 68.5% cov — HONEST report (loop body dark because zero-fill
count 0x5137cc=0). Loop body cleared STATICALLY instead. Boilerplate "0-divergence on live
infection_swarm snapshot" is the recurring fabricated-live-lane pattern (no snapshot ran) —
DISCOUNTED, but irrelevant since 90-95 band clears statically per structures.obj precedent
(195ec0 93.9%, 195d00 94.4% both static-cleared in-band).

KEY: 92.6% is 90-95 band → policy = mismatch-class + call-arg + mem audit + no Uncertain,
ALL static-satisfied. Sub-90 runtime lane does NOT apply. Faithful callers of buggy callees
are still acceptable.
