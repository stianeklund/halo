---
name: a10-matched-geometry-AI-bug-confirmed
description: a10 grunt-passive — DECISIVE matched-geometry trajectory diff PROVES a genuine ported AI input-feeder bug (NOT physics/FP drift). At pixel-identical player pos + 0.05u-identical a6 pos, dist 3.78, a6 at door, cachebeta promotes a6 player-prop 5->3 (engaged) but default stays state5 (passive) forever. Confirmed via 1000-reads/s persistent-RDCP capture.
metadata:
  type: project
---

# a10 grunt-passive: matched-geometry trajectory diff = AI BUG CONFIRMED (2026-06-29)

## The test (advisor's discriminator)
Two-compiler builds (clang default.xbe vs MSVC cachebeta.xbe) of a physics sim drift on
identical CONTROLLER input — so "deterministic divergence" alone does NOT imply AI-localized.
The decisive question: **at the tick a6's perception diverges, is a6 GEOMETRY identical between
builds?** Identical => AI input-feeder bug. Drifting => physics/world-state drift, AI audit = phantom.

## Method that finally worked (capture mechanics)
- Cross-build clock = `game_time_globals->time` : ptr @ VA `0x45708c`, field `+0xc`, pure int 30Hz
  tick. The CORE (a10-checkpoint-5s-action) was saved at **tick ~6470**; every fresh replay loads
  at 6470 and plays forward — consistent cross-build window. (struct: types.h game_time_globals_t,
  time@0xc, initialized@0,active@1,paused@2,elapsed@0x10,speed@0x18.)
- a6 = slot6/salt0xe38b (stable across reboots, baked in core). a6 pos @ actor+0x120/0x124/0x128;
  focus prop @ actor+0x270; prop fields state@+0x24 accum@+0x2c perc@+0x30 vis@+0x32 tracked@+0x18.
- Pool addrs STABLE across reboots (deterministic allocator from shared core): adata=0x8027e718,
  pdata=0x802f5ec0, pcount=768. Player obj resolved 0x5a8d50->+0x34->idx*0xc->+8->+0xc (varies/boot).
- **KEY UNBLOCKER: persistent RDCP socket.** Subprocess-per-getmem (~150ms) was too slow to catch
  the ~5s a6-live window buried after a ~25s boot. Inline minimal RdcpClient (port 731) holding one
  socket open = **~1000 reads/s** (150x). Tool: /tmp/capfast.py (TAG DUR OUT). Start it, then trigger
  `capture_scenario.py replay --xbe <build>`; it reconnects across the reboot and burst-captures.

## RESULT — geometry identical, perception diverges (DECISIVE)
Promotion window tick 6560-6656, default vs cachebeta at matched ticks:
- **|Δplayer pos| = 0.000** (pixel-identical, player stationary at px=-7.99), **|Δa6 pos| <= 0.067u**,
  **|Δdist| <= 0.071u**. a6 advances to the door identically on both (a6x -13.08 -> -11.76).
- cachebeta: a6 player-prop starts state 5 (aged, from core); as a6 advances to dist~4, at **tick 6592
  / dist 3.98 it promotes 5 -> 3 (ENGAGED, accum 1.0, vis 3)** and stays engaged at the door (dist 3.78).
- default: a6 player-prop **stays state 5 (accum 0, vis 0) FOREVER** — only states [2,5] across its
  entire 2109-tick arc (6471->8579); 0 ticks ever reach state3 or vis>=3.
- (8.37 max dist-delta was a capture-boundary artifact at tick 6884 where cb's player handle resolved
  to 0 at capture end; not real divergence.)

=> At PIXEL-IDENTICAL geometry, a6 at the door dist 3.78, faithful promotes the player-prop to
engaged and patched does not. **Genuine, deterministic, build-caused AI input-feeder bug. Physics/FP
drift REFUTED as the cause** (geometry is identical to <0.07u through the divergence). This is the
matched-geometry test five prior sessions never ran; it confirms the AI-localization the toggle
bisect implied, on rigorous footing.

## Mechanism (precise, for the next phase)
The prop is state 5 (aged) in BOTH builds from tick 6472. The divergence is the **5->3
re-acknowledgement** at the door (dist~4): cachebeta fires it, default does not. Re-ack = UNPORTED
`actor_perception_become_acknowledged 0x33330` (sets state 3), called by UNPORTED dispatcher 0x355f0
under a condition computed from INPUTS a ported fn supplies. So a ported producer feeds the unported
promotion machine a divergent per-tick input for a6 at the door (perception +0x30 / vis-channel /
rate-table index actor+0x6c/0x70/0x74 / LOS actor+0x9e). props.obj already CLEARED (all 10 ported
faithful, [[a10-propsobj-clean-negative-and-overturned-localizations]]); ai.obj-alone revert does NOT
fix. The input-feeder is elsewhere among the ported AI objects.

## PROXIMATE CAUSE FOUND (2026-06-29, full-record per-tick differential) — vis (prop+0x32)
Ran full actor(0x274)+prop(0x138) per-tick capture on both builds (tools /tmp/capfull.py,
/tmp/full_{def,cb}.jsonl). Diffed in the state-5 window (ticks 6470-6591). DECISIVE:
- At tick 6591, a6 at door dist 3.78 IDENTICAL geometry: **cachebeta prop+0x32 (vis) = 3**, **default
  prop+0x32 (vis) = 0**. One tick later (6593) cachebeta promotes prop+0x24 5->3 (accum 1.0, aware
  a+0x268=10, focus a+0x270 -> the now-active prop, p+0x3a resets 0). Default stays s5/vis0 FOREVER
  (prop+0x24 only ever [2,5], 0 state-3 ticks across the whole arc).
- => vis=3 (a6 CAN SEE the player) is the trigger for the 5->3 re-acknowledgement. **Default's
  line-of-sight/visibility computation returns vis=0 (not visible) at the door where the faithful
  build returns vis=3 (visible), at PIXEL-IDENTICAL geometry.** This is the proximate divergent field.
- Secondary observation: default's whole perception sequence LAGS cachebeta by exactly 1 tick
  (a+0x64 timestamp cb=0x1947/def=0x1948; a+0x9c, a+0x7c, a+0x268, p+0x3a all = cb's value one tick
  later; chain props are different instances allocated 1 tick apart: cb f78e0012/f791001a vs def
  f78d0011/f7900015). But vis diverges CATEGORICALLY, not by phase — default never gets vis=3 even one
  tick late. The 1-tick lag is a minor scheduling artifact; the vis=0 is the real bug.
- prop+0x30 (perception/audibility channel) = 0 in BOTH — so it is NOT the audibility path; it is the
  VISUAL (vis) channel that diverges. This is the ORIGINAL "los->vis perception lift" subsystem.

## REFINED PROXIMATE CAUSE (2026-06-29 cont.): a6's FACING is rotated ~32° (the vis-test INPUT)
Per advisor: vis is graded and default sits at HARD 0 = "player outside the perceived set" = outside
a6's vision CONE, not a grazed ray. Checked a6's orientation frame at actor+0x180..0x1ac at matched
STATIC ticks (6602-6608, both at door, pos delta ~0.006u). DECISIVE:
- cachebeta: near-IDENTITY frame, forward ~= (0.998,0.037,0.053) = +X, pointing straight at player
  (player at +X from a6). Upright, axis-aligned.
- default: forward ~= (0.840,0.530,-0.114), tilted ~32° off-axis (toward +Y, pitched down). Whole
  frame rotated. Angle(cb_fwd, def_fwd) ~= 31.6deg, STABLE across all static ticks.
=> default a6 faces ~32deg away from the player -> player outside a6's vision cone -> unported
visibility test returns vis=0 -> prop never promotes -> a6 passive. PERFECTLY explains "advances to
the door but won't fire": movement/geometry correct (identical), but FACING is wrong so a6 can't see
the player. This is aim/facing territory (cf [[project_biped_aim_up_euler_nan]], actor_looking :529).
- actor+0x180/0x184/0x188 read as a vector at actors.c:4020 ("attack/cover vector"); the +0x180..0x1ac
  region = a6's look/aim orientation frame (3 unit vecs). prop+0x30 (audibility) =0 both => VISUAL path.
- NOTE struct caveat: types.h offsets 0x120/0x198/0x1a0 are object_data_t, NOT the actor (0x724) record;
  do not borrow those names. The 0x724 actor record is what's read (adata=*(*0x6325A4+0x34), stride0x724).

## AIRTIGHT CHAIN (2026-06-29 cont.) — root = AI desired-LOOK (actor_looking desired_facing), NOT 0x3dc20, NOT biped
Live capture of a6's biped object (resolve actor+0x18 unit -> object table) + the unit control/lerp
slots (tool /tmp/capbiped.py; /tmp/bip_{def,cb}.jsonl). Unit has 5 direction slots; desired-aim=+0x1e0,
cur-aim=+0x1ec, desired-look=+0x204, cur-look=+0x210. The cone test reads actor+0x18c = cur-look
(unit+0x210), copied faithfully by FUN_0003dc20 (proven: on default actor+0x18c == biped+0x210 for all
static ticks; 0x3dc20 is a FAITHFUL COPY — analyst pick #1 REFUTED, its prior "exonerated" verdict stands).
DECISIVE split:
- DEFAULT: desired-AIM(+0x1e0) = (1.00,0.04,0.04) at player (correct, cur-aim lerps to it). desired-LOOK
  (+0x204) = (0.84,0.53,-0.11) STUCK ~32deg off forever; cur-look lerps to that stuck target. Look-lerp
  is FINE; the DESIRED-LOOK target is wrong.
- CACHEBETA: desired-LOOK turns to the player — (0.95,0.27,0.18) -> at tick 6593 jumps to (1.00,0.08,0.05)
  -> (1.00,0.03,0.05) ≈ +X; cur-look follows; that is what yields vis=3 and the 5->3 promotion. cachebeta
  desired-look ≈ desired-aim (look follows the threat). default's look does NOT follow.
=> ROOT: the AI's DESIRED-LOOK (actor desired_facing, actor+0x5a4 -> control_data->looking -> unit+0x204)
is computed wrong in our lift — stuck ~32deg off instead of turning to the player. This is the
actor_looking.c look-spec / desired_facing producer (lines ~8477/8635/8674/8741/8885 write desired_facing
= actor+0x5a4 from primary_vec/secondary_vec; magnitude check :9010; the :529 look-spec area, FUN_00028660
/ FUN_00028ed0 look-spec helpers). NOT 0x3dc20 (faithful copy), NOT the biped aim (aim is correct), NOT
the cone test (faithful), NOT physics (geometry identical).

Full chain: actor_looking desired_facing WRONG (stuck) -> control_data->looking -> unit+0x204 stuck ->
unit+0x210 cur-look lerps to wrong -> 0x3dc20 copies -> actor+0x18c -> cone test 0x314f0 -> vis(prop+0x32)=0
-> prop+0x24 never 5->3 -> a6 passive/won't-fire. Secondary 1-tick perception phase-lag is unrelated.

## ★ DONE — COMMITTED c85ad81a (2026-06-29). Smoke: broad fix, no regression, no crash.
Smoke check (all 24 actor slots, fixed default replay): combat window 9 grunts present, UP TO 3 engage (combat-target
actor+0x610 set) — the door group; a6 reaches state3 @tick6598. Only 3/9 engage (distant grunts don't spuriously
perceive → NO over-eager-perception regression). 392 samples across full scenario + reboots, NO crash/assert. 0x3dc20
runs for EVERY actor so this corrected the perception look-frame for ALL of them — explains the long-standing broad
"grunts too passive / aim off" complaint. VC71 FUN_0003dc20 = 80.4% (FPU structural ceiling, no regression; behavioral
oracle = cachebeta is the authoritative evidence for this FPU-heavy fn). Committed (NOT pushed — runner). Stashed
al_only kb.json toggle still on the stash stack (test-only, droppable).

## ✅✅✅ FIX CONFIRMED ON ORACLE (2026-06-29) — a6 now perceives, promotes, engages
Applied the up+right store swaps in actors.c (FUN_0003dc20), kb.json clean, built+deployed fix-only, full-scenario
replay. FIXED default a6:
- right vector actor+0x1a4 = (0.10,0.06,0.99) ≈ +Z (⊥ look) — CORRECT (was degenerate ≈+X). matches cachebeta.
- vis (prop+0x32) → 3 @tick6592 (was 0 forever). prop+0x24 state 5→3 @tick6594 (PROMOTES; was [2,5] only, now [2,3,5]).
- combat target actor+0x610 set (0xf78d0011). a6 ENGAGED → fires.
- look (+0x18c) THEN turns to player ((0.84,0.53,-0.11)->≈+X by t6602) — CONFIRMS the 32°-off look-spec was a
  CONSEQUENCE (no perception -> look never targets player), not a 2nd bug. Single root, single fix. advisor's bet held.
This matches cachebeta tick-for-tick. NEXT: VC71 verify (byte-match the original), AI smoke check (0x3dc20 runs for
ALL actors -> likely fixes broad grunt-passivity; watch for over-eager-perception regressions), then /lift commit.

## ★★★ ROOT CAUSE FOUND + FIX (2026-06-29) — 0x3dc20 right-vector cross-product X/Z store swap (actors.obj)
The look frame's RIGHT vector (actor+0x1a4..0x1ac), read by the perception cone test 0x314f0 (fmul [edi+0x1a4]),
is computed WRONG in our lift: degenerate (right ≈ look) vs cachebeta (right ⊥ look). Measured all 3 builds at
look=+X: cachebeta right=+Z (correct ⊥); broken-def + reverted-def right≈+X (= look, degenerate). Wrong right ⇒
cone azimuth projection garbage ⇒ 0x314f0 returns <3 ⇒ vis(prop+0x32)=0 ⇒ a6 never perceives player ⇒ never
promotes ⇒ won't fire. Survives reverting actor_looking (bug is actors.obj 0x3dc20, not actor_looking).

DEFECT (actors.c ~7325-7334, FUN_0003dc20 actor_input_update, ported): right = cross(looking, up). Original
disasm 0x3e341-0x3e37a computes cross.z, cross.y, cross.x (push order) then fstp pops LIFO → stores
+0x1a4=cross.x, +0x1a8=cross.y, +0x1ac=cross.z (natural xyz). The C lift named them ax=cross.z/ay=cross.y/
az=cross.x (source=compute order) and stored ax→+0x1a4, az→+0x1ac → **+0x1a4 and +0x1ac SWAPPED** (X gets
cross.z, Z gets cross.x). §4 cross-product / FPU-stack-LIFO transcription hazard.
FIX: swap the two stores —
  *(float*)(actor+0x1a4) = az;  // cross.x = ly*uz - lz*uy
  *(float*)(actor+0x1a8) = ay;  // cross.y (unchanged)
  *(float*)(actor+0x1ac) = ax;  // cross.z = lx*uy - ly*ux
Verify look=+X,up=+Y: cross=(0,0,1)=+Z; fixed C gives +0x1a4=cross.x=0,+0x1ac=cross.z=1 = +Z ✓ = cachebeta.

RECONCILES EVERYTHING: the look-spec "32° off" (broken-def) was a CONSEQUENCE (no perception → look never
turns to player); reverted-def look=+X but vis=0 because the right vector (0x3dc20, unaffected by actor_looking
revert) was still swapped; the divert/blind was a dead end (blind=0 measured); FP/physics drift refuted (geometry
identical). SINGLE ROOT = the cross-product store swap. Closing oracle: fix via /lift → default a6 right vector ⊥
look (=+Z at look=+X) → 0x314f0 returns 3 → prop+0x32 vis→3 → prop+0x24 5→3 → a6 fires; the look-spec turns to
player on its own (consequence resolves). RESTORE kb.json (un-toggle al_only actor_looking) BEFORE the fix build.

## ★ DEFINITIVE ROOT (2026-06-29, SUPERSEDED by cross-product finding above) — LOOK-spec is causal; divert REFUTED; cone test is the gate
Divert REFUTED (analyst + advisor): local_4 = (encounter resolves && encounter+0x40 BLIND) || actor+0x6a==1.
*0x5ab270 = encounter pool; +0x40 = BLIND flag; both blind writers FAITHFUL (init-from-map same data, encounter_set_blind
only via script cmd; NO per-tick writer). actor+0x34=ee6e0009 & actor+0x6a=3 IDENTICAL both builds => local_4 identical
=> cachebeta fires (local_4=0) => NEITHER build diverts => 0x314f0 IS reached and returns 0 in our build.
Cone gate 0x314f0 (UNPORTED) decoded: projects target dir onto the LOOK frame (actor+0x18c..0x1ac); elevation cone
[-45°,+30°] (consts 0x256138/0x25613c); azimuth via 0x2f470 vs actr_tag+0x28 (map data, SAME both builds); + LOS
(prop+0x38) ∈{0,1} + dist²≤36 -> returns 3 (visible).
**CLEAN A/B PROOF (tick 6592-6593, both LOS=0, geometry matched, ALL 0x314f0 inputs identical EXCEPT the look):**
- cachebeta look = 21.7° off player -> vis=3 (in cone). broken-def look = 30.0° off -> vis=0 (out of cone).
=> The LOOK is the sole divergent input and IS causal. azimuth cone threshold actr_tag+0x28 ∈ (21.7°, 30°).
ROOT: the look-spec actor+0x570 (produced by FUN_00028cc0, actor_looking.obj) resolves ~30° off in our lift vs
cachebeta ~21°(then 14°) at MATCHED geometry, while desired_facing/aim (actor+0x5a4) is faithful. Wrong look -> player
outside perception cone -> 0x314f0 returns <3 -> prop+0x32 vis=0 -> prop never promotes 5->3 -> a6 passive / won't fire.
NOTE the 1-tick LOS lag (def los 3->0 at 6592 vs cb 6591) is SECONDARY (both clear by 6592). The look-spec is the cause.
UNRESOLVED TENSION: reverted-def (whole actor_looking ported=false) made the look ~0° yet vis stayed 0 — a CONFOUND
(reverting 120 fns drifts geometry ~0.06u and/or breaks a perception-path input; advisor: can't bear weight). The clean
tick-6592 A/B (only-look-differs) overrides it. FIX = surgical: why does FUN_00028cc0's look-spec resolve to ~30°-off
(VC71/branch/clamp diff in look-spec resolution), NOT a whole-object revert. Then re-test cone/vis with ONLY that fix.

## GATE DECODED (2026-06-29) — vis divert hinges on datum_get(*0x5ab270, actor+0x34)+0x40 flag (REFUTED — see above)
Disasm of 0x33440 prologue + local_4 (capstone, cachebeta.xbe):
- 0x3347b: eax = actor+0x34 (= 0xee6e0009, a6's encounter/squad handle). If ==-1 -> [ebp-0x18]=0.
- 0x33490-0x3349e: [ebp-0x18] = datum_get(pool *0x5ab270, actor+0x34).  (*0x5ab270 = a datum pool; live
  base≈0x803306f8 stride≈0x80 — verify in replay state, idle probe hit salt=0/empty.)
- local_4 (0x334d0-0x334eb): `local_4 = ([ebp-0x18]!=0 && byte[[ebp-0x18]+0x40]!=0) || (actor+0x6a==1)`.
- prop+0x133 gate (0x334ef-0x33503): = bit10 of biped[+0x1b4]; forced 1 if prop+0x12e && FUN_fff80()==0 &&
  [0x5ac9c6]. Measured prop+0x133=0 both builds.
Since actor+0x6a=3 (not 1) and prop+0x133=0 (both), and our lift PROVABLY diverts (reverted-def vis=0 with
perfect look+LOS for 20+ ticks), by elimination **byte[datum(*0x5ab270,ee6e0009)+0x40] = nonzero in our lift,
0 in cachebeta** => local_4=1 => orphan branch diverts to zero-store 0x33986 => perc/vis forced 0 => a6 never
promotes => won't fire. This is the ROOT GATE. The earlier LOS 3->0 1-tick lag (def 6592 vs cb 6591) and the
~32° look error are SECONDARY/parallel consequences, NOT the cause (reverted-def has both fixed, still vis=0).
NEXT: (1) confirm record+0x40 differs cb/def live at the a6 window; (2) identify pool *0x5ab270 (encounter/squad?)
and the PORTED fn that writes [datum+0x40] — that writer's object is the culprit to toggle-confirm. encounters.obj
is an allocator (toggle shifts a6 off door) so prefer fixing the specific writer over a whole-object toggle.
CAVEAT (2026-06-29): my live +0x40 read used pool layout base=hdr+0x34/stride=0x80 → enc_salt=0x0000 (WRONG; valid
record salt should be 0xee6e). So that datum_get layout for *0x5ab270 is not the standard +0x34/+0x20 one — need the
real datum-array element_size/base offsets (analyst af508de3be4d367ce resumed to get pool name + layout + the +0x40
ported writer). /tmp/enc.py has the (wrong-layout) capture scaffold; fix base/stride once analyst reports. Deduction
that datum+0x40 is set in our lift stands by elimination (actor+0x6a=3, prop+0x133=0 both; reverted-def diverts with
perfect look+LOS, ~0.06u drift ≈1° can't break the cone at dist 3.78 → must be local_4 divert, not 0x314f0 geometry).

## CONCLUSIVE (2026-06-29): look is DEFINITIVELY not the cause; vis-gate = orphan-branch control flow
Reverted-def at ticks 6600-6696: a6 look (actor+0x18c) = (1.00,0.03,0.04) DEAD-ON player, LOS (prop+0x38)=0 CLEAR,
for 100+ ticks — yet vis=0, perc=0, state=5 the WHOLE time. So reverted-def is a TRUE negative (not drift-masked).
The visual perc/vis detection is gated by a NON-LOOK condition.
Analyst (reference_vis_gate_orphan_branch_33967_disasm): state-5 prop takes the ORPHAN branch of 0x33440; vis
(prop+0x32) written nonzero IFF prop+0x133==0 AND local_4==0 AND 0x314f0 (@0x33967) returns nonzero. Else diverts to
zero-store 0x33986. local_4 = ([ebp-0x18]obj && obj+0x40) || actor+0x6a==1 (inputs actor+0x34->obj+0x40, actor+0x6a).
0x314f0 needs param_5 LOS∈{0,1} (=prop+0x38, CLEAR here) + cone basis actor+0x18c (correct here) + origin (0x31df0) +
target prop+0x104. Checked identical/OK between builds: prop+0x133, actor+0x6a(=3,not 1), LOS(=0), look basis. So the
divergence is either local_4's ([ebp-0x18]obj && obj+0x40) [actor+0x34's object +0x40 flag] OR 0x314f0 cone geometry
(origin from unported 0x31df0). Decision point: chase those 2 uncaptured inputs vs empirical group-toggle of the
non-allocator perception/combat objects (advisor fallback). actor_looking/props/ai already ruled out.

## REAL BUG ISOLATED (2026-06-29): VISUAL perc/vis detection never fires; NOT gated on look
Look-controlled prop diff (reverted-def vs cachebeta, look now correct on both): the player-prop is IDENTICAL
pre-tick-6591 (both state5, perc0, vis0, same tracked handle; only chain-link salts + a 0.05u dist+0x11c differ).
At tick 6591: **cachebeta perc(prop+0x30) AND vis(prop+0x32) BOTH jump 0->3** -> state 5->3 @6593. Reverted-def:
perc & vis STAY 0 forever. So the VISUAL PERCEPTION DETECTION (sets prop+0x30 perc & prop+0x32 vis to 3 when a6
"sees" the player) fires in cachebeta, not in default — and is NOT gated on a6's look direction (reverted-def
looks straight at player, actor+0x18c≈+X confirmed, yet perc=0). The bug is a ported gate/input to the visual
sense, NOT look or vis math. perc & vis move TOGETHER (both 0->3) — it's the perception-level write, not just vis.
Suspects (perception/combat side; actor_looking/props/ai already cleared): actor_perception.obj, actors.obj,
actor_combat.obj, encounters.obj, ai_profile/communication, actions, path, actor_moving. Analyst af508de3be4d367ce
reading the unported vis/perc-write gate (0x33440/0x33d45/0x314f0) for the gate field + its ported writer.
kb.json still has al_only toggle (actor_looking ported=false) — dirty; next bisect_toggle restores HEAD. Box runs
the toggled default. Removed a stale .git/index.lock.

## TOGGLE RESULT (2026-06-29): actor_looking.obj revert FIXES the LOOK but NOT the promotion => LOOK IS A CONSEQUENCE
Built+deployed default with actor_looking.obj ported=false (al_only). Replayed, captured:
- LOOK FIXED: is_aim(+0x55d)=1, look-spec actor+0x570 turns to player (0.973,0.196,0.121 -> 0.999,0.032,0.039 ≈ +X),
  biped cur-look +0x210 follows to +X. (broken default was stuck 0.840,0.530,-0.114.)
- PROMOTION STILL BROKEN: a6 player-prop states [2,5] only, vis(prop+0x32)=0, accum=0, NO state-3 — identical to broken.
=> Fixing the look does NOT fix vis/promotion. **The look-spec/is_aim divergence was a PARALLEL CONSEQUENCE, not the
cause of vis=0.** The promotion bug is on the PERCEPTION/COMBAT side, independent of actor_looking. Advisor's "NOT
fixed -> downstream" branch realized. RETRACT the chain "look-spec wrong -> player outside cone -> vis=0": with the
look CORRECT (a6 looking at player) vis is STILL 0.
- KEY REFRAME: cachebeta writes vis=3 WHILE prop state=5 (tick 6591), THEN promotes 5->3 (tick6593). So state-5 does
  NOT bypass the vis-writer (prior memory note WRONG). vis is computable at state 5; default just never writes vis=3.
- So the vis cone test (FUN_000314f0, UNPORTED) reads correct look frame + correct a6->player geom in reverted-default
  yet returns vis=0 => either it isn't CALLED for a6's prop (gate), or a non-look INPUT it reads is still wrong, set by
  a ported perception/combat fn. props.obj CLEARED, ai.obj-alone no-fix, actor_looking no-fix(promotion). Remaining
  suspects: actor_perception.obj, actors.obj, actor_combat.obj, encounters.obj, ai_profile/communication, actions, path,
  actor_moving. NEXT: toggle-bisect the perception/combat side (oracle = prop+0x32->3 / prop+0x24 5->3 at ~6591).
RESTORE kb.json to clean HEAD before next experiment (git checkout kb.json).

## (test setup, superseded by result above) actor_looking.obj toggled ported=false (config "al_only", 120 fns)
Empirical toggle per advisor (reads can't separate cause/effect in the perception↔look↔combat loop; tick-6471
"earliest" divergences are dominated by benign prop-salt allocation noise + a +0x558 pointer 0x802f5e88 ≈ pdata-0x34
that's either the look_buf producer clue or uninit slop — toggle decides). State: bisect_toggle.py al_only
actor_looking.obj applied (kb.json dirty), build_deploy_run.sh -q running. ORACLE (judge on PRE-promotion ticks):
def a6 actor+0x570 turns toward player (≈cachebeta 0.946,0.272,0.179) AND prop+0x32 (vis) hits 3 @~tick6591.
- FIXED -> bug in actor_looking.obj; bisect within (start look_buf producer / FUN_00028cc0 cluster; +0x558 smoking gun).
- NOT fixed -> look-spec divergence is downstream of perception/combat (is_attacking/look_type side) or prop alloc not benign.
RESTORE kb.json to clean HEAD after (git checkout kb.json) — toggle is test-only, not for commit (deactivation hook).
Tools to re-confirm: /tmp/capwide.py (actor 0x5b0 incl +0x570/+0x5a4 + biped), replay default.xbe. is_aim=use_aim&&transient_aim;
use_aim=is_attacking=(actor_attacking_target!=-1 && look_type∈{0,2}); actor_attacking_target=0x3b270 returns WEAPON handle.

## PINNED (2026-06-29 final) — root field = look-spec actor+0x570, producer = FUN_00028cc0 (actor_looking.obj)
Corrected the bridge theory: actor+0x5a4 (desired_facing) feeds AIMING (faithful, cb≈def, →+X). The LOOKING
source is the LOOK-SPEC at actor+0x570 (cone test reads actor+0x18c = unit cur-look = lerp of unit+0x204
desired-look = control->looking = actor+0x570 look-spec). MATCHED-TICK proof (wide capture both builds):
- actor+0x570: cachebeta = (0.946,0.272,0.179) (toward player) vs default = (0.840,0.530,-0.114) (stuck ~32deg
  off). Both stable; DIFFERENT values at identical geometry. actor+0x5a4 (aim) cb≈def (both →+X) — aim faithful.
- Stuck-look value (0.840,0.530,-0.114) also found at actor+0x18c (cur-look copy) and actor+0x580/584/588
  (idle-minor look-spec). cone-test reads the +0x18c/+0x570 frame.
PRODUCER: **FUN_00028cc0 (0x28cc0) "primary look update", actor_looking.obj, ported=true** (actor_looking.c
:8187). Takes target dir_vec, clamps within az/el angular range via **FUN_000283b0** (azimuth/elevation
projection), writes result to actor+0x570 (line 8256-8257). Callees: FUN_00027ff0 (look-spec gate, :8206),
FUN_00028250 (look-spec resolve, :8263), FUN_000283b0 (angular constraint — prime math suspect; cb/def differ
like an elevation/azimuth clamp). The cb/def divergence is a wrong-DIRECTION (clean unit vec, wrong target/clamp),
NOT the NaN/value :529 bug — a fresh defect, likely FPU operand-order/clamp math in FUN_000283b0 or a wrong
branch/input in FUN_00028cc0/FUN_00027ff0. => actor_looking.obj after all (advisor #1). Toggle FUN_00028cc0
+ callees (or the object) -> confirm actor+0x570 -> ≈cachebeta & vis(prop+0x32)->3. Then VC71/disasm the math fn.

## (superseded) NEXT: toggle-confirm actor_looking.obj (advisor #1) then narrow to the desired_facing producer
Toggle actor_looking.obj ported=false (non-allocator, a6 stays slot6 — verify scan_door) -> build_deploy_run.sh
-q -> replay default -> capbiped -> confirm desired-LOOK (unit+0x204) turns to +X AND vis(prop+0x32) hits 3
@~6591. If fixed -> culprit in actor_looking.obj; binary-search to the desired_facing/look-spec fn (suspect
the ~8400-9010 look-spec selection: which of primary_vec/secondary_vec/look-spec it picks for desired_facing).
Then fix via /lift. Need: capture actor+0x5a4 (desired_facing) directly (wider actor read >0x274) to confirm
it's the actor field, not just the unit slot. Closing oracle unchanged.

## (superseded) earlier NEXT: toggle-confirm + name the writer
Advisor directive: do NOT logic-audit LOS for faithfulness (false-"faithful" 3x). Instead (1) find the
PORTED writer of actor+0x180..0x1ac (a6's facing frame), (2) toggle that object ported=false, rebuild
default, replay, check the razor oracle: does default a6's facing match cachebeta (~+X) AND does
prop+0x32 hit 3 at tick ~6591. Start with actor_looking.obj (aim/LOS, non-allocator -> a6 stays slot6;
verify with scan_door). The deterministic oracle (capfull, matched by game_time tick) is the arbiter,
not the audit. B1 confirmed (no def prop anywhere reaches state3/vis3). Closing oracle: fix -> default
a6 facing ~= +X -> prop+0x32 hits 3 @~6591 -> prop+0x24 flips 5->3 @~6593.

## OLD NEXT (superseded by the facing finding): trace the ported producer of vis (prop+0x32)
vis-writer per prior note = 0x33d45 inside UNPORTED prop_status_refresh 0x33440 (CAVEAT: that note
said state-5 bypasses the vis-writer, yet cb writes vis=3 while state=5 — so re-verify the gate). If
the writer is unported/faithful, the divergence is in the LOS INPUT it reads — a raycast / visibility
cone between a6 and the player. PORTED LOS/raycast candidates: 0x14df70 (bsp/collision raycast; touched
in main-menu-collision + raycast-freeze fixes), actor_looking LOS producers. Hand analyst: "what PORTED
fn computes the visibility/LOS result feeding prop+0x32 for a6->player; at a6-at-door geometry why does
it return not-visible vs original visible — silent-class audit vs disasm." Closing oracle UNCHANGED:
fix -> default a6 prop+0x32 hits 3 at tick ~6591 -> prop+0x24 flips 5->3 at ~6593.
