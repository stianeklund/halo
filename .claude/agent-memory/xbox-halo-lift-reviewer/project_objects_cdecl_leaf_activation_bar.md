---
name: objects-cdecl-leaf-activation-bar
description: What cleared the objects.obj cdecl-leaf activation gate (134ae0 glow-init 98.9%, 13fe10 object_get_first_cluster 100%) — the read-only/cluster-edge proof and the VC71 stale-chunk cross-check method
metadata:
  type: project
---

Two objects.obj cdecl leaves were AUTO_ACCEPTED for activation on 2026-06-22:
`FUN_00134ae0` (glow-widget init, 98.9% VC71) and `object_get_first_cluster`
(0x13fe10, cluster-iter init, 100% VC71). Both pure cdecl, no @<reg> in kb decl,
entry disasm confirmed args from [EBP+8]/[EBP+0xc] only.

**Why these cleared the bar (the evidence pattern to repeat):**
- VC71 independently re-measured at claimed %, ABI matched entry disasm.
- The one register-arg callee in 134ae0's path (FUN_001345b0 glow_widget@<eax>)
  had its EAX setup confirmed by the 98.9% codegen match (re-fetched datum_get
  result lands in @<eax> slot, object_handle PUSHed).
- 13fe10 proven READ-ONLY w.r.t. lifecycle: callees {FUN_0013d7f0,
  FUN_00191690, object_get_and_verify_type} and callers {0x34c80, 0x13aa10,
  0x1407e0} have ZERO edge into the GC/lifecycle cluster. Raw delinked disasm
  shows it writes ONLY iter_state[0] (mov %eax,(%esi)) and iter_state+4 (lea
  0x4(%esi)) = 8-byte footprint. Both callers declare ≥8B buffers (char[16],
  int[2]) — no buffer-alias hazard. Paired with FUN_0013d5f0 cluster-next.

**VC71 stale-chunk cross-check (the analyst's warning was real but benign here):**
vc71_verify auto-selected the BATCH ref `0013fb30.obj` (FUN_0013fe10 at offset
0x2e0) instead of the standalone `0013fe10.obj`. To prove it wasn't stale-chunk
shadowing: `objdump -h` both refs (standalone .text = 0x57 bytes), `objdump -d`
the standalone ref, confirm the symbol's body is byte-identical to the batch
member. compare_obj --function FUN_0013fe10 FAILS to match because the candidate
TU renamed the symbol to `object_get_first_cluster` — name-based lookup misses
it; rely on vc71_verify -f <renamed_name> + raw objdump of the ref instead.

**The 2 dormant cluster-holds were CONFIRMED real (not false benches):**
- attachments_new (0x13ecb0): caller = FUN_00143c80 (object_new, cluster member)
  CONFIRMED; mutates object+0xf4/+0xfc/+0x4. Correctly dormant despite 92.1%.
- FUN_0013cb80 (86.7%): callees include FUN_00144770 (cluster) + FUN_00144b50
  (objects_garbage_collect_tick). GC-tick edge CONFIRMED. Correctly dormant.

How to apply: for objects.obj cdecl-leaf activations, require (1) re-measured
VC71 at claim, (2) entry-disasm cdecl confirmation, (3) for any iterator/buffer
primitive, prove the write footprint from delinked disasm and check caller
buffer size, (4) cluster-edge check via get_function_callees/callers against the
known lifecycle cluster set. Relates to [[compare_obj-diff-polarity]].
