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
  decl: str
  object_name: str
  source_path: str


@dataclass
class StageResult:
  name: str
  ran: bool
  ok: bool
  details: str


@dataclass
class VerifyRiskAssessment:
  score: int
  threshold: int
  must_verify: bool
  triggers: list[str]
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
      target = Target(addr=addr, name=name, decl=decl, object_name=object_name, source_path=source_path)
      by_name[name] = target
      by_addr[addr] = target
  return by_name, by_addr


def pick_target_from_frontier(frontier_limit: int, object_filter: str, artifact_dir: Path) -> Optional[tuple[str, str]]:
  log_path = artifact_dir / "frontier.log"
  proc = run_command(
    ["python3", "tools/analysis/frontier.py", "--limit", str(frontier_limit)],
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


def parse_match_percent(text: str) -> Optional[float]:
  m = re.search(r'(\d+\.\d+)% match', text)
  if not m:
    m = re.search(r'match:\s*(\d+\.\d+)%', text)
  return float(m.group(1)) if m else None


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


def resolve_root_path(path_text: str) -> Path:
  path = Path(path_text)
  if not path.is_absolute():
    path = ROOT / path
  return path


def find_matching_delimiter(text: str, start: int, open_char: str, close_char: str) -> int:
  depth = 0
  in_line_comment = False
  in_block_comment = False
  in_string: Optional[str] = None
  escape = False

  i = start
  while i < len(text):
    ch = text[i]
    nxt = text[i + 1] if i + 1 < len(text) else ""

    if in_line_comment:
      if ch == "\n":
        in_line_comment = False
      i += 1
      continue

    if in_block_comment:
      if ch == "*" and nxt == "/":
        in_block_comment = False
        i += 2
      else:
        i += 1
      continue

    if in_string is not None:
      if escape:
        escape = False
      elif ch == "\\":
        escape = True
      elif ch == in_string:
        in_string = None
      i += 1
      continue

    if ch == "/" and nxt == "/":
      in_line_comment = True
      i += 2
      continue

    if ch == "/" and nxt == "*":
      in_block_comment = True
      i += 2
      continue

    if ch == '"' or ch == "'":
      in_string = ch
      i += 1
      continue

    if ch == open_char:
      depth += 1
    elif ch == close_char:
      depth -= 1
      if depth == 0:
        return i

    i += 1

  return -1


def extract_function_body(source_text: str, function_name: str) -> Optional[str]:
  pattern = re.compile(rf"\b{re.escape(function_name)}\s*\(")
  for match in pattern.finditer(source_text):
    open_paren = source_text.find("(", match.start())
    if open_paren < 0:
      continue
    close_paren = find_matching_delimiter(source_text, open_paren, "(", ")")
    if close_paren < 0:
      continue

    search_start = close_paren + 1
    next_open = source_text.find("{", search_start)
    next_semi = source_text.find(";", search_start)
    if next_open < 0:
      continue
    if next_semi >= 0 and next_semi < next_open:
      continue

    close_brace = find_matching_delimiter(source_text, next_open, "{", "}")
    if close_brace < 0:
      continue

    return source_text[next_open:close_brace + 1]

  return None


def strip_c_comments_and_literals(text: str) -> str:
  without_block_comments = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
  without_line_comments = re.sub(r"//[^\n]*", " ", without_block_comments)
  without_strings = re.sub(r'"(?:\\.|[^"\\])*"', " ", without_line_comments)
  without_chars = re.sub(r"'(?:\\.|[^'\\])'", " ", without_strings)
  return without_chars


def assess_verify_risk(target: Target, threshold: int) -> VerifyRiskAssessment:
  score = 0
  triggers: list[str] = []

  source_path = resolve_root_path(target.source_path) if target.source_path else None
  if not source_path or not source_path.exists():
    score += 3
    triggers.append("source-unavailable")
    details = f"score={score} threshold={threshold} source=unavailable"
    return VerifyRiskAssessment(
      score=score,
      threshold=threshold,
      must_verify=True,
      triggers=triggers,
      details=details,
    )

  source_text = source_path.read_text(encoding="utf-8", errors="replace")
  body = extract_function_body(source_text, target.name)
  if body is None:
    score += 3
    triggers.append("function-body-unavailable")
    details = f"score={score} threshold={threshold} source={target.source_path} body=unavailable"
    return VerifyRiskAssessment(
      score=score,
      threshold=threshold,
      must_verify=True,
      triggers=triggers,
      details=details,
    )

  cleaned = strip_c_comments_and_literals(body)

  branch_count = len(re.findall(r"\b(?:if|for|while|switch|goto)\b", cleaned))
  has_loop_or_switch = bool(re.search(r"\b(?:for|while|switch)\b", cleaned))

  call_names = []
  call_keywords = {
    "if", "for", "while", "switch", "return", "sizeof", "typeof", "alignof", "do", "else", "case",
  }
  for m in re.finditer(r"\b([A-Za-z_][A-Za-z0-9_]*)\s*\(", cleaned):
    callee = m.group(1)
    if callee in call_keywords:
      continue
    call_names.append(callee)
  call_count = len(call_names)

  has_pointer_write = bool(re.search(r"\*\s*[A-Za-z_][A-Za-z0-9_]*(?:\s*[+\-]\s*[^=;\n]+)?\s*=", cleaned))
  has_index_write = bool(re.search(r"[A-Za-z_][A-Za-z0-9_]*\s*\[[^\]]+\]\s*=", cleaned))
  has_offset_access = bool(
    re.search(r"\b[A-Za-z_][A-Za-z0-9_]*\s*[+\-]\s*0x[0-9A-Fa-f]+\b", cleaned)
    or re.search(r"->\s*(?:field|pad|unknown|unk)_[0-9A-Fa-fA-F]+", cleaned)
  )
  has_hardcoded_address = bool(re.search(r"0x[0-9A-Fa-f]{5,}", cleaned))
  has_reg_args = "@<" in target.decl

  if branch_count >= 2:
    score += 2
    triggers.append(f"branches={branch_count}")
  if has_loop_or_switch:
    score += 2
    triggers.append("loop-or-switch")
  if call_count >= 2:
    score += 1
    triggers.append(f"calls={call_count}")
  if has_pointer_write or has_index_write:
    score += 3
    triggers.append("pointer-write")
  if has_offset_access:
    score += 2
    triggers.append("offset-access")
  if has_reg_args:
    score += 2
    triggers.append("reg-args")
  if has_hardcoded_address:
    score += 1
    triggers.append("hardcoded-address")

  strong_trigger = has_pointer_write or has_index_write or has_offset_access or has_reg_args
  must_verify = strong_trigger or (score >= threshold)
  details = (
    f"score={score} threshold={threshold} branches={branch_count} calls={call_count} "
    f"loop_switch={'yes' if has_loop_or_switch else 'no'} "
    f"ptr_write={'yes' if (has_pointer_write or has_index_write) else 'no'} "
    f"offset={'yes' if has_offset_access else 'no'} "
    f"reg_args={'yes' if has_reg_args else 'no'}"
  )

  return VerifyRiskAssessment(
    score=score,
    threshold=threshold,
    must_verify=must_verify,
    triggers=triggers,
    details=details,
  )


def can_auto_generate_verify_payload(args: argparse.Namespace, artifact_dir: Path) -> tuple[bool, list[str]]:
  missing: list[str] = []

  if not args.verify_new_address:
    missing.append("--verify-new-address")

  def has_input(file_arg: str, cmd_arg: str, default_name: str) -> bool:
    if file_arg:
      return resolve_root_path(file_arg).exists()
    if cmd_arg:
      return True
    return (artifact_dir / default_name).exists()

  if not has_input(args.orig_decompile_file, args.orig_decompile_cmd, "orig_decompile.txt"):
    missing.append("orig_decompile input")

  if not has_input(args.new_decompile_file, args.new_decompile_cmd, "new_decompile.txt"):
    missing.append("new_decompile input")

  return (len(missing) == 0), missing


def run_pipeline(args: argparse.Namespace) -> int:
  if not (0.0 <= args.low_match_reject_below <= args.low_match_behavior_both_below <= args.low_match_threshold <= 100.0):
    raise RuntimeError(
      "invalid low-match thresholds (expected 0 <= reject <= behavior_both <= threshold <= 100)"
    )

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
      return finalize(summary, stages, artifact_dir, ok=False, quiet=args.quiet)
  else:
    stages.append(StageResult("ghidra_extract", ran=False, ok=True,
                              details="skipped (no --extract-cmd)"))

  abi_cmd = ["python3", "tools/audit/audit_reg_abi.py", "--target", target.name]
  abi_source = "kb.json only"
  if args.abi_caller_disasm_file:
    abi_cmd.extend(["--caller-disasm-file", args.abi_caller_disasm_file])
    abi_source = args.abi_caller_disasm_file
  elif args.abi_caller_disasm_cmd:
    abi_cmd.extend([
      "--caller-disasm-cmd",
      render_template(args.abi_caller_disasm_cmd, target=target, artifact_dir=artifact_dir),
    ])
    abi_source = "command"
  else:
    default_caller_disasm = artifact_dir / "caller_disasm.txt"
    if default_caller_disasm.exists():
      abi_cmd.extend(["--caller-disasm-file", str(default_caller_disasm)])
      abi_source = str(default_caller_disasm)
  if args.abi_strict_callers:
    abi_cmd.append("--strict-callers")

  proc = run_command(abi_cmd, cwd=ROOT, log_path=artifact_dir / "abi_audit.log")
  ok = proc.returncode == 0
  details = f"source={abi_source}"
  if proc.stdout:
    first_line = proc.stdout.strip().splitlines()[0]
    details += f" {first_line}"
  stages.append(StageResult("abi_audit", ran=True, ok=ok, details=details))
  if not ok:
    return finalize(summary, stages, artifact_dir, ok=False, quiet=args.quiet)

  if args.candidate:
    source = args.source if args.source else target.source_path
    if not source:
      stages.append(StageResult("source_update", ran=True, ok=False,
                                details="no source path resolved; pass --source"))
      return finalize(summary, stages, artifact_dir, ok=False, quiet=args.quiet)

    cmd = [
      "python3",
      "tools/analysis/maintain.py",
      "--update-from",
      args.candidate,
      source,
    ]
    proc = run_command(cmd, cwd=ROOT, log_path=artifact_dir / "source_update.log")
    stages.append(StageResult("source_update", ran=True, ok=proc.returncode == 0,
                              details=f"candidate={args.candidate} source={source}"))
    if proc.returncode != 0:
      return finalize(summary, stages, artifact_dir, ok=False, quiet=args.quiet)
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
      return finalize(summary, stages, artifact_dir, ok=False, quiet=args.quiet)

  risk_assessment = assess_verify_risk(target, args.verify_risk_threshold)
  trigger_text = ",".join(risk_assessment.triggers) if risk_assessment.triggers else "none"

  manual_verify_requested = bool(args.verify_input) or bool(args.verify_auto)
  policy_requires_verify = False
  verify_auto_from_policy = False
  policy_missing_inputs: list[str] = []
  policy_decision = "manual-only"

  if args.verify_policy == "manual":
    policy_decision = "manual-only"
  elif manual_verify_requested:
    policy_decision = "manual-requested"
  elif risk_assessment.must_verify and not manual_verify_requested:
    policy_requires_verify = True
    can_auto, missing_inputs = can_auto_generate_verify_payload(args, artifact_dir)
    if can_auto:
      verify_auto_from_policy = True
      policy_decision = "auto-verify"
    else:
      policy_missing_inputs = missing_inputs
      policy_decision = "verify-recommended-but-inputs-missing"
  else:
    policy_decision = "verify-not-required"

  verify_policy_details = (
    f"mode={args.verify_policy} decision={policy_decision} {risk_assessment.details} "
    f"triggers={trigger_text}"
  )
  if policy_missing_inputs:
    verify_policy_details += f" missing={','.join(policy_missing_inputs)}"

  if args.verify_policy == "strict" and policy_requires_verify and policy_missing_inputs:
    stages.append(StageResult("verify_policy", ran=True, ok=False, details=verify_policy_details))
    return finalize(summary, stages, artifact_dir, ok=False, quiet=args.quiet)

  stages.append(StageResult("verify_policy", ran=True, ok=True, details=verify_policy_details))

  verify_input_path = args.verify_input
  verify_auto_requested = bool(args.verify_auto or verify_auto_from_policy)

  if not verify_input_path and verify_auto_requested:
    if not args.verify_new_address:
      if verify_auto_from_policy:
        stages.append(StageResult("verify_payload", ran=False, ok=True,
                                  details="skipped (policy requested verify but --verify-new-address is missing)"))
      else:
        stages.append(StageResult("verify_payload", ran=True, ok=False,
                                  details="--verify-new-address is required with --verify-auto"))
        return finalize(summary, stages, artifact_dir, ok=False, quiet=args.quiet)
    else:
      verify_input_path = args.verify_output or str(artifact_dir / "verify_payload.json")
      collect_cmd = [
        "python3",
        "tools/verify/collect_verify_payload.py",
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
        return finalize(summary, stages, artifact_dir, ok=False, quiet=args.quiet)
  else:
    if verify_input_path:
      details = "skipped (verify payload generation not needed: --verify-input provided)"
    elif policy_requires_verify and policy_missing_inputs:
      details = "skipped (verify requested by policy but inputs are missing)"
    else:
      details = "skipped (verify not requested)"
    stages.append(StageResult("verify_payload", ran=False, ok=True, details=details))

  structural_enabled = bool(verify_input_path) or bool(args.objdiff_reference and args.objdiff_candidate)
  structural_ok = True

  if verify_input_path:
    proc = run_command(
      ["python3", "tools/verify/verify_lift.py", verify_input_path],
      cwd=ROOT,
      log_path=artifact_dir / "verify_lift.log",
    )
    ok = proc.returncode == 0
    structural_ok = structural_ok and ok
    stages.append(StageResult("verify_lift", ran=True, ok=ok, details=verify_input_path))
  else:
    stages.append(StageResult("verify_lift", ran=False, ok=True,
                              details="skipped (no verify payload)"))

  objdiff_match_pct: Optional[float] = None

  if args.objdiff_reference and args.objdiff_candidate:
    cmd = [
      "python3",
      "tools/verify/objdiff_lift.py",
      "--target",
      target.name,
      "--reference",
      args.objdiff_reference,
      "--candidate",
      args.objdiff_candidate,
      "--symbol",
      target.name,
    ]
    if args.objdiff_tool:
      cmd.extend(["--tool", args.objdiff_tool])
    proc = run_command(cmd, cwd=ROOT, log_path=artifact_dir / "objdiff.log")
    objdiff_output = (proc.stdout or "") + (proc.stderr or "")
    objdiff_match_pct = parse_match_percent(objdiff_output)
    ok = proc.returncode == 0
    structural_ok = structural_ok and ok
    detail = f"{args.objdiff_reference} vs {args.objdiff_candidate}"
    if objdiff_match_pct is not None:
      detail += f" ({objdiff_match_pct:.1f}%)"
    stages.append(StageResult("objdiff", ran=True, ok=ok, details=detail))
  else:
    stages.append(StageResult("objdiff", ran=False, ok=True,
                              details="skipped (no --objdiff-reference/--objdiff-candidate)"))

  vc71_verify_ran = False
  vc71_verify_ok = False
  vc71_match_pct: Optional[float] = None
  vc71_has_fpu_warn = False

  if not args.skip_vc71_verify and build_ok:
    vc71_source = Path(target.source_path) if target.source_path else None
    vc71_ref = None
    if vc71_source:
      # First: try TU-level delinked ref via objdiff.json
      try:
        with open(ROOT / "objdiff.json") as f:
          objdiff_cfg = json.load(f)
        for u in objdiff_cfg.get("units", []):
          src = u.get("metadata", {}).get("source_path", "")
          if src and str(vc71_source).endswith(src):
            ref_path = ROOT / u.get("base_path", "")
            if ref_path.exists():
              vc71_ref = ref_path
            break
      except (FileNotFoundError, json.JSONDecodeError):
        pass
      # Fallback: per-function delinked ref for split TUs
      if not vc71_ref and target.addr:
        try:
          addr_hex = f"{int(target.addr, 16):08x}"
          per_func = ROOT / "delinked" / "functions" / f"{addr_hex}.obj"
          if per_func.exists():
            vc71_ref = per_func
        except (ValueError, TypeError):
          pass

    if vc71_source and vc71_ref and (ROOT / vc71_source).exists():
      vc71_verify_ran = True
      cmd = [
        "python3", "tools/verify/vc71_verify.py",
        str(vc71_source), "--function", target.name,
        "--show-diffs", "--threshold", "0",
      ]
      proc = run_command(cmd, cwd=ROOT, log_path=artifact_dir / "vc71_verify.log")
      output = (proc.stdout or "") + (proc.stderr or "")
      vc71_has_fpu_warn = "FPU-WARN" in output
      vc71_match_pct = parse_match_percent(output)
      vc71_verify_ok = proc.returncode == 0
      if proc.returncode == 0:
        details = f"{vc71_match_pct:.1f}% match" if vc71_match_pct is not None else "PASS"
      elif vc71_has_fpu_warn:
        details = f"{vc71_match_pct:.1f}% match, FPU operand-order warnings" if vc71_match_pct else "FPU warnings"
      else:
        details = "VC71 compilation or comparison failed"
      stages.append(StageResult("vc71_verify", ran=True, ok=vc71_verify_ok,
                                details=details + (" [REVIEW FPU-WARN]" if vc71_has_fpu_warn else "")))
    else:
      if vc71_source:
        reason = "no delinked reference — run: python3 tools/audit/batch_delink.py --per-function-only"
      else:
        reason = "no source_path"
      stages.append(StageResult("vc71_verify", ran=False, ok=True,
                                details=f"skipped ({reason})"))
  else:
    stages.append(StageResult("vc71_verify", ran=False, ok=True,
                              details="skipped" + (" (--skip-vc71-verify)" if args.skip_vc71_verify else "")))

  # Optional last-mile permuter pass (informational; never auto-applies).
  if args.permute:
    if vc71_match_pct is not None and 85.0 <= vc71_match_pct < 99.0:
      cmd = [
        "python3", "tools/permuter/run.py", target.name,
        "--time", str(args.permute_time),
      ]
      proc = run_command(cmd, cwd=ROOT, log_path=artifact_dir / "permute.log")
      best = parse_match_percent((proc.stdout or "") + (proc.stderr or ""))
      details = (f"vc71={vc71_match_pct:.1f}% best_perm={best:.1f}%"
                 if best is not None else f"vc71={vc71_match_pct:.1f}% no improvement")
      stages.append(StageResult("permute", ran=True, ok=proc.returncode == 0,
                                details=details))
    else:
      reason = ("vc71 match unknown" if vc71_match_pct is None
                else f"vc71 match {vc71_match_pct:.1f}% out of [85, 99) band")
      stages.append(StageResult("permute", ran=False, ok=True,
                                details=f"skipped ({reason})"))
  else:
    stages.append(StageResult("permute", ran=False, ok=True,
                              details="skipped (--permute not set)"))

  # Unicorn-Engine differential test. In auto mode this is a behavioral signal
  # only when the target is applicable; skips do not count as proof.
  equivalence_ok = False
  equivalence_policy = "auto" if args.equivalence else args.equivalence_policy
  if equivalence_policy != "off":
    equivalence_json = artifact_dir / "equivalence.json"
    cmd = [
      "python3", "tools/equivalence/unicorn_diff.py", target.name,
      "--seeds", str(args.equivalence_seeds),
      "--output-json", str(equivalence_json),
      "--no-leaf-cache",
      "--z3-equiv",
      "--allow-stubs",
    ]
    proc = run_command(cmd, cwd=ROOT, log_path=artifact_dir / "equivalence.log")
    output = (proc.stdout or "") + (proc.stderr or "")
    payload: dict[str, object] | None = None
    if equivalence_json.exists():
      try:
        payload = json.loads(equivalence_json.read_text(encoding="utf-8"))
        summary["equivalence"] = payload
      except json.JSONDecodeError:
        payload = None

    status = str(payload.get("status", "")) if payload else ""
    reason = str(payload.get("reason", "")) if payload and payload.get("reason") else ""

    if status == "pass":
      equivalence_ok = True
      z3_proven = bool(payload.get("z3_proven")) if payload else False
      passed = int(payload.get("passed", 0)) if payload else 0
      failed = int(payload.get("failed", 0)) if payload else 0
      errors = int(payload.get("errors", 0)) if payload else 0
      seeds = int(payload.get("seeds", args.equivalence_seeds)) if payload else args.equivalence_seeds
      if z3_proven:
        details = "Z3 PROVEN EQUIVALENT (formal proof, all inputs)"
      else:
        details = f"{passed} passed, {failed} diverged, {errors} errors / {seeds} seeds"
      stages.append(StageResult("equivalence", ran=True, ok=True, details=details))
    elif status == "fail":
      passed = int(payload.get("passed", 0)) if payload else 0
      failed = int(payload.get("failed", 0)) if payload else 0
      errors = int(payload.get("errors", 0)) if payload else 0
      seeds = int(payload.get("seeds", args.equivalence_seeds)) if payload else args.equivalence_seeds
      details = f"{passed} passed, {failed} diverged, {errors} errors / {seeds} seeds"
      if reason:
        details += f" reason={reason}"
      stages.append(StageResult("equivalence", ran=True, ok=False, details=details))
      return finalize(summary, stages, artifact_dir, ok=False, quiet=args.quiet)
    elif status == "not_applicable":
      ok = equivalence_policy != "required"
      details = f"skipped ({reason or 'not_applicable'})"
      stages.append(StageResult("equivalence", ran=False, ok=ok, details=details))
      if not ok:
        return finalize(summary, stages, artifact_dir, ok=False, quiet=args.quiet)
    else:
      if "external relocations" in output.lower() or "not a pure leaf" in output.lower():
        ok = equivalence_policy != "required"
        stages.append(StageResult("equivalence", ran=False, ok=ok,
                                  details="skipped (external relocations)"))
        if not ok:
          return finalize(summary, stages, artifact_dir, ok=False, quiet=args.quiet)
      else:
        details = f"error ({reason or 'no structured result'}; see equivalence.log)"
        stages.append(StageResult("equivalence", ran=True, ok=False, details=details))
        return finalize(summary, stages, artifact_dir, ok=False, quiet=args.quiet)
  else:
    stages.append(StageResult("equivalence", ran=False, ok=True,
                              details="skipped (--equivalence-policy=off)"))

  behavior_check_ok = False
  if args.behavior_check_cmd:
    behavior_cmd = render_template(args.behavior_check_cmd, target=target, artifact_dir=artifact_dir)
    proc = run_command(
      ["bash", "-lc", behavior_cmd],
      cwd=ROOT,
      log_path=artifact_dir / "behavior_check.log",
    )
    behavior_check_ok = proc.returncode == 0
    stages.append(StageResult("behavior_check", ran=True, ok=behavior_check_ok, details=behavior_cmd))
  else:
    stages.append(StageResult("behavior_check", ran=False, ok=True,
                              details="skipped (no --behavior-check-cmd)"))

  runtime_enabled = args.with_runtime
  runtime_ok = True
  if runtime_enabled:
    patched_iso = args.runtime_patched_iso
    boot_args = ["bash", "tools/xbox/boot_hash.sh"]
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

  if args.low_match_policy == "off":
    stages.append(StageResult("low_match_policy", ran=False, ok=True,
                              details="skipped (--low-match-policy=off)"))
  else:
    # Use the best available structural match for policy decisions.
    best_match_pct = vc71_match_pct
    if objdiff_match_pct is not None:
      if best_match_pct is None or objdiff_match_pct > best_match_pct:
        best_match_pct = objdiff_match_pct
    elif (not vc71_verify_ran) and (vc71_match_pct is None):
      if args.low_match_policy == "strict":
        stages.append(StageResult("low_match_policy", ran=True, ok=False,
                                  details="strict mode requires structural verify data (all skipped)"))
        return finalize(summary, stages, artifact_dir, ok=False, quiet=args.quiet)
      stages.append(StageResult("low_match_policy", ran=False, ok=True,
                                details="skipped (no verify data)"))
      return finalize(summary, stages, artifact_dir, ok=structural_ok, quiet=args.quiet)

    if (vc71_verify_ran and not vc71_verify_ok) and (objdiff_match_pct is None) and (vc71_match_pct is None):
      stages.append(StageResult("low_match_policy", ran=True, ok=False,
                                details="vc71_verify failed; low-match policy cannot evaluate"))
      return finalize(summary, stages, artifact_dir, ok=False, quiet=args.quiet)
    elif best_match_pct is None:
      stages.append(StageResult("low_match_policy", ran=True, ok=False,
                                details="verify output missing match percentage"))
      return finalize(summary, stages, artifact_dir, ok=False, quiet=args.quiet)
    else:
      behavior_any_ok = equivalence_ok or behavior_check_ok or (runtime_enabled and runtime_ok)
      behavior_both_ok = equivalence_ok or (behavior_check_ok and runtime_enabled and runtime_ok)

      match_source = "objdiff" if (objdiff_match_pct is not None and (vc71_match_pct is None or objdiff_match_pct >= vc71_match_pct)) else "vc71"
      details = [
        f"policy={args.low_match_policy}",
        f"match={best_match_pct:.1f}%",
        f"source={match_source}",
        f"threshold={args.low_match_threshold:.1f}%",
        f"behavior_both_below={args.low_match_behavior_both_below:.1f}%",
        f"reject_below={args.low_match_reject_below:.1f}%",
        f"fpu_warn={'yes' if vc71_has_fpu_warn else 'no'}",
        f"equivalence={'pass' if equivalence_ok else ('n/a' if equivalence_policy == 'off' else 'skip')}",
        f"behavior_check={'pass' if behavior_check_ok else ('n/a' if not args.behavior_check_cmd else 'fail')}",
        f"runtime_check={'pass' if (runtime_enabled and runtime_ok) else ('n/a' if not runtime_enabled else 'fail')}",
      ]

      policy_ok = True
      reason = "accepted"

      if vc71_has_fpu_warn and not equivalence_ok and (match_source == "vc71" or objdiff_match_pct is None):
        policy_ok = False
        reason = "FPU operand-order warnings present"
      elif best_match_pct < args.low_match_reject_below:
        policy_ok = False
        reason = f"match below hard floor ({args.low_match_reject_below:.1f}%)"
      elif best_match_pct < args.low_match_behavior_both_below:
        if not behavior_both_ok:
          policy_ok = False
          reason = "strict low-match range requires Unicorn equivalence PASS or both behavior_check and runtime_check PASS"
      elif best_match_pct < args.low_match_threshold:
        if not behavior_any_ok:
          policy_ok = False
          reason = "low match requires at least one behavior signal (equivalence, behavior_check, or runtime_check)"

      details.append(f"verdict={'PASS' if policy_ok else 'FAIL'}")
      details.append(f"reason={reason}")
      stages.append(StageResult("low_match_policy", ran=True, ok=policy_ok, details=" ".join(details)))
      if not policy_ok:
        return finalize(summary, stages, artifact_dir, ok=False, quiet=args.quiet)

  if args.no_metadata_update:
    stages.append(StageResult("metadata_update", ran=False, ok=True,
                              details="skipped (--no-metadata-update)"))
  else:
    md_ok = True
    md_notes: list[str] = []
    if build_ok:
      proc = run_command(
        ["python3", "tools/analysis/kb_meta.py", "set-status", "--addr", target.addr, "--status", "ported"],
        cwd=ROOT,
        log_path=artifact_dir / "metadata_ported.log",
      )
      md_ok = md_ok and (proc.returncode == 0)
      md_notes.append("ported")

    verification_enabled = structural_enabled or runtime_enabled
    if build_ok and verification_enabled and structural_ok and runtime_ok:
      proc = run_command(
        ["python3", "tools/analysis/kb_meta.py", "set-status", "--addr", target.addr, "--status", "verified"],
        cwd=ROOT,
        log_path=artifact_dir / "metadata_verified.log",
      )
      md_ok = md_ok and (proc.returncode == 0)
      md_notes.append("verified")

    stages.append(StageResult("metadata_update", ran=True, ok=md_ok,
                              details=", ".join(md_notes) if md_notes else "no status change"))

  overall_ok = all(s.ok for s in stages if s.ran)
  return finalize(summary, stages, artifact_dir, ok=overall_ok, quiet=args.quiet)


def _quiet_details(name: str, details: str) -> str:
  if name == "low_match_policy":
    parts = details.split()
    keep = {}
    for p in parts:
      if p.startswith("match=") or p.startswith("verdict="):
        k, v = p.split("=", 1)
        keep[k] = v
    if keep:
      return " ".join(f"{k}={v}" for k, v in keep.items())
  return details


def finalize(summary: dict[str, object], stages: list[StageResult], artifact_dir: Path, ok: bool,
             quiet: bool = False) -> int:
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
    if quiet and not s.ran:
      continue
    state = "PASS" if s.ok else "FAIL"
    run = "ran" if s.ran else "skip"
    det = _quiet_details(s.name, s.details) if quiet else s.details
    print(f"- {s.name:<16} [{run}] {state}: {det}")
  if not quiet:
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
  ap.add_argument("--build-cmd", default="python3 tools/build/build.py -q --target halo",
                  help="Build command used when build stage runs.")

  ap.add_argument("--abi-caller-disasm-file", default="",
                  help="Optional caller disassembly file for pre-lift register ABI audit.")
  ap.add_argument("--abi-caller-disasm-cmd", default="",
                  help="Optional command producing caller disassembly for ABI audit.")
  ap.add_argument("--abi-strict-callers", action="store_true",
                  help="Fail ABI audit if caller evidence misses expected register writes.")

  ap.add_argument("--verify-input", default="",
                  help="Input JSON for tools/verify/verify_lift.py.")
  ap.add_argument("--verify-auto", action="store_true",
                  help="Generate verify_lift input via tools/verify/collect_verify_payload.py.")
  ap.add_argument("--verify-new-address", default="",
                  help="Lifted function address (required for --verify-auto).")
  ap.add_argument("--verify-output", default="",
                  help="Output path for generated verify payload (defaults to artifact dir).")
  ap.add_argument("--verify-policy", choices=["manual", "auto", "strict"], default="auto",
                  help="Verification policy: manual=only explicit flags, auto=enable verify for risky functions when inputs exist, strict=fail if risky function cannot be verified.")
  ap.add_argument("--verify-risk-threshold", type=int, default=3,
                  help="Risk score threshold for policy-driven verify decisions.")

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
                  help="Reference object for tools/verify/objdiff_lift.py.")
  ap.add_argument("--objdiff-candidate", default="",
                  help="Candidate object for tools/verify/objdiff_lift.py.")
  ap.add_argument("--objdiff-tool", default="",
                  help="Optional objdiff executable path/name.")

  ap.add_argument("--skip-vc71-verify", action="store_true",
                  help="Skip VC++ 7.1 compilation and FPU operand comparison.")
  ap.add_argument("--behavior-check-cmd", default="",
                  help="Optional non-interactive behavior/reference check command (exit 0 = PASS).")
  ap.add_argument("--low-match-policy", choices=["off", "auto", "strict"], default="strict",
                  help="Enforce low-match acceptance: off=disabled, auto=enforce when VC71 data exists, strict=fail if VC71 data is missing (default).")
  ap.add_argument("--low-match-threshold", type=float, default=50.0,
                  help="Low-match threshold percentage (default: 50.0).")
  ap.add_argument("--low-match-behavior-both-below", type=float, default=40.0,
                  help="Below this match percentage, require both behavior_check and runtime_check to pass (default: 40.0).")
  ap.add_argument("--low-match-reject-below", type=float, default=25.0,
                  help="Hard reject below this match percentage (default: 25.0).")

  ap.add_argument("--with-runtime", action="store_true",
                  help="Enable runtime hash comparison via tools/xbox/boot_hash.sh.")
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

  ap.add_argument("--permute", action="store_true",
                  help="When VC71 match falls in [85, 98], spawn a permuter "
                       "pass via tools/permuter/run.py. Reports best score; "
                       "does NOT auto-apply permutations.")
  ap.add_argument("--permute-time", type=int, default=60,
                  help="Permuter time budget in seconds (default 60).")
  ap.add_argument("--equivalence-policy", choices=["off", "auto", "required"], default="auto",
                  help="Unicorn differential policy: off=disabled, auto=run when applicable "
                       "and treat skips as non-proof, required=fail if skipped (default: auto).")
  ap.add_argument("--equivalence", action="store_true",
                  help="Compatibility alias for --equivalence-policy=auto.")
  ap.add_argument("--equivalence-seeds", type=int, default=100,
                  help="Number of seeds for unicorn_diff (default 100).")
  ap.add_argument("-q", "--quiet", action="store_true",
                  help="Suppress skipped stages and condense detail strings in final output.")
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
