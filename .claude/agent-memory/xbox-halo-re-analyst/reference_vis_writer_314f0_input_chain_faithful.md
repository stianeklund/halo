---
name: vis-writer-314f0-input-chain-faithful
description: 2026-06-28 exhaustive disasm audit of the los->vis writer 0x314f0 and ALL its ported input producers for the a6 grunt vis=3->0 bug — every ported lever is byte-faithful; vis=0 likely arrives via a 0x33440 literal-0 BRANCH, not the writer returning 0
metadata:
  type: reference
---

# Visual writer 0x314f0 input chain — full ported-producer audit (a6 grunt vis 3->0)

Context: a10 PoA close-door grunt "a6" won't fire on patched build. Pinned to
los->visual-perception conversion: with matched origin/target/los(prop+0x38=0 CLEAR),
cachebeta writes vis(prop+0x32)=3, patched writes 0.

## Writer 0x314f0 control flow (return = vis level 3/2/1/0), UNPORTED
- Gate: `param_5(los) != 0 && != 1` -> return 0. los=0 PASSES.
- `local_c` = perception DISTANCE = max([actr_tag+0x18], [actv_tag+0x150]) * switch_const[param_8].
  switch(param_8): 0->_DAT_00253524, 1->_DAT_00253f3c, 2->_DAT_002533f0, 3->_DAT_002533c8.
- `local_10` = squared dist(param_2 origin, param_3 target).
- STEP-3 distance gate (0x315f9 FCOMPP): `if (local_c*local_c <= local_10) return 0` — cleanest 3->0.
- `local_8` = view-cone weight via FUN_0018e690(param_2+0x24, param_2, param_3) + param_4.
- STEP-6 gate (0x316d1): `if (local_10 < (local_8*local_c)^2)` else return 0.
- CASE A (`[actr+6]==0 && param_6!=0`): elevation fpatan from actor BASIS +0x18c..+0x1ac;
  if outside cone [_DAT_00256138.._0025613c] -> `local_c=0` (other clean 3->0); else
  FUN_0002f470 refine. CASE B: `local_c = _DAT_002533c4 * fVar4`.
- RETURN-3 gate: `if (param_5==0 && local_10 < local_c*local_c) { local_10<_DAT_00255fd8 ? 3 : 2 }`.

## ALL ported producers of writer inputs — VERIFIED BYTE-FAITHFUL vs disasm (do NOT re-audit)
- 0x2f380 knowledge_type -> param_8 (switch selector). FAITHFUL (signed cmps JL/JLE/JGE/SETGE all match; final `[actor+0x6e]>=2?2:([actor+0x6a]>=3)`). disasm 0x2f380-0x2f46a.
- 0x211f0 actor_combat_get_firing_variant_definition -> actv tag (=> [actv+0x150] distance). FAITHFUL. disasm 0x211f0-0x21260.
- 0x3b270 actor_attacking_target (callee of 0x211f0, selects base vs weapon-override actv).
  FAITHFUL incl. 1st call MOVZX (0x3b2aa) / 2nd call MOVSX (0x3b2f3) of [unit+0x2a2]. disasm 0x3b270-0x3b310.
- 0x3dc20 actor_input_update -> BASIS +0x18c..+0x1ac (case A elevation). FAITHFUL.
  up-vec cross (0x3e300-0x3e337): +0x198=ly*wx-wy*lx, +0x19c=wz*lx-lz*wx, +0x1a0=lz*wy-wz*ly.
  right-vec cross (0x3e341-0x3e37a): +0x1a4=lx*uy-ly*ux, +0x1a8=ux*lz-lx*uz, +0x1ac=ly*uz-lz*uy.
  C source actors.c:7290-7335 matches disasm EXACTLY (Ghidra DECOMPILE shuffled operands and is the
  misleading one — source was lifted from raw disasm per its own comment). looking +0x18c<-biped+0x210.

## KEY REFRAME (advisor): vis=0 != writer returned 0
prop+0x32 (vis) is stored from ~8 sites in 0x33440 (prop_status_refresh, UNPORTED):
- writer result: lines 292, 339, 442, 445
- LITERAL 0: lines 352 (LAB_00033cd1), 407, 451
For the LIVE path (prop+0x24=0 state<4 => line-198 block; los=0), the writer call at line 328/339
is reached only if NOT routed to LAB_00033cd1. LAB_00033cd1 (vis=0) gates:
`cVar7`(=local_8 or recomputed), `[prop+0x131]`(=_DAT_00253398<(float)unitobj[0xcb], line 220),
`[prop+0x60]`(friendship), `[prop+0x12e]`, `[prop+0x11c]`. local_8 from squad rec
(datum_get 0x5ab270,[actor+0x34])[+0x40] or [actor+0x6a]==1.
These flag inputs come from the UNIT OBJECT (local_20 = object_get_and_verify_type(prop+0x18,3),
biped physics, outside ai.obj) and squad/encounter pool. A single divergent flag routes to vis=0
with the writer being called identically or not at all.

## CALL-SITE ARG MAP (2026-06-28 follow-up, disasm-verified)
0x33440 has 3 writer call sites. State gate at 0x338c6: `[prop+0x24]<4 || >5` -> JL/JG 0x3399d
(line-198 block, sites 0x33bec + 0x33d45); state in [4,5] -> 0x33967.
LIVE for broken state=0 = **0x33d45** (decompile line 329, result -> [prop+0x32] @0x33d6b). Args (PUSH order param_8..param_1):
- param_8 = CALL 0x2f380 (knowledge_type) -> switch mult {0:0.4, 1:0.6, 2:0.8, 3:1.0}
- param_7 = [prop+0x12e] (zero-ext byte) — gates debug-record store; NO ported writer in src
- param_6 = local_14 [EBP-0x10] = case A/B SELECTOR (1=caseA, 0=caseB). NOT a cone weight (coordinator's premise wrong).
  Computed 0x33c8c-0x33cc7: =0 if [actor+0x15e]==4 or [actor+4]==0xf; elif [prop+0x60]!=0 friendly...;
  elif enemy([prop+0x60]==0): =0 if [actor+0x6a]>=3, else =1 iff ([prop+0x127]!=0 || [prop+0x12c]!=0).
- param_4 = [EBP-0x24] = [prop+0x120] byte, or literal 2 if [prop+0x132]!=0
0x33bec (line 287, danger path local_c!=0): param_6=LIT 1, param_7=LIT 0, param_4=[prop+0x120].
0x33967 (line 440, state 4/5): param_8=LIT 2, param_6=LIT 1, param_7=LIT 0.

## WRITER CONSTANTS (read 2026-06-28)
switch mult 0.4/0.6/0.8/1.0; _DAT_00255fd8=36.0 (return3 if local_10<36, else 2); caseA elev cone
[_DAT_00256138=-0.785398 rad(-45deg), _DAT_0025613c=+0.523599 rad(+30deg)]; _DAT_002533c0=0.0.
distance 3.78 => local_10~14.3 < 36 => return-3 region; so vis=0 is a HARD gate-0, not the 3-vs-2 split.
ONLY case A returns hard 0 (elevation out of [-45,+30] cone). caseB always returns >=1 once past step-6.
=> vis=0 mechanism = case A taken (param_6=1, [actr+6]==0) AND elevation out of cone (basis +0x18c..0x1ac
or biped+0x210 looking-vec divergence), OR param_8 shrinking local_c below dist (less likely at 3.78).

## param_4/6/7/8 PRODUCER PORTED-STATUS (coordinator follow-up answer)
ALL computed INSIDE unported 0x33440. Corruptible inputs:
- UNIT-OBJECT sourced (unported, outside ai.obj): [prop+0x127]=unitobj+0xb6>>2&1, [prop+0x132]=unitobj+0x6d>>0x13,
  [prop+0x131]=0.785398<(float)unitobj[0xcb]. Common-cause: [prop+0x127] feeds BOTH param_6 AND param_8.
- [prop+0x60] friendship: PORTED (0x40010/0x40280) but FAITHFUL (prior memory).
- [prop+0x12e],[prop+0x120],[prop+0x12c]: NO ported src writer -> unported create/perception-event.
- PORTED AI alertness/state (ONLY ported lift-bug candidates, large state machines NOT localized writer-input lifts):
  [actor+0x6a] (actions.c 827/1099/1399 set =3, actors.c 6316/6324/8021/8026 set 0/2; 0x3d3d0 dormancy) -> feeds param_6 AND param_8.
  [actor+0x6e] (actions.c:1101=2, actors.c:5383=0) -> feeds param_8 (0x2f380).
  [prop+0x66] unit_effect (actors.c:6351) -> feeds param_8 (0x2f380).
NO single ported writer-input mis-lift exists; only ported candidates are AI-state setters whose
divergence = upstream alertness desync, not a static writer-input defect.

## LIVE WATCH LIST (no :1234) — read on BOTH builds at divergent a6 tick, priority order
1. [actor+0x6a] (short alertness) — does it differ? (patched>=3 -> param_6=0 caseB returns 3; <3 -> caseA can return 0). PORTED.
2. [prop+0x127] (byte) + [prop+0x12c] (byte) — feed param_6=1 (caseA) + 0x2f380 return-3. unit-object sourced.
3. param_6 value = whether caseA(1) vs caseB(0) — the actual divergence point.
4. basis actor+0x18c..0x1ac (caseA elevation) + source biped+0x210 (looking vector) — UNCONFIRMED, the M3 root.
5. [prop+0x66] (unit_effect short), [actor+0x6e] (short) — feed param_8.
NOTE: writer debug-record (+0x656c/+0x6570/+0x6574) base DAT_00331f58=0 statically (runtime/debug-only) — may be unusable.

## VERDICT
Every ported lever on the writer-input path is byte-faithful. Per deterministic-core-replay
theorem a mis-lift must exist on the path, BUT it is NOT in any writer-input producer audited here.
The unchecked surface is (a) the 0x33440 literal-0 branch GATES (unit-object + squad flags), and
(b) whether the writer even returns 0. CANNOT be resolved statically — needs the live oracle:
at the divergent tick on BOTH builds, diff at the 0x314f0 call site: is-writer-reached, return value,
all 8 args (param_4/6/7/8 unconfirmed), gate flags cVar7/local_c/prop+0x127/0x131/0x132, actor+6, basis.
Consistent with resolved memory "a10 grunt won't fire — NO LIFT DEFECT on this path; prior repro contaminated".

## 2026-06-28 RUNTIME BREAKTHROUGH RECONCILED w/ checkpoint-8 root (SUPERSEDES late-snapshot framing)
Coordinator runtime (matched a6, los=0, in-cone, dist~3.8): default orphan prop+0x24=5, aw+0x268=3,
actor+0x6e=1, accum prop+0x2c=0.0, PASSIVE; cachebeta prop+0x24=3, aw=10, +0x6e=7, accum=1.0, ENGAGES.
actor+0x6a=3 BOTH (ruled out). vis=0 is a CONTROL-FLOW outcome: writer 0x33d45 runs iff prop+0x24<4||>5;
default state=5 -> writer never runs -> vis pinned 0.
- AWARENESS 0x300b0 is UNPORTED + a PURE SWITCH on orphan prop+0x24 (state5 -> aw=4-([prop+0xbb]!=0)=3/4;
  state2/3 + [prop+0x32]>=2 -> aw=10). => aw 3-vs-10 is 100% a faithful readout of prop state 5-vs-3. #2 collapses into #1.
- prop+0x24 state machine = UNPORTED 0x355f0 (per-tick): case0 `0<prop+0x30`->st1; case1 accum prop+0x2c +=
  rate[DAT_00255f30[percep+knowledge*4]]; accum>=1.0 -> st3. State 5 reached only via st4 aging (case4/5
  increments prop+0x3c past threshold). So default prop went 0->..->4->5 (engaged then aged), a SELF-LOCKING
  ratchet — NOT a never-promote. The late snapshot is a CONSEQUENCE; cause is in first few ticks.
- 0x3d430 actor_handle_unit_effect (PORTED): +0x66/+0x68 store + case gates re-audited vs disasm -> FAITHFUL
  (+0x68 = unit_effect==3?0x96:0x1e matches `((p3!=3)-1&0x78)+0x1e`; case1/2 gate local_5&&0x133 equiv to early-returns).

PRIOR HIGH-CONFIDENCE ROOT (project_a10_earliest_divergent_field.md checkpoint 8, FINAL) EXPLAINS THIS:
broken build creates 3 EXTRA perception props (objects e2a50036/e2a30034/e2b50046, non-actors=allied
marines/gunfire sources, st24=0 dist10-13u) ABSENT in fixed/unp, REPLICATED across all 3 focus grunts 6/10/11
(=> born divergence). Extra props crowd the chain / consume prop budget+priority -> PLAYER prop pushed to
chain tail, never promotes to active-3 (ally props DO promote -> machinery works -> PLAYER-PROP-SPECIFIC).
TOGGLE-PROVEN: alloff (12 AI objs on ORIGINAL code) FIXED, full (lifted) BROKEN, rest of build byte-identical
=> lifted bug INSIDE one of the 12 AI objects. prop-create primitive 0x64b40 + audibility 0x31850 + promote
0x33330 UNPORTED (faithful given input); corrupt INPUT = over-broad object/scan DECISION by a PORTED fn.
Exact culprit NOT pinned within ~25-call budget. FUN_00064400 (chain unlink) disasm-FAITHFUL.

CONVERGENT NEXT (advisor + prior BEST-NEXT, do NOT blind-walk the tree again): take coordinator noise-free
TOGGLE-BISECT. Recorded split plan in checkpoint 9: SplitA ported=false {props,actor_perception,actor_combat,
ai_communication,actor_moving,path} vs SplitB {encounters,actions,ai}, keep {actors,actor_looking,ai_profile} on;
then per-object then per-fn binary split; RECORD each config's ported=false list + dump filename in committed
artifacts/ai_regression/bisect_configs.json (the lost "3b" config is why naming stalled before). Ranked ai.obj
prior (treat as prior NOT result, prior bisect repro-voided): 0x3f5f0 > 0x413c0 > 0x40570/0x41420.
Live watch to confirm root directly (no toggle): per focus grunt, walk chain (actor+0x50 head, prop+0x8 next),
COUNT props with prop+0x18=non-player non-actor obj at dist>8u + prop+0x24==0; >=2 = BROKEN, 0 = FIXED (/tmp/chainstate.py).

## 2026-06-29 props.obj-10 EXHAUSTIVE AUDIT (B1 vs B2 for a6 +0x270 -> wrong prop) -> CLEAN NEGATIVE
Coordinator tightened: prop populations IDENTICAL both builds (182 props, same aggregate state dist, other
grunts' player-props DO reach s3/v3) => a6-SPECIFIC. default a6 actor+0x270 = ORPHAN prop state5 w/ parent
idx0x11(itself state0); cachebeta = SINGLE prop parent=-1 state3 vis3. Only PORTED code in create/promote/
linkage path = props.obj 10 fns. AUDITED ALL vs disasm:
- 0x64100 prop_data alloc / 0x64140 noop / 0x64150 dispose / 0x64160 clear — trivial, faithful.
- 0x64400 prop_remove_from_actor (@<eax> actor,@<edi> prop): singly-linked unlink of actor+0x50 chain via
  [prop+8] next. Re-traced EAX-at-loop-exit = REMOVED node's record (datum_get(next_handle)==datum_get(prop)),
  so splice `prev->next = removed->next` CORRECT. C props.c:101 matches disasm 0x64400-0x6453a EXACTLY. FAITHFUL.
- 0x64540 prop_iterator_new (out[1]=actor+0x50) / 0x64570 prop_iterator_next (iter[0]=cur, iter[1]=[prop+8])
  — faithful, the chain walker 0x355f0 uses.
- 0x64a80 prop_detach = 0x64400 + datum free. faithful.
- 0x64ab0 prop_get_active_by_unit_index (THE B2 selector, advisor-flagged): walks actor+0x50 chain, returns
  first prop with (state>=2 OR state<0) AND ([prop+0x18]==obj OR ([prop+0x14]!=0 && [prop+0x1c]==target!=-1)).
  STATE>=2 FILTER (CMP DX,1 JLE continue; TEST DX,DX JL bypass) is the BINARY's own behavior. C props.c:288
  `if(state>=0 && state<=1) continue` + object/indirect match + return cur_handle / -1 = matches disasm
  0x64ab0-0x64b3f EXACTLY. FAITHFUL. On AI path (callers 0x32380 etc.) used as a QUERY ("active prop exists
  for obj?"), NOT a +0x270 writer; the 0x32380 sVar5==1/2/3 path = scripted-look/projectile/vehicle, off the
  a6 normal-player promotion path.
- 0x64ee0 = libtiff TIFFClose, MIS-GROUPED, irrelevant.
NONE of the 10 writes actor+0x270 or sets/clears [prop+0xc] parent. +0x270 written by UNPORTED 0x303f0
actor_situation_update; +0xc parent + state by UNPORTED 0x33330/0x355f0. => NO PORTED defect on the
+0x270/+0xc/chain SELECTION-or-LINKAGE path predicts the a6 flip. Verdict for coordinator: clean negative on
props.obj => escalate to captured-state dual-oracle on a6 (B1: a ported fn feeds a divergent INPUT to the
faithful unported promotion machine 0x355f0 — the accum prop+0x2c never reaching 1.0 / state aging 4->5 —
must be driven by a divergent per-tick input produced upstream, NOT a props.obj linkage bug).
