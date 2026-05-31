#!/usr/bin/env bash
# Manage Ghidra MCP bridge servers with independent start/stop/restart/status.
#
# Usage:
#   ./tools/shell/mcp-servers.sh start   [ghidra|live|retrieval|all]
#   ./tools/shell/mcp-servers.sh stop    [ghidra|live|retrieval|all]
#   ./tools/shell/mcp-servers.sh restart [ghidra|live|retrieval|all]
#   ./tools/shell/mcp-servers.sh status

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(dirname "$(dirname "$SCRIPT_DIR")")"

BRIDGE_PY="$REPO_DIR/tools/ghidra/ghidra_mcp_bridge.py"
LIVE_PY="$REPO_DIR/tools/ghidra_live_mcp/server.py"
RETRIEVAL_PY="$REPO_DIR/tools/retrieval/server.py"
XEMU_PY="$REPO_DIR/tools/xemu_mcp/server.py"

BRIDGE_PY_WIN=""
if command -v wslpath >/dev/null 2>&1; then
    BRIDGE_PY_WIN="$(wslpath -w "$BRIDGE_PY")"
fi

PYTHON_WIN="/mnt/c/Users/stian/scoop/shims/python3.exe"
PYTHON_VENV="$REPO_DIR/.venv/bin/python3"

PID_DIR="/tmp/mcp-servers"
mkdir -p "$PID_DIR"

PORTS=( "ghidra:8090" "live:8091" "retrieval:0" "xemu:4445" )
GHIDRA_PORT=8090
LIVE_PORT=8091
XEMU_PORT=4445

# ---------------------------------------------------------------------------
# PID file helpers
# ---------------------------------------------------------------------------
pid_file() { echo "$PID_DIR/mcp-$1.pid"; }
read_pid() { [ -f "$(pid_file "$1")" ] && cat "$(pid_file "$1")" 2>/dev/null || true; }
write_pid() { echo "$2" > "$(pid_file "$1")"; }
rm_pid()  { rm -f "$(pid_file "$1")"; }

is_running() {
    local pid pfile
    pfile="$(pid_file "$1")"
    [ -f "$pfile" ] || return 1
    pid="$(cat "$pfile" 2>/dev/null)" || return 1
    kill -0 "$pid" 2>/dev/null
}

# ---------------------------------------------------------------------------
# Port killing (handles WSL+Windows)
# ---------------------------------------------------------------------------
kill_port() {
    local port=$1
    if command -v fuser >/dev/null 2>&1; then
        fuser -k "${port}/tcp" 2>/dev/null || true
    fi
    pkill -f ".*--port=${port}" 2>/dev/null || true
    pkill -f ".*:${port}" 2>/dev/null || true
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

# ---------------------------------------------------------------------------
# Start
# ---------------------------------------------------------------------------
start_ghidra() {
    if is_running ghidra; then
        echo "[mcp] ghidra bridge already running (pid=$(read_pid ghidra))"
        return 0
    fi
    echo "[mcp] Starting ghidra bridge on :$GHIDRA_PORT ..."

    kill_port "$GHIDRA_PORT"
    sleep 0.5

    if [ -n "$BRIDGE_PY_WIN" ]; then
        nohup "$PYTHON_WIN" "$BRIDGE_PY_WIN" --port="$GHIDRA_PORT" > /tmp/mcp-ghidra.log 2>&1 &
    else
        nohup "$PYTHON_VENV" "$BRIDGE_PY" --port="$GHIDRA_PORT" > /tmp/mcp-ghidra.log 2>&1 &
    fi
    write_pid ghidra $!
    echo "[mcp] ghidra bridge started (pid=$!)"
}

start_live() {
    if is_running live; then
        echo "[mcp] ghidra-live already running (pid=$(read_pid live))"
        return 0
    fi
    echo "[mcp] Starting ghidra-live on :$LIVE_PORT ..."

    kill_port "$LIVE_PORT"
    sleep 0.5

    nohup "$PYTHON_VENV" "$LIVE_PY" --port="$LIVE_PORT" > /tmp/mcp-live.log 2>&1 &
    write_pid live $!
    echo "[mcp] ghidra-live started (pid=$!)"
}

start_retrieval() {
    if is_running retrieval; then
        echo "[mcp] retrieval already running (pid=$(read_pid retrieval))"
        return 0
    fi
    echo "[mcp] Starting retrieval embedding server ..."

    pkill -f "tools/retrieval/server.py" 2>/dev/null || true
    sleep 0.5

    nohup "$PYTHON_VENV" "$RETRIEVAL_PY" > /tmp/retrieval_server.log 2>&1 &
    write_pid retrieval $!
    echo "[mcp] retrieval started (pid=$!, log=/tmp/retrieval_server.log)"
}

start_xemu() {
    if is_running xemu; then
        echo "[mcp] xemu already running (pid=$(read_pid xemu))"
        return 0
    fi
    echo "[mcp] Starting xemu MCP on :$XEMU_PORT ..."

    kill_port "$XEMU_PORT"
    sleep 0.5

    # Sibling-module imports resolve because running the script by path puts its
    # directory on sys.path[0] (same convention as the live/retrieval servers).
    # xemu-facing env is overridable; defaults match this machine's layout. The
    # daemon now owns these (it spawns xemu), since Claude no longer spawns the MCP.
    XEMU_MCP_HTTP_PORT="$XEMU_PORT" \
    XEMU_PATH="${XEMU_PATH:-/mnt/g/dev/xemu/build/qemu-system-i386}" \
    XEMU_SCREENSHOT_DIR="${XEMU_SCREENSHOT_DIR:-/mnt/g/dev/halo/screenshots}" \
        nohup "$PYTHON_VENV" "$XEMU_PY" > /tmp/mcp-xemu.log 2>&1 &
    write_pid xemu $!
    echo "[mcp] xemu MCP started (pid=$!, log=/tmp/mcp-xemu.log)"
}

start_all() {
    start_retrieval
    start_ghidra
    start_live
    start_xemu
}

# ---------------------------------------------------------------------------
# Stop
# ---------------------------------------------------------------------------
stop_server() {
    local name=$1 pid
    pid="$(read_pid "$name")"

    if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
        echo "[mcp] Stopping $name (pid=$pid) ..."
        kill "$pid" 2>/dev/null || true
        wait "$pid" 2>/dev/null || true
    fi
    rm_pid "$name"

    case "$name" in
        ghidra)    kill_port "$GHIDRA_PORT" ;;
        live)      kill_port "$LIVE_PORT" ;;
        retrieval) pkill -f "tools/retrieval/server.py" 2>/dev/null || true ;;
        xemu)      kill_port "$XEMU_PORT" ;;
    esac
    echo "[mcp] $name stopped."
}

stop_all() {
    stop_server ghidra
    stop_server live
    stop_server retrieval
    stop_server xemu
}

# ---------------------------------------------------------------------------
# Restart
# ---------------------------------------------------------------------------
restart_server() {
    stop_server "$1"
    sleep 1
    "start_$1"
}

restart_all() {
    stop_all
    sleep 1
    start_all
}

# ---------------------------------------------------------------------------
# Status
# ---------------------------------------------------------------------------
check_health() {
    local port=$1 name=$2
    local health
    health="$(curl -sf http://127.0.0.1:${port}/health 2>/dev/null)" || true
    if [ -n "$health" ]; then
        local sessions tools ghidra_up
        sessions="$(echo "$health" | python3 -c "import sys,json; print(json.load(sys.stdin).get('active_sessions','?'))" 2>/dev/null || echo '?')"
        tools="$(echo "$health" | python3 -c "import sys,json; print(json.load(sys.stdin).get('tools','?'))" 2>/dev/null || echo '?')"
        ghidra_up="$(echo "$health" | python3 -c "import sys,json; print(json.load(sys.stdin).get('ghidra','?'))" 2>/dev/null || echo '?')"
        echo "  $name :$port  UP  sessions=$sessions  tools=$tools  ghidra=$ghidra_up"
    else
        local pid
        pid="$(read_pid "$name")"
        if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
            echo "  $name :$port  STARTING (pid=$pid, health not responding)"
        else
            echo "  $name :$port  DOWN"
        fi
    fi
}

status_all() {
    echo "[mcp] Server status:"
    check_health "$GHIDRA_PORT" ghidra
    check_health "$LIVE_PORT" live

    local rpid
    rpid="$(read_pid retrieval)"
    if [ -n "$rpid" ] && kill -0 "$rpid" 2>/dev/null; then
        echo "  retrieval        UP  (pid=$rpid, log=/tmp/retrieval_server.log)"
    else
        echo "  retrieval        DOWN"
    fi
    rm_pid retrieval 2>/dev/null || true  # stale pid cleanup

    local xhealth
    xhealth="$(curl -sf http://127.0.0.1:${XEMU_PORT}/health 2>/dev/null)" || true
    if [ -n "$xhealth" ]; then
        local xconn xrun
        xconn="$(echo "$xhealth" | python3 -c "import sys,json; print(json.load(sys.stdin).get('connected','?'))" 2>/dev/null || echo '?')"
        xrun="$(echo "$xhealth" | python3 -c "import sys,json; print(json.load(sys.stdin).get('xemu_running','?'))" 2>/dev/null || echo '?')"
        echo "  xemu :$XEMU_PORT  UP  qmp_connected=$xconn  xemu_running=$xrun"
    else
        local xpid
        xpid="$(read_pid xemu)"
        if [ -n "$xpid" ] && kill -0 "$xpid" 2>/dev/null; then
            echo "  xemu :$XEMU_PORT  STARTING (pid=$xpid, health not responding)"
        else
            echo "  xemu :$XEMU_PORT  DOWN"
        fi
    fi
}

# ---------------------------------------------------------------------------
# Dispatch
# ---------------------------------------------------------------------------
CMD="${1:-}"
TARGET="${2:-all}"

usage() {
    echo "Usage: $0 {start|stop|restart|status} [ghidra|live|retrieval|all]"
    echo ""
    echo "  start    Start server(s) in background"
    echo "  stop     Stop server(s)"
    echo "  restart  Stop then start server(s)"
    echo "  status   Show server health"
    echo ""
    echo "  Targets: ghidra (port 8090), live (port 8091), retrieval, xemu (port 4445), all"
    exit 1
}

case "$CMD" in
    start)
        case "$TARGET" in
            ghidra)    start_ghidra ;;
            live)      start_live ;;
            retrieval) start_retrieval ;;
            xemu)      start_xemu ;;
            all|"")    start_all ;;
            *)         usage ;;
        esac
        ;;
    stop)
        case "$TARGET" in
            ghidra)    stop_server ghidra ;;
            live)      stop_server live ;;
            retrieval) stop_server retrieval ;;
            xemu)      stop_server xemu ;;
            all|"")    stop_all ;;
            *)         usage ;;
        esac
        ;;
    restart)
        case "$TARGET" in
            ghidra)    restart_server ghidra ;;
            live)      restart_server live ;;
            retrieval) restart_server retrieval ;;
            xemu)      restart_server xemu ;;
            all|"")    restart_all ;;
            *)         usage ;;
        esac
        ;;
    status) status_all ;;
    *)      usage ;;
esac
