---
description: Show the decompilation frontier
subtask: true
---

Show the decompilation frontier — what's implemented, what's remaining, and what to work on next. For actionable target selection, prefer the combined selector.

Steps:
1. Run `rtk python3 tools/analysis/fun_pipeline.py status` first. If the output shows more than ~200 functions in `<common>`, recommend running `/fun-pipeline reclassify` before proceeding — frontier scoring is inaccurate until `<common>` is reduced.
2. Run `rtk python3 tools/llm_auto_lift.py select --limit 20` for combined strategic priority and liftability.
2. If the user asks for raw frontier data, run `rtk python3 tools/analysis/frontier.py --limit 20`.
3. Report:
   - Total coverage: implemented / total (percentage)
   - Active TUs: partially-implemented translation units with remaining function count
   - Quick wins: unstarted TUs with few functions (1-3)
   - Recommended next targets with lane: `auto-lift`, `cache-context`, `manual-lift`, or `defer`
4. Keep the output concise — table format preferred.
