#!/usr/bin/env python3
"""Self-test for the strict-lift soundness gate (Phase 0.1).

Run with:
    python3 tools/equivalence/test_z3_strict_lift.py

Background
----------
x86_to_z3 used to record an instruction it could not model into
`state.unsupported_insns` and then *skip* it. z3_equiv.prove_equivalence
never inspected that list, so the dropped semantics were simply absent from
both formulas. Two functions that differ only in an unmodelled instruction
therefore lifted to identical expressions, the solver returned UNSAT, and the
function was recorded `z3_proven` -- a verdict that (per docs/z3-equivalence.md)
supersedes VC71 and, before Phase 0.2, also caused the Unicorn sweep to be
skipped entirely. The strongest-looking evidence sat on the least-tested code.

Tests
-----
  1. strict=True makes the lifter REFUSE an unmodelled instruction (LiftError).
  2. strict=False (seed generation) still records-and-skips, as before.
  3. prove_equivalence reports not_applicable -- never proven -- for the exact
     false-proof shape: two functions that are equivalent ONLY if you delete
     the instruction the lifter cannot model.
  4. A genuinely liftable equivalent pair is still proven (strict mode did not
     break the normal proof path).
  5. A genuinely liftable NON-equivalent pair is still refuted.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(__file__))

try:
    import z3  # noqa: F401
    from x86_to_z3 import X86State, X86Lifter, LiftError
    from z3_equiv import prove_equivalence
    from abi import parse_decl
    _AVAILABLE = True
except ImportError as e:
    print(f"SKIP: z3/capstone not importable ({e})")
    _AVAILABLE = False

# mov eax, 1 ; cpuid ; ret
#   CPUID is not modelled by the lifter AND it clobbers EAX, so deleting it
#   changes the result. This is the false-proof shape.
ORACLE_CPUID = bytes([0xB8, 0x01, 0x00, 0x00, 0x00, 0x0F, 0xA2, 0xC3])
# mov eax, 1 ; ret
CAND_NO_CPUID = bytes([0xB8, 0x01, 0x00, 0x00, 0x00, 0xC3])

# mov eax, 5 ; ret   (fully modelled)
MOV5_RET = bytes([0xB8, 0x05, 0x00, 0x00, 0x00, 0xC3])
# mov eax, 6 ; ret
MOV6_RET = bytes([0xB8, 0x06, 0x00, 0x00, 0x00, 0xC3])


def test_strict_raises():
    lifter = X86Lifter(X86State("t_"), code_base=0, strict=True)
    try:
        lifter.lift_function(ORACLE_CPUID, address=0)
    except LiftError as e:
        assert "cpuid" in str(e).lower(), e
        print("  [1] strict=True refuses unmodelled instruction         OK")
        return
    raise AssertionError("strict lifter accepted cpuid instead of raising")


def test_permissive_records():
    state = X86State("t_")
    lifter = X86Lifter(state, code_base=0, strict=False)
    lifter.lift_function(ORACLE_CPUID, address=0)
    assert state.unsupported_insns, "permissive lifter recorded nothing"
    assert "cpuid" in state.unsupported_insns[0].lower(), state.unsupported_insns
    print("  [2] strict=False still records-and-skips (seed path)     OK")


def test_false_proof_refused():
    """The regression this whole phase exists for."""
    abi = parse_decl("int f(void)")
    r = prove_equivalence(ORACLE_CPUID, CAND_NO_CPUID, abi)
    assert not r.proven, (
        "FALSE PROOF: functions differing by an unmodelled instruction "
        f"were proven equivalent (reason={r.reason})")
    assert r.not_applicable, f"expected not_applicable, got {r}"
    assert "liftable" in r.reason or "unsupported" in r.reason, r.reason
    print("  [3] false-proof shape reported not_applicable            OK")


def test_real_proof_still_works():
    abi = parse_decl("int f(void)")
    r = prove_equivalence(MOV5_RET, MOV5_RET, abi)
    assert r.proven, f"strict mode broke a valid proof: {r.reason}"
    print("  [4] genuinely equivalent pair still proven               OK")


def test_real_divergence_still_caught():
    abi = parse_decl("int f(void)")
    r = prove_equivalence(MOV5_RET, MOV6_RET, abi)
    assert not r.proven, "non-equivalent pair was proven equivalent"
    assert not r.not_applicable, f"divergence misreported as N/A: {r.reason}"
    print("  [5] genuinely divergent pair still refuted               OK")


def main():
    if not _AVAILABLE:
        return 0
    print("test_z3_strict_lift:")
    test_strict_raises()
    test_permissive_records()
    test_false_proof_refused()
    test_real_proof_still_works()
    test_real_divergence_still_caught()
    print("PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
