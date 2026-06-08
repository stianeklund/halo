---
name: biped-equiv-activation-bar
description: The sanctioned evidence bar for activating (ported=true) register-arg biped functions on equivalence rather than byte-match; the 1430 precedent and how 0e00/2290/0b30 measure against it
metadata:
  type: project
---

Register-arg biped functions in src/halo/units/bipeds.c (`@<edi>/@<eax>` callees) have a
permanent VC71 byte-match ceiling in the mid-80s: VC71 compiles them cdecl-frame + pushes,
scored against the original's frameless register prologue. Empirically confirmed (permuter
ran 188 non-vacuous iters on 0x1a1e70 for 0.15% best). So byte-match CANNOT clear 90/95/98 —
these fall in the `<90%` review lane that REQUIRES golden/runtime behavior verification.

**The only sanctioned activation bar = commit ed973c20 (biped_fix_position 0x1a1430):**
- Method: TRUE DUAL-ORACLE in-engine — real original called by address (ported=false) vs a
  verbatim copy of the lift, run back-to-back on LIVE bipeds.
- Witnessed: the ACTUAL object mutation (obj+0xc position) bitwise-identical, 24/24 on the
  production path (dont_teleport=0, both real callers' config), plus the dont_teleport=1 path.
- Coverage: BOTH code paths, 2 distinct bipeds.
- Caveat accepted: only unexercised SECONDARY branches (seat-occupied seat!=-1).

**Why:** Activation flips LIVE biped physics across ALL object states. A wrong ACCEPT ships a
movement regression. The 1430 bar witnessed the real product on the real path; that is the bar.

**How to apply:** Unicorn-equivalence with a single combat/tweaked snapshot is WEAKER than the
1430 dual-oracle and does NOT clear the bar when ANY of these holds (each = fail-closed):
- An untested divergent-OUTPUT branch under the single snapshot (not dead/defensive code).
  Ex: 0x1a0e00 writes word[obj+0x460]=CX where CX∈{0,1}; all 50 seeds hit CX=0 (witnessed
  0x460=0x0000), the CX=1 output branch is in the uncovered 20.6%. -> NEEDS-MORE.
- The product is a side-effect via a STUBBED callee, hence unwitnessed. Ex: 0x1a0b30's product
  is object_delete(unit) via FUN_00140cc0 (ported=false, stubbed); harness saw CONTROL-FLOW-ONLY,
  return always 0. Call ABI is faithful (PUSH EDI;CALL 140cc0 = 1 cdecl arg) so NOT a reject,
  but the erase was never witnessed. -> NEEDS-MORE.
- confidence:WEAK + all-seeds-identical-return + single path, when the input space is gated by
  tag/object state the single snapshot fixes. Ex: 0x1a2290 (all 50 ret 0xe26f0001) skipped the
  actor_aim_jump veto path (actor_handle==-1 every seed). -> NEEDS-MORE even though stores match.

What WOULD flip each to ACCEPT: a dual-oracle/golden harness (like 1430's) that witnesses the
real product on the gated path — CX=1 store for 0e00, the actual object_delete for 0b30, the
actor_aim_jump-vetoed and non-vetoed paths for 2290 — original-vs-ours bitwise identical.

See [[or-const-store-width-benign]] for the 0x1a2290 obj+0x424 width-disjoint adjudication.
