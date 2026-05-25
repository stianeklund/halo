#!/usr/bin/env python3
"""Batch-run runtime-oracle golden tests for curated targets."""

from __future__ import annotations

import argparse
import csv
import json
import signal
import subprocess
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent.parent
DEFAULT_TARGETS = ROOT / "tools" / "verify" / "runtime_oracle_targets.json"
DEFAULT_OUTPUT = ROOT / "artifacts" / "runtime_oracle_batch"
DEFAULT_PROGRESS_DIR = ROOT / "artifacts" / "progress"
FAIL_STATUSES = {"fail", "error"}


def load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def load_targets(path: Path) -> list[dict]:
    raw = load_json(path)
    targets = raw.get("targets", [])
    cleaned = []
    for entry in targets:
        if not isinstance(entry, dict):
            continue
        if entry.get("disabled"):
            continue
        target = str(entry.get("target") or entry.get("name") or "").strip()
        if not target:
            continue
        cleaned.append({
            "target": target,
            "obj": str(entry.get("obj", "")).strip(),
            "reason": str(entry.get("reason", "")).strip(),
            "timeout": int(entry.get("timeout", 60)),
            "poll_interval": float(entry.get("poll_interval", 2.0)),
            "flags": [str(flag) for flag in entry.get("flags", [])],
        })
    return cleaned


def sanitize_target(text: str) -> str:
    out = []
    for ch in text:
        if ch.isalnum() or ch in ("_", "-"):
            out.append(ch)
        else:
            out.append("_")
    return "".join(out).strip("_") or "target"


def summary_status(summary: dict) -> str:
    if summary.get("ok"):
        return "pass"
    if summary.get("error"):
        return "error"
    return "fail"


def failure_key(row: dict) -> str:
    return f"{row['target']}|{row['status']}|{row.get('error', '')}"


def baseline_failure_keys(path: Path | None) -> set[str]:
    if not path:
        return set()
    raw = load_json(path)
    return {
        failure_key(row)
        for row in raw.get("rows", [])
        if row.get("status") in FAIL_STATUSES
    }


def update_dashboard(progress_dir: Path) -> None:
    progress_dir.mkdir(parents=True, exist_ok=True)
    cmd = [
        sys.executable,
        str(ROOT / "tools" / "report" / "generate_decomp_report.py"),
        "--output", str(progress_dir / "report.json"),
        "--html", str(progress_dir / "index.html"),
    ]
    subprocess.run(cmd, check=True, cwd=str(ROOT))


def run_target(entry: dict, batch_id: str, skip_build: bool, skip_deploy: bool) -> tuple[dict, str]:
    target = entry["target"]
    run_id = f"{batch_id}-{sanitize_target(target)}"
    summary_path = ROOT / "artifacts" / "runtime_oracle" / run_id / "summary.json"
    cmd = [
        sys.executable,
        str(ROOT / "tools" / "verify" / "run_golden_tests.py"),
        "--target", target,
        "--run-id", run_id,
        "--timeout", str(entry["timeout"]),
        "--poll-interval", str(entry["poll_interval"]),
    ]
    if skip_build:
        cmd.append("--skip-build")
    if skip_deploy:
        cmd.append("--skip-deploy")
    cmd.extend(entry.get("flags", []))

    started = time.time()
    proc = subprocess.run(cmd, capture_output=True, text=True, cwd=str(ROOT))
    elapsed = time.time() - started

    if summary_path.exists():
        summary = load_json(summary_path)
    else:
        summary = {
            "target": target,
            "run_id": run_id,
            "artifact_dir": str(summary_path.parent),
            "ok": False,
            "error": f"missing summary.json (exit {proc.returncode})",
        }
    summary["_batch_elapsed_seconds"] = elapsed
    return summary, proc.stdout + proc.stderr


def main() -> int:
    parser = argparse.ArgumentParser(description="Batch-run runtime-oracle golden tests")
    parser.add_argument("--targets", type=Path, default=DEFAULT_TARGETS,
                        help="Target manifest JSON")
    parser.add_argument("--limit", type=int, default=0,
                        help="Run at most N targets (0 = all)")
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT,
                        help="Batch artifact directory")
    parser.add_argument("--batch-id", default="",
                        help="Optional batch identifier")
    parser.add_argument("--csv", action="store_true",
                        help="Write per-target results CSV")
    parser.add_argument("--dry-run", action="store_true",
                        help="List targets without running them")
    parser.add_argument("--skip-build", action="store_true",
                        help="Pass --skip-build through to run_golden_tests.py")
    parser.add_argument("--skip-deploy", action="store_true",
                        help="Pass --skip-deploy through to run_golden_tests.py")
    parser.add_argument("--baseline", type=Path, default=None,
                        help="Previous batch summary.json for comparison")
    parser.add_argument("--fail-on-new", action="store_true",
                        help="Exit non-zero only on new failures vs baseline")
    parser.add_argument("--no-update-dashboard", action="store_true",
                        help="Do not regenerate artifacts/progress after the batch run")
    args = parser.parse_args()

    targets = load_targets(args.targets)
    if args.limit > 0:
        targets = targets[:args.limit]

    print(f"Targets: {len(targets)}")
    if args.dry_run:
        for entry in targets:
            print(f"  {entry['target']:24s} {entry['obj']:24s} timeout={entry['timeout']}  {entry['reason']}")
        return 0

    batch_id = args.batch_id or time.strftime("%Y%m%d-%H%M%S")
    output_dir = args.output_dir / batch_id
    output_dir.mkdir(parents=True, exist_ok=True)

    rows = []
    csv_rows = []
    results = {"total": len(targets), "pass": 0, "fail": 0, "error": 0}
    interrupted = False
    t0 = time.time()

    def on_sigint(signum, frame):
        nonlocal interrupted
        interrupted = True
        print("\n\nInterrupted - writing summary...")

    prev_handler = signal.signal(signal.SIGINT, on_sigint)

    try:
        for index, entry in enumerate(targets, start=1):
            if interrupted:
                break
            print(f"[{index}/{len(targets)}] {entry['target']:32s} ", end="", flush=True)
            summary, output = run_target(entry, batch_id, args.skip_build, args.skip_deploy)
            status = summary_status(summary)
            results[status] += 1
            row = {
                "target": entry["target"],
                "obj": entry["obj"],
                "reason": entry["reason"],
                "status": status,
                "run_id": summary.get("run_id", ""),
                "artifact_dir": summary.get("artifact_dir", ""),
                "summary_path": str((ROOT / "artifacts" / "runtime_oracle" / summary.get("run_id", "") / "summary.json") if summary.get("run_id") else ""),
                "error": summary.get("error", ""),
                "elapsed_seconds": round(float(summary.get("_batch_elapsed_seconds", 0.0)), 2),
                "oracle_assert_pass": (((summary.get("oracle") or {}).get("parsed") or {}).get("assertions") or {}).get("pass", 0),
                "oracle_assert_fail": (((summary.get("oracle") or {}).get("parsed") or {}).get("assertions") or {}).get("fail", 0),
                "candidate_assert_pass": (((summary.get("candidate") or {}).get("parsed") or {}).get("assertions") or {}).get("pass", 0),
                "candidate_assert_fail": (((summary.get("candidate") or {}).get("parsed") or {}).get("assertions") or {}).get("fail", 0),
                "diff": summary.get("diff", ""),
            }
            rows.append(row)
            csv_rows.append(row)

            if status == "pass":
                print(f"PASS ({row['elapsed_seconds']:.1f}s)")
            elif status == "fail":
                print(f"FAIL ({row['elapsed_seconds']:.1f}s)")
            else:
                print(f"ERROR ({row['error']})")

            log_path = output_dir / f"{sanitize_target(entry['target'])}.log"
            log_path.write_text(output, encoding="utf-8")
    finally:
        signal.signal(signal.SIGINT, prev_handler)

    baseline_keys = baseline_failure_keys(args.baseline)
    current_failure_rows = [row for row in rows if row["status"] in FAIL_STATUSES]
    comparison = {
        "baseline": str(args.baseline) if args.baseline else "",
        "new_failures": [row for row in current_failure_rows if failure_key(row) not in baseline_keys],
        "known_failures": [row for row in current_failure_rows if failure_key(row) in baseline_keys],
    }

    elapsed = time.time() - t0
    summary = {
        "batch_id": batch_id,
        "targets_file": str(args.targets),
        "results": results,
        "rows": rows,
        "comparison": comparison,
        "elapsed_seconds": elapsed,
        "interrupted": interrupted,
    }
    summary_path = output_dir / "summary.json"
    summary_path.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")

    if not args.no_update_dashboard:
        update_dashboard(DEFAULT_PROGRESS_DIR)

    if args.csv:
        csv_path = output_dir / "results.csv"
        with csv_path.open("w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=[
                "target", "obj", "reason", "status", "run_id", "artifact_dir",
                "summary_path", "error", "elapsed_seconds", "oracle_assert_pass",
                "oracle_assert_fail", "candidate_assert_pass", "candidate_assert_fail", "diff",
            ])
            writer.writeheader()
            writer.writerows(csv_rows)

    print(f"\n{'=' * 60}")
    print(f"RUNTIME ORACLE BATCH RESULTS ({elapsed:.1f}s)")
    print(f"{'=' * 60}")
    print(f"  Total:        {results['total']}")
    print(f"  Pass:         {results['pass']}")
    print(f"  Fail:         {results['fail']}")
    print(f"  Error:        {results['error']}")
    if args.baseline:
        print(f"  New failures: {len(comparison['new_failures'])}")
        print(f"  Known:        {len(comparison['known_failures'])}")
    print(f"\nSummary: {summary_path}")

    if args.fail_on_new and args.baseline:
        return 1 if comparison["new_failures"] else 0
    return 1 if results["fail"] or results["error"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
