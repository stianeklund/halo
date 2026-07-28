#!/usr/bin/env bash
# Pre-commit gate: XCALL raw-cast type mismatches vs kb.json decls.
#
# An XCALL function-pointer cast with the wrong return/param type silently
# reads the wrong register (ST0 vs EAX) or truncates a float before pushing
# it. That is how the plasma-pistol overcharge orb stayed invisible for
# weeks (real_a_rgb_color_to_pixel32, 0x99530).
#
# Only ERROR-level (float<->int) mismatches block; the checker exits with
# the ERROR count. WARN-level type-class differences are advisory and are
# printed by the full audit, not here.
#
# Runs only on staged .c files, so it costs nothing on kb.json-only commits.

REPO_ROOT="$(git rev-parse --show-toplevel)"

# NOTE: this reads the worktree copy of each staged file, not the staged
# blob. A partially-staged file is therefore judged by its worktree text.
mapfile -t STAGED_C < <(git diff --cached --name-only --diff-filter=ACMR | grep '\.c$')
[ ${#STAGED_C[@]} -eq 0 ] && exit 0

python3 "$REPO_ROOT/tools/audit/check_xcall_types.py" "${STAGED_C[@]}" >/tmp/xcall-types-check.txt 2>&1
RC=$?
if [ $RC -ne 0 ]; then
    echo ""
    echo "pre-commit BLOCKED: $RC XCALL type mismatch(es) at ERROR level."
    grep "ERROR" /tmp/xcall-types-check.txt | head -20
    echo ""
    echo "A float<->int mismatch reads the wrong register (ST0 vs EAX) or"
    echo "truncates the arg. Fix the XCALL cast to match the kb.json decl."
    echo "Bypass: git commit --no-verify (emergency only)."
fi
exit $RC
