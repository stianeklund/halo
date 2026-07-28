#!/usr/bin/env bash
# Pre-commit gate: kb.json declared stack-arg counts vs the ADD ESP,N cleanup
# observed at each callee's original call sites in the pristine XBE.
#
# A decl that under- or over-declares its stack args makes every lifted call
# site push the wrong number of dwords, drifting ESP. Pre-existing latent
# mismatches live in tools/audit/arg_count_baseline.json; only NEW ones (or
# ones whose observed cleanup CHANGED) block.
#
# Runs only when the staged diff touches kb.json or the baseline -- the
# findings are derived from kb.json decls, so nothing else can move them.
# Full sweep is ~2.5s. Skips silently when the XBE is absent (hosted CI).

REPO_ROOT="$(git rev-parse --show-toplevel)"

STAGED=$(git diff --cached --name-only)
case "$STAGED" in
    *kb.json*|*arg_count_baseline.json*) ;;
    *) exit 0 ;;
esac

[ -f "$REPO_ROOT/halo-patched/cachebeta.xbe" ] || exit 0

python3 "$REPO_ROOT/tools/audit/check_arg_counts.py" --check --no-report \
    >/tmp/arg-counts-check.txt 2>&1
RC=$?
if [ $RC -ne 0 ]; then
    echo ""
    echo "pre-commit BLOCKED: new arg-count mismatch (kb.json decl vs call-site ADD ESP)."
    sed -n '/--check FAILED/,$p' /tmp/arg-counts-check.txt | head -25
    echo ""
    echo "Bypass: git commit --no-verify (emergency only)."
fi
exit $RC
