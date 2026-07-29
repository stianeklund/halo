#!/usr/bin/env python3
"""Self-tests for the stub-convention gate (lift-learnings §30).

A kb.json decl that disagrees with the callee's binary RET makes the synthetic
stub pop the wrong number of bytes.  Both oracle and candidate honor the same
wrong decl, so the differential passes while the box drifts ESP -- the 0x158df0
boot crash.  The gate therefore has to fail the run.

But it must fail only when a synthetic stub honoring that decl is actually
EXECUTED.  Registration happens before _load_real_callees and covers every
callee in the stub map regardless of reachability, so the raw mismatch list
over-reports on two counts that cannot drift anything:

  * a callee whose real oracle bytes are loaded (its own RET N pops correctly,
    the decl is never consulted), and
  * a callee no seed ever calls.

Measured before this split: 64 batch targets errored as
`stub_convention_mismatch` on just 15 distinct unported CRT decls, none of them
reached from lifted C.  bink_playback_stop was blocked by FUN_000e5590 while
reporting 0 stub calls on the covered path, and passed 20/20 once unblocked.

These tests pin BOTH directions.  The relaxation is only sound while the
executed-and-synthetic case still fails, so that case is pinned first.
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from stubs import StubManager, CalleeStub


_SENT_A = 0x00F00000
_SENT_B = 0x00F00010


def _mgr():
    """A StubManager with no I/O performed.

    blocking_convention_mismatches() reads only _conv_mismatches, _stubs and
    _executed_stubs, so the constructor's paths are never exercised -- but we
    build the real class so a constructor/attribute rename breaks this test
    instead of silently passing against a stale fake.
    """
    return StubManager(kb_path=Path("kb.json"), delinked_dir=Path("delinked"))


def _stub(sentinel, has_real_code):
    return CalleeStub(name=f"callee_{sentinel:#x}", code=b"", abi={},
                      sentinel_addr=sentinel, has_real_code=has_real_code)


def _mismatch(mgr, sentinel, has_real_code=False, executed=False):
    mgr._stubs[sentinel] = _stub(sentinel, has_real_code)
    mgr._conv_mismatches.append({
        "sentinel": sentinel,
        "addr": 0x1D0C48,
        "name": f"callee_{sentinel:#x}",
        "msg": f"callee_{sentinel:#x} @ 0x1d0c48: decl implies RET 0 but the "
               f"binary RETs 8",
    })
    mgr.convention_mismatches.append("raw")
    if executed:
        mgr._executed_stubs.add(sentinel)


def test_executed_synthetic_stub_is_blocking():
    """The §30 case itself: this must keep failing the run.

    If this test ever passes vacuously the gate is gone and a wrong decl ships
    a silent ESP drift.
    """
    mgr = _mgr()
    _mismatch(mgr, _SENT_A, has_real_code=False, executed=True)
    blocking = mgr.blocking_convention_mismatches()
    assert len(blocking) == 1, f"executed synthetic stub must block: {blocking}"
    print("  PASS  test_executed_synthetic_stub_is_blocking")


def test_real_code_callee_is_not_blocking():
    """get_stub_code returns the callee's own bytes; the decl is never read."""
    mgr = _mgr()
    _mismatch(mgr, _SENT_A, has_real_code=True, executed=True)
    blocking = mgr.blocking_convention_mismatches()
    assert blocking == [], f"real-code callee cannot drift ESP: {blocking}"
    print("  PASS  test_real_code_callee_is_not_blocking")


def test_unexecuted_stub_is_not_blocking():
    """No seed called it, so no stub ran -- the bink_playback_stop case."""
    mgr = _mgr()
    _mismatch(mgr, _SENT_A, has_real_code=False, executed=False)
    blocking = mgr.blocking_convention_mismatches()
    assert blocking == [], f"uncalled stub cannot drift ESP: {blocking}"
    print("  PASS  test_unexecuted_stub_is_not_blocking")


def test_mismatch_is_still_reported_when_not_blocking():
    """Downgraded to a warning, never dropped.

    A wrong decl stays a real kb.json defect even when this run cannot trip on
    it; check_stdcall_ret.py is what fixes it, and it needs the finding to
    remain visible.
    """
    mgr = _mgr()
    _mismatch(mgr, _SENT_A, has_real_code=True, executed=True)
    _mismatch(mgr, _SENT_B, has_real_code=False, executed=False)
    assert mgr.blocking_convention_mismatches() == []
    assert len(mgr.convention_mismatches) == 2, \
        "non-blocking mismatches must still be reported"
    print("  PASS  test_mismatch_is_still_reported_when_not_blocking")


def test_one_blocking_among_excused_still_fails():
    """A genuine mismatch is not masked by excused ones sharing the run."""
    mgr = _mgr()
    _mismatch(mgr, _SENT_A, has_real_code=True, executed=True)   # excused
    _mismatch(mgr, _SENT_B, has_real_code=False, executed=True)  # blocking
    blocking = mgr.blocking_convention_mismatches()
    assert len(blocking) == 1, f"expected exactly the real one: {blocking}"
    assert f"{_SENT_B:#x}" in blocking[0], blocking[0]
    print("  PASS  test_one_blocking_among_excused_still_fails")


def test_unknown_sentinel_is_not_silently_excused():
    """A mismatch whose stub is absent from _stubs must not be dropped.

    Only has_real_code excuses a stub; a missing entry means we do not know it
    ran real code, so if it executed it still blocks.  Excusing on absence
    would forgive exactly the case we cannot reason about.
    """
    mgr = _mgr()
    mgr._conv_mismatches.append({"sentinel": _SENT_A, "addr": 0x1D0C48,
                                 "name": "gone", "msg": "gone: RET 8"})
    mgr._executed_stubs.add(_SENT_A)          # executed, but no _stubs entry
    blocking = mgr.blocking_convention_mismatches()
    assert len(blocking) == 1, f"unknown-but-executed must block: {blocking}"
    print("  PASS  test_unknown_sentinel_is_not_silently_excused")


def test_no_mismatches_is_clean():
    mgr = _mgr()
    assert mgr.blocking_convention_mismatches() == []
    print("  PASS  test_no_mismatches_is_clean")


def main():
    print("Running stub-convention gate self-tests...")
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
