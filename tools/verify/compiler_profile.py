#!/usr/bin/env python3
"""Measure VC71 optimization profiles against a fixed corpus.

This is deliberately a measurement lane: it never changes the production build
or verifier defaults.  A profile is adopted only after a corpus run shows a
non-regressing aggregate improvement.

Example:
  python3 tools/verify/compiler_profile.py \
    --case src/halo/math/real_math.c:vector3d_scale_add \
    --case src/halo/objects/objects.c:object_get_origin \
    --opt /O2 --opt "/O2 /Ob1" --output artifacts/compiler_profile/pilot.json

The scheduled calibration workflow supplies a committed ``--corpus`` file.
Manual ``--case`` and ``--opt`` values remain available for focused experiments.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
VERIFY = ROOT / "tools" / "verify" / "vc71_verify.py"
SCORE_CONTEXT = ROOT / "artifacts" / "score_context"


def parse_case(value: str) -> tuple[str, str]:
    """Parse a portable ``source:function`` corpus case."""
    source, separator, function = value.rpartition(":")
    if not separator or not source or not function:
        raise ValueError("case must be source.c:function_name")
    if not source.endswith(".c"):
        raise ValueError("case source must be a .c file")
    return source, function


def load_corpus(path: Path) -> tuple[list[str], list[str]]:
    """Load a versioned calibration corpus without accepting ambiguous input."""
    try:
        corpus = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise ValueError(f"could not read corpus {path}: {exc}") from exc
    if not isinstance(corpus, dict):
        raise ValueError("corpus must be a JSON object")
    cases = corpus.get("cases")
    profiles = corpus.get("profiles", [])
    if (not isinstance(cases, list) or not cases or
            not all(isinstance(case, str) for case in cases)):
        raise ValueError("corpus cases must be a non-empty list of source:function strings")
    if not isinstance(profiles, list) or not all(isinstance(opt, str) for opt in profiles):
        raise ValueError("corpus profiles must be a list of optimization strings")
    for case in cases:
        parse_case(case)
    return cases, profiles


def score_case(source: str, function: str, opt: str) -> dict[str, Any]:
    """Compile one corpus case and return its current score-context metrics."""
    source_path = ROOT / source
    if not source_path.is_file():
        return {"source": source, "function": function, "opt": opt,
                "error": "corpus source does not exist"}
    command = [sys.executable, str(VERIFY), source, "--function", function,
               "--opt", opt, "--no-cache", "--quiet", "--threshold", "0"]
    process = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
    context_path = SCORE_CONTEXT / f"{function}.json"
    result: dict[str, Any] = {
        "source": source, "function": function, "opt": opt,
        "returncode": process.returncode,
    }
    if process.returncode != 0:
        result["error"] = "VC71 verifier failed"
        return result
    if not context_path.exists():
        result["error"] = "score context missing after verifier run"
        return result
    try:
        context = json.loads(context_path.read_text(encoding="utf-8"))
        source_sha = hashlib.sha256(source_path.read_bytes()).hexdigest()
        if context.get("candidate_source_sha256") != source_sha:
            result["error"] = "score context does not match current corpus source"
            return result
        scores = context.get("scores", {})
        result["official_pct"] = scores.get("official_pct")
        result["dp_lcs_pct"] = scores.get("dp_lcs_pct")
        result["frame"] = context.get("frame", {})
        result["classification"] = context.get("classification", [])
    except (OSError, json.JSONDecodeError) as exc:
        result["error"] = f"could not read score context: {exc}"
    return result


def summarize(rows: list[dict[str, Any]]) -> dict[str, dict[str, float]]:
    """Return per-profile means using only measurements that produced a score."""
    profiles: dict[str, list[dict[str, Any]]] = {}
    for row in rows:
        if isinstance(row.get("official_pct"), (int, float)):
            profiles.setdefault(str(row["opt"]), []).append(row)
    summary = {}
    for opt, measured in profiles.items():
        summary[opt] = {
            "cases": len(measured),
            "official_mean": sum(float(row["official_pct"]) for row in measured) / len(measured),
            "dp_lcs_mean": sum(float(row.get("dp_lcs_pct") or 0.0) for row in measured) / len(measured),
        }
    return summary


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--case", action="append", default=[], metavar="SOURCE:FUNCTION",
                        help="Repeatable known-good corpus member.")
    parser.add_argument("--corpus", type=Path, default=None,
                        help="Versioned JSON corpus used by scheduled calibration.")
    parser.add_argument("--opt", action="append", default=[], metavar="FLAGS",
                        help="Repeatable VC71 optimization profile (default: /O2).")
    parser.add_argument("--output", type=Path, default=None,
                        help="Write the complete immutable measurement report here.")
    args = parser.parse_args(argv)
    try:
        corpus_cases: list[str] = []
        corpus_profiles: list[str] = []
        if args.corpus:
            corpus_cases, corpus_profiles = load_corpus(args.corpus)
        raw_cases = corpus_cases + args.case
        if not raw_cases:
            raise ValueError("provide --corpus or at least one --case")
        cases = [parse_case(value) for value in raw_cases]
    except ValueError as exc:
        parser.error(str(exc))
    profiles = args.opt or corpus_profiles or ["/O2"]
    rows = [score_case(source, function, opt)
            for source, function in cases for opt in profiles]
    report = {"schema": 1, "profiles": profiles,
              "cases": raw_cases, "corpus": str(args.corpus) if args.corpus else None,
              "results": rows, "summary": summarize(rows)}
    encoded = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(encoded, encoding="utf-8")
    print(encoded, end="")
    return 0 if all("error" not in row for row in rows) else 1


if __name__ == "__main__":
    raise SystemExit(main())
