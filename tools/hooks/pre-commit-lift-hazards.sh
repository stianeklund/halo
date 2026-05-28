#!/usr/bin/env bash
# Pre-commit hook: run lift hazard scanner on staged .c files.
# Blocks on intrinsics, undersized buffers, callee output size, and x87 math.

STAGED=$(git diff --cached --name-only)

HAS_C=$(echo "$STAGED" | grep -E '^src/.*\.c$')

if [ -z "$HAS_C" ]; then
    exit 0
fi

python3 tools/audit/check_lift_hazards.py --changed-only --cached -q 2>/tmp/lift-hazards-stderr.txt
RC=$?

WARNINGS=$(cat /tmp/lift-hazards-stderr.txt 2>/dev/null)
if [ -n "$WARNINGS" ]; then
    # Check for x87 math warnings specifically
    X87=$(echo "$WARNINGS" | grep -oP 'x87_math: \K[0-9]+')
    if [ -n "$X87" ] && [ "$X87" -gt 0 ]; then
        echo ""
        echo "WARNING: $X87 x87 math hazard(s) in staged files."
        echo "cos()/sin()/fmod() compile to wrong x87 instructions under clang -mno-sse."
        echo "Use x87_fcos_mul()/x87_fsin_mul()/x87_fmod() helpers instead."
        echo "Run: python3 tools/audit/check_lift_hazards.py --changed-only --cached"
        echo ""
    fi
fi

if [ $RC -ne 0 ]; then
    echo "ERROR: lift hazard scan failed for staged changes."
    echo "Run: python3 tools/audit/check_lift_hazards.py --changed-only --cached"
    echo "Fix the issue, or use: git commit --no-verify"
    exit 1
fi

exit 0
