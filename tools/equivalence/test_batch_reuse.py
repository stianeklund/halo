#!/usr/bin/env python3
"""Self-tests for fingerprinted and age-aware batch result reuse."""

import json
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from batch_verify import (CACHE_SCHEMA, CACHE_SCHEMA_FIELD, FINGERPRINT_FIELD,
                          VERIFIED_AT_FIELD, candidate_fingerprint,
                          reusable_results)


NOW = 1_800_000_000.0


def _fixture(tmp, result_ages, statuses=None, fingerprints=None):
    """Build an output directory with controlled result metadata."""
    od = Path(tmp) / "out"
    od.mkdir()
    statuses = statuses or {}
    fingerprints = fingerprints or {}
    for stem, age in result_ages.items():
        result = {
            "status": statuses.get(stem, "pass"),
            CACHE_SCHEMA_FIELD: CACHE_SCHEMA,
            FINGERPRINT_FIELD: fingerprints.get(stem, "fp"),
            VERIFIED_AT_FIELD: NOW - age,
        }
        (od / f"{stem}.json").write_text(json.dumps(result))
    return od


def _reuse(output_dir, expected=None, max_age=500.0):
    return reusable_results(output_dir, expected or {},
                            max_age_seconds=max_age, now=NOW)


def test_result_older_than_ttl_is_invalidated():
    with tempfile.TemporaryDirectory() as tmp:
        od = _fixture(tmp, {"old": 1000}, fingerprints={"old": "fp"})
        reusable, invalidated = _reuse(od, {"old": "fp"})
        assert reusable == set(), reusable
        assert invalidated == 1, invalidated
    print("  PASS  test_result_older_than_ttl_is_invalidated")


def test_result_newer_than_ttl_is_reused():
    with tempfile.TemporaryDirectory() as tmp:
        od = _fixture(tmp, {"fresh": 10}, fingerprints={"fresh": "fp"})
        reusable, invalidated = _reuse(od, {"fresh": "fp"})
        assert reusable == {"fresh"}, reusable
        assert invalidated == 0, invalidated
    print("  PASS  test_result_newer_than_ttl_is_reused")


def test_mismatched_fingerprint_is_invalidated():
    with tempfile.TemporaryDirectory() as tmp:
        od = _fixture(tmp, {"changed": 10}, fingerprints={"changed": "old"})
        reusable, invalidated = _reuse(od, {"changed": "new"})
        assert reusable == set(), reusable
        assert invalidated == 1, invalidated
    print("  PASS  test_mismatched_fingerprint_is_invalidated")


def test_failed_results_are_never_reused():
    with tempfile.TemporaryDirectory() as tmp:
        od = _fixture(tmp, {"fail": 10, "error": 10},
                      statuses={"fail": "fail", "error": "error"},
                      fingerprints={"fail": "fp", "error": "fp"})
        reusable, invalidated = _reuse(od, {"fail": "fp", "error": "fp"})
        assert reusable == set(), reusable
        assert invalidated == 2, invalidated
    print("  PASS  test_failed_results_are_never_reused")


def test_pre_metadata_result_is_invalidated():
    with tempfile.TemporaryDirectory() as tmp:
        od = Path(tmp) / "out"
        od.mkdir()
        (od / "legacy.json").write_text(json.dumps({"status": "pass"}))
        reusable, invalidated = _reuse(od, {"legacy": "fp"})
        assert reusable == set(), reusable
        assert invalidated == 1, invalidated
    print("  PASS  test_pre_metadata_result_is_invalidated")


def test_summary_json_is_never_counted():
    with tempfile.TemporaryDirectory() as tmp:
        od = _fixture(tmp, {"summary": 10, "target": 10},
                      fingerprints={"summary": "fp", "target": "fp"})
        reusable, invalidated = _reuse(od, {"summary": "fp", "target": "fp"})
        assert reusable == {"target"}, reusable
        assert invalidated == 0, invalidated
    print("  PASS  test_summary_json_is_never_counted")


def test_missing_output_dir_is_empty_not_an_error():
    with tempfile.TemporaryDirectory() as tmp:
        reusable, invalidated = _reuse(Path(tmp) / "nope")
        assert reusable == set() and invalidated == 0
    print("  PASS  test_missing_output_dir_is_empty_not_an_error")


def test_candidate_fingerprint_includes_target_identity():
    base = "base"
    candidate = {"addr": "0x10", "name": "fn", "class": "leaf",
                 "obj": "math.obj", "decl": "int fn(void)"}
    changed = dict(candidate, decl="int fn(int value)")
    assert candidate_fingerprint(base, candidate) != candidate_fingerprint(base, changed)
    print("  PASS  test_candidate_fingerprint_includes_target_identity")


def main():
    print("Running batch_verify reuse self-tests...")
    tests = [v for k, v in sorted(globals().items())
             if k.startswith("test_") and callable(v)]
    failed = 0
    for test in tests:
        try:
            test()
        except AssertionError as exc:
            print(f"  FAIL  {test.__name__}: {exc}")
            failed += 1
    if failed:
        print(f"\n{failed}/{len(tests)} FAILED")
        return 1
    print(f"\nAll {len(tests)} tests passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
