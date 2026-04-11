#!/usr/bin/env bash
# xemu.sh — launch xemu with the project ISO, with optional diagnostic flags.
#
# xemu 0.8.x reads machine config (bios, hdd, eeprom, avpack, etc.) from its
# xemu.toml and merges it with any CLI args. This script updates only the
# dvd_path in xemu.toml for the requested ISO and passes diagnostic flags on
# the CLI — so machine config should be set once via the xemu GUI and left
# alone.
#
# Paths come from tools/xemu.env (gitignored). Copy tools/xemu.env.example
# to tools/xemu.env on first use.
#
# Usage:
#   tools/xemu.sh                         # boot halo-patched/halo-patched.iso
#   tools/xemu.sh path/to/other.iso       # boot a specific ISO
#   tools/xemu.sh -T                      # enable CPU trace to stderr
#   tools/xemu.sh -g                      # enable gdb stub on :1234
#   tools/xemu.sh -m                      # enable QEMU monitor on :4444
#   tools/xemu.sh -T -l /tmp/xemu.log     # trace, tee combined output to file
#
# Flags may be combined. Unknown positional arg is treated as the ISO path
# (accepts /mnt/X/... WSL paths, Windows paths, or repo-relative).
#
# NOTE: xemu on Windows is a GUI app and always opens a window — there is no
# true headless mode. `-T` enables QEMU's `-d int,cpu_reset,guest_errors,unimp`
# and `-serial stdio`, so the trace output appears in your terminal, but the
# xemu window still shows up. The log file (`-l`) captures both.
#
# QEMU monitor (-m): once running, talk to it with `nc localhost 4444`
# (or from Windows side: `telnet localhost 4444`). Useful commands:
#   info registers      # dump CPU state
#   x /64wx 0xADDR      # hex-dump 64 words at ADDR
#   info mem            # memory mappings
#   q                   # quit xemu

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
ENV_FILE="$SCRIPT_DIR/xemu.env"

if [[ ! -f "$ENV_FILE" ]]; then
  echo "error: $ENV_FILE not found." >&2
  echo "copy $SCRIPT_DIR/xemu.env.example to $ENV_FILE and edit it." >&2
  exit 1
fi

# shellcheck source=/dev/null
source "$ENV_FILE"

: "${XEMU_BIN:?set XEMU_BIN in $ENV_FILE (path to xemu.exe or xemu binary)}"
: "${XEMU_TOML:?set XEMU_TOML in $ENV_FILE (path to xemu.toml)}"

trace=0
gdb=0
monitor=0
iso=""
log_file=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    -T|--trace)    trace=1; shift ;;
    -g|--gdb)      gdb=1; shift ;;
    -m|--monitor)  monitor=1; shift ;;
    -l|--log)      log_file="$2"; shift 2 ;;
    -h|--help)
      sed -n '3,/^$/p' "$0" | sed 's/^# \?//'
      exit 0
      ;;
    -*) echo "unknown flag: $1" >&2; exit 1 ;;
    *)  iso="$1"; shift ;;
  esac
done

if [[ -z "$iso" ]]; then
  iso="$REPO_ROOT/halo-patched/halo-patched.iso"
fi

# Translate a WSL /mnt/X/... path into a Windows-style X:/... path. Leaves
# non-WSL paths untouched. xemu on Windows accepts both forward and back
# slashes, so we normalize to forward slashes for simpler sed escaping.
to_win_path() {
  local p="$1"
  if [[ "$p" == /mnt/?/* ]]; then
    local drive="${p:5:1}"
    local rest="${p:6}"
    printf '%s:%s\n' "$(echo "$drive" | tr 'a-z' 'A-Z')" "$rest"
  else
    printf '%s\n' "$p"
  fi
}

win_iso=$(to_win_path "$iso")

if [[ ! -f "$XEMU_TOML" ]]; then
  echo "error: $XEMU_TOML not found. Launch xemu GUI once to create it." >&2
  exit 1
fi

if ! grep -q '^dvd_path' "$XEMU_TOML"; then
  echo "error: no dvd_path line in $XEMU_TOML. Load any ISO via the xemu GUI once to seed it." >&2
  exit 1
fi

# Rewrite dvd_path. Use '|' as sed delimiter so path slashes don't escape.
sed -i "s|^dvd_path = .*|dvd_path = '${win_iso//|/\\|}'|" "$XEMU_TOML"

args=()
if [[ $trace -eq 1 ]]; then
  args+=(-serial stdio -d int,cpu_reset,guest_errors,unimp)
fi
if [[ $gdb -eq 1 ]]; then
  args+=(-s)
fi
if [[ $monitor -eq 1 ]]; then
  args+=(-monitor "tcp:127.0.0.1:4444,server,nowait")
fi

echo "xemu: $XEMU_BIN" >&2
echo "iso:  $win_iso" >&2
echo "mode: trace=$trace gdb=$gdb monitor=$monitor" >&2

if [[ -n "$log_file" ]]; then
  "$XEMU_BIN" "${args[@]}" 2>&1 | tee "$log_file"
else
  "$XEMU_BIN" "${args[@]}"
fi
