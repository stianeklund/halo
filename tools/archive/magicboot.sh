#!/usr/bin/env bash
set -euo pipefail

if (($# != 1)); then
  printf 'usage: %s <xbox-ip>\n' "$(basename "$0")" >&2
  exit 2
fi

xbox_host="$1"
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

"${script_dir}/run-xbe.sh" --host "$xbox_host"
