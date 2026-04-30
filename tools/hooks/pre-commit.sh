#!/usr/bin/env bash
# Composite pre-commit hook — runs all checks in tools/hooks/pre-commit-*.sh

DIR="$(cd "$(dirname "$0")" && pwd)"

for hook in "$DIR"/pre-commit-*.sh; do
    [ -x "$hook" ] || continue
    "$hook"
    RC=$?
    if [ $RC -ne 0 ]; then
        exit $RC
    fi
done

exit 0
