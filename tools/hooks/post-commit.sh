#!/usr/bin/env bash
# Regenerate CI status dashboard and refresh CodeGraph in background after every commit.
ROOT="$(git rev-parse --show-toplevel)"

LOG="/tmp/dashboard_regen.log"
(
    cd "$ROOT"
    tools/report/update_dashboard.sh >>"$LOG" 2>&1
    if command -v codegraph >/dev/null 2>&1; then
        codegraph sync >/dev/null 2>&1
    fi
) &
disown

echo "  [dashboard] Regenerating in background -> $LOG"

# Retrieval index refresh (arch review 2026-07-07, item #6).
# main-only + single-writer: DuckDB is single-writer and linked worktrees must not
# race the index, so refresh only on main (the canonical worktree) and behind a
# non-blocking flock. Skips unless the commit actually touched kb.json or src/
# (the extract inputs). Runs detached so it never delays the commit; embed is
# incremental (re-embeds only changed rows).
BRANCH="$(git rev-parse --abbrev-ref HEAD 2>/dev/null)"
if [ "$BRANCH" = "main" ]; then
    CHANGED="$(git diff-tree --no-commit-id --name-only -r HEAD 2>/dev/null)"
    if printf '%s\n' "$CHANGED" | grep -qE '^(kb\.json|src/)'; then
        RLOG="/tmp/retrieval_refresh_auto.log"
        PY="$ROOT/.venv/bin/python3"; [ -x "$PY" ] || PY=python3
        (
            exec 9>/tmp/halo_retrieval_refresh.lock
            if flock -n 9; then
                cd "$ROOT"
                echo "=== $(date -Is) post-commit refresh @ $(git rev-parse --short HEAD) ===" >>"$RLOG"
                "$PY" tools/retrieval/build_index.py extract  >>"$RLOG" 2>&1 && \
                "$PY" tools/retrieval/build_index.py outcomes >>"$RLOG" 2>&1 && \
                "$PY" tools/retrieval/build_index.py embed    >>"$RLOG" 2>&1 && \
                "$PY" tools/retrieval/build_index.py stats    >>"$RLOG" 2>&1
                echo "=== $(date -Is) refresh done (exit chain above) ===" >>"$RLOG"
            else
                echo "=== $(date -Is) refresh skipped (another refresh holds the lock) ===" >>"$RLOG"
            fi
        ) &
        disown
        echo "  [retrieval] index refresh queued in background -> $RLOG"
    fi
fi

exit 0
