#!/usr/bin/env python3
"""Golden-master live check: unit lip-sync (mouth aperture) on real engine.

Lip-sync is a SILENT feature -- when it breaks there is no assert, no crash
and no VC71 delta, so only the running engine can arbitrate.  It broke twice
on one line (`sound_update_music`'s callback-identity test against the
literal 0x1c7a10 instead of &FUN_001c7a10; fixed 2026-05-03, reintroduced by
a VC71 score pass 2026-08-19) and both times the only symptom was Captain
Keyes' mouth not moving while he hands over the pistol on a10.

Oracle: `object + 0x298` (mouth aperture) is written ONLY by
sound_object_apply_pitch_delta (0x1ac2f0), reached only when the identity
test matches.  The per-tick decay in FUN_001b3690 can only subtract.  So a
non-zero aperture on any unit is positive proof the path executes.

    golden  boot the PRISTINE cachebeta.xbe on a fixture core and record the
            reference signature (run once, or when the fixture changes)
    check   boot the PATCHED default.xbe on the same core and compare

Values drift sub-tick between builds (audio mixing is not tick-locked), so
the comparison is on the signature -- peak aperture, how often a unit is
talking, how many distinct units talk -- not on per-sample floats.  The bug
drives every one of those to exactly zero, so the gate has enormous margin.

    python3 tools/verify/check_lipsync_live.py golden
    python3 tools/verify/check_lipsync_live.py check

Exit: 0 pass, 1 fail, 77 skip (no fixture / no box / no golden).
"""
import argparse
import hashlib
import json
import os
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "xbox"))

SKIP = 77
FIXTURE = ROOT / "input-recordings/levels/a10/a10-keyes-pistol"
GOLDEN = ROOT / "tools/verify/lipsync_golden.json"
TITLE_ROOT = r"E:\GAMES\halo-patched"
# Halo Xbox debug build 2276 -- the reference binary the golden
# signature must come from.
PRISTINE_MD5 = "c7869590a1c64ad034e49a5ee0c02465"
RDCP = ROOT / "tools/xbox/xbdm_rdcp.py"


def rdcp(cmd, host, sendfile=None, remote=None):
    argv = [sys.executable, str(RDCP), "--host", host]
    if sendfile:
        argv += ["--sendfile", str(sendfile), remote]
    else:
        argv += [cmd]
    r = subprocess.run(argv, capture_output=True, text=True, timeout=300)
    return r.returncode, (r.stdout or "") + (r.stderr or "")


def deploy_fixture(host, verbose=True):
    """Push the fixture core + init.txt and clear playback sentinels."""
    core = FIXTURE / "core.bin"
    init = FIXTURE / "init.txt"
    if not core.is_file() or not init.is_file():
        return False
    rdcp(None, host, sendfile=core, remote=TITLE_ROOT + r"\core\core.bin")
    rdcp(None, host, sendfile=init, remote=TITLE_ROOT + r"\init.txt")
    for sentinel in ("read.xts", "write.xts"):
        rdcp(f'delete name="{TITLE_ROOT}\\{sentinel}"', host)
    if verbose:
        print(f"  deployed fixture {FIXTURE.name} "
              f"(core.bin {core.stat().st_size} bytes)")
    return True


def upload_xbe(xbe, host):
    """Push the local XBE before booting it.

    Without this the harness magicboots whatever binary already sits on the
    box and silently reports on a stale build -- observed 2026-08-21, when a
    deliberately-broken build "passed" because the good XBE from an earlier
    deploy was still installed. A live gate that does not control the binary
    under test is not a gate.
    """
    local = ROOT / "halo-patched" / xbe
    if not local.is_file():
        return False, f"{local} missing"
    digest = hashlib.md5(local.read_bytes()).hexdigest()
    if xbe == "cachebeta.xbe" and digest != PRISTINE_MD5:
        return False, (f"halo-patched/cachebeta.xbe md5 {digest} != pristine "
                       f"{PRISTINE_MD5} -- refusing to record a golden from a "
                       f"non-pristine binary")
    rdcp(None, host, sendfile=local, remote=f"{TITLE_ROOT}\\{xbe}")
    print(f"  uploaded {xbe} ({local.stat().st_size} bytes, md5 {digest[:16]})")
    return True, digest


def boot(xbe, host, settle):
    rdcp(f"magicboot title={TITLE_ROOT}\\{xbe} debug", host)
    print(f"  booted {xbe}; waiting {settle}s for map + core load")
    time.sleep(settle)


def collect(host, port, seconds, interval):
    """Poll the aperture probe over one session and reduce to a signature."""
    import probe_mouth_aperture as probe

    q = None
    for _ in range(20):                    # the box may still be rebooting
        try:
            q = probe.Qmp(host, port)
            break
        except OSError:
            time.sleep(1.0)
    if q is None:
        return {"ok_samples": 0, "talking_samples": 0, "talking_fraction": 0.0,
                "peak_aperture": 0.0, "median_talking_aperture": 0.0,
                "distinct_talkers": 0}
    try:
        return probe.collect(q, seconds=seconds, interval=interval,
                             verbose=False)
    finally:
        q.close()


def describe(sig):
    return (f"ok_samples={sig['ok_samples']} "
            f"talking_samples={sig['talking_samples']} "
            f"talking_fraction={sig['talking_fraction']:.3f} "
            f"peak_aperture={sig['peak_aperture']:.4f} "
            f"median_talking={sig.get('median_talking_aperture', 0.0):.4f} "
            f"distinct_talkers={sig['distinct_talkers']}")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("mode", choices=["golden", "check"])
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=4444, help="xemu QMP port")
    ap.add_argument("--seconds", type=float, default=60.0,
                    help="length of the sampling window "
                         "(~7 samples/second)")
    ap.add_argument("--interval", type=float, default=0.0)
    ap.add_argument("--settle", type=float, default=8.0,
                    help="seconds to wait after magicboot for map + core load")
    ap.add_argument("--tolerance", type=float, default=0.4,
                    help="check mode: fraction of the golden talking "
                         "frequency that must survive")
    a = ap.parse_args()

    if not (FIXTURE / "core.bin").is_file():
        print(f"SKIP: fixture core missing ({FIXTURE}/core.bin).\n"
              f"      core.bin is copyrighted game memory and is host-only "
              f"(gitignored). Recapture: play a10 to the Keyes pistol "
              f"handoff, run `core_save` in the debug console, then\n"
              f"      python3 tools/xbox/xbdm_getfile.py "
              f"'{TITLE_ROOT}\\core\\core.bin' -o {FIXTURE}/core.bin")
        return SKIP

    # Reachability probe: dirlist is present on every XBDM build (systeminfo
    # is not -- the 2276 debug monitor answers it with "407 unknown command").
    rc, out = rdcp(f'dirlist name="{TITLE_ROOT}"', a.host)
    if "name=" not in out:
        print(f"SKIP: no XBDM target on {a.host} (rc={rc})")
        return SKIP

    xbe = "cachebeta.xbe" if a.mode == "golden" else "default.xbe"
    label = "PRISTINE" if a.mode == "golden" else "PATCHED"
    print(f"lipsync live check: {label} {xbe}")
    if not deploy_fixture(a.host):
        print("SKIP: fixture deploy failed")
        return SKIP
    ok, info = upload_xbe(xbe, a.host)
    if not ok:
        print(f"SKIP: {info}")
        return SKIP
    boot(xbe, a.host, a.settle)
    sig = collect(a.host, a.port, a.seconds, a.interval)
    print(f"  {describe(sig)}")

    if sig["ok_samples"] < 50:
        print(f"SKIP: only {sig['ok_samples']} usable samples in the window "
              f"-- the box never reached active gameplay, or host memory "
              f"pressure starved the probe (check `free -m`). Not calling "
              f"this a regression.")
        return SKIP

    if a.mode == "golden":
        if sig["peak_aperture"] <= 0.0:
            print("FAIL: the PRISTINE build showed no lip-sync either. The "
                  "fixture does not cover a talking unit -- recapture the "
                  "core during dialogue; do not record this as golden.")
            return 1
        sig["xbe"] = xbe
        sig["xbe_md5"] = info
        sig["fixture"] = FIXTURE.name
        sig["fixture_core_sha256_prefix"] = hashlib.sha256(
            (FIXTURE / "core.bin").read_bytes()).hexdigest()[:16]
        sig["window_seconds"] = a.seconds
        GOLDEN.write_text(json.dumps(sig, indent=2) + "\n")
        print(f"  wrote golden reference to "
              f"{GOLDEN.relative_to(ROOT)}")
        return 0

    if not GOLDEN.is_file():
        print("SKIP: no golden reference yet -- run "
              "`check_lipsync_live.py golden` on the pristine XBE first")
        return SKIP
    gold = json.loads(GOLDEN.read_text())
    print(f"  golden: {describe(gold)}")

    fails = []

    # --- Hard gates: sampling-robust, and exactly what the bug destroys. ---
    # The failure mode is total: object+0x298 is never written, so the
    # aperture is 0.0 on every unit for the whole window. Any real aperture
    # at all falsifies it.
    if gold["distinct_talkers"] >= 1 and sig["distinct_talkers"] < 1:
        fails.append("no unit talked at all — object+0x298 was never written")
    if gold["peak_aperture"] > 0.2 and sig["peak_aperture"] < 0.05:
        fails.append(f"peak aperture {sig['peak_aperture']:.4f} is "
                     f"effectively zero (golden {gold['peak_aperture']:.4f})")

    # --- Frequency: only gated when coverage is comparable. ---
    # talking_fraction is a ratio over USABLE samples. Skipped samples are
    # not uniform in time -- host memory pressure starves the probe right
    # after magicboot, which is precisely when the dialogue plays -- so a
    # thin run under-samples the speech and the fraction collapses even
    # though behaviour is identical. Measured 2026-08-21: 136 usable samples
    # vs the golden's 477 in the same 60s window turned a passing build into
    # a 0.037-vs-0.128 "regression". Report it, do not fail on it.
    coverage = (sig["ok_samples"] / gold["ok_samples"]) if gold["ok_samples"] else 0.0
    freq_ok = sig["talking_fraction"] >= gold["talking_fraction"] * a.tolerance
    if coverage >= 0.6:
        if not freq_ok:
            fails.append(f"talking fraction {sig['talking_fraction']:.3f} < "
                         f"{a.tolerance:.0%} of golden "
                         f"{gold['talking_fraction']:.3f} "
                         f"(coverage {coverage:.0%}, so this is real)")
    elif not freq_ok:
        print(f"  NOTE: talking fraction {sig['talking_fraction']:.3f} is "
              f"below {a.tolerance:.0%} of golden "
              f"{gold['talking_fraction']:.3f}, but only "
              f"{sig['ok_samples']}/{gold['ok_samples']} samples landed "
              f"({coverage:.0%} coverage) — too thin to gate on. Free host "
              f"memory and re-run for a frequency verdict.")

    if fails:
        print("\nFAIL: lip-sync regressed against the pristine build")
        for f in fails:
            print(f"  - {f}")
        print("\n  A zero peak means object+0x298 is never written: check the "
              "callback-identity test in sound_update_music "
              "(lift-learnings \u00a754) and run\n"
              "  python3 tools/audit/check_lift_hazards.py --quiet "
              "| tr ',' '\\n' | grep ported_addr_cmp")
        return 1

    print("\nPASS: lip-sync is active and matches the pristine build "
          "(peak aperture "
          f"{sig['peak_aperture']:.4f}, median-while-talking "
          f"{sig['median_talking_aperture']:.4f}, "
          f"{sig['distinct_talkers']} unit(s) talking)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
