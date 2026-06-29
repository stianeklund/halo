---
name: a10-passive-live-ab-prep
description: a10 grunt passive — live PATCHED characterization for the controlled stock A/B (2026-06-24). Close grunts pinned at state 0x6a=2 (below >=3 eligible) with player IN perception chain (ppst0) but awareness 0x268 frozen at 0; needs player-action stimulus (10min idle = zero change). Field map + state-name tables corrected. Stale-0x610 lead KILLED.
metadata:
  type: project
---

# a10 grunt passive — live A/B prep (2026-06-24)

User-chosen path: **controlled stock cachebeta.xbe A/B** (old artifacts/ai_regression "unpatched"
dumps are UNTRUSTED — user doesn't know their provenance). Capturing fresh on both builds.

## Authoritative actor field map (cross-checked vs src/halo/ai this session)
- `actor+0x6a` (s16) = **main AI state**; `>=3` = engagement-eligible (actor_perception.c:153
  `return (*(short*)(actor+0x6a) >= 3)`); used as "submode" actor_moving.c:3677.
- `actor+0x6c` (s16) = **current action**; compared ==4/==9/==0xa/==0xb in looking/moving.
- `actor+0x6e` (s16) = **aggression** (>4 → berserk eligible per actions.c:513); NOT "look-state".
- `actor+0x268` (s16) = **awareness/alertness** accumulator (0 = has not perceived player).
- `actor+0x60c` (s16) = **in-combat** flag (1 = in combat).
- `actor+0x610` (u32) = **combat PROP handle** into prop table `*0x5ab23c` — VALID ONLY when
  `+0x60c==1`. NOT a unit/combat-target. Leftover when not in combat (don't resolve via object table).
- `actor+0x50` = perception prop-chain head; `prop+0x18` = tracked unit handle; `prop+0x24` =
  perception state (pst); `prop+0x8` = next.
- `actor+0x4a8` (u8) = path_active (1 = actively pathing).
- `actor+0x3e` = team (3 = grunt). `actor+0x18` = unit handle. `actor+0x12c..0x134` = position.

## State-name tables (cachebeta AI debug overlay)
- firing-position eval reason (ctx+0x4): `{0 fight, 1 panic, 2 cover, 3 uncover, 4 guard, 5 pursue}`
  (actor_looking.c:7216).
- stimulus types (actors.c:521): `{none, enemy-shoot, impact, enemy-close, grenade, damage,
  unexp-close-shoot, unexp-behind-shoot}`.
- anim_mode (actor_looking.c:2947): `{idle aim weapon, noncombat, asleep, combat, panic}`.
- ("alert" string not located in src — likely squad/encounter overlay; user's "alert/uncover/guard/
  fight" = the state-machine the A/B must compare.)

## LIVE PATCHED characterization (player @ (-8.4,-2.5), grunts in artifacts/ai_regression/*pat_clean*,*pat_provoked*)
- Close grunts g6/g10/g11 @ **4–5u**: state `0x6a=2` (NOT eligible), act 2, agg 0, **aw 0x268=0**,
  inc 0, path 0, **player IN chain (ppst0)**. Perception chain LIVE (len 5↔6 oscillates each tick)
  → the player IS being tracked, but awareness never increments.
- Far grunts g7/g8 @ 34u: state 3, **agg 7** (aroused) but aw 0, no player prop.
- Far g2/g4 @ 47u: state 3, act 5, agg 2.
- **10 minutes idle (player still) = ZERO grunt state change.** No spontaneous/timed engage →
  trigger is a player-driven stimulus (move into LOS / fire), not a timer.
- User report: grunts don't transition through uncover/guard normally, stuck in "alert"; can reach
  "fight" if player gets very close but then **won't actually engage** (no inc1, no shooting/advance).

## Anomaly framing (still UNCONFIRMED bug-vs-faithful)
Close grunts track the player (prop in chain) yet awareness `0x268` and state `0x6a` never escalate
at point-blank. Could be faithful (LOS blocked by the "door small no glass" → legit wait) OR a bug
(awareness gain suppressed). The discriminator = **does stock cachebeta promote awareness/state at the
SAME distance+LOS+weapon while idle/under the same stimulus?** A/B capture pending (user booting stock).

## ★ A/B RESULT — REAL REGRESSION CONFIRMED (2026-06-24)
Stock cachebeta.xbe booted (running title E:\GAMES\halo-patched\cachebeta.xbe, no DECOMP banner).
Matched: same checkpoint, player @(-7.8,-3.0) vs patched (-8.4,-2.5), close grunts @4-5u both sides,
same weapon state. **DECISIVE divergence at point-blank:**
| field (close grunts 4-5u) | PATCHED | UNPATCHED(stock) |
|---|---|---|
| state 0x6a | 2 (not eligible) | **3** (eligible) |
| awareness 0x268 | 0 (frozen) | **2..10 climbing** |
| in-combat 0x60c | 0 | **1** (reaches combat) |
| aggression 0x6e | 0 | **3** |
| player ppst (prop+0x24) | 0 | **0->1->3 active** |
| path 0x4a8 | 0 | **1** (pathing to FPs) |
Unpatched dumps: actors/props_unp_idle{0,2}.bin. Patched: *_pat_clean{0,5}, *_pat_provoked{0,2}.
Confound controlled: same checkpoint/weapon; unpatched STILL engages → NOT the unarmed-player excuse.
**Verdict: patched perception/escalation is suppressed.** Prior "NO DEFECT FOUND" (handover) OVERTURNED.

### Confound controls (2026-06-24, user-confirmed)
- WEAPON: controlled. Player unarmed at checkpoint; weapon unholsters at the SAME door/trigger on
  BOTH builds. Not a variable.
- POSITION/LOS: user hands-on confirms PATCHED grunts "engage but only MUCH closer/slower" than
  unpatched (NOT never, NOT same) → genuine engagement-threshold DEGRADATION, not a no-LOS snapshot
  artifact. The ~0.8u capture-position delta does not explain a consistent lived degradation.
### Offline player-prop diff (patched g10 d4.2 vs unpatched g5 d3.9, both player-tracking prop)
Patched prop has sensory sub-data EMPTY though the grunt is CLOSER: prop+0x2c=0.0 (UNP 1.0,
visibility/confidence?), prop+0x80..0x98 all-zero (UNP populated vec3 last-seen pos), prop+0x7c/0x8c
=ffffffff (UNP 0x1b26 ref). => patched perception record never fills in => awareness 0x268 can't
accumulate. (This is the SYMPTOM surface; cause is upstream input to the unported sensory pipeline.)
### Second symptom (grenades, user)
Patched grunts THROW plasma grenades at the player — unusual for a10 early. Unpatched grunts (also
grenade-equipped) do NOT throw. Neither drops grenades on death. => patched AI is simultaneously
UNDER-aggressive (slow engage) and OVER-aggressive (inappropriate grenades) => smells like a
miscalibrated AGGRESSION/difficulty/encounter PARAMETER, not just a dead perception tick. aggression
actor+0x6e was paradoxical live: close grunts agg0, far grunts agg7. 0x6e recompute (0x302b0,unported)
= max(0x72,0x74,table[0x255f18+0x268]). Worth checking difficulty/variant/tag param reads on ported path.
Next: localize the upstream ported function that suppresses awareness/state escalation (perception
chain itself is unported/identical → divergent INPUT). Toggle-bisect on patched is the oracle.

## ★ TOGGLE-BISECT (2026-06-24) — localizing the lifted culprit
- kb.json backup: /tmp/kb_bisect_backup.json (all-original ported state). Helper flips TU sets OFF.
- **Cycle 1: 933 fns OFF across 17 AI+obj/biped/physics TUs → grunts ENGAGE like unpatched
  (user-confirmed + close grunts no longer frozen at 4-5u).** => bug IS in our lifted code, NOT
  build flags / x87-FP / data. Toggle liveness verified (orig prologue 0x55, not redirect 0xE9).
- AI group = {actors, actor_looking, actor_combat, actor_moving, actor_perception, ai, ai_profile,
  actions, encounters, path}. OBJ group = {objects, units, bipeds, collision_usage, projectiles,
  damage, data}.
- Build: `python3 tools/build/build.py -q` (no --target = builds patched_xbe ALL target → default.xbe).
  Deploy: `python3 tools/xbox/deploy_xbox.py --xbox 127.0.0.1 --xbe-only` (magicboot, self-verifies).

- **Cycle 2: AI group OFF / OBJ group ON → GOOD behavior (user).** => bug is in the AI group (10
  TUs, 522 fns). OBJ/biped/physics lifts EXONERATED (the prior "upstream spawn/BSP input" hypothesis
  in [[biped-430-clobber-fun-1a5300]] is WRONG for this bug). 0x3f030 spawn is in actors.obj (AI).
- Cycle 3 (next): split AI — OFF {actors, actor_looking, actor_perception}; ON the rest.

- **Cycle 3 (perception-core {actors,actor_looking,actor_perception} OFF, 7 non-core AI ON) →
  BOOT CRASH** (decals "invalid decal type -11598" in decal_surface_add). Cross-TU coupling: our-lift
  7-non-core AI calling ORIGINAL perception-core breaks (mixed-lift incompatibility). Can't revert
  perception-core in isolation while rest of AI is our-lift.
- **Cycle 3b (complement: perception-core ON, 7 non-core AI OFF) → PARTIAL.** Close grunts aw0x268=5
  (vs 0 broken), agg2, disposition promotes + one grunt PATHS to door — but inc0, no shooting.
  => perception-core (our-lift) holds the "won't finish engaging" bug; reverting 7-non-core restored
  disposition/path. Looks COMPOUND (perception-core caps engagement; 7-non-core suppressed awareness 0).
- Booting configs so far: {all AI off}=GOOD, {perc-core on, 7-noncore off}=PARTIAL(boots),
  {full patched}=BROKEN. Non-booting: {perc-core off, 7-noncore on}.
- Cycle 4 plan: within perception-core, revert actor_looking (LOS/sense/firing) keeping actors+
  actor_perception ON, 7-noncore OFF (booting base). good→actor_looking; partial→actors/actor_perception.

## Killed lead (do not re-chase)
`actor+0x610` "stale handle" was a PHANTOM — it's a prop handle (table 0x5ab23c) resolved via the
WRONG (object 0x5a8d50) table; only meaningful when +0x60c==1 (it was 0). datum_get salt-checks
internally. No corruption. See [[a10-passive-alertness-root-traced]] (prop+0x24 chain is the real axis).

## Probe (works identically on stock — same global addrs/strides)
/tmp/cap2.py <tag> <nsamples> dumps team-3 grunts: dist, st(0x6a), act(0x6c), agg(0x6e), aw(0x268),
inc(0x60c), path(0x4a8), player-ppst; archives actor+prop blobs to artifacts/ai_regression/.
Tables: actor *0x6325a4 stride 0x724; prop *0x5ab23c stride 0x138; obj *0x5a8d50 stride 0xc;
player *0x5aa6d4 +0x38, unit @ +0x34. XBDM getmem (non-invasive; NOT the :1234 halt landmine).

## ★ CORRECTION (2026-06-24 post-compaction) — real kb object model + ground-truthed bisect
**The summary's "10 AI source-file TUs" do NOT exist as kb.json objects.** kb.json has only **4 AI
objects** (original link units; our src/halo/ai/*.c compile INTO these): `actors.obj` (124 ported),
`actor_looking.obj` (120), `ai.obj` (35), `actions.obj` (35). `game_engine.obj` (237) is separate.
setswath.py flips by **.obj name only** (fn schema = {addr,decl,ported}, no source path). So the
real bisect granularity is these 4 — earlier "perception-core {actors,actor_looking,actor_perception}"
= really {actors.obj, actor_looking.obj}; "7 non-core" = really {ai.obj, actions.obj}.

**Box-liveness ground truth of the deployed "3b" build** (getmem prologue byte @ one fn/obj; `68..C3`
PUSH→RET trampoline = ON/our-lift, `55 8B EC` = OFF/original): actors.obj=ON (0x2a360), actor_looking.obj
=ON (0x13dd0), ai.obj=**OFF** (0x3f5f0), actions.obj=**OFF** (0x1c030), game_engine.obj=ON (0xa81a0).
=> "engages-at-close" build = {actors,actor_looking,game_engine ON; ai,actions OFF}. Full(all ON)=frozen.
**Culprit ∈ {ai.obj, actions.obj}.** game_engine ON in both → not the differentiator.

**REFRAME (user hands-on, current build):** at door range grunts stay action="alert"(0x6c=2), no shoot;
move POINT-BLANK → action cycles uncover/search/cover, disposition→3, **and they SHOOT AND HIT.** So the
**aim pipeline is NOT broken** (prior "aim way off" was a symptom of low awareness / half-tracking). The
defect = **engagement/awareness escalation THRESHOLD too tight**: turning ai.obj+actions.obj ON suppresses
escalation so grunts only engage point-blank instead of at door range (stock engages at door).

**Cycle 4 (set, NOT yet built):** `setswath.py ai.obj` → actions.obj ON, ai.obj OFF, perc-core ON
(crash-free). break(frozen-close)=actions.obj culprit; engage-close=ai.obj culprit. actions.obj is prime
suspect (combat-transition 0x204f0 + grenade-throw 0x205a0 → fits slow-engage AND over-aggressive grenades).

## ★ CHECKPOINT/CORE BOOT WORKFLOW (reusable — kills the position confound)
Debug build runs `d:\init.txt` as HaloScript at boot (console_startup, src/halo/main/console.c:314).
deploy_xbox.py always uploads repo-root `init.txt`→`d:\init.txt` (no flag; edit the file to control boot).
Default `init.txt` = `game_difficulty_set impossible` + `map_name a10` → loads a10 **FRESH** (why we replay).
- Console open = **`~`** (tilde). Verified XBE command strings: `core_save` ("saves debug game state to
  core\<path>" → d:\core\<name>), `core_load`, `core_load_name_at_startup` ("...as soon as the map is
  initialized"), `game_revert` (revert to last checkpoint), `map_name`.
- **Boot into exact encounter state:** one-time at the spot → `~` → `core_save grunts` (writes d:\core\grunts).
  Then init.txt = `game_difficulty_set impossible` + `map_name a10` + `core_load_name_at_startup grunts`.
  Every deploy boots a10 then overlays that core → identical player pos/weapon/grunt state each cycle.
- Core = game-state heap only (NOT XBE code) → new bisect code runs on restored state. Save BEFORE grunts
  perceive player so each boot tests escalation fresh. F4=Select-This-Actor shows the AI debug overlay.
- d:\core already had stale cores {derelict, halo} (3.42MB each) — unknown state; make a fresh one.
- init.txt currently EDITED to add `core_load_name_at_startup grunts`; build/deploy HELD until user core_saves.
- CORRECTION: `core_save` takes NO arg → writes `d:\core\core.bin`; init.txt uses `core_load_at_startup`
  (no-arg). Reload after death = console `~` then `core_load` (core_load_at_startup only fires at boot;
  death reverts to the game's own in-mission checkpoint baked in the core, set earlier than the hallway).

## ★ CONTROLLED-CORE RESULTS (2026-06-24, identical hallway start every cycle)
Judging axis: proactive-engage-at-range (unprovoked) = good/stock; engage-only-when-close = faithful;
freeze-even-point-blank = the bug. Reticle-red / return-fire = REACTION, not proactive (don't count).
- **Cycle 4** (actions.obj ON, ai.obj OFF, perc-core ON): grunts engage when close (reactive to look/shoot),
  no proactive at range. == 3b. So actions.obj ON alone does NOT reproduce the point-blank freeze.
- **All-AI-off** (all 4 AI objs OFF, game_engine+OBJ ON): SAME — sit until close, NO proactive engagement.
  => **at-range passivity is FAITHFUL** (stock behaves identically). Kills the "compound/perc-core gradient"
  theory — perception-core is NOT causing an at-range regression. The "door small no glass" / encounter
  scripting legitimately holds engagement until the player is close.
- **The only real defect = the CLOSE/point-blank FREEZE**: full-patched grunts froze even at 4-5u
  (awareness 0, never engage); stock + ai-OFF builds engage when close. Delta cycle4(ai OFF, engages)
  vs full(ai ON, freezes) = **ai.obj**. => ai.obj is the prime culprit for the freeze.
- **Cycle 5 (next):** `setswath.py actions.obj` = ai.obj ON, actions.obj OFF, perc-core ON. Get CLOSE:
  freeze-even-close => ai.obj ALONE is the culprit (dive ai.obj 35 fns); engages-close => compound ai+actions
  (then re-confirm full on core). Boot risk: actions OFF while ai ON may hit ai↔actions coupling crash; if so,
  test full-patched on core instead to re-confirm the freeze under controlled position.

## ★ Cycle 5 result + the pivot to re-confirming FULL (2026-06-24)
- **Cycle 5** (ai.obj ON, actions.obj OFF, perc-core ON) BOOTED (no ai↔actions crash). Result:
  long-range = no engage (faithful); **REAL close → action → "fight" (engages)**, else "alert".
  => ai.obj ALONE does NOT freeze close engagement.
- Combined: cycle4 (actions-only) engages close; cycle5 (ai-only) engages close. **Neither single object
  freezes.** So the full-build "froze even point-blank" is EITHER compound (needs ai+actions together) OR
  was the position/capture ARTIFACT the advisor warned about (original A/B was position-uncontrolled).
- **DECISIVE TEST DEPLOYED: FULL-patched (all 4 AI ON) on the SAME controlled core.** If full also reaches
  "fight" when real-close → NO freeze under controlled position → original A/B freeze = artifact → patched
  AI may be behaviorally FAITHFUL (passivity-at-range is stock; close engagement works in every config).
  If full stays "alert"/frozen even real-close → COMPOUND bug (ai.obj+actions.obj interaction) → hunt the
  interacting fns. Watch the "real close" THRESHOLD vs all-off too (subtler gradient) + grenade behavior.

## core.bin → unicorn: VERIFIED feasible (2026-06-24)
Downloaded real d:\core\core.bin (3,428,352 B) via `xbdm_rdcp.py "getfile name=d:\\core\\core.bin"
--binary-length 3428352 --output ...` (4-byte RDCP size prefix precedes content). Content matches the
feasibility note: +0x04="levels\a10\a10" (scenario), +0x104="01.10.12.2276" (build). core.bin = flat dump
of game-state pool, VA = file_off + 0x80061000. Usable for unicorn_diff --state-snapshot once the ~6
out-of-heap global pages (0x5a8d50 obj table, 0x6325a4 actor table, etc.) are seeded separately. Full
analysis + converter sketch: [[reference_corebin_unicorn_feasibility]]. Probe saved artifacts/corebin_probe/.

## ★ FULL-patched on controlled core — FREEZE DOES NOT REPRODUCE (2026-06-24)
User result (full = all 4 AI ON, same core): (1) **engages** (NOT frozen) — "earlier but not as early/easily as
unpatched"; (2) threshold "better but not the same as unpatched"; (3) one grunt **threw a plasma grenade**
("shouldn't happen in original"). => The dramatic point-blank FREEZE from the original A/B **does not reproduce
under controlled position** — it was almost certainly the position/LOS capture ARTIFACT the advisor warned of.
Residual signals: (a) a SUBTLE engagement-threshold feel (below eyeball resolution — STOP bisecting by eye);
(b) a discrete grenade anomaly (verify it's even anomalous: grunts DO throw plasma grenades in CE generally).
**Advisor pivot: make it NUMERICAL.** Same core, player COMPLETELY STATIONARY (no approach = no confound),
read grunts' aw 0x268 / agg 0x6e / disp 0x6a via cap2.py at load instant + after ~10s dwell, full vs all-off.
Identical numbers from identical saved state => code paths equivalent, "threshold" = approach-noise => FAITHFUL
verdict (write it up). Divergent => real controlled signal => localize. Sharpest extra reads: difficulty global
(game_difficulty_level_get 0xa7460) + aggression inputs (actor+0x72 [WRITTEN in ported actions.c:1100 =2!],
+0x74, table 0x255f18) feeding unported recompute 0x302b0. DO NOT start compound ai+actions code hunt — the
freeze premise is undercut. Lead-if-divergent: actions.c:1100 `*(short*)(actor+0x72)=2` is a ported write to an
aggression-recompute input — prime suspect for both slow-escalate AND wrong grenade-eligibility (one param).

## ★ CONTROLLED STATIONARY NUMERICAL A/B — FULL == ALL-OFF (IDENTICAL) (2026-06-24)
Same core, player stationary @(-8.3,-2.2), 3 samples/12s, cap2.py. Results BIT-IDENTICAL full vs all-off:
- close grunts g6/g10/g11 @4-5u: st2 act2 agg0 **aw0** inc0 ppst0 (player tracked, awareness flat — even at 4-5u,
  standing still, no escalation; escalation needs player MOTION/aim/fire, not static proximity → faithful idle).
- far grunts g2/g4/g7/g8 @34-44u: st3 act3 **agg7** aw0 (aggression recompute identical both builds).
=> Per advisor's pre-committed criterion: flat numbers = patched per-tick AI state evolution == stock from an
identical frozen state → the "passivity/threshold feel" was APPROACH/LOS noise, NOT a code defect. Strong
FAITHFUL signal. CAVEAT: stationary does NOT exercise the escalation-during-approach path (aw stays 0 both) —
the user's actual complaint. Best controlled escalation test = a DISCRETE repeatable stimulus (fire 1 shot at a
fixed grunt from the core spot) then read aw/agg evolution full vs all-off; firing is far more repeatable than
walking. Grenade claim still OPEN (advisor: grunts DO throw plasma in CE generally; verify all-off NEVER vs full
RELIABLY under identical provocation). Artifacts: full_still / alloff_still blobs in artifacts/ai_regression/.

## ★★ CONFIRMED REGRESSION — single-shot escalation diverges (2026-06-24) ★★
Controlled escalation A/B: same core, fire EXACTLY ONE shot (floor/wall, non-lethal stimulus), cap3.py 2s sampling.
- **STOCK (all-AI-off): 1 shot → close grunts g6/g10/g11 escalate FULLY**: ppst 0→3, aw 0x268 0→5→**10**,
  inc→**1**, st 0x6a→**3**, agg 0x6e→**7**, action cycles 2→4→5→6. (artifacts/ai_regression/*alloff_fire*)
- **FULL-patched: 1 shot → ZERO escalation**: g6/g10/g11 stay st2 act2 agg0 **aw0** inc0 **ppst0** across all 14s.
  User hands-on agrees: "needs more than one shot." (artifacts/ai_regression/*full_fire*)
- DIRECT cause chain: aw 0x268 (recomputed by UNPORTED 0x302b0) can't climb because its input **ppst=prop+0x24
  (player perception-prop state) never advances 0→3** on the gunshot stimulus. => the lifted SENSE / threat-
  detection that should fill the perception record + advance prop+0x24 is broken. => points at PERCEPTION-CORE
  (actors.obj / actor_looking.obj), NOT the aggression actor+0x72 (actions.obj) the advisor named. Matches old
  [[project_a10_passive_alertness_root_traced]] (prop+0x24 stuck; sensory sub-data prop+0x2c/+0x80 empty in patched).
- **THE SHARP ORACLE for bisect now**: build config → core_load → ONE shot → cap3: does aw climb to ~10 (good)
  or stay 0 (bug)? Objective, no eyeball. Next: re-bisect with this oracle — test 3b (perc-core ON, ai+actions
  OFF): aw stays 0 => perc-core culprit (split actors vs actor_looking); aw climbs => ai/actions. Prior eyeball
  "cycle4/cycle5 engage when close" was MOVEMENT-confounded (approach is a stronger/longer stimulus than 1 shot).
