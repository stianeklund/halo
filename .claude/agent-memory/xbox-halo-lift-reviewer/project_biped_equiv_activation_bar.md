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
  UPDATE 2026-06-08: the CX=1 unicorn witness was since OBTAINED (3 threshold legs on a combat
  snapshot, threshold bits edited per-leg: A=0.02 no-write / B=0.10 CX=0 word=0 / C=0.50 CX=1
  word=1, 0x429=0x3c, all 16/16 oracle==cand, mem-trace clean; the "real witness" control holds —
  leg B WROTE word=0, which a zeroed tag could not produce). This was NECESSARY and PREDICTIVE
  (it gives the in-engine harness its exact target values: word[0x460]=1, byte[0x429]=0x3c past
  fVar2≈0.16667) but is NOT SUFFICIENT: it is the same Unicorn+snapshot method lines 25-26
  pre-classify as weaker than the 1430 in-engine dual-oracle. Coverage gap closed != method
  upgraded. Reviewer verdict 2026-06-08 remained NEEDS_RUNTIME/HOLD on this basis. Do NOT
  re-litigate the coverage point as if it were the whole gap; the open item is the IN-ENGINE
  witness, not more unicorn legs.
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

**INHERITED RULE (set 2026-06-08, 0e00 second adjudication): one dual-oracle leg per
reachable computational branch, and at least one NON-SATURATED leg.** The 1430 bar cleared
because it witnessed BOTH paths (dont_teleport 0/1) on 2 bipeds — branch coverage, not a single
MATCH. 1430 being near-linear is *why* its coverage was easy; 0e00's two-branch + clamp
structure is why one leg is insufficient.

**0e00 second adjudication 2026-06-08 = NEEDS_RUNTIME/HOLD (the IN-ENGINE dual-oracle was
obtained but covers too narrow a slice).** Artifact: artifacts/dualoracle/0e00_inengine_match.txt
(build c57c15d0). Real 0x1a0e00 by @<eax> asm thunk (ported=false -> original intact; thunk RET
caller-cleans, addl $4 correct) vs verbatim static candidate; live biped E2A20033; word[obj+0x460]
+428+429 bitwise-identical MATCH (460:0001,428:00,429:3C). Binary re-verified: tweak touches only
0x3e4, fVar2 (0x3e0) selects CX, so CX=1 holds on natural tag; clamp saturates t->1.0 at
threshold=100 so byte429 tweak-independent; function never READS 0x460 (sentinel symmetric);
constants 0x2533c0=0.0, 0x2533c8=1.0, 0x2546a4=1/30, 0x253394=30. Caller read confirmed at
0x1a546e = `CMP word[EDI+0x460],1`. ALL claims true. HELD anyway because:
  - The witnessed leg is the LEAST precision-sensitive path: t clamped to exactly 1.0, so FDIVP
    result discarded, FMUL is trivial scaled*1.0, clamp-low never ran. The x87 divide / non-trivial
    multiply / clamp arithmetic — the exact chain the dual-oracle exists to de-risk vs MSVC — were
    NOT witnessed.
  - The CX=0 branch (flag=0, base=tag[0x3d4], range=fVar2-fVar1, writes word[0x460]=0) NEVER
    executed in-engine. Divergent-output, reachable normal-gameplay branch (the else of the caller's
    0x1a546e test). = untested divergent-output branch -> fail-closed.
  - A natural-tag re-run does NOT close it (threshold=100 still gives CX=1, t->1.0). Needs a
    different INPUT, not tweak removal.
**What flips 0e00 to ACTIVATE:** add ONE leg, same in-engine method, threshold≈0.10 on the
NATURAL tag -> lands fVar1(0.05) <= thr < fVar2(0.16667) = CX=0, t=(0.10-0.05)/0.11667≈0.43
non-saturated. That single leg witnesses both the CX=0 branch and the full precision chain
(FDIVP fractional, FMUL scaled*t non-trivial, clamp meaningful, _ftol2 truncating a fraction)
bitwise-identical original-vs-ours. Combined with the existing saturated CX=1 leg = both branches
+ non-saturated precision witnessed = clears the bar.
Secondary (non-blocking, noted): candidate is static/single-call in a different TU than the
extern reg-arg-thunked activated fn; "verbatim source" != identical codegen, so MATCH proves
candidate≡original and activated-lift≡original rides a codegen-transfer assumption — low risk,
but argues against one saturated leg being airtight.

**0e00 THIRD adjudication 2026-06-08 = AUTO_ACCEPT (the required leg was delivered).**
Artifact artifacts/dualoracle/0e00_inengine_match.txt gained the 3-leg natural-tag run
(build c57c15d0 19:30:20): Leg A thr=0.02 early-return/no-write (460 sentinel FFFF survives),
Leg B thr=0.10 CX=0 t=0.43 NON-SATURATED byte429=0x07 (FDIVP+FMUL+clamp exercised), Leg C
thr=100 CX=1 saturated byte429=0x3C — all bitwise oracle(real 0x1a0e00 by addr)==candidate.
Leg B is EXACTLY the pre-registered "what flips to ACTIVATE" leg (lines 76-81). Independently
re-verified the binary this round: disasm 0e00-0f00; CX-selection direction re-derived via PF
parity (FCOMP threshold,fVar2; TEST AH,0x5; JP → threshold>fVar2=CX=1, threshold<fVar2=CX=0;
consistent with empirical harness cx values — do NOT apply the feedback_fpu_comparison_direction
operator shorthand without re-deriving parity for the actual operand order, it bit me mid-trace);
constants confirmed from memory (2546a4=0x3d088988=1/30, 253394=30.0, 2533c0=0.0, 2533c8=1.0);
candidate body (units.c:282 dualoracle_0e00_candidate) is statement-IDENTICAL to activated lift
(bipeds.c:925 FUN_001a0e00) incl. the non-obvious offset=threshold in CX=1 branch (FDIVP consumes
the x87-resident threshold from the 0e58 reload); oracle thunk ABI sound (push threshold; call;
addl $4 caller-clean since plain RET; +a handle; memory clobber). Residuals all NON-BLOCKING per
this bar: codegen-transfer (discharged by the identical-source read), clamp-low (mechanical mirror
of witnessed clamp-high, and t>=0 by construction in CX=0 so unreachable there), range<=0 bail
(degenerate-tag defensive, no write — analogous to 1430's accepted unexercised-secondary caveat).
LESSON: when this reviewer pre-registers an exact ACTIVATE criterion and it is delivered + binary
re-verified, honor it — that is the opposite of goalpost-moving. Activation still gated on the
operator's post-flip smoke (in-place turn across fVar2 boundary, caller read at 0x1a546e) +
verify_toggles_live before commit.

**0x1a0680 adjudication 2026-06-08 = AUTO_ACCEPT (cdecl, not register-arg; first cdecl biped
activation on this bar).** Artifact artifacts/dualoracle/0680_inengine_match.txt (build a58a132e).
`char FUN_001a0680(int unit_handle)` cdecl — ragdoll death-settle node updater. Oracle = real
0x1a0680 by plain C cast `(char(*)(int))0x1a0680` (ported=false -> original live; cdecl cast is
the correct ABI, no thunk needed). Candidate units.c:282 dualoracle_0680_candidate is
STATEMENT-IDENTICAL to activated lift bipeds.c:323 (full read of both — only the name/comment
differ; discharges the codegen-transfer residual that nagged 0e00). Both legs MATCH on a NATURAL
player-created corpse h=E2A60037, N=19 ragdoll nodes:
  - leg=NATfall (real fall-through, the node-writing branch): node_block 988B + 0x4e49f0 copy
    228B BITWISE identical (nbdiff/cbdiff=-1), ret o00==c00, gated++ post47c o01==c01,
    control(O1==O2)=PASS.
  - leg=early (47c>=47d): ret o01==c01.
Binary re-verified this round (disasm 0x1a0680-0x1a0773): early-exit JNZ->0x76b epilogue does
NO writes before ret (CL=CMP/SBB/INC result); register marshalling into FUN_001a03c0 confirmed
EAX=unit_handle, EDI=node_block, stack=[node_count(antr+0x68), 0x4e49f0] — EXACTLY the lift's
call + kb decl (@<eax>,@<edi>); gated ++ is integer `CMP AL,0x7f; JNC; INC AL` (NOT FPU — so
feedback_fpu_comparison_direction does NOT apply; VC71/source-review catch any boundary flip).
KEY ABI INSIGHT: FUN_001a03c0 is ported=true, so BOTH oracle and candidate route through OUR
lifted 03c0. This does NOT weaken the verdict — the dual-oracle holds the ENTIRE callee env
identical on both sides (the only variable is the 0680 body), so nbdiff=-1 attributes zero
divergence to 0680 and SPECIFICALLY CERTIFIES 0680's reg-arg marshalling into 03c0 is byte-faithful
(an ABI bug VC71 can't see). 03c0's own correctness is held-constant + already independently
activated; activating 0680 adds zero new exposure to 03c0 (the live original 0680 already calls
the redirected 03c0 lift). Two unwitnessed SUB-branches of fall-through, both NON-BLOCKING by
classification (NOT divergent-output forks like 0e00's CX): (a) count<=0 skips copy loop — count
is antr+0x68 animation-graph constant, <=0 = degenerate tag, structurally identical `if(count>0)`
both sides; (b) 47c>=0x7f saturation skips ++ — a clamp MIRROR of the witnessed 0->1 increment,
integer boundary, not a precision chain. 0680 is INTEGER-ONLY so there is no x87 precision axis —
the "non-saturated computational leg" requirement that gated 0e00 is N/A here; NATfall already
exercises the real work non-trivially (988+228B). COMMIT PRECONDITION (not a verdict blocker):
dualoracle_0680_probe() is wired LIVE+unconditional at units.c:685 inside units_update — it pokes
47c/47d + snapshot/restores live biped state EVERY tick; MUST be stripped/gated before flipping
ported=true or it runs in the shipped build. Activation still gated on operator post-flip smoke
(patch.py cdecl redirect on a real biped death — by-address oracle never exercised the redirect)
+ verify_toggles_live.

See [[or-const-store-width-benign]] for the 0x1a2290 obj+0x424 width-disjoint adjudication.
