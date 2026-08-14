#!/usr/bin/env python3
"""Build a reproducible summary of VC71 floor scores and score contexts."""

import argparse
import hashlib
import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent.parent
DEFAULT_FLOOR = ROOT / "tools/verify/vc71_scores.json"
DEFAULT_CONTEXTS = ROOT / "artifacts/score_context"


def _sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _aggregate_hash(paths):
    digest = hashlib.sha256()
    for path in sorted(paths, key=lambda item: item.name):
        digest.update(path.name.encode("utf-8"))
        digest.update(b"\0")
        digest.update(bytes.fromhex(_sha256(path)))
    return digest.hexdigest()


def _display_path(path):
    try:
        return str(path.relative_to(ROOT))
    except ValueError:
        return str(path)


def _band(score):
    if score == 100:
        return "100"
    floor = int(score // 10) * 10
    return "%d-%d" % (floor, floor + 9)


def _has_rule(context, rule):
    return any(item.get("rule") == rule
               for item in context.get("classification", []))


def build_census(floor_path, context_dir, low_threshold=70.0,
                 gap_threshold=10.0, classifier_gap_threshold=12.0):
    floor_doc = json.loads(floor_path.read_text())
    floor = floor_doc["scores"]
    context_paths = sorted(context_dir.glob("*.json"))
    contexts = {}
    invalid = []
    for path in context_paths:
        try:
            context = json.loads(path.read_text())
            name = context.get("name")
            if not name:
                raise ValueError("missing name")
            contexts[name] = context
        except (OSError, ValueError, json.JSONDecodeError) as exc:
            invalid.append({"file": path.name, "error": str(exc)})

    joined_names = sorted(set(floor) & set(contexts))
    low_names = sorted(name for name, row in floor.items()
                       if row["score"] < low_threshold)
    low_contexts = [contexts[name] for name in low_names if name in contexts]

    bands = {}
    for row in floor.values():
        key = _band(row["score"])
        bands[key] = bands.get(key, 0) + 1

    context_schemas = {}
    for context in contexts.values():
        schema = str(context.get("schema", 1))
        context_schemas[schema] = context_schemas.get(schema, 0) + 1

    rule_counts = {}
    with_rule = 0
    multi_rule = 0
    for context in low_contexts:
        rules = context.get("classification", [])
        if rules:
            with_rule += 1
        if len(rules) > 1:
            multi_rule += 1
        for item in rules:
            rule = item.get("rule")
            if rule:
                rule_counts[rule] = rule_counts.get(rule, 0) + 1

    gaps = []
    classifier_gaps = []
    for name in joined_names:
        scores = contexts[name].get("scores", {})
        official = scores.get("official_pct")
        dp = scores.get("dp_lcs_pct")
        if official is None or dp is None:
            continue
        gap = dp - official
        if gap > gap_threshold:
            gaps.append((name, gap, floor[name]["score"] < low_threshold))
        if gap > classifier_gap_threshold:
            classifier_gaps.append(
                (name, gap, floor[name]["score"] < low_threshold))

    one_insn = [(name, row) for name, row in floor.items()
                 if row.get("n_r") == 1]
    regarg_scores = [floor[name]["score"] for name in joined_names
                     if _has_rule(contexts[name], "regarg_structural_ceiling")]

    return {
        "schema": 1,
        "inputs": {
            "floor": _display_path(floor_path),
            "floor_sha256": _sha256(floor_path),
            "floor_version": floor_doc.get("version"),
            "context_dir": _display_path(context_dir),
            "context_files": len(context_paths),
            "context_aggregate_sha256": _aggregate_hash(context_paths),
            "context_schema_counts": dict(sorted(context_schemas.items())),
            "invalid_contexts": invalid,
        },
        "join": {
            "floor_rows": len(floor),
            "context_names": len(contexts),
            "joined_rows": len(joined_names),
            "missing_context": sorted(set(floor) - set(contexts)),
            "stale_context": sorted(set(contexts) - set(floor)),
        },
        "distribution": dict(sorted(bands.items(), reverse=True)),
        "low_score": {
            "threshold": low_threshold,
            "count": len(low_names),
            "contexts": len(low_contexts),
            "with_any_rule": with_rule,
            "without_rule": len(low_contexts) - with_rule,
            "with_multiple_rules": multi_rule,
            "rule_hits": sum(rule_counts.values()),
            "rule_counts": dict(sorted(rule_counts.items())),
        },
        "metric_gap": {
            "threshold_pp": gap_threshold,
            "count": len(gaps),
            "low_score_count": sum(1 for _, _, low in gaps if low),
            "sum_pp": sum(gap for _, gap, _ in gaps),
            "classifier_threshold_pp": classifier_gap_threshold,
            "classifier_count": len(classifier_gaps),
            "classifier_low_score_count": sum(
                1 for _, _, low in classifier_gaps if low),
        },
        "one_instruction_references": {
            "count": len(one_insn),
            "perfect": sum(1 for _, row in one_insn if row["score"] == 100),
            "below_low_threshold": sum(
                1 for _, row in one_insn if row["score"] < low_threshold),
            "exact_zero": sum(1 for _, row in one_insn if row["score"] == 0),
            "low_rows": [
                {"name": name, "score": row["score"], "kind": row.get("kind")}
                for name, row in sorted(one_insn)
                if row["score"] < low_threshold
            ],
        },
        "regarg_structural_ceiling": {
            "count": len(regarg_scores),
            "below_70": sum(1 for score in regarg_scores if score < 70),
            "from_70_to_89": sum(
                1 for score in regarg_scores if 70 <= score < 90),
            "at_least_90": sum(1 for score in regarg_scores if score >= 90),
        },
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--floor", type=Path, default=DEFAULT_FLOOR)
    parser.add_argument("--contexts", type=Path, default=DEFAULT_CONTEXTS)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--check", action="store_true",
                        help="fail if --output differs from generated report")
    args = parser.parse_args()
    report = build_census(args.floor.resolve(), args.contexts.resolve())
    text = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.check:
        if not args.output:
            parser.error("--check requires --output")
        current = args.output.read_text() if args.output.exists() else ""
        if current != text:
            print("VC71 census manifest is stale: %s" % args.output,
                  file=sys.stderr)
            return 1
        print("VC71 census manifest is current: %s" % args.output)
        return 0
    if args.output:
        args.output.write_text(text)
    else:
        print(text, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
