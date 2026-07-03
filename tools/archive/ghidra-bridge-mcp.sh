#!/usr/bin/env bash
# Start the Ghidra MCP bridge server (SSE on port 8090).
# Kills any stale listener on the port first to survive OpenCode restarts.

set -euo pipefail

PORT=8090
PYTHON_WIN="/mnt/c/Users/stian/scoop/shims/python3.exe"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BRIDGE_PY="$SCRIPT_DIR/ghidra_mcp_bridge.py"
BRIDGE_PY_WIN="$(wslpath -w "$BRIDGE_PY")"

# Kill anything already on this port (handles stale processes from prior sessions)
if command -v fuser >/dev/null 2>&1; then
    fuser -k "${PORT}/tcp" 2>/dev/null || true
fi
pkill -f "ghidra-mcp-bridge.py.*--port=${PORT}" 2>/dev/null || true
pkill -f "ghidra_mcp_bridge.py.*--port=${PORT}" 2>/dev/null || true
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

while true; do
    "$PYTHON_WIN" "$BRIDGE_PY_WIN" --port="$PORT"
    echo "[ghidra-bridge] exited (status $?), restarting in 5s..." >&2
    sleep 5
done
