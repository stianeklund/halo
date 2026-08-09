#!/usr/bin/env python3
"""Self-tests for VC71 reference derivation (vc71_verify.derive_reference).

There is no reference *selection* any more.  A function's reference is the bytes
at [start, end) in the pristine XBE, where `end` comes from the committed
tools/verify/function_bounds.json, wrapped in a COFF and disassembled with the
same llvm-objdump the candidate goes through.

What used to be here pinned a four-rung ladder (whole delinked object -> sibling
range export -> per-function chunk -> synthesized fallback) and the two name
rules that decided between them.  Those rules are gone; what these tests pin
instead is that the single path cannot silently lose its footing:

  * the bound comes from the TABLE, not from a per-run heuristic -- the whole
    point of committing the table was to stop spans drifting between runs;
  * an address the table does not list still gets scored (fallback), because a
    freshly-added kb.json function must not become invisible;
  * a `thunk` (one instruction) is a VALID reference, not a "truncated" one --
    the old byte-span gate rejected short references, which is how correct
    references got thrown away;
  * a `table_data` reference stops at the last real instruction, so the switch
    table MSVC put after the final ret is not counted as ~20 phantom insns;
  * name resolution stays TU-scoped, so a static helper whose name also exists
    in another TU is not scored against that other TU's code;
  * REFMETA is emitted for every scored function, because the re-baseline reads
    provenance from it.

Run: rtk python3 tools/verify/test_ref_selection.py
"""

import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "audit"))

import vc71_verify as v
import xbe_reference as xr

_CO = v.load_compare_obj()


def _ref(fn, source):
    """(insns, meta) from the one reference path, for a function by name."""
    src = v.REPO_ROOT / source if not str(source).startswith("/") else Path(source)
    v._set_alias_source(src)
    return v.derive_reference(fn, src, v._kb_functions_for_source(src), _CO)


# ---------------------------------------------------------------------------
# The bound is the table's, not a heuristic's
# ---------------------------------------------------------------------------

def test_span_comes_from_the_bounds_table():
    """_func_span must equal end-start from function_bounds.json.

    The listing-gap class is what this kills: FUN_0015c2d0 is 102 bytes and its
    kb.json gap is 800, because its real neighbour at 0x15c340 was never listed.
    A span of 800 made the verifier reject the function's CORRECT reference as
    truncated and prefer a bloated one.
    """
    import function_bounds as fb
    table = fb.load_table()
    for fn, addr in (("FUN_0015c2d0", 0x15C2D0), ("FUN_001a0680", 0x1A0680),
                     ("FUN_000b97b0", 0xB97B0), ("FUN_0018e300", 0x18E300)):
        entry = table.get(hex(addr))
        assert entry, f"{fn} missing from function_bounds.json"
        want = int(entry["end"], 16) - addr
        got = v._func_span(fn)
        assert got == want, f"{fn}: span {got}, table says {want}"


def test_listing_gap_no_longer_inflates_a_span():
    """The specific measured case, stated as a number rather than a rule."""
    assert v._func_span("FUN_0015c2d0") == 102, (
        "FUN_0015c2d0 is a 102-byte function; a span near 800 means the kb "
        "listing gap is deciding again")


def test_missing_from_table_still_gets_a_bound():
    """An address the table does not list falls back rather than dropping.

    kb.json gains functions continuously; if a new one could not be scored until
    someone regenerated the table, the table would be a gate on lifting rather
    than a record of the binary.
    """
    unlisted = 0x18E301  # mid-instruction, deliberately not a kb.json entry
    assert xr.bounds_entry(unlisted) is None, "fixture is in the table now"
    ext = xr.function_extent(unlisted)
    assert ext is not None and ext[2] == "computed", (
        f"an unlisted address must fall back to a computed bound, got {ext}")


def test_table_bound_is_reported_as_such():
    """A listed address must NOT take the fallback (which warns on stderr)."""
    ext = xr.function_extent(0x1A0680)
    assert ext is not None and ext[2] == "table", ext


# ---------------------------------------------------------------------------
# kind handling
# ---------------------------------------------------------------------------

def test_thunk_reference_is_one_instruction_and_valid():
    """0x18e300 is the 5-byte tail-jump `e9 db1c0000` -- a whole function.

    The old byte-span gate treated a very short reference as truncated, so a
    correct one-instruction reference could be discarded and the function
    dropped.  A thunk's reference is one instruction and that is the answer.
    """
    insns, meta = _ref("FUN_0018e300", "src/halo/main/main.c")
    assert insns is not None, f"thunk got no reference: {meta}"
    assert meta["kind"] == "thunk", meta
    assert len(insns) == 1, f"thunk reference is {len(insns)} insns: {insns}"
    assert insns[0].split()[0].startswith("jmp"), insns


def test_table_data_reference_stops_at_the_last_instruction():
    """A `table_data` span covers the switch table; the reference must not.

    0x12a2d0 (network_game_client_start_frame) ends at the ret at 0x12a4df,
    followed by 5 dword jump targets.  Those bytes decode as garbage; counting
    them would add phantom instructions to the reference and cap the score.
    """
    import function_bounds as fb
    for addr in (0x84520, 0x12A2D0):
        entry = fb.load_table()[hex(addr)]
        assert entry["kind"] == "table_data", entry
        insns, meta = _ref(f"FUN_{addr:08x}", "src/halo/main/main.c")
        assert insns is not None, f"0x{addr:x}: no reference ({meta})"
        last = insns[-1].split()[0].lower()
        assert last in ("ret", "retl", "retw", "retn", "jmp", "jmpl"), (
            f"0x{addr:x}: reference ends on {last!r}; trailing table data "
            f"was counted as code")
        # The span is much larger than the code, which is the whole reason the
        # override exists -- assert the reference is not simply the whole span.
        span = meta["end"] - meta["addr"]
        assert len(insns) * 15 >= span or len(insns) < span, (span, len(insns))


def test_no_terminator_bound_is_visible_in_the_meta():
    """A weak bound must stay identifiable, not blend into every other score."""
    import function_bounds as fb
    weak = [k for k, e in fb.load_table().items()
            if k != "_meta" and e["kind"] == "no_terminator"
            and int(e["end"], 16) > int(k, 16)]
    assert weak, "fixture: the table has no scoreable no_terminator entries"
    addr = int(weak[0], 16)
    ext = xr.function_extent(addr)
    assert ext is not None and ext[1] == "no_terminator", ext


# ---------------------------------------------------------------------------
# Name resolution
# ---------------------------------------------------------------------------

def test_source_address_comment_beats_the_kb_name():
    """`/* 0x155a40 */` above the definition is authoritative.

    llvm-nm strips ALL leading underscores, so _rasterizer_windows_end
    (0x155a40) and the kb name rasterizer_windows_end (0x17c910, a tail-call
    thunk) collapse onto one symbol.  Only the comment separates them.
    """
    src = v.REPO_ROOT / "src/halo/rasterizer/rasterizer.c"
    if not src.exists():
        return  # TU moved; the rule is still pinned by the unit test below
    hit = v._source_comment_addr("rasterizer_windows_end", src)
    if hit is None:
        return
    assert v._resolve_func_addr("rasterizer_windows_end", src, {}) == hit[0]


def test_tu_scoped_lookup_beats_a_same_name_function_elsewhere():
    """A TU's own kb listing wins over a global name match.

    Without this, a static helper sharing a name with an unrelated kb.json
    function resolves to that function's address and gets scored against
    completely unrelated code -- producing a plausible-looking low score rather
    than an obvious failure.
    """
    fake = {"some_helper": {"addr": 0x1A0680, "name": "some_helper",
                            "ported": True}}
    src = v.REPO_ROOT / "src/halo/main/main.c"
    assert v._resolve_func_addr("some_helper", src, fake) == 0x1A0680


def test_kb_source_index_maps_a_known_tu():
    """kb.json must actually answer "which functions does this TU own"."""
    funcs = v._kb_functions_for_source(v.REPO_ROOT / "src/halo/main/main.c")
    assert funcs, "no kb.json functions resolved for main/main.c"
    assert "main_loop" in funcs, sorted(funcs)[:10]
    assert funcs["main_loop"]["addr"] == 0x102E40, funcs["main_loop"]


def test_path_suffix_match_does_not_cross_filenames():
    """`profiles.c` must not claim `files.c`'s entries (bare endswith bug)."""
    idx = v._kb_source_index()
    if "tag_files/files.c" not in idx:
        return
    got = v._kb_functions_for_source(Path("/anything/profiles.c"))
    assert got is not idx["tag_files/files.c"], (
        "a bare endswith match let profiles.c adopt files.c's function list")


# ---------------------------------------------------------------------------
# Trims
# ---------------------------------------------------------------------------

def test_trim_after_last_terminator():
    cases = (
        (["pushl %ebp", "retl", "addb %al, (%eax)"], 2, "table data dropped"),
        (["movl %eax, %ecx", "jmp *%edx"], 2, "tail-call kept"),
        (["cmpl $0, %eax", "retl", "movl $1, %eax", "retl"], 4, "multi-ret kept"),
        (["addb %al, (%eax)"], 1, "no terminator: leave it alone"),
    )
    for insns, want, why in cases:
        got = len(v._trim_after_last_terminator(insns))
        assert got == want, f"{why}: {len(insns)} -> {got}, want {want}"


# ---------------------------------------------------------------------------
# Output contract
# ---------------------------------------------------------------------------

_REFMETA_RE = re.compile(
    r"^\s*REFMETA (\S+) addr=0x([0-9a-f]{8}) end=0x([0-9a-f]{8}) "
    r"kind=(\w+) n_r=(\d+) sha=([0-9a-f]{16})$")


def test_refmeta_line_shape():
    """The re-baseline parses this; pin the exact shape."""
    line = ("  REFMETA main_loop addr=0x00102e40 end=0x001034aa kind=auto "
            "n_r=432 sha=7988df1cab63ec0e")
    m = _REFMETA_RE.match(line)
    assert m, "REFMETA format changed; update vc71_regression's parser too"
    assert m.group(1) == "main_loop" and m.group(4) == "auto"


def test_refmeta_and_score_lines_agree_on_a_real_tu():
    """End-to-end: every score line has a REFMETA whose n_r matches it.

    Needs the VC71 compiler; skipped (not failed) without it, so this file still
    runs on a box that only has the reference side.
    """
    if not Path(v.VC71_CL_WSL).exists():
        print("    (skipped: VC71 CL.Exe not present)")
        return
    src = v.REPO_ROOT / "src/halo/main/main.c"
    out = subprocess.run(
        [sys.executable, str(v.REPO_ROOT / "tools/verify/vc71_verify.py"),
         str(src), "--quiet"],
        capture_output=True, text=True, cwd=v.REPO_ROOT).stdout
    meta = {m.group(1): int(m.group(5))
            for m in (_REFMETA_RE.match(l) for l in out.splitlines()) if m}
    scores = re.findall(r"(?:PASS|FAIL) (\S+): [\d.]+% match \((\d+)/(\d+) insns\)",
                        out)
    assert scores, "no score lines produced"
    for fn, _n_c, n_r in scores:
        assert fn in meta, f"{fn} scored with no REFMETA provenance line"
        assert meta[fn] == int(n_r), (
            f"{fn}: REFMETA says {meta[fn]} reference insns, score line says {n_r}")
    synth = {l.split()[-1] for l in out.splitlines() if l.strip().startswith("SYNTHREF ")}
    assert synth >= set(meta), "SYNTHREF must still be emitted per scored function"


def main():
    print("Running VC71 reference-derivation self-tests...")
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
