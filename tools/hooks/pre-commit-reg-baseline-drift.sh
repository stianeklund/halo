#!/usr/bin/env bash
# Pre-commit hook: detect drift between kb.json @<reg> annotations and
# tools/kb_reg_baseline.json. Runs only when the staged diff touches kb.json
# or the baseline itself, so unrelated commits never get blocked by upstream
# drift introduced elsewhere.

STAGED=$(git diff --cached --name-only)
case "$STAGED" in
    *kb.json*|*kb_reg_baseline.json*) ;;
    *) exit 0 ;;
esac

python3 tools/audit/extract_reg_args.py --check >/tmp/reg-drift-stdout.txt 2>&1
RC=$?
if [ $RC -ne 0 ]; then
    echo ""
    echo "ERROR: kb_reg_baseline drift detected."
    cat /tmp/reg-drift-stdout.txt
    echo ""
    echo "Run: tools/audit/extract_reg_args.py --apply  (then stage kb_reg_baseline.json)"
    echo "Or:  git commit --no-verify  (emergency bypass)"
    exit 1
fi

exit 0
