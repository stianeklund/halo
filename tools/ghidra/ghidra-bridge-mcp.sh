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
# Also try killing by pattern in case fuser misses WSL/Windows processes
pkill -f "ghidra-mcp-bridge.py.*--port=${PORT}" 2>/dev/null || true
pkill -f "ghidra_mcp_bridge.py.*--port=${PORT}" 2>/dev/null || true

exec "$PYTHON_WIN" "$BRIDGE_PY_WIN" --port="$PORT"
