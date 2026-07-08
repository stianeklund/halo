---
name: lruv-cache-11d110-stateless-allocator-sub90-accept
description: lruv_cache.obj FUN_0011d110 lru_cache_new allocator 85.1% VC71 AUTO_ACCEPT on stateless-leaf zero-fill+stubs equivalence lane (no live snapshot needed); plus ICF self-call + assert-path system_exit shape notes
metadata:
  type: project
---

FUN_0011d110 (0x11d110, lruv_cache.obj) = lru_cache allocator/initializer, 6-arg cdecl
`void* (const char* name, int total_size, int block_size, void* delete_cb, void* query_cb, void* buffer)`.
85.1% VC71, AUTO_ACCEPT.

**Why sub-90 cleared:** full-body 1:1 static decode of ALL insns vs disasm (0x11d110-0x11d24e) —
every offset (+0x20 count, +0x24 element_size, +0x28 total, +0x2c delete_cb, +0x30 query_cb,
+0x34 buffer, +0x38 owns_buffer byte, +0x3c/+0x40 zero, +0x44 'curl'=0x6c727563, name copy +0..+0x1f
NUL), signed IDIV element_count=total/element_size, alignment round-up (ADD 0x10; TEST BL,3; OR 3; INC),
NULL-callback default substitution to FUN_0011cfd0/FUN_0011cfe0, all 6 cdecl call sites arg-order+count
verified vs push sequences (debug_malloc 4-arg ×2 lines 0x6e/0x75, csmemset(cache,0,0x48), csstrncpy
(cache,name,0x1f), debug_free line 0x8e, display_assert 4-arg), and the sole @<eax> callee FUN_0011d090
(cache in EAX — MOV EAX,ESI before CALL, kb decl `int cache@<eax>` preserved). Zero defects.

**Runtime lane:** equivalence 100/100 zero-fill+stubs, 0 divergences, 0 mem-trace, 0 stub-arg mismatches,
HIGH confidence, 74.9% coverage. KEY: this is a GENUINELY STATELESS allocator — reads NO globals, NO
object/datum tables — so zero-fill+stubs IS the complete/appropriate lane; a live snapshot adds nothing
(object-pool state cannot reach any branch). The uncovered 25% is the malloc-FAILURE + assert-halt paths
that stubs render unreachable; decoded 1:1 by hand (free+return-0 @0x11d235 trivial; asserts halt). This
is the AUTO_ACCEPT pattern (body RAN at high conf, like convex_hull2d 66.4%) NOT the NEEDS_RUNTIME pattern
(bsp3d 88.4% guard-only 16% cov, dark body). Coverage discriminator = "did the substantive body run",
not raw %.

**Discount contradictory acceptance boilerplate:** the run's equivalence note claimed both "live-state
snapshot was NOT used (stateless)" AND "a 0-divergence pass on the live infection_swarm snapshot is
accepted evidence" — self-contradictory copy-paste. The live-snapshot pass did NOT happen; do not credit
it. The real+sufficient evidence is the zero-fill+stubs run. Verify what actually ran, don't rubber-stamp
the boilerplate justification.

**Mismatch class (the ~15% gap):** display_assert/assert-macro expansion + instruction scheduling of the
interleaved store block (0x11d1e1-0x11d21b) + register allocation. Assert-macro LCS desync — not a body bug.

**Assert-path fidelity note (non-blocking):** source writes `halt_and_catch_fire()` but the binary does
`PUSH -0x1; CALL 0x8e2f0` = `system_exit(-1)` (0x8e2f0 = system_exit __noreturn). Sibling FUN_0011d010
correctly uses system_exit(-1); FUN_0011d110 source is slightly less faithful here. Unreachable on valid
input (display_assert halt=1 terminates first), functionally equivalent (both halt), contributes to LCS
gap only. See memory [[feedback_system_exit_vs_hcf]] (re-lift target if pushing % higher).

**Sibling FUN_0011d010 (in same pending commit, NOT the review target):** validator with a literal
`CALL 0x0011d010` self-call at 0x11d031 — CONFIRMED present in shipped binary disasm (=/OPT:ICF COMDAT
fold of a byte-identical sibling validator onto this addr). Reproducing it as a literal self-call in C is
byte-FAITHFUL — introduces no regression vs original. FUN_0011d110 does not call it, so it cannot affect
this verdict. If that validator is ever live on an intact block it recurses on unchanged entry+4/entry+0xc
(a property of the original binary, not our lift) — worth a human eyeball but out of scope here.
