#!/usr/bin/env bash
# Pre-commit: run Unicorn regression tests when source files are staged.
# Quick mode (5 seeds) keeps it under 10 seconds.

STAGED=$(git diff --cached --name-only -- 'src/*.c' 'src/*.h' 'kb.json')
if [ -z "$STAGED" ]; then
    exit 0
fi

ROOT="$(git rev-parse --show-toplevel)"
SCRIPT="$ROOT/tools/equivalence/regression_test.py"

if [ ! -f "$SCRIPT" ]; then
    exit 0
fi

# Find python with unicorn installed
VENV_PY="$ROOT/.venv/bin/python3"
if [ -x "$VENV_PY" ]; then
    PY="$VENV_PY"
else
    PY="python3"
fi

# Check unicorn is importable before running (skip silently if not installed)
if ! "$PY" -c "import unicorn" 2>/dev/null; then
    exit 0
fi

echo "Running Unicorn regression tests (quick)..."
"$PY" "$SCRIPT" --quick
RC=$?
if [ $RC -ne 0 ]; then
    echo ""
    echo "Unicorn regression test FAILED. Fix the divergence before committing."
    echo "Run: python3 tools/equivalence/regression_test.py  (for full details)"
fi
exit $RC
