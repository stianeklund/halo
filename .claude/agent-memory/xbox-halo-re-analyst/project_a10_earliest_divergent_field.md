---
name: a10-earliest-divergent-field
description: a10 grunt passive-engage — hunt for EARLIEST born-divergent actor/prop field (full vs alloff vs unp) and attribute to a ported writer. FP/SSE precision hypothesis REFUTED structurally (-mno-sse global).
metadata:
  type: project
---

# a10 grunt passive-engage: earliest divergent field hunt (2026-06-24/25)

Goal: find the most data-upstream field that differs (full vs alloff) AND agrees
(alloff vs unp_*) for the focus grunt, then attribute it to the PORTED function
that writes it. Iron constraint: divergence is BORN in a ported fn; unported code
is byte-identical → identical input ⇒ identical output.

## Orientation findings (checkpoint 1)

### FP-precision (SSE vs x87) hypothesis — REFUTED structurally
CMakeLists.txt:99 sets `-mno-sse -march=pentium3` GLOBALLY in CMAKE_C_FLAGS. No
per-file SSE override (only `set_source_files_properties` is GENERATED for
build_info.c, not flags). clang cannot emit SSE float ops with -mno-sse. So the
primary FP-precision mechanism (SSE vs x87) CANNOT be the cause. Any FP
divergence would have to be the rarer x87-internal kind (80-bit spill timing,
FPREM1 vs MSVC FPREM, control word, instr reordering changing intermediate
rounding) — secondary lens only. Dominant hypothesis shifts to logic-level born
divergence in a ported AI fn.

### kb.json schema gotcha
Real function DB is in `.objects[]` array (210 objects, each {name, functions[],
data[]}), NOT the top-level address-keyed entries (those are a small working set
of ~48). `rtk jq`/`rtk proxy jq`/plain `jq 'length'` all return 50 = the
top-level key count, MISLEADING. Use `jq '.objects[] ...'` and grep
`'"name":'` (515 fns total) for true counts. rtk truncates jq output here — use
plain jq on .objects.

### AI-domain objects WITH ported functions (attribution universe)
actors.obj 124/124, actor_looking.obj 120/120, encounters.obj 99/139,
ai_profile.obj 46/46, actions.obj 35/73, ai.obj 35/48, actor_moving.obj 28/29,
path.obj 13/41, actor_perception.obj 12/45, actor_combat.obj 10/19,
props.obj 10/35, ai_communication.obj 5/48, ai_debug.obj 9/73.
ALSO units.obj 204/204 (sensory input source likely). 
NOTE: prior audit said perception writers unported, but actor_perception.obj has
12 ported and props.obj 10 ported — born divergence could be one of THOSE.

### Method locked (per advisor)
- full↔alloff: 36 slots each, same session/checkpoint, deterministic spawn →
  VERIFY team(0x3e)+unit-type agree per slot, then SLOT-MATCH directly for the
  "differs" half. (Do NOT anchor on position — the bug corrupts movement.)
- alloff↔reference: state-match still↔unp_idle, fire↔unp_sustain (NOT 256-slot
  unpatched.bin). Identity-match via focus_probe logic (team3 + chain-tracks-player).
- "earliest" = TEMPORAL: walk still0→still2→fire0→fire6, record when each
  footprint field FIRST diverges. Field diverging at still0 is upstream of one
  that diverges only at fire6.
- Float lens INLINE: for each differing 4-byte field print int+float+|delta|.
  Tiny delta=precision; large=logic; differs-everywhere(fails agree-unp)=noise.
- Attribution discriminator: born-divergence writer must be PORTED and in one of
  the 12 toggled objects. If unported or outside → go upstream or report negative.
- disp-only [reg+disp] writer match can't tell actor_data/prop_data/0x44 buffer
  apart — trace base reg to its datum_get before believing a writer.

Dump slot counts: full/alloff = 36 actors / 147 props. unp_idle/unp_sustain = 34
actors / 157 props. unpatched.bin = 256/1181. Existing tools use range(256)/768 →
OVERRUN small dumps; must bound to file size.

## Checkpoint 2 — BEHAVIORAL footprint CONFIRMED, temporal signature is DECAY

Focus grunts = slots 6,10,11 in full/alloff (unit 0x18 IDENTICAL per slot
full<->alloff: e5570166/e55a016c/e55d016f → slot-match VALID). In unp_idle they
are slots 5,10,11 (unit e7xxxxxx, only gen-nibble differs).

Temporal sequence reveals the bug is a DECAY/timeout, not a never-promote:
- full(BROKEN) fire0: grunts PROMOTE 6a=3 6e=1 268aw=3, 610tgt==270foc set, prop st24=0
- full(BROKEN) fire6: grunts REGRESS 6a=2 6e=0 268aw=0, 270foc=-1 (LOST), 610tgt STALE, st24=0
- alloff(FIXED) fire0/fire6: 6a=3 6e=1 268aw=3 SUSTAINED, prop st24=5/4, 270foc set
- unp(REF) idle: 6a=3 268aw=6/10 SUSTAINED, prop st24=3/4, 270foc set

Behavioral footprint (FIXED & REF agree, BROKEN diverges):
| field | BROKEN full_fire6 | FIXED alloff_fire6 | REF unp_idle |
| actor 0x6a | 2 | 3 | 3 |
| actor 0x6e | 0 | 1 | 3 |
| actor 0x268 aware | 0 | 3 | 6 |
| prop 0x24 pstate | 0 stuck | 5/4 | 4/3 |
| actor 0x270 focus | -1 lost | set | set |

prop+0x24 and actor+0x268 are UNPORTED-written (0x300b0/0x302b0/promote 0x33330/
demote 0x32c10). So the EARLIEST born field must be UPSTREAM — likely a
TIMER/TIMESTAMP/last-seen field that the unported decay logic reads. BROKEN
acquires then forgets → the "last perceived tick" or "still-visible" input is
being written wrong by a ported fn, so the decay expires it.

Next: 3-way per-field footprint filter differs(full,alloff) AND agrees(alloff,unp),
focus grunt identity-matched, at fire0 (pre-decay) to find the earliest field.

## Checkpoint 3 — TIGHTENED filter (byte-equal agree) + INTERSECT across grunts

CRITICAL FILTER FIX (advisor): cat==2 means "all nonzero" NOT "alloff agrees unp".
Float "footprints" were FALSE POSITIVES. Real test: alloff BYTE-EQUAL unp (discrete)
OR same sentinel-cat(0/ffffffff). Then INTERSECT footprint offsets across grunts
6,10,11 (born divergence hits all 3 identically; drift hits one). FP/SSE hypothesis
now also REFUTED EMPIRICALLY: clean survivors are wholesale 0-vs-present / valid-vs-
invalid, NOT ULP deltas — except prop+0x0f0 (see below).

CLEAN survivors (byte-equal agree, all 3 grunts):
STILL2 (pre-combat, EARLIEST):
- actor +0x590: full=00000100 fixed=0 ref=0  (byte at +0x592 = 1; a flag set in BROKEN only)
- prop  +0x0f0: full=c0fc701c fixed=c0fd4337 ref=c0fd4337  FLOAT -7.888(broken)/-7.914(both fixed)
  *** alloff and unp BIT-IDENTICAL across independent runs => deterministic stored
      value; patched computes it slightly wrong (delta 0.026). STRONGEST attribution
      candidate — a stored input/param, not per-tick noise.
FIRE0:
- actor +0x1a0: full=present(-0.36..) fixed/ref=0  [byteq all grunts]
- actor +0x624: full=0002/00030000 fixed/ref=0  (s16 at 0x626 = 2/3 in broken only)
- prop  +0x060: full=00000001 fixed/ref=01000001 (byte +0x63 = 0 broken / 1 both fixed) FLAG
- prop  +0x100: full=e3f50007 fixed/ref=e3f50008  player-handle gen byte, run-specific, DISCOUNT

EARLIEST = still2 actor+0x590 (flag) and prop+0x0f0 (float param). Both pre-combat.
prop+0x0f0 sits in a 0x0bc-0x10c block that looks like point/vector data (values
-7.x, -2.x, 0.5, 0.96...). Need: confirm at still0, identify prop+0x0f0 field, find
ported writer.

## Checkpoint 4 — still-condition findings are CONFOUNDS; look-state EXONERATED

REVISED (advisor + coordinator + 2 controlled checks). The still-condition byte
footprint (0x590/0x591/0x1a0, prop+0x0f0) is NOT the born divergence — it is
uncontrolled look-aim/target-acquisition timing across independent runs:
- actor+0x591 is set in actor_looking.c:8993 (look_update, snap-to-look), gated by
  is_attacking + combat-target 0x610. It is a CONSEQUENCE of differing 0x610.
- 0x610 itself FAILS the footprint test: at still slot6, full=set AGREES with unp=set,
  DISAGREES with alloff=unset → target-acquisition timing, not a footprint.
- prop+0x0f0 byteq vanished at still0 → was a still2 prop-lifecycle/identity artifact.

CHECK 1 (grep): 0x590/0x591/0x598-0x5a0 are NOT read anywhere in the escalation/
awareness path (actor_perception.c, props.c, actor_combat.c, ai.c). Look-state
cannot cause the perception bug. RED HERRING confirmed.

CHECK 2 (controlled dump actors_actorlooking_fire6): focus grunts 6/10/11 show
6a=2/6e=0/268=0 = BROKEN signature (matches full, NOT alloff). So toggling
actor_looking does NOT fix it → ACTOR_LOOKING.OBJ EXONERATED as sole cause. Bug
needs the combination of ported AI objects (fix = all 12 off).

FP/SSE hypothesis: REFUTED (structural -mno-sse global; empirical categorical
0-vs-3 outcomes, no ULP pattern in clean survivors).

REAL TARGET (fire-conditional): grunts PROMOTE at fire0 (6a=3,268=3, 610==270 set)
then FAIL TO SUSTAIN by fire6 (6a=2,268=0,270 lost) where fixed SUSTAINS. Non-visual
combat stimulus (vis=0 both builds) escalation that does not persist. Attribution:
which PORTED fn in actor_perception.obj(12)/props.obj(10)/actor_combat.obj(10)/ai.obj
writes 6a / 268 / prop+0x24 / focus-create — trace base->datum_get. If all writers
unported -> clean NEGATIVE (enters via target-acq or object/scheduling upstream).

## Checkpoint 5 — SMOKING-GUN MECHANISM: focus-prop destroyed -> awareness reset

Field writer survey (lifted source):
- actor+0x6a (2 vs 3): actor_action_change actions.c:821-828 — sets 6a from action-type
  table flag 0x253fb0. PURE FN of chosen action; downstream of action selection.
  Not the cause; consequence of which action (combat=3 vs noncombat=2) is selected.
- actor+0x268 PROMOTE (to 3/5/6): UNPORTED (0x302b0 recompute reads table[0x255f18+pstate]).
- actor+0x268 = 0 writes in PORTED source: actors.c:5469 (actor_new spawn init, one-shot)
  and actors.c:4175 in FUN_0003b410 = actor_perception_replace_prop (PORTED, actors.obj).
- prop+0x24 advance to 5/4: UNPORTED promote (0x33330). Ported writes only 0/1 baseline.

*** FUN_0003b410 actor_perception_replace_prop (PORTED) lines 4172-4176:
  if (actor+0x270 == old_prop) { actor+0x270 = new_prop;
      if (new_prop == -1) actor+0x268 = 0; }   // focus prop DESTROYED -> awareness RESET
This EXACTLY produces the broken fire6 signature: 268=0 AND 270=-1 (focus lost).
=> The grunt's focus prop is being DESTROYED/GC'd in broken (replaced w/ -1), firing
   this reset. Fixed build keeps the prop alive -> awareness sustains.

This fn is the MECHANISM. CORRECTION: the player prop is NOT destroyed — chainstate
dump shows it SURVIVES in the chain (prop#8 trk=player st24=0) in broken; only
actor+0x270 focus is cleared. So 0x3b410-via-destruction is likely NOT the path.

## Checkpoint 6 — KEY DISCRIMINATOR + ISOLATION-DUMP BISECT

KEY (advisor): In BOTH builds the grunt promotes its ALLY props (grunts 10/11,
trk e55d016f/e55a016c) to st24=3. ONLY the PLAYER prop fails to promote in broken
(st24=0 vs fixed 5). => promote machinery WORKS; bug is PLAYER-PROP-SPECIFIC
promotion gate, not the general tick. Broken player prop sits LAST in a LONGER chain
(6 props vs 4 fixed). +0x2c is alloff-only artifact (broken 1.0 AGREES unp 1.016) —
DO NOT chase.

ISOLATION-DUMP BISECT (survey focus grunts 6/10/11):
- 3b_fire10/3b_fire16: 6a=2 6e=2 268aw=5 focus SET prop st24=4 => FIXED signature
  (perception SUSTAINS). The "3b" toggle FIXES the bug.
- pat_provoked2: all 3 grunts 2/0/0 focus=-1 st24=0 => BROKEN.
- c3b1: MIXED (grunt6 broken 0, grunt11 fixed st24=4).
- swathoff1: focus grunts in search-7, inconclusive (diff scenario state).

Dumps captured 2026-06-24 single session, NOT committed (no git log). No toggle-config
note. INFERENCE (needs confirmation): "3b" prefix => toggling 0x3bxxx-range functions
(actors.obj perception cluster: 0x3b3c0 actor_derive_target_information, 0x3b410
actor_perception_replace_prop, ...). If 3b-range OFF => FIXED, culprit is a ported
0x3b-range fn. CAVEAT: dump-name->object mapping is INFERRED, not proven.

NEXT: list ported 0x3b-range fns; find which gates player-prop promotion / clears
focus 0x270 specifically for the player; base->datum_get + faithfulness vs disasm.
Primary footprint should be full-vs-unp (unp=ground truth; alloff=corroboration only).

## Checkpoint 7 — Stimulus path mapped; "outside-AI" negative REFUTED; merge cluster

Non-visual (gunfire NOISE) combat-stimulus -> promote path (grunts vis=0 both builds):
  actors_handle_unit_effect 0x3e570 (PORTED actors.obj) iterates encounter actors ->
  actor_perception_find_sense_position 0x31a90 (UNPORTED) ->
  actor_audibility_at_point 0x31850 (UNPORTED) gate >=2 ->
  FUN_00064b40 prop-create (UNPORTED props.obj) ->
  actor_audibility_at_point again (prop+0xbc) >=2 ->
  actor_handle_unit_effect 0x3d430 (PORTED) case 3 (gate prop+0x133!=0) ->
  actor_perception_become_acknowledged 0x33330 (UNPORTED promote to st24=5).
Also FUN_00055110 ai_magically_see_unit (PORTED ai_profile) = scripted unit_effect=3.

PORTED on path: only the two *_handle_unit_effect dispatchers (0x3d430, 0x3e570) +
FUN_00055110. Gate computations (audibility, sense, promote, prop-create) UNPORTED.

*** ADVISOR DECISIVE: the "bug outside AI" negative is REFUTED. alloff = the 12 AI
objects reverted to ORIGINAL code (ported=false runs original) -> FIXED; unpatched
agrees; rest of build byte-identical alloff-vs-full. => 12 AI objects on LIFTED code
= bug; on ORIGINAL code = no bug. That IS a lifted bug inside one of the 12 AI objects.
When toggle evidence conflicts with code-reading inference, TOGGLE WINS. The unported
gate does not exonerate ported code FEEDING it a corrupt input.

FOOTPRINT favors MERGE/ORPHAN cluster: broken=1 unpromoted player prop (st24=0);
fixed=2 player props, one promoted active-5. Merge/orphan-creation divergence, NOT
stimulus delivery. Prior "0x3b-0x3c orphan trio faithful" was LOGIC-only audit; the
SILENT class (struct-field rotation, register aliasing, buffer-alias = repo #1 hazard)
is exactly what logic audits miss + does not move VC71.

"3b" dump-name UNDECODABLE (no shell history / helper / snapshot record; toggles were
in-session Edit/sed). Cannot name culprit from dump alone.

NEXT (concrete cut): unported gate READS prop+0xbc/+0xfc/+0x38/+0x66/+0x133 (+ sense
block, actor sensory pos). Find which PORTED in-the-12 fn WRITES each; silent-class
re-audit (offsets/registers vs disasm, NOT logic) of that writer. 0x3d430 writes
+0x66/+0x68 — re-check that store's offsets+reg sourcing vs disasm first.

## Checkpoint 8 — STRUCTURAL ROOT: extra perception props in broken (FINAL)

CHAIN-COMPOSITION divergence REPLICATES across all 3 grunts 6/10/11 (consistent =>
born divergence, not drift):
- BROKEN (full_fire6): EVERY grunt's prop chain has 3 EXTRA props tracking objects
  e2a50036 / e2a30034 / e2b50046 (st24=0, prio=0, dist 10-13u) that are ABSENT in
  fixed. Player prop always at chain TAIL, st24=0, never promoted.
- FIXED (alloff_fire6): shorter chains, NO e2a/e2b props, player prop present
  (grunt6 promoted st24=5). These e2a/e2b objects are NOT actors (not in actor table)
  -> tracked OBJECTS (allied marines / other bipeds / bodies) that broken spuriously
  creates perception props for. Extra props crowd the chain / consume prop budget /
  priority, pushing player-prop promotion out.

=> ROOT MECHANISM: broken build creates perception props for objects fixed does not
   track. Prop-creation = FUN_00064b40 (props.obj, UNPORTED). Decision of WHICH
   objects to perceive/scan = perception-scan cluster.

Disasm CHECK done (first binary-vs-source this session) on FUN_00064400
prop_remove_from_actor (props.obj PORTED, the chain-unlink): splice offsets (0x50,+8)
+ register use (EDI=prop_handle, ESI=&prev->next) MATCH C exactly @ 0x64400-0x6453a.
FAITHFUL — not the culprit.

## Checkpoint 9 — naming attempt (timeboxed) + RECORDED BISECT PLAN

Ported 0x64b40 (prop-create primitive) callers examined:
- actor_update_prop_desire 0x14360 (actor_looking.obj): creates prop for actor's FOLLOW
  target actor+0xa8 (single leader object). NOT a multi-object scan.
- actor_handle_damage 0x3b6f0 (actors.obj, actors.c:4424): creates prop for a DAMAGE
  source (per damaged actor). Per-actor, not the all-grunt 10-13u pattern.
- ai.c:693/702: pairwise unit-interaction prop (unit_effect=0).
- 0x3dc20 actors.c:7474: EXONERATED (coordinator, control-proven).
None is an obvious "scan many nearby objects" creator. The e2a/e2b pattern (3 distinct
non-actor objects, EVERY grunt, fire-only, dist 10-13u, vis=0) = a proximity/sound
stimulus scan admitting nearby firing/effect objects (likely allied-marine gunfire
sources). The exact scan creator NOT pinned within ~25-call budget (spans encounters/
actors per-tick paths). e2a/e2b type unresolvable from artifacts (only zero-filled
v_obj.bin; no object-table dump committed).

AI-domain ported objects (alloff set, 13): actors(124) actor_looking(120) encounters(99)
ai_profile(46) actions(35) ai(35) actor_moving(28) path(13) actor_perception(12)
actor_combat(10) props(10) ai_debug(9) ai_communication(5).

### RECORDED BISECT PLAN (deliverable 2) — read junk e2a/e2b props from ONE full_fire dump/config
Per-config footprint check (no 3-run averaging): walk focus grunt 6/10/11 chain
(actor+0x50 head, prop+0x8 next); COUNT props whose prop+0x18 tracks a non-player non-actor
object at dist>8u with prop+0x24==0. >=2 such = BROKEN; 0 = FIXED. Tool: /tmp/chainstate.py
<dump> <slot> prints the full chain already.

Split 1 (exclude the 3 biggest first; toggle TWO disjoint halves of the rest):
  Config A ported=false: {props.obj, actor_perception.obj, actor_combat.obj,
                          ai_communication.obj, actor_moving.obj, path.obj}
  Config B ported=false: {encounters.obj, actions.obj, ai.obj}
  Leave {actors.obj, actor_looking.obj, ai_profile.obj} ON in A and B.
  - A FIXED -> culprit in A's set; B FIXED -> culprit in B's set.
  - BOTH still BROKEN -> culprit in the big-three -> Split 2.
Split 2 (big-three, one object per dump): Config C1 ported=false {actors.obj};
  C2 {actor_looking.obj}; C3 {ai_profile.obj}. Whichever flips FIXED names the OBJECT.
Split 3 (within named object): binary-split its ported function addr list (2-3 configs)
  to the function.
RECORD each config's exact ported=false list + dump filename in a COMMITTED json
(artifacts/ai_regression/bisect_configs.json) — the lost "3b" config is why naming
stalled; do not repeat that.

## VERDICT (deliverable)
EARLIEST born divergent field: prop chain COMPOSITION — broken tracks 3 extra
perception props (objects e2a*/e2b*) absent in fixed/unp; consequence cascade:
player prop unpromoted (prop+0x24=0) -> awareness actor+0x268=0 -> action submode
actor+0x6a=2 -> alertness 0x6e=0 -> no advance. (still-condition actor+0x591/0x1a0
were look-state CONFOUNDS, not the bug.)

ATTRIBUTION: LOCALIZED, NOT individually named. TOGGLE-PROVEN lifted bug in one of
the 12 AI objects (alloff=AI-objects-on-original => FIXED; full=lifted => BROKEN;
rest of build byte-identical). "Outside AI" negative is REFUTED by alloff. Footprint
points at the PORTED perception-SCAN / prop-creation-DECISION cluster (props.obj 10/35
+ actors.obj perception cluster) that selects which objects to track — a ported fn
admits objects (e2a/e2b) into the perception set that the original rejects. The
creation primitive 0x64b40 + audibility gate 0x31850 + promote 0x33330 are UNPORTED
(faithful given input); the corrupt INPUT (the over-broad object set / scan decision)
is produced by ported scan code. FUN_00064400 (chain unlink) disasm-verified faithful.

FP/SSE: REFUTED both ways (global -mno-sse; categorical 0-vs-nonzero footprints, no
ULP signature).

CONFIDENCE: HIGH on localization + footprint + FP-refutation; MEDIUM on exact culprit
cluster (scan/creation-decision) — not narrowed to one function.

BEST NEXT EXPERIMENT: identify objects e2a50036/e2a30034/e2b50046 by type (object
table dump), then find the PORTED perception-scan fn that decides to track them and
silent-class audit it vs disasm. OR re-run a targeted toggle bisect on props.obj vs
actors.obj-perception alone (split the 12) to bracket which object, recording the
toggle config this time (the "3b" config was lost).

