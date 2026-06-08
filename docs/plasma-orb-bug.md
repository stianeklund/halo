# Bug: charged plasma-pistol gun-tip orb is invisible (patched build)

**Status:** OPEN. Class: visual regression (cull / no-draw / dropped self-illum), introduced by
one of our ported functions. Not present in the original Xbox XBE.

## Symptom

In the patched build, when the plasma pistol is **fully charged/overcharged**, the small green
**orb at the gun tip** (a first-person-weapon self-illum effect) does **not** render. The
*environmental* green glow (`ligh` light objects cast by the charge) works fine — so the lighting
subsystem is healthy; only the orb geometry/effect at the muzzle is missing.

**Repro:** load any level with a plasma pistol → fully charge it (hold trigger until overcharged) →
look at the gun tip. Original XBE: green orb visible. Patched build: orb absent.

## What is ruled OUT (with evidence)

1. **Not a pooled effect.** Live xemu memory scan while the orb was visible (unpatched, paused)
   showed every candidate pool EMPTY: particle, particle_system, effect, effect_location, weather,
   contrail, contrail_point — all `last_index=0`. The widget pool had only a type-3 lightning
   widget, no glow widget. (Scan validated by positive control: lights pool had 37 active lights.)
   → The orb is **not** a contrail/particle/effect/glow-widget/lens-flare. Skip those TUs.

2. **Object drive-state is byte-identical between builds.** Captured the charged plasma-pistol
   object on both builds (paused, fully charged): `type=2`, `func_active_bits=0x0f`,
   `func_values=[1.0,0.0,1.0,1.0]`, and header bytes `obj+0x60..0x140` BYTE-IDENTICAL.
   → The overcharge "drive state" the orb reads is the same on both builds. Not an input-data bug.

3. **Node transforms are identical.** FP-weapon node matrix block (entry+0x108c, 40 nodes × 0x34)
   compared across builds: all node scales = 1.0 on both; every node's distance-from-node0 is
   identical (0/40 mismatch). The raw per-node diff is an aim/yaw capture confound, not a bug.
   → The model placement / skeleton transform path is fine. Bug is downstream, in draw/shader.

4. **NOT in the 7 obvious draw TUs.** EXPERIMENT 1 (2026-06-04, see below): toggled `ported=false`
   on all 51 functions in `first_person_weapons.obj` + `shaders.obj` + `render.obj` +
   `rasterizer.obj` + `rasterizer_common.obj` + `rasterizer_xbox.obj` + `model_animations.obj`,
   routing them all to ORIGINAL. Orb **still invisible**. This result is trustworthy (deploy gate
   printed `verify: OK`; toggle confirmed applied in kb.json; build booted stably past `main_loop`).
   → The dropped-orb code is **not** in those draw TUs.

## Method that works (live memory diff)

- xemu launched with QMP (`tools/xbox/xemu.sh -q`). Read VIRTUAL memory (exposes XBE image globals)
  via `tools/xbox/xemu_qmp.py hmp "x/<N>wx 0x<addr>"`; physical via `xp`/`pmemsave` for heap
  (`0x80xxxxxx` → subtract `0x80000000`).
- Key globals: FP weapon array PTR `0x46bea8` (stride `0x1ea0`; `+0`=active, `+8`=obj handle,
  `+0x108c`=node matrices 0x34 each). Object pool `DAT_005a8d50`; `obj+0x64`=type (2=weapon),
  `obj+0xd3`=func active-bits, `obj+0xe4`=float[4] func values. s_data_array hdr: `+0x2e`=last_index,
  `+0x34`=data ptr; element[0]=int16 salt (nonzero=active).
- The orb's overcharge func values are computed by ORIGINAL `weapon_export_function_values`
  (`0xfbf00`) → `object_compute_function_values` (`0x13e7b0`).

## Remaining hypotheses (next experiments)

Memory diffing is exhausted (divergence is transient GPU/shader state). The oracle for this bug
class is the runtime **ported-flag toggle-bisect** (NOT VC71/instruction-diff — those don't
converge on visual regressions; see memory `feedback_toggle_bisect_visual_regression`).

- **(A) `objects.obj` (63 core funcs)** corrupts state the original render reads. Toggling all 63
  at once destabilized the build last session (reboots) — must bisect `objects.obj` in **small
  sub-batches**, not whole. Lower-risk than it sounds if sliced.
- **(B) Upstream non-render ported fn** corrupts the shader/lighting/geometry *input*. Prime
  suspects: the func-value computation chain `weapon_export_function_values 0xfbf00` /
  `object_compute_function_values 0x13e7b0`, and `object_lights` vec3 work (commit `70b9de31`
  "MP green-tint … object_lights vec3" — green self-illum adjacency). These produce the values the
  self-illum constant reads even though `obj+0xe4` ended up byte-identical — a transient/per-frame
  recompute could still diverge.
- **(C) Render-source UNCONFIRMED** (standing caveat): the orb may not render from this weapon
  object at all. Before more toggling, confirm the actual draw path — an xemu nv2a/GPU trace or a
  hardware watchpoint on the self-illum constant would pin the real producer.

## Hard-won process rules (do not relearn the expensive way)

- **Never trust a toggle result whose deploy did not print `verify: OK`.** An entire prior session
  was wasted on a stale xemu-cached XBE. The gate (`tools/xbox/deploy_xbox.py`, see
  `docs/`/memory `feedback_deploy_self_verify`) now proves running==built every cycle.
- **`ported=false` is NOT always safe in large batches.** Toggling 114 funcs (incl. all of
  `objects.obj`) rebooted the build. Keep batches small; exclude core lifecycle TUs.
- **Toggle tooling:** `artifacts/toggle_ported.py off-tu <TU.obj> ...` / `status` / restore via
  `git checkout kb.json`. Never commit during a bisect.

## EXPERIMENT log

- **EXP 1 (2026-06-04):** OFF = first_person_weapons + shaders + render + rasterizer{,_common,_xbox}
  + model_animations (51 funcs). Deploy `verify: OK` (rev 95434598). Booted stably. **Orb still
  invisible** → bug not in these 7 draw TUs. `kb.json` restored.
  - *xemu state at handover:* still running the EXP-1 **toggled** XBE (kb.json reverted in tree, but
    the deployed binary is toggled). **Next session: redeploy committed build first**
    (`./tools/xbox/build_deploy_run.sh -q`, expect `verify: OK`) to re-establish the clean baseline.

- **EXP 2 (2026-06-04):** Branch (B) reduced to a single toggle. Discovered 2 of the 3 named
  Branch-B producers are **already original no-ops**: `weapon_export_function_values 0xfbf00`
  (`ported=null`) and `object_compute_function_values 0x13e7b0` (`ported=false`). The only ported
  function from the `object_lights`/green-tint commit `70b9de31` in the lighting path is
  **`FUN_0013ab20` (0x13ab20)** in `objects.obj` — the lighting-sample helper that fills the
  0x74-byte lighting struct (`object_lights.c`, the per-object environmental light env). Toggled it
  OFF alone (1 of 65 active objects.obj funcs; low reboot risk). Deploy `verify: OK` (rev 9e48a226),
  built-XBE sha changed (`85482f65`→`ac189afc`, proving patch re-ran), and **running-image memory at
  VA 0x13ab20 = original prologue `55 8B EC 81 EC 90...`, no `E9` redirect** → toggle confirmed live
  in the executing binary (closes the rev-token gap: `rev` is a pre-patch source token and does NOT
  change with kb.json toggles; trust sha-change + redirect-byte read, not rev). User confirmed
  in-game (charged plasma pistol, gun tip): **orb STILL INVISIBLE**. → `0x13ab20` is NOT the culprit.
  `kb.json` restored. *Note:* `object_lights` computes the **environmental** light sampled to shade
  the model; the orb is a **self-illum/emissive** effect that typically bypasses object_lights — a
  negative was the ~2:1 expected outcome.
  - **PIVOT (pre-registered):** stop eliminating ported suspects one-by-one. The render-source is
    still UNCONFIRMED (hypothesis C). Confirm the actual draw path before any more toggling: which
    code/GPU draw call emits the orb pixels, and which input it reads. Candidate refinement: the
    self-illum shader may read a **per-frame-recomputed** function value (a render-time evaluation),
    not the stored `obj+0xe4` we proved byte-identical — find that evaluator and its consumer.

- **EXP 3 (2026-06-04) — EXP-1 re-verified at the STRONG standard; paradox is REAL:** EXP-1's
  original `verify: OK` only proved same-*source*-build (the `rev` token is pre-patch and does not
  change with kb.json toggles), so it never proved the 51 toggles were live in the running image.
  Re-ran the identical 51-set toggle, deployed (`verify: OK`, rev 9e48a226), then **redirect-byte-
  verified the running image via QMP** (`x/2wx <VA>`, xemu paused). Discriminator (learned this
  session): a still-ported function shows a **push-ret trampoline** `68 <impl≥0x642000> C3` (or an
  `E9` to the impl region); live-original shows a genuine prologue. Sampled 24 functions: **16/16
  first_person_weapons** + 8 across shaders/render/rasterizer{,_common,_xbox}/model_animations — ALL
  live original. Control: `0x13ab20` (ported, NOT in set) correctly showed `68 50 43 6e 00 C3` →
  trampoline to impl `0x6e4350`, proving the discriminator. User re-tested: **orb STILL INVISIBLE.**
  → The EXP-1 conclusion is vindicated at the standard EXP-1 lacked: with the 7 obvious draw TUs
  genuinely running ORIGINAL code (+ identical object data + identical node transforms), the orb is
  still gone. **This is NOT a stale-toggle confound. The paradox is real.** The orb is not drawn by
  ported code in those 7 TUs.
  - *QMP read gotcha (cost me a vacuous first pass):* memory reads via `xemu_qmp.py hmp "x/..."`
    return EMPTY while the VM is RUNNING; they only work when **paused**. Always pause before
    redirect-byte verification.
  - **Candidate render TUs NOT in the EXP-1 7** (ported-fn counts): objects.obj(65), decals.obj(53),
    rasterizer_text.obj(35), effects.obj(34), weapons.obj(22), rasterizer_decals.obj(11),
    player_effects.obj(7), structure_detail_objects.obj(5), render_cameras.obj(3),
    rasterizer_xbox_shadows.obj(1), rasterizer_sprites.obj(1). `player_effects.obj` and `weapons.obj`
    (overcharge logic) are the most orb-plausible un-toggled render-adjacent TUs; effects pools were
    already proven empty so effects.obj is lower-probability.
  - *Caveat on resolution-1 (function-value evaluator):* may collapse — the ONLY function-value
    computation path, `object_compute_function_values 0x13e7b0`, is ALREADY original (`ported=false`),
    so a render-time re-eval would call the original and produce the identical value. Reconcile the
    next technique with the advisor before spending cycles.

- **EXP 4 (2026-06-04) — weapons.obj + player_effects.obj ruled out (incl. `weapon_overcharged`):**
  Toggled OFF all 29 funcs of `weapons.obj`(22) + `player_effects.obj`(7), routing to original.
  Tests hypothesis (B): a ported non-render fn corrupting the overcharge "draw-glow" gate the
  original render reads. The batch contains `weapon_overcharged 0xfb2f0` — the exact gate predicted.
  Deploy `verify: OK` (rev 9e48a226). Redirect-byte-verified 8/29 live original (incl. 0xfb2f0:
  `55 8b ec 8b 45 08...` genuine prologue) with the ported control 0x13ab20 correctly showing the
  trampoline. User tested: **orb STILL INVISIBLE.** → The overcharge gate / first-person-effect
  hypothesis in these TUs is dead. `weapon_overcharged` returning a correct boolean is NOT the
  missing piece.
  - **Running tally of FUNCTIONS routed to original across all experiments, orb still invisible:**
    51 (7 render TUs) + 1 (object_lights 0x13ab20) + 29 (weapons+player_effects) = **81 ported funcs
    eliminated**, plus object drive-state & node transforms proven identical. Every toggle negative.
  - **Pre-registered next (hypothesis A):** `objects.obj` is the last untested render-adjacent
    heavyweight and contains **`object_render`** — the actual per-object geometry submission the FP
    weapon draw routes through (EXP-1 toggled the FP-weapon *caller* 0xddae0 to original, but its
    `object_render` callee in objects.obj stayed ported — so objects.obj render is a genuinely
    untested suspect, NOT a blind grind). Toggling all 65 objects.obj funcs rebooted before, so
    isolate the render subset (object_render + model submission) rather than the whole TU.
  - **Standing alternative if objects.obj is also negative:** the premise itself ("orb invisibility
    is a single ported-function regression on the FP-weapon self-illum path") may be wrong — consider
    (i) a non-function/data/build difference, or (ii) the orb rendering from a path/TU not yet
    enumerated (transparent-geometry / self-illum pass). Escalate to GPU/draw-path confirmation.

---

## EXP-5 — Pivot to toggle-set bisect at HEAD; large-toggle instability (2026-06-04)

**Breakthrough (prior):** orb is VISIBLE on patched builds at commit `8cf76234`
(2026-05-17) and `f99152d7` (2026-04-30); INVISIBLE at HEAD `9e48a226`. So it is a
real, localizable regression in the 1590-commit window — not World B / foundational.

**Commit-bisect abandoned:** mid-range commits are NOT clean-buildable. Auto-lift/
worktree merges (10 merges in good..bad) leave kb<->source desyncs that a fresh
recompile surfaces (e.g. `f8a4f775`: source `char FUN_00076bd0(int)` vs kb
`void FUN_00076bd0(void)` ported:null — introduced by merge `7cc35a2d` reverting kb
over the `1abc3b7a` port). `f8a4f775` is on the first-parent chain, so `--first-parent`
does not dodge it. Bisect log saved: `/tmp/orb_bisect.log`.

**Pivot:** build-stable toggle-set bisect at HEAD. 861 funcs became ported:true between
`8cf76234` and HEAD (list: `/tmp/orb_newports.txt`). Method: toggle subsets ported:false
(-> original behavior) at HEAD, binary-search which one restores the orb.

**Results so far (INCONCLUSIVE — instability):**
- ALL 861 off  -> build OK (size 4354048), boots (clean banner, no crash), VM running,
  but PURE BLACK SCREEN (never reaches render; xdbm screenshot = "unexpected framebuffer
  format"). Toggle deploy verified correct (genuine prologues at 0x17060/0x1c030/0x13ab20,
  not 68..C3 trampolines). Liveness OK (HDD size == local).
- UPPER half off (431, 0xaff70..0x1d7d21; list `/tmp/orb_half_upper.txt`) -> build OK
  (size 4358144), deploy LIVENESS OK, boots clean banner — but ALSO PURE BLACK SCREEN.
  User confirmed: pure black, no boot-loop (user rebooted manually).
- LOWER half: `/tmp/orb_half_lower.txt` (430, 0x13dd0..0xaff20) — NOT yet tested.

**Open question (UNRESOLVED):** is the black screen caused by the toggle CONFIG
(a destabilizer function among the toggled set that hangs pre-render when run as original
while its HEAD-ported callers expect the ported version) or by the ENVIRONMENT (xemu
display wedged after many rapid redeploys/QMP resets)? Evidence leans CONFIG (XBDM/QMP
stay responsive; framebuffer never initializes = game-side early hang) but is NOT proven.
Next planned step was advisor consult + a clean-HEAD control (does untouched HEAD still
reach gameplay in this xemu session?).

**Known-stable when off (from EXP-1/2/4, all in the 861):** 7 draw TUs (51 funcs),
object_lights 0x13ab20, weapons+player_effects incl weapon_overcharged 0xfb2f0 (~81 total)
— these booted to gameplay (orb still invisible). So small/render toggles are stable;
the destabilizer is elsewhere and only bites in large sets.

**Tooling:** toggle `artifacts/toggle_ported.py off-addr $(cat <list>)`; restore
`git checkout kb.json`. Build `/tmp/orb_build_alloff.sh` (rm stray
.vc71_regcall_objects.c; rm build/generated/{decl.h,thunks.c,halo.xbe.def}; cmake; build.py).
Deploy `/tmp/orb_deploy.sh` (lean: upload + magicboot + HDD-size liveness + debug.txt pull;
NO QMP pause). HOST 127.0.0.1, XBE E:\GAMES\halo-patched\default.xbe.

**Repo state at handover:** on `main` @ `9e48a226`; bisect reset (none active).
kb.json MODIFIED (uncommitted): 431 upper-half funcs ported:false — RESTORE with
`git checkout kb.json` before any other work. build.main.bak = clean 06-02 main build;
build.bisect.tmp = polluted bisect dir. Anchor stash `stash@{0}` = orb-bisect-anchor
(artifacts only, benign). Deployed XBE on the box = upper-half-off build.

---

## EXP-6 (2026-06-04) — LIVE good-vs-bad capture: weapon STATE is byte-identical → confirmed render-code bug

**Method (new):** real Xbox/xemu at 127.0.0.1 (default.xbe = patched, cachebeta.xbe = stock).
`tools/xbox/capture_orb_state.py <label>` resolves the FP-weapon slot (0x46bea8, stride 0x1ea0;
+0 active, +4 unit, +8 weapon handle) and the held weapon object (pool 0x5a8d50, elem 0xc, data
ptr @+0x34, obj ptr @ elem+8), dumping obj 0x0..0x250 + FP slot 0x0..0x200 + node block. QMP reads
work only when xemu PAUSED. Build-identity verified by prologue bytes (0xdc750 = `68 80 7a 00 00`
original `game_state_malloc("first person weapons",0,0x7a80)`; 0x13ab20 original prologue → stock).

**Captured at overcharge (split-screen: slot0=overcharged, slot1=idle):**
- func values (obj+0xe4) = [1.0,0.0,1.0,1.0], active-bits (0xd3)=0x0f, region perms (0x130),
  FP anim state (fp+0xc)=4, widget list (obj+0x11c)=-1 (NO widgets), no children (obj+0xc8=-1)
  — ALL byte-identical patched vs stock.
- func[2]/func[3]=1.0 are the overcharge functions (idle slot has them 0) → overcharge value IS
  present and correct on patched.
- Every diff was timing/pose noise (aim, position, charge phase, animation frame).
- RED HERRING: fp+0x1c=13(patched)/0(stock) looked overcharge-specific but decompiling its writer
  (0xde560, UNPORTED) showed it is the **movement-overlay frame counter** (fp+0x1a=movement overlay
  index = -1 in all 4 captures → stale leftover; patched-overcharged player had moved, others
  hadn't). NOT the bug.

**Conclusion (high confidence):** the orb's stored state inputs are identical between builds. This
is NOT a corrupted-state bug — it is a **render-code regression**: ported render code reads correct
state but fails to draw the orb. The orb's render SOURCE remains unconfirmed (hypothesis C).
ELIMINATED as source: pooled effects (empty), env light (works), weapon widgets (none attached),
weapon child objects (none), FP-weapon model OPAQUE submission (FUN_000dd580 → 0x121d60/0x123aa0/
0x10cc00 — all ported BEFORE good 8cf76234, unchanged, so correct). model_animations new port
FUN_00120590 = faithful frame prefetch, not it.

**Next:** live-verified toggle-bisect of the one untested render leg — objects.obj new-since-good
render functions (the whole-TU toggle rebooted prior sessions; use a curated render-only subset,
exclude lifecycle/lighting-cluster/player-win). Stale-toggle problem SOLVED: verify redirect bytes
live (push-ret `68 <impl> C3` / `E9` = ported-active; genuine prologue = original) after each deploy.

---

## EXP-7 (2026-06-04) — Orb identified as the overcharge WIDGET; surgical toggle test

**Identification:** Decoded the widget-type dispatch table @0x323528 (5 types, 0x28 stride,
+0x20=update_proc, +0x24=render_proc): type0="flag" type1="ant!" type2="glw!"(glow) type3="mgs2"
type4="elec". Live widget pool (WIDGET_DATA_PTR 0x5a90c4 -> array @0x8007bad4, elem 0xc, data
@0x8007bb0c) on STOCK during overcharge held **3 active widgets, all type-3 "mgs2"** — the
overcharge widgets (the bug doc's "type-3 lightning" = mgs2). Weapon obj+0x11c=-1 and unit
obj+0x11c=-1 (widgets attached elsewhere / dynamically).

**Subsystem is almost entirely ORIGINAL on patched:** all type-2 glw! and type-3 mgs2 handlers
(0x132f40..0x135210) are ported=null/false EXCEPT the type-3 new_fn/delete:
- FUN_00134be0 (mgs2 new_fn, table+0x18) — ported=true, **NEW since good**. Allocates datum in
  0x46f020, stores param at +4, returns handle. Looks faithful.
- FUN_00134c20 (mgs2 delete, table+0x1c) — ported=true, **NEW since good**. datum_delete. Faithful.
Widget creation FUN_00136150 + delete FUN_001362d0 are ported but **NOT new since good** (ran
lifted at the good commit where orb worked) -> correct, excluded.

**ABI/toggle-blind checks (advisor (A)):** decl drift good->HEAD on 0x121c30 (animation_update_internal
void->4args), 0xb6dd0, 0x141b70 (object_compute_node_matrices ported true->false) — all coherent
fixes/deports, lifted callers (event_manager/projectiles) not on FP-orb path. @<reg> drift: 108
funcs gained reg annotations (analysis progress); glow_widget helpers 0x1330f0/1331d0/1339a0/133300
gained @<eax>/@<ecx>/@<edi> but are ported=null (original) with NO lifted callers in src -> inert.
raw_cast_baseline 411->417 (+6) spread across many TUs, none clearly FP-orb render path.

**TEST RUNNING:** toggled FUN_00134be0 + FUN_00134c20 -> ported=false (original), built + deployed
default.xbe (127.0.0.1). Awaiting user visual check: does the overcharge orb return?
- Orb returns -> the mgs2 new/delete lift is the regression (subtle ABI/codegen despite faithful
  source) -> investigate/permute/equiv those two.
- Orb stays gone (toggle live-verified) -> the only new mgs2 funcs are eliminated; escalate to
  (B) globals diff during HELD overcharge (state 2, not 3) patched vs stock, per advisor.

## EXP-8 (2026-06-04) — RESULT: mgs2 new/delete CLEANLY ELIMINATED (live-verified)

**Visual oracle:** User loaded a level on the toggled patched build, overcharged, reports **orb
still gone**.

**Toggle live-verified (QMP paused):** redirect bytes read at both addresses on the running image:
- 0x134be0: `55 8B EC A1 20 F0 46 00` = `push ebp; mov ebp,esp; mov eax,[0x46f020]` — genuine
  ORIGINAL prologue (0x46f020 is the exact data array the source loads). Toggle LIVE.
- 0x134c20: `55 8B EC 8B 45 08 83 F8...` = `push ebp; mov ebp,esp; mov eax,[ebp+8]; cmp eax,..` —
  genuine ORIGINAL prologue (the param_1 != -1 check). Toggle LIVE.
=> Both mgs2 new/delete run as ORIGINAL code and orb is still absent. **The mgs2 widget
new/delete lift is NOT the regression.** (If the orb were the mgs2 widget and creation were
failing in those funcs, toggling to original would have restored it. It did not -> orb is either
not the mgs2 widget, or the failure is upstream of new/delete.)

**Patched live state at pause (split-screen, 2 active FP slots):**
- slot 0 obj 0x800c7504 = the overcharger: +0x211 trigger state = **0x03 (RELEASING)**, not held;
  +0xe4 func floats [0.11, 0.89, 0.0, 1.0]; +0x1f8 integrated light all-zero.
- slot 1 obj 0x800c904c = idle: +0x211 = 0x00.
Captured at state 3 (releasing) AGAIN, not state 2 (held) — the advisor's recurring warning. The
orb shows during HELD (state 2); need both patched and stock captured at state 2 to diff cleanly.

## EXP-9 (2026-06-04) — Pivot to git bisect; INCREMENTAL-BUILD CONTAMINATION discovered

**Decision (advisor):** toggle-bisect/state-diff cannot catch a toggle-blind regression (kb decl/@reg
drift, raw casts, build/codegen). Two free checks empty: src/types.h UNCHANGED good->bad; raw_cast_
baseline.txt is just a counter. => Pivot to `git bisect` good=8cf76234 (dist 0) bad=9e48a226 (795).
Stashed tracked WIP (kb toggle + baselines + weapons.c + frontier).

**Build-break minefield hit at 4/4 midpoints — root cause = stale build/ artifacts:**
- f8a4f775/829f3733 (dist 397/398): GENUINE — 0x76bd0 source defines char(int) but commit kb.json
  reverted to void(void)/null (rebase artifact). Window [7cc35a2d(394)..829f3733(398)], fixed at
  09f5f2f7(399). Skipped span 1abc3b7a^..09f5f2f7 (dist 375-399).
- 257eec65: reg-baseline DRIFT (kb arg2@<esi> vs immutable baseline no-esi). --apply can't fix
  (immutable). 
- b79682b1 (dist 602): reg-baseline MISSING 31 (fixed via --apply, behavior-neutral), then patch.py
  guard claimed ported=true for "game_engine_playlist_next" — **but that name is ABSENT from
  b79682b1's kb.json AND src/ entirely** (exists only in later commits). PROOF that patch.py read a
  STALE generated kb from build/, i.e. incremental builds across 795-commit jumps mix artifacts.

**CORRECTION (contamination theory was WRONG — jq error):** the "phantom" game_engine_playlist_next
was NOT stale-artifact contamination. It IS in b79682b1's kb.json (ported=true, source_path=null,
NO src impl). My earlier `git show kb.json | jq 'select(.name=="...")'` returned [] only because the
function's `name` field is null and the symbol lives in the `decl` string — wrong jq field. Confirmed
via `grep -c game_engine_playlist_next kb.json` (1 match, working tree clean = b79682b1). cmake clean
did NOT remove it (because it was never stale). **Builds are NOT contaminated; incremental is fine.**

**REAL OBSTACLE:** genuine committed guard-failures. The strict patched_xbe guard chain (asm_thunk,
raw_casts, hazards, maintain, kb_meta, reg-baseline) + patch.py export guard reject many historical
commits that were committed with lagging/premature metadata (functions marked ported=true before
impl, baseline lag, rebase decl reverts). At b79682b1 exactly 1 func (game_engine_playlist_next)
is ported=true w/ no impl -> patch.py export guard fails. Such funcs run ORIGINAL regardless (no
impl), so neutralizing them is behavior-neutral and cannot hide the regression.

**FIX:** HALO_KB_OVERLAY env (patch.py reads it, line 1059) to set absent-export funcs ported=false
at patch time — no kb.json edit, no revert. reg-baseline MISSING -> extract_reg_args --apply. Drop
the unnecessary clean step (incremental faithful). Genuine compile errors / decl-mismatch -> skip.

## EXP-10 (2026-06-04/05) — Nuclear-path build WORKS, but b79682b1 is NON-BOOTABLE (tar-pit)

Built b79682b1 (dist 602) via nuclear path: `cmake --build build --target halo` (compiled clean) ->
manual `patch.py cachebeta.xbe build/halo default.xbe --kb-overlay {game_engine_playlist_next:false}`
(+ extract_reg_args --apply for 31 missing baseline) -> deploy_xbox.py --xbe-only --skip-build.
default.xbe (4.26MB) deployed; **build identity VERIFIED** (read 0x81720 = `68 C0 1D 66 00 C3` =
push 0x661dc0;ret, the exact thread_is_done redirect). Helper: /tmp/orb_round.sh.

**RESULT: build HANGS on intro video playback — never reaches gameplay.** Not caused by the bypass
(overlay/baseline edits behavior-neutral). b79682b1 = a "Port game_engine.obj batch 51" commit in
the known game_engine mass-lift session (project_game_engine_session: 138/145, variant-offset bug
class) -> genuinely non-bootable mid-dev commit. Could be the known Bink/BinkDoFrame video hang
(project_demo_gpu_hang) or a boot-path lift bug. Either way: **0 orb info -> git bisect skip.**

**Implication:** historical auto-lift commits don't reliably BOOT (not just don't build). Bisect over
arbitrary commits = tar pit. Pivot candidates: (B) no-build enumeration of the actual raw-cast ABI
changes in orb-path source good->bad (advisor's flagged fallback); or bisect only over known-boot
commits (e.g. ones with deploy/verify history). Bisect state left at b79682b1, baseline dirty.

## EXP-11 (2026-06-05) — Pivot to live no-pause capture; STALLED on flaky reads + state-2

ABANDONED bisect (tar pit). git bisect reset -> 9e48a226 (HEAD, bootable bad build). Unlock:
XBDM getmem reads a RUNNING VM (read 0x81720 live), so user can HOLD overcharge while I read (no
pause = no state-3 trap). Wrote /tmp/orb_live_xbdm.py (XBDM) + /tmp/orb_live_qmp.py (QMP).

INCREMENTAL-BUILD CONTAMINATION IS REAL (object-level): first HEAD rebuild (on build/ polluted by
bisect commits) banner-verified OK but HUNG after boot. cmake clean + rebuild needed for a bootable
HEAD. dcg blocks rm -rf AND git clean -f; cmake --build build --target clean is allowed.

Widget pool 0x5a90c4 decoded: s_data_array @struct, +0x20 max=64, +0x22 elem_size=0x0c, +0x28 sig
'd@t@', +0x34 data=0x8007bb0c; +0x2e and +0x3a both =0x0003 (maybe active count=3?). Salt-scan
(salt@elem+0) found only 1 active (widget[2]=79e9030002006ee9ffffffff) — CONFLICTS with count=3.

Captures keep landing at weapon trigger=3 (slot0 obj 0x800c7504, charge=[1,0,1,1]), NOT state 2 —
even when I planned to QMP-pause remotely, the user self-paused -> state 3 again. And reads are
FLAKY: XBDM returns '????' (unmapped) / timeouts, QMP flips to "Cannot access memory" on a block it
read seconds earlier. Cannot get a reliable patched-vs-stock widget diff at a matched state.

OPEN DECISION: (a) fix live capture (remote-pause while user holds for state 2; clean bootable HEAD;
resolve salt-scan vs count-field), or (b) pivot to no-build source/raw-cast diff of orb-path TUs
good->bad (counter went 443->417 NET REMOVED, so scan files, not the counter).

## EXP-12 (2026-06-05) — READ PASS (no build/user/xemu): toggle-blind diff good(8cf76234)->bad(9e48a226)

Pivoted to READING (advisor): both dynamic approaches (bisect, live capture) died on uncooperative
system. NOTE: `rtk jq` MANGLES kb.json output (returned 61 ported; bare jq = 2656). USE BARE jq.

1. glow_widget helpers (0x1330f0/1331d0/1339a0/133300): gained @reg good->bad but ported=null and
   grep src finds ZERO ported callers (thunks.c emits thunks but nothing references them; called only
   by ORIGINAL widget dispatch). **VERIFIED INERT** (EXP-7 dismissal now confirmed, not assumed).

2. Decl/@reg DRIFT among ported=true-at-BOTH-ends (the toggle-blind ABI class): only **9 funcs**
   (good 1866 -> bad 2656 ported; 790 newly ported are toggle-VISIBLE). The 9:
   - 0xfb6e0 weapon_start_effect [weapons]: int param_2/3 -> float scale/param_3. BUT both call sites
     (weapons.c:705,954) pass 0,0 -> 0 bits == 0.0f bits -> BENIGN. Eliminated.
   - 0x17ca50/cbb0/cbc0/cbd0/ccc0 [decals.c]: void(void) -> typed; these are TAIL-CALL THUNKS
     (commit cc41c8bc "Fix decals.c tail-call thunk signatures after callee ports"). Forward to
     "rasterizer dynamic vertex geometry decal" (FUN_0015abe0/0016bed0/0016c5a0/0016c090/00172590).
     0x17cbc0/cbd0 carry widget_handle+shader+zbuf+position. Pattern split: ca50/ccc0 FORWARD args;
     cbb0/cbc0/cbd0 take args, (void)-cast all, call callee() with NO args (relies on tail-call JMP
     preserving stack). Decal-flavored (separate known decal regression exists) but dyn-vertex-geom
     may be shared with overcharge glow. AMBIGUOUS — needs: do they compile to JMP(forward) or
     CALL(broken)? are callees decl'd with args? are cbc0/cbd0 on the overcharge path?
   - 0x10aa60 periodic_table_fill [random_math] int->short; 0x1c8d90 [sound] int->short tick;
     0x78b80 [bitmap] retyped filter args. OFF-PATH.
   Source diff read (#3) NOT yet done cleanly (Explore agent parroted this doc instead of diffing).

   DECALS CLUSTER RESOLVED: callees FUN_0016c5a0/0016c090 are void(void)/ported=null (original) at
   both ends, so the thunks tail-call-JMP to forward args. Callers are game_engine.c:9011-9016 inside
   a WIDGET render function (draws screen-quad with shader, routes transparent shader -> FUN_0017cbd0,
   ends with rasterizer_widget_set_tint_factor(widget)). This is the SHARED object-widget render path
   — normal-fire glow renders through it FINE -> it works for the orb too -> NOT the bug. The thunk
   sig fixes were coordinated with the game_engine caller port (benign). Decl-drift class EXHAUSTED.

CONCLUSION OF READ PASS: toggle-blind decl/@reg drift class found NO orb regression. Combined with
prior: weapon state byte-identical (EXP-6), widget render path works (normal glow), mgs2 new/delete
toggled-fine (EXP-8), creation FUN_00136150 + mgs2 update_proc are original/not-new. Everything on
the mgs2-widget hypothesis checks out yet orb is invisible. REFRAME NEEDED: either (a) the "orb =
mgs2 widget" ID is wrong (reconsider contrail / first-person effect / integrated-light / glw! type-2),
or (b) overcharge-specific CREATION/TRIGGER of the glow is gated by a value that's correct at state 3
(all captures) but wrong at state 2 (held, never captured), or (c) a NEW (false->true, toggle-visible
but untested) overcharge lift. Source diff read #3 still pending.

## EXP-13 (2026-06-05) — Toggle-bisect localizes the bug to objects.obj / object_lights.c

Decisive break in the investigation. Toggling the 13 object_lights.c color/light
functions (`0x139810 0x1398b0 0x1398d0 0x139990 0x139a30 0x139e50 0x13a250 0x13a420
0x13a5f0 0x13a740 0x13ab20 0x13b380 0x13bce0`) to `ported=false`:

- **All 13 OFF (original)** → **orb SHOWS** (live byte-verified: 0x139a30=`55 8B EC` prologue;
  control 0xae250=`68..C3` redirect still active). So the bug IS in this cluster.
- **Only 0x139a30 OFF, other 12 ON** → **orb GONE** (byte-verified live: 0x139a30 prologue,
  0x13bce0/0x13ab20 = `68..C3` redirects). 0x139a30 is OFF in both the working and broken
  builds, so it is **ELIMINATED**. Culprit ∈ the other 12.

This also **corrects the prior "orb = FP self-illum geometry" conclusion**: the bug lives in
the dynamic-point-light subsystem (object_lights.c, lights datum array `0x5a90bc`), which is
none of the effect/widget/contrail/particle pools the earlier scans checked — explaining why
those came up empty and why toggling the 7 draw TUs never fixed it.

**Round-1 12-way split** (balanced, suspects split across halves):
- Group A OFF (6): 0x139810 (color clamp), 0x1398b0, 0x1398d0, 0x139990, 0x139e50, 0x13a250
- Group B ON  (6): 0x13a420, 0x13a5f0, 0x13a740, 0x13ab20 (scene-light sample), 0x13b380, 0x13bce0 (per-object scene lighting)
- Orb SHOWS → culprit ∈ A; Orb GONE → culprit ∈ B.

## EXP-14 (2026-06-05) — CULPRIT CONFIRMED: FUN_0013b380 (render_lights)

Bisect resolved to a single function. Each step byte-verified live (QMP) before testing:

| Build (toggled OFF) | Orb | Inference |
|---|---|---|
| all 13 object_lights fns | SHOWS | bug in cluster |
| only 0x139a30 | GONE | 139a30 cleared |
| Group A {139810,1398b0,1398d0,139990,139e50,13a250} | GONE | A cleared |
| {13a420,13a740,13bce0} | GONE | cleared |
| only 0x13ab20 | GONE | 13ab20 cleared |
| **only 0x13b380** | **SHOWS** | **0x13b380 = CULPRIT** |

`FUN_0013b380` = `render_lights` (object_lights.c) — the per-frame loop that iterates all
active dynamic lights, computes color/intensity/position, and submits them to the rasterizer.
Reverting just it (our C → original) restores the overcharge orb with all 12 other
object_lights lifts still active. So the orb IS a dynamic point light, and our lift of
render_lights culls/blanks the overcharge light specifically (normal-fire glow already worked).

Mechanism candidates inside 13b380 (objects.c ~9137-9400+):
- visibility gate at ~9305: light submitted only if color (R/G/B @ +0x14/+0x18/+0x1c) != const 0x2533c0;
- color packing into light_params[10..13] (rasterizer reads 56-byte struct) ~9315-9328;
- attenuation scaling ~9270-9302 (local_c = 1 - obj[+0x32c]);
- note cluster history: commit 70b9de31 "object_lights vec3" — prior vec3/color lift bug here.

NEXT: VC71-diff 13b380 vs delinked reference (+ FPU-warn) to pin the divergence; fix the C lift;
rebuild with 13b380 ported=true and confirm the orb renders from our (fixed) code.

## EXP-15 (2026-06-05) — Render-array diff: dynamic light INNOCENT → corona is the orb

Captured the rasterizer render-light array (count @0x5a37e0, entries @0x5a37e4, 0x38 stride)
in BOTH builds with overcharge held:
- orb-shows (orig 13b380): count=1, light[0] pos=[-0.05,10.36,-0.26] color=[0.082,1,0.059] I=2.0
- broken   (our  13b380):  count=1, light[0] pos=[-11.54,0.59,-2.76] color=[0.082,1,0.059] I=2.0

In both, render pos ≈ the light's WORLD position (values differ only because the player stood
in different spots). Structurally identical: same count, same space, same color/intensity. So
the dynamic point light (FUN_001812c0 path) is submitted identically → **INNOCENT**. The
overcharge orb is therefore the **lens-flare corona** (objects.c 9364-9415) or a side effect,
NOT the dynamic light. Prime suspect: the flagged local_3e0/local_3a4 MSVC stack-alias in the
marker loop (9379-9404) — wrong marker stride/offset → coronas at garbage positions = invisible.
(Normal-fire "glow" is a separate firing effect, not these weapon lights — not a counterexample.)

## EXP-16 (2026-06-05) — ROOT CAUSE + FIX: undersized lens-flare marker buffer in FUN_0013b380

Traced the full original disasm of FUN_0013b380 (0x13b380-0x13bce0) against our lift. The
dynamic-light path, packing (light_params), FP-adjust call, and corona-loop *logic* all match.
The single divergence is a BUFFER SIZE:

- Original: marker fetch (object_get_markers_by_string_id @0x140f10 / FUN_000ddb90 @0xddb90,
  max_count=8) writes to buffer EBP-0x3dc; the corona loop reads markers from EBP-0x3a0
  (= fetch+0x3c=+60), stride 0x6c (108), up to 8 markers. Last byte = fetch+60+8*108 = fetch+924.
  So the buffer needs **924 bytes**.
- Our lift: `char local_3e0[864]` with `local_3a4 = local_3e0+60`. Comment encodes the miscalc:
  "local_3e0[60]+local_3a4[804]"=864, but 804 holds only 7.4 markers. **The 8th marker overflows
  by 60 bytes**, corrupting the corona loop's adjacent stack -> coronas at garbage -> invisible orb.

Why earlier evidence fit: the overflow happens AFTER the dynamic-light submit (FUN_001812c0), so
the captured render-light array was correct in both builds (dynamic light innocent). The orb's
visible element is the lens-flare CORONA, which the overflow breaks. Overcharge triggers 8 markers
(gun-tip flare set); normal-fire "glow" is a separate effect, unaffected.

FIX: `local_3e0[864]` -> `local_3e0[924]` (60 + 8*0x6c). CLAUDE.md "undersized callee buffer" hazard.
Verification = live orb with 13b380 ported=true (our fixed code).

## EXP-17 (2026-06-05) — Buffer fix is a RED HERRING (live test: still no orb)

Applied local_3e0[864]->[924], rebuilt (forced relink, .obj frame SUB ESP,0x440 confirms 924
fits tightly), deployed CLEAN (fresh token 15:50:01, verify OK, 13b380 live as our code). Orb
STILL does not render. So the undersized marker buffer is a real latent bug but NOT the orb
cause (fewer than 8 markers written => no overflow in practice). Kept the [924] fix (faithful to
original buffer extent EBP-0x3dc..EBP-0x40) but it is not the regression.

State of knowledge: bisect SOLID (13b380 our-code=no orb, original=orb). Dynamic-light render
array IDENTICAL between builds (innocent). Disasm trace of packing/submit/FP-adjust/corona-loop/
attenuation all MATCH original. The divergence is still unfound. Open: did NOT verify tag+0xb8
(lens-flare ref) for the orb light -> if -1, corona is skipped and orb != corona. Need a more
rigorous compiled-vs-original diff or a different discriminator.

## EXP-18 (2026-06-05) — CONFIRMED: visible orb is the CORONA, not the dynamic light

Two-build live comparison (13b380 ported=true=no-orb vs ported=false=orb-shows),
captured at held overcharge, paused.

**Render array (0x5A37E4) is byte-identical between builds** (color RGB =
3da8a8a3/3f800000/3d70f0e9, intensity=0x40000000=2.0, render_count=1). Only
position/aim dwords differ (player moved). => the dynamic point light is the
ILLUMINATION and is lifted correctly; it is NOT the visible sphere.

**The visible orb is the lens-flare/corona sprite.** Resolved the orb-family
light tags via tag instance table (base *(0x5054f0), stride 0x20, dataptr +0x14):
- idx 0x38b (0xe4ff038b, flags=7, SUBMITTED to render array): tag+0xb8 = -1 (no corona)
- idx 0x38f (0xe503038f, flags=6): tag+0xb8 = 0xe5040390 (LENS FLARE -> corona active)
- idx 0x38c (0xe500038c, flags=6): tag+0xb8 = 0xe501038d (LENS FLARE -> corona active)

The flags=6 green lights are bit0=0 (never enter the dynamic-light submit), so the
render array can match while the visible orb differs. The orb is rendered by the
corona path (objects.c 9364-9415), via the MARKER branch (light+0x58 spawn == -1,
FUN_000ddb90 FP-weapon markers, gated by local_14[0x64]==2 weapon type). That
branch is FP-weapon-specific, consistent with "only the orb missing; world coronas
fine." The lens-flare array (0x5A8F6C, count 0x5A90AC) is flushed+zeroed at the end
of 13b380, so it is invisible to post-hoc QMP capture — explaining why the bug
evaded the render-array comparisons.

**Next:** find the codegen/logic divergence in the corona MARKER branch (9382-9410):
FUN_0013fea0_2 return, local_14 condition, FUN_000ddb90/FUN_00140f10, marker loop
(local_3a4 stride 0x6c), local_3e0 buffer. Buffer size [864]->[924] already ruled
out (EXP-17). dd340/0013fea0 are ported (same in both builds) so the bug is in
13b380's own corona codegen.

## EXP-19 (2026-06-05) — ROOT CAUSE FOUND AND FIXED

Two XCALL type mismatches in FUN_0013b380's corona/lens-flare path:

1. **FUN_00099530** (real_a_rgb_color_to_pixel32, 0x99530):
   XCALL declared `float(*)(float,float*)` but function returns `uint32_t`.
   On x86, float return reads ST(0) (FPU); uint32_t return is in EAX.
   The compiler read garbage from ST(0) instead of the packed ARGB pixel in EAX.
   Corrupted `lf_params+0x18` (the alpha/color field of the lens-flare params).
   Fix: changed XCALL return type to `unsigned int(*)`.

2. **FUN_00180770** (0x180770):
   XCALL declared `unsigned char(*)(int)` but function takes `float alpha`.
   Call site cast `(int)local_8` which truncated the float to zero/one.
   Corrupted `lf_params+0x23` (the brightness encoding byte).
   Fix: changed XCALL param type to `float`, removed the `(int)` cast.

Both bugs only affected the corona/lens-flare submission path (lines 9365-9367),
which is why the dynamic-light render array was byte-identical between builds
(EXP-18) and the toggle-bisect correctly localized to FUN_0013b380.

The orb was always being queued (FUN_00181670 called correctly), but with wrong
alpha/brightness data. The downstream lens-flare renderer rejected or mis-rendered
entries with corrupted values.

**Bug class:** XCALL raw-cast type mismatches (float↔int return, float↔int param).
This is the same class as the `pointer_as_float` hazard in check_lift_hazards.py.
