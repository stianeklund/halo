---
name: input-fixture
description: "Interactive wizard to capture (record) or replay a deterministic controller-input fixture for a Halo CE Xbox level. Wraps tools/xbox/capture_scenario.py."
---

# Input Fixture — guided record/replay wizard

**Invoke this skill when the user wants to:**
- Record a gameplay sequence into a reusable per-level fixture
- Replay an existing fixture (optionally against the patched build to diff)
- Loop a fixture continuously
- Diff patched vs unpatched behavior on identical input

The underlying tool is `tools/xbox/capture_scenario.py`. The *why* and A/B
trajectory testing doctrine live in the `input-replay-testing` skill.

---

## Interactivity constraint (read first)

`capture_scenario.py record` is **interactive** — `input()` calls hit EOF under
a non-interactive shell. **Never run `record` through the Bash tool.**

Drive the split across conversation turns:
**`arm` → (user plays) → `finalize --no-validate` → `replay` → bless.**

---

# Part 1 — Record a new fixture

## Step 1 — collect arguments

Use **AskUserQuestion** in two rounds (4 questions max per call). Skip values
the user already provided.

**Round 1:**

| Question | Header | Options |
|----------|--------|---------|
| What do you want to do? | Action | **Record new fixture** (recommended) · Replay existing · Arm only · Finalize only |
| Which level? | Level | a10 (Pillar of Autumn) · a30 (Halo) · b30 (Silent Cartographer) · *Other* |
| Start mode? | Start | **mapreset** (recommended — boots straight into the level, no menu) · **core** (saved checkpoint) · **menu** (genuine New Game including menu nav) |
| Which build? | Build | **cachebeta.xbe** — unpatched (recommended) · default.xbe — patched |

**Round 2 (Record / Arm only):**

| Question | Header | Options |
|----------|--------|---------|
| Difficulty? | Difficulty | **impossible** (Legendary) · **hard** (Heroic) · **normal** · **easy** |
| Tail-pad (idle ticks kept past last input)? | Tail-pad | **90** (long) · **36** (default, short) · *Other* |
| Overwrite existing `known_good`? | Overwrite | Yes (adds `--force`) · No |

Free-text: **scenario id** (default `<level>-play-from-start`), **title**/**purpose**
(default `"<level> playthrough"`).

## Step 2 — arm and record

Common args: `--level L --scenario S --start MODE --xbe XBE --tail-pad N
--title "T" --purpose "P" [--difficulty D] [--force]`

1. **Arm** (non-interactive):
   ```bash
   rtk python3 tools/xbox/capture_scenario.py arm <common args>
   ```

2. **Hand off to user:** *"Recording is armed — the level is loading. Play your
   scenario now. Tell me when you've finished."*

3. **Finalize** when they say done (timeout: 200000):
   ```bash
   rtk python3 tools/xbox/capture_scenario.py finalize <common args> --no-validate
   ```

4. **Validate by replay:**
   ```bash
   rtk python3 tools/xbox/capture_scenario.py replay <common args>
   ```

5. **Bless** on success:
   ```bash
   rtk python3 -c "import sys; sys.path.insert(0,'tools/xbox'); import capture_scenario as c; c._set_known_good('L','S',True)"
   ```

---

# Part 2 — Replay an existing fixture

## Step 1 — discover available fixtures

```bash
find input-recordings/levels -maxdepth 1 -mindepth 1 -type d | sort
rtk ls input-recordings/levels/<level>/
```

## Step 2 — collect arguments (one AskUserQuestion call)

| Question | Header | Options |
|----------|--------|---------|
| Which level? | Level | a10 · a30 · b30 · *Other* |
| Which scenario? | Scenario | List actual folder names; mark `known_good=true` with checkmark |
| Which build? | Build | **cachebeta.xbe** (recommended) · default.xbe |
| Loop continuously? | Loop | **No** · Yes (requires default.xbe) |

## Step 3 — run the replay

```bash
rtk python3 tools/xbox/capture_scenario.py replay \
  --level <LEVEL> --scenario <SCENARIO> --xbe <XBE> [--loop]
```

Replay is non-interactive and idempotent. Run with `timeout: 300000`.

### Diff oracle (both builds)

```bash
rtk python3 tools/xbox/capture_scenario.py replay --level <L> --scenario <S> --xbe cachebeta.xbe
rtk python3 tools/xbox/capture_scenario.py replay --level <L> --scenario <S> --xbe default.xbe
```

---

## Notes

- **mapreset is the recommended mode.** `map_reset` restarts the level fresh,
  overriding any savegame — no file deletion needed.
- **Difficulty is per-recording (mapreset).** `--difficulty` pins
  `game_difficulty_set` in `init.txt` and is stored in fixture metadata.
- **Loop requires `default.xbe`.** `--loop` stages `core_loop.xts`.
- **Fixtures are host-only.** `input-recordings/` is gitignored.
- **Pass `--host <ip>`** if targeting a non-default box.
