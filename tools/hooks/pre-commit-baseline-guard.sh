#!/usr/bin/env bash
# Pre-commit hook: prevent removal or modification of existing entries
# in tools/kb_reg_baseline.json.  New entries (additions) are allowed.
#
# Install:  ln -sf ../../tools/hooks/pre-commit-baseline-guard.sh .git/hooks/pre-commit

BASELINE="tools/kb_reg_baseline.json"

# Only run if the baseline is staged for commit.
if ! git diff --cached --name-only | grep -qx "$BASELINE"; then
    exit 0
fi

# Get the committed version.  If the file is new, nothing to protect.
OLD=$(git show HEAD:"$BASELINE" 2>/dev/null) || exit 0

# Get the staged version.
NEW=$(git show :"$BASELINE")

# Extract the "functions" object keys and values from each version.
# Output: "0xaddr<TAB>declaration" per line, sorted.
extract() {
    python3 -c "
import json, sys
data = json.load(sys.stdin)
for addr, decl in sorted(data.get('functions', {}).items(),
                         key=lambda kv: int(kv[0], 16)):
    print(f'{addr}\t{decl}')
"
}

OLD_ENTRIES=$(echo "$OLD" | extract)
NEW_ENTRIES=$(echo "$NEW" | extract)

# Check that every old entry is present and unchanged in the new version.
REMOVED=()
CHANGED=()

while IFS=$'\t' read -r addr decl; do
    new_decl=$(echo "$NEW_ENTRIES" | awk -F'\t' -v a="$addr" '$1 == a {print $2}')
    if [ -z "$new_decl" ]; then
        REMOVED+=("  $addr  $decl")
    elif [ "$new_decl" != "$decl" ]; then
        CHANGED+=("  $addr")
        CHANGED+=("    old: $decl")
        CHANGED+=("    new: $new_decl")
    fi
done <<< "$OLD_ENTRIES"

if [ ${#REMOVED[@]} -gt 0 ] || [ ${#CHANGED[@]} -gt 0 ]; then
    echo "ERROR: kb_reg_baseline.json entries are protected."
    echo ""
    echo "Existing @<reg> baseline entries cannot be removed or changed."
    echo "Only additions of new entries are allowed."
    if [ ${#REMOVED[@]} -gt 0 ]; then
        echo ""
        echo "REMOVED entries:"
        printf '%s\n' "${REMOVED[@]}"
    fi
    if [ ${#CHANGED[@]} -gt 0 ]; then
        echo ""
        echo "CHANGED entries:"
        printf '%s\n' "${CHANGED[@]}"
    fi
    echo ""
    echo "If this is a deliberate policy change, use: git commit --no-verify"
    exit 1
fi
