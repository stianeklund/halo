#!/usr/bin/env bash
# Pre-commit: run vc71_regression.py check on staged source files.
# Blocks the commit if any function's VC71 match dropped below its stored floor.
#
# - Only fires when src/**/*.c files are staged.
# - Files not yet in the baseline are silently skipped (no false positives on new ports).
# - To bypass in an emergency: git commit --no-verify

STAGED_SRC=$(git diff --cached --name-only --diff-filter=ACM | grep -E '^src/.*\.c$')
if [ -z "$STAGED_SRC" ]; then
    exit 0
fi

ROOT="$(git rev-parse --show-toplevel)"
REGR="$ROOT/tools/verify/vc71_regression.py"

if [ ! -f "$REGR" ]; then
    exit 0
fi

# Build the --source list from staged files only
SRC_ARGS=()
while IFS= read -r f; do
    SRC_ARGS+=("$f")
done <<< "$STAGED_SRC"

echo "vc71-regression: checking $(echo "$STAGED_SRC" | wc -l | tr -d ' ') staged source file(s)..."
python3 "$REGR" check --source "${SRC_ARGS[@]}" --quiet
RC=$?
if [ $RC -ne 0 ]; then
    echo ""
    echo "Fix the regression, then update the floor:"
    echo "  python3 tools/verify/vc71_regression.py update --source <file>"
    echo ""
    echo "Emergency bypass (records no floor update): git commit --no-verify"
fi
exit $RC
