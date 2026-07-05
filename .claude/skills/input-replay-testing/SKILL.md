---
name: input-replay-testing
tier: agent
triggers: ["input replay", "deterministic input", "capture scenario", "capture_scenario", "replay fixture", "input fixture"]
description: >-
  Deterministic controller-input record/replay for testing on real engine
  (xemu/XBDM). Invoke when you want to capture gameplay once and replay the EXACT
  same input over and over, drive a level reproducibly, build a per-level test
  fixture, or diff the patched (default.xbe) vs unpatched (cachebeta.xbe) build
  on identical input. Wraps tools/xbox/capture_scenario.py. Full reference:
  docs/input-fixture-capture.md.
---

# Input Replay Testing — Deterministic Record/Replay

**Invoke this skill when you need to:**
- Capture a gameplay sequence and replay the **exact same controller input** every run
- Drive a level/scenario reproducibly for automated or repeated testing
- Build a reusable **per-level test fixture** (input + checkpoint core + boot recipe)
- Diff behavior of the **patched** build vs the **unpatched** build on identical input
- Reset to a known in-game start state without re-playing the level

Tool: `tools/xbox/capture_scenario.py`. Reference: `docs/input-fixture-capture.md`.
Prereq: xemu (or Xbox) up and reachable over XBDM (same transport as deploy).

## The loop (this is the common case)

Replay an existing fixture, on demand, over and over:

```bash
# list fixtures
ls input-recordings/levels/a10/
python3 tools/xbox/input_recordings.py ls --level a10

# replay (reboots fresh into the core, releases any open handle, verifies the build)
python3 tools/xbox/capture_scenario.py replay --level a10 --scenario a10-checkpoint-5s-action

# DIFF ORACLE: same input, both builds
python3 tools/xbox/capture_scenario.py replay --level a10 --scenario a10-checkpoint-5s-action --xbe cachebeta.xbe   # unpatched/faithful
python3 tools/xbox/capture_scenario.py replay --level a10 --scenario a10-checkpoint-5s-action --xbe default.xbe     # patched
```

`replay` is idempotent and self-healing: it frees a `state.data` handle a prior
playback left open (soft reboot → hard QMP reset if needed) and confirms the
requested `--xbe` is actually running before returning. Run it repeatedly.

## Capture a new fixture

```bash
# 1. In-game at the spot you want the test to start, open console (~): core_save
# 2. interactive one-shot (run in a real terminal — it waits for you to play):
python3 tools/xbox/capture_scenario.py record --level a10 --scenario <name> \
    --title "..." --purpose "..."
# 3. play the route -> press Enter -> it trims/stores/validates -> answer "y" to bless known_good
```

Crash-safe split (play at your own pace): `arm` then `finalize` with the same
`--level/--scenario`. Start modes: `--start core` (default; needs a saved core,
tightest) or `--start menu` (script-faithful AI, includes menu nav). `--start map`
is intentionally absent (never validated).

`selftest` validates the trim/analyze logic with no hardware.

## Must-know facts (the box is the only oracle)

- **`--xbe` selects the build and is verified.** `cachebeta.xbe` = unpatched
  faithful, `default.xbe` = patched. The same `core.bin` loads into either.
- **Fixtures are host-only.** `input-recordings/` is gitignored; `core.bin` is a
  copyrighted game-state heap snapshot — never commit it.
- **Death does NOT return to the core** — it reverts to the campaign level start
  (`game_state_revert`). Reset by re-running `replay`, not by dying. (A gated
  die-to-core debug hook is documented but unimplemented — see the reference doc.)
- The native recorder (sentinels `d:\write.xts`/`read.xts`/`loop.xts` →
  `d:\state.data`) is checked once at boot; the tool drives all of it for you.

When something looks wrong on the box, this is a runtime/visual matter — pair with
`crash-triage` / `debug-xemu`. For capture internals (sentinels, packet layout),
see `docs/xbox-pad.md`.
