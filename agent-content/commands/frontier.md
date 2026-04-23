---
description: Show the decompilation frontier
subtask: true
---

Show the decompilation frontier — what's implemented, what's remaining, and what to work on next.

Steps:
1. Parse kb.json for all functions with source mappings
2. Check each source file for function definitions (not just references)
3. Report:
   - Total coverage: implemented / total (percentage)
   - Active TUs: partially-implemented translation units with remaining function count
   - Quick wins: unstarted TUs with few functions (1-3)
   - Recommend the next best targets based on: closing out nearly-done TUs first, then high-impact modules (game_state, main, players), then small unstarted TUs
4. Keep the output concise — table format preferred
