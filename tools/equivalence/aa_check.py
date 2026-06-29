#!/usr/bin/env python3
"""A/A determinism check for trajectory testing (docs/ab-trajectory-testing.md).

Replays ONE input fixture twice on the SAME build (cachebeta by default) and
diffs the two captured state trajectories. The deterministic engine + identical
input + same core must reproduce byte-identically; the exact-tick overlap MUST be
clean. This is the go/no-go before goldens or CI: if A/A diverges, the
non-determinism is in the harness/engine/capture, not in any lift, and the whole
A/B oracle is unsound.

It also exercises the capture loop's pause-perturbation and the diff aligner on a
real recording pair (untested outside selftest), in one shot.

    python3 tools/equivalence/aa_check.py --level a10 --scenario a10-checkpoint-5s-action

Each run: capture_scenario.py replay (deterministic input, fresh boot) -> wait
for gameplay -> capture_trajectory.py (HMRC). Then trajectory_diff.
"""
import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
REPLAY = ROOT / "tools" / "xbox" / "capture_scenario.py"
CAPTURE = ROOT / "tools" / "xbox" / "capture_trajectory.py"
sys.path.insert(0, str(ROOT / "tools" / "equivalence"))
import halorec_to_snapshot as h2s   # noqa: E402
import trajectory_diff as td        # noqa: E402


def run(cmd):
    print(f"  $ {' '.join(str(c) for c in cmd)}")
    cp = subprocess.run(cmd, cwd=str(ROOT))
    if cp.returncode != 0:
        sys.exit(f"error: command failed ({cp.returncode}): {cmd[1]} {cmd[2] if len(cmd) > 2 else ''}")


def capture_run(level, scenario, xbe, host, out, ticks, quantum, no_wait_spawn):
    replay_cmd = ["python3", str(REPLAY), "replay", "--level", level,
                  "--scenario", scenario, "--xbe", xbe]
    if host:
        replay_cmd += ["--host", host]
    run(replay_cmd)
    cap_cmd = ["python3", str(CAPTURE), "-o", str(out), "--name", out.stem,
               "--ticks", str(ticks), "--quantum", str(quantum)]
    if no_wait_spawn:
        cap_cmd.append("--no-wait-spawn")
    run(cap_cmd)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--level", required=True)
    ap.add_argument("--scenario", required=True)
    ap.add_argument("--xbe", default="cachebeta.xbe",
                    help="build to run BOTH times (default cachebeta.xbe = faithful)")
    ap.add_argument("--ticks", type=int, default=200)
    ap.add_argument("--quantum", type=int, default=1)
    ap.add_argument("--host", default="")
    ap.add_argument("--no-wait-spawn", action="store_true")
    ap.add_argument("--out-dir", type=Path,
                    default=ROOT / "tmp" / "aa_check")
    ap.add_argument("--reuse", action="store_true",
                    help="skip capture; diff existing run1/run2 halorec in --out-dir")
    a = ap.parse_args(argv)

    a.out_dir.mkdir(parents=True, exist_ok=True)
    run1 = a.out_dir / f"{a.level}_{a.scenario}_run1.halorec"
    run2 = a.out_dir / f"{a.level}_{a.scenario}_run2.halorec"

    if not a.reuse:
        print("== A/A run 1 ==")
        capture_run(a.level, a.scenario, a.xbe, a.host, run1, a.ticks, a.quantum,
                    a.no_wait_spawn)
        print("== A/A run 2 ==")
        capture_run(a.level, a.scenario, a.xbe, a.host, run2, a.ticks, a.quantum,
                    a.no_wait_spawn)

    _, _, framesA = h2s.parse_halorec(str(run1))
    _, _, framesB = h2s.parse_halorec(str(run2))
    res = td.diff_trajectories(framesA, framesB)
    print()
    print("=" * 78)
    print(f"A/A CHECK: {a.level}/{a.scenario} on {a.xbe} (x2)")
    if "error" in res:
        sys.exit(f"error: {res['error']}")
    print(f"  run1={res['ticks_A']} ticks  run2={res['ticks_B']} ticks  "
          f"matched={res['matched']}  clean={res['clean_matched']}  "
          f"divergent={res['divergent_matched']}")
    if res["matched"] == 0:
        print("VERDICT: INCONCLUSIVE — no exact-tick overlap (capture cadence/anchor "
              "differs between runs). Lower --quantum or investigate anchoring.")
        return 2
    if res["divergent_matched"] == 0:
        print("VERDICT: CLEAN — A/A is deterministic. Harness sound; proceed to A/B.")
        return 0
    e = res["first_divergence"]
    print(f"VERDICT: DIVERGENT — first at rel={e['rel']} pool={e['pool']} kind={e['kind']}.")
    print("  A/A must be clean; investigate capture atomicity / engine nondeterminism")
    print("  / salt assignment before building goldens. Re-run trajectory_diff for detail:")
    print(f"    python3 tools/equivalence/trajectory_diff.py {run1} {run2}")
    return 3


if __name__ == "__main__":
    sys.exit(main())
