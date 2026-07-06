#!/usr/bin/env bash
set -euo pipefail
NAME="${0##*/}"
VENV="${VENV_PYTHON:-.venv/bin/python3}"

usage() {
    echo "Usage: $NAME [--batch] [--vc71|--full-vc71] [--run-id NAME]"
    echo ""
    echo "Refresh the local progress dashboard from the latest build."
    echo ""
    echo "By default this ONLY re-renders the dashboard from the existing"
    echo "scores — it does NOT run VC71 verification.  Verification is expensive"
    echo "and now happens at lift time (the lift pipeline runs an incremental"
    echo "populate for the TU it just changed) and via the pre-commit hook, so a"
    echo "dashboard refresh no longer needs to re-verify anything.  Opt in with"
    echo "--vc71 / --full-vc71 when you explicitly want to recompute scores."
    echo ""
    echo "  --batch       Run batch equivalence tests first (slow)."
    echo "  --vc71        Refresh VC71 scores incrementally before rendering"
    echo "               (only TUs whose inputs changed are re-verified)."
    echo "  --full-vc71   Refresh VC71 scores, re-verifying every TU (full pass)."
    echo "  --skip-vc71   Accepted for back-compat; now the default (no-op)."
    echo "  --run-id NAME Tag the snapshot with a custom run label."
    echo "               Default: 'local-<timestamp>'"
    echo "  --help        This message."
    exit 0
}

RUN_ID="local-$(date +%Y%m%d-%H%M%S)"
DO_BATCH=false
# Render-only by default: verification is decoupled from dashboard rendering.
# --vc71 (incremental) or --full-vc71 (full) opt back in to running populate.
DO_VC71=false
VC71_MODE="--incremental"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --batch)     DO_BATCH=true; shift ;;
        --vc71)      DO_VC71=true; VC71_MODE="--incremental"; shift ;;
        --full-vc71) DO_VC71=true; VC71_MODE=""; shift ;;
        --skip-vc71) DO_VC71=false; shift ;;
        --run-id)    RUN_ID="$2"; shift 2 ;;
        --help|-h)   usage ;;
        *)           echo "Unknown: $1"; usage ;;
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
