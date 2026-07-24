#!/usr/bin/env bash
# Pre-commit readability gate (readable-lift Phase 3).
#
#   HARD  -- block if a staged .c file ADDS a raw function-pointer cast to a
#            literal address (((T(*)(A))0xADDR)(...)). These bypass kb.json and
#            the thunk system and hide calling-convention bugs; a raw cast is
#            never necessary (add the callee to kb.json instead).
#   SOFT  -- print, but never block, the FUN_-call / offset-deref findings in
#            touched files (they legitimately grow as new code is lifted; the
#            ratchet in check_readability.py --check locks the wins over time).
#
# Bypass with --no-verify in an emergency.
REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

staged_c="$(git diff --cached --name-only --diff-filter=ACMR | grep '\.c$' || true)"
if [ -z "$staged_c" ]; then
    exit 0
fi

# HARD: newly-added raw fn-ptr casts in the staged diff (added lines only).
# [+] char class = a literal leading '+' portably (ugrep/BRE/ERE all agree); the
# '+++ path' diff header can never match the cast pattern, so no separate exclude.
added_raw="$(git diff --cached --unified=0 -- $staged_c \
    | grep -E '^[+].*\(\(.*\(\*\).*\)0x[0-9a-fA-F]' || true)"

# SOFT advisory (non-blocking): per-file findings across touched files.
python3 "$REPO_ROOT/tools/audit/check_readability.py" --changed-only || true

if [ -n "$added_raw" ]; then
    echo ""
    echo "pre-commit BLOCKED: a staged change adds a raw function-pointer cast:"
    echo "$added_raw" | sed 's/^[+]/    /'
    echo ""
    echo "Add the callee to kb.json with its signature (and @<reg> if register-"
    echo "passed) and call it by name instead. Bypass with --no-verify."
    exit 1
fi
exit 0
