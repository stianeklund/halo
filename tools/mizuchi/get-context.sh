#!/usr/bin/env bash

set -euo pipefail

obj_path="${1:-}"

printf '#include "xdk_common.h"\n'
printf '#include "types.h"\n'

if [[ -z "$obj_path" ]]; then
  exit 0
fi

project_root="$(pwd)"
source_path="${obj_path#"$project_root"/build/CMakeFiles/halo.dir/}"
source_path="${source_path%.obj}"

if [[ ! -f "$source_path" ]]; then
  exit 0
fi

awk '
  BEGIN {
    emitted["#include \"xdk_common.h\""] = 1
    emitted["#include \"types.h\""] = 1
  }
  /^#include "/ {
    if (!emitted[$0]++) {
      print
    }
    next
  }
  started {
    exit
  }
  /^[[:space:]]*$/ {
    next
  }
  {
    started = 1
  }
' "$source_path"
