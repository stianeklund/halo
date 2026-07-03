#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ENV_FILE="$SCRIPT_DIR/xemu.env"

if [[ -f "$ENV_FILE" ]]; then
  # shellcheck source=/dev/null
  source "$ENV_FILE"
fi

if [[ -n "${XEMU_BIN:-}" && -z "${XEMU_PATH:-}" ]]; then
  export XEMU_PATH="$XEMU_BIN"
fi

export XEMU_QMP_HOST="${XEMU_QMP_HOST:-localhost}"
export XEMU_QMP_PORT="${XEMU_QMP_PORT:-4444}"
export XEMU_AUTO_LAUNCH="${XEMU_AUTO_LAUNCH:-true}"

exec node /mnt/g/dev/xemu_mcp/dist/index.js
