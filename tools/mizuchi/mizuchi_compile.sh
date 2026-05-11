#!/usr/bin/env bash
# mizuchi_compile.sh — VC71 compile adapter for mizuchi (no /FI force-include)
#
# Like tools/permuter/compile.sh but omits /FI xdk_common.h.
# mizuchi pre-expands xdk_common.h into the context via mizuchi_context.sh, so
# the preprocessed source file already has all type definitions inlined.
# Passing /FI again would cause duplicate typedef errors in VC71.
#
# Called by mizuchi as: bash mizuchi_compile.sh <preprocessed.c> -o <output.o>

set -euo pipefail

REAL_SCRIPT="$(readlink -f "${BASH_SOURCE[0]}")"
SCRIPT_DIR="$(cd "$(dirname "$REAL_SCRIPT")" && pwd)"
REPO_ROOT="${REPO_ROOT:-$(cd "$SCRIPT_DIR/../.." && pwd)}"
VC71_STAGE="${REPO_ROOT}/build/vc71"
mkdir -p "$VC71_STAGE"

VC71_CL_WSL="${VC71_CL_WSL:-/mnt/c/Program Files (x86)/RXDK/xbox/bin/vc71/CL.Exe}"
RXDK_INC='C:\Program Files (x86)\RXDK\xbox\include'

C_FILE="$1"
shift
if [[ "$1" != "-o" ]]; then
    echo "Usage: mizuchi_compile.sh <input.c> -o <output.o>" >&2
    exit 1
fi
O_FILE="$2"

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

BASENAME="$(basename "${C_FILE%.c}")"
UNIQUE="$$_${RANDOM}"
COFF_TMP="${VC71_STAGE}/${BASENAME}_${UNIQUE}.obj"

cleanup() { rm -f "$COFF_TMP"; }
trap cleanup EXIT

C_WIN="$(wsl_to_win "$C_FILE")"
COFF_WIN="$(wsl_to_win "$COFF_TMP")"
GEN_INC="$(wsl_to_win "$REPO_ROOT/build/generated")"
SRC_INC="$(wsl_to_win "$REPO_ROOT/src")"

# No /FI here — xdk_common.h types are already inlined by mizuchi_context.sh
"$VC71_CL_WSL" \
    /nologo /c /TC \
    /O2 /Oy- /GF /Gy /Gd \
    /W0 /Zl /X \
    /DMSVC /DXDK_BUILD \
    "/I${GEN_INC}" \
    "/I${SRC_INC}" \
    "/I${RXDK_INC}" \
    "/Fo${COFF_WIN}" \
    "$C_WIN" \
    >/dev/null 2>&1

if [[ ! -s "$COFF_TMP" ]]; then
    echo "mizuchi_compile.sh: VC71 produced no output for $C_FILE" >&2
    exit 1
fi

cp "$COFF_TMP" "$O_FILE"
