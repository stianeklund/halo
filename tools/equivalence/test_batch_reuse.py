#!/usr/bin/env python3
"""Self-tests for age-aware --skip-existing reuse (batch_verify).

A cached per-target result is evidence about the harness that produced it, not
about the current one. Before this, --skip-existing reused any result that
existed, so a persistent output dir accumulated results indefinitely: on
2026-07-29 `summary.json` reported 435 `error` rows of which 270 came from
per-target JSONs dated 07-08/07-09 -- older than both the data-page guard
(b37696b4) and real-callees-by-default. The aggregate looked like a current
measurement while being mostly memory, and the fossil cluster (124 rows of an
already-fixed bug) buried the only cluster that was still live.

Keying invalidation on harness source mtime means no date to maintain: editing
the harness invalidates everything measured before the edit.
"""

import os
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from batch_verify import harness_mtime, reusable_results, HARNESS_SOURCES


def _fixture(tmp, result_ages, harness_age):
    """Build a fake harness dir + output dir with controlled mtimes.

    Ages are seconds-ago, so a larger age is older.
    """
    now = 1_800_000_000.0
    hd = Path(tmp) / "harness"
    od = Path(tmp) / "out"
    hd.mkdir()
    od.mkdir()
    for name in HARNESS_SOURCES:
        f = hd / name
        f.write_text("x")
        os.utime(f, (now - harness_age, now - harness_age))
    for stem, age in result_ages.items():
        f = od / f"{stem}.json"
        f.write_text("{}")
        os.utime(f, (now - age, now - age))
    return hd, od


def test_result_older_than_harness_is_invalidated():
    with tempfile.TemporaryDirectory() as tmp:
        hd, od = _fixture(tmp, {"fossil": 1000}, harness_age=100)
        reusable, n_inv = reusable_results(od, hd)
        assert reusable == set(), f"stale result must not be reused: {reusable}"
        assert n_inv == 1, n_inv
    print("  PASS  test_result_older_than_harness_is_invalidated")


def test_result_newer_than_harness_is_reused():
    with tempfile.TemporaryDirectory() as tmp:
        hd, od = _fixture(tmp, {"fresh": 10}, harness_age=100)
        reusable, n_inv = reusable_results(od, hd)
        assert reusable == {"fresh"}, reusable
        assert n_inv == 0, n_inv
    print("  PASS  test_result_newer_than_harness_is_reused")


def test_mixed_dir_splits_correctly():
    """The real shape: mostly fossils, a few fresh -- the 270/141 case."""
    with tempfile.TemporaryDirectory() as tmp:
        ages = {f"old{i}": 1000 for i in range(5)}
        ages.update({f"new{i}": 10 for i in range(2)})
        hd, od = _fixture(tmp, ages, harness_age=100)
        reusable, n_inv = reusable_results(od, hd)
        assert reusable == {"new0", "new1"}, reusable
        assert n_inv == 5, n_inv
    print("  PASS  test_mixed_dir_splits_correctly")


def test_summary_json_is_never_counted():
    """summary.json is an aggregate, not a per-target result."""
    with tempfile.TemporaryDirectory() as tmp:
        hd, od = _fixture(tmp, {"summary": 1000, "t": 10}, harness_age=100)
        reusable, n_inv = reusable_results(od, hd)
        assert reusable == {"t"}, reusable
        assert n_inv == 0, f"summary.json must not count as invalidated: {n_inv}"
    print("  PASS  test_summary_json_is_never_counted")


def test_missing_output_dir_is_empty_not_an_error():
    with tempfile.TemporaryDirectory() as tmp:
        hd, _ = _fixture(tmp, {}, harness_age=100)
        reusable, n_inv = reusable_results(Path(tmp) / "nope", hd)
        assert reusable == set() and n_inv == 0
    print("  PASS  test_missing_output_dir_is_empty_not_an_error")


def test_real_harness_dir_resolves():
    """The default path must find actual sources, not silently return 0.

    A zero cutoff would reuse everything forever -- the exact bug, restored.
    """
    mt = harness_mtime()
    assert mt > 0, ("harness_mtime() found no sources; HARNESS_SOURCES is stale "
                    "and every cached result would be reused unconditionally")
    print("  PASS  test_real_harness_dir_resolves")


def test_every_named_source_exists():
    """A renamed harness file must break this, not quietly stop invalidating."""
    here = Path(__file__).resolve().parent
    missing = [n for n in HARNESS_SOURCES if not (here / n).exists()]
    assert not missing, f"HARNESS_SOURCES names non-existent file(s): {missing}"
    print("  PASS  test_every_named_source_exists")


def main():
    print("Running batch_verify reuse self-tests...")
    tests = [v for k, v in sorted(globals().items())
             if k.startswith("test_") and callable(v)]
    failed = 0
    for t in tests:
        try:
            t()
        except AssertionError as exc:
            print(f"  FAIL  {t.__name__}: {exc}")
            failed += 1
    if failed:
        print(f"\n{failed}/{len(tests)} FAILED")
        return 1
    print(f"\nAll {len(tests)} tests passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
