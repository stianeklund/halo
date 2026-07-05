#!/usr/bin/env bash
set -euo pipefail
NAME="${0##*/}"
VENV="${VENV_PYTHON:-.venv/bin/python3}"

usage() {
    echo "Usage: $NAME [--batch] [--skip-vc71] [--run-id NAME]"
    echo ""
    echo "Refresh the local progress dashboard from the latest build."
    echo ""
    echo "  --batch       Run batch equivalence tests first (slow)."
    echo "  --skip-vc71   Do not refresh tools/verify/vc71_scores.json."
    echo "  --full-vc71   Re-verify every TU instead of only changed ones"
    echo "               (default is incremental: skips unchanged TUs)."
    echo "  --run-id NAME Tag the snapshot with a custom run label."
    echo "               Default: 'local-<timestamp>'"
    echo "  --help        This message."
    exit 0
}

RUN_ID="local-$(date +%Y%m%d-%H%M%S)"
DO_BATCH=false
DO_VC71=true
# Incremental by default: only TUs whose source/reference changed are re-verified.
# A tooling/kb.json/delinked change bumps the epoch inside populate and forces a
# full pass automatically.  Use --full-vc71 to force a full re-verify.
VC71_MODE="--incremental"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --batch)    DO_BATCH=true; shift ;;
        --skip-vc71) DO_VC71=false; shift ;;
        --full-vc71) VC71_MODE=""; shift ;;
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

if $DO_VC71; then
    if [[ -n "$VC71_MODE" ]]; then
        echo "=== Refreshing VC71 scores (incremental) ==="
    else
        echo "=== Refreshing VC71 scores (full) ==="
    fi
    $VENV tools/verify/vc71_regression.py populate $VC71_MODE
fi

echo "=== Generating CI status page ==="
mkdir -p artifacts/progress
$VENV tools/report/generate_ci_status.py --output-dir artifacts/progress

echo "=== Generating dashboard ==="
$VENV tools/report/generate_decomp_report.py \
    --output artifacts/progress/report.json \
    --html artifacts/progress/index.html

echo "=== Recording snapshot ($RUN_ID) ==="
$VENV tools/report/history.py \
    --add-snapshot artifacts/progress/report.json

echo "=== Done ==="
echo "Open http://localhost:8080/ in your browser"
echo "Or run: python3 tools/report/progress_server.py"
