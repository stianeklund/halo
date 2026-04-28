#!/usr/bin/env python3
"""Run the Option 3 verification ladder for a target lift.

Option 3 is intended as a practical default lane:
1) build the patched XBE
2) optionally build ISO
3) run objdiff (if reference/candidate objects are provided)
4) optionally load/reset ISO in xemu via QMP
5) remind the user to run assertion tripwire + smoke scenario

This script is intentionally conservative and fail-fast.
"""

from __future__ import annotations

import argparse
import json
import shlex
import subprocess
import sys
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
ARTIFACT_ROOT = ROOT / "artifacts" / "verify_option3"


@dataclass
class StageResult:
  name: str
  ran: bool
  ok: bool
  details: str
  log_path: str = ""


def run_command(argv: list[str], *, log_path: Path) -> subprocess.CompletedProcess:
  proc = subprocess.run(argv, cwd=str(ROOT), capture_output=True, text=True)
  log_path.parent.mkdir(parents=True, exist_ok=True)
  with open(log_path, "w", encoding="utf-8") as f:
    f.write(f"$ {' '.join(shlex.quote(a) for a in argv)}\n")
    f.write(f"exit={proc.returncode}\n\n")
    if proc.stdout:
      f.write("[stdout]\n")
      f.write(proc.stdout)
      if not proc.stdout.endswith("\n"):
        f.write("\n")
    if proc.stderr:
      f.write("\n[stderr]\n")
      f.write(proc.stderr)
      if not proc.stderr.endswith("\n"):
        f.write("\n")
  return proc


def run_shell(command: str, *, log_path: Path) -> subprocess.CompletedProcess:
  return run_command(["bash", "-lc", command], log_path=log_path)


def stage_detail_for_command(command: list[str] | str) -> str:
  if isinstance(command, str):
    return command
  return " ".join(shlex.quote(x) for x in command)


def finalize(
  *,
  target: str,
  run_id: str,
  started_utc: str,
  stages: list[StageResult],
  artifact_dir: Path,
  smoke_scenario: str,
) -> int:
  ok = all(s.ok for s in stages if s.ran)
  summary = {
    "target": target,
    "run_id": run_id,
    "started_utc": started_utc,
    "completed_utc": datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S"),
    "ok": ok,
    "smoke_scenario": smoke_scenario,
    "stages": [
      {
        "name": s.name,
        "ran": s.ran,
        "ok": s.ok,
        "details": s.details,
        "log_path": s.log_path,
      }
      for s in stages
    ],
  }
  summary_path = artifact_dir / "summary.json"
  summary_path.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")

  print(f"verify_option3: {'PASS' if ok else 'FAIL'}")
  for stage in stages:
    run_text = "ran" if stage.ran else "skip"
    result_text = "PASS" if stage.ok else "FAIL"
    detail = stage.details
    if stage.log_path:
      detail = f"{detail} (log: {stage.log_path})"
    print(f"- {stage.name:<18} [{run_text}] {result_text}: {detail}")
  print(f"- artifacts: {summary_path}")

  return 0 if ok else 1


def build_parser() -> argparse.ArgumentParser:
  ap = argparse.ArgumentParser(
    description="Run a practical Option 3 lift verification ladder.")
  ap.add_argument("--target", required=True,
                  help="Function name or address for report labeling.")
  ap.add_argument("--run-id", default="",
                  help="Optional run id for artifact folder naming.")

  ap.add_argument("--skip-build", action="store_true",
                  help="Skip build stage.")
  ap.add_argument("--build-cmd", default="python3 tools/build.py -q",
                  help="Shell command used for build stage.")

  ap.add_argument("--skip-iso", action="store_true",
                  help="Skip ISO build stage.")
  ap.add_argument("--iso-cmd", default="python3 tools/build_iso.py",
                  help="Shell command used for ISO build stage.")

  ap.add_argument("--objdiff-reference", default="",
                  help="Reference object path for tools/objdiff_lift.py.")
  ap.add_argument("--objdiff-candidate", default="",
                  help="Candidate object path for tools/objdiff_lift.py.")
  ap.add_argument("--objdiff-tool", default="",
                  help="Optional objdiff executable path/name.")

  ap.add_argument("--load-into-xemu", action="store_true",
                  help="Load ISO into xemu and reset VM using QMP.")
  ap.add_argument("--iso-path", default="halo-patched.iso",
                  help="ISO path passed to tools/xemu_qmp.py load-iso.")
  ap.add_argument("--no-launch-if-missing", action="store_true",
                  help="Do not auto-launch xemu when QMP is unavailable.")
  ap.add_argument("--qmp-host", default="",
                  help="Optional QMP host override.")
  ap.add_argument("--qmp-port", type=int, default=0,
                  help="Optional QMP port override.")

  ap.add_argument("--smoke-scenario", default="short manual subsystem exercise",
                  help="Scenario reminder printed in final checklist.")
  return ap


def main() -> int:
  args = build_parser().parse_args()

  run_id = args.run_id.strip() or datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")
  artifact_dir = ARTIFACT_ROOT / run_id
  artifact_dir.mkdir(parents=True, exist_ok=True)
  started_utc = datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")

  stages: list[StageResult] = []

  if args.skip_build:
    stages.append(StageResult("build", False, True, "skipped (--skip-build)"))
  else:
    build_log = artifact_dir / "build.log"
    proc = run_shell(args.build_cmd, log_path=build_log)
    build_ok = proc.returncode == 0
    stages.append(StageResult(
      "build",
      True,
      build_ok,
      stage_detail_for_command(args.build_cmd),
      str(build_log),
    ))
    if not build_ok:
      return finalize(
        target=args.target,
        run_id=run_id,
        started_utc=started_utc,
        stages=stages,
        artifact_dir=artifact_dir,
        smoke_scenario=args.smoke_scenario,
      )

  if args.skip_iso:
    stages.append(StageResult("build_iso", False, True, "skipped (--skip-iso)"))
  else:
    iso_log = artifact_dir / "build_iso.log"
    proc = run_shell(args.iso_cmd, log_path=iso_log)
    iso_ok = proc.returncode == 0
    stages.append(StageResult(
      "build_iso",
      True,
      iso_ok,
      stage_detail_for_command(args.iso_cmd),
      str(iso_log),
    ))
    if not iso_ok:
      return finalize(
        target=args.target,
        run_id=run_id,
        started_utc=started_utc,
        stages=stages,
        artifact_dir=artifact_dir,
        smoke_scenario=args.smoke_scenario,
      )

  if args.objdiff_reference and args.objdiff_candidate:
    objdiff_cmd = [
      "python3",
      "tools/objdiff_lift.py",
      "--target",
      args.target,
      "--reference",
      args.objdiff_reference,
      "--candidate",
      args.objdiff_candidate,
    ]
    if args.objdiff_tool:
      objdiff_cmd.extend(["--tool", args.objdiff_tool])
    objdiff_log = artifact_dir / "objdiff.log"
    proc = run_command(objdiff_cmd, log_path=objdiff_log)
    objdiff_ok = proc.returncode == 0
    stages.append(StageResult(
      "objdiff",
      True,
      objdiff_ok,
      stage_detail_for_command(objdiff_cmd),
      str(objdiff_log),
    ))
    if not objdiff_ok:
      return finalize(
        target=args.target,
        run_id=run_id,
        started_utc=started_utc,
        stages=stages,
        artifact_dir=artifact_dir,
        smoke_scenario=args.smoke_scenario,
      )
  else:
    stages.append(StageResult(
      "objdiff",
      False,
      True,
      "skipped (provide both --objdiff-reference and --objdiff-candidate)",
    ))

  if args.load_into_xemu:
    xemu_cmd = ["python3", "tools/xemu_qmp.py"]
    if args.qmp_host:
      xemu_cmd.extend(["--host", args.qmp_host])
    if args.qmp_port > 0:
      xemu_cmd.extend(["--port", str(args.qmp_port)])
    if not args.no_launch_if_missing:
      xemu_cmd.append("--launch-if-missing")
    xemu_cmd.extend(["load-iso", args.iso_path, "--reset"])

    xemu_log = artifact_dir / "xemu_load_reset.log"
    proc = run_command(xemu_cmd, log_path=xemu_log)
    xemu_ok = proc.returncode == 0
    stages.append(StageResult(
      "xemu_load_reset",
      True,
      xemu_ok,
      stage_detail_for_command(xemu_cmd),
      str(xemu_log),
    ))
    if not xemu_ok:
      return finalize(
        target=args.target,
        run_id=run_id,
        started_utc=started_utc,
        stages=stages,
        artifact_dir=artifact_dir,
        smoke_scenario=args.smoke_scenario,
      )
  else:
    stages.append(StageResult(
      "xemu_load_reset",
      False,
      True,
      "skipped (--load-into-xemu not set)",
    ))

  reminder_lines = [
    f"scenario={args.smoke_scenario}",
    "tripwire: gdb -x tools/asserts.gdb",
    "success: no severity!=0 assertions during smoke run",
  ]
  stages.append(StageResult(
    "assert_tripwire",
    True,
    True,
    " | ".join(reminder_lines),
  ))

  return finalize(
    target=args.target,
    run_id=run_id,
    started_utc=started_utc,
    stages=stages,
    artifact_dir=artifact_dir,
    smoke_scenario=args.smoke_scenario,
  )


if __name__ == "__main__":
  raise SystemExit(main())
