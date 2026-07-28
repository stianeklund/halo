#!/usr/bin/env bash
# PreToolUse hook: enforce the CLAUDE.md mandate that a Ghidra MCP preflight runs
# before any mcp__ghidra / mcp__ghidra-live tool call.
#
# Claude Code blocks a tool call only when a PreToolUse hook exits with code 2, so
# we map a failed preflight to exit 2 and print the reason on stderr (which is fed
# back to the model). A healthy preflight exits 0 and the tool proceeds.
#
# Probing the SSE endpoints on *every* ghidra call would add real latency, so a
# successful check is cached for CACHE_TTL seconds — the network probe only runs
# on the first ghidra call (or after the cache goes stale / a prior failure).
set -u

CACHE_TTL=60
CACHE_FILE="/tmp/halo_ghidra_preflight_ok"
ROOT="$(git rev-parse --show-toplevel 2>/dev/null || echo /mnt/g/dev/halo)"

# Drain stdin (hook payload) so the writer never blocks on a full pipe.
cat >/dev/null 2>&1 || true

# Fresh success cached? Skip the network probe.
if [ -f "$CACHE_FILE" ]; then
  now=$(date +%s)
  mtime=$(stat -c %Y "$CACHE_FILE" 2>/dev/null || echo 0)
  if [ $(( now - mtime )) -lt "$CACHE_TTL" ]; then
    exit 0
  fi
fi

# Fail fast: no startup-wait retry loop, short per-endpoint timeout.
if rtk python3 "$ROOT/tools/audit/check_ghidra_mcp.py" --timeout 2 --startup-wait 0 >/tmp/halo_ghidra_preflight.out 2>&1; then
  : > "$CACHE_FILE"
  exit 0
fi

rm -f "$CACHE_FILE"
echo "[ghidra-preflight] Ghidra MCP endpoints are not reachable. $(cat /tmp/halo_ghidra_preflight.out 2>/dev/null)" >&2
echo "[ghidra-preflight] Start them (tools/shell/mcp-servers.sh) and confirm Ghidra is running before retrying this tool." >&2
exit 2
