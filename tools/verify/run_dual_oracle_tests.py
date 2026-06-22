#!/usr/bin/env python3
"""Run a same-boot runtime oracle/candidate comparison for one harness target."""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

from run_golden_tests import (DEBUG_TXT_PATH, ROOT, deploy_variant,
                              extract_harness_lines, parse_structured_lines,
                              restore_harness_off, run_command)


ARTIFACT_ROOT = ROOT / "artifacts" / "runtime_dual_oracle"


def write_overlay(path: Path, target: str) -> None:
    payload = {
        "functions": {
            target: {
                "dual_oracle": True,
            },
        },
    }
    path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")


def build_dual_oracle(target: str, overlay: Path, artifact_dir: Path,
                      skip_build: bool) -> None:
    if skip_build:
        return

    env = os.environ.copy()
    env["HALO_KB_OVERLAY"] = str(overlay)
    run_command(
        [
            "cmake", "-B", "build", "-S", str(ROOT),
            "-DHALO_TEST_HARNESS=ON",
            f"-DHALO_DUAL_ORACLE_TARGET={target}",
            "-DCMAKE_TOOLCHAIN_FILE=toolchains/llvm.cmake",
        ],
        ROOT,
        env,
        artifact_dir / "dual_configure.txt",
    )
    run_command(
        ["cmake", "--build", "build", "--", "-j"],
        ROOT,
        env,
        artifact_dir / "dual_build.txt",
    )


def capture_output(args: argparse.Namespace, artifact_dir: Path) -> dict:
    fetch_cmd = [sys.executable, "tools/xbox/xbdm_debug_txt.py"]
    deadline = time.time() + args.timeout
    last_output = ""

    while time.time() < deadline:
        time.sleep(args.poll_interval)
        proc = subprocess.run(fetch_cmd, cwd=str(ROOT), capture_output=True,
                              text=True)
        last_output = proc.stdout
        if proc.returncode != 0:
            continue
        try:
            debug_text = DEBUG_TXT_PATH.read_text(encoding="utf-8")
        except OSError:
            continue
        try:
            lines = extract_harness_lines(debug_text)
        except RuntimeError:
            continue

        output_path = artifact_dir / "dual_oracle.txt"
        output_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
        parsed = parse_structured_lines(lines)
        return {"path": str(output_path), "parsed": parsed}

    raw_path = artifact_dir / "dual_last_debug.txt"
    raw_path.write_text(last_output, encoding="utf-8")
    raise RuntimeError(f"timeout waiting for dual-oracle harness output; see {raw_path}")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Run same-boot original-vs-candidate runtime oracle target.")
    parser.add_argument("--target", required=True,
                        help="Function name with a DUAL_ORACLE_TARGET_* harness case")
    parser.add_argument("--run-id", default="",
                        help="Optional artifact folder name")
    parser.add_argument("--timeout", type=float, default=60.0,
                        help="Seconds to wait for harness output")
    parser.add_argument("--poll-interval", type=float, default=2.0,
                        help="Seconds between debug.txt polls")
    parser.add_argument("--skip-build", action="store_true",
                        help="Reuse current XBE instead of rebuilding")
    parser.add_argument("--skip-deploy", action="store_true",
                        help="Reuse currently deployed XBE")
    parser.add_argument("--skip-restore", action="store_true",
                        help="Do not restore HALO_TEST_HARNESS=OFF after the run")
    return parser


def main() -> int:
    args = build_parser().parse_args()
    run_id = args.run_id or datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")
    artifact_dir = ARTIFACT_ROOT / run_id
    artifact_dir.mkdir(parents=True, exist_ok=True)

    overlay = artifact_dir / "dual_overlay.json"
    write_overlay(overlay, args.target)

    summary = {
        "target": args.target,
        "run_id": run_id,
        "artifact_dir": str(artifact_dir),
        "started_utc": datetime.now(timezone.utc).isoformat(),
        "dual_overlay": str(overlay),
        "ok": False,
    }

    try:
        build_dual_oracle(args.target, overlay, artifact_dir, args.skip_build)
        deploy_variant("dual_oracle", artifact_dir, args.skip_deploy)
        result = capture_output(args, artifact_dir)
        assertion_counts = result["parsed"]["assertions"]
        summary.update({
            "dual_oracle": result,
            "ok": assertion_counts.get("fail", 0) == 0,
            "finished_utc": datetime.now(timezone.utc).isoformat(),
        })
    except Exception as exc:
        summary.update({
            "error": str(exc),
            "finished_utc": datetime.now(timezone.utc).isoformat(),
        })
    finally:
        if args.skip_restore:
            summary["restore_harness_off"] = {
                "ok": True,
                "skipped": True,
                "reason": "deferred by --skip-restore",
            }
        else:
            summary["restore_harness_off"] = restore_harness_off(
                artifact_dir, args.skip_build, args.skip_deploy)

    summary_path = artifact_dir / "summary.json"
    summary_path.write_text(json.dumps(summary, indent=2) + "\n",
                            encoding="utf-8")
    print(f"runtime_dual_oracle: {'PASS' if summary['ok'] else 'FAIL'}")
    print(f"summary: {summary_path}")
    return 0 if summary["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
