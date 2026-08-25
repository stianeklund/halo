#!/usr/bin/env bash
# Pre-commit hook: kb.json object names must stay unique.
#
# A duplicate name is silently destructive, not loudly wrong: batch_delink.py
# derives the export path from the name, so two objects sharing one write the
# same file and the later one in the kb array overwrites the other's delinked
# reference — while objdiff.json still points at that path. Several tools also
# build {name: obj} maps where last-wins, so function moves land in the wrong
# object and get written back with no error.
#
# The trap this exists to catch: acting on a proven TU misattribution by
# renaming the object to the real TU, when an object of that name already
# exists. The remedy there is a merge, not a rename.
#
# Bypass: git commit --no-verify

if ! git diff --cached --name-only | grep -qx 'kb.json'; then
    exit 0
fi

ROOT="$(git rev-parse --show-toplevel)"

TMP="$(mktemp)"
trap 'rm -f "$TMP"' EXIT
if ! git show ":kb.json" > "$TMP" 2>/dev/null; then
    echo "kb-object-names: cannot read staged kb.json; skipping." 1>&2
    exit 0
fi

python3 "$ROOT/tools/audit/check_kb_object_names.py" --check --kb "$TMP" 1>&2
RC=$?
if [ $RC -ne 0 ]; then
    echo "" 1>&2
    echo "Commit blocked: kb.json introduces a duplicate object name." 1>&2
    echo "Bypass with: git commit --no-verify" 1>&2
fi
exit $RC
