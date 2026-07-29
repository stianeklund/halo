#!/usr/bin/env python3
"""Self-test for the divergence triage classifiers (Phase 1).

Run with:
    python3 tools/equivalence/test_triage_classify.py

The defect pinned here: classify() had no awareness of the stub-argument
differential, which is the shape of 272 of the 331 divergences in the
2026-07-28 batch. Every one of them fell through to "genuine" -- the bucket
labelled "real bugs, INVESTIGATE" -- so the largest category in the report was
dominated by differences that cannot be lift bugs at all:

  * assert metadata. _display_assert takes (message, __FILE__, __LINE__). Our
    lifted sources do not reproduce the original's line numbering, so arg[2]
    differs by construction (oracle=0xa89 candidate=0x374 is a real observed
    pair), and our string literals land in a different section than the
    original's, so arg[0]/arg[1] differ too. 101 functions diverge for this
    reason and nothing else.

  * stack-pointer arguments. MSVC (oracle) and clang (candidate) lay out frames
    differently, so &local passed to a stubbed callee has a different numeric
    value on each side no matter how faithful the lift is.

A wrong argument to a *real* callee is the opposite: it names the callee and
the arg index, which is the docs/lift-learnings.md §10 caller-side argument
swap/drop signature. That must stay suspect-real.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(__file__))

from triage_failures import BUCKETS, classify, parse_smoke_log

import tempfile
from pathlib import Path


HEADER = """\
addr : 0x1c5a0
conv : cdecl
oracle code: 120 bytes, 3 relocs
lifted code: 118 bytes, 3 relocs
oracle class: stubbable
lifted class: stubbable
Running 50 seeds...
"""

FOOTER = """\
coverage: 61/70 blocks (87.1%)
confidence: high
"""


def _smoke(body):
    """Materialize a smoke log and parse it the way triage does."""
    tmp = Path(tempfile.mkdtemp()) / "t_smoke.log"
    tmp.write_text(HEADER + body + FOOTER, encoding="utf-8")
    return parse_smoke_log(tmp)


ROW = {"name": "t", "addr": "0x1c5a0", "obj": "t.obj",
       "seeds_passed": 0, "seeds_total": 50}


def test_assert_line_number_is_not_a_bug():
    """__LINE__ differing between oracle and candidate is expected."""
    body = "".join(
        f"  seed[ {i}] call[3] _display_assert arg[2]: "
        f"oracle=0xa89 candidate=0x374\n"
        f"  seed[ {i}] FAIL: stub-args: 1 arg mismatch(es)\n"
        for i in range(50)
    )
    cat, detail = classify(ROW, _smoke(body))
    assert cat == "assert_metadata", f"got {cat}: {detail}"
    assert BUCKETS[cat] == "harness-artifact", BUCKETS[cat]
    assert "2697" in detail or "__LINE__" in detail, detail
    print("  ok  assert __LINE__ divergence -> assert_metadata/harness-artifact")


def test_assert_string_pointers_are_not_a_bug():
    """Message/__FILE__ literals live in different sections on each side."""
    body = "".join(
        f"  seed[ {i}] call[1] _display_assert arg[0]: "
        f"oracle=0x1f4a20 candidate=0x4c1180\n"
        f"  seed[ {i}] FAIL: stub-args: 1 arg mismatch(es)\n"
        for i in range(50)
    )
    cat, _ = classify(ROW, _smoke(body))
    assert cat == "assert_metadata", cat
    print("  ok  assert string-pointer divergence -> assert_metadata")


def test_wrong_arg_to_real_callee_stays_suspect():
    """The §10 signature must not be swallowed by the assert exemption."""
    body = "".join(
        f"  seed[ {i}] call[2] _tag_block_get_element arg[1]: "
        f"oracle=0x8 candidate=0x0\n"
        f"  seed[ {i}] FAIL: stub-args: 1 arg mismatch(es)\n"
        for i in range(50)
    )
    cat, detail = classify(ROW, _smoke(body))
    assert cat == "arg_mismatch", f"got {cat}: {detail}"
    assert BUCKETS[cat] == "suspect-real", BUCKETS[cat]
    assert "_tag_block_get_element" in detail, detail
    print("  ok  wrong arg to real callee -> arg_mismatch/suspect-real")


def test_assert_plus_real_callee_stays_suspect():
    """An assert diff must not launder a co-occurring real-callee diff."""
    body = "".join(
        f"  seed[ {i}] call[1] _display_assert arg[2]: oracle=0xa89 candidate=0x374\n"
        f"  seed[ {i}] call[2] _datum_get arg[0]: oracle=0x700300 candidate=0x0\n"
        f"  seed[ {i}] FAIL: stub-args: 2 arg mismatch(es)\n"
        for i in range(50)
    )
    cat, _ = classify(ROW, _smoke(body))
    assert BUCKETS[cat] == "suspect-real", f"{cat} -> {BUCKETS[cat]}"
    print("  ok  assert diff + real-callee diff -> suspect-real")


def test_assert_arg_with_garbage_value_is_not_exempt():
    """A non-metadata-shaped value in an assert arg is not auto-forgiven."""
    body = "".join(
        f"  seed[ {i}] call[1] _display_assert arg[2]: "
        f"oracle=0xa89 candidate=0xdeadbeef\n"
        f"  seed[ {i}] FAIL: stub-args: 1 arg mismatch(es)\n"
        for i in range(50)
    )
    cat, _ = classify(ROW, _smoke(body))
    assert cat != "assert_metadata", cat
    assert BUCKETS[cat] == "suspect-real", f"{cat} -> {BUCKETS[cat]}"
    print("  ok  garbage value in assert arg -> not exempted")


def test_stack_pointer_args_are_frame_layout():
    body = "".join(
        f"  seed[ {i}] FAIL: stub-args: 2 arg mismatch(es), 2 stack-ptr arg(s)\n"
        for i in range(50)
    )
    cat, _ = classify(ROW, _smoke(body))
    assert cat == "stack_ptr_args", cat
    assert BUCKETS[cat] == "harness-artifact", BUCKETS[cat]
    print("  ok  stack-pointer args -> stack_ptr_args/harness-artifact")


def test_value_divergence_is_not_exempted_by_assert_args():
    """A diverging return value is a behavioural difference regardless."""
    body = "".join(
        f"  seed[ {i}] call[1] _display_assert arg[2]: oracle=0xa89 candidate=0x374\n"
        f"  seed[ {i}] FAIL: EAX: oracle=0x1 lifted=0x0\n"
        for i in range(50)
    )
    cat, _ = classify(ROW, _smoke(body))
    assert cat != "assert_metadata", cat
    assert BUCKETS[cat] == "suspect-real", f"{cat} -> {BUCKETS[cat]}"
    print("  ok  EAX divergence alongside assert args -> suspect-real")


def test_memset_fill_literal_is_benign():
    """memset(p, -1, n) and memset(p, 0xff, n) write identical bytes.

    All 92 fill-argument divergences in the 2026-07-28 batch were this pair.
    """
    body = "".join(
        f"  seed[ {i}] call[0] _csmemset arg[1]: oracle=0xffffffff candidate=0xff\n"
        f"  seed[ {i}] FAIL: stub-args: 1 arg mismatch(es)\n"
        for i in range(50)
    )
    cat, detail = classify(ROW, _smoke(body))
    assert cat == "benign_arg_width", f"got {cat}: {detail}"
    assert BUCKETS[cat] == "benign-difference", BUCKETS[cat]
    print("  ok  memset fill 0xff vs -1 -> benign_arg_width")


def test_memset_fill_differing_low_byte_is_suspect():
    """Only the low byte is unobservable; a different one is a real bug."""
    body = "".join(
        f"  seed[ {i}] call[0] _csmemset arg[1]: oracle=0xffffffff candidate=0x0\n"
        f"  seed[ {i}] FAIL: stub-args: 1 arg mismatch(es)\n"
        for i in range(50)
    )
    cat, _ = classify(ROW, _smoke(body))
    assert cat == "arg_mismatch", cat
    assert BUCKETS[cat] == "suspect-real", BUCKETS[cat]
    print("  ok  memset fill with different low byte -> suspect-real")


def test_memset_benign_does_not_launder_other_args():
    """A benign fill arg must not excuse a wrong destination or size."""
    body = "".join(
        f"  seed[ {i}] call[0] _csmemset arg[1]: oracle=0xffffffff candidate=0xff\n"
        f"  seed[ {i}] call[0] _csmemset arg[2]: oracle=0x40 candidate=0x10\n"
        f"  seed[ {i}] FAIL: stub-args: 2 arg mismatch(es)\n"
        for i in range(50)
    )
    cat, detail = classify(ROW, _smoke(body))
    assert cat == "arg_mismatch", f"got {cat}: {detail}"
    assert "arg[2]" in detail, detail
    print("  ok  benign fill + wrong size -> arg_mismatch on the size")


def test_byte_fill_rule_is_scoped_to_memset():
    """The mod-256 rule is a memset contract, not a general one."""
    body = "".join(
        f"  seed[ {i}] call[0] _datum_get arg[1]: oracle=0xffffffff candidate=0xff\n"
        f"  seed[ {i}] FAIL: stub-args: 1 arg mismatch(es)\n"
        for i in range(50)
    )
    cat, _ = classify(ROW, _smoke(body))
    assert cat == "arg_mismatch", cat
    print("  ok  mod-256 exemption does not apply to non-memset callees")


def test_call_seq_divergence_is_suspect():
    body = "".join(
        f"  seed[ {i}] FAIL: stub-args: call-seq diverged at index 2\n"
        for i in range(50)
    )
    cat, detail = classify(ROW, _smoke(body))
    assert cat == "call_seq", cat
    assert BUCKETS[cat] == "suspect-real", BUCKETS[cat]
    assert "index 2" in detail, detail
    print("  ok  call-sequence divergence -> call_seq/suspect-real")


def test_narrow_field_read_wide_is_load_width():
    """oracle reads 16 bits, we read 32 and pull in the adjacent fill.

    This shape also satisfies _is_dirty_eax (low 16 bits match), which buckets
    it harness-artifact. It must reach load_width/suspect-real instead, or a real
    wrong-width return type is filed as benign and never looked at. Real case:
    unit_inventory_next_weapon returns a short NONE (0xffff), our lift an int -1.
    """
    body = "".join(
        f"  seed[ {i}] FAIL: EAX: oracle=0x0000ffff lifted=0xffffffff\n"
        for i in range(8)
    )
    cat, detail = classify(ROW, _smoke(body))
    assert cat == "load_width", cat
    assert BUCKETS[cat] == "suspect-real", BUCKETS[cat]
    print("  ok  16-bit field read as 32-bit -> load_width/suspect-real")


def test_byte_field_read_wide_is_load_width():
    """Same bug one width down: oracle reads 8 bits of 0xcc, we read 32."""
    body = "".join(
        f"  seed[ {i}] FAIL: EAX: oracle=0x000000cc lifted=0xcccccccc\n"
        for i in range(4)
    )
    cat, _ = classify(ROW, _smoke(body))
    assert cat == "load_width", cat
    print("  ok  8-bit field read as 32-bit -> load_width")


def test_stale_upper_bits_stays_dirty_eax():
    """The benign direction must NOT be captured by the new detector.

    A real `mov ax` leaves arbitrary leftovers in the upper bits -- not a
    repeated-byte fill -- so it stays dirty_eax/harness-artifact. Without this
    pin, widening load_width would quietly reclassify a whole benign class as
    suspect-real and bury the genuine bugs in noise.
    """
    body = "".join(
        f"  seed[ {i}] FAIL: EAX: oracle=0x00601234 lifted=0x00001234\n"
        for i in range(50)
    )
    cat, _ = classify(ROW, _smoke(body))
    assert cat == "dirty_eax", cat
    assert BUCKETS[cat] == "harness-artifact", BUCKETS[cat]
    print("  ok  stale upper bits stay dirty_eax/harness-artifact")


def test_every_category_has_a_bucket():
    """A category with no bucket would silently become needs-evidence."""
    import triage_failures as tf
    import re
    src = Path(tf.__file__).read_text(encoding="utf-8")
    body = src.split("def classify(", 1)[1].split("\ndef ", 1)[0]
    returned = set(re.findall(r'return\s+"([a-z_]+)"\s*,', body))
    missing = returned - set(BUCKETS)
    assert not missing, f"categories with no bucket: {sorted(missing)}"
    print(f"  ok  all {len(returned)} classify() categories have a bucket")


def main():
    tests = [v for k, v in sorted(globals().items()) if k.startswith("test_")]
    print(f"Running {len(tests)} triage classifier tests...\n")
    failed = 0
    for t in tests:
        try:
            t()
        except AssertionError as exc:
            print(f"  FAIL  {t.__name__}: {exc}")
            failed += 1
    print()
    if failed:
        print(f"{failed}/{len(tests)} FAILED")
        return 1
    print(f"All {len(tests)} passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
