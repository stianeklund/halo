#!/usr/bin/env bash
# Start Ghidra MCP bridge servers for multi-client access.
# Both servers run as SSE endpoints so multiple Claude/opencode instances can connect.
#
# Usage: ./tools/shell/mcp-servers.sh
#   Starts both ghidra (port 8090) and ghidra-live (port 8091) in background.
#   Ctrl-C stops both.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(dirname "$(dirname "$SCRIPT_DIR")")"

BRIDGE_PY="$REPO_DIR/tools/ghidra/ghidra_mcp_bridge.py"
LIVE_PY="$REPO_DIR/tools/ghidra_live_mcp/server.py"
BRIDGE_PY_WIN=""
if command -v wslpath >/dev/null 2>&1; then
    BRIDGE_PY_WIN="$(wslpath -w "$BRIDGE_PY")"
fi

PYTHON_WIN="/mnt/c/Users/stian/scoop/shims/python3.exe"
PYTHON_VENV="$REPO_DIR/.venv/bin/python3"

PID_BRIDGE=""
PID_LIVE=""
PID_RETRIEVAL=""
CLEANED_UP=0

PORTS=(8090 8091)

kill_port() {
    local port=$1
    # WSL2: fuser can see Windows TCP sockets
    if command -v fuser >/dev/null 2>&1; then
        fuser -k "${port}/tcp" 2>/dev/null || true
    fi
    # Kill WSL-side python processes holding the port
    pkill -f ".*--port=${port}" 2>/dev/null || true
    pkill -f ".*:${port}" 2>/dev/null || true
    # Kill Windows-side processes holding the port
    if [ -f /mnt/c/Windows/System32/netstat.exe ]; then
        while read -r pid; do
            pid="${pid//$'\r'/}"
            [ -n "$pid" ] && /mnt/c/Windows/System32/taskkill.exe /F /PID "$pid" 2>/dev/null || true
        done < <(
            /mnt/c/Windows/System32/netstat.exe -ano 2>/dev/null | \
                awk -v port=":${port}" '$0 ~ port " " { gsub(/\r/, "", $5); print $5 }' | \
                sort -u || true
        )
    fi
}

cleanup() {
    if [ "$CLEANED_UP" -eq 1 ]; then
        return
    fi
    CLEANED_UP=1
    trap - EXIT INT TERM

    echo "Stopping MCP servers..."
    for pid in "$PID_BRIDGE" "$PID_LIVE" "$PID_RETRIEVAL"; do
        [ -n "$pid" ] && kill "$pid" 2>/dev/null || true
        [ -n "$pid" ] && wait "$pid" 2>/dev/null || true
    done
    for port in "${PORTS[@]}"; do
        kill_port "$port"
    done
    echo "MCP servers stopped."
}
trap cleanup EXIT
trap 'cleanup; exit 130' INT
trap 'cleanup; exit 143' TERM

# Kill any stale processes before starting
for port in "${PORTS[@]}"; do
    kill_port "$port"
done
pkill -f "tools/retrieval/server.py" 2>/dev/null || true
sleep 1

echo "[mcp-servers] Starting retrieval embedding server ..."
"$PYTHON_VENV" "$REPO_DIR/tools/retrieval/server.py" > /tmp/retrieval_server.log 2>&1 &
PID_RETRIEVAL=$!

if [ -n "$BRIDGE_PY_WIN" ]; then
    echo "[mcp-servers] Starting ghidra bridge (Windows) on :8090 ..."
    "$PYTHON_WIN" "$BRIDGE_PY_WIN" --port=8090 &
    PID_BRIDGE=$!
elif [ -n "$PYTHON_VENV" ]; then
    echo "[mcp-servers] Starting ghidra bridge (Linux) on :8090 ..."
    "$PYTHON_VENV" "$BRIDGE_PY" --port=8090 &
    PID_BRIDGE=$!
fi

echo "[mcp-servers] Starting ghidra-live on :8091 ..."
"$PYTHON_VENV" "$LIVE_PY" --port=8091 &
PID_LIVE=$!

echo "[mcp-servers] All servers running. Retrieval log: /tmp/retrieval_server.log. Ctrl-C to stop."
wait
