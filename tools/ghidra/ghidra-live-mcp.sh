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

cd "$REPO_DIR/tools/ghidra_live_mcp"
exec "$REPO_DIR/.venv/bin/python3" server.py --port="$PORT"
