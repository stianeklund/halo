#!/usr/bin/env bash
# Composite pre-commit hook — runs all checks in tools/hooks/pre-commit-*.sh
#
# This script is invoked via the symlink .git/hooks/pre-commit, so $0 (and
# BASH_SOURCE) point at .git/hooks — NOT at this file's real location. Deriving
# DIR from $0 therefore globbed .git/hooks/pre-commit-*.sh (zero matches) and
# silently ran NONE of the gates. Resolve tools/hooks from the repo root instead.

DIR="$(git rev-parse --show-toplevel)/tools/hooks"

for hook in "$DIR"/pre-commit-*.sh; do
    [ -x "$hook" ] || continue
    "$hook"
    RC=$?
    if [ $RC -ne 0 ]; then
        exit $RC
    fi
done

exit 0
