---
name: input-replay-testing
tier: agent
triggers: ["input replay", "deterministic input", "capture scenario", "capture_scenario", "replay fixture", "input fixture", "halorec", "trajectory", "a/b", "ab check", "ab_check", "behavior_diff", "aa check", "trajectory diff", "regression oracle"]
description: Deterministic controller-input record/replay and A/B trajectory behavior-diff testing on real engine (xemu/XBDM). Invoke to capture gameplay fixtures, replay inputs, or diff patched vs unpatched game-state trajectories.
---

# Input Replay & Trajectory Testing

**Invoke this skill when you need to:**
- Capture a gameplay sequence and replay the **exact same controller input** every run
- Drive a level/scenario reproducibly for automated testing
- Build a reusable **per-level test fixture** (input + checkpoint core + boot recipe)
- Diff behavior of the **patched** (`default.xbe`) vs **unpatched** (`cachebeta.xbe`) build on identical input
- Run an **A/B trajectory behavior-diff test** (`behavior_diff.py`) to detect game-state divergence over time
- Run an **A/A determinism check** (`aa_check.py`) before trusting an A/B verdict

Primary Tools: `tools/xbox/capture_scenario.py`, `tools/equivalence/ab_check.py`. References: `docs/input-fixture-capture.md`, `docs/ab-trajectory-testing.md`.

---

## 1. Input Replay (The Fast Loop)

Replay an existing fixture on demand:

```bash
# List available fixtures:
python3 tools/xbox/input_recordings.py ls --level a10

# Replay fixture (reboots fresh into core, verifies build):
python3 tools/xbox/capture_scenario.py replay --level a10 --scenario a10-checkpoint-5s-action

# Diff input across builds:
python3 tools/xbox/capture_scenario.py replay --level a10 --scenario a10-checkpoint-5s-action --xbe cachebeta.xbe   # unpatched/faithful
python3 tools/xbox/capture_scenario.py replay --level a10 --scenario a10-checkpoint-5s-action --xbe default.xbe     # patched
```

---

## 2. A/B Trajectory State Testing (Behavioral Diff)

To diff the resulting game state over time (objects, players, actors) rather than manually watching xemu:

```bash
# One-command A/B regression check (builds, deploys, gates liveness, replays, captures, diffs):
rtk python3 tools/equivalence/ab_check.py --level a10 --scenario a10-checkpoint-5s-action
```

### Differ Tool Reference:
- **`behavior_diff.py`** (TOLERANT): Compares behavior over time (tick windows, slot match, value eps). Use as the **A/B regression oracle**.
- **`trajectory_diff.py`** (STRICT): Requires byte-exact match at exact ticks. Use ONLY for **A/A determinism checks**.

---

## 3. A/A Determinism Verification

Run once per fixture before trusting any A/B verdict to ensure the harness itself is deterministic:

```bash
rtk python3 tools/equivalence/aa_check.py --level a10 --scenario a10-checkpoint-5s-action
```

Replays the fixture twice on `cachebeta.xbe` and strict-diffs. Must return `VERDICT: CLEAN`.

---

## 4. Capture a New Fixture

1. In-game at the desired test start location, open debug console (`~`) and type: `core_save`.
2. Run the interactive recorder:
   ```bash
   python3 tools/xbox/capture_scenario.py record --level a10 --scenario <name> --title "..." --purpose "..."
   ```
3. Play the route, press Enter, trim/validate, and confirm blessing to save as known good.
