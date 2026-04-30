---
description: Show the decompilation frontier
agent: haiku
subtask: true
---

Run `python3 tools/analysis/frontier.py` to check the decompilation frontier, then briefly reason about the output and recommend the best next targets to work on. Focus on: nearly-complete TUs first, then high-impact modules (game_state, main, players), then small unstarted TUs. Keep output concise and table-format preferred.

When recommending targets, note that the orchestrator should use the @"xbox-halo-re-analyst" agent for performing the actual reverse engineering and lifting work on those targets.
