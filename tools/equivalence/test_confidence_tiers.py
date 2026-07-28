#!/usr/bin/env python3
"""Self-test for the confidence-tier rules (Phase 0.4).

Run with:
    python3 tools/equivalence/test_confidence_tiers.py

Two defects are pinned here:

  1. A run could record a confidence tier at 0.0% coverage. 129 cached entries
     were in that state -- a verdict with no evidence behind it, which reads
     downstream (leaf_cache, selector boosts, dashboard) exactly like a verdict
     with evidence. Below COVERAGE_FLOOR_PCT the tier is now "none".

  2. Path diversity was judged on the return value alone, so any function that
     returns a constant -- notably one returning its own out-param pointer --
     was forced to "weak" no matter how thoroughly it was exercised.
     vector3d_scale_add covers 100% of its body and writes different floats
     every seed, and was recorded "weak". Diversity now counts scratch
     payloads and memory writes too.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(__file__))

from unicorn_diff import _classify_confidence, COVERAGE_FLOOR_PCT, _CONFIDENCE_RANK


def test_zero_coverage_has_no_tier():
    assert _classify_confidence(0.0, False, passed=10) == "none"
    assert _classify_confidence(0.0, True, passed=10) == "none"
    assert _classify_confidence(COVERAGE_FLOOR_PCT - 0.1, True, 10) == "none"
    print("  [1] sub-floor coverage records no tier                   OK")


def test_no_passing_seeds_has_no_tier():
    assert _classify_confidence(100.0, True, passed=0) == "none"
    print("  [2] zero passing seeds records no tier                   OK")


def test_high_requires_output_variation():
    assert _classify_confidence(100.0, True, 10) == "high"
    assert _classify_confidence(60.0, True, 10) == "high"
    # Full coverage but every observable output identical -> not "high".
    assert _classify_confidence(100.0, False, 10) == "moderate"
    print("  [3] high requires coverage>=60 AND varied output         OK")


def test_scratch_variation_earns_high():
    """The vector3d_scale_add shape: constant EAX, varying scratch buffer."""
    # output_varied is True because scratch digests differed, even though the
    # return value never did. Under the old monotonic_return rule this was
    # unconditionally "weak" despite 100% coverage.
    assert _classify_confidence(100.0, True, 20) == "high"
    print("  [4] constant return + varying scratch still reaches high OK")


def test_mid_and_low_bands():
    assert _classify_confidence(45.0, True, 10) == "moderate"
    assert _classify_confidence(30.0, False, 10) == "moderate"
    assert _classify_confidence(29.9, True, 10) == "weak"
    assert _classify_confidence(COVERAGE_FLOOR_PCT, False, 10) == "weak"
    print("  [5] moderate/weak bands unchanged                        OK")


def test_none_ranks_below_weak():
    """The best-of policy in _record_confidence must never let "none" win."""
    assert _CONFIDENCE_RANK["none"] < _CONFIDENCE_RANK["weak"]
    print("  [6] 'none' ranks below 'weak' for best-of writes         OK")


def main():
    print("test_confidence_tiers:")
    test_zero_coverage_has_no_tier()
    test_no_passing_seeds_has_no_tier()
    test_high_requires_output_variation()
    test_scratch_variation_earns_high()
    test_mid_and_low_bands()
    test_none_ranks_below_weak()
    print("PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
