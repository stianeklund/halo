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
import hashlib
import json
import shlex
import shutil
import struct
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
from capture_profile import PROFILES, normalize_profile  # noqa: E402
from case_manifest import write_case  # noqa: E402

RECIPE_SCHEMA_VERSION = 1


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


def _recording_has_pool(frames, ptr, min_record_size=0):
    """Return true only when a valid used pool span is present in a recording."""
    for frame in frames:
        reader = h2s._reader(frame.regions)
        header = h2s._pool_header(reader, ptr)
        if header is None or header["magic"] != h2s.DATA_T_MAGIC:
            continue
        if header["es"] < min_record_size or header["es"] <= 0:
            continue
        count = header["cur"] if 0 < header["cur"] <= header["max"] else header["max"]
        if 0 < count <= 5000 and reader(header["data"], count * header["es"]) is not None:
            return True
    return False


def _coverage(frames_a, frames_b, cfg, profile):
    """Describe required pool/field coverage across both recordings."""
    profile = normalize_profile(profile)
    required_pools = ["ticks", "object_index", "players", "actors"]
    if profile != "ai-core":
        required_pools.append("props")
    pool_ptrs = {
        "object_index": td.POOLS["objects"],
        "players": td.POOLS["players"],
        "actors": td.POOLS["actors"],
        "props": td.POOLS["props"],
    }
    result = {
        "required_pools": required_pools,
        "required_fields": {},
        "ticks": bool(any(td.frame_tick(h2s._reader(f.regions)) is not None
                           for f in frames_a) and
                      any(td.frame_tick(h2s._reader(f.regions)) is not None
                          for f in frames_b)),
        "object_index": False,
        "object_bodies": False,
        "players": False,
        "actors": False,
        "props": False,
        "effects": False,
        "camera": False,
        "missing_required": [],
        "missing_fields": {},
    }
    for name, ptr in pool_ptrs.items():
        result[name] = bool(_recording_has_pool(frames_a, ptr) and
                            _recording_has_pool(frames_b, ptr))
    if not result["ticks"]:
        result["missing_required"].append("ticks")
    for name in required_pools:
        if name != "ticks" and not result[name]:
            result["missing_required"].append(name)
    for pool_name, pool_cfg in cfg.get("pools", {}).items():
        fields = pool_cfg.get("fields", [])
        labels = [field.get("label", "field") for field in fields]
        result["required_fields"][pool_name] = labels
        if not fields or pool_name not in td.POOLS:
            continue
        try:
            required_size = max(
                int(field["off"]) + struct.calcsize(field["fmt"])
                for field in fields)
        except (KeyError, TypeError, ValueError, struct.error):
            required_size = 1
        if not (_recording_has_pool(frames_a, td.POOLS[pool_name], required_size) and
                _recording_has_pool(frames_b, td.POOLS[pool_name], required_size)):
            result["missing_fields"][pool_name] = labels
    return result


def _empty_coverage(profile):
    profile = normalize_profile(profile)
    required = ["ticks", "object_index", "players", "actors"]
    if profile != "ai-core":
        required.append("props")
    return {
        "required_pools": required,
        "required_fields": {},
        "ticks": False,
        "object_index": False,
        "object_bodies": False,
        "players": False,
        "actors": False,
        "props": False,
        "effects": False,
        "camera": False,
        "missing_required": list(required),
        "missing_fields": {},
    }


def _fixture_input_hash(level, scenario):
    state_data = ROOT / "input-recordings" / "levels" / level / scenario / "state.data"
    if not state_data.is_file():
        return None
    digest = hashlib.sha256()
    with state_data.open("rb") as fh:
        for block in iter(lambda: fh.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _load_recipe(path):
    """Load a viewer v1 recipe and its source case into normalized CLI state."""
    path = Path(path).resolve()
    recipe = json.loads(path.read_text())
    if recipe.get("schema_version", 1) != RECIPE_SCHEMA_VERSION:
        raise ValueError("unsupported focused-capture recipe schema")
    source_value = recipe.get("source_case")
    if not isinstance(source_value, str) or not source_value:
        raise ValueError("recipe has no source_case")
    source_case = Path(source_value)
    if not source_case.is_absolute():
        source_case = path.parent / source_case
    source_case = source_case.resolve()
    source = json.loads(source_case.read_text())
    if source.get("schema_version", 1) != 1:
        raise ValueError("unsupported source case schema")
    fixture = source.get("fixture") or {}
    level = fixture.get("level")
    scenario = fixture.get("scenario")
    if not level or not scenario:
        raise ValueError("source case has no fixture level/scenario")

    onset = int(recipe.get("onset_tick", 0))
    capture = source.get("capture") or {}
    faithful_anchor = (capture.get("gameplay_anchors") or {}).get("faithful")
    if faithful_anchor is None:
        faithful_value = (source.get("faithful") or {}).get("path")
        if faithful_value:
            faithful_path = Path(faithful_value)
            if not faithful_path.is_absolute():
                faithful_path = source_case.parent / faithful_path
            faithful_anchor = _recording_anchor(faithful_path)
    onset_relative = onset - int(faithful_anchor) if faithful_anchor is not None else onset
    if onset_relative < 0:
        raise ValueError("recipe onset precedes the source gameplay anchor")
    before = int(recipe.get("window_before", 30))
    after = int(recipe.get("window_after", 60))
    if before < 0 or after < 0:
        raise ValueError("recipe windows must be non-negative")
    quantum = int(recipe.get("quantum", 1))
    if quantum != 1:
        raise ValueError("focused recapture requires quantum 1")
    actor_slots = [int(slot) for slot in recipe.get("actor_slots", [])]
    if not actor_slots or any(slot < 0 or slot > 0xFFFF for slot in actor_slots):
        raise ValueError("recipe actor_slots must contain valid datum slots")
    relation_cap = int(recipe.get("max_relation_nodes", 16))
    if relation_cap < 0 or relation_cap > 16:
        raise ValueError("max_relation_nodes must be in range 0..16")

    flags = {
        "include_perception": bool(recipe.get("include_perception", False)),
        "include_linked_object_body": bool(
            recipe.get("include_linked_object_body", False)),
        "include_weapon_bodies": bool(recipe.get("include_weapon_bodies", False)),
        "include_object_relations": bool(
            recipe.get("include_object_relations", False)),
    }
    capture_args = [
        "--tick-start", str(max(0, onset_relative - before)),
        "--focused-actor-slots", ",".join(str(slot) for slot in actor_slots),
        "--max-relation-nodes", str(relation_cap),
    ]
    for enabled, option in (
        (flags["include_perception"], "--include-focused-perception"),
        (flags["include_linked_object_body"], "--include-focused-linked-bodies"),
        (flags["include_weapon_bodies"], "--include-focused-weapon-bodies"),
        (flags["include_object_relations"], "--include-focused-object-relations"),
    ):
        if enabled:
            capture_args.append(option)
    return {
        "path": path,
        "source_case": source_case,
        "level": str(level),
        "scenario": str(scenario),
        "input_hash": fixture.get("input_hash"),
        "onset_relative": onset_relative,
        "tick_start": max(0, onset_relative - before),
        "tick_end": onset_relative + after,
        "quantum": quantum,
        "actor_slots": actor_slots,
        "max_relation_nodes": relation_cap,
        "flags": flags,
        "capture_args": capture_args,
        "alignment_window": capture.get("alignment_window"),
        "minimum_sustained_run": capture.get("minimum_sustained_run"),
    }


def _recording_anchor(path):
    try:
        _, _, frames = h2s.parse_halorec(str(path))
    except (Exception, SystemExit):
        return None
    ticks = [td.frame_tick(h2s._reader(frame.regions)) for frame in frames]
    ticks = [tick for tick in ticks if tick is not None]
    return min(ticks) if ticks else None


def _write_case(a, tag, golden, candidate, report, verdict, coverage,
                candidate_verification, diagnostics=(), result=None,
                gameplay_anchors=None):
    metrics = {"profile": a.profile}
    if result is not None:
        metrics["frames"] = result.get("framesA", 0) + result.get("framesB", 0)
    path = a.out_dir / f"{tag}.halocase.json"
    write_case(
        path,
        level=a.level,
        scenario=a.scenario,
        profile=a.profile,
        backend="xemu-qmp",
        ticks=a.ticks,
        quantum=a.quantum,
        alignment_window=(result or {}).get("window", 0),
        minimum_sustained_run=(result or {}).get("min_run", a.min_run or 0),
        faithful=golden,
        candidate=candidate,
        behavior_report=report,
        faithful_build=a.golden_xbe,
        candidate_build=a.candidate_xbe,
        candidate_verification=candidate_verification,
        verdict=verdict,
        coverage=coverage,
        metrics=metrics,
        parent_case=a.parent_case,
        diagnostics=diagnostics,
        input_hash=a.input_hash,
        tick_start=a.tick_start,
        gameplay_anchors=gameplay_anchors,
    )
    print(f"  [case] -> {path}")
    return path


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--level")
    ap.add_argument("--scenario")
    ap.add_argument("--recipe", type=Path,
                    help="viewer-generated v1 .halocapture.json focused recapture")
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
    ap.add_argument("--ticks", type=int, default=None)
    ap.add_argument("--quantum", type=int, default=None)
    ap.add_argument("--profile", choices=PROFILES, default=None,
                    help="capture profile for both recordings (default: ai-core)")
    ap.add_argument("--window", type=int, default=None, help="behavior_diff +/- tick tolerance")
    ap.add_argument("--min-run", type=int, default=None, help="behavior_diff sustained-onset run")
    ap.add_argument("--config", type=Path, default=None, help="watch-list JSON (default: a10 AI)")
    ap.add_argument("--report", type=Path, default=None,
                    help="also write the behavior_diff result JSON here; ingestible by the "
                         "halo-memory-viewer Compare tab ('Load behavior_diff report') to jump "
                         "to the onset tick + entity")
    ap.add_argument("--host", default="")
    ap.add_argument("--no-wait-spawn", action="store_true")
    ap.add_argument("--out-dir", type=Path, default=ROOT / "tmp" / "ab_check")
    ap.add_argument("--reuse", action="store_true",
                    help="skip captures; diff existing golden/candidate in --out-dir")
    a = ap.parse_args(argv)
    recipe = None
    if a.recipe:
        try:
            recipe = _load_recipe(a.recipe)
        except (Exception, SystemExit) as exc:
            ap.error(f"invalid --recipe: {exc}")
        if a.level and a.level != recipe["level"]:
            ap.error("--level does not match the recipe source case")
        if a.scenario and a.scenario != recipe["scenario"]:
            ap.error("--scenario does not match the recipe source case")
        if a.profile and normalize_profile(a.profile) != "ai-core":
            ap.error("focused recapture always uses the ai-core base profile")
        a.level = recipe["level"]
        a.scenario = recipe["scenario"]
        a.profile = "ai-core"
        a.tick_start = recipe["tick_start"]
        a.ticks = recipe["tick_end"]
        a.quantum = 1
        a.parent_case = recipe["source_case"]
        a.input_hash = recipe["input_hash"] or _fixture_input_hash(a.level, a.scenario)
        if a.window is None and recipe["alignment_window"] is not None:
            a.window = int(recipe["alignment_window"])
        if a.min_run is None and recipe["minimum_sustained_run"] is not None:
            a.min_run = int(recipe["minimum_sustained_run"])
    else:
        if not a.level or not a.scenario:
            ap.error("--level and --scenario are required without --recipe")
        a.profile = normalize_profile(a.profile or "ai-core")
        a.tick_start = 0
        a.ticks = 200 if a.ticks is None else a.ticks
        a.quantum = 1 if a.quantum is None else a.quantum
        a.parent_case = None
        a.input_hash = _fixture_input_hash(a.level, a.scenario)
    if a.ticks < a.tick_start or a.quantum <= 0:
        ap.error("invalid capture tick range or quantum")

    a.out_dir.mkdir(parents=True, exist_ok=True)
    tag = (f"{a.level}_{a.scenario}_focused_t{recipe['onset_relative']}"
           if recipe else f"{a.level}_{a.scenario}")
    golden = a.out_dir / f"{tag}_golden.halorec"
    candidate = a.out_dir / f"{tag}_candidate.halorec"
    report = a.report or a.out_dir / f"{tag}_behavior.json"
    candidate_verification = "unverified"
    gameplay_anchors = {"faithful": None, "candidate": None}

    def abort(reason):
        print(f"VERDICT: INCONCLUSIVE — {reason}")
        _write_case(a, tag, golden, candidate, report, "INCONCLUSIVE",
                    _empty_coverage(a.profile), candidate_verification,
                    diagnostics=(reason,), gameplay_anchors=gameplay_anchors)
        return 2

    def cap(xbe, out):
        if recipe:
            return capture_run(
                a.level, a.scenario, xbe, a.host, out, a.ticks, a.quantum,
                a.no_wait_spawn, a.profile, capture_args=recipe["capture_args"])
        return capture_run(a.level, a.scenario, xbe, a.host, out, a.ticks,
                           a.quantum, a.no_wait_spawn, a.profile)

    def remember_anchor(role, metadata, recording):
        anchor = metadata.get("anchor_tick") if isinstance(metadata, dict) else None
        gameplay_anchors[role] = anchor if anchor is not None else _recording_anchor(recording)

    unverified = False  # did the candidate capture run an unproven build?

    if not a.reuse:
        # 0. Make the candidate on-box build == local source, and PROVE it.
        #    Fail-fast: do this BEFORE spending minutes capturing the golden.
        deployed = False
        if a.deploy and a.candidate_xbe == "default.xbe":
            if deploy_candidate(a.host, a.build_args) != 0:
                return abort("deploy/build-verify FAILED; the box is NOT running the local build")
            deployed = True
            candidate_verification = "verified"
        elif a.deploy:
            print(f"  [deploy] WARNING candidate-xbe={a.candidate_xbe!r} is not default.xbe; "
                  "deploy_xbox only writes default.xbe, so auto-deploy is SKIPPED.")

        run_gate = (deployed and a.verify_live_after_deploy) or a.verify_live
        if run_gate:
            if liveness_gate(a.host, a.port) != 0:
                return abort("liveness gate FAILED; the running image is stale/unpatched or a toggle did not take")
            candidate_verification = "verified"

        unverified = not deployed and not run_gate

        # 1. golden (faithful) — reuse a frozen one if given and present
        if a.golden and a.golden.exists() and not a.freeze:
            print(f"== reusing frozen golden: {a.golden} ==")
            golden = a.golden
            remember_anchor("faithful", None, golden)
        else:
            print(f"== capture golden ({a.golden_xbe}) ==")
            try:
                metadata = cap(a.golden_xbe, golden)
                remember_anchor("faithful", metadata, golden)
            except (Exception, SystemExit) as exc:
                return abort(f"golden capture failed: {exc}")
            if a.freeze and a.golden:
                a.golden.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy(golden, a.golden)
                print(f"  [freeze] golden -> {a.golden}")

        # 1b. optional A/A determinism check (second faithful capture, strict diff)
        if a.aa_first:
            aa_run2 = a.out_dir / f"{tag}_aa_run2.halorec"
            print(f"== A/A check: second {a.golden_xbe} capture ==")
            try:
                cap(a.golden_xbe, aa_run2)
                _, _, fa = h2s.parse_halorec(str(golden))
                _, _, fb = h2s.parse_halorec(str(aa_run2))
                aa = td.diff_trajectories(fa, fb)
            except (Exception, SystemExit) as exc:
                return abort(f"A/A validation failed: {exc}")
            if aa.get("error") or aa["matched"] == 0:
                return abort("A/A had no comparable frames; fix capture before trusting A/B")
            if aa["divergent_matched"] != 0:
                e = aa["first_divergence"]
                return abort("A/A is dirty; first divergence at rel=%s %s" %
                             (e["rel"], e["pool"]))
            print(f"  [A/A] CLEAN ({aa['clean_matched']}/{aa['matched']} frames)")

        # 2. candidate (patched)
        print(f"== capture candidate ({a.candidate_xbe}) ==")
        try:
            metadata = cap(a.candidate_xbe, candidate)
            remember_anchor("candidate", metadata, candidate)
        except (Exception, SystemExit) as exc:
            return abort(f"candidate capture failed: {exc}")
    else:
        # --reuse diffs existing captures; build freshness is not (re)checked.
        unverified = True
        if a.golden and a.golden.exists():
            golden = a.golden
        remember_anchor("faithful", None, golden)
        remember_anchor("candidate", None, candidate)

    # 3. tolerant behavioral diff
    cfg = None
    if a.config:
        try:
            cfg = json.loads(a.config.read_text())
        except (Exception, SystemExit) as exc:
            return abort(f"watch-list config failed: {exc}")
    else:
        cfg = dict(bd.DEFAULT_CONFIG)
    if a.window is not None:
        cfg["window"] = a.window
    if a.min_run is not None:
        cfg["min_run"] = a.min_run

    try:
        res = _diff_behavior(golden, candidate, cfg)
        _, _, frames_a = h2s.parse_halorec(str(golden))
        _, _, frames_b = h2s.parse_halorec(str(candidate))
        coverage = _coverage(frames_a, frames_b, cfg, a.profile)
    except (Exception, SystemExit) as exc:
        return abort(f"comparison failed: {exc}")

    res["coverage"] = coverage
    try:
        report.parent.mkdir(parents=True, exist_ok=True)
        report.write_text(json.dumps(res, indent=2) + "\n")
    except (Exception, SystemExit) as exc:
        return abort(f"behavior report write failed: {exc}")
    print(f"  [report] -> {report}  (load in halo-memory-viewer Compare tab)")

    covered = not coverage["missing_required"] and not coverage["missing_fields"]
    verdict = ("INCONCLUSIVE" if not covered else
               ("CLEAN" if res["onset_count"] == 0 else "DIVERGENT"))
    _write_case(a, tag, golden, candidate, report, verdict, coverage,
                candidate_verification, result=res,
                gameplay_anchors=gameplay_anchors)
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
    if not covered:
        print("VERDICT: INCONCLUSIVE — required capture coverage is missing: "
              + ", ".join(coverage["missing_required"] or
                            coverage["missing_fields"].keys()))
        return 2
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
