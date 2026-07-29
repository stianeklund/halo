#!/usr/bin/env python3
"""Self-tests for delinked-reference selection (vc71_verify.choose_unit).

A DROP produces no score line, so choosing a narrow reference does not look
like a failure -- it looks like the file has fewer functions.  Measured
2026-07-29: objects.c was scored against a 2-function objects_FUN_00084a10.obj
while a 754-function objects.obj sat on disk unregistered, and actor_moving.c
against 1 of 33; the two actor_move_* functions the equivalence lane had
flagged could not be byte-checked at all.

Both directions of a name-only rule are wrong, which is what these tests pin:

  * "prefer the exact stem"  loses coverage -- delinked/units.obj holds 18
    symbols where units_new.obj holds 88.
  * "prefer the widest"      picks another TU -- delinked/files_windows.obj
    (368 symbols) is a candidate for files.c, whose own reference has 17, and
    delinked/actors.obj is a candidate for actor_moving.c.

So width decides, but only among same-TU names, and "same TU" admits only
address-range / FUN_<addr> suffixes -- never a bare word, since sibling TUs
share prefixes.

Run: rtk python3 tools/verify/test_ref_selection.py
"""

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import vc71_verify as v


def _chosen(rel_source):
    unit = v.choose_unit(str(v.REPO_ROOT / rel_source), _UNITS, None)
    return Path(unit["base_path"]).name if unit else None


def _width(base_name):
    return len(v.object_symbols(v.REPO_ROOT / "delinked" / base_name))


def _old_choice(rel_source):
    """The pre-fix rule: alphabetical, chunks last, no width awareness."""
    existing = [u for u in v.find_units(str(v.REPO_ROOT / rel_source), _UNITS)
                if (v.REPO_ROOT / u.get("base_path", "")).exists()]

    def pri(u):
        base = Path(u.get("base_path", "")).name
        return (True, bool(re.match(r"[0-9a-f]{8}\.obj$", base, re.I)), base)

    existing.sort(key=pri)
    return Path(existing[0]["base_path"]).name if existing else None


_UNITS = v.load_units()


def test_sibling_tu_is_not_adopted():
    """files.c must keep files.obj, not the larger files_windows.obj.

    files_windows.obj is a DIFFERENT TU that merely shares the "files" prefix.
    Adopting it would score every function against unrelated code.
    """
    got = _chosen("src/halo/tag_files/files.c")
    assert got == "files.obj", (
        f"files.c adopted {got}; a sibling TU sharing the stem prefix must not "
        f"win on width")


def test_word_suffix_is_not_treated_as_same_tu():
    """units.c keeps units.obj: "_new" is not provably a slice of units.c.

    Deliberately conservative -- units_new.obj may well be a wider export of
    the same TU, but the name cannot prove it, and a wrong reference silently
    fakes the score.  If it is ever confirmed to be the same TU, register it
    and widen the suffix rule on purpose, not by accident.
    """
    got = _chosen("src/halo/units/units.c")
    assert got == "units.obj", f"units.c adopted {got} via a bare word suffix"


def test_whole_object_beats_narrow_slice():
    """The TU's own whole-object export must win over a 1-function slice."""
    for src, want in (("src/halo/units/bipeds.c", "bipeds.obj"),
                      ("src/halo/ai/actor_moving.c", "actor_moving.obj"),
                      ("src/halo/objects/objects.c", "objects.obj"),
                      ("halo/physics/bsp3d.c", "bsp3d.obj")):
        got = _chosen(src)
        assert got == want, f"{src}: chose {got}, want {want}"


def test_range_suffix_counts_as_same_tu():
    """<stem>_<hex>_<hex>.obj IS a slice of the TU and may win on width."""
    got = _chosen("src/halo/cache/predicted_resources.c")
    assert got is not None and got.startswith("predicted_resources"), got
    assert _width(got) >= _width("predicted_resources.obj"), (
        f"{got} is narrower than the exact-stem reference")


def test_no_source_gets_a_narrower_reference_than_before():
    """The fix must only ever widen. A narrowing means the rule mis-fired.

    Only files whose choice actually changed are measured, to keep the llvm-nm
    count small.
    """
    srcs = sorted({u.get("metadata", {}).get("source_path", "") for u in _UNITS
                   if u.get("metadata", {}).get("source_path")})
    narrowed = []
    for sp in srcs:
        old, new = _old_choice(sp), _chosen(sp)
        if old == new or not (old and new):
            continue
        if _width(new) < _width(old):
            narrowed.append(f"{sp}: {old} ({_width(old)}) -> {new} ({_width(new)})")
    assert not narrowed, "reference narrowed for:\n  " + "\n  ".join(narrowed)


def test_registered_whole_objects_exist():
    """Every delinked/<stem>.obj registered for a TU must be on disk.

    A registered-but-absent reference falls back to a narrow slice silently.
    """
    missing = []
    for u in _UNITS:
        sp = u.get("metadata", {}).get("source_path", "")
        bp = u.get("base_path", "")
        if not (sp and bp):
            continue
        if Path(bp).name.lower() != f"{Path(sp).stem.lower()}.obj":
            continue
        if not (v.REPO_ROOT / bp).exists():
            missing.append(f"{sp} -> {bp}")
    assert not missing, "whole-object reference registered but absent:\n  " + \
        "\n  ".join(missing)


def test_padding_trim_keeps_real_code():
    """The trim must remove only trailing nop/ret filler."""
    cases = (
        # (input, expected length, why)
        (["pushl %ebp", "popl %ebp", "retl"], 3, "clean ending untouched"),
        (["movl %eax, %ecx", "retl", "nop", "nop"], 2, "alignment nops dropped"),
        (["cmpl $0, %eax", "retl", "movl $1, %eax", "retl"], 4, "multi-ret body kept"),
        (["movl %eax, %ecx", "jmp *%edx"], 2, "tail-call kept"),
        (["movl %eax, %ecx", "retl", "nopw 0x0(%eax)"], 2, "multi-byte nop dropped"),
        (["retl"], 1, "empty stub kept"),
        (["nop", "nop"], 2, "all-padding slice left alone"),
    )
    for insns, want, why in cases:
        got = len(v._trim_trailing_padding(insns))
        assert got == want, f"{why}: {len(insns)} -> {got}, want {want}"


def test_bloated_slice_is_detected():
    """bipeds.obj's FUN_001a0680 slice is 152 insns for a 91-insn function."""
    import compare_obj as co
    slice_ = co.disassemble(str(v.REPO_ROOT / "delinked" / "bipeds.obj"))["FUN_001a0680"]
    assert len(slice_) == 152, f"fixture changed: slice is {len(slice_)} insns"
    assert v._slice_is_bloated(slice_), "trailing nop/ret filler not detected"
    assert len(v._trim_trailing_padding(slice_)) == 91, "trim did not reach the real ret"


def test_true_insn_count_reads_the_binary():
    """The XBE must yield a real length, else every choice silently no-ops.

    Anchored on two functions measured by hand: 0x1a0680 ends at +0xf3 (91
    instructions) and 0xb97b0 at +0xc4 (68), both well short of their kb.json
    spans (320 and 480 bytes), which is why kb.json cannot be the arbiter.
    """
    for fn, want in (("FUN_001a0680", 91), ("FUN_000b97b0", 68)):
        got = v._true_insn_count(fn)
        assert got == want, (
            f"{fn}: binary says {got}, hand-verified {want}; if this returns "
            f"None the reference choice has no arbiter and falls back silently")


def test_closer_to_truth_has_no_opinion_without_truth():
    """A None truth must never claim one candidate is better."""
    assert not v._closer_to_truth(10, 999, None)
    assert v._closer_to_truth(68, 154, 68)
    assert not v._closer_to_truth(154, 68, 68)


def main():
    print("Running delinked-reference selection self-tests...")
    tests = [val for name, val in sorted(globals().items())
             if name.startswith("test_") and callable(val)]
    failed = 0
    for t in tests:
        try:
            t()
            print(f"  PASS  {t.__name__}")
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
