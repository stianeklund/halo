---
name: project_a10_cdb0_LABfail_2238_squadhandle
description: a10 passive grunts — CLEAN no-shoot cdb0 trace localizes the bug to the case-3 squad-handle LAB_fail at actor_moving.c:2237/2238
metadata:
  type: project
---

# a10 grunt-passive — clean no-shoot cdb0 latch-LAST trace (2026-06-27)

Door squad enc `0xee6e0009` (a6/a10/a11), Impossible, **player crossed first door, never fired**.
Read `g_cdb0_trace` @ `0x77590c` (stride 20). Door slots 6/10/11:

```
slot 6 : hit=2 LATCHED msrc==3  exit=2 LAB_fail ret=0  dist=2.004 facing=403 f498=0
slot 10: hit=2 LATCHED msrc==3  exit=2 LAB_fail ret=0  dist=2.103 facing=403 f498=0
slot 11: hit=2 LATCHED msrc==3  exit=2 LAB_fail ret=0  dist=1.130 facing=403 f498=0
```

So the grunts **DO** get the scripted move order (`msrc==3`), cdb0 runs, hits **LAB_fail, returns 0** at 1.1–2.1u from the firing position → selector reverts msrc=3→1 (guard) within 1 frame → never advance. This is why the 0.25s sampler only ever sees `mode==1` (b4uaq4equ capture: 107/107 mode==1, atd=1, patt=0, pact=0). Toggle bisect already proved cdb0 (0x2cdb0) is the SOLE culprit (cdb0-alone ported=false, no-shoot → grunts move).

## Localization: the fail is at line 2238, NOT the on-foot facing check

cdb0 case-3 (squad order) reachable `goto LAB_fail` sites:
- **2238**: `if (*(uint*)(actor+0x34) == 0xffffffff) goto LAB_fail;`  ← BEFORE dest copy
- 2349: on-foot `if (f498==0 && facing(0x494)==-1) goto LAB_fail;`  ← AFTER line 2249 overwrites 0x494

Trace shows `facing=403` at LAB_fail. Line 2249 (`actor[0x494]=order[0x14]`) would have overwritten 0x494 if we'd reached the dest-copy; 403 is the STALE prior-frame facing. Therefore the bail happens at **2238, before 2249** — i.e. the **squad-handle `==0xffffffff` check is firing**. `FUN_0002a3a0` (LAB_fail body) only writes 0x4a8/0x484/0x4a0, so trace-time fields == branch-time fields.

CONTRADICTION: the door-squad probe shows `actor[0x34]==0xee6e0009` (stable 107/107), so 2237 should be FALSE. Two hypotheses:
1. **Codegen/lift bug** — our clang build mis-evaluates line 2237 (reads wrong reg/offset, sees -1). Consistent with toggle (ported=false original code = fix).
2. Handle genuinely transiently -1 at the cdb0 frame and the ORIGINAL survives it (different control flow) while our lift fails. Less likely (handle is stable).

## UPDATE 2026-06-27b — exit=2 is the REPATH path-build fail, NOT the handle check

Offline clang disasm (compiled actor_moving.c with shipped flags `-O3 -target i386-pc-win32 -march=pentium3 -mno-sse ...` → /tmp/am_clang.o; objdump cdb0=_actor_path_refresh) vs delinked original. Mapped ALL 7 jumps to the fail target (0x4635 = FUN_0002a3a0; CDB0_TRACE(2,0); return 0):
- 40ab case-3 handle==-1, 41c8 case-4 handle==-1, 420c/4218 case-4 order bounds, 41bd merge facing==-1, 4310 mounted path_3d==0, **4569 REPATH path-build fail (test bl,bl; je 4635)**.

The door grunt (handle valid, on-foot 0x99=0, facing=403≠-1) passes handle+on-foot checks. By elimination its exit=2 is **4569 = on-foot pathfinder pipeline returned path_found=0** (source 2475-2508 falls into LAB_fail at 2510). facing=403 is the REAL order[0x14] (not stale); patt(0x4a4) would be =1 (set at 4554/src 2484) — NOT yet confirmed live.

**cdb0 is FAITHFUL**: verified clang vs original for handle check (eax=actor, no clobber), case-3 dest copy, the shared merge@0x4195 (0x99 sense + on-foot facing!=-1), AND the full on-foot pipeline args:
- actor_path_input_new(actor_handle, local_nav=[ebp-0x78]) ✓
- paths_dispose(local_nav, actor[0x480]) ✓
- path_input_set_attractor(local_nav, &actor[0x2b0], actor[0x294], actor[0x28c], 10.0f@[esp+0x10]) ✓
- path_state_new(local_nav, large_buf, path_state): 0x44ec `mov ebx,esi`(ebx=local_nav) then push ebx → arg1=local_nav ✓
- FUN_0005e0d0(large_buf, &actor[0x488]=[ebp-0x14], actor[0x494], actor[0x498]) ✓; FUN_0005ff70(large_buf) ✓; path_state_build_path(large_buf, &actor[0x4a8]) ✓

TENSION: cdb0 control-flow + args all faithful, yet cdb0-alone ported=false toggle FIXES it. So divergence is NOT a control/arg bug in cdb0. Candidates left: (1) FPU/x87 value divergence feeding the pathfinder (dist / FUN_0005e0d0 destination), (2) `static char large_buf[0x1408c]` — source made it STATIC (.bss) but original kept it STACK-local ([EBP+0xfffebf14]); shared-vs-per-call could matter if a callee leaves state, (3) local_nav stack-aliasing in clang layout, (4) toggle evidence itself needs re-confirmation with discriminating fields. Box is POISONED (0xCA fill) — needs redeploy to read live.

## UPDATE 2026-06-27c — v2 discriminating trace DEPLOYED (advisor-prescribed)

Advisor: "repath by elimination" was circular (never measured actor[0x34] at the msrc=3 frame; facing=403 doesn't discriminate stale vs fresh). MUST measure the fail site, not infer. Also confirmed deployed build = **-O3 -DNDEBUG -fno-omit-frame-pointer -std=gnu90** (flags.make ground truth; CMAKE_BUILD_TYPE empty was a red herring) → offline /tmp/am_exact.o IS the right binary (byte-identical cdb0, 7 fail paths).

v2 trace struct (stride 28) adds: fail_site(off3, 1..8 per LAB_fail entry), p99(6), patt(7), pf_run(8)=FUN_0005ff70 ret, pf_build(9)=path_state_build_path ret (-2=not run), handle(24)=actor[0x34]. fail_site codes: 1=c3-handle==-1, 2=c4-handle==-1, 3=c4-oidx<0, 4=c4-oidx>=cnt, 5=default, 6=mounted-3d==0, 7=onfoot-facing==-1, **8=repath-pf==0**.

LIVE ADDRESSES (this build, deployed 2026-06-27 16:27): cdb0 impl entry **0x649b50** (redirect `push 0x649b50;ret` @ VA 0x2cdb0 — entry stable across rebuilds), g_cdb0_trace base **0x775910** stride 28, g_dbg_fail_site 0x77590c, g_dbg_pf_run 0x77590d, g_dbg_pf_build 0x77590e. Reader: `/tmp/read_trace2.py 0x775910`. Box freshly booted, trace empty.

DECISION TREE on next no-shoot read of door slots 6/10/11:
- handle==0xee6e0009 + patt=1 + site=8 + pf_run/pf_build tell which pathfinder call returned 0 → divergence is in pipeline VALUES (local_nav from actor_path_input_new), equiv/value-trace FUN_0005ff70 input.
- handle==-1 or site=1 (c3-handle) + patt=0 → it's the handle check (proven faithful) → original would fail identically → **cdb0-is-culprit premise CONFOUNDED**, toggle "fix" came from elsewhere. Critical to know.
- site=7 (onfoot-facing==-1) → facing genuinely -1 (order[0x14]) → dest/order lookup issue.

## RESOLVED LOCALIZATION 2026-06-27d — door grunts take the OVERRIDE_PATH branch, path build returns 0

v2 trace, clean no-shoot run, door slots 6/10/11 (handle 0xee6e0009 VALID):
`exit=2 LAB_fail site=8(repath-pf==0) msrc=3 p99=0 patt=1 pf_run=-2 pf_build=-2 facing=403 f498=0 dist 1.1-2.1`

- site=8 = repath fall-through (path_found==0), NOT handle check → cdb0-culprit premise is sound (handle valid).
- p99=0 (on-foot) + pf_run=-2 (FUN_0005ff70 NEVER called) ⟹ repath took the **override_path branch** (src ~2414-2432), NOT the normal on-foot pipeline. `g_dbg_pf_run/pf_build` are set ONLY in the on-foot else-branch; both -2 ⟹ override branch ran.
- Override branch: `FUN_0005e0d0(override_path, &actor[0x494], actor[0x498], 0); path_found = path_state_build_path(override_path, &actor[0x4a8]);` → returned 0 → LAB_fail → ret 0 → selector reverts msrc=3→guard → grunt won't advance.

KEY SUSPICION: normal "advance to firing position" should NOT use a pre-computed override path — override_path being non-null is suspect. Two hypotheses:
1. **override_path wrongly non-null** in clang-cdb0 (mis-read 3rd param / wrong stack slot) → takes override when original takes on-foot. Toggle (orig cdb0) → on-foot path build succeeds → grunt moves. FITS toggle evidence.
2. Both take override (override_path legitimately non-null) but clang override-branch FUN_0005e0d0 args wrong → bad dest → path_state_build_path fails. NOTE source override FUN_0005e0d0 args differ oddly from on-foot: (override_path, &actor[0x494], actor[0x498], 0) vs on-foot (large_buf, &actor[0x488], actor[0x494], actor[0x498]) — VERIFY vs original disasm 0x2d164-0x2d1bb / 0x2df4-0x2e04.

NEXT: (a) add override_path (+dest 0x488) to trace to confirm non-null & compare to original branch decision, OR (b) diff clang override branch (param [ebp+0x10] read + FUN_0005e0d0 args) vs original. cdb0 sig has store_distance + override_path params.

## ✅ ROOT CAUSE 2026-06-27e — override-branch FUN_0005e0d0 mis-lifted args (actor_moving.c:2429)

Original override branch (delinked 0x2df4 LAB_0002d194):
`mov ecx,[esi+0x498]; mov edx,[esi+0x494]; push ecx; push edx; push edi(&actor[0x488]); push ebx(override_path); call FUN_0005e0d0`
= **FUN_0005e0d0(override_path, &actor[0x488], actor[0x494], actor[0x498])** (same shape as on-foot call, only arg1 differs).

OUR SOURCE (actor_moving.c:2429): `FUN_0005e0d0(override_path, &actor[0x494], actor[0x498], 0)` — **arg2/arg3/arg4 all WRONG**:
- arg2 &actor[0x494] → must be &actor[0x488] (destination position ptr)
- arg3 actor[0x498] → must be actor[0x494] (facing)
- arg4 0 → must be actor[0x498]

Effect: FUN_0005e0d0 sets the override path-build state's DESTINATION from &actor[0x494] (facing field) instead of &actor[0x488] (real dest) → garbage destination → path_state_build_path returns 0 → site=8 LAB_fail → ret 0 → selector reverts msrc=3→guard → scripted door grunts won't advance. ONLY affects actors with a non-null override_path (the a10 door squad's scripted advance), explaining why only these grunts go passive while normal on-foot AI is fine. Matches cdb0-alone toggle fix exactly.

FIX: `FUN_0005e0d0(override_path, (float *)(actor + 0x488), *(int *)(actor + 0x494), *(int *)(actor + 0x498));`
path_state_build_path(override_path, &actor[0x4a8]) is already correct (orig 2e09-2e11).
Also REMOVE all debug instrumentation (struct/macro/globals/fail_site/pf captures) before commit.

## ✅ FIX VALIDATED + DEPLOYED 2026-06-27f

Fix applied (actor_moving.c override branch): `FUN_0005e0d0(override_path, (float*)(actor+0x488), *(int*)(actor+0x494), *(int*)(actor+0x498));` Instrumentation + debug noinline all removed; `git diff HEAD` = the single arg fix + comment only.

VALIDATION (5-fold, all green):
1. Runtime (instrumented build): door slots 6/10/11 went exit=2 LAB_fail/dist 1-2u → exit=3 LAB_path_ok/dist 0.3u (physically advanced), both fresh + checkpoint reload.
2. Delinked disasm (Ghidra delinker): orig override call 0x2d194 = (override_path, &actor[0x488], actor[0x494], actor[0x498]), ESP cleanup 0x18.
3. Fixed clang obj (/tmp/am_clean.o): byte-identical call shape (ebx=override_path from [ebp+0x10], [ebp-0x14]=&actor[0x488]); buggy HEAD differed on 3 args.
4. Live Ghidra agent (xbox-halo-re-analyst): ALL 4 claims CONFIRMED — FUN_0005e0d0 DEREFERENCES param_2 as 3-float position → state +0x50/54/58; param_3/4 stored raw +0x5c/60. Buggy &actor[0x494] put facing into dest-XYZ. "byte-faithful."
5. Hazard scan --changed-only: clean. VC71 auto-selected only FUN_0002b5d0 (unrelated, pre-existing 35% ceiling); Ghidra byte-match supersedes.

Clean un-instrumented build deployed + verified (rev d8ad32eb, 17:29). REMAINING (separate bug, NOT this fix): grunts slow/inconsistent to FIRE after advancing = perception/awareness path; prior note [[reference_a10_grunt_aim_faithful_no_defect]] found firing passivity FAITHFUL (no defect) — needs cachebeta A/B, do NOT re-chase blind. Commit pending.

## NEXT (decisive)
Add `squad_handle=actor[0x34]` + a `fail_site` byte (distinct per goto) to the trace, OR objdump-diff our cdb0 vs `delinked/actor_moving.obj` (FUN_0002cdb0 @ .text 0x2a10) around the case-3 squad-handle check. If trace shows `actor[0x34]==0xee6e0009` AND exit from 2238 → line 2237 codegen bug confirmed → fix the specific mis-compiled compare.

## Instrumentation state (MUST REVERT before commit)
src/halo/ai/actor_moving.c has: noinline on actor_test_destination; `g_cdb0_trace[64]` + `CDB0_TRACE` macro (latch-LAST, struct stride 20); 4 CDB0_TRACE calls at cdb0 exits. g_cdb0_trace base shifts each rebuild — re-extract from live impl. See [[project_a10_FAITHFUL_REPRO_and_gold_signature]].
