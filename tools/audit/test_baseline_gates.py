#!/usr/bin/env python3
"""Self-test for the baseline-diff logic of the audit gates.

The failure mode that matters for a baselined gate is not "it reports a
wrong finding" -- it is "it silently forgives one it should have raised".
A baseline keyed on the address alone would do exactly that: the finding
changes shape, the address still matches, the gate stays quiet.

So every case here pins BOTH directions: a baselined finding must not
fire, and a CHANGED finding at a baselined address must.

Run: python3 tools/audit/test_baseline_gates.py
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import check_arg_counts as ac          # noqa: E402
import check_callee_reg_args as cra    # noqa: E402


class _Result:
    """Minimal stand-in for CalleeResult (only the keyed fields matter)."""

    def __init__(self, addr, declared_stack, verdict, observed):
        self.addr = addr
        self.addr_str = "0x%06x" % addr
        self.name = "f"
        self.declared_stack = declared_stack
        self.verdict = verdict
        self.observed = observed


def test_arg_counts_baselined_finding_is_quiet():
    bl = {"0x001234": {"name": "f", "declared_stack": 0,
                       "verdict": "UNDER-DECLARED", "observed": {"1": 1}}}
    assert ac._new_highs([_Result(0x1234, 0, "UNDER-DECLARED", {1: 1})], bl) == []


def test_arg_counts_changed_finding_refires():
    """Same address, different evidence -> must NOT stay forgiven."""
    bl = {"0x001234": {"name": "f", "declared_stack": 0,
                       "verdict": "UNDER-DECLARED", "observed": {"1": 1}}}
    for changed in (
        _Result(0x1234, 0, "UNDER-DECLARED", {2: 1}),   # observed cleanup moved
        _Result(0x1234, 1, "UNDER-DECLARED", {1: 1}),   # decl changed
        _Result(0x1234, 0, "OVER-DECLARED", {1: 1}),    # verdict flipped
    ):
        assert len(ac._new_highs([changed], bl)) == 1, changed.verdict


def test_arg_counts_new_address_fires():
    bl = {"0x001234": {"name": "f", "declared_stack": 0,
                       "verdict": "UNDER-DECLARED", "observed": {"1": 1}}}
    assert len(ac._new_highs([_Result(0x5555, 0, "UNDER-DECLARED", {1: 1})], bl)) == 1


def test_callee_reg_baselined_finding_is_quiet():
    bl = {"0x001234": {"name": "f", "inputs": ["eax"]}}
    assert cra._new_findings([{"addr": 0x1234, "name": "f", "inputs": ["eax"]}], bl) == []


def test_callee_reg_grown_regset_refires():
    """A callee that gains a register input is a new hazard, not the old one."""
    bl = {"0x001234": {"name": "f", "inputs": ["eax"]}}
    grown = {"addr": 0x1234, "name": "f", "inputs": ["eax", "ecx"]}
    assert cra._new_findings([grown], bl) == [grown]


def test_callee_reg_new_address_fires():
    bl = {"0x001234": {"name": "f", "inputs": ["eax"]}}
    new = {"addr": 0x9999, "name": "g", "inputs": ["edx"]}
    assert cra._new_findings([new], bl) == [new]


def test_callee_reg_regset_order_does_not_matter():
    """Baselines store sorted regs; an unsorted finding must still match."""
    bl = {"0x001234": {"name": "f", "inputs": ["eax", "ecx"]}}
    assert cra._new_findings(
        [{"addr": 0x1234, "name": "f", "inputs": ["ecx", "eax"]}], bl) == []


def main():
    tests = [v for k, v in sorted(globals().items()) if k.startswith("test_")]
    failed = 0
    for fn in tests:
        try:
            fn()
            print(f"  PASS {fn.__name__}")
        except AssertionError as exc:
            failed += 1
            print(f"  FAIL {fn.__name__}: {exc}")
    print(f"\n{len(tests) - failed}/{len(tests)} passed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
