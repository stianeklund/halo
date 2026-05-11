#!/usr/bin/env python3
"""
compile_and_view_assembly — compile C code and diff against delinked reference.

This is the core feedback tool for the decompilation loop. Any agent (Claude Code,
OpenCode, Codex, human) calls this after writing C code to see how close it is to
the original binary. Iterate until diff_count == 0.

Usage:
    python3 tools/mizuchi/compile_and_view.py <FUNC_NAME> [--c-file FILE]
    python3 tools/mizuchi/compile_and_view.py <FUNC_NAME> --c-code "void FUN_..."
    echo "void FUN_..." | python3 tools/mizuchi/compile_and_view.py <FUNC_NAME>

Output (JSON to stdout):
    {
      "success": true/false,       # true = 0 diffs (exact match)
      "diff_count": 3,             # approximate mismatch count; 0 = done
      "match_pct": 94.2,           # percentage match
      "output": "...",             # human-readable diff from compare_obj.py
      "error": "..."               # present only on compile failure
    }

Exit code: 0 if compiled successfully (even if diff_count > 0), 1 on hard error.
"""

import argparse
import json
import os
import re
import subprocess
import sys
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[2]
COMPILE_SH = ROOT / "tools" / "permuter" / "compile.sh"
COMPARE_OBJ = ROOT / "tools" / "verify" / "compare_obj.py"
GET_CONTEXT_SH = ROOT / "tools" / "mizuchi" / "get-context.sh"
PROMPTS_DIR = ROOT / "artifacts" / "mizuchi" / "prompts"
VC71_STAGE = ROOT / "build" / "vc71"


def load_settings(func_name: str) -> dict:
    settings_path = PROMPTS_DIR / func_name / "settings.yaml"
    if not settings_path.exists():
        print(json.dumps({"error": f"No prompt folder for {func_name} — run gen_prompts.py first"}))
        sys.exit(1)
    with open(settings_path) as f:
        return yaml.safe_load(f)


def get_context(target_obj: str) -> str:
    result = subprocess.run(
        ["bash", str(GET_CONTEXT_SH), target_obj],
        capture_output=True, text=True, cwd=ROOT,
    )
    return result.stdout if result.returncode == 0 else '#include "xdk_common.h"\n#include "types.h"\n'


def compile_and_diff(func_name: str, c_code: str, target_obj: Path) -> dict:
    VC71_STAGE.mkdir(parents=True, exist_ok=True)
    context = get_context(str(target_obj))
    combined = context + "\n\n" + c_code

    src = VC71_STAGE / f"{func_name}_cav.c"
    obj = VC71_STAGE / f"{func_name}_cav.obj"
    src.write_text(combined, encoding="utf-8")

    try:
        compile_result = subprocess.run(
            ["bash", str(COMPILE_SH), str(src), "-o", str(obj)],
            capture_output=True, text=True,
            env={**os.environ, "REPO_ROOT": str(ROOT)},
        )
        if compile_result.returncode != 0 or not obj.exists():
            err = (compile_result.stderr or compile_result.stdout or "unknown compile error").strip()
            return {"success": False, "diff_count": None, "match_pct": None,
                    "error": f"Compilation failed:\n{err}"}

        cmp = subprocess.run(
            [sys.executable, str(COMPARE_OBJ), str(obj), str(target_obj),
             "--function", func_name, "--show-diffs"],
            capture_output=True, text=True,
        )
        output = cmp.stdout.strip()

        # Parse match % and instruction counts from "XX.X% match (N_c/N_r insns)"
        match_pct = None
        diff_count = None
        m = re.search(r"(\d+(?:\.\d+)?)%\s+match\s+\((\d+)/(\d+)\s+insns\)", output)
        if m:
            match_pct = float(m.group(1))
            n_c = int(m.group(2))
            diff_count = 0 if cmp.returncode == 0 else max(1, round(n_c * (1 - match_pct / 100)))
        else:
            diff_count = 0 if cmp.returncode == 0 else -1

        return {
            "success": cmp.returncode == 0,
            "diff_count": diff_count,
            "match_pct": match_pct,
            "output": output,
        }
    finally:
        src.unlink(missing_ok=True)
        obj.unlink(missing_ok=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("func_name", help="Function name, e.g. FUN_000425c0")
    ap.add_argument("--c-code", default="", help="C source code as a string")
    ap.add_argument("--c-file", default="", help="Path to .c file")
    args = ap.parse_args()

    if args.c_file:
        c_code = Path(args.c_file).read_text()
    elif args.c_code:
        c_code = args.c_code
    elif not sys.stdin.isatty():
        c_code = sys.stdin.read()
    else:
        print(json.dumps({"error": "Provide C code via --c-code, --c-file, or stdin"}))
        sys.exit(1)

    settings = load_settings(args.func_name)
    target_obj = Path(settings["targetObjectPath"])
    if not target_obj.exists():
        print(json.dumps({"error": f"Target object not found: {target_obj}"}))
        sys.exit(1)

    result = compile_and_diff(args.func_name, c_code, target_obj)
    print(json.dumps(result, indent=2))
    sys.exit(0)


if __name__ == "__main__":
    main()
