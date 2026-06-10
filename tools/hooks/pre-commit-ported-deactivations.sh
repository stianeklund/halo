#!/usr/bin/env bash
# Pre-commit hook: block commits that introduce new non-allowlisted active
# port deactivations in kb.json.
#
# `ported: false` on a function with a source impl is a real switch — patch.py
# skips the redirect at the original address and JMPs from our impl to original.
# Useful for bisection, but agents have "fixed" bugs by flipping working lifts
# to ported=false and shipping that. This hook blocks such commits unless the
# deactivation is in the allowlist (tools/audit/deactivation_allowlist.json).
#
# Bypass options (choose one):
#   HALO_ALLOW_DEACTIVATIONS=1 git commit   — env-var bypass (print in failure msg)
#   git commit --no-verify                  — skip all pre-commit hooks

if ! git diff --cached --name-only | grep -qx 'kb.json'; then
    exit 0
fi

if [ -n "$HALO_ALLOW_DEACTIVATIONS" ]; then
    echo "INFO: HALO_ALLOW_DEACTIVATIONS set — skipping deactivation gate." 1>&2
    exit 0
fi

ROOT="$(git rev-parse --show-toplevel)"
python3 "$ROOT/tools/audit/check_ported_deactivations.py" --check 1>&2
RC=$?

if [ $RC -ne 0 ]; then
    echo "" 1>&2
    echo "To bypass: HALO_ALLOW_DEACTIVATIONS=1 git commit" 1>&2
    echo "Or:        git commit --no-verify" 1>&2
    exit 1
fi

exit 0
