#!/usr/bin/env bash
# HALO-HOOK-TRIGGER: ^(kb\.json|src/.*\.[ch])$
# Pre-commit hook: every kb.json "static_in_tu": true flag must be provable.
#
# The flag makes the VC7.1 verifier compile the function file-static (private
# register ABI, no frame; docs/lift-learnings.md 32a). A flag on a function
# whose address is taken, that has a caller outside its TU, or that is called
# before its definition scores a compile the original could not have produced.
# check_static_in_tu.py proves each flag against the pristine XBE and src/.
#
# Bypass: git commit --no-verify

. "$(dirname "${BASH_SOURCE[0]}")/lib-staged.sh"
if ! staged_list all | grep -qE '^(kb\.json|src/.*\.[ch])$'; then
    exit 0
fi

ROOT="$(git rev-parse --show-toplevel)"
if [ ! -f "$ROOT/halo-patched/cachebeta.xbe" ]; then
    echo "static-in-tu: pristine XBE missing; skipping." 1>&2
    exit 0
fi

TMP="$(mktemp)"
trap 'rm -f "$TMP"' EXIT
if ! git show ":kb.json" > "$TMP" 2>/dev/null; then
    echo "static-in-tu: cannot read staged kb.json; skipping." 1>&2
    exit 0
fi

OUT="$(python3 "$ROOT/tools/audit/check_static_in_tu.py" --check --kb "$TMP" 2>&1)"
RC=$?
if [ $RC -ne 0 ]; then
    echo "$OUT" 1>&2
    echo "" 1>&2
    echo "Commit blocked: a static_in_tu flag in kb.json is not provable." 1>&2
    echo "Bypass with: git commit --no-verify" 1>&2
fi
exit $RC
