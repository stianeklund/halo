#!/usr/bin/env python3
"""Self-test for the stub-argument differential comparator.

Run with:
    python3 tools/equivalence/test_stub_arg_trace.py

Tests:
  1. Identical oracle and candidate traces   → no differences
  2. Two args swapped between oracle/cand    → 2 arg mismatches
  3. Both-stack-pointer args differ          → soft match (no hard mismatch)
  4. Call-sequence length divergence         → sequence_diverged
  5. Call-sequence callee-identity diverge   → sequence_diverged at correct index
"""

import sys
import os
sys.path.insert(0, os.path.dirname(__file__))

from stubs import (
    StubArgTracer, StubCallRecord, compare_stub_arg_traces,
    _STACK_BASE, _STACK_TOP,
)

# A stack address that should be treated as a soft match
_SP1 = _STACK_BASE + 0x100   # oracle stack pointer
_SP2 = _STACK_BASE + 0x200   # candidate stack pointer (different frame layout)

# Sentinel addresses
_SENTINEL_A = 0x40000000
_SENTINEL_B = 0x40004000


def _make_tracer(*records):
    t = StubArgTracer()
    t.records = list(records)
    return t


def _rec(seq, addr, name, *args):
    return StubCallRecord(seq=seq, callee_addr=addr, callee_name=name,
                          args=list(args), is_varargs=False)


def test_identical():
    oracle = _make_tracer(
        _rec(0, _SENTINEL_A, "foo", 0x01, 0x02, 0x03),
        _rec(1, _SENTINEL_B, "bar", 0xAA, 0xBB),
    )
    cand = _make_tracer(
        _rec(0, _SENTINEL_A, "foo", 0x01, 0x02, 0x03),
        _rec(1, _SENTINEL_B, "bar", 0xAA, 0xBB),
    )
    d = compare_stub_arg_traces(oracle, cand, seed_label="s0")
    assert not d.has_differences(), f"expected no diff, got: {d}"
    assert d.total_calls == 2
    assert d.arg_mismatches == 0
    assert d.soft_stack_ptr_matches == 0
    assert not d.sequence_diverged
    print("  PASS  test_identical")


def test_swapped_args():
    oracle = _make_tracer(
        _rec(0, _SENTINEL_A, "foo", 0x10, 0x20),
    )
    # candidate has args swapped
    cand = _make_tracer(
        _rec(0, _SENTINEL_A, "foo", 0x20, 0x10),
    )
    d = compare_stub_arg_traces(oracle, cand, seed_label="s1")
    assert d.has_differences(), "expected differences"
    assert d.arg_mismatches == 2, f"expected 2 mismatches, got {d.arg_mismatches}"
    assert d.soft_stack_ptr_matches == 0
    assert not d.sequence_diverged
    # Check detail entries
    assert len(d.details) == 2
    assert d.details[0][3] == 0  # arg_pos 0
    assert d.details[1][3] == 1  # arg_pos 1
    print("  PASS  test_swapped_args")


def test_stack_ptr_soft_match():
    # Both sides pass a stack pointer but the frame layout differs (different
    # stack offsets) — this is expected and should NOT be a hard mismatch.
    oracle = _make_tracer(
        _rec(0, _SENTINEL_A, "foo", _SP1, 0xDEAD),
    )
    cand = _make_tracer(
        _rec(0, _SENTINEL_A, "foo", _SP2, 0xDEAD),  # same 0xDEAD, different SP
    )
    d = compare_stub_arg_traces(oracle, cand, seed_label="s2")
    # 0xDEAD matches exactly, stack ptr pair is a soft match
    assert not d.sequence_diverged
    assert d.arg_mismatches == 0, f"expected 0 hard mismatches, got {d.arg_mismatches}"
    assert d.soft_stack_ptr_matches == 1, f"expected 1 soft match, got {d.soft_stack_ptr_matches}"
    assert not d.has_differences(), "soft match should not flag as has_differences"
    print("  PASS  test_stack_ptr_soft_match")


def test_stack_ptr_one_side_only():
    # One side passes a stack pointer, the other a non-stack value → hard mismatch
    oracle = _make_tracer(
        _rec(0, _SENTINEL_A, "foo", _SP1),
    )
    cand = _make_tracer(
        _rec(0, _SENTINEL_A, "foo", 0x12345678),  # not a stack pointer
    )
    d = compare_stub_arg_traces(oracle, cand, seed_label="s3")
    assert d.arg_mismatches == 1
    assert d.soft_stack_ptr_matches == 0
    assert d.has_differences()
    print("  PASS  test_stack_ptr_one_side_only")


def test_sequence_length_diverged():
    oracle = _make_tracer(
        _rec(0, _SENTINEL_A, "foo", 0x1),
        _rec(1, _SENTINEL_B, "bar", 0x2),
    )
    cand = _make_tracer(
        _rec(0, _SENTINEL_A, "foo", 0x1),
        # bar not called in candidate
    )
    d = compare_stub_arg_traces(oracle, cand, seed_label="s4")
    assert d.sequence_diverged, "expected sequence divergence"
    assert d.sequence_diverge_index == 1
    assert d.has_differences()
    print("  PASS  test_sequence_length_diverged")


def test_sequence_callee_diverged():
    oracle = _make_tracer(
        _rec(0, _SENTINEL_A, "foo", 0x1),
        _rec(1, _SENTINEL_B, "bar", 0x2),
    )
    # candidate calls a different function at index 1
    cand = _make_tracer(
        _rec(0, _SENTINEL_A, "foo", 0x1),
        _rec(1, _SENTINEL_A, "foo", 0x2),  # wrong callee (A instead of B)
    )
    d = compare_stub_arg_traces(oracle, cand, seed_label="s5")
    assert d.sequence_diverged, "expected sequence divergence"
    assert d.sequence_diverge_index == 1
    assert d.has_differences()
    print("  PASS  test_sequence_callee_diverged")


def test_empty_traces():
    oracle = StubArgTracer()
    cand = StubArgTracer()
    d = compare_stub_arg_traces(oracle, cand, seed_label="s6")
    assert not d.has_differences()
    assert d.total_calls == 0
    print("  PASS  test_empty_traces")


def main():
    print("Running stub-arg trace comparator self-tests...")
    test_identical()
    test_swapped_args()
    test_stack_ptr_soft_match()
    test_stack_ptr_one_side_only()
    test_sequence_length_diverged()
    test_sequence_callee_diverged()
    test_empty_traces()
    print("\nAll tests passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
