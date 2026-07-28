#!/usr/bin/env bash
# Pre-commit hook: run lift hazard scanner on staged .c files.
# Blocks on ERROR-level findings (intrinsics, undersized buffers, callee output
# size, concat survival, void-EAX return type mismatches).
# Prints a clearly-labelled non-blocking "REVIEW THESE WARNINGS" block for
# WARN-level findings (duplicate args, pointer-as-float, buffer alias, x87 math,
# float smuggling, addr-value-add, param-loop, discarded result).
#
# Background: a real bug shipped because an fmod/FPREM1 warning in
# hud_messaging.c was ignored as pre-existing noise (2026-06-10). Scoping to
# staged files only makes every warning directly actionable.

STAGED=$(git diff --cached --name-only)

HAS_C=$(echo "$STAGED" | grep -E '^src/.*\.c$')

if [ -z "$HAS_C" ]; then
    exit 0
fi

# Capture stderr (warnings/errors) and stdout (quiet counts) separately.
TMPOUT=$(mktemp)
TMPERR=$(mktemp)

python3 tools/audit/check_lift_hazards.py --staged-only 2>"$TMPERR" >"$TMPOUT"
RC=$?

ERRTEXT=$(cat "$TMPERR")
rm -f "$TMPOUT" "$TMPERR"

if [ -n "$ERRTEXT" ]; then
    # Separate blocking (ERROR:) lines from warning lines.
    ERRORS=$(echo "$ERRTEXT" | grep -E '^ERROR:')
    WARNS=$(echo "$ERRTEXT" | grep -v '^ERROR:')

    if [ -n "$WARNS" ]; then
        echo ""
        echo "========================================================"
        echo "  REVIEW THESE WARNINGS (non-blocking, staged files only)"
        echo "========================================================"
        echo "$WARNS"
        echo "========================================================"
        echo "  These hazards are in files YOU staged. They are not"
        echo "  pre-existing noise. Investigate before pushing."
        echo "  Run: python3 tools/audit/check_lift_hazards.py --staged-only"
        echo "========================================================"
        echo ""
    fi

    if [ -n "$ERRORS" ]; then
        echo ""
        echo "ERROR: lift hazard scan found blocking issues in staged files."
        echo "$ERRORS"
        echo ""
        echo "Run: python3 tools/audit/check_lift_hazards.py --staged-only"
        echo "Fix the issue, or use: git commit --no-verify"
        exit 1
    fi
fi

if [ $RC -ne 0 ]; then
    echo "ERROR: lift hazard scan failed for staged changes."
    echo "Run: python3 tools/audit/check_lift_hazards.py --staged-only"
    echo "Fix the issue, or use: git commit --no-verify"
    exit 1
fi

exit 0
