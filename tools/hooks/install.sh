#!/usr/bin/env bash
# Idempotent git-hook installer for this repo.
#
# Keeps core.hooksPath = .git/hooks (the current, working setup). Git only
# auto-populates .sample hooks, so the tracked hooks under tools/hooks/ have to
# be wired in explicitly — in particular `prepare-commit-msg`, which git never
# installs on its own, so the auto-lift commit-message hook silently never fires.
#
# It also reconciles the rtk-trust post-checkout logic with codegraph's managed
# post-checkout/post-merge files: codegraph owns those as regular files, so
# rather than clobber them we append a guarded rtk-trust block. Both run.
#
# Usage:
#   tools/hooks/install.sh          install/repair (idempotent; backs up first)
#   tools/hooks/install.sh --check  report drift only; exit 1 on drift; no mutation
set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel)"
HOOKS_DIR="$(git rev-parse --git-path hooks)"
case "$HOOKS_DIR" in
    /*) : ;;
    *)  HOOKS_DIR="$REPO_ROOT/$HOOKS_DIR" ;;
esac

CHECK_ONLY=0
[ "${1:-}" = "--check" ] && CHECK_ONLY=1

# Hook name -> target basename under tools/hooks/. The symlink value is relative
# (../../tools/hooks/<file>) so it resolves correctly from .git/hooks/.
SYMLINK_NAMES="pre-commit post-commit prepare-commit-msg"
symlink_target() {
    case "$1" in
        pre-commit)         echo "../../tools/hooks/pre-commit.sh" ;;
        post-commit)        echo "../../tools/hooks/post-commit.sh" ;;
        prepare-commit-msg) echo "../../tools/hooks/prepare-commit-msg-hook.sh" ;;
    esac
}

RTK_MARKER_BEGIN="# >>> rtk-trust hook (managed by tools/hooks/install.sh) >>>"
RTK_MARKER_END="# <<< rtk-trust hook <<<"

report() { echo "  [hooks] $*"; }

# ------------------------------------------------------------------ drift scan
drift=0
for name in $SYMLINK_NAMES; do
    want="$(symlink_target "$name")"
    path="$HOOKS_DIR/$name"
    if [ -L "$path" ] && [ "$(readlink "$path")" = "$want" ]; then
        continue
    fi
    drift=1
    [ "$CHECK_ONLY" = 1 ] && report "DRIFT: $name is not a symlink -> $want"
done
if [ ! -f "$HOOKS_DIR/post-checkout" ] || ! grep -qF "$RTK_MARKER_BEGIN" "$HOOKS_DIR/post-checkout"; then
    drift=1
    [ "$CHECK_ONLY" = 1 ] && report "DRIFT: post-checkout missing rtk-trust block"
fi

if [ "$CHECK_ONLY" = 1 ]; then
    if [ "$drift" = 1 ]; then
        report "hook drift detected — run: tools/hooks/install.sh"
        exit 1
    fi
    exit 0    # silent on success (keeps SessionStart quiet)
fi

# ------------------------------------------------------------------ install
# Back up whatever is currently installed, once, before touching anything.
BACKUP_DIR="$HOOKS_DIR/.backup-$(date +%Y%m%d-%H%M%S)"
backed_up=0
for name in pre-commit post-commit post-checkout post-merge prepare-commit-msg; do
    src="$HOOKS_DIR/$name"
    if [ -e "$src" ] || [ -L "$src" ]; then
        [ "$backed_up" = 0 ] && mkdir -p "$BACKUP_DIR" && backed_up=1
        cp -Pp "$src" "$BACKUP_DIR/$name"
    fi
done
[ "$backed_up" = 1 ] && report "backed up existing hooks -> ${BACKUP_DIR#"$REPO_ROOT"/}"

# Ensure the three symlinks.
for name in $SYMLINK_NAMES; do
    want="$(symlink_target "$name")"
    path="$HOOKS_DIR/$name"
    if [ -L "$path" ] && [ "$(readlink "$path")" = "$want" ]; then
        continue
    fi
    rm -f "$path"
    ln -s "$want" "$path"
    report "linked $name -> $want"
done

# Reconcile post-checkout: keep codegraph's managed block, append rtk-trust once.
PCO="$HOOKS_DIR/post-checkout"
if [ ! -f "$PCO" ]; then
    printf '#!/bin/sh\n' > "$PCO"
    chmod +x "$PCO"
fi
if ! grep -qF "$RTK_MARKER_BEGIN" "$PCO"; then
    {
        echo ""
        echo "$RTK_MARKER_BEGIN"
        echo '# Auto-trust RTK project filters so generated commit messages are clean.'
        echo 'RTK_REPO_ROOT="$(git rev-parse --show-toplevel 2>/dev/null)"'
        echo 'if [ -n "$RTK_REPO_ROOT" ] && [ -f "$RTK_REPO_ROOT/.rtk/filters.toml" ] && command -v rtk >/dev/null 2>&1; then'
        echo '    rtk trust >/dev/null 2>&1 || true'
        echo 'fi'
        echo "$RTK_MARKER_END"
    } >> "$PCO"
    report "appended rtk-trust block to post-checkout (codegraph block preserved)"
fi

report "install complete"
exit 0
