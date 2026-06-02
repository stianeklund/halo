#!/usr/bin/env bash
set -euo pipefail
NAME="${0##*/}"
VENV="${VENV_PYTHON:-.venv/bin/python3}"

usage() {
    echo "Usage: $NAME [--batch] [--run-id NAME]"
    echo ""
    echo "Refresh the local progress dashboard from the latest build."
    echo ""
    echo "  --batch       Run batch equivalence tests first (slow)."
    echo "  --run-id NAME Tag the snapshot with a custom run label."
    echo "               Default: 'local-<timestamp>'"
    echo "  --help        This message."
    exit 0
}

RUN_ID="local-$(date +%Y%m%d-%H%M%S)"
DO_BATCH=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --batch)    DO_BATCH=true; shift ;;
        --run-id)   RUN_ID="$2"; shift 2 ;;
        --help|-h)  usage ;;
        *)          echo "Unknown: $1"; usage ;;
    esac
done

cd "$(git rev-parse --show-toplevel 2>/dev/null || echo .)"

if $DO_BATCH; then
    echo "=== Running batch equivalence tests ==="
    $VENV tools/equivalence/batch_verify.py \
        --seeds 50 --timeout 120 --csv --skip-existing \
        --allowlist tools/equivalence/batch_verify_allowlist.json \
        --output-dir artifacts/batch_verify
fi

echo "=== Generating dashboard ==="
mkdir -p artifacts/progress
$VENV tools/report/generate_decomp_report.py \
    --output artifacts/progress/report.json \
    --html artifacts/progress/index.html

echo "=== Recording snapshot ($RUN_ID) ==="
$VENV tools/report/history.py \
    --add-snapshot artifacts/progress/report.json

echo "=== Done ==="
echo "Open http://localhost:8080/ in your browser"
echo "Or run: python3 tools/report/progress_server.py"
