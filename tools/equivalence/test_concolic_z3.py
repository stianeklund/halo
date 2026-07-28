"""Self-test for the Z3-backed concolic solver (Phase 3).

Each case pins one property AND its boundary, because the failure mode
that matters here is not "the solver is wrong" -- it is "the solver
quietly produces injections that look plausible and mean nothing".
"""

import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from concolic import disassemble_branches, find_uncovered, generate_memory_injections  # noqa: E402

try:
    import concolic_z3
    _HAVE = concolic_z3.available()
except ImportError:
    _HAVE = False


BASE = 0x1000
GLOBAL_OK = 0x00400100      # a normal global: injectable
GLOBAL_SPURIOUS = 0x00500100  # inside the DIR32 slot arena: never injectable


def _derived_compare(global_addr: int) -> bytes:
    """mov eax,[global]; add eax,7; cmp eax,0x2a; jne skip; <block>; skip: ret

    The compared value is the global PLUS SEVEN.  The old heuristic reads
    the CMP immediate and injects 0x2a, 0x29, 0x2b -- none of which flip
    this branch.  Only a solve gets 0x23.
    """
    return (
        b"\xa1" + struct.pack("<I", global_addr)  # 0x1000 mov eax,[global]
        + b"\x83\xc0\x07"                          # 0x1005 add eax, 7
        + b"\x83\xf8\x2a"                          # 0x1008 cmp eax, 0x2a
        + b"\x75\x03"                              # 0x100b jne 0x1010
        + b"\x33\xc0\x90"                          # 0x100d xor eax,eax; nop
        + b"\xc3"                                  # 0x1010 ret
    )


# The seed run read 0 from the global, so eax became 7, so the JNE was
# taken: everything except the fallthrough block was executed.
_VISITED = {0x1000: 5, 0x1005: 3, 0x1008: 3, 0x100b: 2, 0x1010: 1}


def _uncovered_for(code):
    branches = disassemble_branches(code, BASE)
    return find_uncovered(branches, _VISITED, BASE)


def _solve(code, global_addr, size=4, observed=0):
    return concolic_z3.solve_uncovered(
        code, BASE, _VISITED, _uncovered_for(code),
        {global_addr: (size, observed)})


def test_solves_a_derived_comparison():
    """The property: solve through arithmetic the heuristic cannot see."""
    code = _derived_compare(GLOBAL_OK)
    injections, stats = _solve(code, GLOBAL_OK)
    assert stats.sat == 1, stats.summary()
    assert len(injections) == 1
    assert injections[0] == {GLOBAL_OK: struct.pack("<I", 0x23)}


def test_heuristics_alone_do_not_find_it():
    """The boundary: without the solver this branch is genuinely missed.

    If this ever starts passing, the case has stopped exercising what it
    claims to and must be made harder.
    """
    code = _derived_compare(GLOBAL_OK)
    heuristic = generate_memory_injections(
        _uncovered_for(code), {GLOBAL_OK: (4, 0)}, BASE, use_z3=False)
    values = [inj.get(GLOBAL_OK) for inj in heuristic]
    assert struct.pack("<I", 0x23) not in values


def test_spurious_address_is_never_injected():
    """The DIR32 slot arena stays off-limits even when it would satisfy."""
    code = _derived_compare(GLOBAL_SPURIOUS)
    injections, stats = _solve(code, GLOBAL_SPURIOUS)
    assert injections == []
    assert stats.sat == 0


def test_register_gated_branch_yields_nothing():
    """A branch on an incoming register is not injectable -- say so.

    The dangerous outcome is an injection that flips an unrelated global
    just to satisfy "memory must differ", implying a causal link that is
    not there.
    """
    code = (
        b"\x83\xf9\x05"      # 0x1000 cmp ecx, 5
        + b"\x75\x03"        # 0x1003 jne 0x1008
        + b"\x33\xc0\x90"    # 0x1005 xor eax,eax; nop
        + b"\xc3"            # 0x1008 ret
    )
    visited = {0x1000: 3, 0x1003: 2, 0x1008: 1}
    branches = disassemble_branches(code, BASE)
    uncovered = find_uncovered(branches, visited, BASE)
    injections, stats = concolic_z3.solve_uncovered(
        code, BASE, visited, uncovered, {GLOBAL_OK: (4, 0)})
    assert injections == []
    assert stats.register_only == 1, stats.summary()


def test_unsatisfiable_branch_reports_unsat_not_a_guess():
    """An impossible condition must produce nothing, not a fallback value."""
    code = (
        b"\xa1" + struct.pack("<I", GLOBAL_OK)  # 0x1000 mov eax,[global]
        + b"\x83\xe0\x01"                        # 0x1005 and eax, 1
        + b"\x83\xf8\x02"                        # 0x1008 cmp eax, 2
        + b"\x74\x04"                            # 0x100b je 0x1011 (impossible)
        + b"\x33\xc0\x90\xc3"                    # 0x100d fallthrough block
        + b"\xc3"                                # 0x1011 never reached
    )
    visited = {0x1000: 5, 0x1005: 3, 0x1008: 3, 0x100b: 2,
               0x100d: 2, 0x100f: 1, 0x1010: 1}
    branches = disassemble_branches(code, BASE)
    uncovered = find_uncovered(branches, visited, BASE)
    injections, stats = concolic_z3.solve_uncovered(
        code, BASE, visited, uncovered, {GLOBAL_OK: (4, 0)})
    assert injections == []
    assert stats.unsat == 1, stats.summary()


def test_walk_steers_toward_the_target_branch():
    """The target branch sits behind an already-both-ways-visited branch.

    The visited set is a union over seeds, so branches with both directions
    seen are common.  Preferring the fallthrough at those walks straight
    into the `ret` at 0x100d and reports `path-not-reached`; only steering
    by static reachability gets to the branch at 0x1017.  This is the
    regression guard for that steering.
    """
    g1, g2 = 0x00400200, 0x00400300
    code = (
        b"\xa1" + struct.pack("<I", g1)   # 0x1000 mov eax,[g1]
        + b"\x83\xf8\x00"                  # 0x1005 cmp eax, 0
        + b"\x75\x05"                      # 0x1008 jne 0x100f
        + b"\x33\xc0"                      # 0x100a xor eax, eax
        + b"\x90"                          # 0x100c nop
        + b"\xc3"                          # 0x100d ret   <- the dead end
        + b"\x90"                          # 0x100e (padding)
        + b"\xa1" + struct.pack("<I", g2)  # 0x100f mov eax,[g2]
        + b"\x83\xf8\x07"                  # 0x1014 cmp eax, 7
        + b"\x74\x01"                      # 0x1017 je 0x101a
        + b"\xc3"                          # 0x1019 ret
        + b"\xc3"                          # 0x101a ret  <- never visited
    )
    visited = {a: 1 for a in (0x1000, 0x1005, 0x1008, 0x100a, 0x100c,
                              0x100d, 0x100f, 0x1014, 0x1017, 0x1019)}
    branches = disassemble_branches(code, BASE)
    uncovered = find_uncovered(branches, visited, BASE)
    assert [u.branch.address for u in uncovered] == [0x1017]

    injections, stats = concolic_z3.solve_uncovered(
        code, BASE, visited, uncovered, {g1: (4, 1), g2: (4, 0)})
    assert stats.sat == 1, stats.summary()
    assert injections == [{g2: struct.pack("<I", 7)}]


def test_every_attempt_is_accounted_for():
    """No attempt may fall through uncounted -- silence must not read as ok."""
    code = _derived_compare(GLOBAL_OK)
    uncovered = _uncovered_for(code)
    # A read the filter rejects outright: the only global on offer is in the
    # DIR32 arena, so the attempt must land in `no-injectable-globals`.
    _, stats = concolic_z3.solve_uncovered(
        code, BASE, _VISITED, uncovered, {GLOBAL_SPURIOUS: (4, 0)})
    assert stats.attempted == 1
    assert stats.no_globals == 1
    assert stats.accounted(), stats.summary()

    _, stats2 = _solve(code, GLOBAL_OK)
    assert stats2.accounted(), stats2.summary()


def test_wiring_is_additive():
    """Solved injections lead, heuristic injections still follow."""
    code = _derived_compare(GLOBAL_OK)
    uncovered = _uncovered_for(code)
    reads = {GLOBAL_OK: (4, 0)}
    with_z3 = generate_memory_injections(
        uncovered, reads, BASE, code=code, visited_pcs=_VISITED, use_z3=True)
    without = generate_memory_injections(
        uncovered, reads, BASE, use_z3=False)
    assert with_z3[0] == {GLOBAL_OK: struct.pack("<I", 0x23)}
    assert len(with_z3) > len(without)
    for inj in without:
        assert inj in with_z3


def test_missing_z3_degrades_to_heuristics():
    """The lane must never break a run when z3 is absent."""
    code = _derived_compare(GLOBAL_OK)
    uncovered = _uncovered_for(code)
    reads = {GLOBAL_OK: (4, 0)}
    saved = sys.modules.get("concolic_z3")
    sys.modules["concolic_z3"] = None  # import returns None -> AttributeError
    try:
        out = generate_memory_injections(
            uncovered, reads, BASE, code=code, visited_pcs=_VISITED)
    finally:
        if saved is None:
            sys.modules.pop("concolic_z3", None)
        else:
            sys.modules["concolic_z3"] = saved
    baseline = generate_memory_injections(uncovered, reads, BASE, use_z3=False)
    assert out == baseline


def main():
    tests = [v for k, v in sorted(globals().items()) if k.startswith("test_")]
    if not _HAVE:
        print("SKIP: z3 or capstone unavailable")
        return 0
    failed = 0
    for fn in tests:
        try:
            fn()
            print(f"  PASS {fn.__name__}")
        except AssertionError as exc:
            failed += 1
            print(f"  FAIL {fn.__name__}: {exc}")
        except Exception as exc:
            failed += 1
            print(f"  ERROR {fn.__name__}: {type(exc).__name__}: {exc}")
    print(f"\n{len(tests) - failed}/{len(tests)} passed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
