#!/usr/bin/env bash
# mizuchi_context.sh — pre-expanded context generator for mizuchi
#
# mizuchi's CCompiler runs `cpp -P` (no -I flags) on the combined context+code
# before calling our compilerScript. `#include "xdk_common.h"` would fail because
# clang doesn't know the project include paths.
#
# Fix: run our own `cpp -P` with the correct include paths here and output the
# fully-expanded type definitions. mizuchi's subsequent `cpp -P` pass on already-
# preprocessed content trivially succeeds (no more #include directives).
#
# Paired with mizuchi_compile.sh which does NOT use /FI xdk_common.h (since the
# types are already inlined into the context by this script).
#
# Usage: tools/mizuchi/mizuchi_context.sh [target_obj_path]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
RXDK_INC="/mnt/c/Program Files (x86)/RXDK/xbox/include"

obj_path="${1:-}"

# Start with the standard Halo headers
{
  printf '#include "xdk_common.h"\n'
  printf '#include "types.h"\n'

  # Add any extra includes from the matching source file
  if [[ -n "$obj_path" ]]; then
    source_path="${obj_path#"$PROJECT_ROOT"/build/CMakeFiles/halo.dir/}"
    source_path="${source_path%.obj}"
    if [[ -f "$PROJECT_ROOT/$source_path" ]]; then
      awk '
        BEGIN {
          emitted["#include \"xdk_common.h\""] = 1
          emitted["#include \"types.h\""] = 1
        }
        /^#include "/ {
          if (!emitted[$0]++) { print }
          next
        }
        started { exit }
        /^[[:space:]]*$/ { next }
        { started = 1 }
      ' "$PROJECT_ROOT/$source_path"
    fi
  fi
} | cpp -P -m32 \
    -I"$PROJECT_ROOT/src" \
    -I"$PROJECT_ROOT/build/generated" \
    "-I$RXDK_INC" \
    -DMSVC -DXDK_BUILD -D_MSC_VER=1300 \
    - 2>/dev/null \
  || {
    # Fallback (e.g., RXDK not installed on this machine): emit bare includes.
    # compile.sh with /FI will still handle them for the actual VC71 compile.
    printf '#include "xdk_common.h"\n'
    printf '#include "types.h"\n'
  }
