#!/usr/bin/env python3
"""Orchestrate decompile -> reimplementation -> build -> verify workflow.

This script is intentionally conservative and supports an assist-first mode.
It can pick a target from frontier output, optionally insert/update a function,
build, run structural checks, run runtime hash checks, and update kb_meta status.
"""

from __future__ import annotations

import argparse
import json
import re
import shlex
import subprocess
import sys
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Optional


ROOT = Path(__file__).resolve().parent.parent
ARTIFACT_ROOT = ROOT / "artifacts" / "lift_runs"


@dataclass
class Target:
  addr: str
  name: str
  object_name: str
  source_path: str


@dataclass
class StageResult:
  name: str
  ran: bool
  ok: bool
  details: str


def run_command(argv: list[str], *, cwd: Path, log_path: Path) -> subprocess.CompletedProcess:
  proc = subprocess.run(argv, cwd=str(cwd), capture_output=True, text=True)
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


def parse_function_name_from_decl(decl: str) -> str:
  left = decl.split("(", 1)[0].strip()
  token = re.split(r"[\s\*]+", left)[-1]
  return token.strip()


def load_kb_index() -> tuple[dict[str, Target], dict[str, Target]]:
  kb = json.loads((ROOT / "kb.json").read_text(encoding="utf-8"))
  by_name: dict[str, Target] = {}
  by_addr: dict[str, Target] = {}
  for obj in kb.get("objects", []):
    object_name = obj.get("name", "?")
    source = obj.get("source", "")
    source_path = f"src/halo/{source}" if source else ""
    for fn in obj.get("functions", []):
      addr = str(fn.get("addr", "")).lower()
      decl = fn.get("decl", "")
      name = parse_function_name_from_decl(decl)
      if not addr or not name:
        continue
      target = Target(addr=addr, name=name, object_name=object_name, source_path=source_path)
      by_name[name] = target
      by_addr[addr] = target
  return by_name, by_addr


def pick_target_from_frontier(frontier_limit: int, object_filter: str, artifact_dir: Path) -> Optional[tuple[str, str]]:
  log_path = artifact_dir / "frontier.log"
  proc = run_command(
    ["python3", "tools/frontier.py", "--limit", str(frontier_limit)],
    cwd=ROOT,
    log_path=log_path,
  )
  if proc.returncode != 0:
    return None

  current_object = ""
  object_re = re.compile(r"^\s*candidates for ([^:]+):\s*$")
  cand_re = re.compile(r"^\s*-\s*(0x[0-9a-fA-F]+)\s+(\S+)\s*$")

  candidates: list[tuple[str, str, str]] = []
  for line in proc.stdout.splitlines():
    m_obj = object_re.match(line)
    if m_obj:
      current_object = m_obj.group(1)
      continue
    m_cand = cand_re.match(line)
    if m_cand and current_object:
      addr = m_cand.group(1).lower()
      name = m_cand.group(2)
      candidates.append((current_object, addr, name))

  if not candidates:
    return None

  if object_filter:
    for obj, addr, name in candidates:
      if obj == object_filter:
        return addr, name
    return None

  _, addr, name = candidates[0]
  return addr, name


def resolve_target(args: argparse.Namespace, artifact_dir: Path) -> Target:
  by_name, by_addr = load_kb_index()

  token = args.target.strip() if args.target else ""
  if not token:
    picked = pick_target_from_frontier(args.frontier_limit, args.frontier_object, artifact_dir)
    if not picked:
      raise RuntimeError("Failed to pick a frontier target")
    token = picked[0]

  if token.lower().startswith("0x"):
    target = by_addr.get(token.lower())
  else:
    target = by_name.get(token)

  if not target:
    raise RuntimeError(f"Target not found in kb.json: {token}")
  return target


def parse_boot_hash_sha(text: str) -> Optional[str]:
  m = re.search(r"sha256=([0-9a-fA-F]{64})", text)
  return m.group(1).lower() if m else None


def render_template(value: str, *, target: Target, artifact_dir: Path) -> str:
  out = value
  replacements = {
    "{target_addr}": target.addr,
    "{target_name}": target.name,
    "{target_object}": target.object_name,
    "{artifact_dir}": str(artifact_dir),
  }
  for key, repl in replacements.items():
    out = out.replace(key, repl)
  return out


def run_pipeline(args: argparse.Namespace) -> int:
  timestamp = datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")
  run_id = args.run_id or timestamp
  artifact_dir = ARTIFACT_ROOT / run_id
  artifact_dir.mkdir(parents=True, exist_ok=True)

  summary: dict[str, object] = {
    "run_id": run_id,
    "started_utc": timestamp,
    "stages": [],
  }

  stages: list[StageResult] = []

  target = resolve_target(args, artifact_dir)
  summary["target"] = {
    "addr": target.addr,
    "name": target.name,
    "object": target.object_name,
    "source": target.source_path,
  }

  stage = StageResult("target_pick", ran=True, ok=True,
                      details=f"{target.addr} {target.name} ({target.object_name})")
  stages.append(stage)

  if args.extract_cmd:
    cmd = args.extract_cmd.format(
      target_addr=target.addr,
      target_name=target.name,
      target_object=target.object_name,
      artifact_dir=str(artifact_dir),
    )
    proc = run_command(["bash", "-lc", cmd], cwd=ROOT,
                       log_path=artifact_dir / "extract.log")
    stages.append(StageResult("ghidra_extract", ran=True, ok=proc.returncode == 0,
                              details="custom extract command"))
    if proc.returncode != 0:
      return finalize(summary, stages, artifact_dir, ok=False)
  else:
    stages.append(StageResult("ghidra_extract", ran=False, ok=True,
                              details="skipped (no --extract-cmd)"))

  if args.candidate:
    source = args.source if args.source else target.source_path
    if not source:
      stages.append(StageResult("source_update", ran=True, ok=False,
                                details="no source path resolved; pass --source"))
      return finalize(summary, stages, artifact_dir, ok=False)

    cmd = [
      "python3",
      "tools/maintain.py",
      "--update-from",
      args.candidate,
      source,
    ]
    proc = run_command(cmd, cwd=ROOT, log_path=artifact_dir / "source_update.log")
    stages.append(StageResult("source_update", ran=True, ok=proc.returncode == 0,
                              details=f"candidate={args.candidate} source={source}"))
    if proc.returncode != 0:
      return finalize(summary, stages, artifact_dir, ok=False)
  else:
    stages.append(StageResult("source_update", ran=False, ok=True,
                              details="skipped (no --candidate)"))

  if args.skip_build:
    stages.append(StageResult("build", ran=False, ok=True,
                              details="skipped (--skip-build)"))
    build_ok = True
  else:
    cmd = shlex.split(args.build_cmd)
    proc = run_command(cmd, cwd=ROOT, log_path=artifact_dir / "build.log")
    build_ok = proc.returncode == 0
    stages.append(StageResult("build", ran=True, ok=build_ok, details=args.build_cmd))
    if not build_ok:
      return finalize(summary, stages, artifact_dir, ok=False)

  verify_input_path = args.verify_input
  if not verify_input_path and args.verify_auto:
    if not args.verify_new_address:
      stages.append(StageResult("verify_payload", ran=True, ok=False,
                                details="--verify-new-address is required with --verify-auto"))
      return finalize(summary, stages, artifact_dir, ok=False)

    verify_input_path = args.verify_output or str(artifact_dir / "verify_payload.json")
    collect_cmd = [
      "python3",
      "tools/collect_verify_payload.py",
      "--name",
      target.name,
      "--orig-address",
      target.addr,
      "--new-address",
      args.verify_new_address,
      "--output",
      verify_input_path,
    ]

    if args.orig_decompile_file:
      collect_cmd.extend(["--orig-decompile-file", args.orig_decompile_file])
    elif args.orig_decompile_cmd:
      collect_cmd.extend(["--orig-decompile-cmd", render_template(args.orig_decompile_cmd, target=target, artifact_dir=artifact_dir)])
    else:
      collect_cmd.extend(["--orig-decompile-file", str(artifact_dir / "orig_decompile.txt")])

    if args.new_decompile_file:
      collect_cmd.extend(["--new-decompile-file", args.new_decompile_file])
    elif args.new_decompile_cmd:
      collect_cmd.extend(["--new-decompile-cmd", render_template(args.new_decompile_cmd, target=target, artifact_dir=artifact_dir)])
    else:
      collect_cmd.extend(["--new-decompile-file", str(artifact_dir / "new_decompile.txt")])

    if args.orig_callees_file:
      collect_cmd.extend(["--orig-callees-file", args.orig_callees_file])
    elif args.orig_callees_cmd:
      collect_cmd.extend(["--orig-callees-cmd", render_template(args.orig_callees_cmd, target=target, artifact_dir=artifact_dir)])
    else:
      default_orig_callees = artifact_dir / "orig_callees.txt"
      if default_orig_callees.exists():
        collect_cmd.extend(["--orig-callees-file", str(default_orig_callees)])

    if args.new_callees_file:
      collect_cmd.extend(["--new-callees-file", args.new_callees_file])
    elif args.new_callees_cmd:
      collect_cmd.extend(["--new-callees-cmd", render_template(args.new_callees_cmd, target=target, artifact_dir=artifact_dir)])
    else:
      default_new_callees = artifact_dir / "new_callees.txt"
      if default_new_callees.exists():
        collect_cmd.extend(["--new-callees-file", str(default_new_callees)])

    proc = run_command(collect_cmd, cwd=ROOT, log_path=artifact_dir / "verify_payload.log")
    ok = proc.returncode == 0
    stages.append(StageResult("verify_payload", ran=True, ok=ok, details=verify_input_path))
    if not ok:
      return finalize(summary, stages, artifact_dir, ok=False)
  else:
    stages.append(StageResult("verify_payload", ran=False, ok=True,
                              details="skipped (no --verify-auto or --verify-input already set)"))

  structural_enabled = bool(verify_input_path) or bool(args.objdiff_reference and args.objdiff_candidate)
  structural_ok = True

  if verify_input_path:
    proc = run_command(
      ["python3", "tools/verify_lift.py", verify_input_path],
      cwd=ROOT,
      log_path=artifact_dir / "verify_lift.log",
    )
    ok = proc.returncode == 0
    structural_ok = structural_ok and ok
    stages.append(StageResult("verify_lift", ran=True, ok=ok, details=verify_input_path))
  else:
    stages.append(StageResult("verify_lift", ran=False, ok=True,
                              details="skipped (no verify payload)"))

  if args.objdiff_reference and args.objdiff_candidate:
    cmd = [
      "python3",
      "tools/objdiff_lift.py",
      "--target",
      target.name,
      "--reference",
      args.objdiff_reference,
      "--candidate",
      args.objdiff_candidate,
    ]
    if args.objdiff_tool:
      cmd.extend(["--tool", args.objdiff_tool])
    proc = run_command(cmd, cwd=ROOT, log_path=artifact_dir / "objdiff.log")
    ok = proc.returncode == 0
    structural_ok = structural_ok and ok
    stages.append(StageResult("objdiff", ran=True, ok=ok,
                              details=f"{args.objdiff_reference} vs {args.objdiff_candidate}"))
  else:
    stages.append(StageResult("objdiff", ran=False, ok=True,
                              details="skipped (no --objdiff-reference/--objdiff-candidate)"))

  runtime_enabled = args.with_runtime
  runtime_ok = True
  if runtime_enabled:
    patched_iso = args.runtime_patched_iso
    boot_args = ["bash", "tools/boot_hash.sh"]
    if args.runtime_addr:
      boot_args.extend(["--addr", args.runtime_addr])
    if args.runtime_nwords:
      boot_args.extend(["--nwords", str(args.runtime_nwords)])
    if args.runtime_wait:
      boot_args.extend(["--wait", str(args.runtime_wait)])

    patched_proc = run_command(
      boot_args + [patched_iso],
      cwd=ROOT,
      log_path=artifact_dir / "runtime_patched.log",
    )
    patched_hash = parse_boot_hash_sha((patched_proc.stdout or "") + "\n" + (patched_proc.stderr or ""))

    if args.runtime_baseline_hash:
      baseline_hash = args.runtime_baseline_hash.lower()
      baseline_ok = True
    elif args.runtime_baseline_iso:
      baseline_proc = run_command(
        boot_args + [args.runtime_baseline_iso],
        cwd=ROOT,
        log_path=artifact_dir / "runtime_baseline.log",
      )
      baseline_hash = parse_boot_hash_sha((baseline_proc.stdout or "") + "\n" + (baseline_proc.stderr or ""))
      baseline_ok = baseline_proc.returncode == 0 and bool(baseline_hash)
    else:
      baseline_hash = None
      baseline_ok = False

    runtime_ok = patched_proc.returncode == 0 and bool(patched_hash) and baseline_ok and (patched_hash == baseline_hash)
    details = f"patched={patched_hash or 'n/a'} baseline={baseline_hash or 'n/a'}"
    stages.append(StageResult("runtime_check", ran=True, ok=runtime_ok, details=details))
  else:
    stages.append(StageResult("runtime_check", ran=False, ok=True,
                              details="skipped (--with-runtime not set)"))

  if args.no_metadata_update:
    stages.append(StageResult("metadata_update", ran=False, ok=True,
                              details="skipped (--no-metadata-update)"))
  else:
    md_ok = True
    md_notes: list[str] = []
    if build_ok:
      proc = run_command(
        ["python3", "tools/kb_meta.py", "set-status", "--addr", target.addr, "--status", "ported"],
        cwd=ROOT,
        log_path=artifact_dir / "metadata_ported.log",
      )
      md_ok = md_ok and (proc.returncode == 0)
      md_notes.append("ported")

    verification_enabled = structural_enabled or runtime_enabled
    if build_ok and verification_enabled and structural_ok and runtime_ok:
      proc = run_command(
        ["python3", "tools/kb_meta.py", "set-status", "--addr", target.addr, "--status", "verified"],
        cwd=ROOT,
        log_path=artifact_dir / "metadata_verified.log",
      )
      md_ok = md_ok and (proc.returncode == 0)
      md_notes.append("verified")

    stages.append(StageResult("metadata_update", ran=True, ok=md_ok,
                              details=", ".join(md_notes) if md_notes else "no status change"))

  overall_ok = all(s.ok for s in stages if s.ran)
  return finalize(summary, stages, artifact_dir, ok=overall_ok)


def finalize(summary: dict[str, object], stages: list[StageResult], artifact_dir: Path, ok: bool) -> int:
  summary["completed_utc"] = datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")
  summary["ok"] = ok
  summary["stages"] = [
    {
      "name": s.name,
      "ran": s.ran,
      "ok": s.ok,
      "details": s.details,
    }
    for s in stages
  ]
  summary_path = artifact_dir / "summary.json"
  summary_path.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")

  print(f"lift_pipeline: {'PASS' if ok else 'FAIL'}")
  for s in stages:
    state = "PASS" if s.ok else "FAIL"
    run = "ran" if s.ran else "skip"
    print(f"- {s.name:<16} [{run}] {state}: {s.details}")
  print(f"- artifacts: {summary_path}")
  return 0 if ok else 1


def build_parser() -> argparse.ArgumentParser:
  ap = argparse.ArgumentParser(
    description="Orchestrate lift workflow stages with conservative gating.")

  ap.add_argument("--target", default="",
                  help="Function address (0x...) or function name. If omitted, picks top frontier candidate.")
  ap.add_argument("--frontier-limit", type=int, default=20,
                  help="Number of frontier rows to request when auto-picking target.")
  ap.add_argument("--frontier-object", default="",
                  help="Restrict auto-pick to a specific object name (e.g. main.obj).")
  ap.add_argument("--run-id", default="", help="Optional artifact run id.")

  ap.add_argument("--extract-cmd", default="",
                  help="Optional extraction command template. Placeholders: {target_addr} {target_name} {target_object} {artifact_dir}")

  ap.add_argument("--candidate", default="",
                  help="Path to a C snippet file for maintain.py --update-from.")
  ap.add_argument("--source", default="",
                  help="Source file for maintain.py (defaults to kb.json source mapping for target).")

  ap.add_argument("--skip-build", action="store_true",
                  help="Skip build stage.")
  ap.add_argument("--build-cmd", default="cmake --build build --target halo",
                  help="Build command used when build stage runs.")

  ap.add_argument("--verify-input", default="",
                  help="Input JSON for tools/verify_lift.py.")
  ap.add_argument("--verify-auto", action="store_true",
                  help="Generate verify_lift input via tools/collect_verify_payload.py.")
  ap.add_argument("--verify-new-address", default="",
                  help="Lifted function address (required for --verify-auto).")
  ap.add_argument("--verify-output", default="",
                  help="Output path for generated verify payload (defaults to artifact dir).")

  ap.add_argument("--orig-decompile-file", default="",
                  help="Original decompile text file for --verify-auto.")
  ap.add_argument("--new-decompile-file", default="",
                  help="Lifted decompile text file for --verify-auto.")
  ap.add_argument("--orig-decompile-cmd", default="",
                  help="Shell command producing original decompile text for --verify-auto.")
  ap.add_argument("--new-decompile-cmd", default="",
                  help="Shell command producing lifted decompile text for --verify-auto.")
  ap.add_argument("--orig-callees-file", default="",
                  help="Optional original callee list file for --verify-auto.")
  ap.add_argument("--new-callees-file", default="",
                  help="Optional lifted callee list file for --verify-auto.")
  ap.add_argument("--orig-callees-cmd", default="",
                  help="Optional shell command producing original callees for --verify-auto.")
  ap.add_argument("--new-callees-cmd", default="",
                  help="Optional shell command producing lifted callees for --verify-auto.")

  ap.add_argument("--objdiff-reference", default="",
                  help="Reference object for tools/objdiff_lift.py.")
  ap.add_argument("--objdiff-candidate", default="",
                  help="Candidate object for tools/objdiff_lift.py.")
  ap.add_argument("--objdiff-tool", default="",
                  help="Optional objdiff executable path/name.")

  ap.add_argument("--with-runtime", action="store_true",
                  help="Enable runtime hash comparison via tools/boot_hash.sh.")
  ap.add_argument("--runtime-patched-iso", default="halo-patched/halo-patched.iso",
                  help="ISO path used for patched runtime hash.")
  ap.add_argument("--runtime-baseline-iso", default="",
                  help="Optional ISO path for baseline runtime hash.")
  ap.add_argument("--runtime-baseline-hash", default="",
                  help="Optional pre-recorded baseline hash.")
  ap.add_argument("--runtime-addr", default="0x500000",
                  help="Runtime hash memory address.")
  ap.add_argument("--runtime-nwords", type=int, default=256,
                  help="Runtime hash word count.")
  ap.add_argument("--runtime-wait", type=int, default=8,
                  help="Runtime hash wait seconds.")

  ap.add_argument("--no-metadata-update", action="store_true",
                  help="Do not update kb_meta status.")
  return ap


def main() -> int:
  args = build_parser().parse_args()
  try:
    return run_pipeline(args)
  except Exception as exc:
    print(f"lift_pipeline: FAIL ({exc})", file=sys.stderr)
    return 1


if __name__ == "__main__":
  raise SystemExit(main())
