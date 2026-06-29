---
name: ab-trajectory-testing
description: >-
  A/B regression testing for the reimplementation: replay the SAME deterministic
  input on the patched (default.xbe) and unpatched (cachebeta.xbe) builds, capture
  each build's game-state trajectory to a .halorec, and diff their BEHAVIOR over a
  time window to detect where the lift strays from the original. Invoke when you
  want a mechanical (low-intervention) regression check across builds, to capture a
  state trajectory from live xemu, to freeze a faithful "golden" trajectory, or to
  run the A/A determinism check. Pairs the input oracle (capture_scenario.py replay)
  with the state oracle (QMP memory capture). Full design: docs/ab-trajectory-testing.md.
---

# A/B Trajectory Testing — does the patched build behave like the original?

**Invoke this skill when you need to:**
- A/B test a lift: replay one input fixture on `cachebeta.xbe` (faithful) and
  `default.xbe` (patched) and **diff the resulting behavior**, with localized output
  (time + field + entity) instead of an LLM eyeballing two runs.
- Capture a **game-state trajectory** from live xemu to a `.halorec` while a fixture replays.
- Freeze a **golden** faithful trajectory for a fixture (capture once, reuse).
- Run the **A/A determinism check** before trusting any A/B verdict.

This is the second oracle. The first oracle — deterministic controller input — is
the `input-replay-testing` skill (`capture_scenario.py`); this skill adds the
state-capture + diff half on top of it. Full design and rationale:
`docs/ab-trajectory-testing.md`.

Prereq: xemu up with QMP on `:4444` (the same box used for deploy/replay), and the
input fixture already captured (`input-recordings/levels/<level>/<scenario>/`).

## Two tools, two jobs — do not confuse them

| Tool | Question | Tolerance | Use for |
|------|----------|-----------|---------|
| `trajectory_diff.py` | byte-identical at the exact tick? | STRICT (masks known phase counters) | **A/A determinism check only** |
| `behavior_diff.py` | same behavior around the same time? | TOLERANT (±W tick, slot match, sustained onset, value eps, handle liveness) | **A/B regression oracle** |

`behavior_diff` is the one you want for "did my lift regress." `trajectory_diff` is
**wrong** for A/B — benign x87 float drift and sub-frame capture jitter make exact
byte-equality fail even on a faithful build. Use it only to prove the harness is
deterministic (A/A).

## The flow

### 0. A/A determinism check (run once per box/fixture, before anything else)

```bash
rtk python3 tools/equivalence/aa_check.py --level a10 --scenario a10-checkpoint-5s-action
```

Replays the fixture **twice on cachebeta** and strict-diffs. Must print
`VERDICT: CLEAN`. If it diverges, the non-determinism is in the harness/engine/
capture — stop and fix that; nothing built on top is trustworthy until A/A is clean.

### 1. Capture a trajectory (state oracle, runs alongside the input oracle)

Start the replay, then capture while it plays:

```bash
# terminal/step 1: deterministic input (idempotent, reboots fresh, verifies --xbe)
rtk python3 tools/xbox/capture_scenario.py replay --level a10 \
    --scenario a10-checkpoint-5s-action --xbe cachebeta.xbe

# step 2: capture the state trajectory while gameplay runs
rtk python3 tools/xbox/capture_trajectory.py -o tmp/a10_golden.halorec \
    --ticks 50 --quantum 2
```

`capture_trajectory` waits for gameplay, anchors on that engine event, and captures
the full pool set on a **fixed relative-tick grid** so two runs of the same fixture
land on the same ticks. The orchestrators (`aa_check`, and the A/B flow below) sequence
the replay + capture for you.

### 2. A/B: faithful golden vs patched

The one-command path (`ab_check` does the replay + capture of BOTH builds + the diff):

```bash
rtk python3 tools/equivalence/ab_check.py --level a10 --scenario a10-checkpoint-5s-action
# reuse a frozen golden (CI tripwire — capture cachebeta once, reuse forever):
rtk python3 tools/equivalence/ab_check.py --level a10 --scenario <s> --golden ~/halo-goldens/a10.halorec
# freeze it the first time:  add  --freeze --golden ~/halo-goldens/a10.halorec
# self-check determinism first: add  --aa-first
```

Or do the diff by hand if you already captured both trajectories (step 1):

```bash
rtk python3 tools/equivalence/behavior_diff.py tmp/a10_golden.halorec tmp/a10_default.halorec
```

- Exit `0` + `CLEAN` → behaviorally equivalent on the watch-list.
- Exit `3` → prints the **earliest sustained divergence**: `tick`, pool, `slot`,
  field, faithful-value vs patched-value. That (time, field, entity) is the lead.

Tune with `--window N` (tick tolerance, default 4) and `--min-run N` (consecutive
mismatched samples before reporting, default 2). Point at a different watch-list with
`--config cfg.json` (built-in default targets a10 AI: combat_state, awareness,
move_mode, firing_pos, path_active, unit/target liveness, position, plus aggregate
alive/in-combat counts).

## A discovers, B confirms

This is **Tier A** — a broad triage/localizer across the whole game state. It tells
you *where and when* a build strayed. To **prove** a specific function regressed,
hand the localized lead to **Tier B**: `unicorn_diff.py --state-snapshot` on that
function under real captured state (see `halo-verify-debug` / the equivalence lane).
A `.halorec` frame converts to a Unicorn state snapshot via `halorec_to_snapshot.py`.

## Must-know facts

- **Capture is atomic by construction.** `qmp_capture` does `stop`→`memsave`→`cont`
  on raw QMP `:4444`. **VIRTUAL `memsave` only** — `pmemsave` is physical and broken
  on this Cerbios/kernel-irqchip=off box. **Never** open the gdbstub `:1234` — a TCP
  connect there halts the emulated CPU and freezes the box.
- **Verify-magic check.** A capture is only trusted if the objtable
  (`*0x5A8D50` → `+0x28`) reads datum magic `0x64407440`. A menu/idle pause reads
  zeros — that is an unusable capture, not a divergence.
- **Align by absolute tick.** The tick (`*0x45708C + 0xC`) resets to the core's saved
  tick on boot, so two core-boots share one timeline. `behavior_diff`'s ±W window
  absorbs anchor/cadence/phase jitter on top of that.
- **`.halorec` and goldens are copyright.** They are literal game memory — host-only,
  gitignored, self-hosted CI only. **Commit only** the harness, the watch-list config,
  and the test case (recording id / tick / expected). Never commit captured bytes.
- **Open precondition.** A lift that shifts time-to-control desyncs the fixed-tick
  presses; ±W absorbs small shifts but not a large head desync. On first reuse of a
  golden against a new build, sanity-check that the heads still line up.

When a divergence points at a runtime/visual symptom rather than a state field, pair
with `crash-triage` / `debug-xemu`. For the input-capture internals, see
`input-replay-testing` and `docs/input-fixture-capture.md`.
