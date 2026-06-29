# A/B Trajectory Testing — Deterministic Input + Memory-State Differential

Compare the **patched** (`default.xbe`) and **unpatched/faithful** (`cachebeta.xbe`)
builds by replaying the *same deterministic controller input* into both and diffing
the *game-state trajectory* they produce. A behavior divergence under identical
input is the regression signal.

This wires together two oracles that already exist but have never been connected:

| Oracle | Tool | Gives | Lacked |
|--------|------|-------|--------|
| **Input** | `tools/xbox/capture_scenario.py replay` | deterministic, open-loop controller playback on either build, same `core.bin`, RNG seed `0xDEADBEEF` | observed *nothing* (manual "looked right? y/N") |
| **State** | halo-memory-viewer `.halorec` + `tools/analysis/halorec_ai_diff.py` + `tools/equivalence/*` | per-frame memory pools; tick-aligned, slot-matched first-divergence diff | needed a *matched* patched/unpatched recording pair it never had |

Deterministic input replay **is** that matched pair: same input + same core +
slot-deterministic spawns ⇒ two trajectories that are tick-for-tick comparable.

See also: `docs/input-fixture-capture.md` (the input side),
`docs/halorec-timeseries-leverage.md` (the time-series diff tooling),
and `/mnt/g/dev/halo-memory-viewer` (the HMRC recorder/viewer).

## Two tiers: A discovers, B confirms

- **Tier A — whole-game trajectory differential (broad discovery / triage).**
  Replay a fixture, capture a discrete-state trajectory on each build, align by
  game tick, report the **first divergence** (which tick, which field, which
  entity). Cheap, wide coverage; weak per-finding rigor (see *Claims* below). A
  nightly **tripwire** over a fixture library.
- **Tier B — per-function equivalence under real state (proof).**
  `.halorec` frame → `--state-snapshot` → `unicorn_diff` runs the MSVC oracle vs
  the clang candidate for one function, isolated, with explicit float tolerance.
  Strong evidence. Already committed and in nightly `equivalence.yml`.

"Extensive **and** efficient" = Tier A finding *where* a build strayed and
automatically narrowing to a Tier B target that *proves* the specific function
diverged. Neither alone gives that.

## Status — implemented & validated (2026-06-29)

Increments 1–2 are built and the A/A determinism check has run. Tools (all
host-side Python, no Xbox at diff time):

| Tool | Role |
|------|------|
| `tools/equivalence/qmp_capture.py` | atomic `stop`/`memsave`/`cont` capture engine + pool resolution + verify-magic |
| `tools/equivalence/hmrc.py` | `.halorec` writer (viewer-loadable; round-trips with `halorec_to_snapshot`) |
| `tools/xbox/capture_trajectory.py` | tick-bucketed trajectory loop (gameplay-ready anchor) |
| `tools/equivalence/trajectory_diff.py` | **strict** byte-diff at exact tick — the A/A determinism check |
| `tools/equivalence/aa_check.py` | orchestrates replay×2 + capture + strict diff |
| `tools/equivalence/behavior_diff.py` | **tolerant** behavioral diff — the A/B regression oracle |

**A/A finding.** Two cachebeta replays of `a10/a10-checkpoint-5s-action` are
**deterministic**: matched frames are byte-identical *except* a handful of
monotonic per-tick counters (`actor+0x4a`, `+0x7c`, `prop+0x26`) that differ by
±1 in ~25% of frames. That is **capture-phase jitter**, not nondeterminism: QMP
pauses at an arbitrary instruction *within* a frame, so two runs catch tick T on
opposite sides of the engine's mid-frame counter bumps. Everything else
(positions, AI state, awareness, health) matches bit-for-bit. **Determinism +
capture are sound.** (Aside: the tick counter resets to the core's saved tick on
boot, so two core-boots share an absolute-tick timeline — align by absolute tick.)

**Two comparison tools, two jobs** (this is the load-bearing lesson):

- **A/A determinism check** → `trajectory_diff` (strict byte, exact tick). Answers
  "does the harness/engine reproduce itself?" Right to be strict; phase counters
  are the only allowed noise.
- **A/B regression oracle** → `behavior_diff` (tolerant). Strict byte-equality is
  *wrong* for A/B: clang-vs-MSVC 1-ULP float drift and capture-phase jitter are
  expected and benign. It aligns by **nearest tick within ±W**, matches entities
  by **slot**, compares **discrete fields as sustained onsets** (≥ `min_run`
  consecutive mismatches, so one-sample blips are ignored), **continuous fields
  (position) at value tolerance**, and **handles by liveness** (salts differ
  across builds). Validated: reports the A/A pair **clean**, and localizes a
  synthetic awareness perturbation to (slot, field, onset tick).

Not yet built: golden-freeze + A/B orchestrator (Increment 3), fixture library +
CI tripwire (Increment 4), and the watch-list config surface beyond the built-in
a10 AI default.

## Capture mechanism (decided): lift-native atomic QMP

Capture is host-side Python over **raw QMP `:4444`** — no Rust, no GUI, no
gdbstub. Per frame: `stop` → `memsave` the region set (virtual; **never**
`pmemsave`) → `cont`. Pausing makes the read atomic by construction and keeps the
title's VA mapped; it does not perturb tick-determinism (no wall-clock entropy in
the RNG path).

Hard rules (from `reference_xemu_qmp_memsave_capture`, proven again 2026-06-29):
- `memsave` is **virtual**; `pmemsave` is physical and reads the wrong bytes on
  this Cerbios/`kernel-irqchip=off` setup.
- HMP `memsave` command-line needs **doubled backslashes** in the Windows path
  (`G:\\dev\\halo\\...`); `G:\` → WSL `/mnt/g/`.
- Capture only during **active gameplay** (a menu/idle/pre-load pause reads zeros).
- **Verify-datum-magic check on every capture**: objtable ptr `*0x5a8d50` must be
  `~0x80xxxxxx`, and its target `+0x28` must equal `0x64407440`. A zero-read
  capture is silently garbage — discard, never diff.
- Never connect to gdbstub `:1234` — it halts the CPU and freezes XBDM:730 + game.

### Viability — PROVEN (Step 0, 2026-06-29)

`tmp/qmp_probe.py`, against the live dev xemu mid-gameplay:

```
[qmp] status: running
[qmp] stopped
[probe] *0x5a8d50 (objtable ptr) = 0x800b9370
[probe] objtable hdr: magic@+0x28=0x64407440 max_count@+0x20=2048 current_count@+0x2e=469
[probe] game_time globals=0x800612e8  tick@+0x0c=133820
[qmp] resumed (cont)
VERDICT: VIABLE
```

Five memsaves in one pause window were all coherent (atomic); a re-run showed the
tick advance `133820 → 134487` (cont resumes, game is live, frames are distinct
and alignable). The mechanism is confirmed viable on this exact setup.

## Artifact format (decided): HMRC from day one

The capture loop writes the viewer's **`.halorec`** format (gzip `HMRC`: magic,
version u32=1, name, frame_count u32, then per frame `t: f64`, `region_count u32`,
`regions: [(addr u32, len u32, bytes)]`). Our QMP capture already produces
`{addr: bytes}` region sets — the same shape — so a ~30-line HMRC writer makes
every capture **both** Python-diffable **and** loadable/scrubbable/diffable in
halo-memory-viewer for free (`halorec_to_snapshot.py` already reads this format).

## The observable (decided): capture broad, discover, codify

You do not need to know what to compare up front.

1. **Capture broadly** — the viewer-decodable region set, so the discovery surface
   is wide and the viewer's inspectors all light up:
   - object table `*0x5a8d50` (data_t: max_count@+0x20, size@+0x22, valid@+0x24,
     magic@+0x28, current_count@+0x2e, data@+0x34)
   - actor pool `*0x6325A4` (es=0x724; AI fields: salt+0x00, team+0x3e, awareness+0x268)
   - perception prop pool `*0x5AB23C` (es=0x138, magic+0x28=0x64407440)
   - player table, and game-time globals `0x45708C` → tick at `*ptr + 0x0C`
2. **Diff selectively** against a **watch-list manifest** — `(region + offset,
   type, label, tolerance)` — generalizing `halorec_ai_diff.py`'s currently
   hardcoded a10 field list into config.
3. **Discover, then codify** — replay+capture both builds, find where they diverge
   (viewer or coarse diff), promote the fields that matter into the manifest. That
   field set becomes the permanent regression check.

**Discrete/integer fields are the primary signal** (AI-state enum, awareness,
mode, health, alive, counts, ammo, score, RNG counter, tick). Floats (positions)
are secondary, compared at tolerance: a faithful clang lift still has 1-ULP x87
differences vs MSVC that snowball through physics/AI feedback; integer fields
resist that drift until it crosses a decision threshold.

## Components — reuse vs build

**Reuse (committed):**
- `capture_scenario.py replay --xbe {cachebeta,default}.xbe` — input driver.
- `tools/analysis/halorec_ai_diff.py localize A B` — tick-anchored, slot-matched
  first-divergence (generalize its field list → watch-list manifest).
- Tier B: `halorec_to_snapshot.py` + `unicorn_diff.py --state-snapshot`,
  `regression_targets.json`, nightly `equivalence.yml` (self-hosted).
- halo-memory-viewer — loads HMRC for human inspection (optional).

**Build (new, small):**
1. Atomic QMP capture helper (`stop`/`memsave`/`cont`, doubled-backslash path,
   verify-magic check) — extends `tools/equivalence/state_snapshot.py`. Probe
   `tmp/qmp_probe.py` is the seed.
2. Trajectory capture loop — poll game-tick (cheap 4-byte memsave); on tick-advance
   grab the region set, tag the frame with its tick, dedup → emit HMRC. (This is
   the viewer's capture worker, host-side.)
3. HMRC writer (~30 lines; inverse of the `halorec_to_snapshot` reader).
4. A/B orchestrator — freeze the cachebeta golden once per fixture (host-only),
   capture default, diff vs golden. Likely a `capture_scenario.py ab-test`
   subcommand.
5. Watch-list manifest + a config-driven diff (generalized `halorec_ai_diff`).

## Execution sequence

- **Step 0 — viability probe. DONE ✓ (2026-06-29).** `stop/memsave/cont` reads
  live, sane, atomic game memory; tick anchor works.
- **Step 1 — A/A determinism check. DONE ✓ (2026-06-29).** Two cachebeta replays
  of `a10/a10-checkpoint-5s-action`: deterministic (see *Status* above). Strict
  byte-diff flagged only capture-phase counters; the tolerant `behavior_diff`
  reports the pair fully clean. Harness sound.
- **Step 2 — minimal A/B.** Freeze the cachebeta golden; capture default; report
  first divergence.
- **Step 3 — discover & codify.** Explore divergences (viewer / coarse diff);
  promote the fields that matter into the watch-list manifest.
- **Step 4 — fixture library + CI.** Per-level fixtures, each with a frozen
  golden; nightly Tier-A tripwire on the self-hosted runner; on a trip, narrow to
  a Tier-B `regression_targets` entry.

## What Tier A can and cannot claim

- **Evidential strength decays with onset tick.** A discrete divergence at tick
  ~50 (before drift can accumulate) is strong bug evidence; a divergence 2000
  ticks in may be benign FP drift finally crossing a threshold. So **first-
  divergence (`localize`) is the right primitive; a full-trajectory byte-diff is
  the wrong one** — it would flag every benign drift.
- Tier A is **triage/localization, not a faithfulness proof.** Tier B is the proof.

## Preconditions / risks (none blocking, do not skip)

- **Time-to-control head alignment.** If a lift shifts how many ticks it takes to
  reach player control, fixed-tick presses land at a different game moment — a
  desync that looks like a bug. The A/A check can't catch this; do a one-time A/B
  head-alignment check the first time a fixture is reused against `default.xbe`
  (`docs/input-fixture-capture.md`, determinism model).
- **Verify-datum-magic on every capture** (above).
- **Capture-cadence jitter:** align by game tick + slot, never frame index.
- **Copyright.** Golden trajectories and `.halorec`/snapshots are literal game
  runtime memory → **host-only, gitignored, self-hosted CI only** (same class as
  the XBE and delinked `.obj`). Commit only the harness, the watch-list manifest,
  and the test case (recording id / tick / expected value) — never the bytes.

## Long-term efficiency (optional)

The ~0.28 s/frame round-trip cap on capture cadence limits localization
granularity. The clean fix for the *patched half* is an in-guest `DECOMP_CUSTOM`
sentinel-gated tick-hook (same pattern as the recorder and the die-to-core hook)
that writes the AI pool + key globals every tick: atomic, zero round-trips,
perfectly tick-aligned. cachebeta can't be instrumented that way — but it's the
frozen reference, so it doesn't need to be. (The viewer's proposed lean AI-only
capture mode is the alternative if staying viewer-based.)
