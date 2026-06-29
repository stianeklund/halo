---
name: a10-prop-deletion-path-faithful
description: a10 grunt "loses player prop / never engages" — the ENTIRE ported AI prop-deletion + per-tick perception path is disasm-faithful; deletion of the state-5 player prop is the FAITHFUL decay-timeout tail; root is upstream LOS/+0x268 input divergence, not a static mis-lift
metadata:
  type: project
---

# a10 prop-deletion path: exhaustive disasm audit (2026-06-25)

**Task premise:** a ported producer prematurely DELETES/ORPHANS the player's perception prop (prop reaches prop+0x24==5 tracking player, datum freed, actor+0x270=-1, FUN_0003b410 forces actor+0x268=0). Player-controlled 2-runs/arm capture. Confirmed prop-level over XBDM.

**Chain-empty discriminator (patA->patB, focus_probe data):** grunts 10/11/12 chain went 7->6 props, NOT ->0. So this is a TARGETED single-prop deletion of the actor+0x270 primary, NOT actor_delete_props (which empties chain to -1). The primary prop (state 5, idx43, parent_c=idx39) was deleted and idx39.parent_c cleared to -1 — the classic parent_c-clear + FUN_0003b410(-1) + datum_delete deletion pattern.

**WHO can delete a prop (pristine E8 caller scan of 0x64a80/0x3b410, byte-accurate):**
- 0x33330 promote (UNPORTED) — site 0x33404
- 0x34c80 (UNPORTED) — 6 sites 0x35209..0x355c8
- 0x355f0 per-tick perception DISPATCHER (UNPORTED) — sites 0x36043/0x360ff/0x36344
- 0x40700 (UNPORTED, ai.obj) — site 0x407ac
- **0x3cbc0 actor_delete_props (PORTED)** — full teardown, empties chain; NOT this bug (chain not emptied)
- **0x5aab0 encounter_stand_down (PORTED)** — explicitly SKIPS primary (`0x5aba9 cmp eax,[ecx+0x270]; je`), C faithful; deletes only orphan secondaries

So the targeted deletion of the state-5 PRIMARY is done by UNPORTED 0x355f0/0x33330/0x34c80, never by ported code. The two ported deleters can't produce the observed signature.

**Dispatcher state-switch (jmp [eax*4+0x36848]):** state 5 -> 0x36265 (clears prop+0xb8=0,+0xb4=-1) -> 0x36295 sets prop+0x24=new -> calls 0x2fc20 -> stores result prop+0xa4. Deletion at 0x36344 reached post-switch when decay flag [ebp-0x1f] set (prop+0x3a < 0 at 0x361cb). **Decay rate (sub word [esi+0x3a],ax at 0x361c0) is COUPLED TO actor+0x6e** (0x361a9 cmp [edi+0x6e],cx ... -> subtracted amount): low alertness => faster decay => prop deleted. prop+0x3a written ONLY by the unported dispatcher (esi=prop there; do NOT confuse with actor+0x3a = squad index).

**Deletion is the FAITHFUL tail.** For state 5, ported 0x2fc20 correctly returns result=0 (state range [2,3] only). Both builds transit state 5 (memory line 77); unpatched promotes 5->3 before decay expires, patched lets decay win. So deleting a state-5 prop is the correct downstream effect of losing the promote-vs-timeout race; it is NOT the bug.

**Ported per-tick perception inputs to dispatcher — ALL RE-AUDITED FAITHFUL vs pristine (instruction-level, 2026-06-25):**
- 0x2fc20 actor_get_perception_knowledge (the "still-perceive" boolean -> prop+0xa4): branch-for-branch faithful incl both side-effect clears (+0xaa/ae/ac, +0x3a8/3ac). NOT inverted.
- 0x2f380 engagement-level reader (0-3): all return-3 clauses + orphan-path r=(orphan+0xb8!=0)+2 + tail (6e>=2?2 : 6a>=3) faithful.
- 0x2f910 find_prop_pathfinding_location: faithful (not in decay loop anyway).
- 0x2f2b0/0x2f380/0x2fd10/0x369c0/0x3d430/0x1f9a0/0x55110/0x14c10 previously audited faithful (prior memory).

**Ported callees of alertness recompute 0x302b0 / target-status 0x300b0 / promote 0x33330:** only CRT/data/objects helpers (0x8d9f0,0x8e2f0,0x119320,0x13d680,0x2f2b0,0x3b410,0x64a80) — all faithful primitives. No AI-logic divergence.

**CROSS-BUILD FIELD DIFF (patA/patB vs unpA/unpB, committed-on-player grunt, matched by trk==player NOT index):** The discriminator is NOT actor+0x9e (LOS result = 0 in BOTH builds). It is PROP STATE / prop+0x66:
- unpatched state-5 primary prop: prop+0x66 = **3**, budget+0x3a = 860, promotes 5->3 (committed, 268=10/6e=7) by unpB.
- patched state-5 primary prop: prop+0x66 = **-1** (uninitialized/never-acknowledged), budget+0x3a = **507** (lower = more budget burned, consistent with prop spending ticks NON-primary at 10/tick fast decay), then DELETED by patB.
prop+0x66 = -1 means the patched prop NEVER received a unit_effect=3 acknowledge (which sets +0x66=3 via 0x3d430). So in patched the promotion never fires AND the prop loses primary repeatedly -> fast decay -> deletion.

**ALL PORTED writers of the divergent fields RE-AUDITED FAITHFUL instruction-level (2026-06-25):**
- prop+0xbb (decay selector): written by PORTED 0x2f2b0 (acknowledge, sets =0) -> FAITHFUL; and unported 0x32c10/0x647c0.
- prop+0x3a (decay budget): seeded =0x384 by unported 0x647c0 (prop create); decremented only by unported dispatcher 0x361c0; PORTED writers 0x3c410/0x59480/0x5d200 are non-perception (squad/encounter), not the player-prop budget.
- prop+0x66 (engagement/acknowledge level): ONLY PORTED writer is 0x3d430 (actor_handle_unit_effect) -> RE-AUDITED FAITHFUL (update gate `+0x66==-1 || unit_effect>=+0x66`, +0x68=(ue==3)?0x96:0x1e, all switch cases match disasm). Other writers 0x355f0/0x64170 unported.
- 0x2fd10 actor_compute_prop_target_weight (the per-tick PRIMARY-SELECTION scorer; FPU-heavy; the LAST ported decision-input on the selection path) -> FULL-BODY disasm diff incl the float score tail: byte-faithful. Bonus conds (270==-1 -> local_c=3.0 if 0x12e!=0||clump==+0x54; clump==270 && 6e>=3 -> bonus_flag=1; 0x134!=0 -> extra_flag=2) match; score = (float)(extra+bonus+vision+aware)*10 + 5/(dist*0.1+1) + local_c with correct FILD(sum)/FLD(dist)/fdivr/operand order. vision_level=EDI, awareness=EAX register mapping correct. orphan-awareness block (assert 0x1086; +0xb8!=0?3:(state==4)+1) faithful.

**WHY the deletion despite 1/tick primary decay (advisor's key catch):** a primary prop (bb=1) decays only 1/tick from budget ~507-900, so it cannot time out in a ~9s gap WHILE primary. The deletion requires the prop to repeatedly LOSE primary status (-> 10/tick fast decay as a secondary). That re-selection happens in the UNPORTED dispatcher 0x355f0 each tick using ported scorer 0x2fd10's weight (faithful) + unported sense/distance (0x31df0/0x31a90). So the player prop's weight/priority is being beaten by other props (or its sensory inputs are intermittent) in the UNPORTED selection — fed by faithful ported inputs. Not a ported mis-lift.

**CONCLUSION (reconfirmed, now rigorous):** No static mis-lift in any ported AI perception / prop-lifecycle / deletion function. Root = upstream divergent INPUT to the unported perception tick: per-tick LOS success (actor+0x9e, set by UNPORTED 0x416e0 ai_test_line_of_sight) drives actor+0x268/+0x6e; low/flickering => decay wins => prop deleted. 0x416e0's ported producer (raycast 0x14df70) audited faithful previously (see [[reference_ai_los_416e0_ported_producers_faithful]]); residual is the raycast INPUT (origin/direction floats) or unported occlusion deciders 0x149480/0x14dce0 fed by BSP/object state. The ported defect, if any, is UPSTREAM of the AI-perception scope (likely biped aim/transform feeding LOS), not in props/actor_perception/actors prop-lifecycle.

**DECISIVE next experiment (not doable statically):** live input-diff the floats passed to 0x14df70 at the 0x416e0 call site, patched vs unpatched, on a committed grunt. Divergent => trace ported writer of the LOS origin/direction (biped aim path). Confirm-toggle for any candidate = toggle the bad-INPUT producer, NOT the deleter/dispatcher (those are unported/faithful; toggling actor_delete_props tail-calls original = non-diagnostic).

**Tooling:** artifacts/ai_regression/disasm_range.py (XBE base 0x10000, code sec0 VA 0x12000; addresses are full VAs, disasm directly). focus_probe.py + the chain-len discriminator (inline python this session) on actors_patA/patB.bin + props_patA/patB.bin. E8 rel32 byte-scan for callers (linear capstone of full sec0 desyncs on data — use byte-scan).
