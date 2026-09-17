#!/usr/bin/env python3
"""
check_vc71_gate.py — Enforce that ported functions achieve >= 90% VC71 byte accuracy.

Rules:
    - Functions in kb.json should ONLY have "ported": true if their instruction-level
      byte accuracy is >= 90.0% using the MSVC 7.1 compiler (cl.exe).
    - Newly ported functions in a PR must achieve >= 90.0% byte match.
    - If a ported function scores < 90.0%, this script reports a FAIL and exits with code 1.
    - Used in GitHub Actions CI (runs-on: self-hosted) and as a verification gate.

Usage:
    python3 tools/verify/check_vc71_gate.py --source src/halo/game/players.c
    python3 tools/verify/check_vc71_gate.py --base-ref origin/main
    python3 tools/verify/check_vc71_gate.py --threshold 90.0 --summary-file $GITHUB_STEP_SUMMARY
"""

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(REPO_ROOT / "tools"))
sys.path.insert(0, str(REPO_ROOT / "tools" / "verify"))

try:
    import vc71_regression
    run_vc71_verify = vc71_regression.run_vc71_verify
except ImportError:
    try:
        from verify.vc71_regression import run_vc71_verify
    except ImportError:
        run_vc71_verify = None


def load_kb() -> dict:
    with open(REPO_ROOT / "kb.json", "r", encoding="utf-8") as f:
        return json.load(f)


def get_changed_c_files(base_ref: str = "origin/main") -> list[Path]:
    """Find modified src/**/*.c files comparing HEAD against base_ref."""
    try:
        res = subprocess.run(
            ["git", "diff", "--name-only", f"{base_ref}...HEAD"],
            capture_output=True,
            text=True,
            check=True,
            cwd=REPO_ROOT,
        )
        files = []
        for line in res.stdout.splitlines():
            line = line.strip()
            if line.startswith("src/") and line.endswith(".c"):
                p = REPO_ROOT / line
                if p.exists():
                    files.append(p)
        return files
    except Exception as e:
        print(f"Warning: Failed to git diff against {base_ref}: {e}", file=sys.stderr)
        return []


def get_newly_ported_functions(base_ref: str = "origin/main") -> dict[str, dict]:
    """Find functions that are ported: true in HEAD but were not ported: true in base_ref.
    Returns dict of {addr: {"name": str, "source": str, "decl": str}}
    """
    try:
        base_data = subprocess.check_output(
            ["git", "show", f"{base_ref}:kb.json"],
            text=True,
            cwd=REPO_ROOT,
            stderr=subprocess.DEVNULL,
        )
        base_kb = json.loads(base_data)
    except Exception:
        return {}

    head_kb = load_kb()
    base_ported = set()
    for obj in base_kb.get("objects", []):
        for fn in obj.get("functions", []):
            if fn.get("ported") is True:
                base_ported.add(fn.get("addr", "").lower())

    newly_ported = {}
    for obj in head_kb.get("objects", []):
        src = (obj.get("source") or "").replace("\\", "/")
        for fn in obj.get("functions", []):
            addr = fn.get("addr", "").lower()
            if fn.get("ported") is True and addr not in base_ported:
                name = fn.get("name")
                if not name:
                    decl = fn.get("decl", "")
                    before_paren = decl.split("(")[0].strip()
                    name = before_paren.split()[-1].lstrip("*")
                newly_ported[addr] = {
                    "name": name,
                    "source": src,
                    "decl": fn.get("decl", "")
                }
    return newly_ported


def get_ported_functions_for_source(kb: dict, source_rel: str) -> dict[str, str]:
    """Return map of {func_name: addr} for functions marked ported: true in given source."""
    norm_source = source_rel.replace("\\", "/")
    if norm_source.startswith("src/halo/"):
        norm_source = norm_source[len("src/halo/"):]
    elif norm_source.startswith("src/"):
        norm_source = norm_source[len("src/"):]

    ported = {}
    for obj in kb.get("objects", []):
        obj_src = (obj.get("source") or "").replace("\\", "/")
        if obj_src == norm_source:
            for fn in obj.get("functions", []):
                if fn.get("ported") is True:
                    name = fn.get("name")
                    if not name:
                        decl = fn.get("decl", "")
                        before_paren = decl.split("(")[0].strip()
                        name = before_paren.split()[-1].lstrip("*")
                    ported[name] = fn.get("addr", "")
    return ported


def main():
    parser = argparse.ArgumentParser(description="Check VC71 byte accuracy (>= 90%) for ported functions")
    parser.add_argument("--source", action="append", help="Source file(s) to check (relative to repo root)")
    parser.add_argument("--base-ref", default="origin/main", help="Git base ref to diff against if --source not provided")
    parser.add_argument("--threshold", type=float, default=90.0, help="Minimum byte accuracy %% required (default: 90.0)")
    parser.add_argument("--all-ported", action="store_true", help="Audit all ported functions in files, not just newly ported")
    parser.add_argument("--summary-file", help="Path to write GitHub step summary markdown")
    args = parser.parse_args()

    kb = load_kb()

    target_files = []
    newly_ported = {}

    if args.source:
        for s in args.source:
            p = Path(s)
            if not p.is_absolute():
                p = REPO_ROOT / p
            if p.exists():
                target_files.append(p)
            else:
                print(f"Warning: source file {s} not found on disk", file=sys.stderr)
    else:
        newly_ported = get_newly_ported_functions(args.base_ref)
        changed_c_files = get_changed_c_files(args.base_ref)
        file_set = set(changed_c_files)

        # Include source files of any newly ported functions
        for addr, info in newly_ported.items():
            src = info.get("source", "")
            if src:
                candidates = [
                    REPO_ROOT / "src" / "halo" / src,
                    REPO_ROOT / "src" / src,
                ]
                for c in candidates:
                    if c.exists():
                        file_set.add(c)
                        break

        target_files = sorted(list(file_set))

    if not target_files:
        print(f"No changed source files or newly ported functions detected between {args.base_ref} and HEAD.")
        sys.exit(0)

    if run_vc71_verify is None:
        print("Error: Could not import run_vc71_verify from tools.verify.vc71_regression", file=sys.stderr)
        sys.exit(1)

    print(f"=== Running VC71 Byte Accuracy Gate (Threshold: {args.threshold}%) ===")
    if newly_ported and not args.all_ported and not args.source:
        print(f"Auditing {len(newly_ported)} newly ported function(s) across {len(target_files)} file(s)...")
    else:
        print(f"Auditing ported functions in {len(target_files)} source file(s)...")

    results = []
    overall_fail = False

    for src_path in target_files:
        rel_path = src_path.relative_to(REPO_ROOT)
        ported_map = get_ported_functions_for_source(kb, str(rel_path))
        if not ported_map:
            continue

        # Filter to newly ported unless --all-ported or explicit --source
        if newly_ported and not args.all_ported and not args.source:
            active_map = {name: addr for name, addr in ported_map.items() if addr.lower() in newly_ported}
            if not active_map:
                continue
        else:
            active_map = ported_map

        print(f"\nVerifying {rel_path} ({len(active_map)} ported functions to audit)...")
        drops = []
        scores = run_vc71_verify(src_path, no_cache=True, drops_out=drops)

        for fn_name, addr in active_map.items():
            entry = scores.get(fn_name)
            if entry is None:
                for s_name, s_data in scores.items():
                    if s_data.get("addr", "").lower() == addr.lower():
                        entry = s_data
                        fn_name = s_name
                        break

            if entry is None:
                status = "ERROR"
                score = 0.0
                reason = "Not found in VC71 output (compile failed, bounds missing, or unexported)"
                overall_fail = True
            else:
                score = float(entry.get("score", 0.0))
                if score >= args.threshold:
                    status = "PASS"
                    reason = "Meets accuracy threshold"
                else:
                    status = "FAIL"
                    reason = f"Score {score:.1f}% is below required {args.threshold:.1f}%"
                    overall_fail = True

            results.append({
                "source": str(rel_path),
                "function": fn_name,
                "addr": addr,
                "score": score,
                "status": status,
                "reason": reason,
            })
            tag = "✅ PASS" if status == "PASS" else ("❌ FAIL" if status == "FAIL" else "⚠️ ERROR")
            print(f"  {tag} {fn_name} ({addr}): {score:.1f}% — {reason}")

    total = len(results)
    passed = sum(1 for r in results if r["status"] == "PASS")
    failed = sum(1 for r in results if r["status"] == "FAIL")
    errors = sum(1 for r in results if r["status"] == "ERROR")

    print("\n=== Summary ===")
    print(f"Total ported functions checked: {total}")
    print(f"Passed (>= {args.threshold}%): {passed}")
    print(f"Failed (< {args.threshold}%): {failed}")
    print(f"Errors (unmeasured): {errors}")

    summary_md = [
        f"## VC71 Byte Accuracy Verification (>= {args.threshold}%)",
        "",
        f"- **Total Ported Functions Checked:** {total}",
        f"- **Passed (>= {args.threshold}%):** {passed}",
        f"- **Failed (< {args.threshold}%):** {failed}",
        f"- **Errors (unmeasured):** {errors}",
        "",
        "| Status | Function | Address | Score | File | Note |",
        "| :---: | :--- | :---: | :---: | :--- | :--- |",
    ]

    for r in results:
        icon = "✅ PASS" if r["status"] == "PASS" else ("❌ FAIL" if r["status"] == "FAIL" else "⚠️ ERROR")
        summary_md.append(f"| {icon} | `{r['function']}` | `{r['addr']}` | {r['score']:.1f}% | `{r['source']}` | {r['reason']} |")

    if args.summary_file:
        try:
            with open(args.summary_file, "a", encoding="utf-8") as f:
                f.write("\n" + "\n".join(summary_md) + "\n")
        except Exception as e:
            print(f"Warning: Failed to write summary to {args.summary_file}: {e}", file=sys.stderr)

    if overall_fail:
        print("\n❌ GATE FAILED: Functions must achieve >= 90% VC71 byte accuracy to be marked ported: true.")
        sys.exit(1)
    else:
        print("\n✅ GATE PASSED: All checked ported functions meet or exceed the 90% byte accuracy threshold.")
        sys.exit(0)


if __name__ == "__main__":
    main()
