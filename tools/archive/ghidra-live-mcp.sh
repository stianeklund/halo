#!/usr/bin/env bash
# Start the Ghidra live delinker MCP server (SSE on port 8091).
# Kills any stale listener on the port first to survive OpenCode restarts.

set -euo pipefail

PORT=8091
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(dirname "$(dirname "$SCRIPT_DIR")")"

# Kill anything already on this port (handles stale processes from prior sessions)
if command -v fuser >/dev/null 2>&1; then
    fuser -k "${PORT}/tcp" 2>/dev/null || true
fi
pkill -f "server.py.*--port=${PORT}" 2>/dev/null || true
pkill -f ".*--port=${PORT}" 2>/dev/null || true
# Kill Windows-side processes holding the port
if [ -f /mnt/c/Windows/System32/netstat.exe ]; then
    while read -r pid; do
        pid="${pid//$'\r'/}"
        [ -n "$pid" ] && /mnt/c/Windows/System32/taskkill.exe /F /PID "$pid" 2>/dev/null || true
    done < <(
        /mnt/c/Windows/System32/netstat.exe -ano 2>/dev/null | \
            awk -v port=":${PORT}" '$0 ~ port " " { gsub(/\r/, "", $5); print $5 }' | \
            sort -u || true
    )
fi

cd "$REPO_DIR/tools/ghidra_live_mcp"
exec "$REPO_DIR/.venv/bin/python3" server.py --port="$PORT"
