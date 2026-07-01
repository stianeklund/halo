---
name: real-math-zlib-putshort-flushpending-disasm-bar
description: What cleared FUN_00110e40 (putShortMSB) + FUN_00110e70 (flush_pending) on 1:1 disasm audit when committed delinked real_math.obj does NOT cover the 0x110e range (VC71 numbers author-reported, reg-DEFINED prologue-capped)
metadata:
  type: project
---

AUTO_ACCEPT on both FUN_00110e40 (87.8% 23/18) + FUN_00110e70 (88.9% 41/40, +1 insn) by STATIC 1:1 disasm audit, not byte-match.

**Why the VC71 number was not the gate:** Both are reg-DEFINED (`@<ecx>/@<eax>` for putShortMSB, `@<eax>` for flush_pending) so VC71 cannot emit the function's own reg-receiving prologue → permanent sub-90% cap that lives in the preamble, not the body (see [[feedback_eax_defining_fn_vc71_ceiling]]). ALSO the committed `delinked/real_math.obj` (Jun 21) only covers the lower geometry funcs — it has NO symbol in the 0x110e range, so the reported VC71 % was produced against a fresh per-fn/extended export that is not reproducible from committed artifacts. Treat such VC71 numbers as author-reported; the gate is the disasm audit.

**Why disasm audit was sufficient (no NEEDS_RUNTIME):** the only categories that force runtime — dark dataflow, FPU ordering, unresolved implicit-reg-arg/offset uncertainty — are all absent:
- T1 putShortMSB is a PURE LEAF, no FPU, no callees.
- T2 flush_pending's only callee is csmemcpy (cdecl `void*(dst,src,size)`, no implicit reg args), so no dark dataflow. Same logic as the gz_* / putLong static-clear precedents ([[project_real_math_gzdestroy_esi_disasm_audit_bar]], [[project_real_math_putLong_eax_ebx_disasm_audit_bar]]): all-cdecl callees = static audit carries.

**The two silent-bug surfaces a clean disasm match MUST name (not skip):**
1. T1 low-byte source: orig stores `CL` (original ECX = UNSHIFTED byte_val); only the EDX copy got `SHR ...,8`. Impl `(unsigned char)byte_val` (not the shifted value) — correct. A lift that stored the shifted value twice would byte-match-fail but is the kind of thing to check.
2. T1 `SHR` is logical / T2 `JBE` is an unsigned min. Impl uses `unsigned int` for byte_val and `unsigned int len` with `(unsigned int*)` casts — both correctly unsigned. A signed `int` len here = real mismatch (signed compare flips the min on huge avail_out).

**T2 base-distinctness check (no aliasing):** strm+0x10 (avail_out) and s+0x10 (pending_out) share the same numeric offset but DIFFERENT bases (strm vs s=strm->state). Impl correctly writes `*(int*)(strm+0x10) -= len` vs `*(int*)(s+0x10) += len`. Verify the base, not just the offset, when two structs collide on an offset.

**T2 reset polarity:** orig `JNZ end` after `TEST pending` → reset (pending_out=pending_buf, s+0x10=s+0x8) ONLY when pending==0. Impl `if (pending==0)` — correct polarity.

**csmemcpy push order (no §10 swap):** `PUSH EDI/EAX/ECX; CALL` → cdecl reverses → (ECX=next_out=dst, EAX=pending_out=src, EDI=len). Impl `csmemcpy(next_out, pending_out, len)`. Matches.

kb arg-name cosmetic mismatch (kb names flush_pending arg "state"; impl names it "strm") is benign legal C — same `@<eax>` storage.
