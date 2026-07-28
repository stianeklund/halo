#!/usr/bin/env python3
"""Self-test for lift_pipeline VC71 score attribution.

Run with:
    python3 tools/verify/test_match_attribution.py

Pins the defect found on 2026-07-28: lift_pipeline's vc71_verify stage parsed
the match percentage with a bare `re.search(r'(\\d+\\.\\d+)% match', output)`.
vc71_verify.py prints one `PASS/FAIL <name>: NN.N% match` line for EVERY function
in the translation unit even when --function is passed, so that parse returned
whichever function sorts first by address -- not the target.

Concretely, in src/halo/text/draw_string.c both of these were reported as 67.8%:
    draw_string_get_color   really 100.0%
    FUN_0019c0a0            really  77.5%
67.8% is FUN_0019b3c0's score, the first function in that file.

This was not cosmetic. vc71_match_pct feeds:
  - the permuter [85, 99) eligibility band, and
  - the low_match_policy accept/reject gate,
so a lift could be accepted, rejected, or sent to the permuter on the strength of
an unrelated function's score.
"""

import importlib.util
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def _load_pipeline():
    path = os.path.join(ROOT, "tools", "lift_pipeline.py")
    spec = importlib.util.spec_from_file_location("_lp_under_test", path)
    mod = importlib.util.module_from_spec(spec)
    sys.modules["_lp_under_test"] = mod
    try:
        spec.loader.exec_module(mod)
    except SystemExit:
        pass
    return mod


FAILURES = []


def check(name, cond):
    print(f"  {'ok  ' if cond else 'FAIL'} {name}")
    if not cond:
        FAILURES.append(name)


# Real vc71_verify output shape from src/halo/text/draw_string.c.
SAMPLE = """Using cached VC71 object: draw_string.obj
Comparing against 0019c0a0.obj...
  PASS FUN_0019b3c0: 67.8% match (28/31 insns)
  PASS FUN_0019b430: 89.2% match (79/78 insns)
  PASS FUN_0019b560: 89.9% match (34/35 insns)
  PASS FUN_0019b640: 100.0% match (97/97 insns)
  PASS FUN_0019b800: 100.0% match (52/52 insns)
  PASS FUN_0019bcc0: 76.3% match (47/31 insns)
  PASS FUN_0019c0a0: 77.5% match (44/37 insns)
  PASS draw_string_get_color: 100.0% match (25/25 insns)
"""


def main():
    lp = _load_pipeline()
    f = lp.parse_match_percent_for_function

    print("VC71 score attribution")

    # A1. The historical defect: the unattributed parse yields the FIRST
    #     function in the file, which is why 67.8% was reported for two
    #     unrelated targets. Pinned so nobody "simplifies" back to it.
    check("A1 bare parse returns first function (the old bug)",
          lp.parse_match_percent(SAMPLE) == 67.8)

    # A2. Named target is attributed exactly, not to the first line.
    pct, exact = f(SAMPLE, ["draw_string_get_color", "FUN_0019b790"])
    check("A2 named target attributed (100.0, exact)", (pct, exact) == (100.0, True))

    # A3. FUN_<addr>-spelled target is attributed exactly.
    pct, exact = f(SAMPLE, ["FUN_0019c0a0"])
    check("A3 FUN_ target attributed (77.5, exact)", (pct, exact) == (77.5, True))

    # A4. Fallback order: kb name absent from output, FUN_ spelling present.
    pct, exact = f(SAMPLE, ["kb_only_name", "FUN_0019bcc0"])
    check("A4 second name tried when first absent", (pct, exact) == (76.3, True))

    # A5. Target absent entirely -> flagged inexact so the caller can warn
    #     instead of silently gating on someone else's score.
    pct, exact = f(SAMPLE, ["not_present_anywhere"])
    check("A5 absent target flagged inexact", (pct, exact) == (67.8, False))

    # A6. FAIL lines are parsed too -- a failing target must not fall through to
    #     a passing neighbour's number.
    fail_out = ("  PASS FUN_0019b3c0: 67.8% match (28/31 insns)\n"
                "  FAIL FUN_0019c0a0: 38.1% match (49/99 insns) [LOADW-WARN]\n")
    pct, exact = f(fail_out, ["FUN_0019c0a0"])
    check("A6 FAIL line attributed, not neighbour", (pct, exact) == (38.1, True))

    # A7. Empty / no-match input is None, not a crash.
    pct, exact = f("nothing here\n", ["FUN_00001000"])
    check("A7 no match -> None", pct is None and exact is False)

    # A8. A name that is a substring of another must not steal its score.
    #     FUN_0019b64 is a prefix of FUN_0019b640; the colon anchor prevents it.
    pct, exact = f(SAMPLE, ["FUN_0019b64"])
    check("A8 prefix name does not match longer name",
          exact is False and pct == 67.8)

    print()
    if FAILURES:
        print(f"SELF-TEST FAILED ({len(FAILURES)}): {', '.join(FAILURES)}")
        return 1
    print("SELF-TEST PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
