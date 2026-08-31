#!/usr/bin/env python3
"""Emit advisory cross-build evidence for one 2276 lift target.

The PDB/map-derived proposal corpus is useful context, not an address oracle.
This tool resolves a *2276* function, then reports only the proposal already
associated with that 2276 target by the importer's TU/order/size analysis.
It never changes source or kb.json, and every emitted candidate is labelled
INFERRED until confirmed against the 2276 binary.

Typical use is automatic through tools/lift_pipeline.py; it can also be run
directly while researching a target.
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parent.parent.parent
PROPOSALS = ROOT / "artifacts" / "punpckhdq_import" / "name_proposals.json"


def function_name(decl: str) -> str:
  """Extract the C identifier from a kb.json declaration."""
  return re.split(r"[\s*]+", decl.split("(", 1)[0].strip())[-1]


def load_targets() -> tuple[dict[str, dict[str, str]], dict[str, dict[str, str]]]:
  kb = json.loads((ROOT / "kb.json").read_text(encoding="utf-8"))
  by_addr: dict[str, dict[str, str]] = {}
  by_name: dict[str, dict[str, str]] = {}
  for obj in kb.get("objects", []):
    for fn in obj.get("functions", []):
      addr = str(fn.get("addr", "")).lower()
      name = function_name(str(fn.get("decl", "")))
      if not addr or not name:
        continue
      target = {
        "addr": addr,
        "name": name,
        "object": str(obj.get("name", "")),
        "source": str(fn.get("source_path", "") or obj.get("source", "")),
      }
      by_addr[addr] = target
      by_name[name] = target
  return by_addr, by_name


def resolve_target(token: str) -> dict[str, str]:
  by_addr, by_name = load_targets()
  target = by_addr.get(token.lower()) if token.lower().startswith("0x") else by_name.get(token)
  if not target:
    raise ValueError(f"target not found in kb.json: {token}")
  return target


def build_context(target: dict[str, str]) -> dict[str, Any]:
  context: dict[str, Any] = {
    "schema": 1,
    "target": target,
    "evidence_label": "INFERRED",
    "rule": (
      "Cross-build data supplies research context only. Confirm every name, "
      "prototype, call, and field against the 2276 binary before changing a lift."
    ),
  }
  if not PROPOSALS.exists():
    context["status"] = "unavailable"
    context["details"] = f"proposal corpus missing: {PROPOSALS.relative_to(ROOT)}"
    return context

  proposals = json.loads(PROPOSALS.read_text(encoding="utf-8"))
  matches = [p for p in proposals if str(p.get("our_addr", "")).lower() == target["addr"]]
  if not matches:
    context["status"] = "no_candidate"
    context["details"] = "no cross-build counterpart was proposed for this 2276 target"
    return context

  candidates = []
  for proposal in matches:
    candidates.append({
      "name": proposal.get("proposed_name", ""),
      "reference_source": proposal.get("their_source", ""),
      "reference_object": proposal.get("their_object", ""),
      "confidence": proposal.get("confidence", "low"),
      "real_name": bool(proposal.get("real_name", False)),
      "ordinal": proposal.get("ordinal"),
      "size_reference": proposal.get("size_their"),
      "size_2276": proposal.get("size_ours"),
    })
  context["candidates"] = candidates
  best = candidates[0]
  if len(candidates) == 1 and best["confidence"] == "high" and best["real_name"]:
    context["status"] = "high_confidence_candidate"
  elif len(candidates) == 1:
    context["status"] = "advisory_candidate"
  else:
    context["status"] = "ambiguous"
  context["required_2276_confirmation"] = [
    "Verify call sites and parameter order in 2276 disassembly.",
    "Verify the body/control flow against 2276 before transcribing reference code.",
    "Treat a conflicting existing 2276 name as a rejected correspondence.",
  ]
  return context


def main() -> int:
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument("--target", required=True, help="2276 function address or name")
  parser.add_argument("--output", type=Path, help="write JSON dossier to this path")
  args = parser.parse_args()

  try:
    context = build_context(resolve_target(args.target))
  except (OSError, ValueError, json.JSONDecodeError) as exc:
    context = {
      "schema": 1,
      "status": "unavailable",
      "evidence_label": "INFERRED",
      "details": str(exc),
    }

  rendered = json.dumps(context, indent=2, sort_keys=True)
  if args.output:
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(rendered + "\n", encoding="utf-8")
  print(rendered)
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
