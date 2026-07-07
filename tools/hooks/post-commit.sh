#!/usr/bin/env bash
# Regenerate CI status dashboard and refresh CodeGraph in background after every commit.
ROOT="$(git rev-parse --show-toplevel)"

LOG="/tmp/dashboard_regen.log"
(
    cd "$ROOT"
    tools/report/update_dashboard.sh >>"$LOG" 2>&1
    if command -v codegraph >/dev/null 2>&1; then
        codegraph sync >/dev/null 2>&1
    fi
) &
disown

echo "  [dashboard] Regenerating in background -> $LOG"
exit 0
