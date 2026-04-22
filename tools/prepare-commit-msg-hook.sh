#!/bin/sh
# .git/hooks/prepare-commit-msg — auto-generate lift commit messages
# Install: cp tools/prepare-commit-msg-hook.sh .git/hooks/prepare-commit-msg && chmod +x .git/hooks/prepare-commit-msg
#
# When kb.json, kb_meta.json, or src/**/*.c files are staged, this hook
# prepopulates the commit message with the output of generate_lift_commit.py.
# The user (or agent) can then review and confirm.

COMMIT_MSG_FILE=$1
COMMIT_SOURCE=$2

# Skip if amending or merging
if [ "$COMMIT_SOURCE" = "merge" ] || [ "$COMMIT_SOURCE" = "squash" ]; then
    exit 0
fi

# Check if lift-related files are staged
if git diff --cached --name-only | grep -qE '^(kb\.json|kb_meta\.json|src/.*\.c)$'; then
    # Generate the message and prepend it
    if [ -x "$(command -v python3)" ] && [ -f "tools/generate_lift_commit.py" ]; then
        GENERATED=$(python3 tools/generate_lift_commit.py 2>/dev/null)
        if [ -n "$GENERATED" ]; then
            echo "$GENERATED" > "$COMMIT_MSG_FILE"
            echo "" >> "$COMMIT_MSG_FILE"
            echo "# Please review the generated message above." >> "$COMMIT_MSG_FILE"
            echo "# If it looks correct, save and quit." >> "$COMMIT_MSG_FILE"
            echo "# If it is empty or wrong, check that all changes are staged." >> "$COMMIT_MSG_FILE"
        fi
    fi
fi

exit 0
