#!/usr/bin/env python3
"""Choose the next lift action from current, fingerprinted evidence.

This deliberately contains no provider/model policy.  It says *what kind of
work is justified*; the caller can choose any currently available provider for
that route.  Score-context files must be from the current candidate attempt.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


STRUCTURAL_CEILING_RULES = {
    "regarg_structural_ceiling",
    "regarg_static_helper_ceiling",
}


def _pipeline_failures(summary: dict[str, Any] | None) -> list[str]:
    """Return failing evidence stages from a lift_pipeline JSON summary."""
    failures = []
    for stage in (summary or {}).get("stages", []):
        if stage.get("ran") and not stage.get("ok"):
            failures.append(str(stage.get("name", "unknown")))
    return failures


def route_attempt(score: float | None, score_context: dict[str, Any] | None,
                  pipeline_summary: dict[str, Any] | None = None) -> dict[str, Any]:
    """Return one deterministic next action and the evidence that selected it."""
    failures = _pipeline_failures(pipeline_summary)
    if failures:
        safety = {"build", "hazard_scan", "abi_audit", "frame_map"}
        if any(name in safety for name in failures):
            return {"route": "repair_gate_failure", "reason": "failed pipeline gate",
                    "evidence": failures}

    scores = (score_context or {}).get("scores", {})
    if score is None:
        score = scores.get("official_pct")
    if score is None:
        return {"route": "measure", "reason": "no current VC71 score",
                "evidence": []}

    rules = (score_context or {}).get("classification", [])
    rule_ids = [str(item.get("rule", "")) for item in rules if item.get("rule")]
    non_ceiling = [rule for rule in rule_ids
                   if rule not in STRUCTURAL_CEILING_RULES and rule != "anchor_collapse"]

    if score < 65.0:
        return {"route": "semantic_relift", "reason": "score below semantic floor",
                "score": score, "evidence": rule_ids}
    if score < 85.0:
        if non_ceiling:
            return {"route": "atlas_fix", "reason": "classified source-controllable gap",
                    "score": score, "evidence": non_ceiling}
        if any(rule in STRUCTURAL_CEILING_RULES for rule in rule_ids):
            return {"route": "park_structural", "reason": "known register-call ceiling",
                    "score": score, "evidence": rule_ids}
        return {"route": "codegen_optimize", "reason": "unclassified structural gap",
                "score": score, "evidence": rule_ids}
    if score < 98.0:
        return {"route": "permute", "reason": "permuter-eligible byte-tuning band",
                "score": score, "evidence": rule_ids}
    return {"route": "behavior_validate", "reason": "byte score is sufficient; validate behavior",
            "score": score, "evidence": rule_ids}


def _load_json(path: str) -> dict[str, Any]:
    if not path:
        return {}
    value = json.loads(Path(path).read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ValueError(f"JSON object required: {path}")
    return value


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--score", type=float,
                        help="Current VC71 official score; defaults to score-context data.")
    parser.add_argument("--score-context", default="", metavar="JSON",
                        help="Fingerprint-matched VC71 score-context artifact.")
    parser.add_argument("--pipeline-summary", default="", metavar="JSON",
                        help="Optional lift_pipeline summary to prioritize failed safety gates.")
    parser.add_argument("--json", action="store_true", help="Emit compact JSON.")
    args = parser.parse_args(argv)
    result = route_attempt(args.score, _load_json(args.score_context),
                           _load_json(args.pipeline_summary))
    print(json.dumps(result, sort_keys=True) if args.json else json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
