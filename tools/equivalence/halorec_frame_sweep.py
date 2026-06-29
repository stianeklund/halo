#!/usr/bin/env python3
"""Multi-frame equivalence sweep over a `.halorec` recording.

A `.halorec` is a *time series* (see halorec_to_snapshot.py): one recorded frame
is one Unicorn state-snapshot. A single-frame `--state-snapshot` run exercises
only the one path that frame's state drives — perception scorers early-exit and
report weak coverage. Sweeping every frame the actor is alive in, and *unioning*
the visited-PC sets, drives the function through the many real states the engine
actually produced over the recording, turning weak single-frame coverage into a
high-confidence union.

The sweep is also a multi-state DIVERGENCE HUNT, not just a coverage tool: a
function our audit calls byte-faithful must pass in *every* real frame. Any frame
that FAILs contradicts the audit and is a stronger finding than the coverage %.

Two soundness rules, baked in:
  * Concolic Phase 2 is force-disabled (`no_concolic=True`). The whole point is
    that real frames REPLACE synthetic global injection; letting Phase 2 fire
    would union invented coverage and we couldn't tell what real states reached.
    (Synthetic injection against real values is Step 4: the value corpus.)
  * Default to alive-frames-only (`--alive-handle`). Frames where the actor is
    absent early-exit identically in oracle and candidate — a trivial pass with
    ~zero new coverage that would inflate the pass count and mask a real
    divergence.

COPYRIGHT: the recording (and any snapshot built from it) is raw Halo runtime
memory — host-only, gitignored. This script is metadata/tooling; it reads the
recording from $HALO_HALOREC_DIR (or an explicit --recording path) and never
writes game memory into the tree except as throwaway temp snapshots.

Usage:
  # One subject: sweep a grunt across every frame it is alive in.
  halorec_frame_sweep.py actor_compute_prop_target_weight \\
      --recording "$HALO_HALOREC_DIR/Recording 19.halorec" \\
      --arg actor_handle 0xe38b0006 --arg clump_item_handle 0xf7420021 \\
      --allow-stubs --float-tolerance 32 --mem-trace

  # Grand union across every subject the manifest pins to this recording
  # (answers "does the union clear the 60%% high bar?" in one command).
  halorec_frame_sweep.py actor_compute_prop_target_weight \\
      --recording "$HALO_HALOREC_DIR/Recording 19.halorec" --from-manifest \\
      --allow-stubs --float-tolerance 32 --mem-trace
"""

import argparse
import contextlib
import io
import json
import os
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

from halorec_to_snapshot import (  # noqa: E402
    parse_halorec, frame_handle_alive, frame_regions_dict,
)

MANIFEST = HERE / "host_snapshots" / "manifest.json"


def _parse_value(v):
    try:
        return int(v, 0)
    except (ValueError, TypeError):
        return v


def _snapshot_dict(name, frame, idx, args):
    """load_snapshot-compatible dict for an already-parsed frame (no re-parse)."""
    regions = frame_regions_dict(frame)
    out = {
        "description": f"{name} frame {idx} (sweep)",
        "regions": {f"0x{a:08x}": regions[a].hex() for a in sorted(regions)},
    }
    if args:
        out["arg_overrides"] = dict(args)
    return out


def _alive_frame_indices(frames, alive_handle):
    if alive_handle is None:
        return list(range(len(frames)))
    return [i for i, fr in enumerate(frames)
            if frame_handle_alive(fr.regions, alive_handle)]


def _select(indices, sel):
    """Apply --frames selector to a list of (already alive) frame indices."""
    if sel in (None, "all"):
        return indices
    if sel.startswith("every:"):
        n = max(1, int(sel.split(":", 1)[1]))
        return indices[::n]
    # explicit comma list of recording-frame indices, intersected with alive set
    wanted = {int(x, 0) for x in sel.split(",") if x.strip()}
    return [i for i in indices if i in wanted]


def _run_one_frame(run_diff, target, snap_dict, seeds, allow_stubs,
                   float_tol, mem_trace):
    """Build a temp snapshot for one frame, run the diff, return its JSON dict."""
    with tempfile.TemporaryDirectory(prefix="halorec_sweep_") as td:
        snap_path = Path(td) / "snap.json"
        out_path = Path(td) / "out.json"
        snap_path.write_text(json.dumps(snap_dict), encoding="utf-8")
        # Capture run_diff's stdout+stderr (the emulator stubs trace datum_get
        # etc. to stderr); only surface it if the frame FAILs.
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf), contextlib.redirect_stderr(buf):
            run_diff(
                target, num_seeds=seeds, state_snapshot=snap_path,
                output_json=out_path, allow_stubs=allow_stubs,
                float_tolerance_ulp=float_tol, mem_trace=mem_trace,
                no_concolic=True,        # real frames replace synthetic injection
                quiet=True, save_log=False, record_leaf=False,
            )
        res = json.loads(out_path.read_text(encoding="utf-8"))
        res["_stdout"] = buf.getvalue()
        return res


def _subjects_from_manifest(recording_path):
    """Manifest entries whose 'recording' basename matches recording_path."""
    if not MANIFEST.exists():
        return []
    man = json.loads(MANIFEST.read_text(encoding="utf-8"))
    base = Path(recording_path).name
    subs = []
    for s in man.get("snapshots", []):
        if s.get("recording") != base:
            continue
        args = {k: _parse_value(v) for k, v in (s.get("args") or {}).items()}
        alive = args.get("actor_handle")
        subs.append({"label": s.get("out", base), "args": args, "alive": alive})
    return subs


def main(argv=None):
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("target", help="function name or address (e.g. 0x2fd10)")
    ap.add_argument("--recording", required=True, help="path to a .halorec recording")
    ap.add_argument("--arg", nargs=2, action="append", metavar=("NAME", "VALUE"),
                    default=None, help="pin an arg override (repeatable)")
    ap.add_argument("--alive-handle", type=lambda x: int(x, 0), default=None,
                    help="restrict to frames where this datum handle is alive "
                         "(default: the actor_handle arg if present)")
    ap.add_argument("--from-manifest", action="store_true",
                    help="sweep every host_snapshots/manifest.json subject pinned "
                         "to this recording; report a grand union across subjects")
    ap.add_argument("--frames", default="all",
                    help="frame selector over the alive set: 'all' (default), "
                         "'every:N', or a comma list of recording-frame indices")
    ap.add_argument("--seeds", type=int, default=20)
    ap.add_argument("--allow-stubs", action="store_true")
    ap.add_argument("--float-tolerance", type=int, default=0)
    ap.add_argument("--mem-trace", action="store_true")
    ap.add_argument("--coverage-in", action="append", default=None, metavar="PATH",
                    help="seed the union PC map from prior runs (repeatable) — "
                         "chains a grand union across separate invocations")
    ap.add_argument("--coverage-out", default=None, metavar="PATH",
                    help="write this run's union PC map {pc: size} for chaining")
    ap.add_argument("--emit-value-corpus", default=None, metavar="PATH",
                    help="(Step 4) write {global_addr: [observed values]} harvested "
                         "from per-frame global reads, for concolic --value-corpus")
    ap.add_argument("--json", default=None, metavar="PATH",
                    help="machine-readable summary output")
    a = ap.parse_args(argv)

    rec = Path(os.path.expanduser(a.recording))
    if not rec.exists():
        print(f"[sweep] recording not found: {rec} — nothing to sweep (SKIP).")
        return 0  # mirror build_host_snapshots: absent host asset => clean skip

    cli_args = {k: _parse_value(v) for k, v in a.arg} if a.arg else {}

    if a.from_manifest:
        subjects = _subjects_from_manifest(rec)
        if not subjects:
            print(f"[sweep] no manifest subjects for {rec.name}; "
                  f"falling back to --arg subject.")
    else:
        subjects = []
    if not subjects:
        alive = a.alive_handle if a.alive_handle is not None else cli_args.get("actor_handle")
        subjects = [{"label": "cli", "args": cli_args, "alive": alive}]

    # Import run_diff lazily (pulls in unicorn/capstone).
    from unicorn_diff import run_diff

    name, _ver, frames = parse_halorec(str(rec))
    print(f"[sweep] {rec.name}: {len(frames)} frames, target={a.target}, "
          f"{len(subjects)} subject(s)")

    # Union PC map: {pc_hex: size}.  Seed from any --coverage-in.
    union_pcs = {}
    for p in (a.coverage_in or []):
        prev = json.loads(Path(p).read_text(encoding="utf-8"))
        union_pcs.update(prev.get("covered_pcs", prev))

    func_size = 0
    value_corpus = {}          # addr_hex -> set(int values)
    subject_reports = []
    any_fail = any_error = False
    total_frames_run = 0

    for sub in subjects:
        alive = sub["alive"]
        idxs = _select(_alive_frame_indices(frames, alive), a.frames)
        if not idxs:
            print(f"  subject {sub['label']}: handle "
                  f"{alive if alive is None else hex(alive)} alive in 0 frames — skip")
            continue
        sub_pcs = {}
        p = f = e = 0
        for i in idxs:
            snap = _snapshot_dict(name, frames[i], i, sub["args"])
            res = _run_one_frame(run_diff, a.target, snap, a.seeds,
                                 a.allow_stubs, a.float_tolerance, a.mem_trace)
            status = res.get("status")
            func_size = res.get("func_size") or func_size
            for pc, sz in (res.get("covered_pcs") or {}).items():
                sub_pcs[pc] = sz
                union_pcs[pc] = sz
            for gr in (res.get("global_reads") or []):
                value_corpus.setdefault(gr["address"], set()).add(
                    int(gr["value"], 0))
            total_frames_run += 1
            if status == "pass":
                p += 1
            elif status == "fail":
                f += 1
                any_fail = True
                print(f"  !! subject {sub['label']} frame {i}: DIVERGENCE")
                tail = "\n".join(res["_stdout"].splitlines()[-8:])
                if tail.strip():
                    print("     " + tail.replace("\n", "\n     "))
            else:
                e += 1
                if res.get("passed", 0) == 0:
                    any_error = True
        sub_cov = (100.0 * sum(sub_pcs.values()) / func_size) if func_size else 0.0
        subject_reports.append({
            "label": sub["label"], "alive": alive, "frames": len(idxs),
            "pass": p, "fail": f, "error": e, "coverage_pct": round(sub_cov, 1),
        })
        print(f"  subject {sub['label']}: {len(idxs)} alive frames "
              f"[pass={p} fail={f} err={e}] coverage={sub_cov:.1f}%")

    union_bytes = sum(union_pcs.values())
    union_pct = (100.0 * union_bytes / func_size) if func_size else 0.0
    tier = ("high" if union_pct >= 60 else
            "moderate" if union_pct >= 30 else "weak")
    verdict = ("FAIL" if any_fail else "ERROR" if any_error else "PASS")

    print("-" * 64)
    print(f"[sweep] frames run: {total_frames_run}  "
          f"union coverage: {union_bytes}/{func_size} bytes "
          f"({union_pct:.1f}%, {tier})  verdict: {verdict}")
    if not any_fail and not any_error and len(subjects) > 1:
        single = max((r["coverage_pct"] for r in subject_reports), default=0.0)
        print(f"[sweep] best single-subject coverage was {single:.1f}% "
              f"— union lifts it to {union_pct:.1f}%")

    if a.coverage_out:
        Path(a.coverage_out).write_text(
            json.dumps({"func_size": func_size, "covered_pcs": union_pcs},
                       indent=2) + "\n", encoding="utf-8")
        print(f"[sweep] union PC map -> {a.coverage_out}")

    if a.emit_value_corpus:
        corpus = {addr: sorted(vals) for addr, vals in value_corpus.items()}
        Path(a.emit_value_corpus).write_text(
            json.dumps(corpus, indent=2) + "\n", encoding="utf-8")
        print(f"[sweep] value corpus ({len(corpus)} globals) "
              f"-> {a.emit_value_corpus}")

    if a.json:
        Path(a.json).write_text(json.dumps({
            "target": a.target, "recording": rec.name,
            "frames_run": total_frames_run, "func_size": func_size,
            "union_bytes": union_bytes, "union_pct": round(union_pct, 1),
            "tier": tier, "verdict": verdict, "subjects": subject_reports,
        }, indent=2) + "\n", encoding="utf-8")

    return 1 if verdict != "PASS" else 0


if __name__ == "__main__":
    sys.exit(main())
