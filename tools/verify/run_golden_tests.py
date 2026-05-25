#!/usr/bin/env python3
"""Capture oracle and candidate runtime harness output without manual kb.json edits."""

import argparse
import difflib
import json
import os
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent.parent
ARTIFACT_ROOT = ROOT / "artifacts" / "runtime_oracle"
DEBUG_TXT_PATH = ROOT / "xbdm" / "debug.txt"
RUN_BEGIN = "RUN|BEGIN|"
RUN_END = "RUN|END|"
LEGACY_BEGIN = "--- TEST_HARNESS_START ---"
LEGACY_END = "--- TEST_HARNESS_END"


def run_command(cmd: list[str], cwd: Path, env: dict[str, str], output_path: Path) -> None:
    with output_path.open("w", encoding="utf-8") as f:
        proc = subprocess.run(cmd, cwd=str(cwd), env=env, text=True,
                              stdout=f, stderr=subprocess.STDOUT)
    if proc.returncode != 0:
        raise RuntimeError(f"{cmd[0]} failed with exit {proc.returncode}; see {output_path}")


def write_overlay(path: Path, target: str, ported: bool) -> None:
    payload = {
        "functions": {
            target: {
                "ported": ported,
            },
        },
    }
    path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")


def build_variant(label: str, overlay: Path, artifact_dir: Path, skip_build: bool) -> None:
    if skip_build:
        return

    env = os.environ.copy()
    env["HALO_KB_OVERLAY"] = str(overlay)
    run_command(
        ["cmake", "-B", "build", "-S", str(ROOT), "-DHALO_TEST_HARNESS=ON"],
        ROOT,
        env,
        artifact_dir / f"{label}_configure.txt",
    )
    run_command(
        ["cmake", "--build", "build", "--", "-j"],
        ROOT,
        env,
        artifact_dir / f"{label}_build.txt",
    )


def deploy_variant(label: str, artifact_dir: Path, skip_deploy: bool) -> None:
    if skip_deploy:
        return
    run_command(
        [sys.executable, "tools/xbox/deploy_xbox.py", "--xbe-only"],
        ROOT,
        os.environ.copy(),
        artifact_dir / f"{label}_deploy.txt",
    )


def extract_harness_lines(debug_text: str) -> list[str]:
    capture: list[str] = []
    recording = False
    for raw_line in debug_text.splitlines():
        line = raw_line.strip()
        if line.startswith(RUN_BEGIN) or line == LEGACY_BEGIN:
            recording = True
            if line.startswith(RUN_BEGIN):
                capture.append(line)
            continue
        if not recording:
            continue
        if line.startswith(RUN_END):
            capture.append(line)
            return capture
        if line.startswith(LEGACY_END):
            return capture
        if line:
            capture.append(line)
    raise RuntimeError("runtime harness markers were not found in debug output")


def parse_kv_fields(fields: list[str]) -> dict[str, str]:
    parsed = {}
    for field in fields:
        if "=" not in field:
            raise ValueError(f"malformed field: {field}")
        key, value = field.split("=", 1)
        if not key:
            raise ValueError(f"empty field key: {field}")
        parsed[key] = value
    return parsed


def parse_structured_lines(lines: list[str]) -> dict:
    run_started = False
    run_finished = False
    assertions = {"pass": 0, "fail": 0}
    cases = {"begin": 0, "end": 0}
    values = 0

    for line in lines:
        parts = line.split("|")
        kind = parts[0]
        if kind == "RUN":
            if len(parts) < 3:
                raise ValueError(f"malformed RUN line: {line}")
            if parts[1] == "BEGIN":
                run_started = True
                parse_kv_fields(parts[2:])
            elif parts[1] == "END":
                run_finished = True
                parse_kv_fields(parts[2:])
            else:
                raise ValueError(f"unknown RUN action: {line}")
        elif kind == "CASE":
            if len(parts) < 4:
                raise ValueError(f"malformed CASE line: {line}")
            if parts[1] == "BEGIN":
                cases["begin"] += 1
            elif parts[1] == "END":
                cases["end"] += 1
            else:
                raise ValueError(f"unknown CASE action: {line}")
        elif kind == "ASSERT":
            if len(parts) < 5:
                raise ValueError(f"malformed ASSERT line: {line}")
            if parts[1] == "PASS":
                assertions["pass"] += 1
            elif parts[1] == "FAIL":
                assertions["fail"] += 1
            else:
                raise ValueError(f"unknown ASSERT status: {line}")
            parse_kv_fields(parts[3:])
        elif kind == "VALUE":
            if len(parts) < 3:
                raise ValueError(f"malformed VALUE line: {line}")
            parse_kv_fields(parts[2:])
            values += 1
        else:
            raise ValueError(f"unstructured harness line: {line}")

    if not run_started or not run_finished:
        raise ValueError("missing RUN begin or end record")
    if cases["begin"] != cases["end"]:
        raise ValueError("CASE begin/end count mismatch")

    return {
        "assertions": assertions,
        "cases": cases,
        "values": values,
        "line_count": len(lines),
    }


def capture_variant(label: str, args: argparse.Namespace, overlay: Path,
                    artifact_dir: Path) -> dict:
    build_variant(label, overlay, artifact_dir, args.skip_build)
    deploy_variant(label, artifact_dir, args.skip_deploy)

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

        output_path = artifact_dir / f"{label}.txt"
        output_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
        parsed = parse_structured_lines(lines)
        return {"path": str(output_path), "parsed": parsed}

    raw_path = artifact_dir / f"{label}_last_debug.txt"
    raw_path.write_text(last_output, encoding="utf-8")
    raise RuntimeError(f"timeout waiting for {label} harness output; see {raw_path}")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Capture original-oracle and candidate runtime harness output.")
    parser.add_argument("--target", default="",
                        help="Function name or address to toggle between oracle and candidate.")
    parser.add_argument("--run-id", default="",
                        help="Optional artifact folder name.")
    parser.add_argument("--timeout", type=float, default=60.0,
                        help="Seconds to wait for each harness run.")
    parser.add_argument("--poll-interval", type=float, default=2.0,
                        help="Seconds between debug.txt polls.")
    parser.add_argument("--skip-build", action="store_true",
                        help="Reuse the current XBE instead of rebuilding each variant.")
    parser.add_argument("--skip-deploy", action="store_true",
                        help="Reuse the deployed XBE instead of deploying each variant.")
    parser.add_argument("--self-test-parser", action="store_true",
                        help="Run structured output parser self-test and exit.")
    return parser


def self_test_parser() -> None:
    good = [
        "RUN|BEGIN|suite=xbox_harness",
        "CASE|BEGIN|clip|example",
        "VALUE|example.ret|value=0001|changed=00|mask=00000001",
        "CASE|END|clip|example|PASS",
        "ASSERT|PASS|normalize3d mag|got=413BC96E|expected=413BC96E",
        "RUN|END|passed=1|failed=0|total=1",
    ]
    parse_structured_lines(good)
    try:
        parse_structured_lines([
            "RUN|BEGIN|suite=xbox_harness",
            "freeform output",
            "RUN|END|passed=0|failed=1|total=1",
        ])
    except ValueError:
        return
    raise RuntimeError("parser accepted malformed output")


def main() -> int:
    args = build_parser().parse_args()
    if args.self_test_parser:
        self_test_parser()
        print("runtime_oracle parser self-test: PASS")
        return 0
    if not args.target:
        raise SystemExit("--target is required unless --self-test-parser is used")

    run_id = args.run_id or datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")
    artifact_dir = ARTIFACT_ROOT / run_id
    artifact_dir.mkdir(parents=True, exist_ok=True)

    oracle_overlay = artifact_dir / "oracle_overlay.json"
    candidate_overlay = artifact_dir / "candidate_overlay.json"
    write_overlay(oracle_overlay, args.target, False)
    write_overlay(candidate_overlay, args.target, True)

    summary = {
        "target": args.target,
        "run_id": run_id,
        "artifact_dir": str(artifact_dir),
        "started_utc": datetime.now(timezone.utc).isoformat(),
        "oracle_overlay": str(oracle_overlay),
        "candidate_overlay": str(candidate_overlay),
        "ok": False,
    }

    try:
        oracle = capture_variant("oracle", args, oracle_overlay, artifact_dir)
        candidate = capture_variant("candidate", args, candidate_overlay,
                                    artifact_dir)

        oracle_text = Path(oracle["path"]).read_text(encoding="utf-8")
        candidate_text = Path(candidate["path"]).read_text(encoding="utf-8")
        diff_lines = list(difflib.unified_diff(
            oracle_text.splitlines(keepends=True),
            candidate_text.splitlines(keepends=True),
            fromfile="oracle.txt",
            tofile="candidate.txt",
        ))
        diff_path = artifact_dir / "diff.txt"
        diff_path.write_text("".join(diff_lines), encoding="utf-8")

        summary.update({
            "oracle": oracle,
            "candidate": candidate,
            "diff": str(diff_path),
            "ok": not diff_lines,
            "finished_utc": datetime.now(timezone.utc).isoformat(),
        })
    except Exception as exc:
        summary.update({
            "error": str(exc),
            "finished_utc": datetime.now(timezone.utc).isoformat(),
        })

    summary_path = artifact_dir / "summary.json"
    summary_path.write_text(json.dumps(summary, indent=2) + "\n",
                            encoding="utf-8")
    print(f"runtime_oracle: {'PASS' if summary['ok'] else 'FAIL'}")
    print(f"summary: {summary_path}")
    return 0 if summary["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
