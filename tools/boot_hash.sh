#!/usr/bin/env bash
# boot_hash.sh — boot an ISO in xemu, wait for the cachebeta NV2A spin to
# settle, dump a memory region via the QEMU monitor, and print a sha256 of
# the dump. Used as a deterministic boot oracle for re-implementation
# bisection: a re-implemented function is structurally equivalent to the
# original iff the hash matches the unpatched baseline.
#
# Usage:
#   tools/boot_hash.sh                            # default ISO and region
#   tools/boot_hash.sh path/to/other.iso          # specific ISO
#   tools/boot_hash.sh -a 0x500000 -n 256         # 256 words = 1KB at 0x500000
#   tools/boot_hash.sh -a 0x5aa820 -n 16          # 64 bytes around game_variant_global
#   tools/boot_hash.sh -w 12 path/to/iso          # wait 12s for spin to settle
#   tools/boot_hash.sh -v ...                     # also print the dump
#
# Defaults: addr=0x500000, nwords=256 (1KB), wait_secs=8.
#
# The script kills any orphan xemu before launching its own, and uses the
# monitor's quit command (with a taskkill fallback) to clean up after.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

addr="0x500000"
nwords=256
wait_secs=8
verbose=0
iso=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    -a|--addr)   addr="$2"; shift 2 ;;
    -n|--nwords) nwords="$2"; shift 2 ;;
    -w|--wait)   wait_secs="$2"; shift 2 ;;
    -v|--verbose) verbose=1; shift ;;
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

kill_xemu() {
  cmd.exe /c "taskkill /F /IM xemu.exe 2>NUL" >/dev/null 2>&1 || true
}
trap kill_xemu EXIT

# Make sure we start clean.
kill_xemu

echo "boot_hash: iso=$iso addr=$addr nwords=$nwords wait=${wait_secs}s" >&2

# Launch xemu with monitor in background. Discard its log; we don't need it.
"$SCRIPT_DIR/xemu.sh" -m "$iso" >/dev/null 2>&1 &

# Wait for the QEMU monitor to start listening on 4444.
for _ in $(seq 1 30); do
  if cmd.exe /c "netstat -an | findstr 4444" 2>/dev/null | grep -q LISTENING; then
    break
  fi
  sleep 0.2
done

if ! cmd.exe /c "netstat -an | findstr 4444" 2>/dev/null | grep -q LISTENING; then
  echo "boot_hash: xemu monitor never came up" >&2
  exit 1
fi

# Let the boot run into the spin.
sleep "$wait_secs"

# Dump memory. `x /Nwx ADDR` = N 32-bit words, hex.
dump=$("$SCRIPT_DIR/xemu-mon.py" "x /${nwords}wx $addr")

# Quit xemu cleanly via the monitor (taskkill in trap is the fallback).
"$SCRIPT_DIR/xemu-mon.py" --quit "info status" >/dev/null 2>&1 || true

if [[ $verbose -eq 1 ]]; then
  echo "$dump" >&2
fi

# Hash the dump. The `x` command's address-prefix on each line is stable
# across runs (same addr, same line count), so we don't need to strip it.
hash=$(printf '%s\n' "$dump" | sha256sum | awk '{print $1}')
echo "addr=$addr nwords=$nwords sha256=$hash"
