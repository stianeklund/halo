#!/usr/bin/env bash
set -euo pipefail

xbox_host="${XBOX_HOST:-127.0.0.1}"
build_args=()

log() {
  printf '[build_deploy_run][%s] %s\n' "$(date '+%H:%M:%S')" "$*"
}

reset_terminal_input_modes() {
  # Disable common mouse-tracking modes in case a prior TUI left them on.
  printf '\033[?1000l\033[?1002l\033[?1003l\033[?1006l\033[?1015l' || true
}

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

log "starting (xbox host: ${xbox_host})"
reset_terminal_input_modes
log "terminal mouse-tracking modes disabled"
if ((${#build_args[@]})); then
  log "build args: ${build_args[*]}"
else
  log "build args: <none>"
fi

if [[ ! -d build ]]; then
    log "configuring CMake build directory"
    cmake -Bbuild -S. -DCMAKE_TOOLCHAIN_FILE=toolchains/llvm.cmake
else
    log "using existing build directory"
fi

log "running build"
python3 tools/build/build.py "${build_args[@]}"
log "deploying XBE to Xbox (${xbox_host})"
python3 tools/xbox/deploy_xbox.py --xbox "$xbox_host" --xbe-only
log "done"
