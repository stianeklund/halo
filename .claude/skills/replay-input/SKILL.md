---
name: replay-input
tier: user
description: >-
  Quick wizard to replay an existing deterministic controller-input fixture on
  xemu or real Xbox. Invoke when the user wants to replay a stored recording,
  run a fixture on a specific build (cachebeta vs patched), loop a fixture
  continuously, or diff both builds on the same input — without remembering the
  capture_scenario.py flags.
---

# Replay Input — quick replay wizard

**Invoke this skill when the user wants to:**
- Replay a stored input fixture ("replay the a10 recording", "run the grunt-spawn fixture")
- Test on a specific build (`cachebeta.xbe` vs `default.xbe`)
- Loop a fixture continuously (`--loop`)
- Diff patched vs unpatched behavior on identical input

This skill is the **replay-only** fast path. For recording new fixtures use `/capture-input`.

---

## Step 1 — discover available fixtures

Run this first so the user picks a real scenario name:

```bash
rtk python3 tools/xbox/capture_scenario.py replay --help 2>/dev/null | head -5
```

Then list what exists for the target level (or all levels if unspecified):

```bash
# All levels
find input-recordings/levels -maxdepth 1 -mindepth 1 -type d | sort

# Specific level
rtk ls input-recordings/levels/<level>/
```

Read the `recording.json` for any fixture the user names to show them the key metadata (difficulty, start_condition, known_good, estimated_ticks).

---

## Step 2 — collect arguments (one AskUserQuestion call)

Ask all of these in a **single** AskUserQuestion (max 4 questions). Skip any the user already provided.

| Question | Header | Options |
|----------|--------|---------|
| Which level? | Level | a10 (Pillar of Autumn) · a30 (Halo) · b30 (Silent Cartographer) · *Other* |
| Which scenario / fixture? | Scenario | List the actual folder names from `input-recordings/levels/<level>/`; mark `known_good=true` ones with ✓ |
| Which build? | Build | **cachebeta.xbe** — unpatched / faithful (recommended) · default.xbe — patched (your lifted C) |
| Loop continuously? | Loop | **No** (single replay) · Yes — loop (requires default.xbe + `--loop`) |

If the user says "diff both builds" skip the Build question and run two replays in sequence (cachebeta first, then default).

---

## Step 3 — run the replay

```bash
rtk python3 tools/xbox/capture_scenario.py replay \
  --level <LEVEL> \
  --scenario <SCENARIO> \
  --xbe <XBE> \
  [--loop]
```

- `replay` is **non-interactive and idempotent** — safe to run directly via Bash.
- It frees any stale `state.data` handle from a prior run (soft reboot → hard QMP reset if needed) before starting.
- It verifies the requested `--xbe` is actually running before returning.
- Run with `timeout: 300000` (5 min) to cover the level boot + fixture duration.

### Diff oracle (both builds on identical input)

```bash
# unpatched first
rtk python3 tools/xbox/capture_scenario.py replay --level <L> --scenario <S> --xbe cachebeta.xbe

# then patched
rtk python3 tools/xbox/capture_scenario.py replay --level <L> --scenario <S> --xbe default.xbe
```

Tell the user what to look for between the two runs.

---

## Notes

- **Loop requires `default.xbe`.** `--loop` stages `core_loop.xts` so the input replays and the core reloads each cycle (no user action needed). It only works with the patched build.
- **Death ≠ core reload.** Dying in-game reverts to the campaign level start, not the fixture core. Re-run `replay` to get back to the fixture start.
- **Fixtures are host-only.** `input-recordings/` is gitignored — never commit fixture files.
- **start mode is embedded in the fixture.** The `recording.json` `start_condition` field shows how the fixture was captured; `replay` respects the same mode automatically (you don't need to pass `--start`).
- **Pass `--host <ip>`** if targeting a non-default box.
- For capture internals see `docs/input-fixture-capture.md` and the `input-replay-testing` skill.
