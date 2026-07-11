#!/usr/bin/env bash
# Pre-commit gate: assert-tail CALL targets (system_exit vs halt_and_catch_fire)
# in staged lifted .c files must match the pristine XBE (lift-learnings §29).
# Skips silently when no .c files are staged or the XBE is absent (CI).

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

# Fast bail: no staged .c files
if ! git diff --cached --name-only --diff-filter=ACMR | grep -q '\.c$'; then
    exit 0
fi

python3 "$REPO_ROOT/tools/audit/check_assert_targets.py" --staged-only --check
RC=$?
if [ $RC -ne 0 ]; then
    echo ""
    echo "pre-commit BLOCKED: a staged lift calls halt_and_catch_fire() where the"
    echo "binary calls system_exit(-1) (or vice versa). Fix the assert tail, or"
    echo "bypass with --no-verify in an emergency."
fi
exit $RC
