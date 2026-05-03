#!/usr/bin/env bash
set -euo pipefail

xbox_host="${XBOX_HOST:-127.0.0.1}"
debug_args=()

while (($#)); do
  case "$1" in
    --xbox)
      if (($# < 2)); then
        printf 'error: --xbox requires a host or IP\n' >&2
        exit 2
      fi
      xbox_host="$2"
      shift 2
      ;;
    --)
      shift
      debug_args+=("$@")
      break
      ;;
    *)
      debug_args+=("$1")
      shift
      ;;
  esac
done

python3 tools/xbox/xbdm_debug_txt.py --host "$xbox_host" "${debug_args[@]}"
