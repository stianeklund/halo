#!/usr/bin/env python3
"""Routing outcome stats — win-rate per (model, effort) and per failure class.

Read-only report over the two ledgers that already record lift attempts, so
future model-routing changes are arithmetic instead of anecdote
(docs/plans/agent-model-routing-2026-08.md section 4, "Per-rung outcome stats").

Sources (never written, never mutated):
  1. Park ledger  artifacts/parked/<slug>.json  (tools/lift/park.py)
     Record: {name, addr, obj, source_path, best_score, best_patch, status,
              first_parked, last_updated, promoted_commit, superseded_reason,
              attempts: [{ts, model, effort, score, cap_hypothesis, reason,
                          notes, patch}]}
     status in {parked, promoted, capped_confirmed, superseded}.
     Dir resolution matches park.py: $HALO_PARKED_DIR, else
     parent-of(`git rev-parse --git-common-dir`)/artifacts/parked, else
     repo_root/artifacts/parked.
  2. Auto-lift failures  artifacts/auto_lift/failures/<slug>.json
     Record: {target|function, addr|address, object,
              attempts: [{model, failure_stage, error_summary}]}
     Old records carry no `attempts` and no `effort` — both tolerated.

OpenCode attempts share this ledger by convention, recording model ids like
"luna-high", "terra-xhigh", "sol-medium". This report treats "model-effort"
strings and separate model+effort fields uniformly: when the effort field is
absent and the model ends in "-<effort>" for a known effort, it is split.

Usage:
  python3 tools/audit/routing_stats.py            # tables
  python3 tools/audit/routing_stats.py --json     # machine-readable aggregate
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from collections import Counter, defaultdict
from pathlib import Path

EFFORTS = ("low", "medium", "high", "xhigh", "max")
UNKNOWN = "unknown"
# Terminal park statuses grouped into report buckets.
STATUS_BUCKET = {
    "promoted": "promoted",
    "parked": "parked",
    "capped_confirmed": "capped",
    "superseded": "superseded",
}
OUTCOMES = ("promoted", "parked", "capped", "superseded", "failed")


def repo_root() -> Path:
    try:
        out = subprocess.run(
            ["git", "rev-parse", "--show-toplevel"],
            capture_output=True, text=True, check=True,
        ).stdout.strip()
        return Path(out)
    except Exception:
        return Path(__file__).resolve().parents[2]


def parked_dir() -> Path:
    env = os.environ.get("HALO_PARKED_DIR")
    if env:
        return Path(env)
    try:
        common = subprocess.run(
            ["git", "rev-parse", "--git-common-dir"],
            capture_output=True, text=True, check=True,
        ).stdout.strip()
        return (Path(common).resolve().parent / "artifacts" / "parked")
    except Exception:
        return repo_root() / "artifacts" / "parked"


def normalize(model, effort):
    """Return (model, effort), splitting a 'model-effort' id when needed."""
    m = (model or "").strip() or UNKNOWN
    e = (effort or "").strip() or ""
    if not e:
        for suffix in EFFORTS:
            if m.lower().endswith("-" + suffix) and len(m) > len(suffix) + 1:
                return m[: -(len(suffix) + 1)], suffix
        return m, UNKNOWN
    return m, e


def load_json_dir(path: Path):
    if not path.is_dir():
        return []
    out = []
    for f in sorted(path.glob("*.json")):
        try:
            with f.open("r", encoding="utf-8") as fh:
                data = json.load(fh)
        except (ValueError, OSError):
            continue
        if isinstance(data, dict):
            out.append((f, data))
    return out


def as_float(v):
    try:
        return float(v)
    except (TypeError, ValueError):
        return None


def aggregate(park_records, failure_records):
    rows = defaultdict(lambda: {
        "attempts": 0,
        "promoted": 0, "parked": 0, "capped": 0, "superseded": 0, "failed": 0,
        "deltas": [],
    })
    stages = defaultdict(Counter)   # failure_stage -> model -> count
    skipped = {"park_no_attempts": 0, "failure_no_attempts": 0}

    for _, rec in park_records:
        attempts = rec.get("attempts") or []
        keys = []
        prev_score = None
        prev_key = None
        for att in attempts:
            key = normalize(att.get("model"), att.get("effort"))
            keys.append(key)
            rows[key]["attempts"] += 1
            score = as_float(att.get("score"))
            if score is not None and prev_score is not None and key != prev_key:
                rows[key]["deltas"].append(score - prev_score)
            if score is not None:
                prev_score = score
                prev_key = key
        if not keys:
            skipped["park_no_attempts"] += 1
            continue
        bucket = STATUS_BUCKET.get(rec.get("status") or "", "parked")
        rows[keys[-1]][bucket] += 1

    for _, rec in failure_records:
        attempts = rec.get("attempts") or []
        keys = []
        for att in attempts:
            key = normalize(att.get("model"), att.get("effort"))
            keys.append(key)
            rows[key]["attempts"] += 1
            stage = (att.get("failure_stage") or UNKNOWN).strip() or UNKNOWN
            stages[stage][key[0]] += 1
        if not keys:
            skipped["failure_no_attempts"] += 1
            stages[(rec.get("verdict") or UNKNOWN).strip().lower() or UNKNOWN][UNKNOWN] += 1
            rows[(UNKNOWN, UNKNOWN)]["attempts"] += 1
            rows[(UNKNOWN, UNKNOWN)]["failed"] += 1
            continue
        rows[keys[-1]]["failed"] += 1

    out_rows = []
    for (model, effort), v in rows.items():
        terminal = sum(v[o] for o in OUTCOMES)
        deltas = v.pop("deltas")
        row = {"model": model, "effort": effort}
        row.update(v)
        row["terminal"] = terminal
        row["promote_rate"] = (v["promoted"] / terminal) if terminal else None
        row["mean_delta"] = (sum(deltas) / len(deltas)) if deltas else None
        row["delta_samples"] = len(deltas)
        out_rows.append(row)
    out_rows.sort(key=lambda r: (-r["attempts"], r["model"], r["effort"]))

    stage_rows = [
        {"failure_stage": s, "total": sum(c.values()), "by_model": dict(c)}
        for s, c in sorted(stages.items(), key=lambda kv: (-sum(kv[1].values()), kv[0]))
    ]
    return {
        "sources": {
            "park_records": len(park_records),
            "failure_records": len(failure_records),
            "skipped": skipped,
        },
        "by_model_effort": out_rows,
        "by_failure_stage": stage_rows,
    }


def fmt_pct(v):
    return "-" if v is None else "{:.0f}%".format(v * 100)


def fmt_delta(v):
    return "-" if v is None else "{:+.1f}".format(v)


def print_tables(agg):
    src = agg["sources"]
    print("routing_stats: {} park records, {} failure records".format(
        src["park_records"], src["failure_records"]))
    skipped = {k: v for k, v in src["skipped"].items() if v}
    if skipped:
        print("  no-attempt records: " + ", ".join(
            "{}={}".format(k, v) for k, v in sorted(skipped.items())))

    print("\n== Per (model, effort) ==")
    hdr = "{:<12} {:<8} {:>8} {:>5} {:>6} {:>6} {:>6} {:>6} {:>8} {:>10}"
    print(hdr.format("model", "effort", "attempts", "prom", "parked",
                     "capped", "supsd", "failed", "prom-rate", "mean-delta"))
    print("-" * 84)
    for r in agg["by_model_effort"]:
        print(hdr.format(
            r["model"][:12], r["effort"][:8], r["attempts"], r["promoted"],
            r["parked"], r["capped"], r["superseded"], r["failed"],
            fmt_pct(r["promote_rate"]),
            "{} ({})".format(fmt_delta(r["mean_delta"]), r["delta_samples"]),
        ))

    print("\n== Per failure stage (auto-lift failure records) ==")
    if not agg["by_failure_stage"]:
        print("  (none)")
        return
    print("{:<28} {:>6}  {}".format("failure_stage", "count", "by model"))
    print("-" * 74)
    for r in agg["by_failure_stage"]:
        by = ", ".join("{}={}".format(m, n)
                       for m, n in sorted(r["by_model"].items(), key=lambda kv: -kv[1]))
        print("{:<28} {:>6}  {}".format(r["failure_stage"][:28], r["total"], by))


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--parked-dir", help="override park ledger dir")
    ap.add_argument("--failures-dir", help="override auto-lift failures dir")
    ap.add_argument("--json", action="store_true", help="emit JSON aggregate")
    args = ap.parse_args(argv)

    pdir = Path(args.parked_dir) if args.parked_dir else parked_dir()
    fdir = (Path(args.failures_dir) if args.failures_dir
            else repo_root() / "artifacts" / "auto_lift" / "failures")

    park_records = load_json_dir(pdir)
    failure_records = load_json_dir(fdir)

    if not park_records and not failure_records:
        msg = "no records found (park dir: {}, failures dir: {})".format(pdir, fdir)
        if args.json:
            print(json.dumps({"sources": {"park_records": 0, "failure_records": 0},
                              "by_model_effort": [], "by_failure_stage": [],
                              "message": msg}, indent=2))
        else:
            print("routing_stats: " + msg)
        return 0

    agg = aggregate(park_records, failure_records)
    agg["sources"]["parked_dir"] = str(pdir)
    agg["sources"]["failures_dir"] = str(fdir)
    if args.json:
        print(json.dumps(agg, indent=2, sort_keys=False))
    else:
        print_tables(agg)
    return 0


if __name__ == "__main__":
    sys.exit(main())
