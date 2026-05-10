#!/usr/bin/env bash
# Pre-commit hook: surface active port deactivations whenever kb.json is staged.
#
# `ported: false` on a function with a source impl is a real switch — patch.py
# skips the redirect at the original address and JMPs from our impl to original.
# Useful for bisection, but easy to forget when committing other work. This
# hook prints a warning so an accidental commit of a deactivation is visible.
# Does not block — use `--check` mode in CI when you want to gate releases.

if ! git diff --cached --name-only | grep -qx 'kb.json'; then
    exit 0
fi

ROOT="$(git rev-parse --show-toplevel)"
python3 "$ROOT/tools/audit/check_ported_deactivations.py" --report 1>&2 || true

exit 0
