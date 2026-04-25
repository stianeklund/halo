#!/usr/bin/env bash
# Pre-commit hook: if staged changes touch lift-sensitive files (src/**/*.c + kb.json),
# run generate_lift_commit.py in dry-run mode to gate on ABI audit.
# Skipped for non-lift commits (docs, tools-only, etc).

STAGED=$(git diff --cached --name-only)

HAS_C=$(echo "$STAGED" | grep -E '^src/.*\.c$')
HAS_KB=$(echo "$STAGED" | grep -qx 'kb.json' && echo "yes")

if [ -z "$HAS_C" ] && [ -z "$HAS_KB" ]; then
    exit 0
fi

if [ -z "$HAS_KB" ]; then
    exit 0
fi

echo "lift-audit: checking staged kb.json changes..."
python3 tools/generate_lift_commit.py --batch-name "_preflight" > /dev/null 2>/tmp/lift-audit-stderr.txt
RC=$?
if [ $RC -ne 0 ]; then
    echo ""
    echo "ERROR: ABI audit failed for staged lift changes."
    cat /tmp/lift-audit-stderr.txt
    echo ""
    echo "Fix the issue, or use: git commit --no-verify"
    exit 1
fi

WARNINGS=$(grep -i "warn" /tmp/lift-audit-stderr.txt 2>/dev/null)
if [ -n "$WARNINGS" ]; then
    echo "$WARNINGS"
fi

exit 0
