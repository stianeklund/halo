#!/usr/bin/env bash
# Pre-commit gate: literal constants passed where the original pushes a
# register parameter.
#
# The lift writes e.g. datum_get(player_data, 0) at a site where the original
# does PUSH EAX with EAX holding the function's @<eax> player_handle. The
# callee then operates on player slot 0 for every player. Nothing else catches
# it: no crash, no assert, and VC71 byte-match often scores the WRONG version
# higher (fixing FUN_000acd00 on 2026-07-31 dropped it 85.7% -> 82.1%, because
# push 0 is one instruction and push [ebp+N] can be two, and neither matches
# the original's push ebx).
#
# Four real bugs on 2026-07-31: FUN_000ae920 (scoreboard title showed player
# slot 0's score), FUN_000acd00, FUN_000b39a0, FUN_000adb20.
#
# Runs only on staged .c files (~2s full-repo, far less scoped), so it costs
# nothing on kb.json-only commits. Findings already in
# tools/audit/const_reg_args_baseline.json are forgiven unless their counts
# changed.

REPO_ROOT="$(git rev-parse --show-toplevel)"

mapfile -t STAGED_C < <(git diff --cached --name-only --diff-filter=ACMR | grep '\.c$')
[ ${#STAGED_C[@]} -eq 0 ] && exit 0

python3 "$REPO_ROOT/tools/audit/check_const_reg_args.py" \
    --check --staged-only --no-report
RC=$?
if [ $RC -ne 0 ]; then
    echo ""
    echo "pre-commit BLOCKED: constant-for-register-argument finding(s)."
    echo ""
    echo "Disassemble the original at the reported address and look at what it"
    echo "pushes for that argument. In Ghidra, an implicit register input shows"
    echo "up as an 'in_EAX'-style local in the decompile, and the caller will do"
    echo "MOV <reg>,<param reg> immediately before the CALL."
    echo ""
    echo "Verified false positive (inlined memset, shared jump-table tail"
    echo "block)? Record it, with a reason, via:"
    echo "  python3 tools/audit/check_const_reg_args.py --update-baseline"
    echo "Bypass: git commit --no-verify (emergency only)."
fi
exit $RC
