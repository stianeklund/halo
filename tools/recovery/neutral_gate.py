#!/usr/bin/env python3
"""Fast byte-identical gate for one TU (readability campaign worker loop).

``capture --source TU --dir D`` compiles TU through its CMake rule and records
the COFF and assertion-metadata baselines in D.  ``check --source TU --dir D``
recompiles and passes only if the object is byte-identical to the baseline
(symbol names excused via ``--rename-map``) and no assertion's emitted metadata
moved.  Recompiles hit the compiler cache, so a check takes about a second.

This proves neutrality against the candidate baseline only.  It is not a VC71
or equivalence gate; the landing orchestrator still runs those.
"""

import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / "build"
GUARDS = ROOT / "tools" / "recovery"


def _object_for(source):
    rel = Path(source).resolve().relative_to(ROOT).as_posix()
    return "CMakeFiles/halo.dir/%s.obj" % rel


def _compile(source):
    target = _object_for(source)
    proc = subprocess.run(
        ["make", "-s", "-f", "CMakeFiles/halo.dir/build.make", target],
        cwd=BUILD, capture_output=True, text=True)
    if proc.returncode != 0:
        sys.stdout.write(proc.stdout[-2000:] + proc.stderr[-2000:])
        return None
    return BUILD / target


def _guard(script, *args):
    proc = subprocess.run([sys.executable, str(GUARDS / script)] + list(args),
                          capture_output=True, text=True)
    return proc.returncode, (proc.stdout + proc.stderr).strip()


def cmd_capture(args):
    out = Path(args.dir)
    out.mkdir(parents=True, exist_ok=True)
    obj = _compile(args.source)
    if obj is None:
        print("REJECT compile failed")
        return 1
    for script, name, target in (("coff_candidate_guard.py", "coff.json", str(obj)),
                                 ("assert_metadata_guard.py", "asserts.json", args.source)):
        code, text = _guard(script, "capture", "-o", str(out / name), target)
        if code != 0:
            print("REJECT %s capture failed: %s" % (script, text[-500:]))
            return 1
    print("captured %s -> %s" % (args.source, out))
    return 0


def cmd_check(args):
    out = Path(args.dir)
    obj = _compile(args.source)
    if obj is None:
        print("REJECT compile failed")
        return 1
    failures = []
    coff_args = ["check", str(out / "coff.json"), str(obj)]
    if args.rename_map:
        coff_args += ["--rename-map", args.rename_map]
    code, text = _guard("coff_candidate_guard.py", *coff_args)
    if code != 0:
        failures.append("coff: " + text[-800:])
    code, text = _guard("assert_metadata_guard.py", "check", str(out / "asserts.json"),
                        args.source)
    if code != 0:
        failures.append("asserts: " + text[-800:])
    for line in failures:
        print("FAIL  " + line)
    print("PASS" if not failures else "REJECT")
    return 0 if not failures else 1


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    for name in ("capture", "check"):
        p = sub.add_parser(name)
        p.add_argument("--source", required=True)
        p.add_argument("--dir", required=True)
        if name == "check":
            p.add_argument("--rename-map")
    args = parser.parse_args(argv)
    return cmd_capture(args) if args.command == "capture" else cmd_check(args)


if __name__ == "__main__":
    sys.exit(main())
