#!/usr/bin/env bash
# Pre-commit gate: stdcall-decl mismatch sweep (lift-learnings §30).
# kb.json decls are checked against each function's RET immediate in the
# pristine XBE — a cdecl/plain decl over a `RET n` body makes every lifted
# call site drift ESP (the 0x158df0 boot crash). Pre-existing latent
# mismatches live in tools/audit/stdcall_ret_baseline.json; only NEW ones
# block. Runs only when the staged diff touches kb.json or the baseline.
# Skips silently when the XBE is absent (GitHub-hosted CI).

REPO_ROOT="$(git rev-parse --show-toplevel)"

STAGED=$(git diff --cached --name-only)
case "$STAGED" in
    *kb.json*|*stdcall_ret_baseline.json*) ;;
    *) exit 0 ;;
esac

[ -f "$REPO_ROOT/halo-patched/cachebeta.xbe" ] || exit 0

python3 "$REPO_ROOT/tools/audit/check_stdcall_ret.py" --check >/tmp/stdcall-ret-check.txt 2>&1
RC=$?
if [ $RC -ne 0 ]; then
    echo ""
    echo "pre-commit BLOCKED: new stdcall-decl mismatch (kb.json decl vs binary RET)."
    grep -E "^ERROR" /tmp/stdcall-ret-check.txt | grep -v "\[baselined\]" | head -20
    tail -5 /tmp/stdcall-ret-check.txt
    echo ""
    echo "Fix the kb.json decl (__stdcall + correct arg count; lift-learnings §30)."
    echo "Bypass: git commit --no-verify (emergency only)."
fi
exit $RC
