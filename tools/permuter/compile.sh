#!/usr/bin/env bash
# compile.sh — VC71/MSVC compile adapter for decomp-permuter
#
# Called by permuter as: ./compile.sh <input.c> -o <output.o>
#
# Compiles a single .c file using Visual C++ 7.1 (CL.Exe) through WSL path
# translation, then writes the COFF output expected by the repo's LLVM/COFF
# scorer path.
#
# Key constraint: VC71 CL.Exe runs as a Windows process and can only write
# to Windows-accessible paths (i.e. drive-mapped paths like G:\..., not /tmp).
# We use REPO_ROOT/build/vc71 as the intermediate COFF staging area.
# The caller (run.py) must set TMPDIR to a Windows-accessible path so that
# Python's tempfile module also creates .c files on a mounted drive.
#
# REPO_ROOT must be set explicitly (or auto-detected via this script's real path,
# not the symlink location). run.py always sets REPO_ROOT.

set -euo pipefail

# --------------------------------------------------------------------------
# Locate repo root via the real path of this script (not the symlink)
# This is critical when the script is symlinked into a permuter work dir.
# --------------------------------------------------------------------------
REAL_SCRIPT="$(readlink -f "${BASH_SOURCE[0]}")"
SCRIPT_DIR="$(cd "$(dirname "$REAL_SCRIPT")" && pwd)"
# REPO_ROOT may be overridden by the environment; otherwise auto-detect
REPO_ROOT="${REPO_ROOT:-$(cd "$SCRIPT_DIR/../.." && pwd)}"
VC71_STAGE="${REPO_ROOT}/build/vc71"
mkdir -p "$VC71_STAGE"

# --------------------------------------------------------------------------
# VC71 toolchain paths
# --------------------------------------------------------------------------
VC71_CL_WSL="${VC71_CL_WSL:-/mnt/c/Program Files (x86)/RXDK/xbox/bin/vc71/CL.Exe}"
RXDK_INC='C:\Program Files (x86)\RXDK\xbox\include'

# --------------------------------------------------------------------------
# Parse arguments: <input.c> -o <output.o>
# --------------------------------------------------------------------------
C_FILE="$1"
shift
if [[ "$1" != "-o" ]]; then
    echo "Usage: compile.sh <input.c> -o <output.o>" >&2
    exit 1
fi
O_FILE="$2"

# --------------------------------------------------------------------------
# WSL→Windows path conversion helper
# Must only be called on paths that are under a /mnt/<drive>/ mount point.
# --------------------------------------------------------------------------
wsl_to_win() {
    local path
    path="$(realpath -m "$1" 2>/dev/null || echo "$1")"
    if [[ "$path" == /mnt/* ]]; then
        local drive="${path:5:1}"
        local rest="${path:7}"
        printf '%s:\\%s' "${drive^^}" "${rest//\//\\}"
    else
        echo "ERROR: path '$path' is not on a Windows-accessible drive mount" >&2
        exit 1
    fi
}

# --------------------------------------------------------------------------
# Generate a unique COFF filename in the Windows-accessible staging dir
# This avoids the /tmp path which Windows CL.Exe cannot write to.
# --------------------------------------------------------------------------
BASENAME="$(basename "${C_FILE%.c}")"
UNIQUE="$$_${RANDOM}"
COFF_TMP="${VC71_STAGE}/${BASENAME}_${UNIQUE}.obj"

cleanup() { rm -f "$COFF_TMP"; }
trap cleanup EXIT

C_WIN="$(wsl_to_win "$C_FILE")"
COFF_WIN="$(wsl_to_win "$COFF_TMP")"
FI_WIN="$(wsl_to_win "$REPO_ROOT/src/xdk_common.h")"
GEN_INC="$(wsl_to_win "$REPO_ROOT/build/generated")"
SRC_INC="$(wsl_to_win "$REPO_ROOT/src")"

# --------------------------------------------------------------------------
# Invoke CL.Exe — same flags as vc71_verify.py:compile_vc71()
# --------------------------------------------------------------------------
"$VC71_CL_WSL" \
    /nologo /c /TC \
    /O2 /Oy- /GF /Gy /Gd \
    /W0 /Zl /X \
    /DMSVC /DXDK_BUILD /DHDATA= \
    "/FI${FI_WIN}" \
    "/I${GEN_INC}" \
    "/I${SRC_INC}" \
    "/I${RXDK_INC}" \
    "/Fo${COFF_WIN}" \
    "$C_WIN" \
    >/dev/null 2>&1

if [[ ! -s "$COFF_TMP" ]]; then
    echo "compile.sh: VC71 produced no output for $C_FILE" >&2
    exit 1
fi

# --------------------------------------------------------------------------
# Return the COFF object directly; decomp-permuter's objdump layer is patched
# in this checkout to score COFF i386 with llvm-objdump.
# --------------------------------------------------------------------------
cp "$COFF_TMP" "$O_FILE"
