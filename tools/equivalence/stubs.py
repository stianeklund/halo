"""Relocation classification, patching, and callee stubbing for non-leaf emulation.

Provides:
  - classify_relocations(): categorize a function's relocations
  - patch_dir32_relocs(): rewrite DIR32 relocations to globals region
  - patch_rel32_calls(): rewrite CALL targets to sentinel addresses
  - StubManager: intercept calls via Unicorn hooks, run callees in sub-emulator
  - StubArgTracer / compare_stub_arg_traces: record and diff per-stub call args
"""

import math
import os
import struct
import json
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional, List, Dict, Tuple

IMAGE_REL_I386_DIR32 = 0x0006
IMAGE_REL_I386_REL32 = 0x0014

GLOBALS_BASE = 0x00500000
GLOBALS_SIZE = 0x00100000  # 1 MB


def _st80_to_double(b: bytes) -> float:
    """Convert 10-byte x87 extended precision to Python float."""
    mantissa = int.from_bytes(b[:8], 'little')
    exp_sign = int.from_bytes(b[8:10], 'little')
    sign = -1 if (exp_sign >> 15) else 1
    exp = exp_sign & 0x7FFF
    if exp == 0 and mantissa == 0:
        return 0.0
    if exp == 0x7FFF:
        return float('inf') * sign if mantissa == (1 << 63) else float('nan')
    power = exp - 16383 - 63
    try:
        return sign * mantissa * (2.0 ** power)
    except OverflowError:
        return float('inf') * sign


def _write_st0_double(uc, val: float):
    """Write a Python float to Unicorn's ST0 as x87 80-bit extended."""
    from unicorn.x86_const import UC_X86_REG_ST0
    bits = struct.unpack('<Q', struct.pack('<d', val))[0]
    sign = (bits >> 63) & 1
    exp = (bits >> 52) & 0x7FF
    frac = bits & ((1 << 52) - 1)
    if exp == 0 and frac == 0:
        ext = sign << 79
    elif exp == 0x7FF:
        ext = (sign << 79) | (0x7FFF << 64) | (1 << 63) | (frac << 11)
    else:
        ext = (sign << 79) | ((exp - 1023 + 16383) << 64) | (1 << 63) | (frac << 11)
    uc.reg_write(UC_X86_REG_ST0, ext)


# ---------------------------------------------------------------------------
# Stub argument tracing
# ---------------------------------------------------------------------------

# Stack region bounds (must match unicorn_diff.py constants).
_STACK_BASE = 0x00100000
_STACK_TOP  = 0x00200000  # STACK_BASE + STACK_SIZE (1 MB)


@dataclass
class StubCallRecord:
    """Arguments captured for one stub invocation."""
    seq: int           # 0-based call sequence index within this run
    callee_addr: int   # sentinel address
    callee_name: str   # resolved callee name (from _stub_names)
    args: List[int]    # dword values: reg-args first, then stack-args (ESP+4, ESP+8, …)
    is_varargs: bool   # True when callee has a '...' param
    # Return address captured at intercept time, i.e. call_site + 5.  Identifies
    # WHO made the call: inside the target's own byte extent, or inside a callee
    # body the emulator is executing natively.  0 = not captured (older records,
    # or an unreadable stack) and must never be treated as "outside".
    caller_addr: int = 0


@dataclass
class StubArgTracer:
    """Accumulates StubCallRecords for one emulator run (oracle or candidate).

    Attach via StubManager.set_tracer() before a run, then read .records after.
    """
    records: List[StubCallRecord] = field(default_factory=list)
    # (start, end) byte extent of the TARGET function in this run's address
    # space.  Set by the runner; when present, compare_stub_arg_traces drops
    # records whose caller_addr falls outside it -- those calls were made by a
    # natively-executed callee, not by the function under test.  None = no
    # attribution available, compare everything (historical behaviour).
    target_range: Optional[Tuple[int, int]] = None

    def reset(self):
        self.records = []


@dataclass
class StubArgDiff:
    """Result of comparing oracle and candidate StubArgTracer records."""
    # Total stubs executed (from oracle run)
    total_calls: int
    # Number of arg-value mismatches (excludes soft-matched stack-ptr pairs)
    arg_mismatches: int
    # Number of arg positions where BOTH sides are stack pointers (soft match)
    soft_stack_ptr_matches: int
    # Detailed mismatch records: (seed_label, seq, callee_name, arg_pos, oracle_val, cand_val)
    details: List[tuple]
    # True when call sequences diverge (different length or different callee order)
    sequence_diverged: bool
    sequence_diverge_index: int  # index where divergence first occurs (-1 if none)
    # Args excused by a callee-contract rule, counted per reason so an
    # exemption is always visible in the summary rather than silently dropped.
    soft_semantic_matches: int = 0
    soft_reasons: Dict[str, int] = field(default_factory=dict)
    # The two callee-name sequences, kept so a divergence can actually be
    # adjudicated. Recording only the index (as this did) says a divergence
    # happened but not what it was, and the two cases have opposite meanings: a
    # SHIFTED sequence (one side has an extra or missing call, the rest lining up
    # after the offset) is a comparator-alignment artifact, whereas a genuinely
    # DIFFERENT callee at the same position is a real control-flow bug. On the
    # 07-29 batch this was 71 of 186 failures -- the largest single class -- and
    # none of them could be told apart from the log alone.
    oracle_seq: List[str] = field(default_factory=list)
    candidate_seq: List[str] = field(default_factory=list)
    # Records dropped per side because caller_addr fell outside that side's
    # target extent, i.e. the call was made by a natively-executed callee and
    # belongs to that callee's own equivalence target, not this one. Reported
    # rather than silently swallowed: a large count means the two sides are
    # executing very different amounts of code, which is context a reader of
    # the divergence needs.
    nested_dropped_oracle: int = 0
    nested_dropped_candidate: int = 0

    def has_differences(self) -> bool:
        return self.sequence_diverged or self.arg_mismatches > 0

    def sequence_relation(self):
        """Classify HOW the two sequences differ: -> (kind, description).

        Three outcomes, and only the last is a lift bug:

        ``truncated``  the shorter sequence is a PREFIX of the longer -- one side
                       simply stopped making calls. Both sides agree on every
                       call they both made, so this is one side ending early
                       (oracle crash, early return, instruction limit), not a
                       control-flow difference. This is the common case: of six
                       call_seq targets sampled on 07-29, four were this.
        ``shifted``    one side has an extra call in the MIDDLE and the tail
                       still lines up. The comparison then walks two lists off
                       by one, so every later position reports as divergent.
                       An alignment artifact.
        ``divergent``  neither -- the two sides genuinely call different things
                       at the same position. A real control-flow difference.
                       Example: FUN_000d6cc0, where the candidate calls
                       _display_assert where the oracle calls
                       _global_scenario_get.

        Prefix is tested first because it subsumes an extra call at either END,
        and 'one side stopped' is the more accurate description of that shape
        than 'off by one'.
        """
        o, c = self.oracle_seq, self.candidate_seq
        if o == c:
            return (None, "")
        longer, shorter, side = (o, c, "oracle") if len(o) > len(c) else (c, o, "candidate")
        if longer[:len(shorter)] == shorter:
            return ("truncated",
                    "%s stopped after %d call(s); %s continued to %d -- both agree "
                    "on every shared call, so one side ended early rather than "
                    "diverging" % ("oracle" if side == "candidate" else "candidate",
                                   len(shorter), side, len(longer)))
        if len(longer) - len(shorter) == 1:
            for k in range(len(longer)):
                if longer[:k] + longer[k + 1:] == shorter:
                    return ("shifted",
                            "%s has one extra call at index %d (sequences identical "
                            "without it) -- alignment artifact, not a control-flow "
                            "difference" % (side, k))
        first = next((i for i, (a, b) in enumerate(zip(o, c)) if a != b), min(len(o), len(c)))
        # One side had calls attributed away as nested while the other had none:
        # the two sides resolved the same callee differently -- one CALLed it (so
        # its inner calls are nested and dropped), the other INLINED it (so the
        # identical inner calls issue from within the target's own extent and are
        # kept). The surviving sequences are then not comparable, and the
        # difference is not evidence about the target. object_get_node_matrix is
        # the worked example: both sides end at the SAME assert (line 0x424),
        # but the candidate shows the inlined helper's datum_get/tag_get/tag_get
        # first while the oracle's three equivalents were dropped as nested.
        if bool(self.nested_dropped_oracle) != bool(self.nested_dropped_candidate):
            return ("inline-asymmetry",
                    "one side CALLed a callee the other INLINED (nested dropped: "
                    "oracle=%d candidate=%d), so the surviving sequences are not "
                    "comparable; first difference at index %d: oracle=%s "
                    "candidate=%s" % (
                        self.nested_dropped_oracle,
                        self.nested_dropped_candidate, first,
                        o[first] if first < len(o) else "(end)",
                        c[first] if first < len(c) else "(end)"))
        return ("divergent",
                "callees differ at index %d: oracle=%s candidate=%s -- real "
                "control-flow difference" % (
                    first,
                    o[first] if first < len(o) else "(end)",
                    c[first] if first < len(c) else "(end)"))

    def sequence_detail(self, window: int = 4) -> List[str]:
        """Human-readable lines describing a sequence divergence.

        Emitted on its own lines, never folded into summary(): triage_failures
        regexes on the exact `call-seq diverged at index N` text in summary(),
        so that string has to stay byte-stable.
        """
        if not self.sequence_diverged:
            return []
        i = max(0, self.sequence_diverge_index - window)
        j = self.sequence_diverge_index + window + 1
        lines = [
            "  call-seq oracle   [%d..%d] len=%d: %s"
            % (i, min(j, len(self.oracle_seq)), len(self.oracle_seq),
               " ".join(self.oracle_seq[i:j]) or "(empty)"),
            "  call-seq candidate[%d..%d] len=%d: %s"
            % (i, min(j, len(self.candidate_seq)), len(self.candidate_seq),
               " ".join(self.candidate_seq[i:j]) or "(empty)"),
        ]
        kind, desc = self.sequence_relation()
        if kind:
            lines.append("  call-seq %s: %s" % (kind.upper(), desc))
        if self.nested_dropped_oracle or self.nested_dropped_candidate:
            lines.append(
                "  call-seq NESTED-DROPPED oracle=%d candidate=%d "
                "(calls made by natively-executed callees, attributed away)"
                % (self.nested_dropped_oracle, self.nested_dropped_candidate))
        return lines

    def summary(self) -> str:
        parts = []
        if self.sequence_diverged:
            parts.append(f"call-seq diverged at index {self.sequence_diverge_index}")
        if self.arg_mismatches:
            parts.append(f"{self.arg_mismatches} arg mismatch(es)")
        if self.soft_stack_ptr_matches:
            parts.append(f"{self.soft_stack_ptr_matches} stack-ptr arg(s) soft-matched")
        for reason, n in sorted(self.soft_reasons.items()):
            parts.append(f"{n} {reason} arg(s) soft-matched")
        if not parts:
            parts.append("ok")
        return "stub-args: " + ", ".join(parts)


_GLOBALS_ARENA_TOP = None


def _globals_arena_top() -> int:
    """Top of the synthetic DIR32 slot arena actually in use this run."""
    if _GLOBALS_ARENA_TOP is None:
        return GLOBALS_BASE + GLOBALS_SIZE
    return max(_GLOBALS_ARENA_TOP, GLOBALS_BASE + GLOBALS_SIZE)


def set_globals_arena_top(top: int) -> None:
    """Record the highest DIR32 slot address allocated for either side."""
    global _GLOBALS_ARENA_TOP
    if _GLOBALS_ARENA_TOP is None or top > _GLOBALS_ARENA_TOP:
        _GLOBALS_ARENA_TOP = top


def reset_globals_arena_top() -> None:
    global _GLOBALS_ARENA_TOP
    _GLOBALS_ARENA_TOP = None


# {oracle_slot_address: original_XBE_address} for this run's oracle DIR32
# slots, derived from the pristine XBE (see _xbe_dir32_symbol_addrs).  Only the
# oracle side: the candidate's slots sit strictly above every oracle slot, so
# an address landing on one of these keys is unambiguously the oracle's.
def _cva_ok(addr) -> bool:
    """True for a kb.json 'addr' string we can turn into an int."""
    if not isinstance(addr, str) or not addr:
        return False
    try:
        int(addr, 16)
    except ValueError:
        return False
    return True


_GLOBALS_SLOT_REAL = {}
# {(slot - real_address) & 0xffffffff} -- the fixed displacement between the
# two address spaces for each mapped global.
_GLOBALS_SLOT_DISPLACEMENTS = set()


def set_globals_slot_real_map(mapping: dict) -> None:
    """Record {slot_address: original_XBE_address} for oracle DIR32 slots."""
    _GLOBALS_SLOT_REAL.clear()
    _GLOBALS_SLOT_REAL.update(mapping or {})
    _GLOBALS_SLOT_DISPLACEMENTS.clear()
    _GLOBALS_SLOT_DISPLACEMENTS.update(
        (slot - real) & 0xFFFFFFFF for slot, real in _GLOBALS_SLOT_REAL.items())


def reset_globals_slot_real_map() -> None:
    _GLOBALS_SLOT_REAL.clear()
    _GLOBALS_SLOT_DISPLACEMENTS.clear()


def _is_slot_vs_real_global(o_val: int, c_val: int) -> bool:
    """True when both sides passed the address of the SAME global, expressed in
    the two different address spaces the harness gives them.

    ``&some_global`` is a DIR32 reloc in the oracle, so patch_dir32_relocs
    rewrites it to a 256-byte slot and the oracle pushes that slot address.
    The candidate reaches the same global through an absolute immediate and
    pushes its real XBE address.  Both are correct and denote one object;
    numerically they never match.  input_flush pushed three of them into
    csmemset and reported 150 arg mismatches across 50 seeds -- 0x500000 vs
    0x46ba4c, 0x500100 vs 0x46bb38, 0x500200 vs 0x46bba0 (2026-07-28).

    _is_stack_ptr already excuses the case where BOTH values land in the slot
    arena; this is the asymmetric one.

    The test is on the DISPLACEMENT, not on a range: the pair is excused only
    when ``o_val - c_val`` equals ``slot - real_address`` for a global this
    function actually relocates.  That keeps it sound -- the two sides must
    have applied the SAME offset to the same object, so a wrong index is still
    reported -- while covering indices that leave the slot's 256-byte window.
    They routinely do, and not only upward: game_engine_clear_goal_position
    computes ``0x4566f8 + (short)index * 0x20``, so a negative index lands
    BELOW GLOBALS_BASE and a range check excused only the 5 non-negative seeds
    out of 41 (2026-07-28).
    """
    if not _GLOBALS_SLOT_DISPLACEMENTS:
        return False
    return ((o_val - c_val) & 0xFFFFFFFF) in _GLOBALS_SLOT_DISPLACEMENTS


def _is_stack_ptr(v: int) -> bool:
    """Return True when v looks like a pointer into the emulated stack region,
    OR into the synthetic globals/rdata slot region (GLOBALS_BASE..+SIZE).
    Both are allocator-numbered scratch regions: the oracle and candidate
    each assign their own slot addresses independently (e.g. two string
    literal args to display_assert), so the same conceptual pointer differs
    numerically between the two sides for reasons unrelated to correctness.

    The candidate's slots are placed ABOVE the oracle's
    (lft_globals_base = GLOBALS_BASE + len(orc_data_slots) * 256), so with
    enough oracle slots the candidate arena runs past GLOBALS_BASE+GLOBALS_SIZE.
    Using the nominal top here made every such candidate slot look like a hard
    mismatch. set_globals_arena_top() raises the ceiling to what was actually
    allocated."""
    return (_STACK_BASE <= v < _STACK_TOP
            or GLOBALS_BASE <= v < _globals_arena_top())


def _is_chkstk_name(name: str) -> bool:
    """Return True for calls that are not comparable across oracle/candidate:
    MSVC stack-probe aliases, and the system_exit/halt_and_catch_fire halt
    path.  The latter is a documented, accepted quirk (see
    feedback_system_exit_vs_hcf.md): the oracle's assert macro compiles to a
    direct CALL to halt_and_catch_fire(void) (0x1029a0) with no argument,
    while correct lifted C calls system_exit(-1) through its thunk at
    0x8e2f0 — different callee address AND arg count for the same halt
    intent. Since both are __noreturn, nothing after them is observable."""
    key = name.lstrip("_").lower()
    return key in ("chkstk", "fun_001d90e0", "system_exit", "halt_and_catch_fire", "fun_001029a0")


# MSVC compiler-runtime intrinsics (see the intrinsic table in CLAUDE.md).
# The MSVC-built oracle CALLs these; correct lifted C writes the equivalent
# idiom ((int)expr, (int64_t)a*b, ...) which clang INLINES — so the intrinsic
# call appears only in the oracle trace and a call-seq compare flags a bogus
# divergence (falsely failed FUN_0018dcf0 on all 100 seeds, 2026-07-04: oracle
# called _ftol2 @0x1d9068 after tag_block_get_element; candidate inlined the
# float->int conversion). Filtering them from BOTH sides is safe: they are pure
# computation with results in registers/stack, so any wrong inline math still
# surfaces in the return-value and mem-trace comparison.
_INLINED_INTRINSIC_KEYS = frozenset((
    "ftol2",       "fun_001d9068",
    "allmul",      "fun_001dd620",
    "aullshr",     "fun_001dd660",
    "aullrem",     "fun_001dd680",
    "aulldiv",     "fun_001dd770",
    "seh_prolog",  "fun_001dd5c8",
    "seh_epilog",  "fun_001dd601",
    # CRT two-arg FP intrinsic dispatch thunk: MOV EDX,0x3312d0; JMP
    # __cintrindisp2 (fmod/atan2/pow dispatcher). MSVC emits a CALL here;
    # clang inlines the x87 sequence (e.g. FPREM1 for fmod). Sole
    # __cintrindisp2 thunk in the XBE (verified via Ghidra xrefs 2026-07-04).
    "fun_001daf7e",
))


def _is_inlined_intrinsic_name(name: str) -> bool:
    return name.lstrip("_").lower() in _INLINED_INTRINSIC_KEYS


# display_assert(const char *reason, const char *filepath, int lineno, bool halt)
#
# Args 0-2 are compile-time metadata about the assert site, not computed
# values, and they differ between oracle and candidate by construction: our
# lifted sources do not reproduce the original's line numbering (a real
# observed pair is oracle=0xa89 candidate=0x374), and our string literals land
# in a different section than the original's. 101 of the 331 divergences in the
# 2026-07-28 batch were nothing but this.
#
# Arg 3 (`halt`) is behavioural — whether the assert aborts — so it is still
# compared, as is the call sequence itself. An assert that fires on one side
# and not the other still shows up as a sequence divergence.
_ASSERT_METADATA_CALLEES = frozenset((
    "display_assert", "fun_0008d9f0", "assert", "assert_halt",
))
_ASSERT_METADATA_ARGS = frozenset((0, 1, 2))


def _is_assert_metadata_arg(name: str, arg_index: int) -> bool:
    return (name.lstrip("_").lower() in _ASSERT_METADATA_CALLEES
            and arg_index in _ASSERT_METADATA_ARGS)


# csmemset(void *buffer, int c, size_t size) — `c` is converted to unsigned
# char, so only its low byte is observable: memset(p, -1, n) and
# memset(p, 0xff, n) write identical bytes. All 92 fill-argument divergences in
# the 2026-07-28 batch were exactly that pair (oracle 0xffffffff / candidate
# 0xff). The literal is still structurally wrong and VC71's [IMM-WARN] reports
# it; it is not a behavioural difference.
_BYTE_FILL_CALLEES = frozenset(("csmemset", "memset", "fun_0008db80"))
_BYTE_FILL_ARG = 1


def _is_byte_fill_equivalent(name: str, arg_index: int, o_val: int, c_val: int) -> bool:
    return (name.lstrip("_").lower() in _BYTE_FILL_CALLEES
            and arg_index == _BYTE_FILL_ARG
            and (o_val & 0xFF) == (c_val & 0xFF))


def compare_stub_arg_traces(oracle_tracer: StubArgTracer,
                             cand_tracer: StubArgTracer,
                             seed_label: str = "",
                             comparable_sentinels: Optional[set] = None
                             ) -> StubArgDiff:
    """Compare oracle and candidate StubArgTracer records for one seed.

    Rules:
      - Call sequences must have the same length and callee order.
      - Per argument: exact compare, EXCEPT when BOTH oracle and candidate
        values are stack pointers (STACK_BASE..STACK_TOP) — those differ due
        to legitimate frame-layout differences and count as soft matches.
      - Varargs callees: only fixed args (those recorded) are compared.

    ``comparable_sentinels`` restricts the comparison to callees that BOTH
    sides route through a sentinel.  A record only exists when the call was
    intercepted, and the two sides do not intercept the same set: when the
    oracle maps its whole .text (raw intra-.text calls, see ``oracle_text``)
    its sibling calls resolve internally and are never recorded, while the
    candidate — which maps only the target slice — must redirect the same
    calls to sentinels, so they ARE recorded.  Comparing those unfiltered
    reports `oracle=<end at 0> candidate=_datum_get[...]`: a pure bookkeeping
    artifact, since both sides execute the same callee bytes. Passing the
    intersection of the two stub maps keeps the differential sound where it
    is meaningful and silent where it is not.  None = compare everything
    (the historical behaviour, correct whenever both sides stub alike).
    """
    def _keeper(tracer):
        """Per-side filter: each side has its own target extent."""
        rng = getattr(tracer, "target_range", None)

        def _keep(r):
            if (_is_chkstk_name(r.callee_name)
                    or _is_inlined_intrinsic_name(r.callee_name)):
                return False
            if (comparable_sentinels is not None
                    and r.callee_addr not in comparable_sentinels):
                return False
            # Only the TARGET's own calls are the target's behaviour.  When one
            # side stubs an intra-object sibling and the other executes it
            # natively, the native side additionally records every call the
            # SIBLING makes -- extra entries that make the sequences differ
            # without the target having done anything different.  Those belong
            # to the sibling's own equivalence target, so drop them here.
            # caller_addr == 0 means attribution was unavailable: keep the
            # record, since absent evidence is not evidence of nesting.
            if rng is not None and r.caller_addr:
                # Upper bound inclusive: a CALL as the function's last
                # instruction leaves a return address equal to `end`.
                if not (rng[0] <= r.caller_addr <= rng[1]):
                    return False
            return True

        return _keep

    def _nested_count(tracer, keep):
        """How many records `keep` rejected specifically for being nested."""
        rng = getattr(tracer, "target_range", None)
        if rng is None:
            return 0
        return sum(1 for r in tracer.records
                   if r.caller_addr and not (rng[0] <= r.caller_addr <= rng[1])
                   and not _is_chkstk_name(r.callee_name)
                   and not _is_inlined_intrinsic_name(r.callee_name))

    _keep_orc, _keep_cand = _keeper(oracle_tracer), _keeper(cand_tracer)
    oc = [r for r in oracle_tracer.records if _keep_orc(r)]
    cc = [r for r in cand_tracer.records if _keep_cand(r)]
    n_nested_orc = _nested_count(oracle_tracer, _keep_orc)
    n_nested_cand = _nested_count(cand_tracer, _keep_cand)

    total_calls = len(oc)
    seq_diverged = False
    seq_diverge_idx = -1

    # Check sequence length
    if len(oc) != len(cc):
        seq_diverged = True
        seq_diverge_idx = min(len(oc), len(cc))

    # Check callee identity up to the shorter of the two.  Run this even when
    # the lengths already differ: the length check only tells us the sequences
    # cannot match, while the FIRST position where the callees disagree is what
    # bounds the arg comparison below.  Skipping it (as this did before) left
    # seq_diverge_idx = min(len) while the real divergence could be at index 0,
    # so misaligned pairs before it were still arg-compared.
    for i, (o, c) in enumerate(zip(oc, cc)):
        if o.callee_addr != c.callee_addr:
            if not seq_diverged or i < seq_diverge_idx:
                seq_diverge_idx = i
            seq_diverged = True
            break

    if seq_diverged:
        _i = seq_diverge_idx
        _on = oc[_i].callee_name if _i < len(oc) else f"<end at {len(oc)}>"
        _cn = cc[_i].callee_name if _i < len(cc) else f"<end at {len(cc)}>"
        _oa = [hex(a) for a in oc[_i].args] if _i < len(oc) else []
        _ca = [hex(a) for a in cc[_i].args] if _i < len(cc) else []
        print(f"      seq-diverge[{_i}]: oracle={_on}{_oa}  candidate={_cn}{_ca} "
              f"(lens {len(oc)}/{len(cc)})")
        print(f"      oracle seq   : {[(r.callee_name, [hex(a) for a in r.args]) for r in oc]}")
        print(f"      candidate seq: {[(r.callee_name, [hex(a) for a in r.args]) for r in cc]}")

    arg_mismatches = 0
    soft_matches = 0
    soft_semantic = 0
    soft_reasons = {}
    details = []

    # Per-arg comparison over the matching prefix ONLY.  Past the divergence
    # index the two lists no longer describe the same calls, so comparing them
    # pairwise reports differences between UNRELATED callees as argument bugs.
    #
    # That is not a corner case: when one side resolves an intra-object sibling
    # internally, its sequence is the other's shifted by one, and every
    # subsequent pair mismatches.  object_has_node had
    #   oracle    = [tag_get('obje',0), tag_get('mode',0)]
    #   candidate = [datum_get(0,0), tag_get('obje',0), tag_get('mode',0)]
    # -- the candidate makes exactly the same two tag_get calls -- yet the
    # unbounded compare reported "arg[0] to _tag_get: oracle=0x6f626a65
    # candidate=0x0", i.e. a dropped tag-group literal that does not exist.
    # About 20 of the divergence ledger's 50 `arg_mismatch` entries were this
    # artifact, several filed as P1 "wrong argument to a named callee".
    #
    # The seed still FAILS on the sequence divergence itself, which is honest;
    # it just no longer manufactures argument evidence against innocent code.
    pairs = list(zip(oc, cc))
    if seq_diverged and seq_diverge_idx >= 0:
        pairs = pairs[:seq_diverge_idx]
    for o_rec, c_rec in pairs:
        n_args = max(len(o_rec.args), len(c_rec.args))
        for ai in range(n_args):
            o_val = o_rec.args[ai] if ai < len(o_rec.args) else 0
            c_val = c_rec.args[ai] if ai < len(c_rec.args) else 0
            if o_val == c_val:
                continue
            if _is_stack_ptr(o_val) and _is_stack_ptr(c_val):
                soft_matches += 1
                continue
            if _is_slot_vs_real_global(o_val, c_val):
                soft_semantic += 1
                soft_reasons["globals-slot-alias"] = \
                    soft_reasons.get("globals-slot-alias", 0) + 1
                continue
            if _is_assert_metadata_arg(o_rec.callee_name, ai):
                soft_semantic += 1
                soft_reasons["assert-metadata"] = \
                    soft_reasons.get("assert-metadata", 0) + 1
                continue
            if _is_byte_fill_equivalent(o_rec.callee_name, ai, o_val, c_val):
                soft_semantic += 1
                soft_reasons["memset-fill"] = \
                    soft_reasons.get("memset-fill", 0) + 1
                continue
            arg_mismatches += 1
            details.append((seed_label, o_rec.seq, o_rec.callee_name, ai, o_val, c_val))

    return StubArgDiff(
        total_calls=total_calls,
        arg_mismatches=arg_mismatches,
        soft_stack_ptr_matches=soft_matches,
        details=details,
        sequence_diverged=seq_diverged,
        sequence_diverge_index=seq_diverge_idx,
        soft_semantic_matches=soft_semantic,
        soft_reasons=soft_reasons,
        # Built from the same filtered lists the comparison walked, so the
        # recorded sequences are exactly what produced the verdict.
        oracle_seq=[r.callee_name for r in oc],
        candidate_seq=[r.callee_name for r in cc],
        nested_dropped_oracle=n_nested_orc,
        nested_dropped_candidate=n_nested_cand,
    )


@dataclass
class RelocClassification:
    """Result of classifying a function's relocations."""
    category: str           # "leaf", "data_only", "stubbable", "non_leaf"
    dir32_count: int        # number of DIR32 (global data) relocations
    call_count: int         # number of external REL32 (call) relocations
    intra_obj_calls: int    # calls to functions defined in same .obj
    external_symbols: list  # names of unresolvable externals
    reason: str             # human-readable classification reason


def classify_relocations(relocs: list, defined_symbols: set) -> RelocClassification:
    """Classify a function's relocations to determine emulation strategy.

    Categories:
      leaf       - no external relocations at all
      data_only  - only DIR32 refs to global data (no calls)
      stubbable  - has external calls, but all callees are known
      non_leaf   - has unresolvable external references
    """
    if not relocs:
        return RelocClassification("leaf", 0, 0, 0, [], "no relocations")

    dir32_ext = 0
    rel32_ext = 0
    intra_obj = 0
    external = []

    for r in relocs:
        sym = r.symbol_name
        if sym.startswith(".text") or sym.startswith(".rdata"):
            continue
        if sym in defined_symbols:
            intra_obj += 1
            continue

        if r.reloc_type == IMAGE_REL_I386_DIR32:
            dir32_ext += 1
            external.append(sym)
        elif r.reloc_type == IMAGE_REL_I386_REL32:
            rel32_ext += 1
            external.append(sym)
        else:
            external.append(sym)

    if dir32_ext == 0 and rel32_ext == 0:
        return RelocClassification("leaf", 0, 0, intra_obj, [],
                                   "all relocations resolve within .obj")

    if rel32_ext == 0:
        return RelocClassification("data_only", dir32_ext, 0, intra_obj, external,
                                   f"{dir32_ext} global data reference(s)")

    return RelocClassification("stubbable", dir32_ext, rel32_ext, intra_obj, external,
                               f"{rel32_ext} external call(s), {dir32_ext} data ref(s)")


def patch_dir32_relocs(code: bytes, relocs: list, defined_symbols: set,
                       globals_base: int = GLOBALS_BASE,
                       return_slots: bool = False,
                       rdata_map: dict = None,
                       snapshot_regions: dict = None):
    """Rewrite DIR32 relocations to point into the globals memory region.

    Each unique external DIR32 symbol gets a 256-byte slot in the globals
    region (enough for most scalar/struct globals).  Section-relative
    relocations (.text, .rdata prefixed) are left untouched.

    Symbols in rdata_map (intra-object cross-section references like .rdata
    constants) also get globals slots, seeded with actual section data.

    snapshot_regions ({addr: bytes} from a --state-snapshot) enables IDENTITY
    relocation: a DAT_/PTR_/FLOAT_<addr> symbol whose encoded XBE address
    falls inside a snapshot region is patched to its REAL address instead of
    a slot.  The region bytes are mapped into emulator memory verbatim, so
    the oracle then reads the same full-size data the candidate reads via
    absolute immediates — required for indexed tables larger than the 8-byte
    slot seed window (e.g. the 2304-byte wind noise table at 0x5057c4, whose
    slot-relocated indexed reads would otherwise walk into neighboring
    slots).

    Returns a mutable copy of the code with patched addresses.
    If return_slots is True, returns (patched, symbol_slots) where
    symbol_slots maps symbol_name -> slot_address.
    """
    import re as _re
    patched = bytearray(code)
    slot_size = 256
    symbol_slots = {}
    rdata_seeds = {}
    next_slot = 0
    if rdata_map is None:
        rdata_map = {}

    def _snapshot_identity_addr(sym_name):
        if not snapshot_regions:
            return None
        m = _re.match(r'(?:DAT|PTR|PTR_DAT|FLOAT)_([0-9a-fA-F]{4,})$',
                      sym_name)
        if not m:
            return None
        orig = int(m.group(1), 16)
        for base, data in snapshot_regions.items():
            if base <= orig < base + len(data):
                return orig
        return None

    for r in relocs:
        if r.reloc_type != IMAGE_REL_I386_DIR32:
            continue
        sym = r.symbol_name
        is_rdata_ref = sym in rdata_map
        if ((sym.startswith(".text") or sym.startswith(".rdata"))
                and not is_rdata_ref):
            continue
        if sym in defined_symbols and not is_rdata_ref:
            continue

        if not is_rdata_ref:
            _ident = _snapshot_identity_addr(sym)
            if _ident is not None:
                off = r.virtual_address
                if off + 4 <= len(patched):
                    struct.pack_into('<I', patched, off, _ident)
                continue

        if sym not in symbol_slots:
            symbol_slots[sym] = globals_base + next_slot * slot_size
            next_slot += 1
            if is_rdata_ref:
                rdata_seeds[symbol_slots[sym]] = rdata_map[sym][:slot_size]

        off = r.virtual_address
        if off + 4 <= len(patched):
            addend = 0
            if is_rdata_ref:
                addend = struct.unpack_from('<I', patched, off)[0]
            addr = symbol_slots[sym] + addend
            struct.pack_into('<I', patched, off, addr)

    if return_slots:
        return patched, symbol_slots, rdata_seeds
    return patched


# Must be within signed-int32 range of CODE_BASE (0x00400000) to avoid
# rel32 displacement overflow when patching CALL instructions.
STUB_BASE = 0x40000000
STUB_SLOT = 0x4000  # 16KB per sentinel slot — enough for real callee code
MAX_RECURSION_DEPTH = 3


@dataclass
class CalleeStub:
    """Pre-loaded callee information for runtime stubbing."""
    name: str
    code: bytes
    abi: dict
    sentinel_addr: int
    has_real_code: bool = False  # True when code is patched oracle bytes, not a trampoline


def patch_rel32_calls(code: bytes, relocs: list, defined_symbols: set,
                      code_base: int = 0x00400000,
                      symbol_sentinels: Optional[dict[str, int]] = None,
                      include_defined: bool = False,
                      force_redirect_names: Optional[set] = None) -> tuple:
    """Rewrite REL32 call relocations to sentinel addresses.

    Returns (patched_code, stub_map) where stub_map is
    {sentinel_addr: symbol_name} for each redirected call.

    include_defined: when True, also redirect calls to symbols DEFINED in the
    same .obj (named siblings like FUN_xxxx) to sentinels.  Needed for callee
    code run standalone at a sentinel — it has no mapped .text section, so its
    intra-object sibling calls would otherwise keep their original (now wrong)
    displacements.  Section-relative ".text"/".rdata" relocs are still skipped
    (they carry an addend, not a single resolvable symbol).

    force_redirect_names: set of canonical (underscore-stripped) symbol names
    that must ALWAYS be redirected to a sentinel, even when the symbol is a
    DEFINED intra-object sibling and include_defined is False.  Used for
    Python-intercept callees (StubManager._INTERCEPT_NAMES, e.g.
    object_get_and_verify_type): the oracle references them via a per-function
    delinked ref where they are external and thus already redirected, but the
    candidate is the WHOLE .obj where the same callee is a defined sibling.
    Left unpatched, the sibling call's disp stays 0 (a no-op `call $+5`) so it
    falls through leaving the pre-call EAX intact — silently substituting the
    caller's argument for the callee's result and producing a false write-trace
    divergence.  Redirecting to the shared sentinel makes the Python model run
    identically on BOTH sides, restoring symmetry.
    """
    patched = bytearray(code)
    stub_map = {}
    if symbol_sentinels is None:
        symbol_sentinels = {}
    if force_redirect_names is None:
        force_redirect_names = frozenset()
    next_idx = len(symbol_sentinels)

    for r in relocs:
        if r.reloc_type != IMAGE_REL_I386_REL32:
            continue
        sym = r.symbol_name
        if sym.startswith(".text") or sym.startswith(".rdata"):
            continue
        if (sym in defined_symbols and not include_defined
                and sym.lstrip("_") not in force_redirect_names):
            continue

        sym_key = sym.lstrip("_")
        sentinel = symbol_sentinels.get(sym_key)
        if sentinel is None:
            sentinel = STUB_BASE + next_idx * STUB_SLOT
            symbol_sentinels[sym_key] = sentinel
            next_idx += 1
        call_addr = code_base + r.virtual_address
        # REL32: displacement = target - (call_addr + 4)
        # The +4 is because the displacement is relative to the end of the instruction
        disp = sentinel - (call_addr + 4)
        off = r.virtual_address
        if off + 4 <= len(patched):
            struct.pack_into('<i', patched, off, disp)
        stub_map[sentinel] = sym

    return bytes(patched), stub_map


# Scratch arena for pointer-returning accessor stubs.
STUB_OBJECT_ARENA = 0x10000000
_ARENA_PAGE = 0x10000

DEFAULT_STUB_RETURNS = {
    "createfilea": 0x100,
    "readfile": 1,
    "writefile": 1,
    "closehandle": 1,
    "getfileattributesa": 0x80,
    "file_exists": 1,
    "file_open": 1,
    "file_close": 1,
    "file_read": 1,
    "display_assert": 0,
    "system_exit": 0,
    "debug_malloc": STUB_OBJECT_ARENA,
    "csmemcpy": STUB_OBJECT_ARENA,
}

# Pointer-returning accessors, ranked by how many of the 331 divergent
# functions in the 2026-07-28 batch call them. All return 0 by default, which
# reads as "object not found", so every caller takes its NULL-check bail-out
# and the interesting body is never executed -- a large part of why 261 cached
# entries sat below 30% coverage and 170 at exactly 0.0%.
#
# Handing back a distinct mapped scratch page instead lets the caller proceed,
# and on a measured sample it moved functions from 0.0% to 100% coverage. It is
# OPT-IN (--rich-stub-returns), not the default, because a zero-filled page is
# not a valid object: a caller that reads a function pointer out of the
# returned struct and calls through it fetches from ~0 and dies
# (UC_ERR_FETCH_UNMAPPED at eip=0xff86). On a 200-function sample of
# previously-passing targets that turned ~6% of clean passes into errors --
# trading a trivially-clean verdict for a deeper crash, which is worse
# evidence, not better.
#
# The sound version of this lever is the plan's first choice: sub-emulate the
# real callee (--real-callees), or replay real state (--state-snapshot), so the
# returned pointer refers to an object that actually exists. Use this flag for
# coverage exploration, not for verdicts.
#
# Distinct pages, not one shared address, so a caller that confuses two
# accessors' results still shows as a difference. Values are identical on both
# sides, so the differential itself stays sound.
ACCESSOR_STUB_RETURNS = {
    "object_get_and_verify_type": STUB_OBJECT_ARENA + 1 * _ARENA_PAGE,
    "datum_get":                  STUB_OBJECT_ARENA + 2 * _ARENA_PAGE,
    "tag_get":                    STUB_OBJECT_ARENA + 3 * _ARENA_PAGE,
    "tag_block_get_element":      STUB_OBJECT_ARENA + 4 * _ARENA_PAGE,
    "global_scenario_get":        STUB_OBJECT_ARENA + 5 * _ARENA_PAGE,
    "scenario_get":               STUB_OBJECT_ARENA + 5 * _ARENA_PAGE,
    "object_header_get":          STUB_OBJECT_ARENA + 6 * _ARENA_PAGE,
}

# Set by unicorn_diff when --rich-stub-returns is passed.
ENABLE_ACCESSOR_STUB_RETURNS = False


def set_accessor_stub_returns(enabled: bool) -> None:
    global ENABLE_ACCESSOR_STUB_RETURNS
    ENABLE_ACCESSOR_STUB_RETURNS = bool(enabled)


class StubManager:
    """Manages callee stubs for non-leaf function emulation.

    Intercepts calls to external functions by patching their targets to
    sentinel addresses and hooking Unicorn's memory fetch errors.
    When a sentinel is hit, runs the callee's oracle code in a
    sub-emulator and copies the return value back.
    """

    def __init__(self, kb_path: Path, delinked_dir: Path):
        self.kb_path = kb_path
        self.delinked_dir = delinked_dir
        self._kb = None
        self._stubs: dict[int, CalleeStub] = {}
        self._stub_names: dict[int, str] = {}
        self._canonical_names: dict[int, str] = {}
        self._depth = 0
        # Real-callee sub-emulation state (populated by prepare_stubs when
        # real_callees is enabled): DIR32 slots the loaded callee code reads
        # from (caller seeds them) and any .rdata constant seeds.
        self._callee_dir32_slots: dict[str, int] = {}
        # {symbol_name: absolute VA of the DIR32 reloc SITE in the pristine
        # XBE}.  The caller reads those 4 bytes to recover which global each
        # callee slot stands for -- the same ground-truth resolution the
        # target function gets, extended to real-callee code.
        self._callee_dir32_sites: dict[str, int] = {}
        self._extra_rdata_seeds: dict[int, bytes] = {}
        self._real_code_count = 0
        # Optional stub argument tracer; set via set_tracer() before each run.
        self._tracer: Optional[StubArgTracer] = None
        # Per-name call counters for list-valued stub_returns (sequenced
        # returns).  Reset via reset_stub_sequences() before each run so
        # oracle and candidate see the identical sequence.
        self._seq_counters: dict[str, int] = {}
        # Per-name call counters for snapshot stub_writes (buffer writes a
        # stubbed callee performs for its caller).  Kept separate from
        # _seq_counters so a callee that has BOTH a sequenced return and a
        # write sequence does not advance one counter twice per call.
        self._write_counters: dict[str, int] = {}
        # Stub-convention verification (lift-learnings §30): every registered
        # stub's declared convention is checked against the real callee's RET
        # immediate in the pristine XBE.  A mismatch means BOTH oracle and
        # candidate stubs honor the same wrong convention — invisible to the
        # differential, fatal on the box (ESP drift, the 0x158df0 boot crash).
        # Mismatch strings accumulate here for reporting; the run-failing subset
        # is blocking_convention_mismatches() (registration cannot yet know
        # which stubs are even used -- see that method).
        self.convention_mismatches: list[str] = []
        # Structured form of the same: {sentinel, addr, name, msg}.
        self._conv_mismatches: list[dict] = []
        self._conv_checked: set[int] = set()
        # Sentinels whose SYNTHETIC stub actually ran (populated by
        # execute_stub, which real-code callees never reach).
        self._executed_stubs: set[int] = set()
        self._kb_addrs_sorted: Optional[list[int]] = None

    def reset_stub_sequences(self):
        """Reset the per-name call counters for list-valued stub_returns.

        Call before every emulation run (oracle AND candidate, every seed)
        so both sides replay the identical return sequence.
        """
        self._seq_counters.clear()
        self._write_counters.clear()

    def _lookup_return_override(self, address: int):
        """Resolve the snapshot stub_returns entry for a sentinel.

        Returns (key, value); value may be an int, a float, or a list
        (sequenced returns). (None, None) when no override matches.
        """
        canon = self._canonical_names.get(address, "").lower()
        raw = self._stub_names.get(address, "").lstrip("_").lower()

        if self.stub_return_overrides:
            if canon in self.stub_return_overrides:
                return canon, self.stub_return_overrides[canon]
            if raw in self.stub_return_overrides:
                return raw, self.stub_return_overrides[raw]

        if canon in DEFAULT_STUB_RETURNS:
            return canon, DEFAULT_STUB_RETURNS[canon]
        if raw in DEFAULT_STUB_RETURNS:
            return raw, DEFAULT_STUB_RETURNS[raw]

        if ENABLE_ACCESSOR_STUB_RETURNS:
            if canon in ACCESSOR_STUB_RETURNS:
                return canon, ACCESSOR_STUB_RETURNS[canon]
            if raw in ACCESSOR_STUB_RETURNS:
                return raw, ACCESSOR_STUB_RETURNS[raw]

        return None, None

    def set_tracer(self, tracer: Optional["StubArgTracer"]):
        """Attach (or detach) a StubArgTracer for the current emulation run.

        Call with a fresh StubArgTracer before running oracle or candidate,
        and with None (or simply don't call) when tracing is disabled.
        The tracer accumulates one StubCallRecord per stub invocation.
        """
        self._tracer = tracer

    def _load_kb(self):
        if self._kb is None:
            self._kb = json.loads(self.kb_path.read_text(encoding="utf-8"))
        return self._kb

    def _find_callee_in_kb(self, symbol_name: str) -> Optional[dict]:
        """Look up a callee symbol in kb.json."""
        kb = self._load_kb()
        # Strip MSVC/clang stdcall decoration ("_Name@12") — candidate-side
        # relocs carry it, and an unmatched lookup falls back to a no-cleanup
        # default stub whose missing RET N drifts ESP on stdcall callees.
        canon = re.sub(r'@\d+$', '', symbol_name.lstrip("_"))
        # Try FUN_XXXXXXXX format
        m = re.match(r'FUN_([0-9a-fA-F]+)', canon)
        if m:
            addr = "0x" + m.group(1).lower().lstrip("0")
            if not addr.endswith("0"):
                addr = "0x" + m.group(1).lower()
        else:
            addr = None

        for obj in kb.get("objects", []):
            for fn in obj.get("functions", []):
                decl = fn.get("decl", "")
                fn_m = re.search(r'\b(\w+)\s*\(', decl)
                fn_name = fn_m.group(1) if fn_m else ""
                if fn_name == canon or (addr and fn.get("addr", "").lower() == addr):
                    return dict(fn, _obj_name=obj.get("name", ""))
        return None

    def _load_callee_code(self, symbol_name: str, kb_entry: dict):
        """Extract callee's oracle code from delinked .obj.

        Returns a FunctionSlice on success, or None if not found.
        """
        _SCRIPT_DIR = Path(__file__).resolve().parent
        sys.path.insert(0, str(_SCRIPT_DIR))
        from coff_loader import extract_function, CoffParseError

        obj_name = kb_entry.get("_obj_name", "").replace(".obj", "").replace("LIBCMT:", "")
        candidates = list(self.delinked_dir.glob(f"{obj_name}.obj"))
        if not candidates:
            candidates = list(self.delinked_dir.glob("*.obj"))
        # Per-function delinked refs (delinked/functions/<addr8>.obj) — the
        # only oracle-side source for intra-object siblings whose TU has no
        # whole-object delinked export.
        _addr = kb_entry.get("addr", "")
        if _addr:
            try:
                _fp = (self.delinked_dir / "functions"
                       / f"{int(_addr, 16):08x}.obj")
                if _fp.exists():
                    candidates.insert(0, _fp)
            except ValueError:
                pass

        # Delinked objects name functions FUN_<addr>, but a call site may use the
        # real name (lifted/clang obj) or the FUN_ form (delinked oracle).  Both
        # resolve to the same kb_entry — normalize to FUN_<addr> so the named
        # call site also finds the real oracle body (else it stays a trampoline
        # and diverges from the FUN_-named oracle side).
        fun_syms = []
        addr = kb_entry.get("addr", "")
        if addr:
            try:
                a = int(addr, 16)
                fun_syms = [f"FUN_{a:08x}", f"FUN_{a:08X}"]
            except ValueError:
                pass

        for try_sym in dict.fromkeys([symbol_name, symbol_name.lstrip("_")] + fun_syms):
            for obj_path in candidates:
                try:
                    fs = extract_function(str(obj_path), try_sym)
                    if fs.code:
                        return fs
                except (CoffParseError, Exception):
                    continue
        return None

    # Cap on total real-code callees loaded, to bound a runaway call graph.
    MAX_REAL_CALLEES = 96

    # Deterministic EAX values for named stubbed callees, keyed by lowercase
    # undecorated symbol name (snapshot "stub_returns"). Applied identically
    # to oracle and candidate, so gated paths open without --real-callees.
    stub_return_overrides = {}

    # Buffer writes a stubbed callee performs on behalf of its caller
    # (snapshot "stub_writes"), keyed by lowercase undecorated symbol name.
    # Value: a list with one entry per call, {"arg": <0-based stack param
    # index>, "data": "<hex>"} (or null for "this call writes nothing"); the
    # last entry repeats past the end.  Needed for read-into-buffer callees --
    # file_read_from_position, tag/name lookups -- whose OUTPUT, not their
    # return value, gates the caller's real body.  A scalar stub_returns can
    # only open the `if (read(...))` guard; the parser behind it still sees an
    # untouched (and frame-layout-dependent) buffer.  Applied identically to
    # oracle and candidate at the same call index, so the differential stays
    # symmetric; a call-count divergence between the two sides desynchronises
    # the sequence and shows up in the stub-arg call-sequence compare.
    stub_write_overrides = {}

    def _lookup_write_override(self, address: int):
        """Resolve the snapshot stub_writes entry for a sentinel.

        Returns (key, list) or (None, None) when no override matches.
        """
        if not self.stub_write_overrides:
            return None, None
        canon = self._canonical_names.get(address, "").lower()
        raw = self._stub_names.get(address, "").lstrip("_").lower()
        for key in (canon, raw):
            if key and key in self.stub_write_overrides:
                return key, self.stub_write_overrides[key]
        return None, None

    def _register_stub(self, sentinel_addr: int, symbol_name: str):
        """Create a trampoline CalleeStub (+ canonical name) for a sentinel.

        Returns (CalleeStub or None, kb_entry or None).  A stub is only created
        when kb.json has a parseable decl for the callee; otherwise the sentinel
        falls back to the synthetic ret-stub in get_stub_code.
        """
        from abi import parse_decl
        self._stub_names[sentinel_addr] = symbol_name
        kb_entry = self._find_callee_in_kb(symbol_name)
        decl = kb_entry.get("decl", "") if kb_entry else ""
        if not decl:
            return None, kb_entry
        fn_m = re.search(r'\b(\w+)\s*\(', decl)
        if fn_m:
            self._canonical_names[sentinel_addr] = fn_m.group(1)
        try:
            abi = parse_decl(decl)
        except Exception:
            return None, kb_entry
        stub = CalleeStub(name=symbol_name, code=b"", abi=abi,
                          sentinel_addr=sentinel_addr, has_real_code=False)
        self._stubs[sentinel_addr] = stub
        self._check_convention(symbol_name, kb_entry, decl, abi, sentinel_addr)
        return stub, kb_entry

    def _next_kb_addr(self, addr: int) -> int:
        """Smallest kb.json function address strictly above addr (scan bound)."""
        if self._kb_addrs_sorted is None:
            addrs = []
            for obj in self._load_kb().get("objects", []):
                for fn in obj.get("functions", []):
                    try:
                        addrs.append(int(fn.get("addr", ""), 16))
                    except ValueError:
                        pass
            self._kb_addrs_sorted = sorted(addrs)
        import bisect
        i = bisect.bisect_right(self._kb_addrs_sorted, addr)
        if i < len(self._kb_addrs_sorted):
            return self._kb_addrs_sorted[i]
        return addr + 0x2000

    def _check_convention(self, symbol_name: str, kb_entry: Optional[dict],
                          decl: str, abi: dict,
                          sentinel_addr: int = 0) -> None:
        """Verify the declared convention against the callee's binary RET.

        Both oracle and candidate stubs honor the kb.json decl, so a wrong
        convention (e.g. Ghidra's ``void f(void)`` over a ``RET 4`` body —
        lift-learnings §30) drifts ESP identically on both sides and the
        differential passes while the box crashes.  Reads the real callee's
        first RET immediate from the pristine XBE and records a mismatch in
        ``self.convention_mismatches``.  Skips silently when the XBE is
        unavailable (CI), the callee address is unknown, or the decl is
        noreturn/varargs.
        """
        addr_s = kb_entry.get("addr", "") if kb_entry else ""
        if not addr_s:
            return
        try:
            addr = int(addr_s, 16)
        except ValueError:
            return
        if addr in self._conv_checked:
            return
        self._conv_checked.add(addr)
        if re.search(r'\bnoreturn\b|\.\.\.', decl):
            return
        try:
            audit_dir = str(Path(__file__).resolve().parent.parent / "audit")
            if audit_dir not in sys.path:
                sys.path.insert(0, audit_dir)
            from check_stdcall_ret import find_first_ret
            res = find_first_ret(addr, self._next_kb_addr(addr))
        except Exception:
            return  # XBE / capstone unavailable — nothing to compare against
        if res.kind != "ret":
            return
        conv = abi.get("conv", "cdecl")
        n_stack = sum(1 for p in abi["params"] if not p.reg)
        expected = 4 * n_stack if conv == "stdcall" else 0
        if res.pop_bytes != expected:
            msg = (f"{symbol_name} @ {addr:#x}: decl is {conv} with {n_stack} "
                   f"stack arg(s) (implies RET {expected}) but the binary RETs "
                   f"{res.pop_bytes} — the stub honors the WRONG convention on "
                   f"both sides; fix the kb.json decl (lift-learnings §30)")
            self.convention_mismatches.append(msg)
            self._conv_mismatches.append({"sentinel": sentinel_addr,
                                          "addr": addr,
                                          "name": symbol_name,
                                          "msg": msg})
            print(f"  [stub-conv MISMATCH] {msg}", file=sys.stderr)

    def blocking_convention_mismatches(self) -> list[str]:
        """The mismatches that could actually corrupt ESP in *this* run.

        A declared convention is only consulted when a SYNTHETIC stub is both
        emitted and executed, so the raw list over-reports on two counts:

        * ``has_real_code`` -- get_stub_code returns the callee's own oracle
          bytes, whose real ``RET N`` pops correctly, so the decl is never
          used.  Registration runs before _load_real_callees, so at check time
          we cannot yet know this.
        * never executed -- registration covers every callee in the stub map
          regardless of reachability, so a decl bug on a callee no seed ever
          calls used to block the whole target (measured: bink_playback_stop
          blocked on FUN_000e5590 with 0 stub calls on the covered path).

        Neither case can drift ESP, so neither should fail the run.  A wrong
        decl is still a real kb.json defect -- it stays in
        ``convention_mismatches`` and is reported as a warning.  Fixing it is
        check_stdcall_ret.py's job, which gates separately at pre-commit/CI.
        """
        blocking = []
        for m in self._conv_mismatches:
            stub = self._stubs.get(m["sentinel"])
            if stub is not None and stub.has_real_code:
                continue
            if m["sentinel"] not in self._executed_stubs:
                continue
            blocking.append(m["msg"])
        return blocking

    def prepare_stubs(self, stub_map: dict, globals_base: int = None,
                      shared_sentinels: dict = None,
                      real_callees: bool = False,
                      snapshot_regions: dict = None) -> int:
        """Prepare callee stubs for all symbols in the stub_map.

        By default each callee is a return-0 trampoline (only its call site is
        exercised).  When ``real_callees`` is set, every non-intercept callee
        with loadable delinked oracle code is loaded and patched to run NATIVELY
        in place at its sentinel: its DIR32 globals are redirected to fresh
        seed slots and its REL32 calls to sentinels (recursively, bounded by
        ``MAX_RECURSION_DEPTH`` and ``MAX_REAL_CALLEES``).  This lets iterator
        and helper loops actually execute over snapshot data instead of
        early-exiting on a NULL/zero stub return.

        ``globals_base`` is the first free address in the globals region (past
        the caller's own oracle/lifted slots); ``shared_sentinels`` is the
        symbol->sentinel map shared with the caller's REL32 patching so the
        same callee always resolves to the same sentinel.

        Returns the number of top-level stubs prepared.  Callee DIR32 slots to
        seed are exposed via ``self._callee_dir32_slots`` (the caller runs them
        through _build_globals_seeds) and ``self._extra_rdata_seeds``.
        """
        prepared = 0
        for sentinel_addr, symbol_name in stub_map.items():
            stub, _ = self._register_stub(sentinel_addr, symbol_name)
            if stub is not None:
                prepared += 1

        if real_callees:
            self._load_real_callees(stub_map, globals_base, shared_sentinels,
                                    snapshot_regions=snapshot_regions)

        return prepared

    def _load_real_callees(self, top_stub_map: dict, globals_base: int,
                           shared_sentinels: dict,
                           snapshot_regions: dict = None) -> None:
        """BFS-load native oracle code for non-intercept callees.

        Mutates self._stubs (sets has_real_code + patched code), discovers
        nested callees and registers their sentinels, and accumulates the
        DIR32 slots their code reads into self._callee_dir32_slots.
        """
        from coff_loader import extract_function, CoffParseError  # noqa: F401

        if globals_base is None:
            globals_base = GLOBALS_BASE
        if shared_sentinels is None:
            shared_sentinels = {}
        glob_cursor = globals_base

        worklist = [(sa, sym, 0) for sa, sym in top_stub_map.items()]
        processed = set()
        while worklist:
            sentinel_addr, symbol_name, depth = worklist.pop(0)
            if sentinel_addr in processed:
                continue
            processed.add(sentinel_addr)
            if (self._real_code_count >= self.MAX_REAL_CALLEES
                    or depth >= MAX_RECURSION_DEPTH):
                continue
            # Math/memcpy/assert intrinsics stay as Python intercepts.
            name = self._resolve_name(sentinel_addr)
            if name in self._INTERCEPT_NAMES or name in self._FTOL2_ADDRS:
                continue
            # Explicit snapshot stub_returns override wins over real-code
            # loading.  The snapshot author deliberately stubbed this callee's
            # return value (e.g. object_header_block_reference_get -> a node
            # block pointer) precisely because its real body has preconditions
            # the synthetic state cannot satisfy — running that body would
            # assert (reference->offset>0) and halt the walk.  Leaving it as a
            # trampoline lets get_stub_code bake in the MOV EAX,<override>;RET,
            # applied identically to oracle and candidate.
            if self.stub_return_overrides:
                _c = self._canonical_names.get(sentinel_addr, "").lower()
                _r = self._stub_names.get(sentinel_addr, "").lstrip("_").lower()
                if (_c in self.stub_return_overrides
                        or _r in self.stub_return_overrides):
                    continue
            # Same for a snapshot stub_writes entry: the author is supplying
            # the callee's OUTPUT by hand precisely because its real body
            # cannot produce it here (file I/O, tag data), so keep the
            # trampoline rather than loading the real code.
            if self.stub_write_overrides:
                _c = self._canonical_names.get(sentinel_addr, "").lower()
                _r = self._stub_names.get(sentinel_addr, "").lstrip("_").lower()
                if (_c in self.stub_write_overrides
                        or _r in self.stub_write_overrides):
                    continue
            stub = self._stubs.get(sentinel_addr)
            if stub is None:
                continue  # no decl/abi -> synthetic ret-stub
            kb_entry = self._find_callee_in_kb(symbol_name)
            # BIPED_REAL_SAME_OBJ=<obj name>: restrict real-code loading to
            # intra-object siblings. Extern callees stay symmetric stubs —
            # the candidate (full clang TU) cannot run them either, so
            # loading them only into the oracle diverges the two sides.
            _same_obj = os.environ.get("BIPED_REAL_SAME_OBJ")
            if (_same_obj and kb_entry
                    and kb_entry.get("_obj_name") != _same_obj):
                continue
            fs = self._load_callee_code(symbol_name, kb_entry) if kb_entry else None
            if fs is None or not fs.code or len(fs.code) > STUB_SLOT:
                continue  # not found / too big for a sentinel slot -> trampoline
            defined = getattr(fs, 'defined_symbols', set())
            rdata = getattr(fs, 'rdata_map', {})
            # DIR32 -> fresh globals slots (each unique global one 256B slot).
            # snapshot_regions: identity-relocate DAT_/FLOAT_ globals that
            # fall inside snapshot regions, so recursively-loaded callees
            # (e.g. valid_real_normal3d's epsilon read) see the authored
            # bytes instead of fresh-zeroed slots.
            patched, slots, rdata_seeds = patch_dir32_relocs(
                fs.code, fs.relocs, defined,
                globals_base=glob_cursor, return_slots=True, rdata_map=rdata,
                snapshot_regions=snapshot_regions)
            glob_cursor += max(1, len(slots)) * 256
            self._callee_dir32_slots.update(slots)
            # Record where each of this callee's DIR32 relocs lives in the
            # original image, so the caller can resolve slot -> real global.
            _cva = kb_entry.get("addr") if kb_entry else None
            if _cva_ok(_cva):
                _cva = int(_cva, 16)
                for _r in fs.relocs:
                    if (_r.reloc_type == IMAGE_REL_I386_DIR32
                            and _r.symbol_name in slots):
                        self._callee_dir32_sites.setdefault(
                            _r.symbol_name, _cva + _r.virtual_address)
            self._extra_rdata_seeds.update(rdata_seeds)
            # REL32 -> sentinels, relative to where this callee will be written.
            # include_defined: sibling (intra-object) calls must also redirect
            # to sentinels, since this callee has no mapped .text section here.
            patched2, new_map = patch_rel32_calls(
                bytes(patched), fs.relocs, defined,
                code_base=sentinel_addr, symbol_sentinels=shared_sentinels,
                include_defined=True)
            # Raw E8/E9 calls with NO reloc: the delinker encodes intra-XBE
            # calls as direct displacements to absolute VAs.  At the original
            # base they resolve; relocated to a sentinel they point to garbage.
            # Redirect them to sentinels too (target must be an EXACT known
            # function address — a strong guard against E8 bytes that are really
            # mid-instruction operands).
            callee_base = int(kb_entry.get("addr", "0"), 16) if kb_entry else 0
            if callee_base:
                reloc_offs = {r.virtual_address for r in fs.relocs}
                patched2, raw_map = self._redirect_raw_calls(
                    bytes(patched2), callee_base, reloc_offs,
                    sentinel_addr, shared_sentinels)
                new_map.update(raw_map)
            stub.code = bytes(patched2)
            stub.has_real_code = True
            self._real_code_count += 1
            if os.environ.get("BIPED_TRACE_REAL") == "1":
                print(f"  [real-callee] {symbol_name} "
                      f"({len(stub.code)}B, depth {depth})")
            # Enqueue nested callees discovered in this callee's body.
            for new_sentinel, new_sym in new_map.items():
                if new_sentinel in processed:
                    continue
                if new_sentinel not in self._stubs:
                    self._register_stub(new_sentinel, new_sym)
                worklist.append((new_sentinel, new_sym, depth + 1))

    def _kb_func_addrs(self) -> set:
        """Set of all kb.json function entry addresses (ints), cached."""
        cached = getattr(self, "_func_addr_set", None)
        if cached is not None:
            return cached
        addrs = set()
        for obj in self._load_kb().get("objects", []):
            for fn in obj.get("functions", []):
                a = fn.get("addr", "")
                if a:
                    try:
                        addrs.add(int(a, 16))
                    except ValueError:
                        pass
        self._func_addr_set = addrs
        return addrs

    def _redirect_raw_calls(self, code: bytes, callee_base: int,
                            reloc_offsets: set, sentinel_addr: int,
                            shared_sentinels: dict) -> tuple:
        """Redirect unrelocated E8/E9 calls/jumps to sentinels.

        Only redirects when the computed absolute target is an EXACT kb.json
        function entry address, so a mid-instruction 0xE8/0xE9 byte (whose
        "target" is essentially random) is left untouched.
        Returns (patched_code, {sentinel_addr: symbol_name}).
        """
        func_addrs = self._kb_func_addrs()
        patched = bytearray(code)
        new_map = {}
        next_idx = len(shared_sentinels)
        i = 0
        n = len(patched)
        while i + 5 <= n:
            op = patched[i]
            if op in (0xE8, 0xE9) and (i + 1) not in reloc_offsets:
                disp = int.from_bytes(patched[i + 1:i + 5], "little", signed=True)
                target_va = (callee_base + i + 5 + disp) & 0xFFFFFFFF
                if target_va in func_addrs:
                    sym = f"FUN_{target_va:08x}"
                    sym_key = sym.lstrip("_")
                    sentinel = shared_sentinels.get(sym_key)
                    if sentinel is None:
                        sentinel = STUB_BASE + next_idx * STUB_SLOT
                        shared_sentinels[sym_key] = sentinel
                        next_idx += 1
                    new_disp = sentinel - (sentinel_addr + i + 5)
                    patched[i + 1:i + 5] = (new_disp & 0xFFFFFFFF).to_bytes(4, "little")
                    new_map[sentinel] = sym
                    i += 5
                    continue
            i += 1
        return bytes(patched), new_map

    def has_stub(self, address: int) -> bool:
        return address in self._stub_names

    def get_stub_addresses(self) -> set:
        return set(self._stub_names.keys())

    _INTERCEPT_NAMES = frozenset((
        "csmemcpy", "memcpy", "csmemcmp", "memcmp", "csstrncpy",
        "csmemset", "memset",
        "crt_sprintf", "debug_string_to_display",
        "system_exit", "display_assert", "halt_and_catch_fire",
        "_chkstk", "__chkstk", "chkstk", "fun_001d90e0",
        "global_scenario_get", "scenario_get", "tag_block_get_element",
        "ciacos", "ciasin", "ciatan2", "cisin", "cicos",
        "cisqrt", "cilog", "cilog10", "cipow", "cifmod", "citan",
        "datum_get",
        "object_get_and_verify_type",
        "tag_get",
        "real_vector3d_valid", "valid_real_point3d",
        "valid_real_normal3d_perpendicular", "valid_real_vector3d",
    ))
    _FTOL2_ADDRS = frozenset(("fun_001d9068", "_ftol2", "ftol2"))
    # XBE address → _CI* intrinsic name for FUN_XXXXXXXX symbols
    _CRT_MATH_ADDRS = {
        "fun_001d94f0": "ciacos",
        "fun_001da0cc": "ciasin",
        "fun_001daf7e": "cifmod",
    }

    def _resolve_name(self, address: int) -> str:
        """Resolve a stub address to its effective intercept name."""
        raw = self._stub_names.get(address, "").lstrip("_").lower()
        if raw in self._CRT_MATH_ADDRS:
            return self._CRT_MATH_ADDRS[raw]
        canonical = self._canonical_names.get(address, "").lower()
        if canonical in self._CRT_MATH_ADDRS:
            return self._CRT_MATH_ADDRS[canonical]
        if canonical in self._INTERCEPT_NAMES:
            return canonical
        return raw

    def should_intercept(self, address: int) -> bool:
        # List-valued stub_returns (sequenced returns) are served dynamically
        # in execute_stub — the static trampoline can only encode one value.
        # An explicit snapshot sequence outranks named intercepts and real
        # loaded code.
        _k, _v = self._lookup_return_override(address)
        if isinstance(_v, list):
            return True
        # Snapshot stub_writes are applied in execute_stub too, so a callee
        # with a write sequence must be served dynamically even when its
        # return value is a plain scalar (which get_stub_code would otherwise
        # bake into a static MOV EAX,imm32 trampoline).
        _wk, _wv = self._lookup_write_override(address)
        if _wv:
            return True
        # Named intercepts always take priority — even if oracle code was loaded.
        name = self._resolve_name(address)
        if name in self._INTERCEPT_NAMES or name in self._FTOL2_ADDRS:
            return True
        stub = self._stubs.get(address)
        # Real-code stubs execute natively via Unicorn; their RET pops the return addr.
        if stub is not None and stub.has_real_code:
            return False
        return False

    def get_stub_code(self, address: int) -> bytes:
        """Return machine code to write at a sentinel address.

        For real-code stubs, returns the oracle bytes directly — the page is
        pre-filled with 0xCC (INT3), so no padding is needed.
        For trampoline stubs, returns a tiny synthetic return sequence.
        """
        stub = self._stubs.get(address)

        if stub is not None and stub.has_real_code:
            return stub.code

        symbol_name = self._resolve_name(address)

        if symbol_name == "fabs":
            return b"\xDD\x44\x24\x04\xD9\xE1\xC3"

        if stub is not None:
            ret_st0 = stub.abi.get('ret_st0', False)
            ret_void = stub.abi.get('ret_void', True)
            conv = stub.abi.get('conv', 'cdecl')
            n_stack_params = sum(1 for p in stub.abi['params'] if not p.reg)
        else:
            ret_st0 = False
            ret_void = False
            conv = 'cdecl'
            n_stack_params = 0

        _, _ret_override = self._lookup_return_override(address)
        if isinstance(_ret_override, list):
            # Sequenced returns are served dynamically (should_intercept →
            # execute_stub); the static bytes below are a never-executed
            # fallback, so bake the safe default instead of one list element.
            _ret_override = None

        code = bytearray()
        if ret_st0:
            if _ret_override is not None:
                # Snapshot-driven float return (same for oracle+candidate):
                # PUSH imm32 (float bits); FLD dword [ESP]; ADD ESP,4
                code += b"\x68" + struct.pack('<f', float(_ret_override))
                code += b"\xD9\x04\x24"
                code += b"\x83\xC4\x04"
            else:
                code += b"\xD9\xEE"  # FLDZ
        elif _ret_override is not None:
            # Snapshot-driven deterministic return (same for oracle+candidate)
            code += b"\xB8" + (int(_ret_override) & 0xFFFFFFFF).to_bytes(4, "little")  # MOV EAX, imm32
        elif not ret_void:
            code += b"\x31\xC0"  # XOR EAX, EAX

        if conv == 'stdcall':
            code += b"\xC2" + int(n_stack_params * 4).to_bytes(2, "little")
        else:
            code += b"\xC3"  # RET

        return bytes(code)

    def execute_stub(self, uc, address: int) -> bool:
        """Execute a callee stub at the given sentinel address.

        Runs the callee's oracle code in a sub-emulator, copies the
        return value (EAX or ST0) back to the caller's Unicorn instance.

        Returns True if handled, False if the stub is not available.
        """
        if address not in self._stub_names or self._depth >= MAX_RECURSION_DEPTH:
            return False
        stub = self._stubs.get(address)
        self._depth += 1

        try:
            from unicorn.x86_const import (
                UC_X86_REG_EAX, UC_X86_REG_EDX, UC_X86_REG_ESP,
                UC_X86_REG_ECX, UC_X86_REG_EBP,
                UC_X86_REG_ST0, UC_X86_REG_FPSW, UC_X86_REG_FPTAG,
            )
            from unicorn import UcError, UC_PROT_ALL

            def _safe_write(addr, data):
                """Write to emulator memory, auto-mapping the page if needed."""
                try:
                    uc.mem_write(addr, data)
                except UcError:
                    page = addr & ~0xFFFF
                    uc.mem_map(page, 0x10000)
                    uc.mem_write(page, b'\x00' * 0x10000)
                    uc.mem_write(addr, data)

            def _safe_read(addr, size):
                """Read from emulator memory, auto-mapping the page if needed."""
                try:
                    return bytes(uc.mem_read(addr, size))
                except UcError:
                    page = addr & ~0xFFFF
                    uc.mem_map(page, 0x10000)
                    uc.mem_write(page, b'\x00' * 0x10000)
                    return bytes(uc.mem_read(addr, size))

            # Read caller's current state
            caller_esp = uc.reg_read(UC_X86_REG_ESP)
            symbol_name = self._resolve_name(address)
            # Only synthetic stubs reach here (should_intercept returns False
            # for real-code callees), so this is exactly the set whose declared
            # convention governs the stack -- see
            # blocking_convention_mismatches().
            self._executed_stubs.add(address)

            # --- Stub argument capture (depth==1 only: top-level callee) ---
            if (self._tracer is not None and self._depth == 1
                    and not _is_chkstk_name(symbol_name)):
                _reg_map = {
                    "eax": UC_X86_REG_EAX, "ecx": UC_X86_REG_ECX,
                    "edx": UC_X86_REG_EDX, "ebp": UC_X86_REG_EBP,
                }
                # Import ESI/EDI lazily (they're in abi._uc_regs but not
                # imported at the top of execute_stub).
                try:
                    from unicorn.x86_const import (UC_X86_REG_ESI,
                                                   UC_X86_REG_EDI,
                                                   UC_X86_REG_EBX)
                    _reg_map["esi"] = UC_X86_REG_ESI
                    _reg_map["edi"] = UC_X86_REG_EDI
                    _reg_map["ebx"] = UC_X86_REG_EBX
                except ImportError:
                    pass

                _args = []
                _is_varargs = False
                _n_stack = 4  # fallback: read 4 stack dwords when no decl
                if stub is not None:
                    params = stub.abi.get("params", [])
                    _is_varargs = any(
                        getattr(p, "c_type", "") == "..." for p in params
                    )
                    # Register args first (in parameter order)
                    for p in params:
                        p_reg = getattr(p, "reg", "")
                        if p_reg:
                            reg_id = _reg_map.get(p_reg.lower())
                            if reg_id is not None:
                                _args.append(uc.reg_read(reg_id) & 0xFFFFFFFF)
                    # Stack args: only the declared fixed count
                    stack_params = [p for p in params
                                    if not getattr(p, "reg", "")
                                    and getattr(p, "c_type", "") != "..."]
                    _n_stack = len(stack_params)
                for _si in range(_n_stack):
                    try:
                        _dw = bytes(uc.mem_read(caller_esp + 4 + _si * 4, 4))
                        _args.append(int.from_bytes(_dw, "little"))
                    except Exception:
                        _args.append(0)

                # The sentinel was reached by CALL, so [ESP] holds the return
                # address (call_site + 5).  Attributes the call to its caller.
                try:
                    _ra = int.from_bytes(
                        bytes(uc.mem_read(caller_esp, 4)), "little")
                except Exception:
                    _ra = 0
                self._tracer.records.append(StubCallRecord(
                    seq=len(self._tracer.records),
                    callee_addr=address,
                    callee_name=self._stub_names.get(address, hex(address)),
                    args=_args,
                    is_varargs=_is_varargs,
                    caller_addr=_ra,
                ))
            # --- end arg capture ---

            # --- Snapshot stub_writes: the callee's buffer output ---------
            # One entry per call, advanced by a counter that is reset per run
            # (reset_stub_sequences), so oracle and candidate replay the same
            # writes at the same call index.  The pointer is read out of the
            # caller's own stack frame, so the data lands wherever THAT side
            # put its buffer -- which is the point: frame layouts differ.
            _w_key, _w_seq = self._lookup_write_override(address)
            if _w_seq:
                _w_idx = self._write_counters.get(_w_key, 0)
                self._write_counters[_w_key] = _w_idx + 1
                _w = _w_seq[_w_idx] if _w_idx < len(_w_seq) else _w_seq[-1]
                for _spec in (_w if isinstance(_w, list) else [_w]):
                    if not _spec:
                        continue
                    _argn = int(_spec.get("arg", 0))
                    _ptr = int.from_bytes(
                        _safe_read(caller_esp + 4 + 4 * _argn, 4), "little")
                    if not _ptr:
                        continue
                    if "image" in _spec:
                        # Positional form: serve the callee's OWN offset/size
                        # arguments out of a synthetic byte image.  Unlike the
                        # sequenced form this does not care how many times the
                        # callee is called or in what order, so it survives
                        # oracle/candidate call-count asymmetry (an inlined or
                        # intra-object sibling that reads the same file on one
                        # side only).
                        _img = bytes.fromhex(_spec["image"])
                        _o = int.from_bytes(_safe_read(
                            caller_esp + 4 + 4 * int(_spec["offset_arg"]), 4),
                            "little")
                        _n = int.from_bytes(_safe_read(
                            caller_esp + 4 + 4 * int(_spec["size_arg"]), 4),
                            "little")
                        if 0 < _n <= 0x10000 and _o < len(_img):
                            _safe_write(_ptr, _img[_o:_o + _n].ljust(_n, b"\x00"))
                        continue
                    _data = bytes.fromhex(_spec["data"])
                    _off = int(_spec.get("offset", 0))
                    _safe_write(_ptr + _off, _data)

            # --- Sequenced stub returns (list-valued snapshot stub_returns) ---
            # The per-name counter advances on every call and is reset via
            # reset_stub_sequences() before each oracle/candidate run, so both
            # sides replay the identical sequence.  Past the end of the list
            # the last value repeats (natural for -1 loop terminators).
            _seq_key, _seq_val = self._lookup_return_override(address)
            if isinstance(_seq_val, list) and _seq_val:
                _idx = self._seq_counters.get(_seq_key, 0)
                self._seq_counters[_seq_key] = _idx + 1
                _val = _seq_val[_idx] if _idx < len(_seq_val) else _seq_val[-1]
                if stub is not None:
                    _seq_st0 = stub.abi.get('ret_st0', False)
                    _seq_conv = stub.abi.get('conv', 'cdecl')
                    _seq_nsp = sum(1 for p in stub.abi['params'] if not p.reg)
                else:
                    _seq_st0 = isinstance(_val, float)
                    _seq_conv = 'cdecl'
                    _seq_nsp = 0
                if _seq_st0:
                    _write_st0_double(uc, float(_val))
                else:
                    uc.reg_write(UC_X86_REG_EAX, int(_val) & 0xFFFFFFFF)
                if _seq_conv == 'stdcall':
                    uc.reg_write(UC_X86_REG_ESP, caller_esp + _seq_nsp * 4)
                return True

            if symbol_name in ("fabs", "fabsf"):
                # CRT/inline float abs — cdecl, arg on stack (fabsf: dword
                # float, fabs: qword double), |x| returned in ST0.  These are
                # OUR runtime helpers (no kb decl), so without a named model
                # the candidate-side call fell through unstubbed.
                if symbol_name == "fabsf":
                    _v = struct.unpack('<f', bytes(uc.mem_read(caller_esp + 4, 4)))[0]
                else:
                    _v = struct.unpack('<d', bytes(uc.mem_read(caller_esp + 4, 8)))[0]
                _write_st0_double(uc, abs(_v))
                return True

            if symbol_name in ("atan2_", "atan2"):
                # inlines.h double atan2_(double y, double x) — cdecl,
                # two qword doubles on the stack, result in ST0.
                _y = struct.unpack('<d', bytes(uc.mem_read(caller_esp + 4, 8)))[0]
                _x = struct.unpack('<d', bytes(uc.mem_read(caller_esp + 12, 8)))[0]
                try:
                    _r = math.atan2(_y, _x)
                except ValueError:
                    _r = 0.0
                _write_st0_double(uc, _r)
                return True

            if symbol_name in ("_chkstk", "__chkstk", "chkstk", "fun_001d90e0"):
                size = uc.reg_read(UC_X86_REG_EAX) & 0xFFFFFFFF
                ret_addr = int.from_bytes(bytes(uc.mem_read(caller_esp, 4)),
                                          "little")
                new_esp = (caller_esp - size) & 0xFFFFFFFF
                _safe_write(new_esp, ret_addr.to_bytes(4, "little"))
                uc.reg_write(UC_X86_REG_ESP, new_esp)
                return True

            if symbol_name in ("csmemcpy", "memcpy"):
                dst = int.from_bytes(bytes(uc.mem_read(caller_esp + 4, 4)), "little")
                src = int.from_bytes(bytes(uc.mem_read(caller_esp + 8, 4)), "little")
                size = int.from_bytes(bytes(uc.mem_read(caller_esp + 12, 4)), "little")
                if size > 0:
                    data = _safe_read(src, size)
                    _safe_write(dst, data)
                uc.reg_write(UC_X86_REG_EAX, dst)
                return True

            # csmemcmp(a, b, size) -> <0 / 0 / >0, cdecl.  Modelled rather
            # than left as a return-0 stub: 0 means "equal", so a chain of
            # csmemcmp tests (e.g. the AIFF sample-rate table in FUN_001c6900)
            # always took its FIRST arm on both sides, the remaining arms were
            # never executed, and a wrong compare constant could not surface
            # as a divergence.
            if symbol_name in ("csmemcmp", "memcmp"):
                _a = int.from_bytes(bytes(uc.mem_read(caller_esp + 4, 4)), "little")
                _b = int.from_bytes(bytes(uc.mem_read(caller_esp + 8, 4)), "little")
                _n = int.from_bytes(bytes(uc.mem_read(caller_esp + 12, 4)), "little")
                _res = 0
                if _n > 0 and _a and _b:
                    for _x, _y in zip(_safe_read(_a, _n), _safe_read(_b, _n)):
                        if _x != _y:
                            _res = 1 if _x > _y else -1
                            break
                uc.reg_write(UC_X86_REG_EAX, _res & 0xFFFFFFFF)
                return True

            if symbol_name == "csstrncpy":
                dst = int.from_bytes(bytes(uc.mem_read(caller_esp + 4, 4)), "little")
                src = int.from_bytes(bytes(uc.mem_read(caller_esp + 8, 4)), "little")
                n = int.from_bytes(bytes(uc.mem_read(caller_esp + 12, 4)), "little")
                if n > 0:
                    data = _safe_read(src, n)
                    idx = data.find(b'\0')
                    if idx != -1:
                        data = data[:idx+1]
                    _safe_write(dst, data)
                uc.reg_write(UC_X86_REG_EAX, dst)
                return True

            if symbol_name in ("csmemset", "memset"):
                dst = int.from_bytes(bytes(uc.mem_read(caller_esp + 4, 4)), "little")
                val = int.from_bytes(bytes(uc.mem_read(caller_esp + 8, 4)), "little") & 0xFF
                n = int.from_bytes(bytes(uc.mem_read(caller_esp + 12, 4)), "little")
                if n > 0:
                    _safe_write(dst, bytes([val] * n))
                uc.reg_write(UC_X86_REG_EAX, dst)
                return True

            if symbol_name == "csstrncpy":
                dst = int.from_bytes(bytes(uc.mem_read(caller_esp + 4, 4)), "little")
                src = int.from_bytes(bytes(uc.mem_read(caller_esp + 8, 4)), "little")
                n = int.from_bytes(bytes(uc.mem_read(caller_esp + 12, 4)), "little")
                if n > 0:
                    data = bytes(uc.mem_read(src, n))
                    idx = data.find(b'\0')
                    if idx != -1:
                        data = data[:idx+1]
                    uc.mem_write(dst, data)
                uc.reg_write(UC_X86_REG_EAX, dst)
                return True

            if symbol_name in ("csmemset", "memset"):
                dst = int.from_bytes(bytes(uc.mem_read(caller_esp + 4, 4)), "little")
                val = int.from_bytes(bytes(uc.mem_read(caller_esp + 8, 4)), "little") & 0xFF
                n = int.from_bytes(bytes(uc.mem_read(caller_esp + 12, 4)), "little")
                if n > 0:
                    uc.mem_write(dst, bytes([val] * n))
                uc.reg_write(UC_X86_REG_EAX, dst)
                return True

            if symbol_name in ("crt_sprintf", "debug_string_to_display"):
                uc.reg_write(UC_X86_REG_EAX, 0)
                return True

            if symbol_name in ("system_exit", "halt_and_catch_fire"):
                uc.emu_stop()
                return True

            if symbol_name in self._FTOL2_ADDRS:
                import struct as _struct
                from unicorn.x86_const import UC_X86_REG_FP0
                # UC_X86_REG_FP0..FP7 are the PHYSICAL x87 data registers; reading
                # one returns a (mantissa: uint64, exponent: uint16) tuple for the
                # full 80-bit value.  UC_X86_REG_ST0..ST7 are LOGICAL but only
                # return the 64-bit mantissa (no exponent word — unusable for decode).
                # Logical ST0 = physical R[TOP]; TOP = FPSW bits 13-11.
                fpsw = uc.reg_read(UC_X86_REG_FPSW)
                old_top = (fpsw >> 11) & 0x7
                fp = uc.reg_read(UC_X86_REG_FP0 + old_top)
                mantissa, exponent_word = fp
                # Reassemble as 10 bytes and use the existing robust helper.
                raw80 = _struct.pack('<QH', mantissa, exponent_word)
                try:
                    val = _st80_to_double(raw80)
                except (OverflowError, ValueError, ZeroDivisionError):
                    val = float('inf')
                try:
                    result = int(val)  # truncates toward zero, matching _ftol2
                except (OverflowError, ValueError):
                    result = 0x80000000  # MSVC _ftol2 saturates to INT_MIN on overflow
                uc.reg_write(UC_X86_REG_EAX, result & 0xFFFFFFFF)
                # Pop ST0 from the x87 stack: increment TOP in FPSW (bits 13-11)
                # and mark the vacated physical slot as empty (11b) in FPTAG.
                new_top = (old_top + 1) & 0x7
                uc.reg_write(UC_X86_REG_FPSW, (fpsw & ~(0x7 << 11)) | (new_top << 11))
                fptag = uc.reg_read(UC_X86_REG_FPTAG)
                uc.reg_write(UC_X86_REG_FPTAG, fptag | (0x3 << (old_top * 2)))
                return True

            if symbol_name == "display_assert":
                uc.reg_write(UC_X86_REG_EAX, 0)
                return True

            if symbol_name == "datum_get":
                # datum_get(data_t *data @stack+4, int datum_handle @stack+8)
                # Returns pointer to entry if salt matches identifier, else NULL.
                try:
                    data_ptr = int.from_bytes(bytes(uc.mem_read(caller_esp + 4, 4)), "little")
                    handle = int.from_bytes(bytes(uc.mem_read(caller_esp + 8, 4)), "little")
                    import sys as _sys
                    _g0_b = bytes(uc.mem_read(0x500000, 4)); _g0 = int.from_bytes(_g0_b, "little"); from unicorn.x86_const import UC_X86_REG_ECX as _ECX_R, UC_X86_REG_EAX as _EAX_R; _ecx_v = uc.reg_read(_ECX_R); _eax_v = uc.reg_read(_EAX_R); print(f"[datum_get] data_ptr=0x{data_ptr:08x} handle=0x{handle:08x} glob0=0x{_g0:08x} ECX=0x{_ecx_v:08x} EAX=0x{_eax_v:08x} esp=0x{caller_esp:08x}", file=_sys.stderr)
                    if data_ptr:
                        identifier = (handle >> 16) & 0xFFFF
                        index = handle & 0xFFFF
                        current_count = int.from_bytes(_safe_read(data_ptr + 0x2e, 2), "little")
                        elem_size = int.from_bytes(_safe_read(data_ptr + 0x22, 2), "little")
                        arr_ptr = int.from_bytes(_safe_read(data_ptr + 0x34, 4), "little")
                        print(f"[datum_get] id={identifier} idx={index} count={current_count} sz={elem_size} arr=0x{arr_ptr:08x}", file=_sys.stderr)
                        if index < current_count and elem_size > 0 and arr_ptr:
                            entry_addr = arr_ptr + index * elem_size
                            salt = int.from_bytes(_safe_read(entry_addr, 2), "little")
                            print(f"[datum_get] entry=0x{entry_addr:08x} salt={salt}", file=_sys.stderr)
                            if salt != 0 and (identifier == 0 or identifier == salt):
                                uc.reg_write(UC_X86_REG_EAX, entry_addr)
                                return True
                except Exception as _e:
                    import sys as _sys
                    print(f"[datum_get] exception: {_e}", file=_sys.stderr)
                uc.reg_write(UC_X86_REG_EAX, 0)
                return True

            if symbol_name == "object_get_and_verify_type":
                # Replicate datum_get + type check without calling the real function.
                # datum array pointer lives at 0x5A8D50 in the XBE's global data.
                # object_header_data_t entry layout (12 bytes):
                #   +0x00 uint16 salt, +0x02 uint16 object_type_byte (byte[3] used),
                #   +0x08 ptr object_data_t*
                # object_data_t.type (int16) is at obj_ptr+0x64.
                try:
                    datum_handle = int.from_bytes(_safe_read(caller_esp + 4, 4), "little")
                    type_mask = int.from_bytes(_safe_read(caller_esp + 8, 4), "little")
                    arr_ptr = int.from_bytes(_safe_read(0x5A8D50, 4), "little")
                    if arr_ptr:
                        max_elements = int.from_bytes(_safe_read(arr_ptr + 0x20, 2), "little")
                        elem_size = int.from_bytes(_safe_read(arr_ptr + 0x22, 2), "little")
                        data_ptr = int.from_bytes(_safe_read(arr_ptr + 0x34, 4), "little")
                        index = datum_handle & 0xFFFF
                        handle_salt = (datum_handle >> 16) & 0xFFFF
                        if (index < max_elements and elem_size >= 12
                                and data_ptr and handle_salt):
                            entry_addr = data_ptr + index * elem_size
                            entry_salt = int.from_bytes(_safe_read(entry_addr, 2), "little")
                            if entry_salt == handle_salt:
                                obj_ptr = int.from_bytes(
                                    _safe_read(entry_addr + 8, 4), "little")
                                if obj_ptr:
                                    raw16 = _safe_read(obj_ptr + 0x64, 2)
                                    obj_type = struct.unpack('<h', raw16)[0]
                                    if type_mask & (1 << (obj_type & 0x1f)):
                                        uc.reg_write(UC_X86_REG_EAX, obj_ptr)
                                        return True
                except Exception:
                    pass
                uc.reg_write(UC_X86_REG_EAX, 0)
                return True

            if symbol_name == "global_scenario_get":
                try:
                    uc.reg_write(UC_X86_REG_EAX,
                                 int.from_bytes(_safe_read(0x5064E4, 4),
                                                "little"))
                except Exception:
                    uc.reg_write(UC_X86_REG_EAX, 0)
                return True

            if symbol_name == "scenario_get":
                try:
                    uc.reg_write(UC_X86_REG_EAX,
                                 int.from_bytes(_safe_read(0x5064E0, 4),
                                                "little"))
                except Exception:
                    uc.reg_write(UC_X86_REG_EAX, 0)
                return True

            if symbol_name == "tag_block_get_element":
                result = 0
                try:
                    block = int.from_bytes(_safe_read(caller_esp + 4, 4),
                                           "little")
                    index = int.from_bytes(_safe_read(caller_esp + 8, 4),
                                           "little", signed=True)
                    element_size = int.from_bytes(
                        _safe_read(caller_esp + 12, 4), "little", signed=True)
                    count = int.from_bytes(_safe_read(block, 4), "little",
                                           signed=True)
                    address_ptr = int.from_bytes(_safe_read(block + 4, 4),
                                                 "little")
                    if (block and address_ptr and element_size > 0
                            and 0 <= index < count):
                        result = (address_ptr + index * element_size) & 0xFFFFFFFF
                except Exception:
                    pass
                uc.reg_write(UC_X86_REG_EAX, result)
                return True

            if symbol_name == "tag_get":
                # OPT-IN real tag resolution (gated by env BIPED_REAL_TAGS=1).
                # Mirrors FUN_001b9bf0 + tag_get: the tag-instances table base
                # lives at 0x5054F0; entry = base + (handle & 0xFFFF)*0x20; the
                # tag body pointer is at entry+0x14 (piVar1[5]). Both oracle and
                # candidate call this same stub, so resolving the REAL bipd/antr
                # tag body (present in a biped state-snapshot) drives both sides
                # down the real path (node-copy loops, jump speed, phase
                # boundaries) that the synthetic projectile tag below zeros out.
                #
                # Falls through to the synthetic projectile tag when the flag is
                # off OR the table/handle does not resolve in snapshot memory, so
                # the projectile path (FUN_000f9c40 / infection_swarm) is
                # bit-for-bit unaffected.
                if os.environ.get("BIPED_REAL_TAGS") == "1":
                    try:
                        group = int.from_bytes(_safe_read(caller_esp + 4, 4), "little")
                        handle = int.from_bytes(_safe_read(caller_esp + 8, 4), "little")
                        tbl_base = int.from_bytes(_safe_read(0x5054F0, 4), "little")
                        idx = handle & 0xFFFF
                        if tbl_base and idx != 0xFFFF:
                            entry = tbl_base + idx * 0x20
                            g0 = int.from_bytes(_safe_read(entry, 4), "little")
                            g1 = int.from_bytes(_safe_read(entry + 4, 4), "little")
                            g2 = int.from_bytes(_safe_read(entry + 8, 4), "little")
                            body = int.from_bytes(_safe_read(entry + 0x14, 4), "little")
                            # Accept only when the requested group matches one of
                            # the entry's three group sigs (bipd/unit/obje etc.)
                            # and the body pointer is a plausible heap address.
                            if (group in (g0, g1, g2) and body
                                    and 0x10000 <= body < 0x84000000):
                                uc.reg_write(UC_X86_REG_EAX, body)
                                return True
                    except Exception:
                        pass
                    # else fall through to synthetic tag (resolution failed)
                # Return a pointer to a synthetic projectile tag block.
                # tag_get(group_tag, datum_handle) → void* tag_data
                # Projectile physics (FUN_000f9c40) uses these key offsets:
                #   +0x1c8 max_range (float): must be non-zero to avoid FUN_000f7e40
                #           intra-COFF call in the speed-clamp/deceleration section.
                #   +0x1b8 / +0x198 / +0x220: tag references (-1 = absent).
                _SYNTH_TAG_ADDR = 0x601000
                _SYNTH_TAG_SIZE = 0x280
                _SYNTH_SENTINEL = b'\xDE\xAD\xBE\xEF'
                try:
                    _tail = bytes(uc.mem_read(_SYNTH_TAG_ADDR + _SYNTH_TAG_SIZE - 4, 4))
                    _initialized = (_tail == _SYNTH_SENTINEL)
                except UcError:
                    _initialized = False
                if not _initialized:
                    import struct as _struct
                    _page = _SYNTH_TAG_ADDR & ~0xFFFF
                    try:
                        uc.mem_map(_page, 0x10000)
                    except UcError:
                        pass
                    uc.mem_write(_page, b'\x00' * 0x10000)
                    _tag_bytes = bytearray(_SYNTH_TAG_SIZE)
                    _struct.pack_into('<f',  _tag_bytes, 0x1c8, 60.0)   # max_range (non-zero)
                    _struct.pack_into('<f',  _tag_bytes, 0x1cc, 0.0)    # gravity_scale_normal
                    _struct.pack_into('<f',  _tag_bytes, 0x1d8, 0.0)    # gravity_scale_super
                    _struct.pack_into('<f',  _tag_bytes, 0x1e8, 0.0)    # min_speed
                    _struct.pack_into('<f',  _tag_bytes, 0x1ec, 0.0)    # lock_on_rate (no steering)
                    _struct.pack_into('<i',  _tag_bytes, 0x1b8, -1)     # effect_tag (absent)
                    _struct.pack_into('<i',  _tag_bytes, 0x198, -1)     # burst_centre_effect (absent)
                    _struct.pack_into('<i',  _tag_bytes, 0x220, -1)     # area_damage_tag (absent)
                    _tag_bytes[_SYNTH_TAG_SIZE - 4:] = _SYNTH_SENTINEL
                    uc.mem_write(_SYNTH_TAG_ADDR, bytes(_tag_bytes))
                uc.reg_write(UC_X86_REG_EAX, _SYNTH_TAG_ADDR)
                return True

            # real_vector3d_valid / valid_real_point3d / valid_real_normal3d_perpendicular /
            # valid_real_vector3d: cdecl (float* vec) -> bool
            # Return 1 if all three floats at the pointer are finite, 0 otherwise.
            _VECTOR3D_VALID = frozenset((
                "real_vector3d_valid", "valid_real_point3d",
                "valid_real_normal3d_perpendicular", "valid_real_vector3d",
            ))
            if symbol_name in _VECTOR3D_VALID:
                result = 0
                try:
                    vec_ptr = int.from_bytes(_safe_read(caller_esp + 4, 4), "little")
                    if vec_ptr:
                        raw = _safe_read(vec_ptr, 12)
                        x, y, z = struct.unpack('<fff', raw)
                        if math.isfinite(x) and math.isfinite(y) and math.isfinite(z):
                            result = 1
                except Exception:
                    pass
                uc.reg_write(UC_X86_REG_EAX, result)
                return True

            # _CI* CRT math intrinsics: arg(s) in ST0 (and ST1), result in ST0
            _CI_ONE_ARG = {
                "ciacos": math.acos, "ciasin": math.asin,
                "cisin": math.sin, "cicos": math.cos, "citan": math.tan,
                "cisqrt": math.sqrt, "cilog": math.log, "cilog10": math.log10,
            }
            _CI_TWO_ARG = {
                "ciatan2": math.atan2, "cipow": math.pow, "cifmod": math.fmod,
            }
            if symbol_name in _CI_ONE_ARG:
                import struct as _st
                st0_raw = uc.reg_read(UC_X86_REG_ST0)
                st0_bytes = st0_raw.to_bytes(10, 'little')
                val = _st80_to_double(st0_bytes)
                try:
                    result = _CI_ONE_ARG[symbol_name](val)
                except (ValueError, OverflowError):
                    result = 0.0
                _write_st0_double(uc, result)
                return True

            if symbol_name in _CI_TWO_ARG:
                import struct as _st
                from unicorn.x86_const import UC_X86_REG_ST1
                st0_raw = uc.reg_read(UC_X86_REG_ST0)
                st1_raw = uc.reg_read(UC_X86_REG_ST1)
                st0_val = _st80_to_double(st0_raw.to_bytes(10, 'little'))
                st1_val = _st80_to_double(st1_raw.to_bytes(10, 'little'))
                try:
                    # MSVC _CI two-arg convention: the first C argument is pushed
                    # first (→ ST1) and the second last (→ ST0). So the faithful
                    # call is func(ST1, ST0): atan2(y=ST1,x=ST0), fmod(x=ST1,y=ST0),
                    # pow(base=ST1,exp=ST0). (Previously passed (st0,st1) — swapped.)
                    result = _CI_TWO_ARG[symbol_name](st1_val, st0_val)
                except (ValueError, OverflowError, ZeroDivisionError):
                    result = 0.0
                _write_st0_double(uc, result)
                return True

            # For now, return 0 from all stubs.
            # Full sub-emulator execution is deferred to avoid complexity.
            # The key win here is that the calling function still executes
            # correctly through the call site, testing the pre-call setup
            # and post-call usage of the return value.
            if stub is not None:
                ret_st0 = stub.abi.get('ret_st0', False)
                ret_void = stub.abi.get('ret_void', True)
                conv = stub.abi.get('conv', 'cdecl')
                n_stack_params = sum(1 for p in stub.abi['params'] if not p.reg)
            else:
                ret_st0 = False
                ret_void = False
                conv = 'cdecl'
                n_stack_params = 0

            if ret_st0:
                # Push 0.0 onto FPU stack
                pass  # ST0 is already undefined; caller will use it as-is
            elif not ret_void:
                _key, _ret_val = self._lookup_return_override(address)
                _ret = _ret_val if _ret_val is not None else 0
                uc.reg_write(UC_X86_REG_EAX, _ret)

            # Clean up stack based on calling convention
            if conv == 'stdcall':
                uc.reg_write(UC_X86_REG_ESP, caller_esp + n_stack_params * 4)

        finally:
            self._depth -= 1

        return True
