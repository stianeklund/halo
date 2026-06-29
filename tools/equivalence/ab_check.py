#!/usr/bin/env python3
"""A/B regression check for trajectory testing (docs/ab-trajectory-testing.md).

One command: replay the SAME deterministic input fixture on the faithful build
(cachebeta = the golden) and the patched build (default = the candidate), capture
each build's game-state trajectory, and run the TOLERANT behavioral diff. The
earliest sustained divergence names the time + field + entity to investigate.

    python3 tools/equivalence/ab_check.py --level a10 --scenario a10-checkpoint-5s-action

Golden freeze / reuse (CI tripwire pattern). The faithful build's trajectory is
deterministic and reusable, so capture it once and reuse it:

    # first time: capture cachebeta and FREEZE it as the golden (host-only path!)
    python3 tools/equivalence/ab_check.py --level a10 --scenario <s> \
        --freeze --golden ~/halo-goldens/a10.halorec
    # thereafter (or in nightly CI): reuse the frozen golden, capture only default
    python3 tools/equivalence/ab_check.py --level a10 --scenario <s> \
        --golden ~/halo-goldens/a10.halorec

`--aa-first` self-checks: it captures cachebeta a SECOND time and strict-diffs the two
faithful runs (the A/A determinism check). If that isn't clean the harness is unsound,
so the A/B is aborted. Recommended on first use of a fixture/box; skip it once you
trust determinism (the cachebeta + default A/A were both CLEAN on a10 2026-06-29).

A discovers, B confirms: a localized divergence here is a *lead*, not a proof. Hand it
to Tier-B `unicorn_diff --state-snapshot` on the named function, or to a toggle-bisect.

Goldens and captured `.halorec` are literal game memory — host-only, gitignored,
never committed. Exit 0 = clean, 3 = divergent, 2 = inconclusive / A/A failed.
"""
import argparse
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "tools" / "equivalence"))
import halorec_to_snapshot as h2s   # noqa: E402
import behavior_diff as bd          # noqa: E402
import trajectory_diff as td        # noqa: E402
from aa_check import capture_run     # noqa: E402  (replay + capture_trajectory)


def _diff_behavior(golden, candidate, cfg):
    _, _, fa = h2s.parse_halorec(str(golden))
    _, _, fb = h2s.parse_halorec(str(candidate))
    return bd.diff_behavior(fa, fb, cfg)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--level", required=True)
    ap.add_argument("--scenario", required=True)
    ap.add_argument("--golden-xbe", default="cachebeta.xbe",
                    help="faithful build (default cachebeta.xbe)")
    ap.add_argument("--candidate-xbe", default="default.xbe",
                    help="patched build under test (default default.xbe)")
    ap.add_argument("--golden", type=Path, default=None,
                    help="reuse a FROZEN golden .halorec instead of capturing cachebeta "
                         "(host-only path). With --freeze, write the fresh capture here.")
    ap.add_argument("--freeze", action="store_true",
                    help="capture cachebeta and copy it to --golden (freeze for reuse)")
    ap.add_argument("--aa-first", action="store_true",
                    help="run the cachebeta A/A determinism check before the A/B; abort if dirty")
    ap.add_argument("--ticks", type=int, default=200)
    ap.add_argument("--quantum", type=int, default=1)
    ap.add_argument("--window", type=int, default=None, help="behavior_diff +/- tick tolerance")
    ap.add_argument("--min-run", type=int, default=None, help="behavior_diff sustained-onset run")
    ap.add_argument("--config", type=Path, default=None, help="watch-list JSON (default: a10 AI)")
    ap.add_argument("--host", default="")
    ap.add_argument("--no-wait-spawn", action="store_true")
    ap.add_argument("--out-dir", type=Path, default=ROOT / "tmp" / "ab_check")
    ap.add_argument("--reuse", action="store_true",
                    help="skip captures; diff existing golden/candidate in --out-dir")
    a = ap.parse_args(argv)

    a.out_dir.mkdir(parents=True, exist_ok=True)
    tag = f"{a.level}_{a.scenario}"
    golden = a.out_dir / f"{tag}_golden.halorec"
    candidate = a.out_dir / f"{tag}_candidate.halorec"

    def cap(xbe, out):
        capture_run(a.level, a.scenario, xbe, a.host, out, a.ticks, a.quantum,
                    a.no_wait_spawn)

    if not a.reuse:
        # 1. golden (faithful) — reuse a frozen one if given and present
        if a.golden and a.golden.exists() and not a.freeze:
            print(f"== reusing frozen golden: {a.golden} ==")
            golden = a.golden
        else:
            print(f"== capture golden ({a.golden_xbe}) ==")
            cap(a.golden_xbe, golden)
            if a.freeze and a.golden:
                a.golden.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy(golden, a.golden)
                print(f"  [freeze] golden -> {a.golden}")

        # 1b. optional A/A determinism check (second faithful capture, strict diff)
        if a.aa_first:
            aa_run2 = a.out_dir / f"{tag}_aa_run2.halorec"
            print(f"== A/A check: second {a.golden_xbe} capture ==")
            cap(a.golden_xbe, aa_run2)
            _, _, fa = h2s.parse_halorec(str(golden))
            _, _, fb = h2s.parse_halorec(str(aa_run2))
            aa = td.diff_trajectories(fa, fb)
            if aa.get("error") or aa["matched"] == 0:
                print("VERDICT: INCONCLUSIVE — A/A had no comparable frames; "
                      "fix capture before trusting A/B.")
                return 2
            if aa["divergent_matched"] != 0:
                e = aa["first_divergence"]
                print(f"VERDICT: A/A DIRTY — faithful build is non-deterministic "
                      f"(first at rel={e['rel']} {e['pool']}). Harness unsound; A/B aborted.")
                return 2
            print(f"  [A/A] CLEAN ({aa['clean_matched']}/{aa['matched']} frames)")

        # 2. candidate (patched)
        print(f"== capture candidate ({a.candidate_xbe}) ==")
        cap(a.candidate_xbe, candidate)
    else:
        if a.golden and a.golden.exists():
            golden = a.golden

    # 3. tolerant behavioral diff
    cfg = None
    if a.config:
        import json
        cfg = json.loads(a.config.read_text())
    else:
        cfg = dict(bd.DEFAULT_CONFIG)
    if a.window is not None:
        cfg["window"] = a.window
    if a.min_run is not None:
        cfg["min_run"] = a.min_run

    res = _diff_behavior(golden, candidate, cfg)
    print()
    print("=" * 78)
    print(f"A/B CHECK: {a.level}/{a.scenario}  golden={a.golden_xbe}  candidate={a.candidate_xbe}")
    print(f"  golden={res['framesA']} frames  candidate={res['framesB']} frames  "
          f"window=+/-{res['window']}  min_run={res['min_run']}  "
          f"sustained divergences={res['onset_count']}")
    if res["onset_count"] == 0:
        print("VERDICT: CLEAN — patched build behaviorally matches faithful on the watch-list.")
        return 0
    for o in res["onsets"]:
        slot = "" if o["slot"] is None else f"slot={o['slot']:<3d} "
        print(f"  tick={o['tick']:<6d} {o['scope']:9s} {slot}{o['field']:14s} "
              f"faithful={bd._fmt(o['a'])}  patched={bd._fmt(o['b'])}  (|dt|={o['dt']})")
    e = res["onsets"][0]
    print("-" * 78)
    print(f"VERDICT: DIVERGENT — first sustained divergence '{e['field']}' "
          + (f"on {e['scope']} slot {e['slot']}" if e["slot"] is not None else f"({e['scope']})")
          + f" at tick {e['tick']} (faithful={bd._fmt(e['a'])} patched={bd._fmt(e['b'])}).")
    print("  Lead, not proof. Confirm same-entity (slot salt + early position), then hand to")
    print("  Tier-B: unicorn_diff --state-snapshot on the implicated function, or toggle-bisect.")
    print(f"  Detail: python3 tools/equivalence/behavior_diff.py {golden} {candidate} --json out.json")
    return 3


if __name__ == "__main__":
    sys.exit(main())
