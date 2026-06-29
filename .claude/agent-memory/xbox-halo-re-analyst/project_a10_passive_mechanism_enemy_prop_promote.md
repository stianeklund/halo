---
name: a10-passive-mechanism-enemy-prop-promote
description: a10 grunt passive — REAL mechanism = patched build is slow/partial/per-grunt-unreliable at PROMOTING the enemy(player) perception prop (st24 0->3->5). FUN_0003f5f0 + junk-prop theory BOTH refuted with evidence. Graded timing bug, not binary.
metadata:
  type: project
---

# a10 grunt passive — mechanism nailed, two prior leads refuted (2026-06-26)

> ## ✅ CONFIRMED CULPRIT (2026-06-26, late) — actor_moving.obj is the SOLE bug (ONE bug). Run H vindicated.
> Clean whole-object toggle ladder with the reliable engage oracle (`/tmp/ab_engage.py`, door grunts
> 6/10/11 enc `0xee6e0009`, user visual + memory trace):
> - **full-patched** = BROKEN (guard).
> - **alloff** (all 13 AI reverted) = FIXED (aw→10, mode→3, advance ~1.7u).
> - **half1** (behavioral core reverted, moving+path patched) = BROKEN: perception WORKS (st24→5, 6a→fight)
>   but mode stuck=1, aw plateau=3, no advance. → perception is NOT the gate.
> - **h1+moving+path** = FIXED (aw→10, mode→3, moved 1.7u).
> - **moving_only** (ONLY actor_moving reverted; perception/path/everything else PATCHED) = **FIXED**.
> ⟹ **actor_moving.obj alone is necessary+sufficient. ONE bug.** The earlier "perception-promotion /
> st24 ladder" framing was a **graded-capture artifact** (full-patched st24=0 came from a different probe
> catching pre-promotion/de-escalated moments; with a sustained capture st24 promotes fine in patched).
> actor_moving was wrongly "exonerated" mid-session because the graded perception reads masked it.
> **Mechanism:** patched actor_moving makes the grunt reach fight (6a=3) but FAIL the firing-position
> move-commit (mode never →3, aw plateaus 3) → the `actor_move_to_firing_position` 0x2d900 →
> `actor_path_refresh` 0x2cdb0 chain that the (patched, working) actor_looking selection depends on.
> Codex/handover had the RIGHT function; static-buffer was the wrong SPECIFIC fix. NEXT: pin the exact
> actor_moving fn + the real bug (clang/x87 or logic divergence in path_refresh return). NOTE function-level
> toggles of the path cluster HANG (boundary loop) — prefer audit/clang-vs-orig differential.
> Everything below ("perception-promotion is THE defect") is SUPERSEDED — kept as history.

> **RE-CONFIRMED 2026-06-26 by direct discrimination (`/tmp/discriminate.py`).** Player walked
> INTO door grunts 6/10/11 (pd_min 3.3–5.4u), player-prop present throughout, yet `st24`=0
> (never ≥3), `aw`=0, never fight, mode=1, moved 0u. A grunt at aw=0/alert/mode-1 never reaches
> the combat-movement code, so **actor_moving is EXONERATED** and the whole "actor_moving culprit
> / Run H" line (in `docs/handover-a10-grunt-passive-actor-moving.md`) was graded-symptom toggle
> noise — now collapsed to history. **THE defect = whatever promotes the player perception prop's
> `st24` (prop+0x24) from 0→3→5.** This file is the authoritative live record.

## SETTLED A/B (2026-06-26) — controlled patched-vs-alloff on the SAME door grunts
Tool `/tmp/ab_engage.py`; door grunts = slots 6/10/11, **encounter `0xee6e0009`** (the player-prop
carriers near the doorway). NOTE the other engaged grunts 2/4/7/8 are a DIFFERENT squad (encounter
`0xee690004`) that engages via an encounter-scripted path with NO player-prop and is engaged in BOTH
builds — they are NOT the regression; reading them is what caused a transient "perception flat in
both / combat-decision" misread. Match door grunts by player-prop presence + pd~4-5u + encounter
`0xee6e0009`, not by slot.

| field | ALLOFF (original) | PATCHED (broken) |
|---|---|---|
| st24 (+0x24) | **4–5** | **0** |
| ack (+0x66)  | **3**   | **−1** |
| aw268 (+0x268)| **3–7** | **0** |
| 6a action    | reaches **3 (fight)** | **2 (alert)** |
| mode 0x46c   | reaches **3** | **1** |
| vis +0x9c    | 0 (same) | 0 |
| aud +0xbc    | −7.92 (same) | −8.04 |

**First divergent field = `st24`/`ack` promotion. `vis`/`aud` (raw stimulus inputs) are IDENTICAL in
both builds** → the inputs are fine; the PROMOTION DECISION that turns inputs into st24/awareness
diverges. aw/6a/mode all follow st24. Vision is NOT involved (vis=0 in both; engagement is the
non-vision awareness/audio path). Prime suspect = ported perception scorer `0x2fd10`
(compute_prop_weight, actor_perception.obj) feeding the faithful unported st24 writers
(0x33330/0x355f0). NEXT (in progress): revert actor_perception.obj ALONE → if st24 lifts on door
grunts, object confirmed; else escalate to actors.obj/actor_combat.obj.

**SUPERSEDES** [[a10-passive-culprit-ai-obj]] (FUN_0003f5f0 culprit) and the junk-prop
root in [[a10-earliest-divergent-field]] Checkpoint 8. Both refuted with hard evidence below.

## REFUTED fix attempt (2026-06-26) — do NOT retry
`actor_path_refresh` (0x2cdb0) had a `static char large_buf[0x1408c]` (82,060-byte path
scratch). Codex hypothesized the `static` shared stale path state across actors → spurious
path_found=0 → firing-position selector rejects positions. Made it a plain stack-local
(matches the original MSVC frame), **deployed (clang obj rebuilt + XBE deploy-verified, box
alive/AI ticking) and door-walked → REFUTED.** Door grunts 6/10/11 at player-dist 4–5u were
unchanged-broken: `aw_max=3` (orig=10), `moved≈0u`, mode `0x46c` flicker 3→1, path-active
`0x4a8`=0, **at-dest `0x484`=1 (always)**, gate `0x504`=0. Buffer is init'd before read, so
storage class is behaviorally moot. Change stashed; tree at clean HEAD. Full record:
`docs/handover-a10-grunt-passive-actor-moving.md` (Refuted fix attempts).
**Cross-theory note:** this run's `aw_max=3` (never 10) and pinned `0x484`=1 are consistent
with BOTH live theories — perception-promotion (this file: grunt never gets st24>=3/aw=10)
AND actor_moving (handover: path_refresh early-returns "at dest" so the brief mode-3 never
sticks). Neither is refuted by this attempt; the perception ladder (st24) was NOT re-measured
this run — re-run /tmp/promote_diff.py alongside /tmp/path_probe.py to discriminate next time.

## What the bug ACTUALLY is (deterministic, field-oracle proven)
The patched build is **slower / partial / per-grunt-unreliable at PROMOTING the enemy
(player) perception prop**. Promotion ladder: prop+0x24 (st24) goes 0 -> 3 (aware/engage)
-> 5 (acknowledged enemy), and prop+0x66 (ack) -1 -> 3. Grunt engages (actor+0x268 aware
high, action+0x6a=3 fight) once its PLAYER prop reaches st24>=3.
- Original (alloff_fire6, all 13 AI ported=false): player prop **st24=5, ack66=3** — reliably acknowledged.
- Patched full_fire6 (early): player prop **st24=0, ack66=-1** — not promoted.
- Patched LIVE (later moment): grunts 6,10 player prop **st24=3, aw268=10, 6a=3** (engaged);
  grunts 0,1 player prop absent/st24=0, aw268=0 (passive). => SOME grunts engage eventually,
  others lag/never. Matches user "they attack eventually, but only one of them / not as fast."
It is a GRADED TIMING/reliability regression, NOT a binary failure. That is why every prior
binary toggle-bisect was noise (uncontrolled game state + graded symptom).

## Discriminator tooling (free, reusable)
- /tmp/promote_diff.py <suffix> — walks each team-3 grunt prop chain (head=actor+0x50,
  next=prop+0x8) and prints per prop: trk(+0x18), st24(+0x24), cat byte(+0x38), ack(+0x66),
  vis(+0x9c), aud(+0xbc float), g133(+0x133), dist(+0x11c). Prop valid check is len only —
  do NOT gate on u16(prop,0)==0 (that word is 0 on valid props; owner-actor handle is at prop+0x4).
- /tmp/oracle_summary.py — adjudicates ALL artifacts/ai_regression/*.bin by junk-count +
  enemy-ack + enemy-st24; runs the causality fork.
- /tmp/capture_live.py <tag> — XBDM:731 getmem of actor table (*0x6325a4, stride 0x724) and
  prop table (*0x5ab23c, stride 0x138) into actors_/props_<tag>.bin. Chunk getmem at 0x800.

## REFUTED lead 1: FUN_0003f5f0 (ai.obj activation sweep)
Disassembled OUR linked build (VA 0x4224e0, found by byte-pattern; halo.map is STALE) vs
original (delinked/ai.obj FUN_0003f5f0 @0x2a0). Instruction-for-instruction behaviorally
IDENTICAL: same global-rotation order/widths (movzx vs mov is inert; cached vs re-read global
inert), same iterator loop, same gates (record+0xb!=0 erase; record+0x6a>0 activate), same
handle iter+0x14, same cdecl cleanups. Activate target 0x421c70 reads handle from STACK [ebp+8]
(verified) so the @esi call delivers correctly. 88.4% VC71 gap = pure codegen. The prior
"FN_3f5f0_only=FIXED" bisect was a false positive of a binary oracle on a graded symptom.

## REFUTED lead 2: junk perception props (e2a/e2b "phantoms") / perception-scan
Causality fork over 40 captures DISSOCIATES: UNPATCHED reference unpA junk=9 / enemy ack=3,
unpB junk=7 / ack=3. The e2a50036/e2a30034/e2b50046/e2a70038 props (category prop+0x38==4) are
NORMAL — present in the original game too. They do NOT gate enemy acknowledge. Checkpoint-8
"junk only in broken" was an artifact of comparing against a junk-free alloff instant.

## Where the bug is (localized, not yet named)
Immediate writer of ack(+0x66)/+0x68 = actor_handle_unit_effect 0x3d430 (PORTED, actors.c:6327-6354);
its +0x66 write is FAITHFUL (gate prop+0x66==-1 passes if called) => bug is UPSTREAM in the
promotion DECISION. st24 writers 0x33330 become_acknowledged + 0x355f0 dispatcher are UNPORTED
(faithful given input). The PORTED perception scorer compute_prop_weight 0x2fd10 (actor_perception.c
~line 416-540: vision_level + awareness from prop fields, tag-dependent) and the per-tick
perception driver feed st24. Cluster to bisect: actor_perception.obj (12 ported) + actors.obj
perception fns + actor_combat.obj. NOTE captures are sound/fire-provoked (vis prop+0x9c=0 in
ALL); user repro is VISUAL sight-on-spawn — confirm which stimulus path before/after any fix.

## CONTROLLED A/B RESULT (2026-06-26) — REGRESSION CONFIRMED + reliable oracle
Same a10 checkpoint, same door-walk, only AI code differs (patched vs alloff). Tool:
/tmp/engage_timeseries.py (XBDM:731, per-grunt timeline). Both builds redeployed+verified.
- PATCHED (full ports): engaged grunts 6/10/11 -> aw268 PLATEAUS at 3, moved~=0.0u (HOLD position).
- AI-ORIGINAL (alloff): same grunts -> aw268 CLIMBS 3->10, moved~=1.6-1.7u (ADVANCE toward player).
- Both perceive at same time/range (~7.5s, pdist 4-6u). Divergence is POST-perception:
  awareness escalation 3->10 + advancing. THIS is the real, measured regression (overturns the
  handover "no defect"). RELIABLE BISECT ORACLE = max aw268 (>=6 original / <=3 patched) AND
  moved (>1u original / ~0 patched), read from a controlled door-walk capture.
prior prop-field discriminator (st24 0->5 / ack66) was a capture-timing artifact (advisor) — the
behavioral aw268-escalation + advance is the durable signal.

## BISECT RESULT (2026-06-26) — CULPRIT OBJECT = actor_moving.obj
Controlled toggle + door-walk per config (oracle: grunt-6 aw268 10=fixed/3=broken + moved):
RunA full=BROKEN; RunB alloff=FIXED; RunC CUT_PCM{perception,combat,moving,actors,ai}=FIXED;
RunD CUT_ACTORS{actors}=BROKEN; RunE CUT_TRIO{perception,combat,moving}=FIXED;
RunF CUT_PC{perception,combat}=BROKEN; RunH CUT_MOVING{actor_moving ALONE}=FIXED.
=> actor_moving.obj NECESSARY+SUFFICIENT. ai.obj/FUN_0003f5f0 EXONERATED. Bug is in one of
actor_moving.obj's 28 ported fns (src/halo/ai/actor_moving.c).
CAVEAT: PARTIAL function reverts of actor_moving can HANG (RunG reverted 12 high-level path fns
-> Halo pathfinding boundary infinite-loop; QMP running:true + sim static = game hang NOT gdb
break). Whole-object reverts stable. Function-localize via SOURCE AUDIT or revert-the-leaf-half
(loop-drivers stay patched). Full handover: docs/handover-a10-grunt-passive-actor-moving.md.
Suspects (engaged-but-won't-advance + aw plateau): actor_move_update 0x2e560,
actor_destination_update 0x2d350, actor_move_to_firing_position 0x2d900, actor_move_force_stop
0x2a860, actor_move_compute_facing 0x2daa0, actor_test_destination 0x2a580; leaf VC71-28.1
actor_move_get_avoidance_direction 0x2b5d0. NEXT: revert 16 leaf fns (staged in kb.json) to
bracket which half. Tools: /tmp/engage_timeseries.py, /tmp/fn_toggle_am.py, bisect_toggle.py.

## LATEST (2026-06-26 cont) — localized to actor_move_update 0x504 gate; compute_facing ELIMINATED
RunI(16 leaf reverted)=BROKEN; RunJ(16 leaf+6 HL-A path/dest reverted, 6 HL-B patched)=BROKEN
-> bug in HL-B move-exec. move_to_* faithful -> suspects compute_facing(0x2daa0)+move_update(0x2e560).
RunK(only move_update patched)=HANG. DISCRIMINATOR (/tmp/gate_probe.py, free read on live broken
build): engaged grunts (aw268=3) have actor+0x504==0 always, 0x506==0, move-vec@0x518==0. Since
move_update:3840 skips compute_facing when 0x504==0, COMPUTE_FACING ELIMINATED -> bug is move_update
wrongly leaving actor+0x504=0. 0x504=1 is set by actor_destination_update (2693,2811); RunJ had it
original+still broken -> our move_update clears 0x504. on-foot block 3698-3770 only clears 0x504;
keep-move path needs 0x15e<1 & 0x160==0 & 0x418==-1 & pending(0x6dc)!=1 & (0x15c==0||0x99!=0) &
0x6a0==0 & 0x360<1. Orig disasm matches source thru 3698; objdump dies at 0x45c0 (use capstone).
NEXT: /tmp/branch_probe.py reads decision fields on engaged grunts (1 door-walk) -> pin clear-branch
-> verify vs disasm -> fix. Game on stable patched build. Full: docs/handover-a10-grunt-passive-actor-moving.md.

## DECISIVE NEGATIVE (2026-06-26 cont) — actor_moving has NO wrong-offset write
Advisor: partial-revert bisect (I/J/K) = WEAKEST evidence; instruction-level disasm vs
original = STRONGEST; disasm wins. So "bug ∈ HL-B" is untrustworthy — only the WHOLE-OBJECT
Run H localizes. Findings (zero door-walks):
- move_update (0x2e560) hand-disassembled ENTIRE fn from pristine XBE (halo-patched/cachebeta.xbe
  has ORIGINAL bytes at 0x2e560, entry push ebp not JMP; delinked obj .text truncates at
  0x45c1=VA0x2e961 → on-foot block only in XBE). Vehicle(0x2e969)+on-foot(0x2eb77-0x2ed53)
  INSTRUCTION-EXACT vs source 3699-3727. FAITHFUL.
- move_to_firing_position (0x2d900) instruction-exact incl 0x4c/0x4a4 fast-path. FAITHFUL.
- STORE-OFFSET SWEEP all 28 fns (/tmp/offset_sweep2.py: lea-pointer-resolved write-disp sets,
  VC71 obj vs XBE): **NO orig-only displacement anywhere** = no missed write, no wrong-offset
  swap. ours-only flags (force_stop 0x504/0x6e0-8, path_refresh 0x488-490, dest_update
  0x4a8/0x50c-514, move_update 0x6ec-6f4) ALL confirmed faithful — original writes them via
  datum_get-return/add/lea bases the tracker can't follow (force_stop mov [edi+0x504],al@0x2a8b5).
- check_lift_hazards + buffer_alias_detector CLEAN on actor_moving.c.
=> wrong-offset-write hypothesis DEAD; region byte-diff no longer the right move. Open: (a)
logic/value bug (wrong const/branch/float) in the other 26 fns — but those govern HOW not
WHETHER a grunt moves; (b) Run H is graded noise & actor_moving exonerated (aw268 is the real
regression, written by UNPORTED 0x300b0 from prop pstate — perception, not movement). CAUSAL
DIRECTION unestablished. NEXT: /tmp/causal_probe.py — 1 door-walk on CURRENT broken build
captures perception(pst/ack)+awareness(aw268/6a)+decision(0x46c mode)+gate(0x504)+advance per
grunt → fixes causal order (decide-then-fail-gate vs never-decide). Then re-confirm Run H/A.

## FINAL STATE 2026-06-26 — Run H reconfirmed; failing step = FP SELECTION (full handover rewritten)
Canonical doc: docs/handover-a10-grunt-passive-actor-moving.md (REWRITTEN clean+correct for reviewers).
- Run H (revert whole actor_moving) reproducibly FIXED (advance 1.7-1.8u, aw->10, mode->3); full
  patched BROKEN (mode stuck 1, no advance). Toggle proven live via verify_toggles_live.py.
- CAUSAL ORDER (solid): awareness is DOWNSTREAM. Timeline: mode 1->3 commit @t=10.05 while aw STILL 3;
  aw->10 only @10.41 AFTER commit+advance. Perception fine on BOTH (pst=5/ack=3).
- Failing step = FIRING-POSITION SELECTION returns -1 -> behavior 0xc0 never =3 -> mode stuck 1.
  Chain: dispatcher action_guard.c (actor_looking.c:2348 case3, needs 0xc0==3) <- 0xc0=3 @2254 iff
  result!=-1 <- result=FUN_000272d0(FUN_00025c10(...)). FUN_00025c10/272d0/24cf0 are actor_looking
  (never reverted) but reproduce via actor_moving CALLEES: actor_path_input_new, move_to_firing_position
  ->actor_path_refresh. That's why Run H fixes it w/ actor_looking patched.
- CORRECTION: broken steady state (0x46c=1,0x4a8=0,0x484=1) is path_refresh EARLY-RETURN (move_src∈{0,1}
  ->return1,0x4a8=0,0x484=1), NOT "pathfinder FUN_0005ff70 failed" (that claim was WRONG/unverified).
- "Faithful" is SCOPED to source+VC71 codegen for ~10 of 28 fns checked (move_update, move_to_firing_position,
  path_3d_available, destination_update loop, test_destination, force_stop, get_stopping_distances,
  path_input_new, path_refresh entry+build-mechanics; offset sweep all 28). Deployed binary is CLANG, NOT verified.
  Defect = {unchecked ~18 fns} ∪ {clang/x87 float divergence}. NOT a paradox — Run H is ground truth.
- UNVERIFIED, in scope: path_refresh move_src==3 destination resolution (2165-2395); FUN_0002a3a0 (2458/2464);
  clang float codegen of any scoring/cost compare. compute_facing/FUN_0002b830 are behind 0x504 (never
  reached on broken) — confirm by call-graph, don't hand-read.
- NEXT (stop hand-scanning): (1) probe candidate-generation vs rejection in FUN_00025c10/272d0 on broken
  (1 door-walk); (2) function-bisect the unchecked half via WHOLE-group toggle (partial reverts HANG).
- Tools: /tmp/causal_probe.py, /tmp/path_probe.py, /tmp/offset_sweep2.py, /tmp/logic_diff2.py,
  artifacts/ai_regression/disasm_range.py, verify_toggles_live.py. kb=clean HEAD (BROKEN).

## FINAL 2026-06-26 (cont) — bisect EXHAUSTED, endgame = CLANG behavioral diff
- Failing step refined: selection SUCCEEDS sometimes (causal_broken grunts 10/11 hit mode 3 briefly
  then revert to 1) => failure is SUSTAINING the move: path_refresh returns path_found=0 during the
  selector's test (FUN_000272d0->move_to_firing_position->path_refresh) => candidate rejected
  (actor+0x3b8=-1) => behavior 0xc0 never stays 3 => mode reverts to 1. Steady state (0x46c=1,0x4a8=0,
  0x484=1) is path_refresh's EARLY-RETURN, not a separate failure.
- VERIFIED FAITHFUL (source/VC71): move_update FULL (on-foot+vehicle+epilogue seed_fallback/store_prev —
  epilogue writes facing/jump/crouch/store_prev, NOT path-control), move_to_firing_position, path_refresh
  (entry+dest-resolution cases2/3/4+build+return), path_3d_available, destination_update loop,
  test_destination, force_stop, get_stopping_distances, path_input_new (VC71 AND CLANG obj). Offset
  sweep (VC71) clean. fpsel_probe: 0xaa re-eval DOES fire, 0x3b8 attempt always -1 at 55Hz (ambiguous —
  set/reset window sub-ms).
- BISECT BLOCKED: reverting move_update(0x2e560) ALONE = HANG (QMP running:true+sim static, confirmed
  game-hang not gdb). HL path-drivers can only be reverted as whole-object (Run H); no path-cluster fn
  isolable. CLANG offset sweep too noisy (clang addressing breaks lea-tracker).
- LEADING HYPOTHESIS: clang miscompile / VC71-vs-clang x87 float divergence in a source-faithful
  executing fn, tipping a borderline path-validity/A*-cost/distance compare. Deployed=clang, behavior
  NOT diff'd vs original. ENDGAME (handover §8): equivalence harness (unicorn_diff clang obj vs delinked,
  --allow-stubs --float-tolerance --mem-trace --state-snapshot) on path_refresh/path_input_new/test_dest;
  OR dual-oracle on live grunt (orig 0x2cdb0 vs clang); OR manual clang FCOM/FUCOMI diff in LAB_check_dest
  (2279-2434) + path_3d_available cone. NOT more source-scan or toggle. kb=clean HEAD (stable BROKEN).

## Next experiment (needs CONTROLLED repeatable state — the missing ingredient)
Timing bug => bisect needs a fixed start state. xbox_pad.py virtual controller (TCP :27000,
Windows/ViGEmBus) IS reachable => can drive the door-walk repro programmatically. Plan: controlled
start -> drive through door -> capture at fixed t-after-spawn -> field oracle (player prop st24
rise-time) -> targeted toggle bisect of the perception cluster. Verify any fix against the actual
door-walk repro, not just the fire-state metric.
