---
name: a10-slot33-aim-culprit-actor-look-update
description: a10 slot-33 grunt aim-vector A/B divergence PINNED by toggle-bisect to FUN_00029040 actor_look_update (0x29040, actor_looking.obj) — C3 (revert only 0x29040) makes slot-33 aim byte-identical to faithful. Two prior suspects (0x1b0630/0x1b3690) EXONERATED. The aim_threshold int->float (FILD) hypothesis was FALSIFIED on-box (C4: aim still 49deg, firing-pos effect uninterpretable/handle-level). Specific aim-selection defect line UNKNOWN; clean-vs-codegen OPEN. Next lead: look_mode/secondary_mode vector-selection branches actor_looking.c ~8560-8720.
metadata:
  type: project
---

# a10 slot-33 aim divergence ROOT = actor_look_update 0x29040 (2026-06-29)

## Verdict (toggle-bisect, all liveness-gated, fresh captures)
Level a10, fixture a10-checkpoint-5s-action, golden=cachebeta.xbe (frozen), candidate=default.xbe.
Slot-33 grunt salt 58246. Anchor tick ~6321. Discriminator = slot-33 aim+0x180 VALUE at
matched ABSOLUTE tick (tools/equivalence aim_align), NOT the bare behavior_diff verdict.

| config | 0x1b0630 | 0x1b3690 | 0x29040 | slot-33 aim vs golden | firing_pos/mm teardown @6402 |
|--------|----------|----------|---------|------------------------|-------------------------------|
| C0 both-on | ON | ON | ON | DIVERGES, peak ~49deg @6328-57, reconverges ~4deg | PRESENT (13611->0 / 2->1) |
| C1 | OFF | ON | ON | unchanged (patched aim byte-identical to C0: -0.31,-0.95) | PRESENT |
| C2 | OFF | OFF | ON | unchanged (entire ported unit_update reverted, still -0.31,-0.95) | PRESENT |
| C3 | ON | ON | **OFF** | **aim BYTE-IDENTICAL to golden (aim_dDEG=0.0 every tick)** | **GONE** |

C3 is decisive: cachebeta runs all-original; C3 is the patched build with ONLY 0x29040
reverted; C3's aim == golden exactly. So among all ported fns, reverting 0x29040 alone
restores faithful aim => **0x29040 is the SOLE ported source of the aim divergence**.
Toggle mechanics rule out callees (original-0x29040 calls the SAME patched callees as
patched-0x29040; only its own body changed C0->C3) => the bug is in 0x29040's BODY.

## Why NOT a precision ceiling (off the table — do not write "leave it")
- An A/B divergence can ONLY originate in a ported=true function. Unported code is
  byte-identical in both builds => identical x87 rounding => zero divergence. So the root
  is a ported body, not precision in the unported perception chain.
- At tick 6321 the two builds' POSITIONS MATCH (0.135,-2.047 both) yet aim differs 13-21deg
  => not physics/position-precision-driven; a contractive damped pursuit cannot accumulate
  ULP into 13deg (it damps noise). Structural, seeded pre-capture.
- C3 converts a SUSTAINED firing_pos/move_mode teardown into a 1-tick lag that resolves =>
  gross sustained AI behavior change => real defect regardless of mechanism (wrong op vs a
  ULP tipping an internal target/look-mode branch). Verdict = REAL DEFECT IN 0x29040, MUST FIX.

## The function + chain
- FUN_00029040 = actor_look_update (0x29040, actor_looking.c ~8436-9120, body 0x29040-0x2a287,
  ~4700 bytes, x87-heavy). Writes the AI's per-tick commanded vectors:
  actor+0x6fc = desired_facing, actor+0x708 = desired_aiming, actor+0x714 = desired_looking
  (asserts at 9105/9110/9115 "&actor->output.facing/aiming/looking_vector", lines 0x6c8-0x6ca).
- Feedback loop: actor+0x708 -> control+0x28 (unit_set_control 0x1af990, units.c:4685 validates
  aiming_vector) -> unit+0x1e0 desired aim -> smooth(0x10f770 UNPORTED)/euler(0x1b0630) ->
  biped+0x1ec -> actor+0x180 (copied by 0x3dc20 actors.c:7273) -> fed back next tick.
- Mechanism (PARTIAL): 0x29040 computes a wrong commanded aim => actor+0x180 diverges (49deg).
  Downstream a firing-pos/move_mode change happens later. NOTE: C4 (below) shows the firing-pos
  symptom and the aim divergence are NOT as tightly causally linked as first assumed — the
  int->float change shifted the firing-pos behavior without touching the aim, so they are at
  least partly separate effects of 0x29040.

## What is FAITHFUL (audited, exonerated — do not re-audit)
- 0x1b0630 euler aim leaf: cross products in unit_update (units.c 13300-02 / 13358-60) are
  byte-faithful vs disasm at 0x1b4097/0x1b42b8 (cross(forward,up) order verified); em struct
  layout {scale,fwd[3],left[3],up[3],trans[3]} contiguous; all 7 cdecl args to 0x1b0630
  match disasm push order. C1/C2 confirm behaviorally inert.
- 0x1b3690 unit_update caller body: exonerated by C2 (full revert, no change).
- 0x3dc20 actor_input_update cross products: up/right derive consistently from look in BOTH
  builds (golden up=(-look.y,look.x); candidate same) => faithful, c85ad81a fix holds.

## Residual (OUT OF SCOPE — separate known root)
C3 still shows slot-33 pos@6359 (~0.5u) + at_dest@6384 + path_active@6386. This is the
prior dig's Group-B path/movement ONSET-LAG (a different root), NOT the aim defect. The
aim defect (the task target) is pinned to 0x29040.

## aim_threshold int->float HYPOTHESIS — FALSIFIED on-box (C4). DO NOT re-propose.
A candidate §6 type bug WAS found and tested, and it does NOT fix the aim:
  actor_looking.c:8448 `int aim_threshold;`  8524 `aim_threshold = *(int*)(tag+0x12c)`
  -> passed to FUN_00027dd0(...,float threshold) at 7 sites. The CLANG on-box build emits a
  real FILD int->float conversion (build/.../actor_looking.c.obj @ _actor_look_update+0xbe5),
  while ORIGINAL + VC71/MSVC push the raw 32-bit bits. So clang turns a float threshold (e.g.
  0.866) into a huge value, which COULD make the angle test always fail. Plausible on paper.

C4 TEST (fix applied: declare+read aim_threshold as float; full rebuild, deployed+gated live,
ab_check vs frozen golden):
- AIM: UNCHANGED — slot-33 aim still diverges 49deg, candidate aim still (-0.31,-0.95),
  byte-identical to the broken C0/C2. => the int->float change does NOT fix the aim divergence.
  The FILD/aim_threshold hypothesis is REFUTED as the aim cause.
- FIRING-POS: changed but UNINTERPRETABLE. firing_pos is a datum HANDLE (cross-build salts/alloc
  differ), so the raw value is NOT rankable. C0 slot33 fp: 13611 then ->0/mm1 @6402 (teardown).
  C4 slot33 fp: constant -16819/mm2 from 6348, then ->0/mm1 @6446 (collapse merely SHIFTED +44
  ticks onto a different also-wrong handle). Golden holds 13611/mm2 indefinitely. So C4 is
  "differently broken," NOT demonstrably better or worse. Earlier "worse" (and the brief "better"
  re-read) were both UNSUPPORTED over-reads of behavior_diff's handle-level fp line.

NET: int->float is not the aim fix. Specific aim-selection defect line UNKNOWN. Clean-source-bug
vs clang-vs-MSVC-codegen still OPEN (C3 proves 0x29040's body is the sole ported source + a real
defect, but the exact mechanism inside it is unresolved).

### Mechanism LINE = NOT pinnable from these captures (2nd dig, 2026-06-29)
Tried to pin the specific aim-selection line via capture field-reads; HIT A CEILING. Findings:
- Selection INPUTS identical: look_type(+0x6dc)=3/3, look_mode_src(+0x3e8)=7/7, snap/panic/
  is_attacking all match golden==patched at the divergence ticks. Only sec_timer(+0x548)
  differs by 3-4 (phase/onset-lag, not a selector).
- NO cross product in actor_look_update's body => the coordinator's §4/§5 store-swap prior
  (from sibling 0x3dc20) is EXCLUDED. primary_vec/secondary_vec are filled by callee
  FUN_00028660 (ported, identical both builds) and only COPIED (correct [0]/[1]/[2] order).
  Output vector stores actor+0x6fc/0x708/0x714 + 0x5a4/0x5b0/0x5bc use CORRECT offsets.
- FUN_00028660 case 2 (look_spec_type=2 strict path): vector source = actor+0x5f2/0x628/0x270.
  At divergence, actor+0x270 (target prop) = 0xffffffff (NONE) in BOTH builds => case-2 third
  sub-case (target direction) returns 0 in both => divergence does NOT flow through that path.
  look_spec_28660_safe wrapper is faithful (only pre-validates copy-case tgt; delegates to 28660).
- Post-divergence field-mining nominated 3 "signals" (firing_pos handle, facing+0x174,
  strict_591/exact-facing flag) — ALL are post-divergence in a per-tick FEEDBACK loop
  (aim out -> next-tick in) with NO pre-divergence frame captured (earliest frame tick 6321
  ALREADY diverges). So cause/effect among in-window differing fields is UNRECOVERABLE from
  trajectory captures. strict_591 (actor+0x591) is an OUTPUT 0x29040 writes (8640/8742/8993,
  consumed 9103 actor_unit_control_exact_facing), NOT in the desired_aiming write path.

### CLEAN-vs-CODEGEN = OPEN (do not claim either). Resolver = dual-oracle-by-address.
The earlier "integer-gated => clean source bug" was PREMATURE: target270=NONE-in-both means the
divergence does not flow through the identical integer selectors. The ONLY tool that separates
"0x29040 computed the vector wrong from identical input" (clean/codegen bug in its body) from
"0x29040 propagated a diverged input" is DUAL-ORACLE-BY-ADDRESS: call original-0x29040 vs
lifted-0x29040 on ONE live actor at ONE tick with identical input, compare outputs
actor+0x6fc/0x708/0x714. Pair with a clean per-fn vc71-vs-delinked diff (solve the
_actor_look_update<->FUN_00029040 name-map + strip valid_real_normal3d inlining noise) for the
clean-source vs clang-codegen split. NOT yet done — gated on whether the divergence is a VISIBLE
defect (user's on-screen call), which outranks blind line-pinning.

### Method lesson (record): behavior_diff handle fields are NOT rankable
firing_pos/combat_tgt/unit etc. are datum handles; their raw values legitimately differ across
builds. behavior_diff flags the value delta, but "13611 vs -16819" cannot be called good/bad
without resolving the handle (held-vs-lost). move_mode (a real enum) IS comparable. Don't read
better/worse off a handle diff. And: on-box visual observation is evidence the field-diff lacks.

## METHOD / harness gotchas (route around; do not "fix" them this session)
- cmake/build_deploy_run.sh SKIP re-patch on a kb-only `ported` toggle (deps unchanged) ->
  redeploys a STALE XBE. MUST force: `rm halo-patched/default.xbe` then
  `HALO_RETAIL64_XBE=0 python3 tools/build/patch.py halo-patched/cachebeta.xbe build/halo halo-patched/default.xbe`
  (distinct sha per toggle proves it took), then `deploy_xbox.py --xbe-only --skip-build`.
  **[CLARIFIED 2026-06-29 coordinator: this applies to a MANUAL patch path or `build.py
  --target halo` (ELF-only, skips patched_xbe). `build_deploy_run.sh -q` passes NO --target ->
  build.py builds ALL targets -> `patched_xbe` is a phony `ALL` custom_target whose recipe
  ALWAYS re-runs patch.py against the current kb.json, so it DOES re-patch a kb-only toggle.
  ab_check uses that path. Generator here is Unix Makefiles, verified `make -C build -n
  patched_xbe`.]**
- verify_toggles_live `--all-off` FALSE-FAILS on original thunk functions **[FIXED 41bd61f1
  2026-06-29: classify() now requires an E9 jmp target to land in the impl region
  (>= IMPL_REGION_FLOOR 0x642000) before calling it ACTIVE; a jmp within original .text reads
  ORIGINAL. tools/xbox/test_verify_toggles_live.py locks it with the 0x1459d0 bytes. The
  route-around below is NO LONGER NEEDED.]**: 0x1459d0 (breakable_surfaces, ported=false in
  HEAD) has pristine bytes `E9 1B FF FF FF` = genuine JMP-thunk to 0x1458f0, which the OLD
  heuristic ("JMP at entry = active redirect") misread as ACTIVE. Not a stale build.
- kb.json edits: NEVER `rtk jq > kb.json` (truncates). Use python json.load/dump with
  `ensure_ascii=False` + trailing `\n` to preserve unicode em-dashes and keep the diff to the
  single ported line.
- Capture limitation: .halorec OBJECT pool captures es=12 (datum header array only) — biped
  s_object_data records (unit+0x1e0/+0x1ec/+0x266) are NOT in the recording. Read the aim
  OUTPUT from the ACTOR record instead (actor+0x180 aim = biped+0x1ec copy; actor+0x18c look;
  actor+0x174 facing). aim_align.py / actor_probe.py in tmp/a10ab/.

## ON-SCREEN VERDICT (2026-06-29) — CLOSED AS BENIGN, do not re-chase
User watched the slot-33 grunt on the looping a10-checkpoint-5s-action replay (default.xbe
patched build) and reported it "all looks pretty much normal." So per the gating question this
note raised: the 0x29040 aim divergence is a CONFIRMED fidelity divergence (the lifted body does
not byte-reproduce the original aim, proven by C3) but is NOT a visible defect. Decision: do NOT
build the dual-oracle-by-address harness to pin the exact line — not worth the setup for an
invisible wobble. Investigation CLOSED as a known-benign cross-build fidelity gap. 0x29040 stays
ported=true. Reopen only if a future on-screen observation shows slot-33 (or other) grunts
visibly aiming/firing wrong; then dual-oracle-by-address is the one tool that can finish it.
