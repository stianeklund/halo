#!/usr/bin/env bash
set -euo pipefail

xbox_host="${XBOX_HOST:-${XBDM_HOST:-127.0.0.1}}"
xbox_dest="${XBOX_DEST:-xE:\GAMES\halo-patched}"

while (($#)); do
  case "$1" in
    --xbox|--host)
      if (($# < 2)); then
        printf 'error: %s requires a host or IP\n' "$1" >&2
        exit 2
      fi
      xbox_host="$2"
      shift 2
      ;;
    --dest)
      if (($# < 2)); then
        printf 'error: --dest requires an Xbox path\n' >&2
        exit 2
      fi
      xbox_dest="$2"
      shift 2
      ;;
    --)
      shift
      break
      ;;
    *)
      printf 'error: unknown argument: %s\n' "$1" >&2
      exit 2
      ;;
  esac
done

xbe_path="${xbox_dest#x}\\default.xbe"
python3 tools/xbox/xbdm_rdcp.py --host "$xbox_host" "magicboot title=${xbe_path} debug"
