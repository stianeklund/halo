# Handover — a10 Grunt "Passive / Aim Way Off / Shots Miss"

> ## ⛔ SUPERSEDED (2026-06-28) — a DEFECT WAS FOUND; this doc's "NO DEFECT" conclusion is RETRACTED.
> The "one test we never ran" (patched-vs-unpatched A/B on the same a10 encounter) HAS now been run, as a
> **deterministic input-replay diff oracle** (fixture `a10-checkpoint-5s-action`, `default.xbe` vs `cachebeta.xbe`).
> It proved the engine is deterministic on fixed input AND that a6 diverges (vis=0 passive vs vis=3 engaged) at
> MATCHED geometry — a real build-caused regression, LOCALIZED to perception-prop PROMOTION (orphan `[prop+0x24]`=5
> bypasses the vis-writer; awareness stalls at 3). **Authoritative, current state:**
> `docs/handover-a10-grunt-passive-actor-moving.md` (top section). Everything below here is investigation history.

**Status (HISTORICAL, retracted):** OPEN (unresolved to user's satisfaction) — but **NO DEFECT FOUND** in the patched build after three independent investigations. Picking this back up requires the one test we have never run: a **patched-vs-unpatched A/B on the same a10 encounter**. Do **not** enter target-acquisition code without a binary-anchored anomaly.

**Map / difficulty:** a10 (Pillar of Autumn), Impossible/Legendary.
**Build under test:** patched/decomp (clang `-target i386-pc-win32`, x87) vs original `halo-patched/cachebeta.xbe`.
**Last updated:** 2026-06-23.

---

## 1. The symptom (user-reported)

Across several sessions the same complaint keeps resurfacing in different words:

- "enemy grunts too passive / slow to engage / shoots but won't advance"
- "the grunt / enemy bipeds aiming is way off in this build they are missing me … the shots are missing" — and specifically **"not almost hitting me — the shots are missing"** (wild misses, not near-misses).
- "No this seems somewhat new of a regression actually ironically."

**Key reframing (important):** the `actor_looking.c:529` crash (now fixed, see §5) was **blocking** sustained grunt combat every prior run — it halted at the magnum-handoff cutscene before the player ever reached a real firefight. So "new regression" almost certainly means **newly visible**, not newly introduced. The aim/passivity behavior became observable only once the crash stopped gating that part of the level.

---

## 2. (HISTORICAL — RETRACTED 2026-06-28) Former verdict — "NO DEFECT FOUND" (with evidence)

Every measurement to date says the patched aim pipeline is **faithful**. Decisive live read (pure GDB, player at `(-8.45, -2.23, 0)`, prop table 63/64 populated = perception active):

| Grunts | Combat target (`actor+0x610`) | Distance | Aim error vs player | Interpretation |
|---|---|---|---|---|
| #5 / #10 / #11 | set (`!= ffffffff`) | 4–5u | **7–18°** | Normal grunt spread → **FAITHFUL**, they hit |
| #6 / #7 | none (`ffffffff`) | 34u | **100–110°** | `aware268=0`, empty prop chain, `focus=ffffffff` → **have not perceived the player** → facing elsewhere is correct *search* behavior |

- **All aim vectors are clean unit normals (`|aim| = 1.0000`)** → the "corrupted-vector" hypothesis is **falsified**.
- **The aim pipeline provably works**: every grunt that has acquired the player points at the player.
- "Close grunts engage, far grunts don't" is textbook Halo AI, not a bug.

### The three independent negatives
1. **`0x24cf0` firing-pos LOS-scorer A/B toggle** (ported on/off) → user felt **no difference**.
2. **Static audit of every ported LOS producer** (`0x14df70` raycast + siblings) → all **byte-faithful** to disasm.
3. **Live aim read** (table above) → **faithful**.

Prior is now **strongly "no defect in the patched build."**

### Trap to avoid
Do **not** reinterpret look-state `actor+0x6e == 7` as "alerted about the player and failing to acquire." Its semantics are unknown and `aware268=0` directly contradicts that reading. Tuning non-anomalous AI to match a *felt* impression = inventing behavior, which is forbidden by the mission rules (binary is source of truth; no speculation).

---

## 3. The ONLY test that converges on "regression vs faithful"

We have **zero patched-vs-original data** — every reading is patched-only, matched against a bug we *expected* to find. To settle it:

1. Deploy the unpatched `halo-patched/cachebeta.xbe` (or a stock build).
2. Reach the **same** a10 encounter (same checkpoint, same weapon state — see confound below).
3. Read the **same** grunts' `actor+0x610` (combat target), aim vector, and distance using the recipe in §4.
4. Compare: does the **original** engage / aim at 34u where the patched build does not?
   - **Original behaves the same** → faithful; close this issue.
   - **Original engages where patched doesn't** → real regression; now you have a binary-anchored anomaly to bisect.

**Cost vs. prior:** this is roughly a full session of setup (boot stock XBE, navigate to the encounter, capture). Weigh against the strong "no-bug" prior before committing.

### Uncontrolled confound — CONTROL THIS in any comparison
The player is **unarmed at the checkpoint start** (lower threat profile → AI legitimately less aggressive). Any patched-vs-original comparison must match weapon state on both sides, or the result is meaningless.

---

## 4. Reusable live-forensics recipe (pure GDB read — no XBDM/QMP needed)

> ⚠️ **Landmine:** xemu's gdbstub on `:1234` HALTS the emulated CPU on *any* TCP connect, which freezes XBDM (`:730`) and the on-screen game. A **read** (`m<addr>,<len>`) is safe while halted and does not disturb XBDM, but **never "probe" :1234 to check state** — the probe itself re-halts. Recover the CPU with a raw GDB detach packet `$D#44` → `$OK`. See `reference_xemu_gdbstub_halts_cpu_landmine` in agent memory.

GDB RSP basics: packet = `$<data>#<2-hex-checksum>`, checksum = `sum(data bytes) & 0xff`. Read memory = `m<hexaddr>,<hexlen>`.

### Structure map (validated live this session)
- **Object table:** `*0x5a8d50` → `+0x38` data base; element stride `0xc`; `+0x8` = object-data ptr; magic `0x64407440` at `hdr+0x28`; name "object".
  - Object **position** @ `obj+0xc`; **aim / forward vector** @ `obj+0x1ec`.
- **Actors:** `*0x6325a4` → `+0x38` data base; element stride `0x724`. Fields (offset from element):
  | Offset | Type | Meaning |
  |---|---|---|
  | `0x0` | u16 | valid flag |
  | `0x18` | u32 | unit handle (→ object table) |
  | `0x3e` | i16 | team (**3 = grunt**) |
  | `0x50` | u32 | perception prop-chain head |
  | `0x60c` | — | in-combat flag |
  | `0x610` | u32 | combat target (`ffffffff` = none) |
  | `0x6e` | i16 | look-state ("6e"; `7` seen on far unaware grunts) |
  | `0x268` | i16 | awareness (`0` = has not perceived player) |
  | `0x270` | u32 | focus (`ffffffff` = none) |
- **Perception props:** `*0x5ab23c` → `+0x38` data base; element stride `0x138`; tracked obj @ `+0x18`; next @ `+0x8`; perception state @ `+0x24`; dist @ `+0x11c`.
- **Player:** `*0x5aa6d4` → `+0x38` player_data; **unit handle @ entry+0x34** (type-3 biped). Resolve through the object table for the player's live position.

### Procedure
1. Connect once to `:1234`, read the on-attach stop reply, `+`-ack.
2. Walk the actor table; for each `team==3` (grunt) read `0x610`, `0x268`, `0x50`, `0x6e`, and the unit handle `0x18`.
3. Resolve each grunt's unit handle → object → read aim `obj+0x1ec`; resolve player unit → position `obj+0xc`.
4. Compute aim error = angle between `obj+0x1ec` and `(player_pos − grunt_pos)`.
5. **Detach with `$D#44`** before doing anything else. Poll only XBDM:730 afterward, never :1234.

---

## 5. Tangled context — the `:529` crash that was masking this (FIXED)

This handoff exists partly because the grunt issue could not even be *observed* until the crash below was fixed. It is **DONE** — recorded here only so you don't re-chase it.

- **Symptom:** `EXCEPTION halt in actor_looking.c,#529: assert_valid_real_normal3d(direction)` at the magnum-handoff cutscene, three variants over three sessions: `(0,0,0)` → `NaN-X` → finite-huge `-3.5e31` X.
- **Root cause:** `look_spec_28660_safe` guard (our inserted code) used a hand-rolled lower-bound magnitude test `!(m2 >= 1e-8f)` that a finite-huge X overflowed past (`m2 → +inf`, `inf >= 1e-8f` is true).
- **Fix (commit `d8ad32eb`):** replace the hand-rolled test with the engine's own predicate `valid_real_normal3d` (`0x21fb0`) — `|dot(v,v)−1| < 0.001 && !NaN/Inf`, the exact check `FUN_00028660` calls internally. Closes both bounds, value-agnostic, byte-faithful to the release path.
- **Box-confirmed:** magnum-handoff cutscene now runs through into sustained grunt combat.
- **VC71 note:** `FUN_00029040` drops 70.6% → 55.5% — a **measurement artifact** (the old inline `m2` FPU incidentally LCS-aligned with the original; the CALL does not; the guard is inserted code the original lacks). Not a regression.
- Full detail: `project_poa_actor_looking_529_nan_guard_fixed` (agent memory), commits `7b5b3195` (0,0,0 variant), `2ccb9a1f` (NaN variant), `d8ad32eb` (finite-huge variant).

---

## 6. Concrete next steps to resume

1. **Decide** whether the cachebeta A/B (§3) is worth a session given the strong "no-bug" prior. This is the user's call.
2. If **yes**: stand up a clean xemu with QMP exposed (so gdbserver can be armed/disarmed deliberately, not the always-on landmine instance), boot stock `cachebeta.xbe`, navigate to the same a10 encounter **with matched weapon state**, capture the §4 table on the original.
3. Compare the two tables field-for-field. Only a divergence in `0x610` / aim / engagement at matched distance is a real anomaly worth opening AI code for.
4. If the comparison shows the original behaves the same → close this issue as **faithful**.
5. **Do not** start any code change without a binary-anchored anomaly. There is currently nothing to bisect.

---

## 7. References

**Agent memory notes:**
- `reference_a10_grunt_aim_faithful_no_defect` — the verdict + forensics recipe (primary).
- `project_poa_ai_passive_engage_forensics` — earlier "shoots but won't advance" forensics (root-caused a *different*, since-fixed scorer bug `FUN_00024cf0`, commit `2e4bb4ed`).
- `project_poa_actor_looking_529_nan_guard_fixed` — the crash that masked this.
- `reference_xemu_gdbstub_halts_cpu_landmine` — the :1234 halt landmine + recovery.
- `reference_a10_grunt_aim_faithful_no_defect` cross-links the live-read structure map.

**Commits:** `d8ad32eb` (:529 finite-huge fix), `2e4bb4ed` (FUN_00024cf0 swapped LOS branches), `2ccb9a1f`, `7b5b3195`.

**Source:** `src/halo/ai/actor_looking.c` (`look_spec_28660_safe` ~line 8405; primary→active look-spec copy ~8911-8912; consumed ~8936). `src/halo/math/vector_math.c:178` (`valid_real_normal3d`).
