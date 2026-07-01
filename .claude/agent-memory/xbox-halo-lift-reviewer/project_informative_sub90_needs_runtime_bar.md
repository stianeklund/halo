---
name: informative-sub90-needs-runtime-bar
description: Why an INFORMATIVE sub-90 VC71 (valid registered ref) is NEEDS_RUNTIME even with a clean static body audit — the blind-fallback disasm bars do NOT apply; band policy governs
metadata:
  type: project
---

A clean, fully-classified static body audit does NOT clear AUTO_ACCEPT when VC71 is **informative and below 90%**. The <90% band literally requires "golden/runtime behavior verification PLUS classified mismatches" — classified mismatches alone are the necessary half, not the sufficient half. Verdict = NEEDS_RUNTIME (not REJECT when no concrete bug + ABI complete + no FPU risk).

**Why:** The disasm-audit bars in memory ([[project_real_math_putLong_eax_ebx_disasm_audit_bar]], gz_destroy, gz_flush, deflateInit2_) all cleared on static-alone *because VC71 was BLIND* — stale/short stub, unregistered ref, or uninformative % (2.5%, 54-insn stub). That is the no-signal fallback. When VC71 is **informative** (e.g. 78.7% against a valid, registered `delinked/functions/<addr>.obj`), the band policy governs and static alone is insufficient. Even the FPU-leaf cap that WAS legitimately capped (cc90/vector_intersects_pill2d, [[project_real_math_fpu_equiv_activation_bar]]) still required EQUIVALENCE, never static-only.

**How to apply:**
- Check the band FIRST and quote the RIGHT band. 78.7% is **<90%**, not 90-95%. The 90-95% requirements (classify + call-audit + offset-audit + no-Uncertain) are NOT the bar for <90% — they're a subset of it.
- Do NOT report "157/158, one delta" as if that's the match. Counts (157/158) differ by 1; the **LCS match is 78.7%**. One redundant ref-only reload explains the COUNT delta, not a ~21pp gap. The gap = regalloc reordering of semantically-equivalent insns + the @<reg>-DEFINED prologue cap, manually reconciled. Saying "1 delta" overstates structural match and the next reviewer can't reconcile it against 78.7%.
- Integer-only + cdecl-stubbable callees = textbook "structurally capped → equivalence to prove behavior." Recommend `unicorn_diff.py <addr> --allow-stubs --mem-trace`; caveat that state-pointer-walking fns (head[]/prev[]/window) need `--state-snapshot` ([[reference_statesnapshot_recovers_vc71capped]]) because zero-fill under-covers the slide+rebase path.
- A high-confidence equivalence pass would make AUTO_ACCEPT defensible on RE-review; until it exists, NEEDS_RUNTIME.

**Worked case (FUN_00111770, zlib fill_window, real_math.obj, @<esi>=state, 78.7%):** body audit was fully clean — 7-point verify all PASS (more special-paths, slide cond 2*w_size-0x106 unsigned, both rebase loops head@0x3c/prev@0x38 not swapped, assert `jae` unsigned, adler `FUN_00110a10(adler,next_in,n)` guarded by wrap==0, csmemcpy(dest,next_in,n), ins_h dual-store @0x40, w_size read ONCE at prologue 0x09 never reloaded in loop). No FPU, no CONCAT, no NULL-reg-arg, callees all cdecl. Still NEEDS_RUNTIME because informative sub-90 with no runtime evidence.
