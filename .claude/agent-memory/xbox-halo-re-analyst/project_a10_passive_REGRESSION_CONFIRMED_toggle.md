---
name: a10-passive-REGRESSION-CONFIRMED-toggle
description: a10 grunt passive bug CONFIRMED REAL via time-locked toggle A/B — full-patched AI stuck in idle (st2/act2/aw0/pp0), all-AI-off escalates out of it to aw3/st3/act6; overturns prior "no defect" verdict
metadata:
  type: project
---

# a10 grunt passive-engage: REGRESSION CONFIRMED (2026-06-24)

**Overturns** the handover `docs/handover-a10-grunt-aim-passive.md` "NO DEFECT FOUND / faithful" verdict.
Prior verdict failed because every reading was patched-only and the instrument was too slow
(cap2/cap3 spawned a subprocess + read whole tables per sample → missed the 1-5s transient).

## The decisive A/B (time-locked, capfast.py, ~100 samples/sec, persistent XBDM socket)

Same map/checkpoint (a10 Impossible, init.txt core_load_at_startup), same 3 close grunts at 4-5u,
matched pure-proximity provocation. Build diff = the 4 AI objects toggled via /tmp/setswath.py.

| Build | close-grunt state over 25-32s | verdict |
|---|---|---|
| **all-ON (full-patched, 314 AI fns)** | FROZEN at `st2 act2 aw0 pp0` — 2/2 runs, **including a shot** | never escalates |
| **all-OFF (4 AI objs ported=false)** | transitions `st2/act2/aw0/pp0` (idle) → `aw0/pp5` (prop orphan-5) → `st3/act6/aw0` (waking) → `st3/act6/aw3/pp5` (aware) | escalates out of idle |

KEY: the all-OFF *idle* state (`st2 act2 aw0 pp0`) is **identical** to the all-ON *perpetual* state.
all-ON grunts are stuck in the exact state all-OFF grunts pass THROUGH on the way to awareness.
=> full-patched AI fails to escalate the PLAYER out of idle. (Ally perception works in both:
all-ON grunts hold state-3 props on nearby allies at d~1.2; only the player prop stays state-0.)

Fields (actor elem stride 0x724, prop elem 0x138): st=actor+0x6a, act=+0x6c, agg=+0x6e,
aw=actor+0x268, player-prop state=prop+0x24 (walk chain head@+0x50, next@+0x8, tracked unit@+0x18==player_unit_handle).
Player unit handle from *0x5aa6d4 → +0x38 → +0x34.

## Confound discharged
Advisor flagged: my FIRST all-off baseline was pre-alerted (aw0=5 at sample 0). FIXED by the
cold-start run that captured the full idle→aware climb in one window. all-OFF *visits* the active
state under the same conditions all-ON is held in idle; all-ON got MORE provocation (proximity+shot)
and stayed idle. Time/provocation is NOT the differentiator — the build is.

## Where the culprit is
- ai_profile.obj = 46/46 ON in BOTH configs → NOT the culprit (constant across the A/B).
- All perception DECIDERS (0x300b0, 0x302b0, promote 0x33330, demote 0x32c10, sensory
  0x31df0/0x31a90) are UNPORTED in both builds → run identical original code → not a static mis-lift there.
- Therefore a PORTED input-producer in one of {actors.obj, actor_looking.obj, ai.obj, actions.obj}
  feeds the perception tick bad data. Toggling those 4 OFF restores escalation; ON breaks it.
- See [[a10-passive-alertness-root-traced]] (static audit found all ported chain fns "faithful" —
  but the empirical toggle outranks that; expect a subtle arg-misgroup / aliasing / reg class bug).

## Tooling
- /tmp/capfast.py <tag> <dur> — the validated instrument. Start FIRST (caches close grunts once),
  then walk in. Analyze with `grep -oE "g6:st.. act.. aw.. pp." | sort | uniq -c` to see state histogram.
- /tmp/setswath.py <obj.obj ...> — restore all-ON from /tmp/kb_bisect_backup.json then flip NAMED off; 0 args = all-ON.
- Build: rtk python3 tools/build/build.py -q  → halo-patched/default.xbe
- Deploy: rtk python3 tools/xbox/deploy_xbox.py --xbox 127.0.0.1 --xbe-only (magicboots to checkpoint)

## ⚠️ BISECT INVALIDATED — all-ON escalates now (2026-06-25, post-compaction)
**Negative control was never re-established this session.** 6 bisect configs (no_al, R1-R5) all
showed ESCALATED=YES; ZERO freezes observed. The only freeze on record (all-ON 2/2) is from the
PRE-COMPACTION session. The tell: R5 narrowed to pair {0x1abd0,0x1ac00}, but BOTH are byte-faithful
vs delinked disasm (0x1abd0: `mov [eax+0xe4],-1`; 0x1ac00: full flee/idle/mount logic — exact match).
Faithful fns CANNOT change behavior when toggled (ported=false routes to original == our impl) → the
toggles were never causing the escalations.
**Decisive: rebuilt TRUE all-ON (kb data == committed HEAD, verified), redeployed, same walk-in →
grunts escalate AGGRESSIVELY: g6 aw6/act11/agg7, g10 aw10/act8/agg7, g11 aw6/act8.** Far past the
bisect rounds' st3/act6/aw3. all-ON does NOT freeze. Idle-duration also swung 285→1207 samples across
rounds with no relation to toggles = walk-in timing/geometry variance.
=> The earlier "regression CONFIRMED" verdict is RETRACTED. No deterministic build-dependent freeze.
Next: 1-2 more all-ON controls to test if freeze is STOCHASTIC (sometimes) vs never. If never →
prior-session freeze was a runtime-state/measurement artifact; original passivity may be faithful
(re-instates [[reference_a10_grunt_aim_faithful_no_defect]]). If stochastic → characterize the
runtime trigger (NOT via build bisect).

## ENCOUNTER GEOMETRY (why the captures were confounded) — user-supplied 2026-06-25
The a10 hallway grunts SPAWN on a trigger: player walks through the FIRST DOORWAY → weapon
equips AND the hallway grunts spawn simultaneously. In bisect rounds no_al/R1-R5 the user said
"in range" right at that doorway and did NOT advance further → every capture read freshly-spawned
grunts at the SAME fixed distance with no added stimulus → identical aw3 ramp regardless of build.
Only the all-ON control walk had the user advance+hold → aw10/agg7. So provocation (player advance),
not the toggle, drove every result. capfast caches grunt elem addrs ONCE at window start; starting
the capture AT the spawn trigger risks reading spawn-init default state (st2/act2/aw0/pp0) — the
likely source of the pre-compaction "freeze". PROTOCOL FIX: start capture, walk through door to
spawn grunts, let them init ~2s, THEN advance steadily and hold; capture the full approach.

## REAL SYMPTOM = MOVEMENT, not awareness (2026-06-25) — /tmp/capmove.py
User clarified the actual bug: in STOCK a10, hallway grunts IMMEDIATELY relocate toward their
target area (2nd door "small no glass" mach @0x8011E4A4 handle 0xE41C018A) and engage aggressively.
Our build: aware but WON'T advance. I'd been measuring awareness (aw/st/act/pp) — wrong dimension;
those escalate fine everywhere. Movement is the discriminator AND is timing-robust (integral over
window, player held still = no provocation confound).
/tmp/capmove.py: resolves target-door obj pos, caches close team-3 grunts, tracks net_disp/path_len/
dist-to-target over window. Geometry: player x≈-7.8, target door x≈-11.0, grunts spawn x≈-11.8..-13.1
(far side of door, down the hall); "advance" = move up toward player/door.
**all-ON, player HOLDING at doorway, 30s/3278 samples: all 3 grunts STATIC** (net_disp≤0.04,
path_len≤0.09, dist-to-target closed 0.0) while reaching aw3/act6. Reproduces the symptom cleanly.
NEXT (decisive): measure the OTHER arm under identical hold-at-doorway — TRUE original unpatched XBE
(gold standard; user has it) or all-off proxy (4 AI objs off via /tmp/setswath.py). If stock grunts
MOVE (path_len >> 1, close distance) while all-ON STATIC → confirmed build-dependent movement
regression (valid: player held still, autonomous movement). Then investigate advancement/firing-pos
system (0x24cf0 scorer cluster — "won't advance" maps here; movement-based bisect now legitimate).
Caveat: static-while-aware COULD be faithful cautious-grunt behavior — the stock arm settles it.

## STOCK BASELINE captured — spawn-transient (2026-06-25) — /tmp/capmove3.py
KEY measurement-fix (user insight): the relocation completes in <1s of spawn; capmove/capmove2
START AFTER the user confirms spawn → only ever saw the post-relocation STEADY STATE (grunts parked
at door, static), which looks identical in both builds. FIX: capmove3 scans the FULL team-3 actor
table every iteration, records each grunt at FIRST SIGHTING (=spawn pos), run it IN BACKGROUND
BEFORE the player walks through the first door. Protocol: reboot checkpoint → stand before 1st door
→ start capmove3 background (45s) → walk through door → hold.
STOCK cachebeta (deathless in init.txt, no console pause), player HELD at doorway:
- Hallway grunts **g6/g10/g11** spawn at t=5.2s near target door (dist 1.8-3.0, x≈-13, "further
  down the hall" past the door at x=-11), MOVE toward it (path 1.7-3.25, closed +0.2..+1.6),
  **peak_aw10 peak_act6, "arrived" (dist<3) within 0.2-1.0s.** = "engage quickly+aggressively".
- Deeper group g0/g1(50u) g2/g4(42u) g7/g8(32u): STATIC at aw0/act0-3 while player holds (react
  only to advance — a separate later encounter, NOT the spawn group).
=> DISCRIMINATOR is sharp: stock hallway grunts hit **aw10/act6 within ~1s**. Note the gross MOVE
is small (~2-3u, they spawn AT the door) — the prior "won't advance" framing is really about
ENGAGEMENT SPEED / awareness ceiling (aw10), not long-haul pathing.
NEXT: identical capmove3 protocol on OUR all-ON build (kb.json ported flags == HEAD, verified 0
diffs). Compare g6/g10/g11 peak_aw, peak_act, arrive-time. If ours caps below aw10 / is slower to
act6 → real engagement regression. If ours also hits aw10/act6 within ~1s → NO regression (re-instate
[[reference_a10_grunt_aim_faithful_no_defect]]).

## MATCHED A/B — capmove3 spawn-transient, STOCK vs OUR all-ON (2026-06-25)
Identical protocol both arms: reboot checkpoint → hold before 1st door → capmove3 bg(45s) →
walk through door → HOLD. deathless in init.txt. Our build = rev d8ad32eb, kb ported==HEAD.
Hallway grunts g6/g10/g11 (spawn near door x≈-13, dist 1.8-3.0):
| grunt | STOCK aw | OUR aw | STOCK path | OUR path | STOCK closed | OUR closed |
| g6  | **10** | **7** | 1.69 | 0.18 | +1.6 | +0.0 |
| g10 | **10** | **7** | 1.82 | 0.42 | +0.2 | -0.0 |
| g11 | **10** | **7** | 3.25 | 0.14 | +1.4 | +0.0 |
act6 reached in BOTH. Spawn-time aw: STOCK 3-5 (climbs→10), OURS 7 (STALLS at 7, never climbs).
=> SIGNATURE: our grunts spawn MORE aware (7) but FREEZE there; stock spawns less aware, rushes to
aw10 + makes the small reposition move to the door. The "stuck" field is **aw=actor+0x268 capping
at 7** (not st/act as the old invalid bisect assumed). 3/3 grunts both runs, matched, timing-robust.
TIMING NOT a confound: ours had 36s post-spawn (spawn t=8.9s, window 45s) and still capped aw7.
⚠️ n=1 per build. This encounter has KNOWN high run-to-run variance (idle-dur swung 285→1207 last
session). MUST replicate (≥2 runs/arm) before root-causing. Our build CAN reach aw10 when player
ADVANCES (prior all-ON advance run hit aw10/act11) — so the regression is specifically "fails to
autonomously escalate aw7→10 + reposition under player-HOLD", i.e. needs more provocation than stock.
Tooling: /tmp/capmove3.py <tag> <dur> [tgt_handle] — full-table team-3 scan, tracks each grunt from
first sighting; RUN IN BACKGROUND before walking through door. Outputs peak_aw/peak_act/path/arrive.

## ✅ REGRESSION CONFIRMED — controlled 2x2 with player instrumented (2026-06-25) /tmp/capmove4.py
Advisor bar MET: >=2 runs/arm, PLAYER LOGGED + matched, non-overlapping distributions.
Protocol all 4 runs: reboot checkpoint → hold before 1st door → capmove4 bg(45s) → walk through →
STOP + set controller down. Player path (hold-quality control): stock 1.62/0.85, ours 1.84/1.75 —
all genuine holds (~0.8-1.6u walk-in then still). Hallway grunts g6/g10/g11:
| arm | aw_traj (representative) | end aw | peak aw | movement |
| stock r1 | g6 0→3→10 ; g10 0→5→6→10→osc(9↔10) ; g11 0→5→6→10 | 10/10/10 | 10 | MOVED 1.7-2.1 |
| stock r2 | g6 3→10 ; g10/g11 5→6→10 | 10/10/10 | 10 | MOVED 1.7-3.0 |
| ours r1  | g6/g10/g11 7→3→0 | 0/0/0 | 7 | STATIC <0.6 |
| ours r2  | g6/g10/g11 3→0   | 0/0/0 | 3 | STATIC <0.6 |
**SIGNATURE: stock grunts BUILD awareness 0→10 and LOCK ON (osc 9↔10 = active target maintenance) +
reposition to door; OUR grunts perceive player at spawn (aw3-7) then DECAY to 0 (LOSE the player) +
sit.** Opposite directions, zero overlap on end-aw (10 vs 0), peak-aw (10 vs ≤7), and movement
(MOVED vs STATIC). aw = actor+0x268 (continuum/alertness; stock shows smooth 0→3→5→6→10 climb +
hi-lock oscillation — classic alertness accumulator). act6 reached in BOTH (so the broken thing is
specifically alertness MAINTENANCE/escalation, not initial threat-action).
=> This is a real build-dependent perception regression. The earlier "won't advance" framing is a
DOWNSTREAM symptom: grunts that lose awareness don't reposition. Movement follows awareness.
=> The metric (end-aw 0 vs 10, decay vs climb) is now player-CONTROLLED + reproducible, so a
toggle-bisect on THIS metric is finally LEGITIMATE (unlike the retracted bisect, whose aw-escalation
metric was player-driven). Perception DECIDERS (0x300b0/0x302b0/promote 0x33330/demote 0x32c10/sensory
0x31df0/0x31a90) UNPORTED in both → identical orig code → a PORTED input-producer in {actors,
actor_looking, ai, actions}.obj feeds them data that drives alertness DOWN. Find what writes/feeds
actor+0x268 decay. Supersedes [[reference_a10_grunt_aim_faithful_no_defect]] (that read aim, not aw-decay).

## 🎯 ROOT MECHANISM = premature PERCEPTION-PROP DELETION (2026-06-25) /tmp/capmove5.py
Prop-instrumented capture (advisor: instrument the prop, discriminate w/ data). prop_data table =
*(u32*)0x5ab23c (datum layout esize@0x22 stride 0x138 data@0x34). actor+0x270 = primary prop handle;
prop+0x18 = tracked unit (==player), prop+0x24 = prop state. OUR build, controlled hold:
g6/g10/g11 trajectory: **aw3/ps5/P → (prop slot freed, ph?1c/1b) → aw0/GONE**.
=> NOT failed sighting: the grunt DOES perceive the player (prop reaches state 5, tracks Player).
The prop datum is then FREED (u16(prop,0)==0) and actor+0x270 set to -1 → FUN_0003b410(...,-1)
(0x3b410 actor_replace_prop_reference) clears alertness +0x268 to 0. Stock KEEPS the prop → aw→10.
**Hypothesis A (prop removed) CONFIRMED over B (sighting starved).** A ported fn prematurely
deletes/orphans the player's perception prop.
Deletion primitive: prop_iterator_next (0x64570, props.c:259) → datum_delete(prop_data). Callers of
the -1/delete path: actor_delete_props (0x3cbc0, actors.c:5705) loops FUN_0003b410+prop_iterator_next
over actor+0x50 chain; encounters.c:3287 prop-cleanup loop. Also actor_perception.c:358-360 engage
evaluator "clears prop tracking fields when engagement drops" (engagement 0-3, actor_perception.c:118).
props.obj PORTED: 0x64100/64140/64150/64160/64400/64540/64570/64a80/64ab0/64ee0 (rest unported=orig).
NEXT: trace which ported fn deletes the player prop (disasm diff — static C reads have misled here
repeatedly). Sub-paths: A1 prop GC/aging timer (props.c lifecycle) vs A2 engagement-drop because a
ported LOS producer reports "lost target" (would implicate recent commits 2e4bb4ed FUN_00024cf0 LOS
+ d8ad32eb actor_looking.c:529 — advisor flagged as highest-prior; DON'T exonerate as "firing-pos
not perception"). Then ONE targeted toggle to confirm (prop-survival → aw→10 is now a clean metric).
Box state: default.xbe (rev d8ad32eb) currently booted; cachebeta.xbe also on box. capmove5 ready.

## REFINED mechanism = prop FRESHNESS-BUDGET drains, never refilled (2026-06-25) /tmp/capmove6.py
Added prop+0x66 (engagement) + prop+0x3a (budget) to instrument. OUR build, controlled hold,
g6/g10/g11 IDENTICAL detailed trajectory:
  spawn aw3/ps5/**eng3**/bud891/P → budget drains ~6-7/sample monotonic (≈1/tick PRIMARY rate) →
  at bud~754 **eng 3→-1** → budget keeps draining → bud2 → slot freed (ph?) → aw0/GONE.
=> Engagement DOES reach 3 (initial sighting/acknowledge works). The analyst's dump discriminator
"prop+0x66=3 unpatched vs -1 patched" was a SNAPSHOT-TIMING artifact of this drain (caught ours
post-decay). REAL mechanism: the prop's FRESHNESS BUDGET (prop+0x3a) drains 891→0 and is NEVER
REFILLED, so the prop EXPIRES and the unported selector deletes it. Grunt sees player ONCE (budget
891, eng3) but never RE-perceives → budget counts down → prop dies. Stock must keep re-seeing →
budget refilled → prop survives → aw→10. Bug = failed per-tick RE-perception / budget top-up.
Budget drains at ~1/tick (primary rate) so the player prop IS primary (+0x270=player throughout) —
it's not losing primary status; it's losing the SIGHTING REFRESH that tops up the budget.

## Analyst (xbox-halo-re-analyst) result: AI prop/perception path BYTE-FAITHFUL (2026-06-25)
Rigorous disasm diff of every PORTED node on deletion+selection path: all faithful. Deletion done by
UNPORTED code (selector 0x355f0, promote 0x33330, 0x34c80, 0x40700). Both recent commits CLEARED
(2e4bb4ed 0x24cf0 LOS correct vs disasm 0x2528c; d8ad32eb guard, actor+0x9e LOS=0 in BOTH builds).
Ported faithful: actor_get_perception_knowledge 0x2fc20, FUN_0002f380 engage-reader, acknowledge
0x2f2b0, actor_handle_unit_effect 0x3d430 (only ported writer of prop+0x66), 0x2f910,
actor_compute_prop_target_weight 0x2fd10 (FPU scorer). Concludes bug is UPSTREAM of AI (ported writer
of object/sensory state the unported sensory 0x31df0/0x31a90 read). Memory: reference_a10_prop_
deletion_path_faithful.md. CAVEAT (advisor): this is a STATIC "all faithful" negative — the result
type that's misled this investigation repeatedly. Converting to empirical via AI-off toggle.

## DECISIVE FORK IN PROGRESS: all-AI-ported-OFF (2026-06-25)
/tmp/set_ai_off.py off → 537 AI fns ported=false (12 objs: actions,actor_combat,actor_looking,
actor_moving,actor_perception,actors,ai,ai_communication,ai_profile,encounters,path,props). Backup
/tmp/kb_aitoggle_backup.json (==HEAD all-ON); `restore` to revert. Building+deploying. Measure with
capmove6 under controlled hold: budget refills/prop survives/aw→10 → bug IS ported AI (analyst's
faithful verdict wrong somewhere). Budget still drains/prop dies → upstream confirmed empirically.
MUST restore kb (set_ai_off.py restore) when done. Box: default.xbe currently our AI-on rev d8ad32eb.

## ✅✅ FORK RESULT: bug IS in ported AI (2026-06-25) — /tmp/aioff_fork.out
All 537 AI fns ported=false (set_ai_off.py off), built+deployed, controlled hold:
g6/g10/g11 → **peak_aw10, prop_removed=False** (props SURVIVE, aw climbs to 10 = STOCK behavior).
vs AI-ON (our_eng): peak_aw3, prop_removed=True (deleted at bud0). EMPIRICAL: flipping AI off FIXES,
on BREAKS → the regression is a PORTED AI function. **The analyst's static "all faithful" verdict is
WRONG somewhere** (advisor was right to demand the empirical toggle over the static negative).
Detail: in AI-OFF even at bud0 the prop is KEPT (g10/g11: aw10/ps3/eng-1/bud0/P, not removed); in
AI-ON the prop is DELETED at bud0. So ported AI deletes-at-budget-expiry where original keeps it.
prop+0x3a (budget) has NO ported writer (unported-managed, drains same in both builds) and prop+0x66
(engagement) only ported writer = actor_handle_unit_effect 0x3d430 (faithful) — so the divergent
thing is the DELETION DECISION fed by a ported producer, NOT the budget/engagement values.
Metric for bisect (clean, player-controlled): prop_removed True/False + peak_aw 0-vs-10.
NEXT: within-AI bisect via /tmp/set_ai_subset.py (keep named objs ON, rest OFF; restores from
/tmp/kb_aitoggle_backup.json). 12 objs. Prior order: perception/prop cluster first
{actor_perception,props,actors,encounters}. RESTORE kb at end: set_ai_off.py restore.

## WITHIN-AI BISECT (LEGITIMATE, clean metric) → actors.obj 4-fn INTERACTION (2026-06-25)
Metric: peak_aw 10=fixed(escalates+prop survives, ends aw10/ps3/eng-1/bud0/P) vs 3=broken
(g6 aw3+deleted, g10/g11 aw5). prop_removed flag UNRELIABLE (initial pre-spawn aw0/GONE scan trips
it — use peak_aw + trajectory END). Tools: /tmp/set_ai_subset.py (whole objs), /tmp/set_actors_range.py
(addr range within actors.obj), backup /tmp/kb_aitoggle_backup.json. Each round = rebuild+deploy+walk.
Chain (all vs cachebeta-style AI-off baseline = FIXED):
- R1 {actor_perception,props,actors,encounters} ON → BROKEN  => bug in this cluster
- R2 {actors,encounters} ON → BROKEN (identical) => perception/props innocent
- R3 {actors} ON → BROKEN => encounters innocent => bug in ACTORS.OBJ
- R4 actors[0x3ae60,0x3f030] ON → BROKEN ; R5 actors[0x3ae60,0x3c25f] ON → FIXED => bug in [0x3c260,0x3f030]
- R6 [0x3c260,0x3cf00] ON → FIXED => bug in [0x3cf10,0x3f030] (16fn)
- R7 [0x3cf10,0x3d950] ON → FIXED (0x3d430 engagement-writer INNOCENT) => bug in [0x3d9f0,0x3f030]
- R8 [0x3d9f0,0x3e7a0] ON (4fn: A=0x3d9f0 actor_pre_activate_check, B=0x3dc20 actor_input_update,
     C=0x3e570 actors_handle_unit_effect, D=0x3e7a0 actor_apply_control_data) → BROKEN
- R9 {C,D} ON → FIXED ; R10 {B} ON → FIXED ; R11 {A} ON → FIXED
=> **NO single fn breaks alone; {A,B,C,D} together BREAKS = INTERACTION bug** (likely ABI/@reg
mismatch between a ported caller+callee among these 4, or shared-state; correct when either is orig).
NEXT: find minimal breaking subset (test {A,B} etc.) OR read call-graph+@reg ABI of the 4 fns
(actors.c: 0x3d9f0@6232, 0x3dc20@6835, 0x3e570@7400, 0x3e7a0@7484). actor_activate 0x3ec80 (OFF in
all rounds=orig) calls A@0x3ecc3, B@0x3ec36, D. Suspect a ported A/B/D pair with reg-arg mismatch.
RESTORE kb at end: /tmp/set_ai_off.py restore (copies backup → kb.json, removes backup).

## REPLICATION (advisor: leaf bisect was n=1) — interaction CONFIRMED real (2026-06-25)
Clean binary metric = g6 final trajectory token: end /P (survived, aw10) = FIXED; end /GONE
(deleted) = BROKEN. g6 is the sensitive indicator (g10/g11 survive at aw5 even when broken).
- R8 (all 4 = A,B,C,D ON): BROKEN 3/3 (orig R8 + r8_rep1 + r8_rep2; g6 always /GONE; peak_aw 3/3/7).
- A=0x3d9f0 alone (R11): FIXED 2/2 (R11 + r11_rep2; g6 /P aw10).
=> R8 reliably broken AND A-alone reliably fixed → INTERACTION is REAL (needs ≥2 of the 4 ON).
Mechanistic: the 4 are all called by ORIGINAL actor_activate 0x3ec80 (cdecl A@0x3ecc3, B@0x3ecdc,
D@0x3ed9e via MOV EAX,ESI=@eax); NO call edges AMONG the 4. So interaction is via SHARED MUTABLE
GLOBAL state (not ABI, not call-edge). Decls: A char(int) cdecl; B=0x3dc20 void(int) cdecl;
C=0x3e570 actors_handle_unit_effect void(int unit,short effect,int) cdecl; D=0x3e7a0 void(int@eax).
Singletons still 1× each for B(R10 fixed)/C,D(R9 fixed) — confirm if pair-test ambiguous.
NEXT: read the 4 fn bodies (actors.c: A@6701, B@6929, C@7405, D@7539) for a GLOBAL that ≥2 write
divergently; OR test pair {A,B}=[0x3d9f0,0x3dc20] (both fixed alone → if broken = the pair).

## 🎯🎯 CULPRIT = FUN_0003dc20 / actor_input_update (0x3dc20) in actors.obj (2026-06-25)
The "interaction" was a FLUKE artifact of R10 (B-alone, 1 lucky FIXED). Replicating B-alone broke it.
COMPLETE pattern across all configs (g6 final-token metric):
- B=0x3dc20 ON → BROKEN: R1,R2,R3,R4,R8(×3),{A,B},b_alone_rep2 = 7/8 distinct configs
- B=0x3dc20 OFF → FIXED: R5,R6,R7,R9,R11(×2),aioff = 7/7
- Lone outlier: R10 (B-alone) FIXED once (the bug is probabilistic — raises deletion prob, not 100%;
  R10 caught the lucky survive). A-alone (B OFF) never broke 2/2.
=> **actor_input_update 0x3dc20 is the buggy ported function.** Decl: void FUN_0003dc20(int actor_handle)
cdecl. actors.c ~6929-7399. Called by actor_activate 0x3ec80 @0x3ecdc AND per-tick. Mechanism fit:
actor_input_update should refresh the actor's per-tick perception (re-acknowledge player → top up the
prop freshness budget prop+0x3a); our lift fails to → budget drains 891→0 → prop expires → deleted →
aw cleared to 0 → grunt never escalates/engages. Note bug is PROBABILISTIC (R10), so verify fix with
≥2 runs. NEXT: disasm-diff 0x3dc20 vs delinked ref → find lift bug (swapped branch / wrong offset /
missing write) → C89 fix via /lift → rebuild all-ON + fix → verify grunts reach aw10 ≥2 runs.
RESTORE baseline first: /tmp/set_ai_off.py restore.

## Bisect progress (2026-06-25) [INVALID — see retraction above]
Leave-one-out (others ON, one object OFF) + per-fn address-half bisect via /tmp/setfns.py
(restores /tmp/kb_bisect_backup.json all-ON, then flips named-object addrs in [start,end] OFF).
Oracle = capfast histogram: idle `st2/act2/aw0/pp0` vs aware `st3/act6/aw3/pp5`.

- **CULPRIT OBJECT = actor_looking.obj** (leave-one-out): actor_looking 0/120 OFF (actors/ai/actions
  ON) → grunts climb idle→aware (g6 628 idle→1955 aware etc). all-ON freezes. CONFIRMED.
- Round 1: actor_looking HIGH half (0x17940..0x2a430, 60 fns) OFF, LOW half + others ON →
  **ESCALATES** (g11 even hits pp3 active-threat) → culprit in HIGH half. (prior 0x24cf0 is in HIGH half.)
- Round 2 (in progress): HIGH-B quarter (0x1abd0..0x2a430, 30 fns, contains 0x24cf0) OFF.
  Quarters: HIGH-A 0x17940..0x1ab70, HIGH-B 0x1abd0..0x2a430.
- NOTE: 0x1f9a0 demote-caller is NOT in actor_looking (misremembered object); only valid in-obj prior = 0x24cf0.

- Rounds 2-4: HIGH-B (0x1abd0..0x2a430) OFF→escalate; B-A (0x1abd0..0x278e0) OFF→escalate;
  C1 (0x1abd0..0x25970, 8 fns) OFF→escalate. Culprit ∈ C1.
- C1 8 fns split by ROLE: idle-look pair {0x1abd0 clear look-at tgt actor+0xe4=-1, 0x1ac00 set
  up idle/wander look block — clears look-suppress flags 0x424-0x428/0x454, look-speed=4} vs
  6 firing-position scorers {0x24ca0,0x24cf0,0x25340,0x25510,0x257a0,0x25970}. Idle-look pair runs
  WHILE IDLE (our frozen state); firing scorers only run when already-engaged → idle-look pair is
  the logical culprit (and has been in the OFF set every round). Round 5 separates them.

## NEXT
Continue address-half bisect within HIGH half (~5 more rounds). When pinpointed: instrument the
candidate's OUTPUT field in all-ON vs candidate-OFF at matched d~5 to find the divergence (don't
re-audit for general correctness — it already passed a "faithful" read once). Restore all-ON (`python3 /tmp/setfns.py` no args) when done.
Cross-links: [[a10-passive-alertness-root-traced]] [[reference_a10_grunt_aim_faithful_no_defect]] (now superseded).
