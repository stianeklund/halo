---
description: Replay an existing deterministic controller-input fixture
---

Use `replay-input` and `input-replay-testing`.

Request: $ARGUMENTS

Locate the requested fixture under `input-recordings/levels/`, report its key metadata, then run the appropriate `rtk python3 tools/xbox/capture_scenario.py replay ...` command. If the user asks for build comparison, route to `ab-trajectory-testing`.
