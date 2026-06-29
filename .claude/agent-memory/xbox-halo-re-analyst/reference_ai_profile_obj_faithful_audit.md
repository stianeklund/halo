---
name: ai-profile-obj-faithful-audit
description: Exhaustive 2026-06-23 instruction-level audit of all 46 ported ai_profile.obj functions vs pristine XBE found ZERO behavioral divergences; delinked ref proven byte-identical to pristine
metadata:
  type: project
---

# ai_profile.obj exhaustive faithfulness audit (2026-06-23)

Hunted a claimed single mis-lifted function in `src/halo/ai/ai_profile.c`
(symptom: "enemy AI won't advance/maneuver toward player on map a10").
**Verdict: all 46 ported functions are faithful lifts of the pristine XBE.**

**Why:** Direct capstone disasm of the pristine `halo-patched/cachebeta.xbe`
compared instruction-by-instruction against the C for every ported function
(0x540b0-0x55dd0). Every selector dispatch, count_type sub-dispatch, struct
offset, comparison operator/branch direction, switch case→offset/stride
mapping, call-site arg order, loop bound, buffer size, and `@<reg>` ABI
annotation matches.

**Proof the reference is trustworthy:** the delinked `delinked/ai_profile.obj`
`.text` is **byte-identical to the pristine XBE modulo relocation slots**
(verified by parsing COFF reloc table and confirming every byte diff lands in
a reloc-covered position). So VC71 matching the delinked ref == matching the
pristine. VC71 reports all 46 PASS (87.2-100%); the sub-100% diffs are pure
MSVC-version register-allocation / instruction-scheduling variance
(edi-vs-ecx loop counters, spill-slot ordering, jmp/nop placement), NOT
semantic differences.

**Key faithful confirmations** (so future hunts don't re-check):
- Maneuver/attack setters: 0x55750 ai_attack `enc[0]=0`; 0x557e0 ai_defend
  `enc[0]=1`; 0x55870 ai_maneuver `enc[1]=1`; 0x55900 ai_maneuver_enable
  `enc[2]=(flag==0)`. Format strings @0x25c5f8/60c/620/634 confirm identities.
- Count accessor 0x55350 (@<edi> count_type): selector 0=profile
  (+0x2a/+0x2c/+0x18/+0x34), 1=platoon via FUN_00054020 (+0x4/+6/+8/+0xc,
  bound +0xa), 2=squad via encounter_get_squad/0x1c270 (+0x16/+0x18/+0x1a/+0x1c,
  bound +0x6). ct2 span = max(0, start-end). Wrappers 55620=ct1, 55640=ct2,
  55660=ct0.
- Iterators: 54310 (3-int record), 544a0/545a0 (Layout A, 5 ints),
  54680/54750 (Layout B, 6 ints = 3 header + 3 embedded
  encounter_actor_iterator which writes exactly [esi]/[esi+4]/[esi+8]).
  iter[4]=live actor handle. All array sizes correct, no overflow.
- Score-improve refactors verified clean: 0x54220 (if-cascade→switch, case
  1=+0x8c/0xac, case 2=+0x80/0xe8 NOT swapped); 0x547c0
  (condition-direction restructure: handle==-1→-1, else→resource; correct
  actor/child register-reuse separation).
- actor_create_for_unit 12-arg call in 0x54860 verified arg-by-arg.

**Conclusion:** If a real a10 AI regression exists, the root cause is NOT an
instruction-level mis-lift inside ai_profile.obj. Candidates to investigate
instead: (a) a callee in another TU with wrong kb.json signature; (b) the
regression localization to this TU is imprecise; (c) a runtime/state-dependent
issue not visible in static analysis. Needs live-box evidence (toggle-bisect),
not more static reading. Reuses [[reference_actor_firing_position_evaluators]]
pipeline context.
