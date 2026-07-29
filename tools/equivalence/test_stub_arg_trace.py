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
    patch_dir32_relocs, IMAGE_REL_I386_DIR32,
    GLOBALS_BASE, _STACK_BASE, _STACK_TOP,
)

# A stack address that should be treated as a soft match
_SP1 = _STACK_BASE + 0x100   # oracle stack pointer
_SP2 = _STACK_BASE + 0x200   # candidate stack pointer (different frame layout)

# Sentinel addresses
_SENTINEL_A = 0x40000000
_SENTINEL_B = 0x40004000
_SENTINEL_C = 0x40008000


class _FakeReloc:
    def __init__(self, reloc_type, symbol_name, virtual_address):
        self.reloc_type = reloc_type
        self.symbol_name = symbol_name
        self.virtual_address = virtual_address


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


def test_sequences_are_recorded():
    """The comparator must keep the two sequences, not just the divergence index.

    Without them a call_seq verdict cannot be adjudicated at all -- which was the
    state on the 07-29 batch, where call_seq was the largest failure class (71 of
    186) and every one of them read only "diverged at index N".
    """
    oracle = _make_tracer(
        _rec(0, _SENTINEL_A, "foo", 0x1),
        _rec(1, _SENTINEL_B, "bar", 0x2),
    )
    cand = _make_tracer(_rec(0, _SENTINEL_A, "foo", 0x1))
    d = compare_stub_arg_traces(oracle, cand, seed_label="s-rec")
    assert d.oracle_seq == ["foo", "bar"], d.oracle_seq
    assert d.candidate_seq == ["foo"], d.candidate_seq
    assert any("call-seq oracle" in ln for ln in d.sequence_detail())
    print("  PASS  test_sequences_are_recorded")


def test_shifted_sequence_is_flagged_as_alignment():
    """One extra call on one side, rest lining up = alignment artifact, not a bug.

    This is the case worth separating: the comparison walks two lists off by one,
    so every position after the insertion 'diverges' while both sides really call
    the same things in the same order.
    """
    oracle = _make_tracer(
        _rec(0, _SENTINEL_A, "foo", 0x1),
        _rec(1, _SENTINEL_B, "extra", 0x9),
        _rec(2, _SENTINEL_C, "bar", 0x2),
    )
    cand = _make_tracer(
        _rec(0, _SENTINEL_A, "foo", 0x1),
        _rec(2, _SENTINEL_C, "bar", 0x2),
    )
    d = compare_stub_arg_traces(oracle, cand, seed_label="s-shift")
    assert d.sequence_diverged
    kind, _ = d.sequence_relation()
    assert kind == "shifted", kind
    assert any("SHIFTED" in ln for ln in d.sequence_detail())
    print("  PASS  test_shifted_sequence_is_flagged_as_alignment")


def test_truncated_sequence_is_flagged_as_early_exit():
    """Shorter is a prefix of longer = one side stopped, not a divergence.

    Four of six call_seq targets sampled on 07-29 were this shape (oracle made
    2 calls, candidate 4-5, oracle's list a prefix). Calling that a wrong-callee
    bug would put four artifacts on the bug list.
    """
    oracle = _make_tracer(
        _rec(0, _SENTINEL_A, "foo", 0x1),
        _rec(1, _SENTINEL_B, "bar", 0x2),
    )
    cand = _make_tracer(
        _rec(0, _SENTINEL_A, "foo", 0x1),
        _rec(1, _SENTINEL_B, "bar", 0x2),
        _rec(2, _SENTINEL_C, "baz", 0x3),
        _rec(3, _SENTINEL_C, "baz", 0x4),
    )
    d = compare_stub_arg_traces(oracle, cand, seed_label="s-trunc")
    assert d.sequence_diverged
    kind, _ = d.sequence_relation()
    assert kind == "truncated", kind
    assert any("TRUNCATED" in ln for ln in d.sequence_detail())
    print("  PASS  test_truncated_sequence_is_flagged_as_early_exit")


def test_genuinely_different_callee_is_not_called_a_shift():
    """The opposite direction: a real wrong-callee must NOT be excused.

    Equal lengths with a different callee at one position can never be explained
    by an off-by-one, so sequence_shift() must return None or the alignment
    story would launder real control-flow bugs.
    """
    oracle = _make_tracer(
        _rec(0, _SENTINEL_A, "foo", 0x1),
        _rec(1, _SENTINEL_B, "bar", 0x2),
    )
    cand = _make_tracer(
        _rec(0, _SENTINEL_A, "foo", 0x1),
        _rec(1, _SENTINEL_C, "baz", 0x2),
    )
    d = compare_stub_arg_traces(oracle, cand, seed_label="s-real")
    assert d.sequence_diverged
    kind, desc = d.sequence_relation()
    assert kind == "divergent", kind
    assert "index 1" in desc, desc
    assert not any("SHIFTED" in ln or "TRUNCATED" in ln
                   for ln in d.sequence_detail())
    print("  PASS  test_genuinely_different_callee_is_not_called_a_shift")


def test_chkstk_ignored():
    oracle = _make_tracer(
        _rec(0, _SENTINEL_A, "FUN_001d90e0", 0x00, 0x00),
        _rec(1, _SENTINEL_B, "datum_get", 0x10, 0x20),
    )
    cand = _make_tracer(
        _rec(0, _SENTINEL_B, "datum_get", 0x10, 0x20),
    )
    d = compare_stub_arg_traces(oracle, cand, seed_label="s6")
    assert not d.has_differences(), f"expected _chkstk to be ignored, got: {d}"
    assert d.total_calls == 1
    print("  PASS  test_chkstk_ignored")


def test_assert_metadata_soft_matched():
    """display_assert's message/__FILE__/__LINE__ differ by construction.

    Our lifted sources do not reproduce the original's line numbering and our
    string literals land in a different section. 101 of the 331 divergences in
    the 2026-07-28 batch were nothing but this.
    """
    oracle = _make_tracer(
        _rec(0, _SENTINEL_A, "_display_assert", 0x1f4a20, 0x1f4b00, 0xa89, 0x1),
    )
    cand = _make_tracer(
        _rec(0, _SENTINEL_A, "_display_assert", 0x4c1180, 0x4c1200, 0x374, 0x1),
    )
    d = compare_stub_arg_traces(oracle, cand, seed_label="s8")
    assert d.arg_mismatches == 0, f"expected 0 hard mismatches, got {d.arg_mismatches}"
    assert d.soft_semantic_matches == 3, d.soft_semantic_matches
    assert d.soft_reasons.get("assert-metadata") == 3, d.soft_reasons
    assert not d.has_differences(), f"assert metadata should not fail: {d}"
    assert "assert-metadata" in d.summary(), d.summary()
    print("  PASS  test_assert_metadata_soft_matched")


def test_assert_halt_arg_still_compared():
    """Arg 3 (`halt`) is behavioural — whether the assert aborts — not metadata."""
    oracle = _make_tracer(
        _rec(0, _SENTINEL_A, "_display_assert", 0x1f4a20, 0x1f4b00, 0xa89, 0x1),
    )
    cand = _make_tracer(
        _rec(0, _SENTINEL_A, "_display_assert", 0x1f4a20, 0x1f4b00, 0xa89, 0x0),
    )
    d = compare_stub_arg_traces(oracle, cand, seed_label="s9")
    assert d.arg_mismatches == 1, f"halt arg must be compared, got {d.arg_mismatches}"
    assert d.has_differences()
    print("  PASS  test_assert_halt_arg_still_compared")


def test_assert_exemption_does_not_cover_other_callees():
    """The metadata rule is a display_assert contract, not a general one."""
    oracle = _make_tracer(_rec(0, _SENTINEL_A, "datum_get", 0x1f4a20, 0x2, 0xa89))
    cand = _make_tracer(_rec(0, _SENTINEL_A, "datum_get", 0x4c1180, 0x2, 0x374))
    d = compare_stub_arg_traces(oracle, cand, seed_label="s10")
    assert d.arg_mismatches == 2, d.arg_mismatches
    assert d.has_differences()
    print("  PASS  test_assert_exemption_does_not_cover_other_callees")


def test_memset_fill_low_byte_soft_matched():
    """memset(p, -1, n) and memset(p, 0xff, n) write identical bytes."""
    oracle = _make_tracer(_rec(0, _SENTINEL_A, "_csmemset", 0x40000, 0xffffffff, 0x20))
    cand = _make_tracer(_rec(0, _SENTINEL_A, "_csmemset", 0x40000, 0xff, 0x20))
    d = compare_stub_arg_traces(oracle, cand, seed_label="s11")
    assert d.arg_mismatches == 0, d.arg_mismatches
    assert d.soft_reasons.get("memset-fill") == 1, d.soft_reasons
    assert not d.has_differences()
    print("  PASS  test_memset_fill_low_byte_soft_matched")


def test_memset_differing_low_byte_still_fails():
    """Only the low byte is unobservable; a different one is a real bug."""
    oracle = _make_tracer(_rec(0, _SENTINEL_A, "_csmemset", 0x40000, 0xffffffff, 0x20))
    cand = _make_tracer(_rec(0, _SENTINEL_A, "_csmemset", 0x40000, 0x00, 0x20))
    d = compare_stub_arg_traces(oracle, cand, seed_label="s12")
    assert d.arg_mismatches == 1, d.arg_mismatches
    assert d.has_differences()
    print("  PASS  test_memset_differing_low_byte_still_fails")


def test_memset_size_arg_still_compared():
    """A benign fill must not launder a wrong destination or size."""
    oracle = _make_tracer(_rec(0, _SENTINEL_A, "_csmemset", 0x40000, 0xffffffff, 0x40))
    cand = _make_tracer(_rec(0, _SENTINEL_A, "_csmemset", 0x40000, 0xff, 0x10))
    d = compare_stub_arg_traces(oracle, cand, seed_label="s13")
    assert d.arg_mismatches == 1, d.arg_mismatches
    assert d.details[0][3] == 2, f"expected the size arg to fail, got {d.details[0]}"
    print("  PASS  test_memset_size_arg_still_compared")


def test_candidate_globals_slot_above_nominal_top():
    """Candidate DIR32 slots sit above the oracle's and can pass 0x600000.

    Using the nominal arena top made every such slot pointer a hard mismatch.
    """
    from stubs import (set_globals_arena_top, reset_globals_arena_top,
                       GLOBALS_SIZE)
    reset_globals_arena_top()
    o_slot = GLOBALS_BASE + 0x300
    c_slot = GLOBALS_BASE + GLOBALS_SIZE + 0x200300   # candidate base ran past
    oracle = _make_tracer(_rec(0, _SENTINEL_A, "foo", o_slot))
    cand = _make_tracer(_rec(0, _SENTINEL_A, "foo", c_slot))

    d = compare_stub_arg_traces(oracle, cand, seed_label="s14")
    assert d.arg_mismatches == 1, "without the arena top this must still fail"

    set_globals_arena_top(c_slot + 0x100)
    d = compare_stub_arg_traces(oracle, cand, seed_label="s14b")
    assert d.arg_mismatches == 0, f"expected soft match, got {d.arg_mismatches}"
    assert d.soft_stack_ptr_matches == 1, d.soft_stack_ptr_matches
    reset_globals_arena_top()
    print("  PASS  test_candidate_globals_slot_above_nominal_top")


def test_rdata_dir32_patched():
    relocs = [
        _FakeReloc(IMAGE_REL_I386_DIR32, ".rdata$switch", 0),
        _FakeReloc(IMAGE_REL_I386_DIR32, ".rdata", 4),
    ]
    patched, slots, seeds = patch_dir32_relocs(
        b"\x00" * 4 + b"\x10\x00\x00\x00",
        relocs,
        defined_symbols=set(),
        return_slots=True,
        rdata_map={
            ".rdata$switch": b"\x11\x22\x33\x44",
            ".rdata": b"\x55" * 32,
        },
    )
    assert int.from_bytes(patched[:4], "little") == GLOBALS_BASE
    assert int.from_bytes(patched[4:8], "little") == GLOBALS_BASE + 0x100 + 0x10
    assert slots[".rdata$switch"] == GLOBALS_BASE
    assert slots[".rdata"] == GLOBALS_BASE + 0x100
    assert seeds[GLOBALS_BASE] == b"\x11\x22\x33\x44"
    assert seeds[GLOBALS_BASE + 0x100] == b"\x55" * 32
    print("  PASS  test_rdata_dir32_patched")


def test_empty_traces():
    oracle = StubArgTracer()
    cand = StubArgTracer()
    d = compare_stub_arg_traces(oracle, cand, seed_label="s7")
    assert not d.has_differences()
    assert d.total_calls == 0
    print("  PASS  test_empty_traces")


def test_excused_sentinel_is_dropped_from_both_sides():
    """A callee one side resolves internally must not read as a divergence.

    When the oracle maps its whole .text, its intra-.text calls never reach
    a sentinel and so are never recorded, while the candidate's equivalents
    are. Both execute the same bytes, so comparing unfiltered reports a
    phantom `oracle=<end at 0>` seq-divergence (observed on FUN_0013c620).
    """
    oracle = _make_tracer()                                   # recorded nothing
    cand = _make_tracer(
        _rec(0, _SENTINEL_A, "datum_get", 0x00, 0x02),
        _rec(1, _SENTINEL_B, "bar", 0xAA),
    )
    # Neither sentinel is comparable -> nothing to diff.
    d = compare_stub_arg_traces(oracle, cand, seed_label="s15",
                                comparable_sentinels=set())
    assert not d.has_differences(), f"excused calls must not diverge: {d}"
    assert d.total_calls == 0
    print("  PASS  test_excused_sentinel_is_dropped_from_both_sides")


def test_unexcused_missing_call_still_diverges():
    """The boundary: a genuinely DROPPED call must keep failing.

    "No record" is also what a dropped call looks like, so the filter must
    be driven by whether the silent side resolves the callee internally --
    never by absence alone. FUN_00019110 omits the oracle's leading
    datum_get; if this ever passes, that real bug goes invisible.
    """
    oracle = _make_tracer(
        _rec(0, _SENTINEL_A, "datum_get", 0x700300, 0x02),
        _rec(1, _SENTINEL_B, "bar", 0xAA),
    )
    cand = _make_tracer(
        _rec(0, _SENTINEL_B, "bar", 0xAA),
    )
    # Both sentinels ARE comparable (neither side resolves them internally).
    d = compare_stub_arg_traces(oracle, cand, seed_label="s16",
                                comparable_sentinels={_SENTINEL_A, _SENTINEL_B})
    assert d.has_differences(), "a dropped call must still diverge"
    assert d.sequence_diverged
    print("  PASS  test_unexcused_missing_call_still_diverges")


def test_filter_none_preserves_historical_behaviour():
    """Default (None) must compare everything, exactly as before."""
    oracle = _make_tracer(_rec(0, _SENTINEL_A, "foo", 0x10, 0x20))
    cand = _make_tracer(_rec(0, _SENTINEL_A, "foo", 0x20, 0x10))
    d = compare_stub_arg_traces(oracle, cand, seed_label="s17")
    assert d.has_differences() and d.arg_mismatches == 2
    print("  PASS  test_filter_none_preserves_historical_behaviour")


def test_shifted_sequence_reports_no_arg_mismatches():
    """A sequence shifted by one extra leading call must not manufacture
    argument evidence against the calls that DO match.

    The real shape, from object_has_node:
      oracle    = [tag_get('obje',0), tag_get('mode',0)]
      candidate = [datum_get(0,0),   tag_get('obje',0), tag_get('mode',0)]
    The candidate makes exactly the same two tag_get calls, but pairing the
    lists positionally compares tag_get against datum_get and reported
    "arg[0]: oracle=0x6f626a65 candidate=0x0" -- a dropped tag-group literal
    that does not exist. ~20 of the ledger's 50 arg_mismatch entries were this.
    """
    oracle = _make_tracer(
        _rec(0, _SENTINEL_A, "tag_get", 0x6F626A65, 0x00),
        _rec(1, _SENTINEL_B, "tag_get", 0x6D6F6465, 0x00),
    )
    cand = _make_tracer(
        _rec(0, _SENTINEL_C, "datum_get", 0x00, 0x00),
        _rec(1, _SENTINEL_A, "tag_get", 0x6F626A65, 0x00),
        _rec(2, _SENTINEL_B, "tag_get", 0x6D6F6465, 0x00),
    )
    d = compare_stub_arg_traces(
        oracle, cand, seed_label="s18",
        comparable_sentinels={_SENTINEL_A, _SENTINEL_B, _SENTINEL_C})
    assert d.sequence_diverged, "the sequence genuinely differs -- still fail"
    assert d.sequence_diverge_index == 0, (
        f"first disagreement is at index 0, got {d.sequence_diverge_index}; "
        "a min(len) index would leave misaligned pairs arg-compared")
    assert d.arg_mismatches == 0, (
        f"expected no fabricated arg mismatches, got {d.arg_mismatches}: "
        f"{d.details}")
    print("  PASS  test_shifted_sequence_reports_no_arg_mismatches")


def test_real_arg_bug_before_divergence_still_caught():
    """The boundary for the truncation: an argument bug in the matching PREFIX
    must survive, even when the sequence diverges later on. Truncating too
    eagerly (e.g. at index 0 whenever lengths differ) would hide it."""
    oracle = _make_tracer(
        _rec(0, _SENTINEL_A, "foo", 0x10, 0x20),
        _rec(1, _SENTINEL_B, "bar", 0xAA),
    )
    cand = _make_tracer(
        _rec(0, _SENTINEL_A, "foo", 0x20, 0x10),   # swapped -- REAL bug
        _rec(1, _SENTINEL_C, "baz", 0xAA),         # sequence diverges here
    )
    d = compare_stub_arg_traces(
        oracle, cand, seed_label="s19",
        comparable_sentinels={_SENTINEL_A, _SENTINEL_B, _SENTINEL_C})
    assert d.sequence_diverged and d.sequence_diverge_index == 1
    assert d.arg_mismatches == 2, (
        f"the swapped args at index 0 are before the divergence and must "
        f"still be reported, got {d.arg_mismatches}")
    print("  PASS  test_real_arg_bug_before_divergence_still_caught")


_REAL_GLOBAL = 0x0046BA4C   # input_flush's first csmemset target


def _with_slot_map(mapping, fn):
    """Run fn() with the oracle slot -> real-address map installed."""
    from stubs import set_globals_slot_real_map, reset_globals_slot_real_map
    set_globals_slot_real_map(mapping)
    try:
        return fn()
    finally:
        reset_globals_slot_real_map()


def test_slot_vs_real_global_soft_matches():
    """`&global` is a slot address in the oracle and a real VA in the
    candidate.  Same object, two address spaces -- not an arg bug."""
    oracle = _make_tracer(_rec(0, _SENTINEL_A, "_csmemset", GLOBALS_BASE, 0, 8))
    cand = _make_tracer(_rec(0, _SENTINEL_A, "_csmemset", _REAL_GLOBAL, 0, 8))
    d = _with_slot_map(
        {GLOBALS_BASE: _REAL_GLOBAL},
        lambda: compare_stub_arg_traces(oracle, cand, seed_label="sg0"))
    assert d.arg_mismatches == 0, f"expected soft match, got {d.arg_mismatches}"
    assert d.soft_reasons.get("globals-slot-alias") == 1, d.soft_reasons
    print("  PASS  test_slot_vs_real_global_soft_matches")


def test_slot_alias_preserves_offset_within_the_global():
    """Offset into the object must agree: slot+0x10 pairs with real+0x10."""
    oracle = _make_tracer(_rec(0, _SENTINEL_A, "_csmemset", GLOBALS_BASE + 0x10))
    cand = _make_tracer(_rec(0, _SENTINEL_A, "_csmemset", _REAL_GLOBAL + 0x10))
    d = _with_slot_map(
        {GLOBALS_BASE: _REAL_GLOBAL},
        lambda: compare_stub_arg_traces(oracle, cand, seed_label="sg1"))
    assert d.arg_mismatches == 0, f"expected soft match, got {d.arg_mismatches}"
    print("  PASS  test_slot_alias_preserves_offset_within_the_global")


def test_wrong_offset_into_right_global_is_still_reported():
    """The soundness property: identifying the global correctly does NOT
    excuse indexing into it wrongly.  A range-based excusal would swallow
    this; the exact base+offset test must not."""
    oracle = _make_tracer(_rec(0, _SENTINEL_A, "_csmemset", GLOBALS_BASE + 0x10))
    cand = _make_tracer(_rec(0, _SENTINEL_A, "_csmemset", _REAL_GLOBAL + 0x20))
    d = _with_slot_map(
        {GLOBALS_BASE: _REAL_GLOBAL},
        lambda: compare_stub_arg_traces(oracle, cand, seed_label="sg2"))
    assert d.arg_mismatches == 1, (
        f"a wrong offset into a correctly-identified global must still be "
        f"reported, got {d.arg_mismatches}")
    print("  PASS  test_wrong_offset_into_right_global_is_still_reported")


def test_slot_alias_inactive_without_a_map():
    """Strictly additive: with no map installed, nothing is excused."""
    oracle = _make_tracer(_rec(0, _SENTINEL_A, "_csmemset", GLOBALS_BASE))
    cand = _make_tracer(_rec(0, _SENTINEL_A, "_csmemset", _REAL_GLOBAL))
    d = _with_slot_map(
        {}, lambda: compare_stub_arg_traces(oracle, cand, seed_label="sg3"))
    assert d.arg_mismatches == 1, (
        f"no map means no excusal, got {d.arg_mismatches}")
    print("  PASS  test_slot_alias_inactive_without_a_map")


def test_slot_alias_covers_index_below_the_arena():
    """game_engine_clear_goal_position computes base + (short)idx * 0x20, so a
    negative index puts the oracle's value BELOW GLOBALS_BASE.  A range check
    excused only the non-negative seeds; the displacement test must cover
    both."""
    off = -0x20 * 3
    oracle = _make_tracer(_rec(0, _SENTINEL_A, "_csmemset", GLOBALS_BASE + off))
    cand = _make_tracer(_rec(0, _SENTINEL_A, "_csmemset", _REAL_GLOBAL + off))
    assert GLOBALS_BASE + off < GLOBALS_BASE, "test premise: must be below arena"
    d = _with_slot_map(
        {GLOBALS_BASE: _REAL_GLOBAL},
        lambda: compare_stub_arg_traces(oracle, cand, seed_label="sg5"))
    assert d.arg_mismatches == 0, (
        f"negative index must still soft-match, got {d.arg_mismatches}")
    print("  PASS  test_slot_alias_covers_index_below_the_arena")


def test_unmapped_slot_is_still_reported():
    """A slot with no XBE-derived real address must not be excused against
    an arbitrary candidate value."""
    oracle = _make_tracer(_rec(0, _SENTINEL_A, "_csmemset", GLOBALS_BASE + 0x100))
    cand = _make_tracer(_rec(0, _SENTINEL_A, "_csmemset", _REAL_GLOBAL))
    d = _with_slot_map(
        {GLOBALS_BASE: _REAL_GLOBAL},
        lambda: compare_stub_arg_traces(oracle, cand, seed_label="sg4"))
    assert d.arg_mismatches == 1, (
        f"unmapped slot must not be excused, got {d.arg_mismatches}")
    print("  PASS  test_unmapped_slot_is_still_reported")


def main():
    # Auto-discover, so a new test is never silently left out of the run.
    print("Running stub-arg trace comparator self-tests...")
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
