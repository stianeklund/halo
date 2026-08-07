#!/usr/bin/env python3
import sys, os
_tools_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)

"""Compare compiled object against delinked reference at instruction level.

Uses basic-block-aware matching: extracts instruction mnemonics per basic block
(split at labels/branches), computes LCS-based similarity, and flags FPU
operand-order differences that indicate potential cross-product or subtraction
argument swaps.

Usage:
    python3 tools/verify/compare_obj.py <compiled.obj> <reference.obj> [--function FUN_XXXXXXXX]
    python3 tools/verify/compare_obj.py <compiled.obj> <reference.obj> --show-diffs
"""

import argparse
import re
import subprocess
import sys
from collections import OrderedDict
from difflib import SequenceMatcher


def disassemble(obj_path: str) -> dict[str, list[str]]:
    """Disassemble object, return {symbol: [instruction_lines]} for FUN_ symbols."""
    result = subprocess.run(
        ["llvm-objdump", "-d", "--no-show-raw-insn", "--no-leading-addr", obj_path],
        capture_output=True, text=True
    )
    if result.returncode != 0:
        print(f"llvm-objdump failed on {obj_path}: {result.stderr}", file=sys.stderr)
        sys.exit(1)

    functions: dict[str, list[str]] = OrderedDict()
    _label_positions: dict[str, set[int]] = {}
    current_func = None
    current_lines: list[str] = []
    current_labels: set[int] = set()

    for raw_line in result.stdout.splitlines():
        line = raw_line.rstrip()
        m = re.match(r'^(?:[0-9a-f]+ )?<([^>]+)>:', line)
        if m:
            sym = m.group(1)
            # Any $-prefixed symbol is a VC71 local label ($L33383, $caseD_...,
            # and user-goto labels like $not_allowed$33397), never a function.
            if sym.startswith("LAB_") or sym.startswith("switchD_") or sym.startswith("$"):
                current_labels.add(len(current_lines))
                continue
            if sym.startswith("FUN_") and current_func and current_lines:
                # Look back past trailing padding to the last real instruction.
                # MSVC pads between functions with nop/int3, so inspecting only
                # current_lines[-1] sees the padding rather than the terminating
                # ret — which mis-folds the *next* function into this one and
                # massively inflates its instruction count (e.g. csmemset read as
                # 298 insns instead of ~80).
                #
                # Fold a FUN_ label into the current function ONLY when real code
                # flows straight into it: the last real instruction is not a ret
                # AND there is no inter-function padding before the label.  A
                # genuine next function is preceded either by a ret or by
                # alignment padding — and MSVC pads (nop/int3) only *between*
                # functions, never mid-body — so padding before a FUN_ symbol
                # marks a real boundary even when the predecessor ends in a
                # tail-call jmp rather than a ret (which would otherwise fold the
                # successor away and drop it from the reference entirely).
                last_mnem = ""
                had_padding = False
                for _prev in reversed(current_lines):
                    _mn = mnemonic(_prev).lower()
                    if _mn in _PAD_MNEMS:
                        had_padding = True
                        continue
                    last_mnem = _mn
                    break
                if last_mnem and last_mnem not in _RET_MNEMS and not had_padding:
                    current_labels.add(len(current_lines))
                    continue
            if current_func and current_lines:
                functions[current_func] = current_lines
                _label_positions[current_func] = current_labels
            current_func = re.sub(r'@\d+$', '', sym.lstrip("_@"))
            current_lines = []
            current_labels = set()
            continue

        stripped = line.strip()
        if not stripped or stripped.startswith("...") or stripped.startswith("Disassembly of section"):
            continue
        # Extract just the mnemonic + operands (skip leading address)
        parts = stripped.split(None, 1)
        if len(parts) >= 1:
            insn = stripped
            # Remove leading hex address if present
            if re.match(r'^[0-9a-f]+:', parts[0]):
                insn = parts[1] if len(parts) > 1 else ""
            if insn:
                current_lines.append(insn)

    if current_func and current_lines:
        functions[current_func] = current_lines
        _label_positions[current_func] = current_labels

    for fn in functions:
        lines = functions[fn]
        while lines and mnemonic(lines[-1]).lower() in _PAD_MNEMS:
            lines.pop()
        lines = _trim_trailing_table_data(lines)
        lines = _trim_trailing_thunks(lines)
        functions[fn] = _trim_unlabeled_bleed(lines, _label_positions.get(fn, set()))

    return functions


def first_function_insns(obj_path: str, aliases) -> list[str] | None:
    """Instructions of the first matching function in a per-function chunk,
    capped at the first genuine *neighbour* boundary (not at internal control
    flow).

    A per-function chunk (delinked/functions/<addr>.obj) is meant to hold a
    single function.  Stale pre-fix (0x2000-byte-window) exports instead packed
    the following functions into the same .text section, labelling them
    LAB_/switchD_ (jump-target style) rather than FUN_ — often as ret+nop
    relocation-stub slots.  disassemble() treats LAB_ as within-function, which
    is correct for whole objects but here swallows those neighbours and inflates
    the symbol (e.g. a real 6-insn function read as 102).

    Boundary rule: a following FUN_/thunk_ symbol is always a hard boundary.  A
    jump-target label (LAB_/switchD_/$L/$case/$next) ends the function only when
    the last real instruction was a `ret` AND the label is NOT internal control
    flow — i.e. it is neither a branch target referenced earlier in the body nor
    a case/default sub-label of a switch we already fell into.  This keeps the
    stale-neighbour protection while no longer truncating multi-return functions
    (each `return;` is a mid-body `ret` followed by the alternate-path label) or
    switch dispatch tables (each `case: return;` is a `ret` before the next
    caseD_ label, which is reached only through the indirect jump table).
    Returns the padding-trimmed instruction list, or None if the symbol is
    absent.
    """
    result = subprocess.run(
        ["llvm-objdump", "-d", "--no-show-raw-insn", "--no-leading-addr", obj_path],
        capture_output=True, text=True
    )
    if result.returncode != 0:
        return None
    return _first_function_insns_from_text(result.stdout, aliases)


def count_bounded_insns(obj_path: str, aliases) -> int | None:
    """Count instructions from the target FUN_ symbol to the next FUN_ symbol,
    stripping trailing NOP/INT3 padding.  This is the ground-truth instruction
    count for the function in the .obj, independent of first_function_insns's
    boundary heuristics.  Returns None if the symbol is absent."""
    result = subprocess.run(
        ["llvm-objdump", "-d", "--no-show-raw-insn", "--no-leading-addr", obj_path],
        capture_output=True, text=True
    )
    if result.returncode != 0:
        return None
    want = set(aliases)
    in_target = False
    found = False
    insns = []
    for line in result.stdout.splitlines():
        m = re.match(r'^(?:[0-9a-f]+ )?<([^>]+)>:', line)
        if m:
            sym = re.sub(r'@\d+$', '', m.group(1).lstrip("_@"))
            if not in_target:
                if sym in want:
                    in_target = True
                    found = True
                continue
            if sym.startswith("FUN_") or sym.startswith("thunk_"):
                break
            continue
        if not in_target:
            continue
        stripped = line.strip()
        if (not stripped or stripped.startswith("...")
                or stripped.startswith("Disassembly of section")):
            continue
        parts = stripped.split(None, 1)
        if parts and re.match(r'^[0-9a-f]+:', parts[0]):
            insns.append(parts[1] if len(parts) > 1 else "")
        elif stripped:
            insns.append(stripped)
    if not found:
        return None
    _pad = {'nop', 'nopl', 'nopw', 'int3'}
    while insns and mnemonic(insns[-1]).lower() in _pad:
        insns.pop()
    return len(insns)


def _parse_objdump_insn(line: str) -> str | None:
    """Return the instruction text of an llvm-objdump body line, else None.

    Shared by the main scan and _has_back_edge_into so both strip the optional
    `<hex>:` address prefix and skip banner/ellipsis lines identically."""
    stripped = line.strip()
    if (not stripped or stripped.startswith("...")
            or stripped.startswith("Disassembly of section")):
        return None
    parts = stripped.split(None, 1)
    insn = stripped
    if parts and re.match(r'^[0-9a-f]+:', parts[0]):
        insn = parts[1] if len(parts) > 1 else ""
    return insn or None


def _has_back_edge_into(lines: list[str], start_idx: int, defined: set[str]) -> bool:
    """True if code at/after `start_idx` branches to a label defined before it.

    A genuine *neighbour* function never branches back into ours, so a back
    edge across a candidate boundary proves the two sides are one function.
    This is the naming-independent backstop for the `seen_switches` exemption:
    Ghidra sometimes names a switch's default arm plain `LAB_<addr>` instead of
    `switchD_<addr>::default`, so the sub-label check cannot recognise it and
    the ret+label rule cuts the function at its first mid-body `ret`.

    Observed on parse_string (0x19be30, draw_string.obj): the first `ret` sits
    at chunk offset 0x145 with LAB_0019bf76 at 0x146, and the arms at
    0x19c033/03d/047/051/058 all `jmp LAB_0019bf60` (offset 0x130) — before the
    boundary. The reference was truncated to 91 of 163 instructions, scoring a
    197-insn candidate at 50.7% and parking it 7 times.

    Scanning stops at the first non-jump-target symbol: that is a hard
    neighbour boundary, and anything past it cannot vouch for our body.
    """
    for raw in lines[start_idx:]:
        line = raw.rstrip()
        m = re.match(r'^(?:[0-9a-f]+ )?<([^>]+)>:', line)
        if m:
            sym = re.sub(r'@\d+$', '', m.group(1).lstrip("_@"))
            if not sym.startswith(("LAB_", "switchD_", "$")):
                return False
            continue
        insn = _parse_objdump_insn(line)
        if insn is None:
            continue
        for ref in re.findall(r'<([^>]+)>', insn):
            if re.sub(r'@\d+$', '', ref.lstrip("_@")) in defined:
                return True
    return False


def _first_function_insns_from_text(stdout: str, aliases) -> list[str] | None:
    """Pure-text core of first_function_insns (see its docstring). Parses
    `llvm-objdump -d --no-show-raw-insn --no-leading-addr` output. Split out so
    the neighbour-vs-internal boundary logic is unit-testable without an .obj."""
    want = set(aliases)
    insns: list[str] = []
    in_target = False
    found = False
    # Labels targeted by a branch/jump seen so far in this function.  An early-
    # return `ret` inside a multi-return function is followed by the alternate-
    # path block's label (e.g. the `je <LAB_x>` target): that label is INTERNAL
    # code, not a neighbour slot, so it must not end the function.  A genuine
    # neighbour stub's entry label is never referenced by one of our jumps.
    referenced: set[str] = set()
    # Switch ids (switchD_<addr>) whose dispatch we have already fallen into.
    # Every caseD_/default sub-label of an entered switch is internal even when
    # the previous case body ends in `ret` (each `case:` `return;`s and the next
    # case is reached only through the indirect jump table, never a direct
    # branch — so `referenced` cannot see it).  A *neighbour's* switch appears
    # with a new id after our terminating ret, so it still hard-breaks.
    seen_switches: set[str] = set()
    # Every symbol defined at or after our entry point, for the back-edge test:
    # a branch from past a candidate boundary to one of these proves the code
    # after the boundary is still ours.  Includes the entry aliases so a loop
    # back to the function head counts.
    defined: set[str] = set(want)

    lines = stdout.splitlines()
    for line_idx, raw_line in enumerate(lines):
        line = raw_line.rstrip()
        m = re.match(r'^(?:[0-9a-f]+ )?<([^>]+)>:', line)
        if m:
            sym = re.sub(r'@\d+$', '', m.group(1).lstrip("_@"))
            if not in_target:
                in_target = sym in want
                found = found or in_target
                continue
            defined.add(sym)
            # Inside the target function.  A real next symbol (FUN_/thunk_/any
            # non-jump-target) is a hard boundary — stop (handles jmp-terminated
            # trampolines whose successor carries a proper symbol).  A jump-target
            # label (LAB_/switchD_/$L/$case/$next) is only a boundary when the
            # last real instruction was a ret AND the label is not an internal
            # branch target of this function; otherwise real code flows into it
            # (early-return ret + alternate-path block, or fall-through target).
            is_label = sym.startswith(("LAB_", "switchD_", "$"))
            if not is_label:
                break
            sw = re.match(r'switchD_([0-9a-f]+)::', sym)
            swid = sw.group(1) if sw else None
            if swid and swid in seen_switches:
                continue
            if sym in referenced:
                if swid:
                    seen_switches.add(swid)
                continue
            last = ""
            for prev in reversed(insns):
                mn = mnemonic(prev).lower()
                if mn in _PAD_MNEMS:
                    continue
                last = mn
                break
            if last in _RET_MNEMS:
                # Last real insn was a `ret` and this label is not a known
                # internal target — normally a neighbour slot.  Before cutting,
                # check for a back edge from past here into our body: if one
                # exists this is internal control flow whose label Ghidra just
                # did not mark as a switch sub-label.  See _has_back_edge_into.
                # `defined` minus this label: only a branch to code defined
                # STRICTLY BEFORE the boundary is evidence.  A jump to the
                # boundary label itself proves nothing — a neighbour's entry
                # label is naturally branched to from inside the neighbour, so
                # counting it would accept any looping neighbour as ours (and
                # each wrong continue grows `defined`, feeding the next).
                if _has_back_edge_into(lines, line_idx + 1, defined - {sym}):
                    if swid:
                        seen_switches.add(swid)
                    continue
                break
            if swid:
                seen_switches.add(swid)
            continue

        if not in_target:
            continue
        insn = _parse_objdump_insn(line)
        if insn:
            insns.append(insn)
            # Record any label this instruction jumps to (operand `<name>`), so a
            # later ret+label boundary check can tell internal targets from
            # neighbour slots.
            for ref in re.findall(r'<([^>]+)>', insn):
                referenced.add(re.sub(r'@\d+$', '', ref.lstrip("_@")))

    if not found:
        return None
    while insns and mnemonic(insns[-1]).lower() in _PAD_MNEMS:
        insns.pop()
    # Mirror the whole-object disassemble() path (see its trims below): strip
    # inline switch-table data — the jump/byte tables MSVC emits in .text after
    # the function's final RET, which llvm-objdump decodes as garbage insns
    # (addb %al,(%eax) etc.).  The per-function chunk path previously skipped
    # this, counting ~20 phantom "instructions" per jump-table function and
    # tanking the score (e.g. FUN_001a88b0 read 31 insns vs a real 10).
    # `defined` lets the trim tell a post-RET shared-epilogue/default arm from
    # inline table data.  Only this per-function-chunk path supplies it; the
    # whole-object path in disassemble() tracks label *positions*, not names,
    # and is left on the previous behaviour to keep this fix's blast radius to
    # the path that was actually wrong.
    insns = _trim_trailing_table_data(insns, defined)
    return insns or None


_RET_MNEMS = {'ret', 'retl', 'retw', 'retq', 'retn'}
_NOP_MNEMS = {'nop', 'nopl', 'nopw'}
# Inter-function padding emitted by MSVC (0x90 nop / 0xCC int3).  Never part of a
# function body; skipped when locating a function's terminating instruction and
# trimmed from function tails.
_PAD_MNEMS = _NOP_MNEMS | {'int3'}
_THUNK_BODY_MNEMS = {'push', 'pushl', 'pushw', 'call', 'calll', 'callw',
                     'add', 'addl', 'nop', 'pop', 'popl'}


def _trim_unlabeled_bleed(insns: list, label_positions: set[int]) -> list:
    """Trim unlabeled trailing code that bleeds into a function's parsed output.

    When an unlabeled helper sits between two named symbols, the parser lumps
    it under the preceding symbol.  Two-pass approach:
    1. Backward scan for RET + 3+ NOPs (safe for all functions — 3+ NOPs
       never appear inside a function body).
    2. If pass 1 didn't trim: forward scan for RET + 2+ NOPs, but only when
       the trailing code is > 40% of the total.  This catches 2-NOP bleed
       (e.g., small 6-insn functions followed by a helper) while avoiding
       over-trimming large functions with out-of-line blocks padded with 2 NOPs.
    """
    if not insns:
        return insns
    n = len(insns)

    # Pass 1: backward scan, 3+ NOPs (original safe heuristic)
    for i in range(n - 1, -1, -1):
        if mnemonic(insns[i]).lower() not in _RET_MNEMS:
            continue

        j = i + 1
        nop_count = 0
        while j < n and mnemonic(insns[j]).lower() in _NOP_MNEMS:
            nop_count += 1
            j += 1

        if nop_count < 3:
            continue

        if j >= n:
            continue

        has_label_in_sled = any(pos > i and pos <= j for pos in label_positions)
        if has_label_in_sled:
            continue

        return insns[:i + 1]

    # Pass 2: forward scan, 2+ NOPs — only if trailing code is > 40% of total
    for i in range(n):
        if mnemonic(insns[i]).lower() not in _RET_MNEMS:
            continue

        j = i + 1
        nop_count = 0
        while j < n and mnemonic(insns[j]).lower() in _NOP_MNEMS:
            nop_count += 1
            j += 1

        if nop_count < 2:
            continue

        if j >= n:
            continue

        trailing_count = n - j
        if trailing_count <= n * 4 // 10:
            continue

        has_label_in_sled = any(pos > i and pos <= j for pos in label_positions)
        if has_label_in_sled:
            continue

        return insns[:i + 1]

    return insns

_DATA_GARBAGE_MARKERS = ('<unknown>', 'addb\t%al, (%eax)', 'addb %al, (%eax)')

# Scaled indirect jump through a switch jump table: `jmp *0x0(,%edx,4)`,
# `jmp *0x100(,%eax,4)`, etc.  AT&T form, 4-byte scale (dword table entries).
# Presence of this instruction means MSVC emitted the table in .text right
# after the function's final RET -- see _trim_trailing_table_data.
_INDIRECT_TABLE_JMP_RE = re.compile(r'\bjmp\w*\s+\*[^(]*\(\s*,\s*%\w+\s*,\s*4\s*\)')


def _tail_branches_into_body(tail: list, body_label_names: set) -> bool:
    """True if any tail instruction branches to a label defined in the body.

    Distinguishes a real post-RET code block (shared epilogue, switch default
    arm) from inline table data: table bytes decode to garbage arithmetic, not
    to a symbolic branch back into the function we are measuring."""
    for ins in tail:
        for ref in re.findall(r'<([^>]+)>', ins):
            if re.sub(r'@\d+$', '', ref.lstrip("_@")) in body_label_names:
                return True
    return False


def _trim_trailing_table_data(insns: list, body_label_names: set | None = None) -> list:
    """Trim inline switch-table data disassembled as garbage after the final RET.

    MSVC places jump tables and byte index tables in .text after the
    function's final RET.  llvm-objdump decodes those bytes as junk
    instructions (addb %al,(%eax) for 00 00 pairs, <unknown>, etc.),
    inflating the instruction count and tanking the match score for every
    function containing a switch jump table.  Detect the artifact by
    scanning the tail after the LAST ret: if it contains a classic
    data-decode marker, everything after that ret is table data.
    """
    if not insns:
        return insns
    last_ret = -1
    for i, ins in enumerate(insns):
        if mnemonic(ins).lower() in _RET_MNEMS:
            last_ret = i
    if last_ret < 0 or last_ret == len(insns) - 1:
        return insns
    tail = insns[last_ret + 1:]
    # Real code can follow the final RET: a multi-exit function whose arms all
    # `jmp` back to a shared epilogue leaves that epilogue's RET as the last
    # one, with the arms after it.  Both rules below would then discard real
    # body -- the indirect-jmp rule unconditionally, since such a function
    # usually also has a switch.  parse_string (0x19be30) lost 72 of 163 insns
    # that way, scoring a 197-insn candidate at 50.7%.  A tail that branches
    # back into the body is code; trim only from the first data marker onward,
    # so a reference holding arms AND the table still drops just the table.
    if body_label_names and _tail_branches_into_body(tail, body_label_names):
        for j, ins in enumerate(tail):
            if any(m in ins for m in _DATA_GARBAGE_MARKERS):
                return insns[:last_ret + 1 + j]
        return insns
    if any(any(m in ins for m in _DATA_GARBAGE_MARKERS) for ins in tail):
        return insns[:last_ret + 1]
    # Marker-independent rule: a scaled indirect jump (jmp *disp(,%reg,4)) is
    # proof MSVC emitted a switch jump table, and MSVC always places that table
    # in .text after the function's final RET.  Enumerating the table's garbage
    # decode is unreliable -- the bytes decode differently per table (this path
    # was added because a byte-index table decoded as `addb $0x4, %al` /
    # `addb %al, (%esp,%eax)`, none of which are in _DATA_GARBAGE_MARKERS, so
    # ~55 phantom insns survived and capped FUN_0019c0a0 at 43.7% against a real
    # 35-insn body).  Keyed on the jump, not on the data.
    if any(_INDIRECT_TABLE_JMP_RE.search(ins) for ins in insns[:last_ret + 1]):
        return insns[:last_ret + 1]
    return insns


def _trim_trailing_thunks(insns: list) -> list:
    """Strip NOP padding and push/call/ret thunk stubs after the real function RET.

    The delinked reference sometimes bundles MSVC thunk stubs under the preceding
    symbol when there is no next-symbol boundary.  These inflate the reference
    instruction count and cause false low-match scores.

    Strategy: scan forward for the first RET that is followed by nothing but
    [nop*] [push+call+ret]+ sequences consuming the entire remainder.  That RET
    is the true function epilogue; everything after it is thunks.
    """
    if not insns:
        return insns
    n = len(insns)

    for i, insn in enumerate(insns):
        if mnemonic(insn).lower() not in _RET_MNEMS:
            continue

        # Skip alignment NOPs following this ret
        j = i + 1
        while j < n and mnemonic(insns[j]).lower() == 'nop':
            j += 1

        if j >= n:
            continue  # nothing meaningful after this ret — not a thunk boundary

        # Try to consume everything from j to end as push+call+ret thunk stubs
        pos = j
        thunks_found = 0
        while pos < n:
            if mnemonic(insns[pos]).lower() not in {'push', 'pushl', 'pushw'}:
                break  # unexpected — not a thunk start
            sub = pos + 1
            found_call = found_ret = False
            while sub < n:
                sm = mnemonic(insns[sub]).lower()
                if sm in {'call', 'calll', 'callw'}:
                    found_call = True
                elif sm in _RET_MNEMS and found_call:
                    found_ret = True
                    pos = sub + 1
                    while pos < n and mnemonic(insns[pos]).lower() == 'nop':
                        pos += 1
                    break
                elif sm not in _THUNK_BODY_MNEMS:
                    break
                sub += 1
            if not (found_call and found_ret):
                break
            thunks_found += 1

        if pos >= n and thunks_found > 0:
            # All remaining instructions were valid thunk stubs — trim them
            return insns[:i + 1]

    return insns


def normalize(insn: str) -> str:
    """Normalize instruction for matching: keep mnemonic + operand shape."""
    # Replace specific addresses/immediates with placeholders
    s = re.sub(r'\$0x[0-9a-f]+', '$IMM', insn)
    s = re.sub(r'\b0x[0-9a-f]+\b', 'IMM', s)
    s = re.sub(r'\b[0-9a-f]{5,}\b', 'IMM', s)
    # Normalize register-relative addressing: keep displacement sign + register
    s = re.sub(r'-?0x[0-9a-f]+\(', 'OFF(', s)
    s = re.sub(r'-?\d+\(', 'OFF(', s)
    return s.strip()


def mnemonic(insn: str) -> str:
    """Extract just the instruction mnemonic."""
    return insn.split()[0] if insn.split() else insn


# --- Register-alias normalization ---

_REG_FAMILIES = {
    'eax': ['eax', 'ax', 'al', 'ah'],
    'ecx': ['ecx', 'cx', 'cl', 'ch'],
    'edx': ['edx', 'dx', 'dl', 'dh'],
    'ebx': ['ebx', 'bx', 'bl', 'bh'],
    'esi': ['esi', 'si'],
    'edi': ['edi', 'di'],
}
_FIXED_REGS = frozenset(['esp', 'sp', 'ebp', 'bp'])

_REG_TO_FAMILY = {}
_SUBREG_SUFFIX = {}
for _base, _members in _REG_FAMILIES.items():
    for _r in _members:
        _REG_TO_FAMILY[_r] = _base
        if _r == _base:
            _SUBREG_SUFFIX[_r] = ''
        elif _r.endswith('x') or _r in ('si', 'di'):
            _SUBREG_SUFFIX[_r] = 'w'
        elif _r.endswith('l'):
            _SUBREG_SUFFIX[_r] = 'l'
        elif _r.endswith('h'):
            _SUBREG_SUFFIX[_r] = 'h'

_REG_PATTERN = re.compile(r'%(' + '|'.join(
    sorted(_REG_TO_FAMILY.keys(), key=len, reverse=True)) + r')\b')


def canonicalize_registers(insns: list[str]) -> list[str]:
    """Canonicalize GP registers to positional aliases based on first-appearance order.

    Fixed registers (ESP, EBP) are preserved. All others are mapped to R0, R1, ...
    based on the order their family first appears in the instruction stream.
    Sub-registers keep a size suffix: R0l (low byte), R0h (high byte), R0w (16-bit).
    """
    family_order = {}
    next_idx = [0]

    def _replace_reg(m):
        reg = m.group(1).lower()
        if reg in _FIXED_REGS:
            return m.group(0)
        family = _REG_TO_FAMILY.get(reg)
        if family is None:
            return m.group(0)
        if family not in family_order:
            family_order[family] = next_idx[0]
            next_idx[0] += 1
        idx = family_order[family]
        suffix = _SUBREG_SUFFIX.get(reg, '')
        return '%R' + str(idx) + suffix

    result = []
    for insn in insns:
        result.append(_REG_PATTERN.sub(_replace_reg, insn))
    return result


# --- @reg-defined parameter modeling (phantom-slot load stripping) ---
#
# kb.json annotates some functions' params as register-passed (@<eax>, @<ebx>,
# @<esi>, @<edi>, @<ax>, ...) -- an original-MSVC convention cl.exe cannot
# express in C (@<ecx>/@<edx>-only functions are handled separately by the
# __fastcall rewrite in vc71_verify).  Our candidate compiles those params as
# stack args, so it must emit mov-family loads from the phantom caller-arg
# slot disp(%ebp), disp = 8 + 4*param_idx, that the true reg-convention
# reference can never have.  Those loads are pure calling-convention
# artifacts; stripping them (candidate-only, count-aware, family-constrained)
# models what cl.exe would emit if it could take the param in the register,
# WITHOUT masking real divergence:
#   - only loads FROM the exact phantom slot with dest IN the annotated
#     register's family are eligible (a load into a different register keeps
#     its matching `mov` mnemonic against the reference's reg-to-reg move);
#   - per (disp, family), at most cand_count - ref_count occurrences are
#     removed (a slot the reference also reads is never stripped);
#   - wrong-slot reads, extra spills, frame-size deltas, and genuinely wrong
#     bodies survive untouched -- removing only candidate-surplus tokens can
#     never lower the LCS match and can never fabricate one.
# See docs/lift-learnings.md "@reg-defined prologue ceiling". Self-tests RP1-RP8.

_MOV_LOAD_MNEMONICS = frozenset([
    'mov', 'movl', 'movw', 'movb',
    'movswl', 'movzwl', 'movsbl', 'movzbl', 'movsbw', 'movzbw',
])

_PHANTOM_LOAD_RE = re.compile(
    r'^(?:0x([0-9a-f]+)|(\d+))\(%ebp\),\s*%([a-z]{2,3})$')


def parse_regdef_from_decl(decl: str) -> list[tuple[int, str]]:
    """Parse a kb.json decl's @<reg> annotations into (param_idx, reg) pairs.

    Mirrors vc71_verify._get_regarg_callees' parsing so both sides agree on
    param indexing (0-based, comma-split).
    """
    try:
        params_str = decl[decl.index("(") + 1:decl.rindex(")")]
    except ValueError:
        return []
    regs = []
    for i, p in enumerate(params_str.split(",")):
        m = re.search(r"@<(\w+)>", p)
        if m:
            regs.append((i, m.group(1).lower()))
    return regs


def _phantom_load_key(insn: str, slot_map: dict):
    """(disp, family) if insn is a mov-family load from a phantom reg-param
    slot into that param's own register family; else None."""
    parts = insn.strip().split(None, 1)
    if len(parts) != 2 or parts[0].lower() not in _MOV_LOAD_MNEMONICS:
        return None
    m = _PHANTOM_LOAD_RE.match(parts[1].strip())
    if not m:
        return None
    disp = int(m.group(1), 16) if m.group(1) else int(m.group(2))
    family = slot_map.get(disp)
    if family is None:
        return None
    if _REG_TO_FAMILY.get(m.group(3).lower()) != family:
        return None
    return (disp, family)


def strip_regparam_loads(insns: list[str], reference: list[str],
                         regdef_params: list[tuple[int, str]] | None
                         ) -> tuple[list[str], int]:
    """Remove candidate-only phantom reg-param slot loads (see module comment).

    Returns (stripped_insns, n_stripped).  Count-aware: per (disp, family),
    strips at most candidate_count - reference_count occurrences, so a slot
    the reference also loads is never touched.
    """
    if not regdef_params:
        return insns, 0
    slot_map = {}
    for idx, reg in regdef_params:
        family = _REG_TO_FAMILY.get(reg.lower())
        if family is not None:
            slot_map[8 + 4 * idx] = family
    if not slot_map:
        return insns, 0

    from collections import Counter
    ref_counts = Counter(
        k for k in (_phantom_load_key(i, slot_map) for i in reference)
        if k is not None)
    cand_keys = [_phantom_load_key(i, slot_map) for i in insns]
    excess = Counter(k for k in cand_keys if k is not None)
    excess.subtract(ref_counts)

    out = []
    n_stripped = 0
    for insn, key in zip(insns, cand_keys):
        if key is not None and excess[key] > 0:
            excess[key] -= 1
            n_stripped += 1
            continue
        out.append(insn)
    return out, n_stripped


def normalize_instruction(insn: str) -> str:
    """Full instruction normalization: mnemonic + operand shape with canonical registers."""
    # Strip disassembler label annotations: <symbol+0xNN> or <LAB_xxx>
    s = re.sub(r'\s*<[^>]+>', '', insn)
    s = re.sub(r'\$0x[0-9a-f]+', '$IMM', s)
    s = re.sub(r'\b0x[0-9a-f]+\b', 'IMM', s)
    s = re.sub(r'\b[0-9a-f]{5,}\b', 'IMM', s)
    s = re.sub(r'-?0x[0-9a-f]+\(', 'OFF(', s)
    s = re.sub(r'-?\d+\(', 'OFF(', s)
    return s.strip()


def extract_normalized_sequence(insns: list[str]) -> list[str]:
    """Canonicalize registers then normalize each instruction for LCS."""
    canonical = canonicalize_registers(insns)
    return [normalize_instruction(i) for i in canonical]


def extract_identity_sequence(insns: list[str]) -> list[str]:
    """Normalize each instruction WITHOUT register canonicalization.

    The identity register mapping: registers keep their real names.  This is
    the complement of extract_normalized_sequence -- canonicalization helps
    when the two builds use *different* registers for the same roles in the
    same order, and hurts badly when the introduction ORDER itself differs,
    because aliases are assigned by global first appearance.
    """
    return [normalize_instruction(i) for i in insns]


def extract_mnemonic_sequence(insns: list[str]) -> list[str]:
    """Get ordered mnemonic sequence for LCS matching."""
    return [mnemonic(i) for i in insns]


def lcs_ratio(a: list[str], b: list[str]) -> float:
    """Longest common subsequence ratio via SequenceMatcher."""
    if not a and not b:
        return 1.0
    if not a or not b:
        return 0.0
    return SequenceMatcher(None, a, b, autojunk=False).ratio()


def dp_lcs_len(a: list[str], b: list[str], cap: int = 25_000_000) -> int | None:
    """True dynamic-programming LCS length over two sequences.

    SequenceMatcher's ratio() is a greedy longest-matching-block heuristic,
    not a true LCS -- on some TUs the greedy anchor collapses and the
    official score reads far lower than real similarity (see
    reference_vc71_scorer_anchor_collapse memory). This is the textbook O(n*m)
    DP, kept memory-lean with a two-row rolling table (O(min(n,m)) space) so
    it is safe to run on every scored function.

    Guards against pathological TU sizes: if n*m exceeds `cap`, returns None
    so callers fall back to the SequenceMatcher-derived score instead of
    paying an unbounded compute cost.
    """
    n, m = len(a), len(b)
    if n == 0 or m == 0:
        return 0
    if n * m > cap:
        return None
    # Iterate the longer sequence in the outer loop so the row is sized to
    # the shorter one.
    if m > n:
        a, b = b, a
        n, m = m, n
    prev = [0] * (m + 1)
    for i in range(1, n + 1):
        curr = [0] * (m + 1)
        ai = a[i - 1]
        for j in range(1, m + 1):
            if ai == b[j - 1]:
                curr[j] = prev[j - 1] + 1
            else:
                pv = prev[j]
                cv = curr[j - 1]
                curr[j] = pv if pv >= cv else cv
        prev = curr
    return prev[m]


def dp_lcs_ratio(a: list[str], b: list[str], cap: int = 25_000_000) -> float | None:
    """LCS-based similarity ratio, same 2*M/T formula SequenceMatcher.ratio()
    uses, so it is directly comparable to the official mnemonic-only score.

    Returns None (rather than raising or silently degrading) when the DP is
    too expensive for this pair (see dp_lcs_len's `cap`).
    """
    if not a and not b:
        return 1.0
    if not a or not b:
        return 0.0
    lcs = dp_lcs_len(a, b, cap=cap)
    if lcs is None:
        return None
    total = len(a) + len(b)
    return (2.0 * lcs) / total if total else 1.0


def mnemonic_diff_opcodes(compiled: list[str], reference: list[str]):
    """SequenceMatcher opcodes over the same mnemonic-only sequences used to
    compute the official (headline) score in compare_functions().

    Returns the raw get_opcodes() list: (tag, i1, i2, j1, j2) tuples where
    tag is one of 'equal'/'replace'/'delete'/'insert' and i/j index into
    `compiled`/`reference` respectively. Callers that already hold the
    official-score SequenceMatcher (e.g. inside compare_functions) can reuse
    its opcodes directly instead of calling this -- it exists for callers
    (like the score-context pack builder) that only have the raw instruction
    lists and want the same alignment the official score used, without
    duplicating extract_mnemonic_sequence() call sites.
    """
    c_seq = extract_mnemonic_sequence(compiled)
    r_seq = extract_mnemonic_sequence(reference)
    return SequenceMatcher(None, c_seq, r_seq, autojunk=False).get_opcodes()


FPU_MNEMONICS = {'fld', 'flds', 'fldl', 'fldt', 'fild', 'fst', 'fstp',
                 'fmul', 'fmuls', 'fmulp', 'fdiv', 'fdivs', 'fdivp', 'fdivr', 'fdivrs', 'fdivrp',
                 'fadd', 'fadds', 'faddp', 'fsub', 'fsubs', 'fsubp', 'fsubr', 'fsubrs', 'fsubrp',
                 'fabs', 'fchs', 'fsqrt', 'fsin', 'fcos', 'fxch', 'fcom', 'fcomp', 'fcompp',
                 'fucom', 'fucomp', 'fucompp', 'fnstsw', 'fnstcw', 'fldcw'}


def extract_fpu_blocks(insns: list[str]) -> list[list[str]]:
    """Extract contiguous FPU instruction sequences (basic blocks of FPU ops)."""
    blocks = []
    current = []
    for insn in insns:
        m = mnemonic(insn).lower()
        if m in FPU_MNEMONICS:
            current.append(insn)
        else:
            if len(current) >= 3:
                blocks.append(current)
            current = []
    if len(current) >= 3:
        blocks.append(current)
    return blocks


def compare_fpu_blocks(compiled_blocks: list[list[str]], ref_blocks: list[list[str]]) -> list[str]:
    """Compare FPU blocks between compiled and reference, flag operand swaps."""
    warnings = []
    # Match blocks by position (rough heuristic)
    for i, (cb, rb) in enumerate(zip(compiled_blocks, ref_blocks)):
        c_mnems = [mnemonic(x).lower() for x in cb]
        r_mnems = [mnemonic(x).lower() for x in rb]
        if c_mnems == r_mnems:
            # Same mnemonic sequence — check if operands differ
            for j, (ci, ri) in enumerate(zip(cb, rb)):
                if normalize(ci) != normalize(ri):
                    cm = mnemonic(ci).lower()
                    if cm.startswith(('fsub', 'fmul', 'fld', 'fadd', 'fdiv')):
                        warnings.append(f"    FPU block {i}, insn {j}: {ci.strip()}  vs  {ri.strip()}")
        elif set(c_mnems) == set(r_mnems) and sorted(c_mnems) == sorted(r_mnems):
            # Same mnemonics but reordered — likely operand swap
            warnings.append(f"    FPU block {i}: same ops, different order (possible operand swap)")
            for ci, ri in zip(cb[:6], rb[:6]):
                warnings.append(f"      compiled:  {ci.strip()}")
                warnings.append(f"      reference: {ri.strip()}")
    return warnings


# --- Load-width mismatch detection (int vs int16_t / int8_t) ---
#
# Both the compiled object and the delinked reference are MSVC codegen, so a
# narrow signed/unsigned memory load (movsx/movzx word/byte) present on one side
# but not the other -- at the same field offset -- almost always means the C type
# width is wrong (e.g. `*(int*)(p+off)` where the original does a signed 16-bit
# read). The aggregate LCS % hides this (one instruction among dozens: the c10
# fog-zone crash moved the score only 85.5%->85.6%), so we census the narrow
# memory loads on each side and report the difference. This is the mechanical
# detector for docs/lift-learnings.md "int vs int16_t" (feedback_check_disasm)
# and the c10 tag_groups.c:3089 pg_surf regression.

# A narrow (16- or 8-bit) memory READ. The SAME 16-bit field read compiles to
# two different AT&T forms across builds, so we must recognise both:
#   fused : movswl/movzwl/movsbl/movzbl  <disp(%reg)>, %r32   (sign/zero-extend from mem)
#   split : movw/movb                    <disp(%reg)>, %r16    (load, widened separately)
# Both READ 16 (or 8) bits from memory, so we classify by access WIDTH ('n16'/'n8')
# and IGNORE the fused-vs-split distinction -- otherwise the split form looks like
# a "missing" narrow load and false-flags a correct lift. 32-bit reads (plain
# `mov`, `push mem`, arithmetic) are NOT in these sets, so a field the original
# reads at 16/8 bits but our lift reads at 32 bits shows up as a census mismatch.
_NARROW_READ_WIDTH = {
    'movswl': 'n16', 'movzwl': 'n16', 'movw': 'n16',
    'movsbl': 'n8',  'movzbl': 'n8',  'movb': 'n8',
    'movsx': 'nX', 'movzx': 'nX',  # Intel-syntax fallback (defensive)
}

# Fused byte/word ALU ops that READ a narrow memory operand. cl.exe (VC71) often
# folds a byte/word field read into one of these instead of emitting a separate
# movb/movw load -- e.g. the original's `movb 0xc(%ebx),%cl; mov $1,%eax;
# test %cl,%al` compiles to a single `testb %cl, 0xc(%eax)`. Both READ the same
# field at the same width; only the split-vs-fused encoding differs. llvm-objdump
# emits an explicit b/w size suffix (`testb`, `andw`, ...), which gives the width.
# These ops read their memory operand in EITHER position (test/cmp read-only;
# and/or/xor/add/sub/adc/sbb are read-modify-write), so a disp(%reg) in any
# operand counts. Recognising them lets the fused form cancel the split load in
# the presence census below (the d3080 hud_draw false positive, 2026-07-02).
_NARROW_ALU_OPS = {'test', 'cmp', 'and', 'or', 'xor', 'add', 'sub', 'adc', 'sbb'}


def narrow_mem_load_shape(insn: str):
    """If `insn` READS 16 or 8 bits FROM MEMORY, return (width_class, operand_key);
    otherwise None.

    The memory operand must be the SOURCE (first AT&T operand) so 16/8-bit stores
    (`movw %ax, 0x24(%eax)`) and register-to-register widenings (`movswl %cx,%edx`)
    are excluded. Only STRUCT-POINTER derefs count: frame-relative accesses
    (%ebp/%esp) are stack locals whose slot assignment is build-specific noise,
    not typed field reads, so they are excluded. Absolute addresses (globals) are
    also excluded -- global access width is confounded by inlining and is a
    different, rarer bug class. The key keeps the displacement (field offset) but
    drops the base register, so it is robust to register-allocation differences.
    """
    parts = insn.split(None, 1)
    if not parts:
        return None
    mnem = parts[0].lower()

    # Narrow memory LOAD (movb/movw/movsx/movzx family). The memory operand must
    # be the SOURCE (first AT&T operand) so 16/8-bit stores and reg-to-reg
    # widenings are excluded.
    wc = _NARROW_READ_WIDTH.get(mnem)
    if wc is not None:
        if len(parts) < 2:
            return None
        src = parts[1].split(',')[0].strip()  # AT&T source operand is first
        m = re.match(r'^(-?0x[0-9a-f]+|-?\d+)?\((%\w+)\)$', src)  # disp(%reg)
        if m:
            base = m.group(2).lower()
            if base in ('%ebp', '%esp'):
                return None  # stack local -- build-specific slot, not a field type
            return (wc, f"disp:{m.group(1) or '0x0'}")
        return None  # absolute/global, register source, store, or unrecognized

    # Fused byte/word ALU op (testb/andw/...) reading a disp(%reg) memory operand
    # in EITHER position. Same width class + offset key as the split load above,
    # so a fused `testb %cl, 0xc(%eax)` cancels a split `movb 0xc(%ebx), %cl`.
    if len(parts) >= 2 and len(mnem) >= 2 and mnem[-1] in ('b', 'w') \
            and mnem[:-1] in _NARROW_ALU_OPS:
        wc = 'n8' if mnem[-1] == 'b' else 'n16'
        # disp(%reg) with the ')' immediately after the base register excludes
        # SIB/indexed operands (`0xc(%eax,%edi,2)`), whose access width is a
        # different, rarer analysis.
        m = re.search(r'(-?0x[0-9a-f]+|-?\d+)?\((%\w+)\)', parts[1])
        if m:
            base = m.group(2).lower()
            if base in ('%ebp', '%esp'):
                return None  # stack local
            return (wc, f"disp:{m.group(1) or '0x0'}")
    return None  # absolute/global, register-only, store, or unrecognized


def compare_load_widths(compiled: list[str], reference: list[str]) -> list[str]:
    """Flag fields that one side reads narrow (16/8-bit) and the other does not.

    Compares the SET of (width-class, field-offset) narrow reads -- PRESENCE, not
    count. Using presence (not a count/Counter) is deliberate: two differently
    sourced MSVC builds routinely read the same field a different NUMBER of times
    (common-subexpression elimination, recomputation), which a count-based diff
    false-flags even though both read it at the correct width. A field present on
    exactly one side is the real signal: reference-only -> we WIDENED it (the c10
    crash direction); compiled-only -> we over-narrowed a 32-bit value.
    """
    def shapes(insns):
        out = {}
        for insn in insns:
            s = narrow_mem_load_shape(insn)
            if s is not None:
                out.setdefault(s, insn.strip())
        return out

    c_sh = shapes(compiled)
    r_sh = shapes(reference)
    warnings = []
    for shape in sorted(set(r_sh) - set(c_sh)):
        wc, key = shape
        warnings.append(
            f"    LOADW: reference reads a narrow field [{wc} {key}] that our lift "
            f"does not (ref: `{r_sh[shape]}`) -- likely a narrow field read as a "
            f"wider type (int vs int16_t/int8_t)")
    for shape in sorted(set(c_sh) - set(r_sh)):
        wc, key = shape
        warnings.append(
            f"    LOADW: our lift reads a narrow field [{wc} {key}] the reference "
            f"does not (ours: `{c_sh[shape]}`) -- possible over-narrowed field "
            f"(reading only the low bits of a wider value)")
    return warnings


# --- Immediate-constant transcription detection (wrong float/magic literal) ---
#
# Both objects here are VC71 codegen (delinked reference = original MSVC 7.1;
# candidate = our source recompiled by the SAME CL.exe). Identical source
# constants therefore produce identical inline immediates, so a large inline
# constant present on exactly one side is a genuine SOURCE difference -- almost
# always a mistyped numeric literal, not compiler noise. The aggregate LCS score
# hides these: a wrong `push $imm32` aligns against the correct `push $imm32`
# (same opcode, so the operand delta is buried, never surfaced as actionable).
# This is the mechanical detector for the actor_aim_grenade cos(30) bug
# (reference 0x3f5db3d7 = 0.8660254 vs candidate 0x3f5b8f13 = 0.8576519, commit
# c2866b56) -- see docs/lift-learnings.md "immediate-constant transcription".
#
# Only immediates whose SIGNED magnitude is >= 0x01000000 (16 MiB) are censused.
# The XBE image/data all live below that, so:
#   - relocated global/string addresses (0x254b58, ...) -- which resolve to a VA
#     in the delinked reference but to a 0-placeholder relocation in our candidate
#     -- are excluded (they would false-flag every function otherwise);
#   - small POSITIVE integers (loop bounds, masks, struct sizes) are excluded as
#     build-variable noise;
#   - small NEGATIVE integers, which sign-extend to huge unsigned values
#     (-1 = 0xffffffff, stack offsets like -100 = 0xffffff9c), are also excluded:
#     -1 in particular is the MSVC SEH scope-index initializer
#     (`movl $-1, -0x4(%ebp)`) and a ubiquitous sentinel -- pure frame noise.
# Using the SIGNED magnitude keeps NEGATIVE float bit-patterns (0xc0490fdb = -pi,
# signed magnitude ~1.07e9) while dropping small sign-extended integers. What
# remains is exactly the class a transcription error corrupts: single-precision
# float bit-patterns (positive and negative), FourCC tag magics
# ('weap'=0x77656170), and large integer magics.
_IMM_ADDR_CEILING = 0x01000000


def _imm_is_large(v: int) -> bool:
    """True if the immediate's signed-32 magnitude is >= the address ceiling."""
    signed = v - 0x100000000 if v >= 0x80000000 else v
    return abs(signed) >= _IMM_ADDR_CEILING


def _imm_is_float(v: int) -> bool:
    """True if the 32-bit pattern is a finite, non-tiny single-precision float."""
    exp = (v >> 23) & 0xff
    return 0 < exp < 0xff  # exclude zero/denormal (exp 0) and inf/nan (exp 0xff)


def _imm_as_float(v: int) -> float:
    import struct
    return struct.unpack("<f", struct.pack("<I", v & 0xffffffff))[0]


def _imm_is_fourcc(v: int) -> bool:
    """True if all 4 bytes are printable ASCII (a tag/group magic like 'weap').

    Genuine float constants almost always have zero mantissa bytes (0x3f800000,
    0x41200000, ...), so they fail this test and fall through to the float
    decode; only true FourCC magics are all-printable. Checked before the float
    decode because such magics also happen to have a valid float exponent.
    """
    b = v.to_bytes(4, "little")
    return all(0x20 <= c < 0x7f for c in b)


def _fmt_imm(v: int) -> str:
    """Human-readable rendering: FourCC tag, float decode, or raw hex."""
    if _imm_is_fourcc(v):
        tag = v.to_bytes(4, "little")[::-1].decode("ascii")  # FourCC reads high byte first
        return f"0x{v:08x} ('{tag}')"
    if _imm_is_float(v):
        return f"0x{v:08x} (~{_imm_as_float(v):.7g}f)"
    return f"0x{v:08x}"


def _iter_immediates(insns: list[str]):
    """Yield (value_uint32, insn) for every AT&T `$0x..` immediate operand."""
    for insn in insns:
        for m in re.finditer(r'\$0x([0-9a-fA-F]+)', insn):
            try:
                yield int(m.group(1), 16) & 0xffffffff, insn.strip()
            except ValueError:
                continue


def compare_immediates(compiled: list[str], reference: list[str]) -> list[str]:
    """Flag large constant immediates (>= 0x01000000) present on exactly one side.

    Presence-based (SET), not count -- like compare_load_widths -- because VC71
    may materialize the same constant a different number of times (CSE). A
    reference-only float whose nearest candidate-only value is within 5% relative
    error is reported as a paired near-miss (`reference X vs our lift Y`): that is
    the transcription-error signature (right magnitude, wrong bits). Unpaired
    constants are reported as absent/introduced. Returns a list of warning lines.
    """
    def big_imms(insns):
        out = {}
        for v, insn in _iter_immediates(insns):
            if _imm_is_large(v):
                out.setdefault(v, insn)
        return out

    c_imms = big_imms(compiled)
    r_imms = big_imms(reference)
    ref_only = sorted(set(r_imms) - set(c_imms))
    cand_only = sorted(set(c_imms) - set(r_imms))

    warnings = []
    used_cand = set()
    for v in ref_only:
        partner = None
        best_rel = None
        if _imm_is_float(v) and not _imm_is_fourcc(v):
            fv = _imm_as_float(v)
            for w in cand_only:
                if w in used_cand or not _imm_is_float(w) or _imm_is_fourcc(w):
                    continue
                fw = _imm_as_float(w)
                rel = abs(fv - fw) / max(abs(fv), 1e-30)
                if rel < 0.05 and (best_rel is None or rel < best_rel):
                    best_rel, partner = rel, w
        if partner is not None:
            used_cand.add(partner)
            warnings.append(
                f"    IMM: near-miss float literal -- reference {_fmt_imm(v)} vs "
                f"our lift {_fmt_imm(partner)} (rel err {best_rel * 100:.3f}%); a "
                f"wrong decimal literal the LCS byte-diff aligns away. Verify the "
                f"source constant against the disassembly immediate.")
        else:
            warnings.append(
                f"    IMM: reference constant {_fmt_imm(v)} absent from our lift "
                f"(ref: `{r_imms[v]}`) -- missing or altered constant.")
    for w in cand_only:
        if w in used_cand:
            continue
        warnings.append(
            f"    IMM: our lift has constant {_fmt_imm(w)} not in the reference "
            f"(ours: `{c_imms[w]}`) -- introduced or altered constant.")
    return warnings


# --- FCOM guard bound-sense detection (inclusive vs strict float compare) ---
#
# After `fcom*/fucom* <src>; fnstsw %ax`, VC71 encodes the comparison SENSE in
# the (TEST $mask,%ah ; Jcc) pair that consumes the status word (C0=0x01,
# C2=0x04, C3=0x40; unordered sets all three):
#     testb $0x41,%ah ; jp   -> taken when ST0 >  src (or unordered)
#     testb $0x05,%ah ; jp   -> taken when ST0 >= src (or unordered)
#     testb $0x41,%ah ; jne  -> taken when ST0 <= src (or unordered)
#     testb $0x01,%ah ; jne  -> taken when ST0 <  src (or unordered)
#     testb $0x44,%ah ; jnp  -> taken when ST0 == src (ordered)
# A strict `<` lifted where the original's guard is inclusive `<=` (or vice
# versa) therefore changes the guard SHAPE -- the mask and/or the jcc -- but
# the aggregate LCS is structurally blind to it: `test` aligns against `test`
# and `jp` against `jne` costs one instruction in a whole function, so the
# score barely moves. The player_control pitch-bound assert (ca725641) sat at
# an unchanged 96.8% through the bug, and the same class recurred in
# quantize_real_to_byte_*_bound three days apart. Both objects here are VC71
# codegen, so -- exactly like IMM -- a guard shape present on one side only is
# a genuine source difference: almost always a comparison operator with the
# wrong bound sense. See docs/lift-learnings.md section 38.
#
# Presence-census (SET, not count), like LOADW/IMM: the same shape
# legitimately recurs (a validator checks four bounds with the same sense),
# and a mis-sensed bound produces a NEW shape on our side and/or removes the
# only instance of one on the reference side.

_FCOM_C0, _FCOM_C2, _FCOM_C3 = 0x01, 0x04, 0x40
# AH status-byte value for each of the four fcom outcomes.
_FCOM_OUTCOMES = (
    ("ST0 > src", 0x00),
    ("ST0 < src", _FCOM_C0),
    ("ST0 == src", _FCOM_C3),
    ("unordered", _FCOM_C0 | _FCOM_C2 | _FCOM_C3),
)

_FCOM_TEST_RE = re.compile(r"^test[bwl]?\s+\$(0x[0-9a-fA-F]+|\d+),\s*%(ah|ax)\b")
# Only ZF/PF consumers decode cleanly from TEST; other jccs never follow a
# VC71 fnstsw/test guard.
_FCOM_JCC = {"jp", "jnp", "je", "jz", "jne", "jnz"}


def fcom_guard_sense(mask: int, jcc: str) -> str:
    """Decode which fcom outcomes take the (TEST $mask,%ah ; Jcc) branch."""
    taken = []
    for name, ah in _FCOM_OUTCOMES:
        v = ah & mask
        if jcc in ("jne", "jnz"):
            hit = v != 0
        elif jcc in ("je", "jz"):
            hit = v == 0
        elif jcc == "jp":
            hit = bin(v).count("1") % 2 == 0  # PF set = even parity (incl. zero)
        else:  # jnp
            hit = bin(v).count("1") % 2 == 1
        if hit:
            taken.append(name)
    return "taken when " + " / ".join(taken) if taken else "never taken"


def extract_fcom_guards(insns: list[str]) -> dict[tuple[int, str], str]:
    """Map each (mask, jcc) FPU-guard shape to an example instruction context.

    A guard is `fnstsw %ax` immediately followed (as VC71 emits it) by
    `test $imm,%ah` (or `%ax`; mask taken from the high byte) and then the
    conditional jump consuming it. The example string carries the preceding
    fcom*/fucom* instruction when present so the warning names the compare.
    """
    out: dict[tuple[int, str], str] = {}
    for i, insn in enumerate(insns):
        if not insn.strip().startswith("fnstsw") or i + 2 >= len(insns):
            continue
        m = _FCOM_TEST_RE.match(insns[i + 1].strip())
        if not m:
            continue
        v = int(m.group(1), 0)
        mask = (v >> 8) & 0xff if m.group(2) == "ax" else v & 0xff
        jcc = mnemonic(insns[i + 2].strip()).lower()
        if jcc not in _FCOM_JCC:
            continue
        prev = insns[i - 1].strip() if i > 0 else ""
        ctx = f"{prev} ; " if prev.startswith(("fcom", "fucom", "ficom")) else ""
        example = f"{ctx}{' '.join(insns[i + 1].split())} ; {jcc}"
        out.setdefault((mask, jcc), example)
    return out


def compare_fcom_guards(compiled: list[str], reference: list[str]) -> list[str]:
    """Flag FPU-guard shapes (mask, jcc) present on exactly one side.

    Presence-based (SET), not count -- see the module comment above. Returns
    a list of warning lines; both directions are reported, and when shapes
    exist on both sides the mismatch is called out as a probable
    comparison-sense (<= vs <, >= vs >) source bug.
    """
    c_g = extract_fcom_guards(compiled)
    r_g = extract_fcom_guards(reference)
    ref_only = sorted(set(r_g) - set(c_g))
    cand_only = sorted(set(c_g) - set(r_g))
    warnings = []
    for mask, jcc in ref_only:
        warnings.append(
            f"    FCOM: reference guard `{r_g[(mask, jcc)]}` "
            f"({fcom_guard_sense(mask, jcc)}) has no counterpart in our lift")
    for mask, jcc in cand_only:
        warnings.append(
            f"    FCOM: our lift guards with `{c_g[(mask, jcc)]}` "
            f"({fcom_guard_sense(mask, jcc)}) -- shape absent from the reference")
    if ref_only and cand_only:
        warnings.append(
            "    FCOM: guard shapes differ on BOTH sides -- likely a comparison "
            "bound-sense bug (<= lifted as <, or >= as >). Verify each operator "
            "against the pristine disassembly's TEST/Jcc pair "
            "(lift-learnings section 38; the MP look-up assert class).")
    return warnings


def compare_functions(compiled: list[str], reference: list[str],
                      reg_normalize: bool = False,
                      regdef_params: list[tuple[int, str]] | None = None
                      ) -> tuple[float, list[str], list[str], list[str], list[str], list[str]]:
    """Compare two functions.
    Returns (match_pct, diff_summary, fpu_warnings, loadw_warnings,
    imm_warnings, fcom_warnings).

    When reg_normalize=True, uses full normalized instructions with register
    canonicalization for LCS instead of mnemonic-only. This catches operand-level
    bugs (argument order swaps, wrong memory sources) while being tolerant of
    benign register allocation differences between compilers.

    regdef_params: optional [(param_idx, reg)] pairs for a @<reg>-DEFINED
    function (params arrive in registers cl.exe cannot model).  Candidate-only
    phantom-slot loads are stripped before comparison (see strip_regparam_loads)
    and the HIGHER of raw/stripped is reported: in mnemonic space a phantom
    `movl` can align against any reference `movl`, so stripping occasionally
    shifts alignment down a fraction -- the model removes a known convention
    penalty and must never introduce one.
    """
    if regdef_params:
        stripped = strip_regparam_loads(compiled, reference, regdef_params)[0]
        if len(stripped) != len(compiled):
            raw = compare_functions(compiled, reference, reg_normalize)
            modeled = compare_functions(stripped, reference, reg_normalize)
            return modeled if modeled[0] >= raw[0] else raw
    if reg_normalize:
        # Score BOTH register mappings and report the higher.
        #
        # canonicalize_registers() aliases registers by GLOBAL first-appearance
        # order, so a candidate and a reference that introduce register
        # families in a different order score near-zero on operands even when
        # the lift is correct.  Measured on FUN_00017ab0 (the largest @<reg>
        # function in the repo): 17.1% canonicalized against 58.8% identity-
        # mapped, with an exhaustive search over all 720 register permutations
        # confirming no global renaming recovers it -- the divergence is
        # positional, so canonicalization actively hurts.  @<reg> functions
        # trigger this systematically: the reference pins the params in
        # callee-saved registers from the prologue while our cl.exe build takes
        # them as stack args, permuting the whole appearance order.
        #
        # Taking the max makes this a true LOWER BOUND on operand fidelity
        # instead of a register-order lottery.  Neither mapping is universally
        # better: canonicalization rescues benign whole-function register
        # substitution, identity rescues order divergence.
        c_canon = extract_normalized_sequence(compiled)
        r_canon = extract_normalized_sequence(reference)
        c_ident = extract_identity_sequence(compiled)
        r_ident = extract_identity_sequence(reference)
        canon_ratio = lcs_ratio(c_canon, r_canon)
        ident_ratio = lcs_ratio(c_ident, r_ident)
        if ident_ratio > canon_ratio:
            c_seq, r_seq, ratio = c_ident, r_ident, ident_ratio
        else:
            c_seq, r_seq, ratio = c_canon, r_canon, canon_ratio
    else:
        c_seq = extract_mnemonic_sequence(compiled)
        r_seq = extract_mnemonic_sequence(reference)
        ratio = lcs_ratio(c_seq, r_seq)

    # Generate unified diff summary (always show raw instructions for readability)
    diffs = []
    sm = SequenceMatcher(None, c_seq, r_seq, autojunk=False)
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag == 'replace':
            for k in range(min(i2 - i1, j2 - j1)):
                ci = i1 + k
                rj = j1 + k
                diffs.append(f"  [{ci:4d}] - {compiled[ci].strip()}")
                diffs.append(f"         + {reference[rj].strip()}")
        elif tag == 'delete':
            for ci in range(i1, i2):
                diffs.append(f"  [{ci:4d}] - {compiled[ci].strip()}")
        elif tag == 'insert':
            for rj in range(j1, j2):
                diffs.append(f"         + {reference[rj].strip()}")

    # FPU-specific comparison
    c_fpu = extract_fpu_blocks(compiled)
    r_fpu = extract_fpu_blocks(reference)
    fpu_warnings = compare_fpu_blocks(c_fpu, r_fpu)

    # Load-width comparison (int vs int16_t/int8_t)
    loadw_warnings = compare_load_widths(compiled, reference)

    # Immediate-constant comparison (wrong float/magic literal)
    imm_warnings = compare_immediates(compiled, reference)

    # FPU-guard bound-sense comparison (inclusive vs strict float compare)
    fcom_warnings = compare_fcom_guards(compiled, reference)

    return ratio * 100, diffs, fpu_warnings, loadw_warnings, imm_warnings, fcom_warnings


def _self_test():
    """Built-in tests for the load-width detector (no external files needed)."""
    failures = []

    def check(name, cond):
        print(f"  {'ok  ' if cond else 'FAIL'} {name}")
        if not cond:
            failures.append(name)

    # 1. Positive: original reads a 16-bit field at +0x24, our lift reads it
    #    wider (the c10 pg_surf regression: `push 0x24(%eax)` [32-bit] vs movswl).
    ours = ["mov %eax, %esi", "push 0x24(%eax)", "call 0x72a1f0"]
    ref  = ["mov %eax, %esi", "movswl 0x24(%eax), %eax", "push %eax", "call 0x19b210"]
    w = compare_load_widths(ours, ref)
    check("widened 16-bit field flagged", any("n16 disp:0x24" in x and "reference reads" in x for x in w))

    # 2. Positive: field read wide via a 32-bit `mov` where original reads 16-bit.
    w = compare_load_widths(["mov 0x8(%ecx), %eax"], ["movzwl 0x8(%ecx), %eax"])
    check("widened via mov flagged", any("n16 disp:0x8" in x for x in w))

    # 3. Positive (reverse): we over-narrow a field the reference reads wide.
    w = compare_load_widths(["movswl 0x10(%eax), %eax"], ["mov 0x10(%eax), %eax"])
    check("over-narrowed field flagged", any("our lift reads" in x and "n16 disp:0x10" in x for x in w))

    # 4. CRITICAL negative: split-form vs fused form of the SAME 16-bit read must
    #    NOT flag (`movw mem,%ax` [our build] == `movswl mem,%eax` [reference]).
    w = compare_load_widths(["movw 0x24(%eax), %ax"], ["movswl 0x24(%eax), %eax"])
    check("split vs fused 16-bit read not flagged", w == [])

    # 5. CRITICAL negative: same field read a DIFFERENT NUMBER of times (CSE /
    #    recomputation) must NOT flag -- this was the FUN_0014ec30 false positive
    #    (reference reads +0x8 twice, our lift once; both 16-bit).
    w = compare_load_widths(["movw 0x8(%eax), %si"],
                            ["movw 0x8(%esi), %ax", "movw 0x8(%esi), %cx"])
    check("differing read COUNT (CSE) not flagged", w == [])

    # 6. Negative: same narrow load, different base register -> no warning.
    w = compare_load_widths(["movswl 0x24(%ecx), %edx"], ["movswl 0x24(%eax), %eax"])
    check("reg-alloc difference not flagged", w == [])

    # 7. Negative: register-to-register widening / 16-bit STORE are not field loads.
    w = compare_load_widths(["movswl %cx, %edx", "movw %ax, 0x8(%esi)"],
                            ["mov %ecx, %edx", "movw %ax, 0x8(%esi)"])
    check("reg widen and 16-bit store not flagged", w == [])

    # 8. Negative: identical narrow loads cancel.
    w = compare_load_widths(["movswl 0x8(%eax), %eax"], ["movswl 0x8(%esi), %ecx"])
    check("matching narrow loads not flagged", w == [])

    # 9. CRITICAL negative: fused byte ALU op == split byte load of the same field.
    #    cl.exe folds `movb 0xc(%ebx),%cl; test %cl,%al` into `testb %cl,0xc(%eax)`;
    #    both READ byte @0xc, so they must cancel (the d3080 hud_draw false positive).
    w = compare_load_widths(["testb %cl, 0xc(%eax)"], ["movb 0xc(%ebx), %cl"])
    check("fused testb vs split movb (same byte field) not flagged", w == [])

    # 10. Positive still holds: a genuine over-widen is NOT masked by the ALU form.
    #     Candidate reads the field WIDE (32-bit movl), reference narrow -> flags.
    w = compare_load_widths(["movl 0xc(%eax), %ecx"], ["movb 0xc(%ebx), %cl"])
    check("wide read vs narrow field still flagged (ALU ext is safe)",
          any("n8 disp:0xc" in x and "reference reads" in x for x in w))

    # 11. Shape-level checks for the fused ALU recogniser.
    check("fused andw recognised as n16",
          narrow_mem_load_shape("andw %ax, 0x8(%eax)") == ('n16', 'disp:0x8'))
    check("fused testb immediate form recognised",
          narrow_mem_load_shape("testb $0x1, 0xc(%esi)") == ('n8', 'disp:0xc'))
    check("fused ALU on stack local excluded",
          narrow_mem_load_shape("testb %cl, -0xc(%ebp)") is None)
    check("32-bit ALU (testl) is not a narrow read",
          narrow_mem_load_shape("testl %ecx, 0xc(%eax)") is None)
    check("fused ALU with SIB (indexed) not misread as disp(%reg)",
          narrow_mem_load_shape("testb %cl, 0xc(%eax,%edi,2)") is None)

    # --- Immediate-constant detector tests ---

    # I1. The actor_aim_grenade cos(30) bug: reference pushes 0x3f5db3d7
    #     (0.8660254), our lift pushes 0x3f5b8f13 (0.8576519). Same opcode, so LCS
    #     aligns them away -- the census must pair them as a near-miss.
    w = compare_immediates(["pushl $0x3f5b8f13", "call 0x10eaa0"],
                           ["pushl $0x3f5db3d7", "call 0x10eaa0"])
    check("cos(30) near-miss float literal flagged",
          any("near-miss float" in x and "3f5db3d7" in x and "3f5b8f13" in x for x in w))

    # I2. CRITICAL negative: relocated global/string addresses (< 0x01000000)
    #     resolve to a VA in the delinked ref but a 0-placeholder in the candidate;
    #     they must NOT be censused (would false-flag every function).
    w = compare_immediates(["pushl $0x0", "call 0x10eaa0"],
                           ["pushl $0x254b58", "call 0x10eaa0"])
    check("relocated address (below ceiling) not flagged", w == [])

    # I3. CRITICAL negative: small integers (stack adj, loop bounds, masks) differ
    #     between builds as benign noise -- excluded by the ceiling.
    w = compare_immediates(["subl $0x2c, %esp", "cmpl $0x10, %eax"],
                           ["subl $0x30, %esp", "cmpl $0x8, %eax"])
    check("small-int stack/loop noise not flagged", w == [])

    # I4. Negative: identical large constants (present on both sides) cancel even
    #     if materialized a different NUMBER of times (CSE) -- presence, not count.
    w = compare_immediates(["pushl $0x3f800000", "pushl $0x3f800000"],
                           ["pushl $0x3f800000"])
    check("identical float constant (differing count) not flagged", w == [])

    # I5. Positive: a wrong FourCC tag magic ('weap' vs 'weaX') is flagged as a
    #     genuine altered constant (not a near-miss float).
    w = compare_immediates(["pushl $0x77656158"], ["pushl $0x77656170"])
    check("wrong FourCC magic flagged",
          any("77656170" in x and "weap" in x for x in w))

    # I6. Formatting: float decode and FourCC rendering.
    check("float immediate decodes", "0.8660254f" in _fmt_imm(0x3f5db3d7))
    check("FourCC immediate decodes", "'weap'" in _fmt_imm(0x77656170))

    # I7. CRITICAL negative: 0xffffffff (-1) is the MSVC SEH scope-index init and
    #     a ubiquitous sentinel -- build-variable frame noise, must NOT flag (this
    #     was the actor_aim_grenade false positive: `movl $-1, -0x4(%ebp)`).
    w = compare_immediates(["nop"], ["movl $0xffffffff, -0x4(%ebp)"])
    check("SEH -1 scope-index initializer not flagged", w == [])

    # I8. Negative: sign-extended small negatives (stack offsets, `add $-100`)
    #     excluded by signed magnitude.
    w = compare_immediates(["addl $0xffffff9c, %eax"], ["addl $0xffffff00, %eax"])
    check("sign-extended small negatives not flagged", w == [])

    # I9. Positive: a NEGATIVE float bit-pattern (-pi = 0xc0490fdb) is still
    #     censused despite the sign bit -- its signed magnitude is ~1.07e9.
    w = compare_immediates(["pushl $0xc0490fd0"], ["pushl $0xc0490fdb"])
    check("negative float (-pi) near-miss flagged",
          any("near-miss float" in x and "c0490fdb" in x for x in w))

    # --- FCOM guard bound-sense detection (compare_fcom_guards) ---
    # These pin the player_control pitch-bound class (ca725641): the original
    # asserts only on STRICTLY greater (testb $0x41 ; jp -- equality passes,
    # NaN asserts) while a strict `<` lift asserts on greater-OR-EQUAL
    # (testb $0x5 ; jp). LCS scores both within noise of each other.
    def _guard(mask_jcc):
        mask, jcc = mask_jcc
        return [f"fcomps 0x0", "fnstsw %ax", f"testb ${mask:#x}, %ah",
                f"{jcc} 0x10 <LAB_00000010>", "retl"]

    # F1. Sense decode: the exact guards from 0xb7e8b and the strict mis-lift.
    check("0x41/jp decodes inclusive (equality passes, NaN taken)",
          fcom_guard_sense(0x41, "jp") == "taken when ST0 > src / unordered")
    check("0x5/jp decodes strict (equality taken)",
          fcom_guard_sense(0x05, "jp") == "taken when ST0 > src / ST0 == src / unordered"
          or fcom_guard_sense(0x05, "jp") == "taken when ST0 > src / unordered / ST0 == src")
    check("0x41/jne decodes <= or unordered",
          fcom_guard_sense(0x41, "jne") == "taken when ST0 < src / ST0 == src / unordered")
    check("0x44/jnp decodes ordered equality",
          fcom_guard_sense(0x44, "jnp") == "taken when ST0 == src")

    # F2. The real bug shape: reference inclusive (0x41/jp), lift strict
    #     (0x5/jp) -> flagged in both directions plus the sense-swap hint.
    w = compare_fcom_guards(_guard((0x05, "jp")), _guard((0x41, "jp")))
    check("strict-for-inclusive bound flagged",
          any("reference guard" in x and "0x41" in x for x in w)
          and any("our lift guards" in x and "0x5" in x for x in w)
          and any("bound-sense bug" in x for x in w))

    # F3. Identical guards (any count) are clean: a validator checking four
    #     bounds with the same sense censuses as ONE shape on each side.
    w = compare_fcom_guards(_guard((0x41, "jp")) + _guard((0x41, "jp")),
                            _guard((0x41, "jp")))
    check("identical guard shapes (differing count) not flagged", w == [])

    # F4. Inverted-jcc layout of the SAME predicate is still a shape change and
    #     must flag: VC71 recompiling identical source keeps the block layout,
    #     so jp-vs-jnp on one side is a source-structure difference.
    w = compare_fcom_guards(_guard((0x41, "jnp")), _guard((0x41, "jp")))
    check("jcc inversion flagged", len(w) >= 2)

    # F5. testw on %ax censuses via the high byte: $0x4100,%ax == $0x41,%ah.
    cand = ["fcomps 0x0", "fnstsw %ax", "testw $0x4100, %ax",
            "jp 0x10 <LAB_00000010>", "retl"]
    w = compare_fcom_guards(cand, _guard((0x41, "jp")))
    check("testw $0x4100,%ax equivalent to testb $0x41,%ah", w == [])

    # F6. Non-guard TEST (integer flags, no fnstsw) is not censused.
    w = compare_fcom_guards(["testb $0x41, %ah", "jp 0x10 <LAB_00000010>"], [])
    check("test without fnstsw not censused", w == [])

    # F7. An sahf-based or non-ZF/PF consumer is skipped, not misdecoded.
    w = compare_fcom_guards(["fnstsw %ax", "sahf", "ja 0x10 <LAB_00000010>"], [])
    check("sahf guard skipped", w == [])

    # --- Per-function chunk boundary detection (first_function_insns) ---
    # These guard the 2026-07-06 fix: vc71 was truncating multi-return and
    # switch functions at the first mid-body ret+label, understating match by
    # 27-50pp (network_connection FUN_001288e0 40.5% -> 90.3%).

    # B1. Multi-return: an early-return `ret` is followed by the alternate-path
    #     block whose label the earlier `je` targets -> INTERNAL, must not cut.
    txt = ("<_FUN_00001000>:\n"
           "   0: test %edi, %edi\n"
           "   2: je 0x8 <LAB_00001008>\n"
           "   4: mov (%edi), %eax\n"
           "   6: ret\n"
           "<LAB_00001008>:\n"
           "   8: xor %eax, %eax\n"
           "   a: ret\n")
    ins = _first_function_insns_from_text(txt, {"FUN_00001000"})
    check("multi-return ret+referenced-label not truncated", ins is not None and len(ins) == 6)

    # B2. Switch: each `case: return;` is a `ret` before the next caseD_ label,
    #     reached only via the indirect jump table -> INTERNAL once the switch is
    #     entered, must not cut.
    txt = ("<_FUN_00002000>:\n"
           "   0: cmp $0x2, %eax\n"
           "   3: ja 0x30 <switchD_00002010::default>\n"
           "   5: jmp *0x100(,%eax,4)\n"
           "<switchD_00002010::caseD_0>:\n"
           "   c: mov $0x1, %eax\n"
           "  11: ret\n"
           "<switchD_00002010::caseD_1>:\n"
           "  13: mov $0x2, %eax\n"
           "  18: ret\n"
           "<switchD_00002010::default>:\n"
           "  30: xor %eax, %eax\n"
           "  32: ret\n")
    ins = _first_function_insns_from_text(txt, {"FUN_00002000"})
    check("switch case ret+caseD label not truncated", ins is not None and len(ins) == 9)

    # B3. CRITICAL negative: a GENUINE neighbour slot (unreferenced label, no
    #     entered switch) after our terminating ret MUST still cut -- preserves
    #     the stale-0x2000-export protection.
    txt = ("<_FUN_00003000>:\n"
           "   0: mov (%edi), %eax\n"
           "   2: ret\n"
           "<LAB_00003008>:\n"
           "   8: xor %eax, %eax\n"
           "   a: ret\n")
    ins = _first_function_insns_from_text(txt, {"FUN_00003000"})
    check("genuine neighbour slot still truncated", ins is not None and len(ins) == 2)

    # B4. Back edge: the switch default arm named plain LAB_ (not
    #     switchD_::default), so seen_switches cannot exempt it, and nothing
    #     before the ret references it -- but its block branches BACK to the
    #     shared epilogue label defined earlier, proving one function.
    #     Regression for parse_string 0x19be30: truncated 163 -> 91 insns,
    #     scoring a 197-insn candidate at 50.7% and parking it 7 times.
    txt = ("<_FUN_00004000>:\n"
           "   0: cmp $0x2, %eax\n"
           "   3: jmp *0x100(,%eax,4)\n"
           "<LAB_00004030>:\n"
           "  30: mov %ebx, %eax\n"
           "  32: ret\n"
           "<LAB_00004033>:\n"
           "  33: mov $0x1, %ebx\n"
           "  38: jmp 0x30 <LAB_00004030>\n")
    ins = _first_function_insns_from_text(txt, {"FUN_00004000"})
    check("back edge past ret+unreferenced label not truncated",
          ins is not None and len(ins) == 6)

    # B5. Negative pair for B4: identical shape but the post-ret block jumps
    #     FORWARD to its own label instead of back into our body -> still a
    #     neighbour, must cut.  Pins that B4 keys on the edge direction and not
    #     merely on the presence of a jmp after the ret.
    txt = ("<_FUN_00005000>:\n"
           "   0: mov (%edi), %eax\n"
           "   2: ret\n"
           "<LAB_00005008>:\n"
           "   8: mov $0x1, %ebx\n"
           "   d: jmp 0x20 <LAB_00005020>\n"
           "<LAB_00005020>:\n"
           "  20: xor %eax, %eax\n"
           "  22: ret\n")
    ins = _first_function_insns_from_text(txt, {"FUN_00005000"})
    check("forward-only jump after ret still truncated",
          ins is not None and len(ins) == 2)

    # B6. Negative: a neighbour whose body branches to its OWN entry label must
    #     still cut.  Only labels defined strictly BEFORE the boundary are
    #     back-edge evidence; counting the boundary label itself made
    #     FUN_00113a90 run 57 -> 999 insns through a LAB_-only symbol run with
    #     no hard boundary, each wrong continue feeding the next.
    txt = ("<_FUN_00006000>:\n"
           "   0: mov (%edi), %eax\n"
           "   2: ret\n"
           "<LAB_00006008>:\n"
           "   8: inc %eax\n"
           "   9: cmp $0x4, %eax\n"
           "   c: jbe 0x8 <LAB_00006008>\n"
           "   e: ret\n")
    ins = _first_function_insns_from_text(txt, {"FUN_00006000"})
    check("self-referencing neighbour label still truncated",
          ins is not None and len(ins) == 2)

    # --- Switch jump-table data trimming (_trim_trailing_table_data) ---
    # Guards the 2026-07-28 fix: the byte-index table after FUN_0019c0a0's final
    # ret decoded as `addb $0x4, %al` / `addb %al, (%esp,%eax)`, none of which are
    # in _DATA_GARBAGE_MARKERS, so ~55 phantom insns survived and capped the
    # function at 38.1% against a real 37-insn body (fixed -> 77.5%).

    # T1. Marker-based path still works (addb %al, (%eax) from 00 00 pairs).
    ins = _trim_trailing_table_data(
        ["movl (%eax), %eax", "retl", "addb %al, (%eax)", "addb %al, (%eax)"])
    check("T1 marker-based table trim", len(ins) == 2)

    # T2. NEW: table whose decode matches no marker is still trimmed, because
    #     the body contains a scaled indirect jump (proof of a jump table).
    ins = _trim_trailing_table_data(
        ["jmp\t*0x0(,%edx,4)", "movw $0x6, 0x14(%eax)", "retl",
         "addb $0x4, %al", "addb %al, (%esp,%eax)", "addb $0x2, %al"])
    check("T2 unmarked table trimmed via indirect jmp", len(ins) == 3)

    # T3. CRITICAL negative: NO indirect table jmp and no marker -> do NOT trim.
    #     Prevents the rule from eating a genuine trailing block.
    ins = _trim_trailing_table_data(
        ["movl (%eax), %eax", "retl", "xorl %eax, %eax", "retl"])
    check("T3 no jmp-table evidence -> no trim", len(ins) == 4)

    # T4. Negative: a scaled indirect CALL is not a jump table -> no trim.
    ins = _trim_trailing_table_data(
        ["call\t*0x0(,%edx,4)", "retl", "xorl %eax, %eax", "retl"])
    check("T4 indirect call is not a jump table", len(ins) == 4)

    # --- @reg-defined phantom-slot load stripping (strip_regparam_loads) ---
    # These guard the @reg-DEFINED prologue-ceiling modeling: a byte-faithful
    # lift of a register-param function must recover its match, while every
    # masking pathway (wrong body, wrong slot, other-register load, slot the
    # reference also reads) must stay untouched.

    # RP1. @<eax> leaf: the single phantom load is stripped -> 100%.
    cand = ["movl 0x8(%ebp), %eax", "addl $0x1, %eax", "retl"]
    ref = ["addl $0x1, %eax", "retl"]
    stripped, n = strip_regparam_loads(cand, ref, [(0, 'eax')])
    check("RP1 @eax leaf load stripped", n == 1 and len(stripped) == 2)
    pct = compare_functions(cand, ref, regdef_params=[(0, 'eax')])[0]
    check("RP1 @eax leaf recovers to 100", pct == 100.0)

    # RP2. Cascade remains honest: an extra candidate spill is NOT stripped;
    #      modeled improves over raw but stays < 100.
    cand = ["movl 0x8(%ebp), %eax", "pushl %eax", "movl %edi, -0x4(%ebp)",
            "calll 0x0", "retl"]
    ref = ["pushl %eax", "calll 0x0", "retl"]
    raw = compare_functions(cand, ref)[0]
    modeled = compare_functions(cand, ref, regdef_params=[(0, 'eax')])[0]
    check("RP2 cascade spill survives, raw < modeled < 100",
          raw < modeled < 100.0)

    # --- operand scoring: max(canonical, identity) register mapping ---------
    # OP1. Same roles, DIFFERENT registers, same introduction order:
    #      canonicalization wins and identity would under-report.
    cand = ["movl %eax, 0x4(%esp)", "addl %ecx, %eax", "retl"]
    ref = ["movl %ebx, 0x4(%esp)", "addl %edx, %ebx", "retl"]
    # NOTE: lcs_ratio returns a 0..1 fraction; compare_functions returns 0..100.
    check("OP1 canonical rescues pure register substitution",
          lcs_ratio(extract_normalized_sequence(cand),
                    extract_normalized_sequence(ref)) == 1.0)
    check("OP1 identity alone would score it lower",
          lcs_ratio(extract_identity_sequence(cand),
                    extract_identity_sequence(ref)) < 1.0)
    check("OP1 compare_functions takes the max",
          compare_functions(cand, ref, reg_normalize=True)[0] == 100.0)

    # OP2. SAME registers, different introduction ORDER: canonicalization
    #      shifts every alias and collapses; identity is exact.  This is the
    #      FUN_00017ab0 signature (17.1% canonical vs 58.8% identity).
    cand = ["movl %eax, (%esp)", "movl %ebx, 0x4(%esp)", "retl"]
    ref = ["movl %ebx, 0x4(%esp)", "movl %eax, (%esp)", "retl"]
    canon = lcs_ratio(extract_normalized_sequence(cand),
                      extract_normalized_sequence(ref))
    ident = lcs_ratio(extract_identity_sequence(cand),
                      extract_identity_sequence(ref))
    check("OP2 identity beats canonical on order divergence", ident > canon)
    check("OP2 compare_functions takes the max",
          abs(compare_functions(cand, ref, reg_normalize=True)[0]
              - ident * 100.0) < 1e-9)

    # OP3. The max is never below either mapping alone (lower-bound property).
    for a, b in ((["movl %eax, %ecx", "retl"], ["movl %esi, %edi", "retl"]),
                 (["addl %eax, %ebx", "subl %ecx, %edx", "retl"],
                  ["subl %ecx, %edx", "addl %eax, %ebx", "retl"])):
        got = compare_functions(a, b, reg_normalize=True)[0]
        check("OP3 max >= both mappings",
              got >= lcs_ratio(extract_normalized_sequence(a),
                               extract_normalized_sequence(b)) * 100.0 - 1e-9
              and got >= lcs_ratio(extract_identity_sequence(a),
                                   extract_identity_sequence(b)) * 100.0 - 1e-9)

    # OP4. Mnemonic path is untouched by the operand change.
    cand = ["movl %eax, %ecx", "retl"]
    check("OP4 mnemonic-only path unaffected",
          compare_functions(cand, ["movl %esi, %edi", "retl"])[0] == 100.0)

    # RP3. @<ax> word param: movswl widening load into %eax is family(eax)
    #      -> stripped.
    cand = ["movswl 0x8(%ebp), %eax", "pushl %eax", "retl"]
    ref = ["movswl %ax, %eax", "pushl %eax", "retl"]
    stripped, n = strip_regparam_loads(cand, ref, [(0, 'ax')])
    check("RP3 @ax movswl widening load stripped", n == 1)

    # RP4a. No regdef -> no-op even with a 0x8(%ebp) load present.
    cand = ["movl 0x8(%ebp), %eax", "retl"]
    stripped, n = strip_regparam_loads(cand, ["retl"], None)
    check("RP4a no regdef is a no-op", n == 0 and stripped == cand)

    # RP4b. Load into a DIFFERENT register than annotated is NOT stripped --
    #       its mov mnemonic already aligns with the reference's reg-to-reg move.
    cand = ["movl 0x8(%ebp), %esi", "retl"]
    ref = ["movl %eax, %esi", "retl"]
    stripped, n = strip_regparam_loads(cand, ref, [(0, 'eax')])
    check("RP4b other-register load not stripped", n == 0)
    pct = compare_functions(cand, ref, regdef_params=[(0, 'eax')])[0]
    check("RP4b mnemonics still align to 100", pct == 100.0)

    # RP5. Genuinely wrong body still fails after stripping.
    cand = ["movl 0x8(%ebp), %eax", "xorl %ecx, %ecx", "subl $0x4, %esp",
            "movl %ecx, (%esp)", "retl"]
    ref = ["pushl %esi", "pushl %eax", "calll 0x0", "addl $0x8, %esp",
           "popl %esi", "retl"]
    modeled = compare_functions(cand, ref, regdef_params=[(0, 'eax')])[0]
    check("RP5 wrong body still fails (<60%)", modeled < 60.0)

    # RP6. Count-aware: the reference ALSO loads the slot (mis-annotation or
    #      aliasing) -> excess 0, nothing stripped.
    cand = ["movl 0x8(%ebp), %eax", "retl"]
    ref = ["movl 0x8(%ebp), %eax", "retl"]
    stripped, n = strip_regparam_loads(cand, ref, [(0, 'eax')])
    check("RP6 ref-also-loads slot -> excess 0", n == 0)

    # RP7. Wrong slot (disp mismatch) is never stripped.
    cand = ["movl 0xc(%ebp), %eax", "retl"]
    stripped, n = strip_regparam_loads(cand, ["retl"], [(0, 'eax')])
    check("RP7 wrong-slot load not stripped", n == 0)

    # RP8. Second param @<esi> maps to disp 0xc; SIB/indexed source rejected.
    cand = ["movl 0xc(%ebp), %esi", "movl 0x8(%ebp,%eax,4), %edi", "retl"]
    stripped, n = strip_regparam_loads(cand, ["retl"], [(1, 'esi'), (0, 'edi')])
    check("RP8 disp 0xc second param stripped, SIB rejected",
          n == 1 and stripped[0] == "movl 0x8(%ebp,%eax,4), %edi")

    # RP9. Monotonicity fallback: when the phantom load's mnemonic was already
    #      aligned (against a reference reg-to-reg move into the same register),
    #      stripping would LOWER the mnemonic ratio -- modeled must fall back
    #      to raw, never below it.
    cand = ["movl 0x8(%ebp), %eax", "retl"]
    ref = ["movl %edx, %eax", "retl"]
    raw = compare_functions(cand, ref)[0]
    modeled = compare_functions(cand, ref, regdef_params=[(0, 'eax')])[0]
    check("RP9 modeled never below raw (max fallback)",
          raw == 100.0 and modeled == raw)

    if failures:
        print(f"\nSELF-TEST FAILED: {len(failures)} case(s)")
        sys.exit(1)
    print("\nSELF-TEST PASSED")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("compiled", nargs="?", help="Compiled .obj file")
    ap.add_argument("reference", nargs="?", help="Delinked reference .obj file")
    ap.add_argument("--function", "-f", help="Compare only this function")
    ap.add_argument("--threshold", "-t", type=float, default=50.0,
                    help="Minimum match %% to pass (default: 50)")
    ap.add_argument("--show-diffs", "-d", action="store_true",
                    help="Show instruction-level diffs")
    ap.add_argument("--fpu-only", action="store_true",
                    help="Only report FPU operand warnings")
    ap.add_argument("--loadw-only", action="store_true",
                    help="Only report load-width (int vs int16/int8) warnings")
    ap.add_argument("--imm-only", action="store_true",
                    help="Only report immediate-constant (wrong float/magic literal) warnings")
    ap.add_argument("--fcom-only", action="store_true",
                    help="Only report FPU-guard bound-sense (<= vs <) warnings")
    ap.add_argument("--reg-normalize", "-r", action="store_true",
                    help="Use register-alias normalization for LCS (canonical operands)")
    ap.add_argument("--regdef-params", default=None,
                    help="Model @<reg>-DEFINED params for the compared function(s): "
                         "'idx:reg[,idx:reg]' e.g. '0:eax' -- strips candidate-only "
                         "phantom-slot loads (intended for --function runs; "
                         "vc71_verify derives this from kb.json automatically)")
    ap.add_argument("--self-test", action="store_true",
                    help="Run built-in detector self-tests and exit")
    args = ap.parse_args()

    regdef_params = None
    if args.regdef_params:
        regdef_params = []
        for tok in args.regdef_params.split(","):
            idx, _, reg = tok.partition(":")
            regdef_params.append((int(idx.strip()), reg.strip().lower()))

    if args.self_test:
        _self_test()
        return

    if not args.compiled or not args.reference:
        ap.error("compiled and reference are required (unless --self-test)")

    compiled_funcs = disassemble(args.compiled)
    reference_funcs = disassemble(args.reference)

    matched = set(compiled_funcs.keys()) & set(reference_funcs.keys())

    # Delinked XDK objects may preserve C++ namespace-qualified names while
    # our C implementations use the unqualified function name.
    namespace_map = {}
    for ref_name in reference_funcs:
        short_name = ref_name.rsplit("::", 1)[-1]
        if short_name != ref_name and short_name in compiled_funcs:
            namespace_map[short_name] = ref_name
            if ref_name not in matched:
                compiled_funcs[ref_name] = compiled_funcs[short_name]
                matched.add(ref_name)
    # Build rename map: when a function was renamed from FUN_xxx, map
    # the new name to the old FUN_xxx name for matching against references.
    rename_map = {}
    if compiled_funcs.keys() - matched:
        try:
            import json
            kb_path = os.path.join(os.path.dirname(os.path.dirname(_tools_dir)), 'kb.json')
            if not os.path.exists(kb_path):
                kb_path = os.path.join(_tools_dir, '..', 'kb.json')
            with open(kb_path) as kf:
                kb = json.load(kf)
            if isinstance(kb, dict):
                if 'objects' in kb:
                    for obj in kb.get('objects', []):
                        for fn_entry in obj.get('functions', []):
                            addr = fn_entry.get('addr', '')
                            decl = fn_entry.get('decl', '')
                            import re as _re
                            m = _re.search(r'\b(\w+)\s*\(', decl)
                            if m and addr:
                                declared_name = m.group(1)
                                fun_name = f"FUN_{int(addr, 16):08x}"
                                if declared_name != fun_name:
                                    rename_map[declared_name] = fun_name
                else:
                    for addr_str, fn_entry in kb.items():
                        if isinstance(fn_entry, dict):
                            declared_name = fn_entry.get('name')
                            if declared_name and addr_str.startswith('0x'):
                                try:
                                    fun_name = f"FUN_{int(addr_str, 16):08x}"
                                    if declared_name != fun_name:
                                        rename_map[declared_name] = fun_name
                                except ValueError:
                                    pass
        except Exception:
            pass

    if args.function:
        fn = args.function.lstrip("_")
        if fn not in matched:
            namespace_name = namespace_map.get(fn)
            if namespace_name and namespace_name in reference_funcs and fn in compiled_funcs:
                compiled_funcs[namespace_name] = compiled_funcs[fn]
                matched = {namespace_name}
                fn = namespace_name
            else:
            # Try rename fallback: look up old FUN_xxx name in reference
                old_name = rename_map.get(fn)
                if old_name and old_name in reference_funcs and fn in compiled_funcs:
                    compiled_funcs[old_name] = compiled_funcs[fn]
                    matched = {old_name}
                    fn = old_name
                else:
                    print(f"Function {fn} not found in both objects")
                    print(f"  compiled:  {sorted(compiled_funcs.keys())[:10]}")
                    print(f"  reference: {sorted(reference_funcs.keys())[:10]}")
                    sys.exit(1)
        matched = {fn}

    # Apply rename map for unmatched compiled functions only for whole-object
    # comparisons. --function must stay restricted to the requested symbol.
    if not args.function:
        for new_name, old_name in rename_map.items():
            if new_name in compiled_funcs and old_name in reference_funcs and new_name not in matched:
                compiled_funcs[old_name] = compiled_funcs[new_name]
                matched.add(old_name)

    if not matched:
        print("No matching functions found between objects")
        print(f"  compiled:  {sorted(compiled_funcs.keys())[:10]}")
        print(f"  reference: {sorted(reference_funcs.keys())[:10]}")
        sys.exit(1)

    any_fail = False
    any_fpu_warn = False
    any_loadw_warn = False
    any_imm_warn = False
    any_fcom_warn = False

    for fn in sorted(matched):
        pct, diffs, fpu_warnings, loadw_warnings, imm_warnings, fcom_warnings = compare_functions(
            compiled_funcs[fn], reference_funcs[fn], reg_normalize=args.reg_normalize,
            regdef_params=regdef_params)
        n_c = len(compiled_funcs[fn])
        n_r = len(reference_funcs[fn])
        status = "PASS" if pct >= args.threshold else "FAIL"
        fpu_tag = " [FPU-WARN]" if fpu_warnings else ""
        loadw_tag = " [LOADW-WARN]" if loadw_warnings else ""
        imm_tag = " [IMM-WARN]" if imm_warnings else ""
        fcom_tag = " [FCOM-WARN]" if fcom_warnings else ""
        trunc_tag = " [REF-TRUNCATED?]" if n_r > 0 and n_c > n_r * 1.4 else ""

        # When reg-normalize is on, also compute mnemonic % to show the gap
        reg_tag = ""
        if args.reg_normalize:
            mnem_pct = compare_functions(compiled_funcs[fn], reference_funcs[fn],
                                         reg_normalize=False)[0]
            reg_tag = f" [struct:{mnem_pct:.1f}%]"

        only_mode = args.fpu_only or args.loadw_only or args.imm_only or args.fcom_only
        if not only_mode:
            print(f"  {status} {fn}: {pct:.1f}% match ({n_c}/{n_r} insns){reg_tag}{fpu_tag}{loadw_tag}{imm_tag}{fcom_tag}{trunc_tag}")

        if fpu_warnings:
            any_fpu_warn = True
            if not args.loadw_only and not args.imm_only and not args.fcom_only:
                if args.fpu_only:
                    print(f"  {fn}:{fpu_tag}")
                for w in fpu_warnings:
                    print(w)

        if loadw_warnings:
            any_loadw_warn = True
            if not args.fpu_only and not args.imm_only and not args.fcom_only:
                if args.loadw_only:
                    print(f"  {fn}:{loadw_tag}")
                for w in loadw_warnings:
                    print(w)

        if imm_warnings:
            any_imm_warn = True
            if not args.fpu_only and not args.loadw_only and not args.fcom_only:
                if args.imm_only:
                    print(f"  {fn}:{imm_tag}")
                for w in imm_warnings:
                    print(w)

        if fcom_warnings:
            any_fcom_warn = True
            # Detail lines are dense in FPU-heavy TUs (45 of real_math's 171
            # functions carry structural comparison divergence), so -- like
            # LOADW -- only expand them in the dedicated --fcom-only mode; the
            # compact [FCOM-WARN] tag on the status line is the hint.
            if args.fcom_only:
                print(f"  {fn}:{fcom_tag}")
                for w in fcom_warnings:
                    print(w)

        if status == "FAIL":
            any_fail = True

        if args.show_diffs and diffs and not only_mode:
            for d in diffs:
                print(d)

    if any_fpu_warn and not args.loadw_only and not args.imm_only and not args.fcom_only:
        print("\nWARNING: FPU operand-order differences detected.")
        print("Check cross-product argument order and FSUB/FSUBR operand direction.")

    if any_loadw_warn and not args.fpu_only and not args.imm_only and not args.fcom_only:
        print("\nWARNING: load-width (int vs int16_t/int8_t) differences detected.")
        print("A field the original narrows (movsx/movzx word/byte) is read wider in our lift,")
        print("or vice versa. Verify the C type against disassembly. See lift-learnings 'int vs int16_t'.")

    if any_imm_warn and not args.fpu_only and not args.loadw_only and not args.fcom_only:
        print("\nWARNING: immediate-constant differences detected.")
        print("A large inline constant (float bit-pattern or magic) differs between our lift and the")
        print("original. Since both are VC71 codegen, this is a source-literal mismatch -- verify the")
        print("numeric literal against the disassembly immediate. See lift-learnings 'immediate-constant'.")

    if any_fcom_warn and not args.fpu_only and not args.loadw_only and not args.imm_only:
        if args.fcom_only:
            print("\nWARNING: FPU-guard bound-sense differences detected.")
            print("A float comparison's TEST/Jcc guard shape differs between our lift and the original.")
            print("Both are VC71 codegen, so this is a source comparison-form mismatch (<= lifted as <,")
            print(">= as >, swapped operands, or positive vs negated form). See lift-learnings section 38.")
        else:
            print("\n[FCOM-WARN] FPU-guard bound-sense differences found; re-run with --fcom-only for "
                  "details (<= vs <; see lift-learnings section 38).")

    sys.exit(1 if any_fail else 0)


if __name__ == "__main__":
    main()
