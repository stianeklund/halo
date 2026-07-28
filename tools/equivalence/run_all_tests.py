#!/usr/bin/env python3
"""Run every self-test in tools/equivalence/, whichever style it is written in.

Why this exists rather than a bare ``unittest discover``: the suites here come
in two shapes, and discovery only sees one of them.

    unittest.TestCase subclasses   6 files, 49 tests   <- discovery finds these
    plain `sys.exit(main())`       6 files             <- discovery finds NOTHING

``test_stub_arg_trace.py`` is in the second group, so a discovery-only CI step
reports a confident green while running none of the comparator's soundness
pins.  That is worse than no step at all.  Running each file as a subprocess
and trusting its exit code covers both shapes uniformly.

Guards against the two ways this could pass vacuously:

  * a file with no ``__main__`` block executes, prints nothing and exits 0 --
    checked statically, and reported as an ERROR rather than a pass;
  * a file that skips itself because an optional dependency (z3, unicorn) is
    missing is reported as SKIP, never folded into the pass count.

Usage:  python3 tools/equivalence/run_all_tests.py [-v] [--timeout SECONDS]
Exit:   0 all passed (skips allowed), 1 something failed or is unrunnable.
"""
import argparse
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent
MAIN_GUARD = '__name__ == "__main__"'
DEFAULT_TIMEOUT = 300


def _discover():
    """Every test_*.py beside this file, excluding this file itself."""
    return sorted(p for p in HERE.glob("test_*.py") if p.name != Path(__file__).name)


def _has_main_guard(path):
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return False
    return MAIN_GUARD in text or "__name__ == '__main__'" in text


def _run_one(path, timeout):
    """-> (verdict, seconds, output). verdict in PASS/FAIL/SKIP/ERROR."""
    if not _has_main_guard(path):
        # Would exit 0 having run nothing at all.
        return "ERROR", 0.0, ("no `if __name__ == \"__main__\":` block -- this "
                              "file exits 0 without running its tests")
    started = time.time()
    try:
        proc = subprocess.run([sys.executable, str(path)], cwd=str(REPO),
                              capture_output=True, text=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        return "ERROR", time.time() - started, f"timed out after {timeout}s"
    elapsed = time.time() - started
    output = (proc.stdout or "") + (proc.stderr or "")
    if proc.returncode != 0:
        return "FAIL", elapsed, output
    # A self-skip is not a pass. These suites announce it on stdout; unittest
    # reports it as "OK (skipped=N)" with every test skipped.
    if "SKIP" in output or "skipped=" in output:
        return "SKIP", elapsed, output
    return "PASS", elapsed, output


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-v", "--verbose", action="store_true",
                    help="print each suite's own output, not just the verdict")
    ap.add_argument("--timeout", type=int, default=DEFAULT_TIMEOUT,
                    help=f"per-file timeout in seconds (default {DEFAULT_TIMEOUT})")
    args = ap.parse_args()

    files = _discover()
    if not files:
        print("ERROR: no test_*.py found in %s" % HERE, file=sys.stderr)
        return 1

    counts = {"PASS": 0, "FAIL": 0, "SKIP": 0, "ERROR": 0}
    bad = []
    print("Running %d equivalence self-test suites...\n" % len(files))
    for path in files:
        verdict, elapsed, output = _run_one(path, args.timeout)
        counts[verdict] += 1
        print("  %-5s %-34s %5.1fs" % (verdict, path.name, elapsed))
        if verdict in ("FAIL", "ERROR"):
            bad.append((path.name, output))
        if args.verbose and output.strip():
            print("\n".join("        " + ln for ln in output.rstrip().splitlines()))

    for name, output in bad:
        print("\n" + "=" * 68)
        print("%s output:" % name)
        print("=" * 68)
        print(output.rstrip() or "(no output)")

    print("\n%d passed, %d skipped, %d failed, %d unrunnable"
          % (counts["PASS"], counts["SKIP"], counts["FAIL"], counts["ERROR"]))
    return 1 if (counts["FAIL"] or counts["ERROR"]) else 0


if __name__ == "__main__":
    sys.exit(main())
