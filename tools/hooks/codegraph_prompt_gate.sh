#!/bin/sh
# Intent-gated CodeGraph prompt frontloading (arch review 2026-07-07, fix #1).
#
# The user-global `codegraph prompt-hook` fires on ANY prompt that names an
# indexed symbol (~16KB / ~4K tokens per hit), which is redundant noise in this
# token-disciplined repo where CLAUDE.md already mandates on-demand
# `codegraph explore`. Project settings therefore set CODEGRAPH_NO_PROMPT_HOOK=1
# to silence the global hook, and THIS project hook re-enables the injection
# selectively: only for debug / hard-problem / explicit-codegraph prompts, where
# frontloaded structural context (call paths, dispatch hops) earns its tokens.
#
# Double gate: this intent gate, then codegraph's own structural/symbol gate
# and 16KB cap still apply inside `codegraph prompt-hook`.
# Contract (like upstream): never break the prompt — all failure paths exit 0.

payload=$(cat) || exit 0
[ -n "$payload" ] || exit 0

prompt=$(printf '%s' "$payload" | jq -r '.prompt // empty' 2>/dev/null) || exit 0
[ -n "$prompt" ] || exit 0

# Hard-problem / debugging / explicit-request intent. Keep this list narrow:
# routine lift & command prompts must stay zero-cost.
if printf '%s' "$prompt" | grep -qiE \
  'crash|page fault|access.violation|assert|freeze|hang|deadlock|regression|\bbug\b|broken|divergen|corrupt|garbage|\bstuck\b|root.cause|investigate|\bdebug|misbehav|wrong (value|color|position|behavior|output|result)|codegraph'; then
  printf '%s' "$payload" | CODEGRAPH_NO_PROMPT_HOOK=0 codegraph prompt-hook 2>/dev/null
fi
exit 0
