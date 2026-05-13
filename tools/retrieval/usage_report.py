#!/usr/bin/env python3
"""Report retrieval and Mizuchi adoption signals from local artifacts.

This is a lightweight telemetry view over existing artifact files. It does not
call MCP or external APIs.
"""

from __future__ import annotations

import argparse
import json
import statistics
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent.parent.parent
CONTEXT_CACHE_DIR = REPO_ROOT / "artifacts" / "auto_lift" / "context_cache"
AUTO_LIFT_FAILURES_DIR = REPO_ROOT / "artifacts" / "auto_lift" / "failures"
MIZUCHI_DIR = REPO_ROOT / "artifacts" / "mizuchi"
LIFT_RUNS_DIR = REPO_ROOT / "artifacts" / "lift_runs"


def _setup_venv_path() -> None:
    venv_sp = REPO_ROOT / ".venv" / "lib" / "python3.12" / "site-packages"
    if venv_sp.exists() and str(venv_sp) not in sys.path:
        sys.path.insert(0, str(venv_sp))


def _read_json(path: Path) -> dict | list | None:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return None


def _summarize_context_cache() -> dict:
    files = sorted(CONTEXT_CACHE_DIR.glob("*.json")) if CONTEXT_CACHE_DIR.exists() else []
    packs = []
    for path in files:
        data = _read_json(path)
        if isinstance(data, dict):
            packs.append(data)

    with_decompile = 0
    with_neighbor_key = 0
    with_neighbors = 0
    with_mizuchi_result = 0
    neighbor_counts = []
    top_sims = []
    all_sims = []

    for pack in packs:
        if pack.get("decompile_c"):
            with_decompile += 1

        neighbors = pack.get("similar_neighbors")
        if neighbors is not None:
            with_neighbor_key += 1
            if isinstance(neighbors, list) and neighbors:
                with_neighbors += 1
                neighbor_counts.append(len(neighbors))
                sims = [n.get("similarity") for n in neighbors if isinstance(n, dict)]
                sims = [float(s) for s in sims if isinstance(s, (int, float))]
                if sims:
                    top_sims.append(max(sims))
                    all_sims.extend(sims)

        if pack.get("mizuchi_result"):
            with_mizuchi_result += 1

    out = {
        "total_packs": len(packs),
        "with_decompile": with_decompile,
        "with_similar_neighbors_key": with_neighbor_key,
        "with_nonempty_similar_neighbors": with_neighbors,
        "with_mizuchi_result": with_mizuchi_result,
        "avg_neighbors_per_injected_pack": (
            statistics.mean(neighbor_counts) if neighbor_counts else None
        ),
        "avg_neighbor_similarity": (
            statistics.mean(all_sims) if all_sims else None
        ),
        "median_neighbor_similarity": (
            statistics.median(all_sims) if all_sims else None
        ),
        "avg_top_neighbor_similarity": (
            statistics.mean(top_sims) if top_sims else None
        ),
    }
    return out


def _summarize_retrieval_index() -> dict:
    _setup_venv_path()
    sys.path.insert(0, str(REPO_ROOT))
    from tools.retrieval import db as _db  # pylint: disable=import-outside-toplevel

    if not _db.DB_PATH.exists():
        return {
            "index_path": str(_db.DB_PATH),
            "exists": False,
        }

    con = _db.connect(read_only=True)
    stats = _db.stats(con)
    con.close()
    stats["index_path"] = str(_db.DB_PATH)
    stats["exists"] = True
    return stats


def _summarize_mizuchi() -> dict:
    files = sorted(MIZUCHI_DIR.glob("*run-results-*.json")) if MIZUCHI_DIR.exists() else []
    total_results = 0
    total_success = 0
    per_file = []

    for path in files:
        data = _read_json(path)
        if not isinstance(data, dict):
            continue
        results = data.get("results", [])
        if not isinstance(results, list):
            continue
        n_results = len(results)
        n_success = sum(1 for r in results if isinstance(r, dict) and r.get("success"))
        total_results += n_results
        total_success += n_success
        per_file.append({"file": path.name, "results": n_results, "success": n_success})

    return {
        "run_result_files": len(files),
        "total_results": total_results,
        "total_success": total_success,
        "success_rate": (float(total_success) / float(total_results) if total_results else None),
        "files": per_file,
    }


def _summarize_lift_outcomes(context_summary: dict) -> dict:
    summaries = sorted(LIFT_RUNS_DIR.glob("*/summary.json")) if LIFT_RUNS_DIR.exists() else []
    if not summaries:
        return {
            "lift_run_summaries_found": 0,
            "note": "No artifacts/lift_runs/*/summary.json found; cannot correlate retrieval with lift outcomes yet.",
        }

    neighbor_targets = set()
    if CONTEXT_CACHE_DIR.exists():
        for path in CONTEXT_CACHE_DIR.glob("*.json"):
            data = _read_json(path)
            if isinstance(data, dict) and data.get("similar_neighbors"):
                neighbor_targets.add(path.stem)

    match_values = []
    match_values_with_neighbors = []
    pass_count = 0
    pass_with_neighbors = 0
    targets_seen = 0
    targets_with_neighbors = 0

    for summary_path in summaries:
        data = _read_json(summary_path)
        if not isinstance(data, dict):
            continue
        target = data.get("target")
        if not isinstance(target, dict):
            continue
        name = str(target.get("name", ""))
        if not name:
            continue
        targets_seen += 1
        has_neighbors = name in neighbor_targets
        if has_neighbors:
            targets_with_neighbors += 1

        ok = bool(data.get("ok"))
        if ok:
            pass_count += 1
            if has_neighbors:
                pass_with_neighbors += 1

        vc71_match = None
        stages = data.get("stages", [])
        if isinstance(stages, list):
            for st in stages:
                if isinstance(st, dict) and st.get("name") == "vc71_verify":
                    details = str(st.get("details", ""))
                    marker = "% match"
                    if marker in details:
                        try:
                            vc71_match = float(details.split(marker, 1)[0].split()[-1])
                        except (IndexError, ValueError):
                            vc71_match = None
                    break

        if vc71_match is not None:
            match_values.append(vc71_match)
            if has_neighbors:
                match_values_with_neighbors.append(vc71_match)

    return {
        "lift_run_summaries_found": len(summaries),
        "targets_seen": targets_seen,
        "targets_with_neighbors": targets_with_neighbors,
        "pass_rate": (float(pass_count) / float(targets_seen) if targets_seen else None),
        "pass_rate_with_neighbors": (
            float(pass_with_neighbors) / float(targets_with_neighbors)
            if targets_with_neighbors
            else None
        ),
        "avg_vc71_match": (statistics.mean(match_values) if match_values else None),
        "avg_vc71_match_with_neighbors": (
            statistics.mean(match_values_with_neighbors)
            if match_values_with_neighbors
            else None
        ),
        "context_nonempty_neighbors": context_summary.get("with_nonempty_similar_neighbors", 0),
    }


def _summarize_failures_with_neighbors() -> dict:
    if not AUTO_LIFT_FAILURES_DIR.exists():
        return {"failure_records": 0, "with_neighbors_context": 0}

    neighbor_targets = set()
    if CONTEXT_CACHE_DIR.exists():
        for path in CONTEXT_CACHE_DIR.glob("*.json"):
            data = _read_json(path)
            if isinstance(data, dict) and data.get("similar_neighbors"):
                neighbor_targets.add(path.stem)

    failure_files = sorted(AUTO_LIFT_FAILURES_DIR.glob("*.json"))
    with_neighbors = 0
    for path in failure_files:
        data = _read_json(path)
        if not isinstance(data, dict):
            continue
        target = str(data.get("target", ""))
        if target in neighbor_targets:
            with_neighbors += 1

    return {
        "failure_records": len(failure_files),
        "with_neighbors_context": with_neighbors,
    }


def _fmt_float(value: float | None) -> str:
    if value is None:
        return "n/a"
    return f"{value:.4f}"


def print_human(report: dict) -> None:
    idx = report["retrieval_index"]
    ctx = report["context_cache"]
    miz = report["mizuchi"]
    out = report["lift_outcomes"]
    fail = report["auto_lift_failures"]

    print("retrieval index")
    print(f"  path: {idx.get('index_path', 'n/a')}")
    print(f"  exists: {idx.get('exists', False)}")
    if idx.get("exists"):
        print(f"  rows total: {idx.get('total', 0)}")
        print(f"  rows with embeddings: {idx.get('with_emb', 0)}")
        print(f"  rows with pseudocode: {idx.get('with_pseudocode', 0)}")
        print(f"  rows with c_source: {idx.get('with_c', 0)}")

    print("\ncontext cache")
    print(f"  packs total: {ctx.get('total_packs', 0)}")
    print(f"  with decompile: {ctx.get('with_decompile', 0)}")
    print(f"  with neighbors key: {ctx.get('with_similar_neighbors_key', 0)}")
    print(f"  with non-empty neighbors: {ctx.get('with_nonempty_similar_neighbors', 0)}")
    print(f"  with mizuchi_result: {ctx.get('with_mizuchi_result', 0)}")
    print(f"  avg neighbors/injected pack: {_fmt_float(ctx.get('avg_neighbors_per_injected_pack'))}")
    print(f"  avg neighbor similarity: {_fmt_float(ctx.get('avg_neighbor_similarity'))}")
    print(f"  median neighbor similarity: {_fmt_float(ctx.get('median_neighbor_similarity'))}")
    print(f"  avg top-neighbor similarity: {_fmt_float(ctx.get('avg_top_neighbor_similarity'))}")

    print("\nmizuchi")
    print(f"  run result files: {miz.get('run_result_files', 0)}")
    print(f"  total results: {miz.get('total_results', 0)}")
    print(f"  total success: {miz.get('total_success', 0)}")
    print(f"  success rate: {_fmt_float(miz.get('success_rate'))}")

    print("\nlift outcomes")
    print(f"  summaries found: {out.get('lift_run_summaries_found', 0)}")
    if out.get("note"):
        print(f"  note: {out['note']}")
    else:
        print(f"  targets seen: {out.get('targets_seen', 0)}")
        print(f"  targets with neighbors: {out.get('targets_with_neighbors', 0)}")
        print(f"  pass rate: {_fmt_float(out.get('pass_rate'))}")
        print(f"  pass rate with neighbors: {_fmt_float(out.get('pass_rate_with_neighbors'))}")
        print(f"  avg vc71 match: {_fmt_float(out.get('avg_vc71_match'))}")
        print(f"  avg vc71 match with neighbors: {_fmt_float(out.get('avg_vc71_match_with_neighbors'))}")

    print("\nauto-lift failures")
    print(f"  failure records: {fail.get('failure_records', 0)}")
    print(f"  failures with neighbors context: {fail.get('with_neighbors_context', 0)}")


def main() -> int:
    ap = argparse.ArgumentParser(
        description="Report embeddings retrieval usage and Mizuchi adoption signals"
    )
    ap.add_argument("--json", action="store_true", help="Emit JSON instead of human-readable text")
    args = ap.parse_args()

    context_summary = _summarize_context_cache()
    report = {
        "retrieval_index": _summarize_retrieval_index(),
        "context_cache": context_summary,
        "mizuchi": _summarize_mizuchi(),
        "lift_outcomes": _summarize_lift_outcomes(context_summary),
        "auto_lift_failures": _summarize_failures_with_neighbors(),
    }

    if args.json:
        print(json.dumps(report, indent=2))
    else:
        print_human(report)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
