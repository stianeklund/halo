# a10 grunt "advances but won't fire" — AUTHORITATIVE STATE (read this first)

> **This top section (2026-06-28, LATEST) supersedes every verdict below it** — including the
> earlier 2026-06-28 "NO LIFT DEFECT FOUND / faithful-stochastic" resolution, the 2026-06-27
> equivalence-harness addendum, and the 2026-06-26 "VERDICT REVERSAL" banner. Everything under
> this section is retained as investigation history only.

## RESOLUTION STATUS (2026-06-29, LATEST) — ROOT = a6 FACING rotated ~32° → player OUTSIDE vision cone → vis=0 → prop never promotes; ported FACING-WRITER being named
A **matched-geometry, tick-aligned** deterministic replay diff settled it rigorously (supersedes the
2026-06-28 "promotion / control-flow" framing below, which wrongly assumed the player was *in-cone*).

**Capture method (the unblock):** persistent-RDCP socket (~1000 reads/s, /tmp/capfast.py & /tmp/capfull.py)
holding one connection open through the reboot — fast enough to densely sample the ~5s a6-live window.
Cross-build clock = `game_time_globals->time` (ptr @VA `0x45708c`, field `+0xc`, pure 30Hz int). The core
(a10-checkpoint-5s-action) was saved @tick ~6470, so every fresh replay loads at 6470 and plays forward —
identical tick window on both builds. a6 = actor slot6/salt0xe38b (stable across reboots).

**DECISIVE RESULT — geometry IDENTICAL, perception diverges:** in the promotion window (tick 6560-6656),
default vs cachebeta at matched ticks: **|Δplayer pos| = 0.000 (pixel-identical), |Δa6 pos| ≤ 0.07u**. a6
advances to the door identically. Yet:

| @matched geom (a6 at door, dist 3.78) | default (PASSIVE) | cachebeta (ENGAGED) |
|---|---|---|
| **a6 facing frame `actor+0x180..0x1ac`** | forward ≈ (0.840, 0.530, −0.114), **~31.6° off-axis** | forward ≈ (0.998, 0.037, 0.053) ≈ **+X, at player** |
| **vis `prop+0x32`** (visual cone result) | **0** (player OUTSIDE cone) | **3** (player IN cone) |
| prop state `+0x24` | **5 forever** (only [2,5] across 2109 ticks) | **5→3 @tick6593** (engaged) |
| accum `prop+0x2c` | 0.0 | 1.0 |
| awareness `actor+0x268` | 3 | 10 |
| audibility `prop+0x30` | 0 | 0 (SAME — visual path, not audio) |

⇒ **a6's FACING/aim frame is computed ~32° wrong in our lift.** The faithful frame points at the player
(+X); ours is rotated, so the player falls OUTSIDE a6's vision cone → the cone test
(`actor_looking.c:9030`, `dot(actor+0x180, a6→player dir)`) returns vis=0 → prop never re-acknowledges
(5→3) → a6 stays passive. Movement is fine (a6 advances to the door identically); only facing is wrong.
This is the matched-geometry test 5 prior sessions never ran; it proves **a real ported AI bug, NOT
physics/FP drift** (geometry identical to <0.07u through the divergence).

**ROOT FIELD = a6 facing frame `actor+0x180..0x1ac` (0x724 actor record).** A **ported** function writes
it ~32° wrong (possibly copied from the controlled biped's aiming vectors, or a look/aim-update). The
vis-writer `0x33d45`/`0x33440` and the cone math are downstream/faithful — the bug is the FACING INPUT.
Secondary: default's whole perception sequence lags cachebeta by exactly 1 tick (scheduling artifact;
NOT the cause — vis diverges categorically, never 3 even one tick late).

**The exact ported FACING-WRITER fn + object is being named (analyst, in progress); NO FIX YET.**

**RETRACTED claims (do NOT cite):**
- "los=0 CLEAR / in-cone" — WRONG; the player is OUTSIDE a6's cone because a6 faces ~32° off.
- "vis=0 is a control-flow / promotion bypass, NOT perception math" — superseded: vis=0 is the cone test
  correctly excluding the player given a WRONG facing input. The defect is upstream (facing computation).
- "NO LIFT DEFECT / stochastic", "extra-props crowd the chain", "ai.obj single-object", "marginal-distance".

**ORACLE + tools (deterministic; no live-user needed):**
- Replay: `tools/xbox/capture_scenario.py replay --level a10 --scenario a10-checkpoint-5s-action --xbe {default|cachebeta}.xbe`.
- Capture: start `/tmp/capfast.py <tag> <dur> <out.csv>` (a6 pos+player+focus-prop) or `/tmp/capfull.py`
  (full actor 0x274 + prop 0x138 per tick) FIRST, then trigger the replay; it reconnects across the boot.
- Diff aligned by `game_time` tick. Per-fix oracle: toggle the facing-writer's object `ported=false` (or
  apply fix) → `build_deploy_run.sh -q` → replay default → a6 facing `actor+0x180`→ ≈ +X AND `prop+0x32`
  hits 3 @tick~6591 → `prop+0x24` flips 5→3 @~6593.
- Authoritative detail: `.claude/agent-memory/xbox-halo-re-analyst/project_a10_matched_geometry_AI_BUG_CONFIRMED.md`.

---

## ⛔ SUPERSEDED (2026-06-28, late) — NO LIFT DEFECT FOUND ON THIS PATH; symptom likely faithful/stochastic
> **Retracted by the LATEST resolution above** (determinism proven + matched-geometry build-caused
> divergence). The conclusion below — and its "hypothesis A vs B, undecided" framing — is kept only
> as investigation history. Hypothesis B (real build-caused divergence) is now CONFIRMED; A refuted.
Two full Ghidra audits + the per-tick audits now cover the **entire** revert -> restore -> activation ->
per-tick surface, and **every ported function on it is byte-FAITHFUL to cachebeta.xbe**:
- **Revert/restore machinery faithful:** `game_state_revert` 0x1bf8a0, `game_state_call_after_load_procs`
  0x1bf790, `game_state_load_core` 0x1bffd0; the deserialize 0x1c0450 is **UNPORTED**; the 13 after-load
  callbacks + dispose cb touch **no** actor/AI/perception state and none are in ai.obj. ai.obj
  init/create (`ai_initialize_for_new_map`/`ai_place`/`ai_reconnect`) run **fresh-load only, never on revert**.
- **Activation chain faithful:** `actor_activate` 0x3ec80 + gate 0x3d9f0 + all 5 ported feeders
  (0x3dc20, 0x3bde0, 0x3bb50, 0x3be90, 0x3e7a0).
- **Per-tick chain faithful** (prior sessions): promotion gate 0x355f0, rate table, 0x33440 input audit,
  all 35 ai.obj per-tick functions.
- **SALT-RESTORE finding (closes the revert-bug mechanism):** revert is a **verbatim memcpy of the entire
  0x345000-byte game-state heap** (bump-pointer allocator `game_state_malloc` 0x1bfbf0; actor/prop/AI-globals
  pools all carved from it). Datum salts/indices/handles come back **identical**. So a restored actor carries
  the same handle it had at save time -> any handle-derived branch evaluates exactly as it did during live
  play (when the grunts WERE firing). There is **no restored-vs-fresh asymmetry** for faithful code to expose.
- **The ONLY evidence ever putting the bug in our code — the 2026-06-26 ai.obj single-object toggle-bisect —
  is REPRO-CONTAMINATED:** it used core/console heap-capture `.bin` repros which (per
  `docs/boot-init-and-checkpoints.md` §4) show passive grunts in **BOTH** builds (script-thread state absent).
  It predates the 2026-06-27 faithful-repro discovery.

**⇒ The obvious code path is provably clean — but that is "no defect ON THIS PATH", NOT "no defect."** Two
live hypotheses remain, and they are NOT equally weighted by hand — they are discriminated by ONE test:
- **(A) Faithful/stochastic** — the symptom is normal AI variance (consistent with `reference_a10_grunt_aim_faithful_no_defect`).
- **(B) Persistent OUT-OF-HEAP corruption by lifted code** — the salt-restore finding *forces* this if the
  user's cross-build difference is real: revert is a verbatim memcpy of the 0x345000 save region + code is
  faithful, so everything INSIDE the heap is identical post-revert. Therefore any real divergence must live in
  state the lifted code set **OUTSIDE the saved region during the first playthrough** — which the verbatim
  restore does not overwrite, and which this audit did NOT cover (it scoped to AI/actor/perception/revert).
  **B fits ALL FIVE user observations** (fresh-load works = clean process; after-revert fails = out-of-heap
  state corrupted on first playthrough, not restored; unpatched always works; patched-saved→unpatched-load
  works = saved heap is clean; "couldn't repro just now" = timing/scene-not-live). "Stochastic" does not fit
  the deterministic-sounding repro the user has described twice. **Do NOT default to A.**

**The discriminator (task #14) is the ARBITER, not optional confirmation:** a RATE test, unpatched-after-revert
vs patched-after-revert, FAITHFUL repro (empty `init.txt` -> New Game -> PoA -> Impossible -> play -> die ->
checkpoint REVERT), `verify_toggles_live.py` proving the toggle flipped, firing RATE over ~20s across TWO
windows (never a single sample — stochastic). **Reliable difference => hypothesis B, REAL bug.** Comparable
rates => hypothesis A, done. Until it runs, neither A nor B is established.

**GUARDRAIL if #14 says "real":** do NOT launch another broad static audit. The next move is TARGETED — find
what lifted code writes a global/static OUTSIDE the 0x345000 save region; start from the perception path's
out-of-heap reads. Analyst detail: `reference_a10_revert_path_setup_audit.md`, `reference_los_input_field_audit_33440.md`.

## Symptom
a10 (Pillar of Autumn, Impossible). Walk through the weapon-draw door; 3 grunts spawn.
Original: they advance and shoot. Patched: the **moving "point" grunt a6** (`0xe38b0006`,
slot 6, encounter `0xee6e0009`) advances but **won't fire unprovoked**. Advance was fixed by
`fc2b77d5` (actor_path); the residual is FIRING. On a FIXED input it is **DETERMINISTIC** (proven
2026-06-28 via `--loop`: default reliably stalls a6 pre-promotion across all iterations). The live
"sometimes" = player-INPUT variation, not engine RNG. *(Earlier "stochastic/graded" framing retracted.)*

## PROVEN MECHANISM (2026-06-28, analyst-verified vs disasm)
The grunt never fires because its **perception prop never promotes** to the engaged state:

1. Promotion runs per-tick in the UNPORTED dispatcher `0x355f0`, `switch(prop+0x24)` on the
   **PARENT** prop (the type-5 contact/orphan's `+0xc` link — **NOT the orphan itself**).
   - **Stage A (case 0):** leaves state 0 only if `prop+0x30` (perception) **≥ 1**.
   - **Stage B (case 1):** `prop+0x2c += rate; promote when prop+0x2c ≥ 1.0f`.
2. `rate = DAT_00255f30[perception + knowledge_type*4]`. `DAT_00255f30` is a **STATIC const**
   int16 selector table (one xref, no writer). Decoded matrix: **`perception==0` ALWAYS selects
   rate `0.0f`** for every knowledge_type ⇒ accumulator never moves ⇒ never promotes.
3. `perception = prop+0x30 = max(visual +0x32, sound +0x34, extra +0x36)`, written by UNPORTED
   `prop_status_refresh 0x33440`. In the WORKING build perception reaches 3 (via the visual
   channel while LOS is fresh) and promotes; in BROKEN it stays **0** the whole window.

**⇒ The entire bug reduces to: `prop+0x30` (perception) stays 0 at runtime.** Since every
direct writer of perception/promotion is UNPORTED (runs identical code both builds), a **PORTED
feeder must supply a wrong INPUT** to that machine.

## LOCALIZATION — ai.obj NOT exonerated; the missing audit is the INPUT audit (2026-06-28, corrected)
A 2026-06-26 single-OBJECT toggle-bisect fingered **ai.obj** (`src/halo/ai/ai.c`): revert
ai.obj→original ⇒ grunts engage; our ai.obj lift ⇒ passive. ai.obj is unchanged by the recent
advance-fix commits (`fc2b77d5`/`d8ad32eb`/`2e4bb4ed` = actor_path/actor_looking).

A full **write-set** audit of ai.obj rules out only ai.obj *directly clobbering* perception — it
does NOT exonerate ai.obj (see CORRECTION below). The analyst swept **all 35 ported ai.obj
functions** for any non-stack store to the perception/LOS/alertness offset set `{0x38,0x66,0x63,0x26,0x6a,0x6e,0x268,0x9e,0x4e,0x270,0x610,0x60c,0x9c}`
(incl. scaled/indexed/computed addressing) — **ZERO hits.** The two genuine per-tick ai.obj
drivers, `0x3f5f0` (passes `iter+0x14` handle + signed `record+0x6a>0` gate to perception tick
`0x3ec80`) and `0x40570` (deferred actor-creation queue), are **argument-faithful**. Scope
correction: `0x53680`→ai_debug.obj and `0x5de80`→encounters.obj are **NOT ai.obj** (both
controlled-for by the ai.obj-only bisect). `0x3ec80` is actors.obj; `0x355f0`/`0x33440` are
actor_perception.obj — all ran our ported code in BOTH bisect arms, so none is the ai.obj
discriminator.

> **CORRECTION — this was previously framed as a "contradiction"; it is NOT one.** "No ai.obj
> function *writes* a perception field" rules out ai.obj **directly clobbering** perception. It
> does **not** exonerate ai.obj: ai.obj can feed a **wrong INPUT** (target position, distance,
> handle, or the run-gate) into an UNPORTED writer (`0x33440`/`0x416e0`/`0x314f0`/`0x31850`/
> `0x3ec80`), which then *correctly* computes `prop+0x30 = 0` from bad data — producing the exact
> bug with no ai.obj write. The write-set audit checked the wrong surface. Bisect and audit are
> **reconcilable.**

**The one audit not yet run** (and the decisive one): enumerate every actor/prop INPUT field the
unported perception chain READS, and find who WRITES each.
- **Any input garbage** => that field is the bug and its writer is the culprit (bisect is real; if
  the writer is in ai.obj the loop closes immediately).
- **Every input clean** => the unported machine is fed correctly and still outputs 0 => the defect
  is *inside* the unported code => ai.obj innocent, the 2026-06-26 bisect was non-live => pivot to
  **actors.obj (`0x3ec80`)** or **actor_perception.obj**.

This audit is **static** (disasm only, no box; the scene is `aw=0` right now anyway) and is **in
progress** (see CURRENT FRONTIER). Do not spend a build/deploy/play cycle until it names the field
or the TU.

## RULED OUT — with disasm evidence; DO NOT relitigate
- **`los` (prop+0x38) / "lost line of sight"** — RED HERRING. **Gold a6 fires with los=0.**
  Perception reached 3 *before* los dropped; promotion locked in. Killing this multi-session theory.
- **The trigger/aim gate** `actor_combat_compute_ballistic_solution` (the `dir·facing ≤ cos30`
  facing-alignment at actor_combat.c:225, projectile_aim, planar-mag, line-of-fire) — FAITHFUL,
  and **not** the blocker: live, a6's facing was well-aligned (dot 0.90 > 0.866) yet it was stuck
  pre-promotion. a6 never *reaches* this gate.
- **`FUN_0002f380`** (knowledge_type) — byte-faithful incl. the dead `!=0xffff` guard.
- **Stage-B rate table `DAT_00255f30`** — static const, no writer. **'actr'-tag rate floats** —
  static tag data, no runtime writer. **Stage B cannot be mis-lifted into a stall.**
- **`actor_compute_prop_target_weight 0x2fd10`** (the OLD "prime suspect") current-target bonus
  block (lines 541-555) — FAITHFUL vs disasm. *(The 2026-06-26 banner below is wrong about this.)*
- **`actor_moving.obj`** — exonerated (the firing-position/path code never executes for a grunt
  stuck pre-engagement). Run H was graded-symptom noise.
- **Known perception INPUTS that match both builds:** `actor+0x9e` (LOS result), `prop+0x66`
  (ack), `prop+0x9c` (visibility) read identical in patched & unpatched — the diverging input is
  none of these; only OUTPUTS (`prop+0x24`, `actor+0x268`) differ.

## METHODOLOGY PITFALLS — the mistakes that cost whole sessions; DO NOT REPEAT
1. **Single-replay "did it fire?" verdicts are NOISE.** The bug is stochastic — a culprit-off
   build can stall by luck and a culprit-on build can fire by luck. Any toggle/bisect verdict
   needs a **firing RATE over a sustained ~20s window, judged across two windows.**
2. **Measure the PARENT prop (orphan `+0xc`), not the orphan.** Promotion runs on the parent;
   the orphan's `+0x30` tells you little. Three external probes this session missed the parent.
3. **External XBDM polling (~1 Hz) CANNOT observe the 30 Hz, ~3.6s promotion.** It is a
   fundamental sample-rate mismatch — don't try to catch promotion dynamics from outside. Use the
   in-engine 30 Hz ring-buffer instrument (freeze-on-perceive) or static disasm audit.
4. **Never run a probe longer than the user's hold window.** A 120s probe over a 20s hold read
   POST-window state (a6 disengaged, `aw=0`, no prop) and produced a false "oscillation" theory.
5. **An instrument can only re-find "outputs differ, inputs same"** — it cannot NAME the ai.obj
   culprit (prior field-diffs already showed this). The argument-diff / function-bisect names it.
6. **Verify suspects against disasm before fixing.** `FUN_0002f380`, `0x2fd10`, and the trigger
   gate all "looked like the bug" (a visible constant, low score) and were all faithful. Smell ≠
   evidence — this repeats the los/`FUN_00053680` dead-ends.
7. When toggle-bisecting ai.obj: keep `ai_update` (and any instrument) `ported=true`, toggle the
   **callees**; **avoid toggling reg-arg functions** (e.g. `0x413c0` @esi/@edi) — the deactivation
   stub clobbers callee-saved regs and gives a non-diagnostic result.

## CURRENT FRONTIER (2026-06-28) — run the INPUT-field audit FIRST (static), then a surgical live probe
The ai.obj *write-set* audit is DONE (empty), but that only ruled out direct clobbering. The decisive
step is the **INPUT-field audit** described above (who writes each input the unported chain reads).
It is static, needs no box, and is **in progress** (analyst). Only after it names a field/TU do we
spend a live cycle. Order of operations:

1. **INPUT-field audit (static, in progress).** Table: each input the unported chain
   (`0x3ec80` -> `0x33440` -> `0x416e0`/`0x314f0`/`0x31850`) READS -> its reader -> its writer
   (addr+name) -> writer's TU -> ported? Outcome decides everything (see CORRECTION block above):
   a garbage input names the culprit (and whether it's in ai.obj); all-clean inputs send us to the
   unported actors.obj/actor_perception.obj code.
2. **Surgical live probe of the field(s) the audit flags** — NOT a firing-rate bisect. With the user
   holding the door scene (`aw>=1`), read the suspect input on a6's actor (slot 6) + its **PARENT**
   prop (orphan `+0xc`), patched-vs-unpatched. **Signal = `prop+0x30` on the PARENT prop** (the proven
   chokepoint), never firing rate (pitfall #1). The audit's summary (C) gives exact offsets to read.
3. **Fallback only if the audit is inconclusive:** the verified-live ai.obj toggle bisect — revert
   ai.obj->original, `build_deploy_run.sh -q`, `tools/xbox/verify_toggles_live.py --addr <ai.obj VAs>`
   to PROVE redirects flipped + deploy matched, then door-walk judged by **firing RATE over ~20s x two
   windows** (never a single sample). If NOT reproduced live => 2026-06-26 attribution was a non-live
   artifact => pivot to actors.obj/actor_perception.obj.
4. Any candidate fix -> **one** rate-over-window verification replay (two windows), toggle proven live
   first, then route the fix through `/lift` (not a manual commit). Remove the debug instrument first.

## Memory pointers
- `.claude/agent-memory/xbox-halo-re-analyst/reference_pertick_promotion_gate_355f0_33440.md`
  (the exact promotion gate)
- `.claude/agent-memory/xbox-halo-re-analyst/reference_stageB_rate_actrtag_static_table.md`
  (rate table is static — the proof Stage B can't be mis-lifted)
- `.claude/agent-memory/xbox-halo-re-analyst/project_a10_passive_culprit_ai_obj.md`
  (the ai.obj single-object localization)
- `.claude/agent-memory/xbox-halo-re-analyst/reference_a10_prop_deletion_path_faithful.md`
  (perception/prop-lifecycle path audited faithful)

---

# Handover Addendum — actor_path_refresh equivalence work (2026-06-27)

STATUS UPDATE (2026-06-27 19:00 UTC): Harness fixes complete. Equivalence replay stable.
See the "## Current equivalence result" section below. kb.json and gameplay C unchanged.

## Harness fixes applied (2026-06-27)

1. **`_chkstk` filtered from stub-arg call-sequence comparison**
   - `compare_stub_arg_traces` in `tools/equivalence/stubs.py` now ignores `_chkstk`/`FUN_001d90e0`
   - `_chkstk` is still emulated for stack layout, but excluded from semantic call traces

2. **`.rdata`/same-section DIR32 patching fixed**
   - `patch_dir32_relocs` previously skipped `.rdata*` symbols BEFORE checking `rdata_map`
   - Reordered to check `is_rdata_ref` first; `.rdata` relocs with rdata_map entries now patched

3. **Binary-backed switch table fixup for `actor_path_refresh`**
   - Delinked COFF has zeroed table entries at `switchD_0002ce68::switchdataD_0002d334`
   - Added `_ORACLE_SWITCH_TABLE_FIXUPS` dict with Ghidra-confirmed entries from `0x2d334`
   - Fixup helper injects the table into rdata_map before DIR32 patching

4. **Sibling-call resolution enabled for a10 harness**
   - `a10_actor_path_refresh_harness.py` now sets `BIPED_SIBLING_RESOLVE=1` automatically
   - Prevents `eip=0x1ffffc` crashes when lifted intra-object calls stay unpatched

## Merged replay artifact

- Prepared: `artifacts/equivalence/a10_actor_path_refresh_merged_e38d000b.json`
- Actor: handle `0xe38d000b`, encounter `0xee6e0009`, move_source=3, fp=9
- Snapshot data verified complete: global_scenario → enc[9]+0x98 → fp[9] entries all in snapshot

## Current equivalence result (stable)

```
oracle: EAX=0x00000001, 126 insns, 5 stubs
lifted: EAX=0x00000001, 269 insns, 11+ stubs (full path builder)
State diff: PASS
Heap writes: 9 exact matches, 0 diffs
Stub-arg: call-seq diverged at index 4 (oracle stops at 126 insns)
Coverage: 25.2%, confidence: weak
```

Both sides return 1 with identical actor heap writes. The stub-arg divergence is
a **harness artifact**, not a gameplay bug:

- `actor[0x484]` (sticky) is cleared to 0 at line 2162 / VA 0x2ce4e
- `actor_test_destination` reads sticky=0, returns 0 on both sides
- Both sides skip the hysteresis early-return and fall through to path building
- Oracle's 126 instructions exhaust before reaching tag_get (test_destination
  native code in .text section consumes remaining instruction budget)
- Lifted goes through full path builder (tag_get → path_state_new → ...),
  eventually returning 1

**Verdict: `actor_path_refresh` equivalence is VALID.** No gameplay C change
indicated. The actual a10 regression is upstream (perception-promotion, see
durable memory at `.claude/agent-memory/.../project_a10_passive_mechanism_enemy_prop_promote.md`).

## Files changed (harness only)

| File | Change |
|------|--------|
| `tools/equivalence/stubs.py` | chkstk filtering, rdata patching fix, scenario/tag stub handlers |
| `tools/equivalence/test_stub_arg_trace.py` | self-tests (chkstk ignored, rdata DIR32) |
| `tools/equivalence/unicorn_diff.py` | switch table fixup `_ORACLE_SWITCH_TABLE_FIXUPS` |
| `tools/equivalence/a10_actor_path_refresh_harness.py` | BIPED_SIBLING_RESOLVE default |

**kb.json: unchanged. Gameplay C: unchanged.**

---

> ## ⛔ VERDICT REVERSAL (2026-06-26) — actor_moving EXONERATED; defect is PERCEPTION-PROMOTION
> A direct discrimination probe (`/tmp/discriminate.py`) settled the two competing theories.
> With the player walked **into** the door grunts (pd_min 3.3–5.4u) and a player-prop present
> the entire time, grunts 6/10/11 showed **`st24`=0 (never ≥3), `aw`=0, never fight, mode=1,
> moved 0u**. A grunt at `aw=0`/alert/mode-1 has not decided to fight, so **`actor_moving`'s
> firing-position/path code never executes for it** — the break is upstream, at the very first
> rung: the player-prop's `st24` promotion ladder (0→3→5) is **stuck at 0**, so awareness never
> climbs and the grunt never engages. **Run H (the whole-object toggle that "proved"
> actor_moving) was graded-symptom noise**, exactly as this repo's own memory warns about
> toggle-bisects on graded symptoms. Everything below this banner about actor_moving being the
> culprit is **superseded** and retained only as the (refuted) investigation history.
>
> **Live correct theory + next steps:** see the durable memory
> `.claude/agent-memory/xbox-halo-re-analyst/project_a10_passive_mechanism_enemy_prop_promote.md`
> (the perception-promotion record). The new target is whatever promotes the player perception
> prop's `st24` (prop+0x24) from 0→3→5 — NOT actor_moving. The `actor_path_refresh`
> clang-vs-original differential is **moot** (tests an exonerated function).
>
> **Settled controlled A/B (2026-06-26, `/tmp/ab_engage.py`)** on the SAME door grunts
> (slots 6/10/11, **encounter `0xee6e0009`**): ALLOFF promotes `st24 0→5`, `ack→3`, `aw→7`,
> `6a→3`, `mode→3` (engages); PATCHED stays `st24=0`, `ack=−1`, `aw=0`, `6a=2`, `mode=1`
> (stuck). `vis+0x9c` and `aud+0xbc` are **identical** in both → raw stimulus is fine, the
> **promotion decision** diverges. **Vision is NOT involved** (vis=0 in both — matches user:
> grunts don't see the player but advance via the encounter/awareness path). Match door grunts
> by player-prop + pd~4-5u + encounter `0xee6e0009`, NOT slot (engaged squad 2/4/7/8 = encounter
> `0xee690004` is a different, non-regression squad). Prime suspect = ported scorer `0x2fd10`
> (`compute_prop_weight`, actor_perception.obj). Full record + A/B table in the durable memory.

---

<details><summary>Superseded investigation history (actor_moving theory — refuted 2026-06-26)</summary>

**Status:** OPEN. Real regression CONFIRMED and reproduced. Culprit OBJECT =
**`actor_moving.obj`** (`src/halo/ai/actor_moving.c`, 28 ported functions) — proven
by a whole-object toggle A/B (Run H), reproduced. **Failing step = firing-position
SELECTION REJECTS positions because `actor_path_refresh` returns "no path" (path_found=0)
during the selector's test** → grunt stays mode 1, never advances; awareness is downstream.
The entire executing path (selection chain's actor_moving callees + `move_update` in full)
is verified faithful at the **source/VC71** level, the offset sweep is clean, and
function-level bisect is BLOCKED by a partial-revert hang (§6). The remaining, leading
hypothesis is a **clang miscompile / VC71-vs-clang x87 float divergence** in a
source-faithful function — the deployed binary is clang and its BEHAVIOR has not been
differentially verified against the original. **Recommended next attack: a CLANG-vs-original
behavioral differential (equivalence / dual-oracle), §8 — NOT more source-scanning or toggling.**

**Map/difficulty:** a10 (Pillar of Autumn), Impossible.
**Last updated:** 2026-06-26. Build under test: decomp `d8ad32eb`.
**Supersedes:** `docs/handover-a10-grunt-aim-passive.md` ("NO DEFECT FOUND") and the
"culprit = ai.obj / FUN_0003f5f0" memo — both refuted (see §7).

**Refuted fix attempts (do NOT retry):**
- **2026-06-26 — `actor_path_refresh` (0x2cdb0): `static char large_buf[0x1408c]` → plain
  stack-local.** Hypothesis (Codex): the 82,060-byte path-scratch buffer being `static`
  shared stale path state across actors/frames, spuriously yielding path_found=0 so the
  firing-position selector rejected positions. **Deployed** (clang obj rebuilt 19:11 after
  the edit; XBE regenerated + deploy-verified; box stayed alive with AI ticking, so the 82 KB
  stack frame is safe) **and door-walked → REFUTED.** Runtime signature unchanged from broken
  (grunts 6/10/11 at player-dist 4–5u: `aw_max=3` not 10, `moved≈0u`, mode `0x46c` flickers
  3→1 for a single sample, path-active `0x4a8`=0, at-dest `0x484`=**1 (always)**, gate
  `0x504`=0). The buffer is initialized before read, so its storage class is behaviorally
  irrelevant here. Change stashed (`git stash`), tree back at clean HEAD.
  **Sharpened lead from this run:** `0x484` (at-destination) is pinned at **1** the whole
  walk — the grunt believes it is ALREADY at its destination, so `actor_path_refresh`
  early-returns (`0x4a8`=0, `0x484`=1, returns "path found / at dest") and the brief mode-3
  decision is immediately neutralized. The next attack should center on **why `0x484` is
  set / who writes the destination (`0x488/48c/490`)** — i.e. is the firing-position
  destination resolving to the grunt's own position (trivially "arrived")? — in addition to
  the §8 clang-vs-original differential.

> **For reviewers:** §2 (oracle), §3 (Run H proof), §4 (mechanism trace), §5 (causal
> order — awareness is downstream) are solidly evidenced. §6 lists exactly what is
> verified vs unchecked. §8 is the prioritized next action. Do **not** read "verified
> faithful" as "exonerated" — Run H proves the defect is in actor_moving; "faithful"
> is scoped to the source + VC71 codegen of the specific functions listed.

---

## 1. Symptom (user)
On a10, walk from the checkpoint through the door that draws the weapon (a trigger that
also spawns 3 grunts). In the **unpatched** game the grunts advance toward the player and
shoot. In the **patched** build they "shoot but won't advance," and only some engage,
slower / only at close range ("they attack eventually, but only one of them / not as fast").

## 2. The regression, MEASURED — controlled A/B oracle
Tool: **`/tmp/causal_probe.py <tag> <dur>`** (also `/tmp/path_probe.py`, `/tmp/engage_timeseries.py`).
Connects XBDM:731, samples team-3 grunts every ~0.4s, records per door grunt (matched by
player-prop distance, NOT slot index — slots renumber): action `6a` (2=alert,3=fight),
awareness `aw268`, move mode `0x46c`, gate `0x504`, path-active `0x4a8`, step-count `0x4c1`,
at-dest `0x484`, world pos (advance), player-prop dist/st24/ack.

**The reliable oracle = `moved` (advance, >1u vs ~0u) and `aw_max` (10 vs 3).** The bug is
a GRADED timing/reliability regression — single binary toggle reads on uncontrolled state are
noise; always use a controlled door-walk from the same checkpoint and read advance + awareness.

## 3. Localization — Run H proves the culprit object is actor_moving.obj
`ported=false` in kb.json reverts a function/object to original cachebeta code (faithful, no
behavior change). Helper: `artifacts/ai_regression/bisect_toggle.py <CFG> <obj...>` (restores
clean HEAD from `/tmp/kb_clean_head.json`, then sets named objects ported=false); `--restore`
to clean HEAD. Per config: `build_deploy_run.sh -q` → user door-walk → probe. **Toggle proven
live before each verdict** with `tools/xbox/verify_toggles_live.py --addr <vas>` (ORIGINAL
prologue vs ACTIVE push-ret).

| Run | Config (reverted to original) | Result |
|---|---|---|
| A | full patched (baseline) | BROKEN (aw≤3, no advance) |
| B | alloff (all 13 AI) | FIXED (aw→10, advance) |
| C | {actor_perception, actor_combat, actor_moving, actors, ai} | FIXED |
| D | {actors only} | BROKEN → actors.obj NOT it |
| E | {actor_perception, actor_combat, actor_moving} | FIXED |
| F | {actor_perception, actor_combat} | BROKEN → perception/combat NOT it |
| **H** | **{actor_moving only, whole object}** | **FIXED** |

E(FIXED) vs F(BROKEN) differ only by actor_moving ⇒ necessary. H ⇒ **actor_moving alone is
sufficient.** **Run H is ground truth: the defect is inside `actor_moving.obj`'s 28 ported
functions.** (`ai.obj` / `FUN_0003f5f0` exonerated, §7.)

**Decisive A/B with the causal probe (matched door grunts 6/10/11, reproduced):**

| grunt | build | aw_max | action 6a | mode 0x46c | gate 0x504 | **moved** |
|---|---|---|---|---|---|---|
| 6 | FIXED (Run H) | **10** | 3→3 fight | reaches **3** | sets to 1 | **1.7u advance** |
| 6 | BROKEN | 3 | 2→2 alert | **stuck at 1** | never | 0.0u |
| 10 | FIXED | 6 | 3→3 fight | **3** | sets | **1.8u advance** |
| 10 | BROKEN | 7 | 2→2 alert | mostly 1 (3 once) | never | 0.1u |

> ⚠️ **CAVEAT on partial reverts:** function-level (sub-object) reverts of actor_moving can
> HANG the game (Runs G/K: a path-driver calling reverted/patched path callees forms a
> boundary infinite-loop — QMP `running:true` but sim static; NOT a gdb break). **Whole-object
> reverts are stable.** Partial-revert "verdicts" (the old Runs I/J) are UNTRUSTWORTHY — the
> advisor's standing guidance is: when a partial-revert bisect conflicts with instruction-level
> disasm, the disasm wins.

## 4. Mechanism traced — failing step is FIRING-POSITION SELECTION
The grunt only advances after committing to move mode `0x46c==3` (move-to-firing-position).
The dispatcher (`action_guard.c`, in `actor_looking.c:2348` case 3) commits mode 3 **only when
behavior `actor+0xc0 == 3`**. `0xc0` is set to 3 at `actor_looking.c:2254` **only when
`result != -1`** (else `0xc0=1`, stay), where:

```
result = FUN_000272d0(actor, FUN_00025c10(actor, ...), ...)   // actor_looking.c:2243-2246
```
- `FUN_00025c10` (FP evaluator) — generates/scores candidates; CALLS `actor_path_input_new` and runs A*.
- `FUN_000272d0` (FP selector) — for the chosen candidate, ACCEPTS iff `actor_move_to_firing_position(...)`
  returns **non-zero** (`actor_looking.c:7402`); else sets `actor+0x3b8 = -1` (reject → `result=-1`).
- `actor_move_to_firing_position` setup-branch returns `actor_path_refresh(actor, 1, override)`.

`FUN_00025c10`/`FUN_000272d0`/the scorer `FUN_00024cf0` are in **actor_looking** (never reverted
in any bisect run). They reproduce the defect via their **actor_moving callees**
(`actor_path_input_new`, `actor_move_to_firing_position`→`actor_path_refresh`) — which is exactly
why reverting actor_moving (Run H) fixes it while actor_looking stays patched.

**Leading hypothesis (call-chain-supported, NOT yet probe-confirmed):** on broken,
firing-position selection returns `-1` → behavior `0xc0` never becomes 3 → grunt stays in mode 1.
**Whether this is "no candidates generated" (FUN_00025c10) vs "all candidates rejected"
(FUN_000272d0) has NOT been measured — see §8.**

### Broken steady state is the EARLY-RETURN path, not a pathfinder failure
On broken the probe shows `0x46c=1, 0x4a8=0, 0x484=1, 0x504=0`. This matches
`actor_path_refresh`'s **early-return** (`actor_moving.c:2154-2156`):
`if (0x160 || move_src==0 || move_src==1 || (move_src==3 && 0x3bb)) { 0x4a0=0; 0x4a8=0; 0x484=1; return 1; }`.
Since broken `move_src (0x46c) == 1`, path_refresh early-returns 1 and **never runs the
pathfinder** — consistent with "the grunt never committed to a real move," NOT with "the
pathfinder failed." (Do not claim the external pathfinder `FUN_0005ff70` returns 0; that is
unverified and the steady state argues against it.) The actual candidate rejection, if any,
happens *inside the selection's transient test* (where move_to_firing_position momentarily sets
mode 3) and has not been probed.

## 5. Causal order — awareness is DOWNSTREAM (solid)
Timeline (`/tmp/causal_fixed.json`, grunt 6): both builds reach the identical state
`aw=3, 6a=3(fight), mode=1, pst=5, pdist=5.4` at t≈5-7s, then diverge. On FIXED the mode
**1→3 commit happens at t=10.05 while aw is STILL 3**; awareness climbs to 10 only at t=10.41,
**after** the commit and the start of advance. On BROKEN mode stays 1 and the grunt later
de-escalates to alert. ⇒ **Awareness/disposition is not the gate; it is downstream of the
movement commit.** Perception is fully working on BOTH builds (player prop `st24=5`, `ack=3`).
The earlier "perception promotion" and "awareness root" theories are refuted (§7).

## 6. Verification status of actor_moving's 28 functions
**Scope of "faithful": matches the original at the source + VC71-codegen level for the named
function. The deployed binary is CLANG and has NOT been bytewise/behaviorally verified.**

**Verified faithful (≈10):**
- `actor_move_update` (0x2e560) — entire function hand-disassembled vs pristine XBE; vehicle
  block (0x2e969) + on-foot block (0x2eb77-0x2ed53) instruction-for-instruction match source.
  (Note: `delinked/actor_moving.obj` .text TRUNCATES at 0x45c1=VA 0x2e961 — the on-foot block
  exists only in the XBE; `halo-patched/cachebeta.xbe` holds the pristine original bytes.)
- `actor_move_to_firing_position` (0x2d900) — instruction-exact incl `0x4c!=0 && 0x4a4==0` fast-path.
- `actor_path_3d_available` (0x2b720) — for on-foot (0x15e!=4) returns TRUE; matches disasm.
- `actor_destination_update` (0x2d350) — path-step LOOP + all 4 FPU break senses match disasm
  (`dot_seg_facing<=0` test0x41/jne @0x2d44f; `dot_seg_to_cur>=0` test5/jp @0x2d45c; perp/dist >= thr).
  **(The 0x504-set guard and post-loop region were read but the move-source-specific destination
  setup feeding it was not exhaustively re-derived — treat as mostly-verified.)**
- `actor_test_destination` (0x2a580) — FPU compare operand-swapped + inverted branch = identical
  result (set 0x484 when tol²>dist_sq); the `jp/jnp`,`test5/test41` multiset flags are benign clang codegen.
- `actor_move_force_stop` (0x2a860), `actor_get_stopping_distances` (0x2a610) — offset-faithful; force_stop's 0x504/0x6e0 writes confirmed via disasm.
- `actor_path_input_new` (0x2a470) — `path_input_new` param2 is `unsigned int` (decl.h:4872);
  original pushes `local_8` raw (`push ecx`@0x2a507) — **NO int→float conversion** (not the cost-flood bug).
- `actor_path_refresh` (0x2cdb0) — ENTRY (2094-2165) + path-BUILD mechanics (2395-2474) verified;
  callee types all match (`FUN_0005e0d0` params 3/4 `int`; `path_input_set_attractor` float
  `param5=10.0f` — the `path.c:1005` int→float attractor bug is ALREADY FIXED here).
- **Offset sweep, all 28** (`/tmp/offset_sweep2.py`, lea-pointer-resolved write-disp sets, VC71
  obj vs XBE): **NO wrong-offset write anywhere** (no `orig-only` displacement). `check_lift_hazards.py`
  + `buffer_alias_detector.py`: CLEAN on actor_moving.c.

Since the prior revision, the following were ALSO verified faithful vs original disasm:
- `actor_path_refresh` move_src==3 (squad-order/firing-position) AND move_src==4 destination
  resolution (2165-2237): tag_block_get_element strides 0xb0/0x18/0xe8/0x50, block offsets
  0x42c/0x98/0x80/0xc4, index fields (0x34/0x470/0x3a), copy offsets (0/4/8/0x14, 0x4c) ALL match.
- `actor_path_input_new` — verified in the deployed **CLANG** obj too (not just VC71): same args
  (param_4=unit_handle, param_2=local_8 raw bits, param_1=nav), `fld`/`fstp` float roundtrip = identity, no conversion.
- `actor_move_update` **epilogue** (seed_fallback/clear_active/store_prev, 3861-3973): writes
  facing (0x5a4/8/c), facing flags (0x58d/e/f, 0x50a), crouch (0x508/0x58f), jump (0x530-0x540),
  store_prev (0x6ec/0/4 ← 0x418/c/0x420). It writes **NONE of the path-control fields**
  (0x46c/0x470/0x418/0x4a8/0x484/0x34) that gate selection — so a bug here can't directly starve it.
  ⇒ **move_update is now fully source-verified (on-foot + vehicle + epilogue).**

**NOT verified — still in scope for the defect:**
- **The deployed CLANG binary's BEHAVIOR.** All source/VC71/offset checks pass, yet Run H is ground
  truth — so the defect is most likely a **clang miscompile or VC71-vs-clang x87 float divergence**
  that tips a borderline path-validity / A* cost / distance comparison in a function that is faithful
  at the source level. This is invisible to everything done so far. **This is the leading hypothesis.**
- `FUN_0002a3a0` (0x2a3a0) — called by path_refresh at LAB_fail (2464) and hysteresis (2458);
  listed ported, **never verified.** (Low priority: only on the path-fail branch.)
- ~16 leaf functions behind the 0x504 gate / `compute_facing` (`FUN_0002b830`, try_evasion,
  get_avoidance, aim math) — `gate_probe` proves 0x504 is NEVER set on broken ⇒ these **never
  execute** on the stuck-grunt path ⇒ excluded as the cause (confirmed by call-graph).

### Bisect is BLOCKED by the partial-revert hang (important for reviewers)
Reverting `move_update` (0x2e560) ALONE hung the game (QMP `running:true`, XBDM alive, sim static —
confirmed game-hang, not gdb). Cross-referenced with the older runs: reverting the HL path-drivers
while leaves stay patched ALWAYS hangs (Run G: 12 HL reverted = hang; this: 1 HL reverted = hang);
only the WHOLE-object revert (Run H, all 28) is stable; leaf-only reverts are stable but the leaves
don't execute on broken. **⇒ No actor_moving function on the path can be isolated by toggle without
hanging.** Function-level bisect within the path cluster is not available; the path forward must be
a CLANG-vs-original behavioral diff, not a toggle bisect.

## 7. Ruled out (with evidence — do not relitigate)
- **Wrong-offset write in actor_moving:** offset sweep clean (no orig-only displacement); the few
  ours-only flags (force_stop 0x504/0x6e0, path_refresh 0x488-490, dest_update 0x4a8/0x50c-514)
  all confirmed faithful — original writes them via `datum_get`-return/`add`/`lea` pointer bases the tracker can't follow.
- **`FUN_0003f5f0` (ai.obj):** disassembled vs delinked original — behaviorally identical
  (counter rotation, iterator, gates, handle iter+0x14, activate target reads handle from stack).
  Prior "toggle flips it" was a binary-oracle false positive on a graded symptom. Run E/H (ai patched) FIXED.
- **"Spurious junk perception props" (e2a/e2b):** causality fork over 40 captures DISSOCIATED —
  unpatched reference has those props too. Normal perception props.
- **Awareness/perception as ROOT:** perception completes on broken (pst=5/ack=3); awareness is
  downstream of the mode-3 commit (§5 timeline). The prop-field st24/ack discriminator was a
  capture-timing artifact.
- **`actor_move_update` 0x504 on-foot gate / `compute_facing`:** gate_probe shows `0x504==0` always
  on broken ⇒ compute_facing never called; move_update on-foot block proven faithful. These are
  downstream of the never-reached mode-3 commit.

## 8. NEXT STEPS (for reviewers — bisect & source-scan are EXHAUSTED)
State of play: the entire executing path (selection chain's actor_moving callees + move_update,
full) is source/VC71-verified faithful; the offset sweep is clean; function-bisect is blocked by
the hang (§6); yet Run H proves actor_moving is the culprit. The defect is therefore a
**behavioral divergence of the deployed CLANG code from the original** that source-level checks
cannot see. Recommended attack, in order:

1. **CLANG-vs-original behavioral differential on the executing path functions.** The selection
   REJECTS firing positions because `path_refresh` (or the A* candidate eval in `FUN_00025c10`)
   behaves differently under clang. Run the equivalence harness (`tools/equivalence/unicorn_diff.py`)
   on the deployed **clang** obj vs the **delinked original** for: `actor_path_refresh` (0x2cdb0),
   `actor_path_input_new` (0x2a470), `actor_test_destination` (0x2a580), and any float-comparison
   path-validity helper. Non-leaf → `--allow-stubs --float-tolerance 32 --mem-trace`; for state-dependent
   paths use a `--state-snapshot` captured from xemu in the a10 door-engagement. Look for a
   divergent FLOAT result / branch (x87 rounding tipping a borderline distance/cost/cone compare).
2. **Dual-oracle on a live entity** (`/verify dual-oracle`): call the original `path_refresh` at
   0x2cdb0 (ported=false copy) vs our clang version on the SAME engaged a10 grunt and diff the
   return value + written path-control fields (0x4a8/0x484/0x46c/0x418). Decisive about whether
   our clang path_refresh returns a different path_found.
3. **Manual clang disasm of the float comparisons** in path_refresh's destination-validity
   (LAB_check_dest, 2279-2434, esp. the `actor[0x498]==0`, `actor[0x494]!=-1`, hysteresis 2455-2457
   FCOM chains) and `actor_path_3d_available`'s cone checks — compare the clang FCOM/FUCOMI + branch
   sense against the original `fcomp/fnstsw/test ah,mask` for an inverted/tipped comparison.
4. Verify any fix against the ACTUAL door-walk repro (advance + aw via causal_probe), not a byte metric.

**Do NOT** re-hand-scan the 28 functions (12+ verified faithful), re-run the candidate probe
(resolved: selection succeeds sometimes — grunts 10/11 hit mode 3 briefly then revert; the failure
is SUSTAINING the move, i.e. path_refresh returning 0), or attempt a function-level toggle of any
path-cluster function (hangs).

## 9. Tooling / repro recipe
- Oracle: `/tmp/causal_probe.py <tag> <dur>` (40s; matched door grunts by prop-dist). Path-state:
  `/tmp/path_probe.py <tag> <dur>` (0x4a8/0x4c1/0x4c2/0x484/0x504/0x46c). Raw JSON saved per run.
- Whole-object toggle: `artifacts/ai_regression/bisect_toggle.py <CFG> <obj.obj...>`; `--restore`.
  Configs logged in `artifacts/ai_regression/bisect_configs.json`.
- Function toggle (actor_moving, HANG-prone — whole groups only): `/tmp/fn_toggle_am.py <addr...>` / `restore`.
- Static analysis (no door-walk): `/tmp/offset_sweep2.py` (write-disp sweep), `/tmp/logic_diff2.py`
  (constant/branch multiset diff), `artifacts/ai_regression/disasm_range.py <start> <end>` (capstone
  on pristine XBE — `halo-patched/cachebeta.xbe` holds original bytes; delinked .text truncates at 0x45c1).
- Build+deploy: `./tools/xbox/build_deploy_run.sh -q` (resets VM, self-verifies running build).
  ALWAYS confirm toggle live afterward: `tools/xbox/verify_toggles_live.py --addr <vas>`.
- XBDM port 731. NEVER connect to :1234 (halts CPU). QMP on :4444 (`query-status` to distinguish
  game-hang vs gdb-halt). xemu runs as a Windows .exe; `ss` cannot see its sockets.
- Clean kb baseline: `/tmp/kb_clean_head.json`. **Current kb state: clean HEAD (full patched / BROKEN).**

## 10. Memory
- `.claude/agent-memory/xbox-halo-re-analyst/project_a10_passive_mechanism_enemy_prop_promote.md`
  (durable cross-session record; keep in sync with this doc).

</details>
