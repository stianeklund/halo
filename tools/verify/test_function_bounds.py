#!/usr/bin/env python3
"""Self-tests for the committed function-bounds table.

A wrong bound is invisible: the reference still builds, the score still prints,
it is just computed over the wrong bytes.  These tests pin the cases that
actually went wrong before the table existed --

  * a 5-byte tail-jump thunk scanned as a 7985-byte function (0x18e300),
  * six functions whose real neighbour is absent from kb.json, so the kb gap
    over-counted them by up to 8x,
  * FUN_00174510, whose interior `ret` is NOT the end because an earlier
    `jl` proves an out-of-line tail block follows,
  * two functions with a switch table living past the final `ret`,

-- plus the invariant that makes the table safe to consume at all: no entry
slices an instruction in half.

Run: rtk python3 tools/verify/test_function_bounds.py
"""

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import function_bounds as fb

_TABLE = fb.load_table()
_ENTRIES = {k: v for k, v in _TABLE.items() if k != "_meta"}
_XBE = fb._xbe()


def _entry(addr: int) -> dict:
    e = _ENTRIES.get("0x%x" % addr)
    assert e is not None, f"0x{addr:x} missing from function_bounds.json"
    return e


def _end(addr: int) -> int:
    return int(_entry(addr)["end"], 16)


def test_meta_matches_the_binary():
    """The table must declare which XBE it was generated from."""
    meta = _TABLE["_meta"]
    assert meta["xbe_md5"] == "c7869590a1c64ad034e49a5ee0c02465", meta
    assert meta["version"] == fb.VERSION, meta
    assert meta["entries"] == len(_ENTRIES), meta


def test_tail_jump_thunk_is_five_bytes():
    """0x18e300 is `e9 db1c0000` and nothing else.

    Scanned with a wide window, the jump target counts as an outstanding branch
    and the body stays open all the way to it -- 0x1f31 bytes, 2882
    instructions, for a one-instruction function.
    """
    e = _entry(0x18E300)
    assert e["kind"] == "thunk", e
    assert _end(0x18E300) == 0x18E305, f"{e} -- want end 0x18e305"


def test_listing_gap_functions_get_true_ends():
    """kb.json is not a complete listing; the gap is not a size.

    Each of these is followed by a real function that kb.json does not list, so
    the kb gap over-counts.  Sizes are from the pristine XBE.
    """
    cases = {  # addr: (end, real neighbour, kb gap end, neighbour is unlisted)
        0x155110: (0x155126, 0x155130, 0x155350, True),
        0x1592E0: (0x159300, 0x159300, 0x1595C0, True),
        0x15C2D0: (0x15C336, 0x15C340, 0x15C5F0, True),
        0x159070: (0x1590D3, 0x1590E0, 0x1592E0, True),
        # 0x155620 IS listed: this one is not a listing hole, it is the same
        # rule tightening a bound by the 10 bytes of alignment filler that the
        # kb gap would have counted as body.
        0x155580: (0x155616, 0x155620, 0x155620, False),
        0x0FB3C0: (0x0FB406, 0x0FB410, 0x0FB510, True),
    }
    starts = {a for a, _ in fb.load_kb_functions()[0]}
    for addr, (want, neighbour, kb_end, is_hole) in cases.items():
        got = _end(addr)
        assert got == want, f"0x{addr:x}: end 0x{got:x}, want 0x{want:x}"
        assert got < kb_end, (
            f"0x{addr:x}: end 0x{got:x} reached the kb gap end 0x{kb_end:x} -- "
            f"the bound came from the listing, not the binary")
        assert (neighbour not in starts) == is_hole, (
            f"0x{neighbour:x}: listed={neighbour in starts}, fixture expects "
            f"unlisted={is_hole} -- kb.json changed under this test")


def test_interior_ret_does_not_end_a_function():
    """FUN_00174510 looks finished at 0x174622 -- it is not.

    `jl 0x174622` at 0x174608 is an outstanding branch target at/after that
    `ret`, which proves MSVC put a tail block out of line.  Cutting there costs
    the whole tail (the function scored 53.4% against a reference bounded that
    way, 100.0% against the real one).
    """
    got = _end(0x174510)
    assert got > 0x174622, f"end 0x{got:x} cut the function at its interior ret"
    assert got == 0x174690, f"end 0x{got:x}, want 0x174690"


def test_table_data_overrides_are_present():
    """A switch table after the final `ret` is part of the function."""
    for addr, end, name in ((0x84520, 0x8473A, "FUN_00084520"),
                            (0x12A2D0, 0x12A4F4,
                             "network_game_client_start_frame")):
        e = _entry(addr)
        assert e["kind"] == "table_data", e
        assert e["name"] == name, e
        assert int(e["end"], 16) == end, e
        assert e.get("note"), f"0x{addr:x}: an override must say why"
        assert addr in fb.OVERRIDES, f"0x{addr:x} not in the generator OVERRIDES"


def test_no_entry_slices_an_instruction():
    """Every bound lands on an instruction boundary.

    `table_data` is exempt by construction (its bytes ARE data) and `unmapped`
    has no bytes at all.
    """
    data, secs = _XBE
    bad = []
    for key, e in _ENTRIES.items():
        if e["kind"] in ("table_data", "unmapped"):
            continue
        start, end = int(key, 16), int(e["end"], 16)
        _, _, problem = fb._walk(data, secs, start, end)
        if problem:
            bad.append(f"{key} ({e['name']}): {problem}")
    assert not bad, "bounds that do not land on an instruction boundary:\n  " \
        + "\n  ".join(bad[:20])


def test_auto_entries_end_on_a_terminator():
    """`auto` means "the binary said so" -- it must mean a ret/jmp."""
    data, secs = _XBE
    bad = []
    for key, e in _ENTRIES.items():
        if e["kind"] != "auto":
            continue
        start, end = int(key, 16), int(e["end"], 16)
        _, mnem, _ = fb._walk(data, secs, start, end)
        if not fb._is_terminator(mnem):
            bad.append(f"{key} ({e['name']}): ends on {mnem}")
    assert not bad, "auto entries not ending on a terminator:\n  " \
        + "\n  ".join(bad[:20])


def test_every_kb_function_has_an_entry():
    """The scoring universe is every listed function, not just ported ones."""
    funcs, _ = fb.load_kb_functions()
    missing = [f"0x{a:x}" for a, _ in funcs if "0x%x" % a not in _ENTRIES]
    assert not missing, f"{len(missing)} kb.json functions absent: {missing[:10]}"
    assert len(_ENTRIES) == len(funcs), (
        f"{len(_ENTRIES)} entries for {len(funcs)} functions")


def test_bounds_are_sane():
    """No zero-length or backwards spans outside the flagged kinds."""
    bad = []
    for key, e in _ENTRIES.items():
        start, end = int(key, 16), int(e["end"], 16)
        if end <= start and e["kind"] != "unmapped":
            bad.append(f"{key} ({e['name']}): end 0x{end:x} <= start")
    assert not bad, "\n  ".join(bad[:20])


def test_agrees_with_vc71_verify_true_end():
    """The lifted scan must stay identical to the one still inside vc71_verify.

    `true_end_offset` was lifted rather than imported (vc71_verify will consume
    this module, so importing it back would be a cycle).  This test is what
    keeps the two copies from drifting until they are unified.
    """
    import vc71_verify as v
    data, secs = _XBE
    funcs, _ = fb.load_kb_functions()
    starts = [a for a, _ in funcs]
    bad = []
    for i in range(0, len(starts) - 1, 17):  # ~470 samples, spread over the map
        addr, nxt = starts[i], starts[i + 1]
        limit = nxt - addr
        mine = fb.true_end_offset(data, secs, addr, limit)
        theirs = v._true_end_offset(addr, limit)
        if mine != theirs:
            bad.append(f"0x{addr:x} limit=0x{limit:x}: {mine} != {theirs}")
    assert not bad, "lifted scan drifted from vc71_verify._true_end_offset:\n  " \
        + "\n  ".join(bad[:10])


def test_bound_source_distribution():
    """The binary, not the listing, must be deciding most bounds.

    If a change silently reverts to the kb gap this collapses, and the whole
    point of the table is gone.  Printed as well as asserted -- the numbers are
    the review artifact for how big the listing-gap class actually is.
    """
    _, stats = fb.build()
    src = stats["sources"]
    print("    bound source:", ", ".join(
        f"{k}={v}" for k, v in sorted(src.items(), key=lambda kv: -kv[1])))
    print("    kb-gap over-count:", ", ".join(
        f"{k}={v}" for k, v in stats["slack"].items()))
    total = sum(src.values())
    assert src.get("true_end", 0) > total // 2, (
        f"only {src.get('true_end', 0)}/{total} bounds came from the binary")
    assert src.get("kb_next", 0) + src.get("unbounded", 0) < 50, (
        f"{src.get('kb_next', 0)} functions fell back to the kb gap")


def test_committed_table_is_current():
    """`--check` is the CI gate; this is the same comparison, in-process."""
    table, _ = fb.build()
    want = fb.serialize(table)
    got = fb.TABLE.read_text()
    assert want == got, (
        "function_bounds.json is stale -- run "
        "`rtk python3 tools/verify/function_bounds.py` "
        f"(committed {len(got)} bytes, regenerated {len(want)})")


def test_serialization_is_one_line_per_entry():
    """A one-function change must be a one-line diff, or review is hopeless."""
    text = fb.TABLE.read_text()
    lines = text.splitlines()
    assert lines[0] == "{" and lines[-1] == "}", "unexpected wrapper"
    assert len(lines) == len(_ENTRIES) + 3, (
        f"{len(lines)} lines for {len(_ENTRIES)} entries + meta + braces")
    assert json.loads(text) == _TABLE, "round-trip failed"


def main():
    print("Running function-bounds self-tests...")
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
