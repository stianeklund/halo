#!/usr/bin/env python3
"""Build a verify_lift input JSON from decompile/callee sources."""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent


def run_shell(cmd: str) -> str:
  proc = subprocess.run(["bash", "-lc", cmd], cwd=str(ROOT), capture_output=True, text=True)
  if proc.returncode != 0:
    raise RuntimeError(
      f"command failed ({proc.returncode}): {cmd}\n"
      f"stdout:\n{proc.stdout}\n"
      f"stderr:\n{proc.stderr}"
    )
  return proc.stdout


def read_text(*, text: str, file_path: str, cmd: str, label: str, required: bool) -> str:
  present = [bool(text), bool(file_path), bool(cmd)]
  if sum(1 for v in present if v) > 1:
    raise RuntimeError(f"{label}: pass only one of --*-text, --*-file, --*-cmd")

  if text:
    return text
  if file_path:
    path = Path(file_path)
    if not path.is_absolute():
      path = ROOT / path
    if not path.exists():
      raise RuntimeError(f"{label}: file does not exist: {path}")
    return path.read_text(encoding="utf-8", errors="replace")
  if cmd:
    return run_shell(cmd)

  if required:
    raise RuntimeError(f"{label}: missing input")
  return ""


def normalize_callee_name(name: str) -> str:
  return name.strip()


def parse_callees(raw: str) -> list[str]:
  raw = raw.strip()
  if not raw:
    return []

  try:
    payload = json.loads(raw)
    if isinstance(payload, list):
      out = [normalize_callee_name(str(v)) for v in payload if str(v).strip()]
      return dedupe_keep_order(out)
    if isinstance(payload, dict):
      for key in ("callees", "functions", "names", "result"):
        value = payload.get(key)
        if isinstance(value, list):
          out = [normalize_callee_name(str(v)) for v in value if str(v).strip()]
          return dedupe_keep_order(out)
  except Exception:
    pass

  out: list[str] = []
  name_json_re = re.compile(r'"name"\s*:\s*"([^"]+)"')
  name_line_re = re.compile(r"^(?:[-*]|\d+[.)])?\s*(?:0x[0-9a-fA-F]+[:\s-]+)?([A-Za-z_][A-Za-z0-9_@$]*)\s*$")
  last_token_re = re.compile(r"(thunk_[A-Za-z0-9_]+|FUN_[0-9a-fA-F]+|[A-Za-z_][A-Za-z0-9_@$]*)")

  for line in raw.splitlines():
    s = line.strip()
    if not s:
      continue

    m = name_json_re.search(s)
    if m:
      out.append(normalize_callee_name(m.group(1)))
      continue

    m = name_line_re.match(s)
    if m:
      out.append(normalize_callee_name(m.group(1)))
      continue

    tokens = last_token_re.findall(s)
    if tokens:
      out.append(normalize_callee_name(tokens[-1]))

  return dedupe_keep_order(out)


def dedupe_keep_order(items: list[str]) -> list[str]:
  out: list[str] = []
  seen: set[str] = set()
  for item in items:
    if item in seen:
      continue
    seen.add(item)
    out.append(item)
  return out


def parse_args() -> argparse.Namespace:
  ap = argparse.ArgumentParser(description="Create verify_lift payload JSON")
  ap.add_argument("--name", required=True)
  ap.add_argument("--orig-address", required=True)
  ap.add_argument("--new-address", required=True)
  ap.add_argument("--output", required=True)

  ap.add_argument("--orig-decompile-text", default="")
  ap.add_argument("--orig-decompile-file", default="")
  ap.add_argument("--orig-decompile-cmd", default="")

  ap.add_argument("--new-decompile-text", default="")
  ap.add_argument("--new-decompile-file", default="")
  ap.add_argument("--new-decompile-cmd", default="")

  ap.add_argument("--orig-callees-text", default="")
  ap.add_argument("--orig-callees-file", default="")
  ap.add_argument("--orig-callees-cmd", default="")

  ap.add_argument("--new-callees-text", default="")
  ap.add_argument("--new-callees-file", default="")
  ap.add_argument("--new-callees-cmd", default="")
  return ap.parse_args()


def main() -> int:
  args = parse_args()
  try:
    orig_decompile = read_text(
      text=args.orig_decompile_text,
      file_path=args.orig_decompile_file,
      cmd=args.orig_decompile_cmd,
      label="orig_decompile",
      required=True,
    )
    new_decompile = read_text(
      text=args.new_decompile_text,
      file_path=args.new_decompile_file,
      cmd=args.new_decompile_cmd,
      label="new_decompile",
      required=True,
    )
    orig_callees = parse_callees(
      read_text(
        text=args.orig_callees_text,
        file_path=args.orig_callees_file,
        cmd=args.orig_callees_cmd,
        label="orig_callees",
        required=False,
      )
    )
    new_callees = parse_callees(
      read_text(
        text=args.new_callees_text,
        file_path=args.new_callees_file,
        cmd=args.new_callees_cmd,
        label="new_callees",
        required=False,
      )
    )

    out = {
      "name": args.name,
      "orig_address": args.orig_address,
      "new_address": args.new_address,
      "orig_decompile": orig_decompile,
      "new_decompile": new_decompile,
      "orig_callees": orig_callees,
      "new_callees": new_callees,
    }
    output = Path(args.output)
    if not output.is_absolute():
      output = ROOT / output
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(out, indent=2) + "\n", encoding="utf-8")
    print(str(output))
    return 0
  except Exception as exc:
    print(f"collect_verify_payload: FAIL ({exc})", file=sys.stderr)
    return 1


if __name__ == "__main__":
  raise SystemExit(main())
