#!/usr/bin/env python3
"""Prove that kb.json `ported`-flag toggles are LIVE in the running xemu image.

WHY THIS EXISTS
---------------
The deploy gate's `rev` token proves running==built *source*, but it is a PRE-PATCH
token: it does NOT change when you flip `ported` flags in kb.json. So a toggle-bisect
can be silently poisoned by a stale/cached XBE -- the orb (or other visual) verdict you
record is against a build that never actually had your toggle applied. An entire prior
session was burned this way (see docs/plasma-orb-bug.md, EXP-1/EXP-3, and memory
`feedback_deploy_self_verify` / `feedback_toggle_bisect_visual_regression`).

This tool closes that gap by reading the running image's bytes at each function's
virtual address via xemu QMP (`x/Nwx`) and classifying what the CPU will actually run:

  * push-ret trampoline  `68 <impl> C3`   -> ACTIVE   (redirected to our C impl; ported=true)
  * jmp rel32            `E9 <rel->impl>`  -> ACTIVE
  * anything else (a genuine prologue)     -> ORIGINAL (running original code; ported=false)

This matches patch.py exactly: ported=true writes `push <hook_target>; ret` at the
original VA (patch.py ~L1326); ported=false leaves the original XBE bytes intact and
puts the redirect at the IMPL entry instead (generate_deactivation_redirect).

ASYMMETRIC ASSERTIONS (important -- read before trusting output)
----------------------------------------------------------------
`ported=true` does NOT guarantee a redirect at the original VA. Two legitimate cases
show original bytes there anyway: (a) thunks, which patch.py skips entirely, and
(b) phantom-ported functions (ported=true but absent from EXE exports -- the recurring
"marked ported without an impl" bug). So we never hard-fail an individual active.

  * HARD FAIL: any function toggled OFF (`ported=false` + `_orb_toggled`) that still
               shows a redirect -> the toggle is NOT live (stale image).
  * HARD FAIL (control): of N sampled `ported=true` functions, if ZERO show a redirect
               the running image is UNPATCHED/stale and ALL results are void.
  * Otherwise PASS.

USE WINDOW: run AFTER deploy and BEFORE `git checkout kb.json`. The working-tree kb.json
must be the exact state you deployed; this tool compares the live image against it.

Usage:
  verify_toggles_live.py                       # verify toggled-off set + positive control
  verify_toggles_live.py --all-off             # check every ported=false func, not just _orb_toggled
  verify_toggles_live.py --addr 0x13ab20 ...   # ad-hoc: just classify specific VAs
  verify_toggles_live.py --controls 16         # positive-control sample size (default 12)
  verify_toggles_live.py --pause               # pause VM around reads (default: read while running)

Exit codes: 0 = all good; 1 = a toggle/control assertion failed; 2 = QMP/usage error.
"""
import argparse
import json
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
KB = os.path.join(ROOT, "kb.json")
QMP = os.path.join(ROOT, "tools", "xbox", "xemu_qmp.py")

# Our reimplementation/thunk code is appended above the original XBE image; the
# original code lives below this. Used only as an informational sanity floor on
# redirect targets, never as a hard gate (image base can move between builds).
IMPL_REGION_FLOOR = 0x642000

_LINE_RE = re.compile(r"^[0-9a-fA-F]+:\s*(.*)$")
_WORD_RE = re.compile(r"0x([0-9a-fA-F]{1,8})")


def hmp(monitor_cmd, host=None, port=None):
    """Run one HMP command via xemu_qmp.py, return stdout text (raises on failure)."""
    # Global options (--host/--port) must precede the `hmp` subcommand for argparse.
    cmd = [sys.executable, QMP]
    if host:
        cmd += ["--host", host]
    if port:
        cmd += ["--port", str(port)]
    cmd += ["hmp", monitor_cmd]
    res = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
    out = (res.stdout or "").strip()
    if res.returncode != 0 or not out:
        raise RuntimeError(
            f"QMP command failed ({monitor_cmd!r}): rc={res.returncode} "
            f"stderr={(res.stderr or '').strip()!r} stdout={out!r}"
        )
    return out


def read_bytes(va, nwords=3, host=None, port=None):
    """Read nwords*4 bytes at virtual address `va`; returns a bytes object.

    Parses xemu's `x/Nwx` output, e.g.:
        0013ab20: 0x6e435068 0x0000c300 0x57565300
    Each word is little-endian, so 0x6e435068 -> bytes 68 50 43 6e.
    """
    out = hmp(f"x/{nwords}wx 0x{va:x}", host=host, port=port)
    line = None
    for ln in out.splitlines():
        if ":" in ln and "0x" in ln:
            line = ln.strip()
            break
    if line is None:
        raise RuntimeError(f"could not parse QMP read of 0x{va:x}: {out!r}")
    m = _LINE_RE.match(line)
    words = _WORD_RE.findall(m.group(1) if m else line)
    if not words:
        raise RuntimeError(f"no words in QMP read of 0x{va:x}: {line!r}")
    b = b"".join(int(w, 16).to_bytes(4, "little") for w in words)
    return b


def classify(va, b):
    """Return (state, target, detail). state is 'ACTIVE' or 'ORIGINAL'."""
    if len(b) >= 6 and b[0] == 0x68 and b[5] == 0xC3:
        target = int.from_bytes(b[1:5], "little")
        return "ACTIVE", target, f"push-ret -> 0x{target:x}"
    if len(b) >= 5 and b[0] == 0xE9:
        rel = int.from_bytes(b[1:5], "little", signed=True)
        target = (va + 5 + rel) & 0xFFFFFFFF
        return "ACTIVE", target, f"jmp -> 0x{target:x}"
    return "ORIGINAL", None, "prologue " + " ".join(f"{x:02x}" for x in b[:4])


def load_funcs():
    """Yield dicts {obj, name, addr(int), ported, toggled} for funcs with an addr."""
    with open(KB) as f:
        data = json.load(f)
    out = []
    for obj in data.get("objects", []):
        oname = obj.get("name", "?")
        for fn in obj.get("functions", []) or []:
            addr = fn.get("addr")
            if not addr:
                continue
            try:
                va = int(addr, 16)
            except (TypeError, ValueError):
                continue
            out.append({
                "obj": oname,
                "name": fn.get("name", "?"),
                "addr": va,
                "ported": fn.get("ported"),
                "toggled": bool(fn.get("_orb_toggled")),
            })
    return out


def kb_provenance():
    """Short git description of kb.json so a tree/deployed mismatch is loud."""
    try:
        h = subprocess.run(["git", "-C", ROOT, "rev-parse", "--short", "HEAD"],
                           capture_output=True, text=True, timeout=10).stdout.strip()
        dirty = subprocess.run(["git", "-C", ROOT, "status", "--porcelain", "kb.json"],
                              capture_output=True, text=True, timeout=10).stdout.strip()
        return f"HEAD {h}" + ("  (kb.json MODIFIED in tree)" if dirty else "  (kb.json clean)")
    except Exception:
        return "unknown"


def sample_controls(actives, n):
    """Deterministic even spread across the address-sorted active set."""
    actives = sorted(actives, key=lambda f: f["addr"])
    if len(actives) <= n:
        return actives
    return [actives[i * len(actives) // n] for i in range(n)]


def main():
    ap = argparse.ArgumentParser(description="Verify kb.json ported toggles are live in xemu.")
    ap.add_argument("--addr", nargs="+", default=None,
                    help="ad-hoc: classify these VAs (hex) and exit; no assertions")
    ap.add_argument("--all-off", action="store_true",
                    help="check every ported=false function, not just _orb_toggled")
    ap.add_argument("--controls", type=int, default=12,
                    help="positive-control sample size (default 12)")
    ap.add_argument("--pause", action="store_true",
                    help="stop the VM around reads, then continue (default: read while running)")
    ap.add_argument("--host", default=None)
    ap.add_argument("--port", type=int, default=None)
    args = ap.parse_args()

    hp = {"host": args.host, "port": args.port}

    def with_pause(fn):
        if not args.pause:
            return fn()
        hmp("stop", **hp)
        try:
            return fn()
        finally:
            hmp("cont", **hp)

    # Probe QMP up front so a dead connection fails cleanly.
    try:
        hmp("info status", **hp)
    except Exception as e:
        print(f"ERROR: xemu QMP not reachable: {e}", file=sys.stderr)
        return 2

    # Ad-hoc classification mode -- no kb.json, no assertions.
    if args.addr:
        kb_by_addr = {f["addr"]: f for f in load_funcs()}
        def run_addr():
            for a in args.addr:
                va = int(a, 16)
                state, _t, detail = classify(va, read_bytes(va, **hp))
                f = kb_by_addr.get(va)
                kb_flag = ("ported=%s" % f["ported"]) if f else "not in kb.json"
                print(f"  0x{va:08x}  {state:8s}  {detail:28s}  [{kb_flag}]")
        with_pause(run_addr)
        return 0

    funcs = load_funcs()
    toggled = [f for f in funcs
               if f["ported"] is False and (args.all_off or f["toggled"])]
    actives = [f for f in funcs if f["ported"] is True]
    controls = sample_controls(actives, args.controls)

    print(f"verify_toggles_live: kb.json @ {kb_provenance()}")
    print("  (this must be the kb.json state you just DEPLOYED, before `git checkout kb.json`)")
    print(f"  toggled-OFF to verify: {len(toggled)}    positive-control sample: {len(controls)}/{len(actives)} active")

    failures = []
    control_redirects = 0

    def run_checks():
        nonlocal control_redirects
        # Positive control: at least one sampled active must be redirected, else
        # the running image is unpatched/stale and every other result is void.
        print("\n[positive control] sampled ported=true functions (>=1 must be ACTIVE):")
        for f in controls:
            state, _t, detail = classify(f["addr"], read_bytes(f["addr"], **hp))
            if state == "ACTIVE":
                control_redirects += 1
            mark = "ok" if state == "ACTIVE" else ".."  # individual originals are fine (thunk/phantom)
            print(f"  [{mark}] 0x{f['addr']:08x}  {state:8s}  {detail:28s}  {f['obj']}/{f['name']}")

        if not toggled:
            return
        print("\n[toggled-off] each MUST show ORIGINAL (genuine prologue):")
        for f in toggled:
            state, _t, detail = classify(f["addr"], read_bytes(f["addr"], **hp))
            ok = state == "ORIGINAL"
            if not ok:
                failures.append(f)
            print(f"  [{'OK' if ok else 'FAIL'}] 0x{f['addr']:08x}  {state:8s}  {detail:28s}  {f['obj']}/{f['name']}")

    with_pause(run_checks)

    print("\n== summary ==")
    rc = 0
    if control_redirects == 0:
        print(f"  HARD FAIL: 0/{len(controls)} sampled active functions show a redirect.")
        print("             The running image is UNPATCHED or STALE -- all toggle results are VOID.")
        print("             Redeploy the committed build (./tools/xbox/build_deploy_run.sh -q) and retry.")
        rc = 1
    else:
        print(f"  control: {control_redirects}/{len(controls)} actives redirected -> patched build is live.")

    if failures:
        print(f"  HARD FAIL: {len(failures)} toggled-off function(s) still ACTIVE (toggle NOT live):")
        for f in failures:
            print(f"             0x{f['addr']:08x}  {f['obj']}/{f['name']}")
        print("             Your deployed XBE predates these toggles. Rebuild+redeploy before testing.")
        rc = 1
    elif toggled:
        print(f"  toggles: {len(toggled)}/{len(toggled)} toggled-off functions confirmed running ORIGINAL.")
    elif rc == 0:
        print("  no toggled-off functions to verify (kb.json has no _orb_toggled entries).")

    print("  RESULT:", "PASS -- toggles are live; trust your in-game verdict." if rc == 0
          else "FAIL -- do NOT trust any in-game verdict until this passes.")
    return rc


if __name__ == "__main__":
    sys.exit(main())
