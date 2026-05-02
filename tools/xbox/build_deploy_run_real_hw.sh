#!/usr/bin/env bash
set -euo pipefail

exec bash tools/xbox/build_deploy_run.sh --xbox "${XBOX_HOST:-10.0.0.29}" -- "$@"
