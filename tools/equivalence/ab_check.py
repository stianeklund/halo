#!/usr/bin/env python3
"""A/B regression check for trajectory testing (docs/ab-trajectory-testing.md).

One command: replay the SAME deterministic input fixture on the faithful build
(cachebeta = the golden) and the patched build (default = the candidate), capture
each build's game-state trajectory, and run the TOLERANT behavioral diff. The
earliest sustained divergence names the time + field + entity to investigate.

    python3 tools/equivalence/ab_check.py --level a10 --scenario a10-checkpoint-5s-action

Tests the build you HAVE, not the one on the box. By default it first
build+deploys local source to default.xbe and gates that the box really runs it:
  1. `build_deploy_run.sh -q` rebuilds ALL targets (patched_xbe is an ALL target,
     so patch.py always re-runs -> a kb.json toggle IS re-patched) and uploads
     default.xbe; deploy_xbox proves running == local via the DECOMP BUILD token.
  2. `verify_toggles_live --all-off` proves the patched build is live AND every
     ported=false function reverted to ORIGINAL on the running image.
A failure in either is reported as INCONCLUSIVE (exit 2) and NO diff is run -- a
wrong CLEAN/DIVERGENT on a stale build is worse than the gap. `--no-deploy` skips
the rebuild (you assert the box is current); add `--verify-live` to still gate it,
else the verdict is flagged "build identity UNVERIFIED".

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
never committed.

Exit codes (keep these distinct — a CI tripwire must tell "harness broke" from
"regression"):  0 = CLEAN,  3 = DIVERGENT (real behavioral difference),
2 = INCONCLUSIVE (deploy failed / liveness gate failed / A/A dirty / no
comparable frames — the run proves nothing, fix the harness/build and retry).
"""
import argparse
import shlex
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
BUILD_DEPLOY = ROOT / "tools" / "xbox" / "build_deploy_run.sh"
VERIFY_TOGGLES = ROOT / "tools" / "xbox" / "verify_toggles_live.py"
sys.path.insert(0, str(ROOT / "tools" / "equivalence"))
import halorec_to_snapshot as h2s   # noqa: E402
import behavior_diff as bd          # noqa: E402
import trajectory_diff as td        # noqa: E402
from aa_check import capture_run     # noqa: E402  (replay + capture_trajectory)


def deploy_candidate(host, build_args):
    """Build local source + upload it to the box's default.xbe, and PROVE the
    running title == that local build via deploy_xbox's DECOMP-BUILD-token verify.

    build.py with no --target builds ALL targets; patched_xbe is an ALL
    custom_target (no output file) so patch.py ALWAYS re-runs and regenerates
    default.xbe from the current kb.json -- a kb.json-only toggle is re-patched.
    (build.py --target halo would skip that and leave a stale default.xbe; we
    never do that here.) Returns the subprocess return code (0 = deployed+verified)."""
    cmd = ["bash", str(BUILD_DEPLOY)] + shlex.split(build_args)
    if host:
        cmd += ["--xbox", host]
    print(f"== deploy candidate (build + upload default.xbe + verify token) ==")
    print(f"  $ {' '.join(cmd)}")
    return subprocess.run(cmd, cwd=str(ROOT)).returncode


def liveness_gate(host, port):
    """verify_toggles_live --all-off: the semantic safety net. Asserts (a) the
    patched build is actually live (>=1 sampled active function is redirected --
    catches a stale/unpatched image) and (b) EVERY ported=false function runs
    ORIGINAL on the running image (--all-off; catches a toggle that did not
    re-patch -- it would show ACTIVE). rc 0 = live & toggles correct; rc 1 =
    stale/toggle-not-live; rc 2 = QMP unreachable. Any nonzero -> abort."""
    cmd = ["python3", str(VERIFY_TOGGLES), "--all-off"]
    if host:
        cmd += ["--host", host]
    if port:
        cmd += ["--port", str(port)]
    print(f"== liveness gate (verify_toggles_live --all-off) ==")
    print(f"  $ {' '.join(cmd)}")
    return subprocess.run(cmd, cwd=str(ROOT)).returncode


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
    ap.add_argument("--no-deploy", dest="deploy", action="store_false",
                    help="skip the auto build+deploy of the candidate (you assert the box "
                         "already runs your local build); verdict is flagged UNVERIFIED "
                         "unless you also pass --verify-live")
    ap.set_defaults(deploy=True)
    ap.add_argument("--build-args", default="-q",
                    help="args forwarded to build_deploy_run.sh -> build.py (default '-q'; "
                         "do NOT pass '--target halo' -- that skips the re-patch)")
    ap.add_argument("--no-verify-live", dest="verify_live_after_deploy", action="store_false",
                    help="skip the verify_toggles_live liveness gate after a deploy")
    ap.set_defaults(verify_live_after_deploy=True)
    ap.add_argument("--verify-live", action="store_true",
                    help="force the liveness gate even under --no-deploy")
    ap.add_argument("--port", type=int, default=None,
                    help="xemu QMP port for the liveness gate (verify_toggles_live default if unset)")
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

    unverified = False  # did the candidate capture run an unproven build?

    if not a.reuse:
        # 0. Make the candidate on-box build == local source, and PROVE it.
        #    Fail-fast: do this BEFORE spending minutes capturing the golden.
        deployed = False
        if a.deploy and a.candidate_xbe == "default.xbe":
            if deploy_candidate(a.host, a.build_args) != 0:
                print("VERDICT: INCONCLUSIVE — deploy/build-verify FAILED; the box is NOT "
                      "running your local build. Fix build/deploy and retry. (no diff run)")
                return 2
            deployed = True
        elif a.deploy:
            print(f"  [deploy] WARNING candidate-xbe={a.candidate_xbe!r} is not default.xbe; "
                  "deploy_xbox only writes default.xbe, so auto-deploy is SKIPPED.")

        run_gate = (deployed and a.verify_live_after_deploy) or a.verify_live
        if run_gate:
            if liveness_gate(a.host, a.port) != 0:
                print("VERDICT: INCONCLUSIVE — liveness gate FAILED (verify_toggles_live): the "
                      "running image is stale/unpatched or a toggle did not take. (no diff run)")
                return 2

        unverified = not deployed and not run_gate

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
        # --reuse diffs existing captures; build freshness is not (re)checked.
        unverified = True
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
    if unverified:
        print("  [!] BUILD IDENTITY UNVERIFIED — the candidate capture ran whatever was on the "
              "box, not necessarily your local source. Re-run without --no-deploy, or add "
              "--verify-live, to gate it. This verdict may not reflect your build.")
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
