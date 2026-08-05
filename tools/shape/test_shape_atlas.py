#!/usr/bin/env python3
"""Unit tests for tools/shape/shape_atlas.py -- pure logic only, no XBE needed.

Uses synthetic AT&T-style instruction-text lists (the same shape objdump
would emit) rather than real disassembly, so these run without
halo-patched/cachebeta.xbe or llvm-objdump. Style mirrors
tools/verify/test_ref_selection.py: plain test_*() functions with `assert`,
runnable both bare (`python3 tools/shape/test_shape_atlas.py`) and, if pytest
is ever installed, via pytest auto-discovery.
"""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from shape_atlas import (  # noqa: E402
    BULK_THRESHOLD,
    classify_groups,
    compute_shape_key,
    donor_tier_and_score,
    group_functions,
    select_donor,
)


# ---------------------------------------------------------------------------
# Shape-key collision / non-collision
# ---------------------------------------------------------------------------

def test_shape_key_collides_for_same_template_different_addresses():
    # Two "accessor" functions differing only in the absolute displacement
    # (field offset / literal) and the callee address -- the same template
    # instantiated twice, at two different code addresses.
    fn_a = [
        "push %ebp",
        "mov %esp, %ebp",
        "mov 0x8(%ebp), %eax",
        "mov 0xc(%eax), %eax",
        "call 0x401000",
        "pop %ebp",
        "ret",
    ]
    fn_b = [
        "push %ebp",
        "mov %esp, %ebp",
        "mov 0x8(%ebp), %eax",
        "mov 0x10(%eax), %eax",
        "call 0x402500",
        "pop %ebp",
        "ret",
    ]
    assert compute_shape_key(fn_a) == compute_shape_key(fn_b)


def test_shape_key_collides_for_same_template_different_registers():
    # Same skeleton, but the compiler picked a different scratch register
    # for the intermediate value -- still the same source template.
    fn_a = [
        "push %ebp",
        "mov %esp, %ebp",
        "mov 0x8(%ebp), %eax",
        "add $0x4, %eax",
        "mov %eax, 0xc(%ebp)",
        "pop %ebp",
        "ret",
    ]
    fn_b = [
        "push %ebp",
        "mov %esp, %ebp",
        "mov 0x8(%ebp), %ecx",
        "add $0x8, %ecx",
        "mov %ecx, 0xc(%ebp)",
        "pop %ebp",
        "ret",
    ]
    assert compute_shape_key(fn_a) == compute_shape_key(fn_b)


def test_shape_key_differs_for_different_mnemonic_sequence():
    fn_a = ["push %ebp", "mov %esp, %ebp", "pop %ebp", "ret"]
    fn_b = ["push %ebp", "mov %esp, %ebp", "xor %eax, %eax", "pop %ebp", "ret"]
    assert compute_shape_key(fn_a) != compute_shape_key(fn_b)


def test_shape_key_differs_for_different_register_reuse_pattern():
    # Same mnemonics and same instruction count in both, but fn_a re-reads
    # the same register it just wrote (a real dependency) while fn_b
    # introduces a third, distinct register instead -- canonicalization by
    # first-appearance order preserves that structural difference even
    # though literal displacements/immediates are irrelevant here.
    fn_a = [
        "mov %eax, %ebx",
        "add %ebx, %ebx",
    ]
    fn_b = [
        "mov %eax, %ebx",
        "add %ecx, %ebx",
    ]
    assert compute_shape_key(fn_a) != compute_shape_key(fn_b)


def test_shape_key_stable_regardless_of_address():
    # compute_shape_key never receives an address, but confirm two calls on
    # the *same* instruction text always agree (determinism).
    fn = ["push %ebp", "mov %esp, %ebp", "pop %ebp", "ret"]
    assert compute_shape_key(fn) == compute_shape_key(list(fn))


# ---------------------------------------------------------------------------
# Grouping
# ---------------------------------------------------------------------------

def test_group_functions_buckets_by_shape_key():
    shared = ["push %ebp", "mov %esp, %ebp", "pop %ebp", "ret"]
    unique = ["push %ebp", "mov %esp, %ebp", "xor %eax, %eax", "pop %ebp", "ret"]
    insns_by_addr = {
        0x1000: list(shared),
        0x2000: list(shared),
        0x3000: list(unique),
    }
    groups = group_functions(insns_by_addr)
    sizes = sorted(len(v) for v in groups.values())
    assert sizes == [1, 2]
    # the 2-member group must be exactly {0x1000, 0x2000}
    pair = next(v for v in groups.values() if len(v) == 2)
    assert sorted(pair) == [0x1000, 0x2000]


# ---------------------------------------------------------------------------
# Donor tier / selection
# ---------------------------------------------------------------------------

def test_donor_tier_exact_when_score_at_or_above_threshold():
    scores = {"real_foo": {"score": 99.5}}
    tier, score = donor_tier_and_score("real_foo", scores)
    assert tier == "exact"
    assert score == 99.5


def test_donor_tier_ported_when_score_below_threshold():
    scores = {"real_foo": {"score": 62.0}}
    tier, score = donor_tier_and_score("real_foo", scores)
    assert tier == "ported"
    assert score == 62.0


def test_donor_tier_ported_when_score_unknown():
    tier, score = donor_tier_and_score("real_unknown", {})
    assert tier == "ported"
    assert score is None


def _rec(name, ported, obj="foo.obj", src="src/halo/foo.c", nbytes=10):
    return {"name": name, "ported": ported, "object": obj, "source_path": src,
            "bytes": nbytes}


def test_select_donor_picks_exact_over_ported():
    resolved = {
        0x1000: _rec("real_a", True),
        0x2000: _rec("real_b", True),
        0x3000: _rec("FUN_003000", False),
    }
    scores = {"real_a": {"score": 60.0}, "real_b": {"score": 99.9}}
    donor = select_donor([0x1000, 0x2000, 0x3000], resolved, scores)
    assert donor is not None
    assert donor["name"] == "real_b"
    assert donor["tier"] == "exact"


def test_select_donor_none_when_no_member_ported():
    resolved = {
        0x1000: _rec("FUN_001000", False),
        0x2000: _rec("FUN_002000", False),
        0x3000: _rec("FUN_003000", False),
    }
    donor = select_donor([0x1000, 0x2000, 0x3000], resolved, {})
    assert donor is None


# ---------------------------------------------------------------------------
# Group classification: transfer groups vs unsolved-bulk groups
# ---------------------------------------------------------------------------

def test_classify_groups_transfer_group_with_ported_donor():
    resolved = {
        0x1000: _rec("real_donor", True, nbytes=20),
        0x2000: _rec("FUN_002000", False, nbytes=15),
        0x3000: _rec("FUN_003000", False, nbytes=25),
    }
    groups = {"k1": [0x1000, 0x2000, 0x3000]}
    scores = {"real_donor": {"score": 99.9}}
    records, stats = classify_groups(groups, resolved, scores)
    assert len(records) == 1
    g = records[0]
    assert g["is_transfer"] is True
    assert g["is_unsolved_bulk"] is False
    assert sorted(g["recipients"]) == ["0x2000", "0x3000"]
    assert stats["transfer_groups"] == 1
    assert stats["unsolved_bulk_groups"] == 0
    assert stats["transfer_recipient_bytes"] == 15 + 25
    assert stats["donor_exact_groups"] == 1
    assert stats["donor_ported_groups"] == 0


def test_classify_groups_unsolved_bulk_requires_threshold():
    # Exactly BULK_THRESHOLD unported members, no donor -> unsolved bulk.
    assert BULK_THRESHOLD == 3
    resolved = {
        0x1000: _rec("FUN_001000", False),
        0x2000: _rec("FUN_002000", False),
        0x3000: _rec("FUN_003000", False),
    }
    groups = {"k1": [0x1000, 0x2000, 0x3000]}
    records, stats = classify_groups(groups, resolved, {})
    assert records[0]["is_unsolved_bulk"] is True
    assert records[0]["is_transfer"] is False
    assert stats["unsolved_bulk_groups"] == 1
    assert stats["transfer_groups"] == 0


def test_classify_groups_below_bulk_threshold_is_neither():
    # Only 2 unported members, no donor -> not enough for "bulk", and no
    # donor means not a transfer group either.
    resolved = {
        0x1000: _rec("FUN_001000", False),
        0x2000: _rec("FUN_002000", False),
    }
    groups = {"k1": [0x1000, 0x2000]}
    records, stats = classify_groups(groups, resolved, {})
    assert records[0]["is_transfer"] is False
    assert records[0]["is_unsolved_bulk"] is False
    assert stats["unsolved_bulk_groups"] == 0
    assert stats["transfer_groups"] == 0


def test_classify_groups_singleton_excluded_from_output():
    resolved = {0x1000: _rec("real_only", True)}
    groups = {"k1": [0x1000]}
    records, stats = classify_groups(groups, resolved, {})
    assert records == []
    assert stats["groups_total"] == 1
    assert stats["groups_ge2"] == 0


def test_classify_groups_donor_excluded_from_recipients():
    resolved = {
        0x1000: _rec("real_donor", True),
        0x2000: _rec("FUN_002000", False),
    }
    groups = {"k1": [0x1000, 0x2000]}
    scores = {"real_donor": {"score": 100.0}}
    records, _stats = classify_groups(groups, resolved, scores)
    g = records[0]
    assert g["donor"]["addr"] == "0x1000"
    assert g["recipients"] == ["0x2000"]


# ---------------------------------------------------------------------------
# Runner (pytest-compatible, but no pytest dependency required)
# ---------------------------------------------------------------------------

def main() -> int:
    tests = [obj for name, obj in sorted(globals().items())
             if name.startswith("test_") and callable(obj)]
    failed = 0
    for t in tests:
        try:
            t()
        except AssertionError as exc:
            print(f"  FAIL  {t.__name__}: {exc}")
            failed += 1
        except Exception as exc:  # noqa: BLE001
            print(f"  ERROR {t.__name__}: {exc!r}")
            failed += 1
    if failed:
        print(f"\n{failed}/{len(tests)} tests FAILED.")
        return 1
    print(f"\nAll {len(tests)} tests passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
