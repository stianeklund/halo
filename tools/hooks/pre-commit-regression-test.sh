#!/usr/bin/env bash
# Pre-commit: run Unicorn regression tests when source files are staged.
# Quick mode is 5 seeds per function, but the corpus has grown to 69 functions
# and the run now takes ~130s -- not the "under 10 seconds" this comment used
# to claim. The harness self-tests below add ~5s.
#
# tools/equivalence/*.py is in the filter deliberately. It used to gate on
# src/kb.json only, so a commit that changed nothing but the harness itself --
# exactly the change that can break the harness -- skipped this hook entirely
# and reported no opinion at all (2026-07-29).

STAGED=$(git diff --cached --name-only -- 'src/*.c' 'src/*.h' 'kb.json' \
                                          'tools/equivalence/*.py')
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

# Harness self-tests first, and only when the harness itself changed: they are
# the pins on the comparator's soundness (a wrong offset into the right global
# must still be REPORTED), and they fail in ~5s, so they should speak before
# the slower differential does.
HARNESS=$(echo "$STAGED" | grep '^tools/equivalence/.*\.py$')
SELFTESTS="$ROOT/tools/equivalence/run_all_tests.py"
if [ -n "$HARNESS" ] && [ -f "$SELFTESTS" ]; then
    echo "Running equivalence harness self-tests..."
    "$PY" "$SELFTESTS"
    RC=$?
    if [ $RC -ne 0 ]; then
        echo ""
        echo "Equivalence harness self-tests FAILED. Fix before committing."
        echo "Run: python3 tools/equivalence/run_all_tests.py -v"
        exit $RC
    fi
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
