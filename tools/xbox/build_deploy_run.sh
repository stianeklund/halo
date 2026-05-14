#!/usr/bin/env bash
set -euo pipefail

xbox_host="${XBOX_HOST:-127.0.0.1}"
build_args=()

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
      build_args+=("$@")
      break
      ;;
    *)
      build_args+=("$1")
      shift
      ;;
  esac
done

if [[ ! -d build ]]; then
    cmake -Bbuild -S. -DCMAKE_TOOLCHAIN_FILE=toolchains/llvm.cmake
fi

python3 tools/build/build.py "${build_args[@]}"
python3 tools/xbox/deploy_xbox.py --xbox "$xbox_host" --xbe-only
