#!/usr/bin/env bash
# Run the hazard-lint self-tests when the lint or its tests are staged.
#
# check_lift_hazards.py is the gate every other gate leans on; narrowing a
# check into silence (a too-strict filter, a regex that stops matching) leaves
# every pipeline green while the class of bug it was written for ships again.
# These tests take ~25ms, so gate on them whenever the lint itself changes.
#
# unittest, not pytest: pytest is installed neither here nor in CI. Discovery
# collects TestCase classes only, so the count guard catches a suite that
# silently collects nothing. CI runs the same step (audit.yml).

STAGED=$(git diff --cached --name-only --diff-filter=ACM)
echo "$STAGED" | grep -qE '^tools/audit/(check_lift_hazards\.py|tests/)' || exit 0

ROOT="$(git rev-parse --show-toplevel)"
OUTPUT=$(cd "$ROOT" && python3 -m unittest discover -s tools/audit/tests -p 'test_*.py' 2>&1)
RC=$?
RAN=$(echo "$OUTPUT" | sed -n 's/^Ran \([0-9]\+\) test.*/\1/p' | tail -1)

if [ $RC -ne 0 ]; then
    echo "$OUTPUT"
    echo "pre-commit: hazard-lint self-tests FAILED" >&2
    exit 1
fi

if [ -z "$RAN" ] || [ "$RAN" -lt 6 ]; then
    echo "$OUTPUT"
    echo "pre-commit: expected at least 6 hazard-lint tests, discovery ran ${RAN:-0}" >&2
    exit 1
fi

echo "pre-commit: hazard-lint self-tests OK ($RAN tests)"
exit 0
