---
name: real-math-callback-iter-regdef-bar
description: What cleared FUN_00110820 (bounded callback iterator, real_math.obj) at 78.1% VC71 — reg-DEFINED prologue ceiling resolved by full candidate-vs-reference disasm body match incl. forwarded-EBX passthru + 4-arg cdecl callback order proof
metadata:
  type: project
---

FUN_00110820 (real_math.obj) AUTO_ACCEPTED at 78.1% VC71 (36 candidate / 28 reference insns). Reg-DEFINED ABI: start@<eax>, passthru@<ebx>, obj@<esi>, param_1 on stack. ported=true.

**Why 78.1% is a ceiling, not a bug:** cl.exe can't see the `@<reg>` comments, so it emits a cdecl prologue (push ebx/esi/edi callee-save + stack-param loads) the original lacks (it receives 3 args in regs). The 8-insn delta is fully additive.

**The bar that cleared it (read-only, no edit):**
1. Per-fn delinked ref `delinked/functions/00110820.obj` EXISTS + registered in repo-root objdiff.json as `halo/FUN_00110820` → real structural evidence (not a stale whole-file stub). objdump -t confirms `FUN_00110820` symbol present.
2. Reference disasm 1:1 with author-supplied authoritative disasm (28 insns, incl. `lea esp,[esp+0x0]` loop-align pad).
3. CANDIDATE disasm obtained from kept obj `build/vc71/real_math.obj` (objdump -d), NOT just the LCS %. Default mnemonic-LCS=78.1%; reg-normalize=46.9% (LCS aligns poorly because cl.exe uses ECX/EBX/EDX where original uses EAX/EBX/ECX — that's reg-alloc noise, NOT a body bug). **Do not trust reg-normalize % for a reg-defined fn; read the disasm.**
4. ARG-ORDER PROOF via stack-slot→C-value→push-position chain. cl.exe lays out all 4 C params as cdecl stack slots in decl order: start[+8], passthru[+c], obj[+10], param_1[+14]. Candidate pushes edi(index)/ecx(passthru)/ebx(param_1)/edx(context) → callback(context, param_1, passthru, index). Reference pushes edi/ebx/eax(param_1)/ecx(context) → same. cdecl first-push=arg4. MATCH.
5. EBX passthru: never written in original body before the PUSH → incoming arg forwarded untouched (not a local). Confirmed.
6. 16-bit signed loop: both `cmp di,[esi+0x10]` (66 3b prefix) + jge/jl signed; return 0 on first nonzero callback ret else 1. Match.
7. callback ret consumed as int via `test eax,eax` (EAX) — NOT float/ST0, no XCALL risk. `add esp,0x10` = caller cleans 4 dwords. No §3 dup-arg (4 distinct sources), no §10 swap.
8. Build green, hazard scan --changed-only exit 0, extract_reg_args --check 545 OK / 0 drift.

**Generalizable method for reg-DEFINED real_math fns at sub-bar VC71:** kept candidate obj is at `build/vc71/<file>.obj` after a `--no-cache` run — disassemble it and do the stack-slot→push-position arg map vs the delinked reference. This is stronger than any % and is the decisive test for an iterator/callback-forwarding body. Distinct from prior real_math reg-defined bars (putShortMSB/flush_pending leaves, putLong/gz_* serializers) — this is the first FORWARDED-passthru + indirect-callback-order case.
