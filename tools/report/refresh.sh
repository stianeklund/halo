#!/bin/bash
# Quick refresh: regen dashboard + save history snapshot
set -e
cd "$(git rev-parse --show-toplevel)"
OUT=artifacts/progress
mkdir -p "$OUT"
python3 tools/report/generate_decomp_report.py \
    --output "$OUT/report.json" \
    --html "$OUT/index.html" \
    --history "$OUT/history.json"
python3 tools/report/history.py --add-snapshot "$OUT/report.json"
python3 tools/report/generate_decomp_report.py \
    --output "$OUT/report.json" \
    --html "$OUT/index.html" \
    --history "$OUT/history.json"
echo "Done: $OUT/index.html"
