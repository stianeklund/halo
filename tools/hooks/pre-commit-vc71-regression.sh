#!/usr/bin/env bash
# Pre-commit: run vc71_regression.py check on staged source files, then
# refresh the floor with any improvements or new ports and re-stage scores.
#
# - Only fires when src/**/*.c files are staged.
# - Files not yet in the baseline are silently skipped by `check` (no false
#   positives on new ports).
# - After `check` passes, `update` records new ports and raises any improved
#   floors. If vc71_scores.json changed, it's auto-staged into the commit so
#   the progress dashboard stays in sync without a separate step.
# - To bypass in an emergency: git commit --no-verify
#
# COST. Both passes below are memoized: run_vc71_verify caches each TU's
# measurement under (tool epoch + source sha + the TU's decl.h slice), so
# `update` reuses what `check` just measured instead of recompiling everything a
# second time, and a hook killed by a timeout does not start over. The key covers
# every input that can move a score EXCEPT the VC71 toolchain environment itself
# (wine/CL), which is why compile failures are never cached. If you have just
# repaired that environment and want to force a clean re-measure:
#   VC71_NO_MEASURE_MEMO=1 git commit ...
# or delete artifacts/audit/vc71_measure_cache.json.

STAGED_SRC=$(git diff --cached --name-only --diff-filter=ACM | grep -E '^src/.*\.c$')
if [ -z "$STAGED_SRC" ]; then
    exit 0
fi

ROOT="$(git rev-parse --show-toplevel)"
REGR="$ROOT/tools/verify/vc71_regression.py"
SCORES="$ROOT/tools/verify/vc71_scores.json"

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
    exit $RC
fi

# Check passed: promote any new/improved scores into the committed JSON so
# the progress dashboard reflects the lift on the same commit. Update failure
# here is non-blocking — check already validated the floors, this is just a
# best-effort sync of the dashboard data.
SCORES_BEFORE=""
if [ -f "$SCORES" ]; then
    SCORES_BEFORE=$(git hash-object "$SCORES")
fi
python3 "$REGR" update --source "${SRC_ARGS[@]}" >/dev/null 2>&1 || {
    echo "vc71-regression: update failed (non-blocking); scores not refreshed"
    exit 0
}
SCORES_AFTER=""
if [ -f "$SCORES" ]; then
    SCORES_AFTER=$(git hash-object "$SCORES")
fi
if [ "$SCORES_BEFORE" != "$SCORES_AFTER" ]; then
    git add "$SCORES"
    echo "vc71-regression: auto-staged refreshed vc71_scores.json"
fi
exit 0
