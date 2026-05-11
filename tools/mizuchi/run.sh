#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
MIZUCHI_DIR="${MIZUCHI_DIR:-/mnt/g/dev/mizuchi}"

mkdir -p "$ROOT_DIR/build/vc71/permuter_tmp" "$ROOT_DIR/artifacts/mizuchi/prompts"

export TMPDIR="$ROOT_DIR/build/vc71/permuter_tmp"

exec env -u CLAUDECODE node "$MIZUCHI_DIR/dist/cli.js" "$@"
