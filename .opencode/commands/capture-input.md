---
description: Capture a deterministic controller-input fixture for Halo CE Xbox testing
---

Use `capture-input` and `input-replay-testing`.

Request: $ARGUMENTS

Run the fixture capture wizard behavior: identify level, scenario name, build, start mode, difficulty, tail padding, title, and purpose; then drive `rtk python3 tools/xbox/capture_scenario.py` with the selected options. Ask concise questions only for missing choices that affect the command.
