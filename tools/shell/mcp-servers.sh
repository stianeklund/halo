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

BRIDGE_PY="C:/Users/stian/Documents/ghidra-mcp-bridge.py"
LIVE_PY="$REPO_DIR/tools/ghidra_live_mcp/server.py"

PYTHON_WIN="/mnt/c/Users/stian/scoop/shims/python3.exe"
PYTHON_VENV="$REPO_DIR/.venv/bin/python3"

cleanup() {
    echo "Stopping MCP servers..."
    kill "$PID_BRIDGE" "$PID_LIVE" "$PID_RETRIEVAL" 2>/dev/null || true
    wait "$PID_BRIDGE" "$PID_LIVE" "$PID_RETRIEVAL" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

# Kill any stale MCP server processes before starting
pkill -f "ghidra-mcp-bridge.py.*--port=8090" 2>/dev/null || true
pkill -f "ghidra_live_mcp/server.py.*--port=8091" 2>/dev/null || true
pkill -f "tools/retrieval/server.py" 2>/dev/null || true
sleep 1

echo "[mcp-servers] Starting retrieval embedding server ..."
"$PYTHON_VENV" "$REPO_DIR/tools/retrieval/server.py" > /tmp/retrieval_server.log 2>&1 &
PID_RETRIEVAL=$!

echo "[mcp-servers] Starting ghidra bridge on :8090 ..."
"$PYTHON_WIN" "$BRIDGE_PY" --port=8090 &
PID_BRIDGE=$!

echo "[mcp-servers] Starting ghidra-live on :8091 ..."
"$PYTHON_VENV" "$LIVE_PY" --port=8091 &
PID_LIVE=$!

echo "[mcp-servers] All servers running. Retrieval log: /tmp/retrieval_server.log. Ctrl-C to stop."
wait
