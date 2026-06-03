#!/usr/bin/env bash
# Regenerate CI status dashboard in background after every commit.
# Fast: only generate_ci_status.py + generate_decomp_report.py (no batch tests).
ROOT="$(git rev-parse --show-toplevel)"
VENV="${ROOT}/.venv/bin/python3"
[ -x "$VENV" ] || VENV="$(command -v python3)"

LOG="/tmp/dashboard_regen.log"
(
    cd "$ROOT"
    mkdir -p artifacts/progress
    "$VENV" tools/report/generate_ci_status.py --output-dir artifacts/progress >>"$LOG" 2>&1 &&
    "$VENV" tools/report/generate_decomp_report.py \
        --output artifacts/progress/report.json \
        --html artifacts/progress/index.html >>"$LOG" 2>&1
) &
disown

echo "  [dashboard] Regenerating in background → $LOG"
exit 0
