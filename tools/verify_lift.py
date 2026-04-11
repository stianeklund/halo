#!/usr/bin/env python3
"""
verify_lift.py — Structural equivalence check for Halo re-implementations.

Given Ghidra decompilation + callee data for the original function (in
cachebeta.xbe) and the lifted replacement (in default.xbe after patching),
produces a pass/fail report.

Input: a single JSON file with the pre-collected data (schema at bottom).
Output: human-readable verdict on stdout; exit 0 on PASS, 1 on FAIL.

The signals it checks, in order of weight:

  HARD  — struct offset set:   offsets read/written via (param+0xXX) must
          be identical between both decompiles. This catches wrong-offset
          mistakes, which are the highest-risk and most common lift bug.

  SOFT  — business-logic callees: after filtering out assertion helpers
          and system_exit thunks (our assert_halt lowers differently than
          the original's assertion plumbing), remaining callee *count and
          names* should match. Warn on divergence; don't fail.

  SOFT  — decompile shape: number of conditional branches in each. Gross
          mismatch suggests missed branches; similarity is only a sanity
          check.

  INFO  — string refs: string literals quoted in the decompile. Useful for
          eyeballing but not automatic; paths and assertion-expansion
          strings change legitimately.
"""
from __future__ import annotations

import json
import re
import sys
from dataclasses import dataclass, field


# Assertion-related functions we want to ignore when diffing callees.
# The original game calls its own display_assert / system_exit; our
# assert_halt macro lowers to display_assert + system_exit. Both sides
# vary in exact address / wrapping, so we strip by NAME pattern.
ASSERT_CALLEE_PATTERNS = [
    re.compile(r'thunk_FUN_'),           # thunks (patched addr differs)
    re.compile(r'^FUN_0008d9[cd]0$'),    # original's display_assert variants
    re.compile(r'^FUN_0008d9f0$'),       # original's display_assert
    re.compile(r'^FUN_00643050$'),       # our lifted display_assert thunk target
    re.compile(r'display_assert'),        # any named form
    re.compile(r'system_exit'),
    re.compile(r'csprintf'),              # assert format helpers
]


def is_assert_related(callee_name: str) -> bool:
    return any(p.search(callee_name) for p in ASSERT_CALLEE_PATTERNS)


@dataclass
class FunctionSide:
    label: str                 # "original" or "lifted"
    address: str
    decompile: str
    callees: list[str] = field(default_factory=list)

    def struct_offsets(self) -> set[int]:
        """
        Return the set of struct member offsets referenced via the
        Ghidra-style (param_N + <offset>) pattern. Both positive decimal
        and hex offsets are matched.
        """
        offsets: set[int] = set()
        for m in re.finditer(r'\(param_\d+\s*\+\s*(0x[0-9a-fA-F]+|\d+)\)', self.decompile):
            offsets.add(int(m.group(1), 0))
        # Also catch 'this' / local deref patterns like (param_1 + 0xXX)[N]
        return offsets

    def branch_count(self) -> int:
        """Count if/while keywords. Rough signal of control flow shape."""
        return len(re.findall(r'\b(if|while|for|do)\b', self.decompile))

    def non_assert_callees(self) -> list[str]:
        return [c for c in self.callees if not is_assert_related(c)]

    def strings(self) -> list[str]:
        """Pull string literals out of the decompile text, ignoring __FILE__."""
        out = []
        for m in re.finditer(r'"([^"]*)"', self.decompile):
            s = m.group(1)
            if 'halo' in s.lower() and ('.c' in s or '.cpp' in s or '.h' in s):
                continue  # __FILE__ paths legitimately differ between orig/lift
            out.append(s)
        return out


@dataclass
class VerifyReport:
    name: str
    orig_addr: str
    new_addr: str
    hard_failures: list[str] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)
    infos: list[str] = field(default_factory=list)

    @property
    def passed(self) -> bool:
        return not self.hard_failures

    def render(self) -> str:
        verdict = 'PASS' if self.passed else 'FAIL'
        lines = [f'=== verify_lift: {self.name}  [{verdict}] ===',
                 f'  original @ {self.orig_addr}  →  lifted @ {self.new_addr}']
        if self.hard_failures:
            lines.append('')
            lines.append('  HARD FAILURES:')
            for f_ in self.hard_failures:
                lines.append(f'    ✗ {f_}')
        if self.warnings:
            lines.append('')
            lines.append('  WARNINGS:')
            for w in self.warnings:
                lines.append(f'    ! {w}')
        if self.infos:
            lines.append('')
            lines.append('  INFO:')
            for i in self.infos:
                lines.append(f'    · {i}')
        return '\n'.join(lines)


def verify(orig: FunctionSide, new: FunctionSide, name: str) -> VerifyReport:
    rep = VerifyReport(name=name, orig_addr=orig.address, new_addr=new.address)

    # HARD: struct offset sets must match.
    o_offs, n_offs = orig.struct_offsets(), new.struct_offsets()
    missing = o_offs - n_offs
    extra = n_offs - o_offs
    if missing:
        rep.hard_failures.append(
            f'lifted code does not reference struct offsets present in original: '
            f'{{{", ".join(hex(x) for x in sorted(missing))}}}')
    if extra:
        rep.hard_failures.append(
            f'lifted code references struct offsets not in original: '
            f'{{{", ".join(hex(x) for x in sorted(extra))}}}')
    if not missing and not extra:
        rep.infos.append(
            f'struct offsets match ({len(o_offs)} distinct: '
            f'{{{", ".join(hex(x) for x in sorted(o_offs))}}})')

    # SOFT: non-assert callees.
    o_calls, n_calls = orig.non_assert_callees(), new.non_assert_callees()
    if sorted(o_calls) != sorted(n_calls):
        rep.warnings.append(
            f'business-logic callees differ (this may be OK if our lift '
            f'restructured assertions):\n        original: {o_calls}\n'
            f'        lifted:   {n_calls}')
    else:
        rep.infos.append(f'business-logic callees match ({len(o_calls)} shared)')

    # SOFT: branch shape.
    o_branches, n_branches = orig.branch_count(), new.branch_count()
    if o_branches != n_branches:
        rep.infos.append(
            f'branch count differs: original={o_branches}, lifted={n_branches} '
            f'(diff of ±1 per simplified assert is expected)')
    else:
        rep.infos.append(f'branch count matches ({o_branches})')

    # INFO: strings.
    o_strs = set(orig.strings())
    n_strs = set(new.strings())
    dropped = o_strs - n_strs
    added = n_strs - o_strs
    if dropped or added:
        rep.infos.append(
            f'strings changed — '
            f'dropped={len(dropped)} added={len(added)} '
            f'(expected when lowering dynamic-format asserts)')

    return rep


def load(path: str) -> VerifyReport:
    with open(path) as f:
        data = json.load(f)
    orig = FunctionSide(
        label='original',
        address=data['orig_address'],
        decompile=data['orig_decompile'],
        callees=data.get('orig_callees', []),
    )
    new = FunctionSide(
        label='lifted',
        address=data['new_address'],
        decompile=data['new_decompile'],
        callees=data.get('new_callees', []),
    )
    return verify(orig, new, data['name'])


def main():
    if len(sys.argv) != 2:
        print('usage: verify_lift.py <collected-data.json>', file=sys.stderr)
        sys.exit(2)
    rep = load(sys.argv[1])
    print(rep.render())
    sys.exit(0 if rep.passed else 1)


if __name__ == '__main__':
    main()


# JSON schema the caller produces:
#
#   {
#     "name": "data_verify",
#     "orig_address": "0x1193f0",
#     "new_address":  "0x644b30",
#     "orig_decompile": "<Ghidra decompile of cachebeta.xbe @ orig_address>",
#     "new_decompile":  "<Ghidra decompile of default.xbe  @ new_address>",
#     "orig_callees":   ["FUN_0008d9d0", "FUN_0008d9f0", "thunk_FUN_001029a0"],
#     "new_callees":    ["FUN_00643050", "thunk_FUN_001029a0"]
#   }
