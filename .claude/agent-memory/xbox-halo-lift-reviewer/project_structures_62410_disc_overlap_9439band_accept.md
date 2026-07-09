---
name: structures-62410-disc-overlap-9439band-accept
description: FUN_00062410 structures.obj 93.9% AUTO_ACCEPT — read-only obstacle-disc overlap query, 90-95 static-clearable band
metadata:
  type: project
---

FUN_00062410 @ 0x62410 (structures.obj, path_obstacles disc query) — 93.9% VC71 = AUTO_ACCEPT via 90-95% static bar.

cdecl short(void *obstacles, short disc_index_skip, float *position_xy, float radius). Returns first disc index whose inflated circle overlaps the 2D query point, else -1.

Full 1:1 body decode, ZERO defect:
- disc_count = *(short*)(base+2), RE-READ each iteration (matches MOV AX,[EDI+2] at loop tail) — faithful, not a cache bug.
- disc addr = base + 8 + i*0x18 (MOVSX SI; LEA EAX*3; LEA EDI+EAX*8+8) — stride 0x18, disc fields +8=x,+0xc=y,+0x10=radius.
- sum_radius = disc[+0x10] + radius; dx = disc[+8]-pos[0]; dy = disc[+0xc]-pos[1].
- FCOMPP polarity VERIFIED: sum_radius² vs dist²; C0=1 iff sum_radius²<dist²; TEST AH,0x1;JZ → return i when NOT(<), i.e. sum_radius²>=dist². Source `if(!(sr*sr < dy*dy+dx*dx)) return i;` matches.
- distance term dy²+dx² vs disasm dx²+dy² = commutative single IEEE add, byte-identical → benign.

Call audit: ONLY assert-path calls — display_assert(msg@0x25e930, file@0x25e990="c:\halo\source\ai\path.h", 0x18c, 1) @0x8d9f0 + system_exit(-1) @0x8e2f0. NOT hcf (per [[feedback_system_exit_vs_hcf]] doctrine). PUSH order + kb decls verified 1:1.

Memory/global audit: READ-ONLY. No writes anywhere, no global side effects, return in AX only. All offsets disasm-verified.

ABI: audit_reg_abi regs=none no hazards. check_arg_counts: declared_stack=4, sole caller 0x61010 push=4 cleanup=4 high_conf OK.

Clean on ALL census detectors: no FPU-WARN, no LOADW-WARN, no IMM-WARN (those belong to 0x61df0/0x99070/0x196330). Hazard dup-arg @structures.c:980 = matrix_transform_point, OUT OF SCOPE (our body is lines 366-434).

Equiv: zero-fill 100/100, 0 diverged, 0 stub-arg mismatch, moderate, 58.1% cov (uncovered = assert dead-path for valid input, decoded 1:1 anyway).

KEY: cleanest 90-95 clear yet — read-only + no reg args + assert-only calls + zero warnings. Same bar as [[project_actions_handle_evasion_9698band_bar]] and [[project_structures_195ec0_twopass_draw_9439band_accept]]. Static audit fully clears; equiv is bonus not gate at >=90.
