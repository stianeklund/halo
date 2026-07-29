#!/usr/bin/env python3
"""Triage batch_verify divergences and maintain the divergence ledger.

A batch reports N divergences; that number is not actionable on its own,
because most divergences are produced by the harness rather than by the lift.
Each failure is classified from its smoke log into a category, and every
category maps to one of four buckets:

  suspect-real      candidate lift bug — investigate
  needs-evidence    cannot be adjudicated from the smoke log alone
  benign-difference provably behaviour-identical source difference
  harness-artifact  the emulator, not the lift, produced the difference

Categories, by bucket:

  suspect-real
    - arg_mismatch: wrong argument to a named non-assert callee. The strongest
      evidence class — this is the lift-learnings §10 caller-side swap/drop
      signature, and it names the callee and argument index.
    - call_seq: the two sides called different callees, or in a different order
    - genuine: both sides complete normally, computed results differ

  needs-evidence
    - globals_reloc_placement: one side holds a DIR32 slot address, the other
      an XBE data address — confirm both denote the same global
    - assert_call_seq: call sequence diverged around an assert path
    - insn_limit / exec_asymmetry / stack_divergence / stub_asymmetry
    - scratch_divergence: only the scratch buffer differs (constant/global)
    - coverage_limited: <30% coverage, only the early-exit path tested
    - unknown: doesn't fit other categories

  benign-difference
    - benign_arg_width: memset fill literal written 0xff instead of -1; the
      argument is converted to unsigned char, so the bytes written are
      identical. Worth fixing for structural (VC71/IMM) match, not a bug.

  harness-artifact
    - assert_metadata: only _display_assert's message/__FILE__/__LINE__ args
      differ. Our sources do not reproduce the original's line numbering.
    - stack_ptr_args: only stack-pointer args differ (MSVC vs clang frames)
    - load_width: one side reads the field wider than the other (int vs int16/8)
    - dirty_eax: oracle uses mov ax/al, upper EAX bits are stale
    - stub_residual: oracle EAX is callee stub garbage, lifted returns 0
    - leaf_mismatch: lifted is a leaf (same-TU callees), crashes on garbage
    - ftol2 / intrinsic: oracle calls a CRT intrinsic that is stubbed to 0
    - assert_path: divergence only on seeds that trigger assert/halt
    - stale: the smoke log passes now; the batch result is outdated

Usage:
    python3 tools/equivalence/triage_failures.py
    python3 tools/equivalence/triage_failures.py --details
    python3 tools/equivalence/triage_failures.py --category arg_mismatch

    # Write/merge the committed ledger and print the prioritized bug list
    python3 tools/equivalence/triage_failures.py \\
        --summary-path artifacts/batch_verify/summary.json --ledger --bug-list

Self-test: python3 tools/equivalence/test_triage_classify.py
Doc:       docs/equivalence-testing.md
"""

import argparse
import json
import re
import sys
from collections import Counter, defaultdict
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
SUMMARY = ROOT / "artifacts" / "batch_verify_full" / "summary.json"
RESULTS_DIR = ROOT / "artifacts" / "batch_verify_full"
SMOKE_DIR = ROOT / "artifacts" / "equivalence"
VC71_DB = ROOT / "artifacts" / "verify_cache" / "vc71.sqlite"
# The ledger lives beside leaf_cache.json / batch_verify_allowlist.json because
# it must be committed, and artifacts/equivalence/ is hard-gitignored ("must
# NEVER be committed" — it holds game-state snapshots and logs).
LEDGER = ROOT / "tools" / "equivalence" / "divergence_ledger.json"

FTOL2_NAMES = {"FUN_001d9068", "_ftol2", "ftol2"}
CRT_INTRINSICS = {
    "FUN_001d9068": "_ftol2",
    "_ftol2": "_ftol2",
    "FUN_001d90e0": "_chkstk",
    "_chkstk": "_chkstk",
    "FUN_001d9030": "_ftol",
    "FUN_001dd5c8": "__SEH_prolog",
    "FUN_001dd601": "__SEH_epilog",
    "FUN_001dd620": "_allmul",
    "FUN_001dd660": "_aullshr",
    "FUN_001dd680": "_aullrem",
    "FUN_001dd770": "_aulldiv",
    "_CIlog": "_CIlog",
    "_CIpow": "_CIpow",
    "_CIlog10": "_CIlog10",
    "_CIsqrt": "_CIsqrt",
    "_CIatan2": "_CIatan2",
    "_CIsin": "_CIsin",
    "_CIcos": "_CIcos",
    "_CIfmod": "_CIfmod",
    "_CIacos": "_CIacos",
    "_CIasin": "_CIasin",
    "_CItan": "_CItan",
}
HALT_NAMES = {"FUN_001029a0", "_system_exit", "system_exit",
              "_halt_and_catch_fire", "halt_and_catch_fire"}
ASSERT_NAMES = {"FUN_0008d9f0", "_display_assert", "display_assert"}

# Callees whose arguments are assert *metadata* (message pointer, __FILE__
# pointer, __LINE__ number) rather than computed values. A divergence in these
# args is expected: our lifted sources do not reproduce the original's line
# numbering, and our string literals land in a different section than the
# original's. See docs/lift-learnings.md §29 and the header-recovery skill.
ASSERT_ARG_CALLEES = ASSERT_NAMES | {"_assert", "assert_halt"}

# csmemset(void *buffer, int c, size_t size) — `c` is converted to unsigned
# char, so only its low byte is observable. memset(p, -1, n) and
# memset(p, 0xff, n) write identical bytes. All 92 fill-argument divergences in
# the 2026-07-28 batch were exactly that pair.
BYTE_FILL_CALLEES = {"_csmemset", "csmemset", "_memset", "memset"}
BYTE_FILL_ARG = 1

# Synthetic DIR32 slot arena. unicorn_diff gives the two sides DISJOINT slot
# ranges -- `lft_globals_base = GLOBALS_BASE + len(orc_data_slots) * 256`
# (unicorn_diff.py) -- and the candidate's base can therefore run past the
# nominal 0x600000 top. A reference to a global can end up relocated into a
# slot on one side while the other reads the original XBE data address; see
# concolic._is_spurious_address, which rejects this region for the same reason.
GLOBALS_ARENA_LO = 0x00500000
GLOBALS_ARENA_HI = 0x00800000

# XBE image address window. Anything in here is a static pointer (string
# literal, table); anything below LINE_MAX is plausibly a __LINE__ number.
XBE_IMAGE_LO = 0x00010000
XBE_IMAGE_HI = 0x00800000
LINE_MAX = 100000

# Where each category lands. Three buckets:
#   harness-artifact — the emulator, not the lift, produced the difference
#   needs-evidence   — cannot be adjudicated from the smoke log alone
#   suspect-real     — candidate lift bug, investigate
BUCKETS = {
    "assert_metadata": "harness-artifact",
    "stack_ptr_args": "harness-artifact",
    "dirty_eax": "harness-artifact",
    "stub_residual": "harness-artifact",
    "leaf_mismatch": "harness-artifact",
    "ftol2": "harness-artifact",
    "intrinsic": "harness-artifact",
    "assert_path": "harness-artifact",
    "stale": "harness-artifact",
    # One side ended early, or the two lists are off by one. Both sides agree on
    # every call they both made, so neither is a control-flow difference.
    "call_seq_truncated": "harness-artifact",
    "call_seq_shifted": "harness-artifact",
    # One side CALLed a callee the other INLINED, so the two surviving sequences
    # describe different amounts of code and cannot be compared. NOT filed as an
    # artifact: the comparison is uninformative, which is not the same as proven
    # benign, and a real bug could sit behind it unnoticed.
    "call_seq_inline_asymmetry": "needs-evidence",
    # A real lift bug: the field/return type is the wrong width on one side.
    "load_width": "suspect-real",
    "insn_limit": "needs-evidence",
    "exec_asymmetry": "needs-evidence",
    "stack_divergence": "needs-evidence",
    "stub_asymmetry": "needs-evidence",
    "scratch_divergence": "needs-evidence",
    "coverage_limited": "needs-evidence",
    "assert_call_seq": "needs-evidence",
    "globals_reloc_placement": "needs-evidence",
    "unknown": "needs-evidence",
    "call_seq": "suspect-real",
    "arg_mismatch": "suspect-real",
    "genuine": "suspect-real",
    # Provably behaviour-identical source differences. Not a bug, but worth
    # fixing for structural (VC71/IMM) match.
    "benign_arg_width": "benign-difference",
}


def parse_smoke_log(path: Path) -> dict:
    """Extract triage-relevant signals from a smoke log."""
    if not path.exists():
        return {"exists": False}

    text = path.read_text(encoding="utf-8", errors="replace")
    info = {"exists": True, "path": str(path)}

    # Extract stub calls from [stub] lines
    stub_calls = []
    for m in re.finditer(r'\[stub\]\s+(\S+)\s+@\s+(0x[0-9a-f]+)', text):
        stub_calls.append(m.group(1))
    info["stub_calls"] = stub_calls
    info["unique_stubs"] = sorted(set(stub_calls))

    # Check for ftol2 calls
    info["has_ftol2"] = any(s in FTOL2_NAMES for s in stub_calls)

    # Check for CRT intrinsic calls
    intrinsics_hit = set()
    for s in stub_calls:
        if s in CRT_INTRINSICS:
            intrinsics_hit.add(CRT_INTRINSICS[s])
    info["intrinsics"] = sorted(intrinsics_hit)

    # Check for assert/halt stub calls
    info["has_assert"] = any(s in ASSERT_NAMES for s in stub_calls)
    info["has_halt"] = any(s in HALT_NAMES for s in stub_calls)

    # Extract FAIL lines with details
    fails = []
    for m in re.finditer(r'seed\[\s*(\d+)\]\s+FAIL:\s+(.*)', text):
        fails.append({"seed": int(m.group(1)), "diff": m.group(2).strip()})
    info["fails"] = fails

    # Extract oracle ESP_delta from first divergence
    esp_deltas = []
    for m in re.finditer(r'ESP_delta=(-?\d+)', text):
        esp_deltas.append(int(m.group(1)))
    info["esp_deltas"] = esp_deltas

    # Extract oracle INSN_count
    insn_counts = []
    for m in re.finditer(r'INSN_count=(\d+)', text):
        insn_counts.append(int(m.group(1)))
    info["insn_counts"] = insn_counts

    # Coverage
    m = re.search(r'coverage:.*?\(([\d.]+)%\)', text)
    if m:
        info["coverage"] = float(m.group(1))

    # Confidence
    m = re.search(r'confidence:\s+(\w+)', text)
    if m:
        info["confidence"] = m.group(1)

    # Oracle/lifted class
    m = re.search(r'oracle class:\s+(\w+)', text)
    if m:
        info["oracle_class"] = m.group(1)
    m = re.search(r'lifted class:\s+(\w+)', text)
    if m:
        info["lifted_class"] = m.group(1)

    # Extract reloc symbols (to detect asymmetric stubs)
    orc_relocs = []
    lft_relocs = []
    for m in re.finditer(r'\[RELOC\]\s+(oracle|lifted):\s+\'([^\']+)\'', text):
        if m.group(1) == "oracle":
            orc_relocs.append(m.group(2))
        else:
            lft_relocs.append(m.group(2))
    info["oracle_relocs"] = orc_relocs
    info["lifted_relocs"] = lft_relocs

    # CRASH lines
    crashes = re.findall(r'(ORACLE|LIFTED)-CRASH:\s+(.*)', text)
    info["crashes"] = crashes

    # --- Stub-argument differential -------------------------------------
    # This is the dominant failure shape (272 of 331 in the 2026-07-28 batch)
    # and the classifier below is the only thing that can see it.
    stub_arg_lines = re.findall(r'FAIL:\s*stub-args:\s*(.*)', text)
    info["stub_arg_lines"] = stub_arg_lines
    info["stub_call_seq"] = any("call-seq diverged" in ln for ln in stub_arg_lines)
    info["stub_arg_mismatch"] = any("arg mismatch" in ln for ln in stub_arg_lines)
    info["stub_stack_ptr"] = any("stack-ptr arg" in ln for ln in stub_arg_lines)

    # Per-callee argument divergences:
    #   seed[ N] call[K] <callee> arg[J]: oracle=0x.. candidate=0x..
    arg_diffs = []
    for m in re.finditer(
        r'call\[(\d+)\]\s+(\S+)\s+arg\[(\d+)\]:\s*'
        r'oracle=(0x[0-9a-f]+)\s+candidate=(0x[0-9a-f]+)', text
    ):
        arg_diffs.append({
            "call": int(m.group(1)),
            "callee": m.group(2),
            "arg": int(m.group(3)),
            "oracle": int(m.group(4), 16),
            "candidate": int(m.group(5), 16),
        })
    info["arg_diffs"] = arg_diffs
    info["arg_diff_callees"] = sorted({d["callee"] for d in arg_diffs})

    # How a call-sequence divergence is shaped, when the harness recorded it.
    # Emitted by stubs.py::sequence_detail. Absent on logs written before
    # 2026-07-29, which is why _seq_relation-based categories only apply when
    # the marker is actually present -- an older log keeps the old verdict
    # rather than being silently reclassified on missing evidence.
    m = re.search(r'call-seq (TRUNCATED|SHIFTED|DIVERGENT|INLINE-ASYMMETRY)', text)
    info["seq_relation"] = m.group(1).lower() if m else ""

    # Which comparison fields actually failed, independent of stub-args
    info["fail_eax"] = bool(re.search(r'FAIL:\s*EAX', text))
    info["fail_scratch"] = bool(re.search(r'FAIL:\s*scratch', text))
    info["fail_st0"] = bool(re.search(r'FAIL:\s*ST0', text))

    # Stubs prepared ratio
    m = re.search(r'stubs prepared:\s*(\d+)/(\d+)', text)
    if m:
        info["stubs_prepared"] = int(m.group(1))
        info["stubs_total"] = int(m.group(2))

    # Per-side INSN_count and ESP_delta from first divergence block
    m = re.search(r'\[oracle\].*?INSN_count=(\d+)', text, re.DOTALL)
    if m:
        info["oracle_insn"] = int(m.group(1))
    m = re.search(r'\[lifted\].*?INSN_count=(\d+)', text, re.DOTALL)
    if m:
        info["lifted_insn"] = int(m.group(1))
    m = re.search(r'\[oracle\].*?ESP_delta=(-?\d+)', text, re.DOTALL)
    if m:
        info["oracle_esp"] = int(m.group(1))
    m = re.search(r'\[lifted\].*?ESP_delta=(-?\d+)', text, re.DOTALL)
    if m:
        info["lifted_esp"] = int(m.group(1))

    return info


def _is_dirty_eax(fails: list) -> bool:
    """Check if EAX divergence is only in upper bits (mov ax/mov al artifact).

    Matches both mov ax (low 16 match) and mov al (low 8 match) patterns.
    Also catches the case where oracle EAX is constant across all seeds
    (same stale upper bits, consistent return value).
    """
    eax_diffs = [f for f in fails if "EAX:" in f["diff"]]
    if not eax_diffs:
        return False
    for f in eax_diffs:
        m = re.search(r'EAX: oracle=(0x[0-9a-f]+) lifted=(0x[0-9a-f]+)', f["diff"])
        if not m:
            return False
        orc = int(m.group(1), 16)
        lft = int(m.group(2), 16)
        if orc == lft:
            return False
        # mov ax: low 16 bits match
        if (orc & 0xFFFF) == (lft & 0xFFFF):
            continue
        # mov al: low 8 bits match, upper 24 differ
        if (orc & 0xFF) == (lft & 0xFF):
            continue
        return False
    return True


def _repeated_byte32(v: int):
    """If `v` is one non-zero byte repeated across all 4 bytes, return it."""
    b = [(v >> s) & 0xFF for s in (0, 8, 16, 24)]
    return b[0] if len(set(b)) == 1 and b[0] != 0 else None


def _is_load_width(fails: list) -> bool:
    """True if every EAX divergence is one side reading a field WIDER than the other.

    Shape: one side is a 32-bit repeated-byte value (0xcccccccc, 0xffffffff --
    the wide read pulled in adjacent fill bytes) and the other is that same byte
    zero-extended from 8 or 16 bits (0x000000cc, 0x0000ffff). Both sides read the
    same memory; only the WIDTH differs, which means one of them has the field's
    type wrong (int vs int16_t/int8_t, or a `short` return declared `int`).

    This must be checked BEFORE _is_dirty_eax, which the same shape also
    satisfies (the low bits match, the upper bits differ) and which buckets it as
    a harness artifact. A `mov ax` leaving stale upper bits is benign; reading a
    narrow field at the wrong width is a real lift bug, so they cannot share a
    category. The discriminator is the repeated-byte fill: stale upper bits are
    arbitrary leftovers, whereas a too-wide read of fill memory yields the fill
    pattern itself.
    """
    eax_diffs = [f for f in fails if "EAX:" in f["diff"]]
    if not eax_diffs:
        return False
    for f in eax_diffs:
        m = re.search(r'EAX: oracle=(0x[0-9a-f]+) lifted=(0x[0-9a-f]+)', f["diff"])
        if not m:
            return False
        orc = int(m.group(1), 16)
        lft = int(m.group(2), 16)
        if orc == lft:
            return False
        wide, narrow = (lft, orc) if _repeated_byte32(lft) else (orc, lft)
        fill = _repeated_byte32(wide)
        if fill is None:
            return False
        # The narrow side must be that same byte zero-extended from 8 or 16 bits.
        if narrow not in (fill, (fill << 8) | fill):
            return False
    return True


def _is_stub_residual(fails: list) -> bool:
    """Check if oracle EAX is random garbage while lifted EAX is 0 or small.

    Pattern: stub callee leaves its return value in EAX, oracle doesn't clear
    it before returning. Lifted code uses XOR EAX,EAX. Detectable because
    oracle EAX values vary wildly across seeds while lifted is constant 0.
    """
    eax_diffs = [f for f in fails if "EAX:" in f["diff"]]
    if len(eax_diffs) < 3:
        return False
    orc_vals = set()
    lft_vals = set()
    for f in eax_diffs:
        m = re.search(r'EAX: oracle=(0x[0-9a-f]+) lifted=(0x[0-9a-f]+)', f["diff"])
        if not m:
            return False
        orc_vals.add(int(m.group(1), 16))
        lft_vals.add(int(m.group(2), 16))
    # Oracle has many distinct values (garbage), lifted has 1-2 (constant)
    return len(orc_vals) >= 3 and len(lft_vals) <= 2


def _is_assert_metadata(diff: dict) -> bool:
    """True if one arg divergence is explainable as assert metadata.

    Two shapes qualify:
      - both sides are small integers  -> __LINE__, which our sources do not
        reproduce (e.g. oracle=0xa89 candidate=0x374)
      - both sides are static image addresses -> the message / __FILE__ string
        literal, placed in a different section by our compiler
    A value that is neither (a stack address, a computed pointer, garbage) does
    NOT qualify, so a genuinely wrong argument still shows through.
    """
    o, c = diff["oracle"], diff["candidate"]
    if o == c:
        return True
    if o < LINE_MAX and c < LINE_MAX:
        return True
    in_img = (XBE_IMAGE_LO <= o < XBE_IMAGE_HI) and (XBE_IMAGE_LO <= c < XBE_IMAGE_HI)
    return in_img


def _is_benign_arg_diff(diff: dict) -> bool:
    """True if the two argument values are interchangeable at the callee.

    Only claims made from the callee's own contract count here. memset's fill
    argument is the one instance in the current set: it is converted to
    unsigned char, so oracle=0xffffffff (a source `-1`) and candidate=0xff
    write the same bytes. The lift is still structurally wrong -- the literal
    should be -1 -- but it is not a behavioural bug.
    """
    if diff["oracle"] == diff["candidate"]:
        return True
    if diff["callee"] in BYTE_FILL_CALLEES and diff["arg"] == BYTE_FILL_ARG:
        return (diff["oracle"] & 0xFF) == (diff["candidate"] & 0xFF)
    return False


def _is_globals_placement_diff(diff: dict) -> bool:
    """True if the two values look like the same global placed differently.

    One side is inside the synthetic DIR32 slot arena and the other is a plain
    XBE data address. That is the shape the emulator produces when a global
    reference is relocated into a slot on one side but read at its original
    address on the other -- it says nothing about whether the lift is correct,
    so it cannot be called a bug, but it also cannot be proven benign from the
    log alone (the two addresses might genuinely be different globals). These
    land in needs-evidence: confirm both addresses denote the same global.
    """
    o, c = diff["oracle"], diff["candidate"]
    o_arena = GLOBALS_ARENA_LO <= o < GLOBALS_ARENA_HI
    c_arena = GLOBALS_ARENA_LO <= c < GLOBALS_ARENA_HI
    if o_arena == c_arena:
        return False
    other = c if o_arena else o
    return XBE_IMAGE_LO <= other < GLOBALS_ARENA_LO


def _assert_metadata_only(smoke: dict) -> bool:
    """True if every observed divergence is assert message/file/line metadata."""
    diffs = smoke.get("arg_diffs", [])
    if not diffs:
        return False
    callees = set(smoke.get("arg_diff_callees", []))
    if not callees or not callees <= ASSERT_ARG_CALLEES:
        return False
    # Any non-arg failure channel means something else also diverged.
    if smoke.get("fail_eax") or smoke.get("fail_scratch") or smoke.get("fail_st0"):
        return False
    if smoke.get("stub_call_seq"):
        return False
    return all(_is_assert_metadata(d) for d in diffs)


def _stack_ptr_only(smoke: dict) -> bool:
    """True if the only reported stub-arg problem is stack-pointer arguments.

    Oracle (MSVC) and candidate (clang) lay out frames differently, so a pointer
    to a local passed to a stubbed callee has a different numeric value on each
    side by construction. The emulator reports it; it is not a lift bug.
    """
    if not smoke.get("stub_stack_ptr"):
        return False
    if smoke.get("fail_eax") or smoke.get("fail_scratch") or smoke.get("fail_st0"):
        return False
    if smoke.get("stub_call_seq"):
        return False
    # Every line that reported a mismatch must have attributed it to stack ptrs
    for ln in smoke.get("stub_arg_lines", []):
        if "arg mismatch" in ln and "stack-ptr arg" not in ln:
            return False
    return True


def classify(row: dict, smoke: dict) -> tuple:
    """Return (category, detail) for a failure."""
    if not smoke.get("exists"):
        return "unknown", "no smoke log"

    coverage = smoke.get("coverage", 100)
    fails = smoke.get("fails", [])
    intrinsics = smoke.get("intrinsics", [])

    # ftol2: oracle calls _ftol2 and all fails show EAX divergence
    if smoke.get("has_ftol2"):
        eax_fails = [f for f in fails if "EAX:" in f["diff"]]
        if len(eax_fails) == len(fails) and fails:
            return "ftol2", "oracle _ftol2 stub returns 0"

    # Other CRT intrinsics
    non_ftol = [i for i in intrinsics if i != "_ftol2"]
    if non_ftol:
        return "intrinsic", f"oracle calls {', '.join(non_ftol)}"

    # --- Stub-argument differential classes -----------------------------
    # Checked before the generic fall-through: a stub-arg failure is the most
    # common shape by far, and most instances are metadata, not behaviour.

    # Assert message/__FILE__/__LINE__ only — expected, benign.
    if _assert_metadata_only(smoke):
        diffs = smoke.get("arg_diffs", [])
        lines = [d for d in diffs if d["oracle"] < LINE_MAX and d["candidate"] < LINE_MAX
                 and d["oracle"] != d["candidate"]]
        if lines:
            d = lines[0]
            detail = (f"assert metadata only (arg[{d['arg']}] __LINE__ "
                      f"{d['oracle']} vs {d['candidate']})")
        else:
            detail = "assert metadata only (message/__FILE__ pointers)"
        return "assert_metadata", detail

    # Stack-pointer arguments differ because the two frames differ — expected.
    if _stack_ptr_only(smoke):
        return "stack_ptr_args", "only stack-pointer args differ (frame layout)"

    # Call sequence diverged, but every argument diff seen is assert metadata:
    # the assert fired at a different point, which needs state to adjudicate.
    if smoke.get("stub_call_seq") and smoke.get("arg_diff_callees"):
        if set(smoke["arg_diff_callees"]) <= ASSERT_ARG_CALLEES:
            return "assert_call_seq", "call-seq diverged around an assert path"

    # A wrong argument to a real (non-assert) callee. This is the strongest
    # evidence class in the whole set: it names the callee and the arg index,
    # which is exactly the §10 caller-side argument swap/drop signature.
    real_diffs = [x for x in smoke.get("arg_diffs", [])
                  if x["callee"] not in ASSERT_ARG_CALLEES]
    substantive = [x for x in real_diffs if not _is_benign_arg_diff(x)]

    # Every real-callee argument difference is provably behaviour-identical
    # (today: a memset fill literal written 0xff instead of -1).
    if real_diffs and not substantive and not smoke.get("stub_call_seq"):
        if not (smoke.get("fail_eax") or smoke.get("fail_scratch")
                or smoke.get("fail_st0")):
            d = real_diffs[0]
            return "benign_arg_width", (
                f"arg[{d['arg']}] to {d['callee']}: "
                f"{d['oracle']:#x} vs {d['candidate']:#x} — same low byte, "
                f"identical effect (source literal should be -1)")

    # Every remaining difference is a globals-placement asymmetry.
    if substantive and all(_is_globals_placement_diff(d) for d in substantive):
        d = substantive[0]
        return "globals_reloc_placement", (
            f"arg[{d['arg']}] to {d['callee']}: {d['oracle']:#x} vs "
            f"{d['candidate']:#x} — one side is a DIR32 slot, the other an XBE "
            f"address; confirm both denote the same global")

    if substantive:
        d = substantive[0]
        seq = " (+call-seq)" if smoke.get("stub_call_seq") else ""
        detail = (f"arg[{d['arg']}] to {d['callee']}: "
                  f"oracle={d['oracle']:#x} candidate={d['candidate']:#x}{seq}")
        return "arg_mismatch", detail

    # Call sequence diverged with no other signal: the two sides called
    # different callees, or in a different order. That is a behavioural
    # difference and stays suspect until explained.
    if smoke.get("stub_call_seq") and not smoke.get("arg_diffs"):
        if not (smoke.get("fail_eax") or smoke.get("fail_scratch")
                or smoke.get("fail_st0")):
            idx = ""
            for ln in smoke.get("stub_arg_lines", []):
                m = re.search(r'call-seq diverged at index (\d+)', ln)
                if m:
                    idx = f" at index {m.group(1)}"
                    break
            # Split by the SHAPE of the divergence when the harness recorded it.
            # Measured on the 07-29 batch: of 71 call_seq targets, 57 were
            # truncated and 4 shifted (both artifacts -- one side stopped early,
            # or the lists are off by one) and only 3 genuinely called different
            # callees. Without this split the largest failure class in the corpus
            # is 95% artifact filed as suspect-real.
            rel = smoke.get("seq_relation")
            if rel == "truncated":
                return "call_seq_truncated", (
                    f"one side stopped early{idx}; both agree on every shared call")
            if rel == "shifted":
                return "call_seq_shifted", (
                    f"sequences off by one{idx}; identical without the extra call")
            if rel == "inline-asymmetry":
                return "call_seq_inline_asymmetry", (
                    f"one side CALLed a callee the other INLINED{idx}; the "
                    f"surviving sequences are not comparable")
            if rel == "divergent":
                return "call_seq", f"different callee at the same position{idx}"
            return "call_seq", f"stub call sequence diverged{idx} (shape not recorded)"

    # Assert-path: oracle hits halt with ESP_delta != 0
    if smoke.get("has_halt") or smoke.get("has_assert"):
        esp = smoke.get("esp_deltas", [])
        insn = smoke.get("insn_counts", [])
        abnormal_esp = any(e != 0 for e in esp)
        hit_max_insn = any(c >= 99999 for c in insn)
        if abnormal_esp or hit_max_insn:
            return "assert_path", "divergence on assert/halt path"

    # --- New false-positive detectors ---

    orc_insn = smoke.get("oracle_insn", 0)
    lft_insn = smoke.get("lifted_insn", 0)
    orc_esp = smoke.get("oracle_esp", 0)
    lft_esp = smoke.get("lifted_esp", 0)

    # Load width: one side reads the field wider than the other. Ordered ahead of
    # dirty_eax deliberately -- the shapes overlap and dirty_eax would bucket this
    # real bug as a harness artifact.
    if fails and _is_load_width(fails):
        only_eax = all(
            f["diff"].startswith("EAX:") and ";" not in f["diff"]
            for f in fails
        )
        if only_eax:
            m = re.search(r'EAX: oracle=(0x[0-9a-f]+) lifted=(0x[0-9a-f]+)',
                          fails[0]["diff"])
            detail = ("field read at different widths (oracle=%s lifted=%s) -- "
                      "check the field/return type for int vs int16_t/int8_t"
                      % (m.group(1), m.group(2))) if m else "field width mismatch"
            return "load_width", detail

    # Dirty EAX: oracle uses mov ax (16-bit), upper bits are stale pointer
    if fails and _is_dirty_eax(fails):
        only_eax = all(
            f["diff"].startswith("EAX:") and ";" not in f["diff"]
            for f in fails
        )
        if only_eax:
            return "dirty_eax", "oracle mov ax leaves upper 16 bits dirty"

    # Stub residual: oracle EAX is random garbage from a stubbed callee,
    # lifted EAX is consistently 0 (proper XOR EAX,EAX)
    if fails and _is_stub_residual(fails):
        eax_count = sum(1 for f in fails if "EAX:" in f["diff"])
        if eax_count >= len(fails) * 0.5:
            return "stub_residual", "oracle EAX is callee stub garbage, lifted returns 0"

    # Leaf mismatch: oracle is "stubbable" but lifted is "leaf" (same-TU callees
    # compiled into one .obj). Lifted runs real callee code with garbage input
    # and crashes/diverges. Detectable: different class + lifted ESP abnormal.
    orc_class = smoke.get("oracle_class", "")
    lft_class = smoke.get("lifted_class", "")
    if orc_class == "stubbable" and lft_class == "leaf":
        if lft_esp != 0 or lft_insn >= 100000:
            return "leaf_mismatch", "lifted is leaf (same-TU callees), crashes on garbage input"

    # Instruction limit: one or both sides hit 100K — didn't complete normally
    if orc_insn >= 100000 or lft_insn >= 100000:
        # Asymmetric: one side completes, other hits limit
        if orc_insn >= 100000 and lft_insn < 100000 and lft_insn > 0:
            ratio = orc_insn / max(lft_insn, 1)
            if ratio > 10:
                return "insn_limit", f"oracle hit 100K insns, lifted only {lft_insn}"
        if lft_insn >= 100000 and orc_insn < 100000 and orc_insn > 0:
            ratio = lft_insn / max(orc_insn, 1)
            if ratio > 10:
                return "insn_limit", f"lifted hit 100K insns, oracle only {orc_insn}"
        # Both hit limit
        if orc_insn >= 100000 and lft_insn >= 100000:
            if orc_esp != 0 or lft_esp != 0:
                return "insn_limit", "both hit 100K insns, ESP abnormal"

    # Asymmetric execution: >10x instruction ratio without hitting limit
    if orc_insn > 0 and lft_insn > 0:
        ratio = max(orc_insn, lft_insn) / max(min(orc_insn, lft_insn), 1)
        if ratio > 10 and (orc_esp != lft_esp):
            return "exec_asymmetry", f"insn ratio {orc_insn}:{lft_insn}, ESP {orc_esp}:{lft_esp}"

    # Abnormal ESP on both sides with non-zero deltas (neither returned normally)
    if orc_esp != 0 and lft_esp != 0 and orc_esp != lft_esp:
        # Both have stack issues — likely both took different abnormal paths
        if abs(orc_esp - lft_esp) > 8:
            return "stack_divergence", f"ESP oracle={orc_esp} lifted={lft_esp}"

    # Coverage-limited: very low coverage means only early-exit tested
    if coverage < 30:
        if row.get("seeds_passed", 0) > row.get("seeds_total", 20) * 0.7:
            return "coverage_limited", f"{coverage:.0f}% coverage, mostly passing"

    # ftol2 even without explicit stub detection (oracle EAX=0 pattern)
    if smoke.get("has_ftol2") and intrinsics:
        return "ftol2", f"oracle _ftol2 + {', '.join(intrinsics)}"

    # Stub asymmetry: oracle and lifted have very different reloc counts
    orc_n = len(smoke.get("oracle_relocs", []))
    lft_n = len(smoke.get("lifted_relocs", []))
    if orc_n > 0 and lft_n > 0 and abs(orc_n - lft_n) > max(orc_n, lft_n) * 0.5:
        return "stub_asymmetry", f"oracle {orc_n} relocs vs lifted {lft_n}"

    # Scratch-only divergence with all seeds failing → likely .rdata or global issue
    if fails:
        scratch_only = all("scratch" in f["diff"] for f in fails)
        if scratch_only:
            return "scratch_divergence", "scratch buffer differs (possible constant/global)"

    # Genuine divergence: both complete normally, results differ
    if fails:
        pass_rate = row.get("seeds_passed", 0) / max(row.get("seeds_total", 1), 1)
        if pass_rate > 0.5:
            return "genuine", f"{len(fails)} seeds fail, {pass_rate:.0%} pass rate"
        return "genuine", f"{len(fails)}/{row.get('seeds_total', '?')} seeds fail"

    # Stale: summary says fail but smoke log shows no failures (re-run passed)
    if not fails and smoke.get("exists"):
        return "stale", "smoke log shows no failures (batch result outdated)"

    return "unknown", "unclassified"


def load_vc71_scores() -> dict:
    """Latest VC71 match_pct per function from the verify cache.

    Returns {fn_name: {"match": float, "source": str}}. Missing cache or an
    unreadable DB is not fatal — the ledger just carries no VC71 column.
    """
    if not VC71_DB.exists():
        return {}
    try:
        import sqlite3
        con = sqlite3.connect(f"file:{VC71_DB}?mode=ro", uri=True)
        rows = con.execute(
            "SELECT fn_name, match_pct, source_path, created_utc FROM fn_results"
        ).fetchall()
        con.close()
    except Exception as exc:          # noqa: BLE001 - advisory data only
        print(f"[warn] VC71 cache unreadable: {exc}", file=sys.stderr)
        return {}

    best = {}
    for fn, pct, src, created in rows:
        prev = best.get(fn)
        if prev is None or created > prev["created"]:
            best[fn] = {"match": pct, "source": src, "created": created}
    for v in best.values():
        v.pop("created", None)
        src = v.get("source") or ""
        if src.startswith(str(ROOT)):
            v["source"] = str(Path(src).relative_to(ROOT))
    return best


def _priority(bucket, category, vc71):
    """Investigation priority for a suspect-real entry.

    A function that diverges behaviourally *and* scores below the 85% VC71
    band is a lift bug, not a structural ceiling — see docs/lift-learnings.md
    §19. Those are P0. Behavioural divergence at a high structural score is
    still real but likelier to be subtle (P1).
    """
    if bucket != "suspect-real":
        return "-"
    if vc71 is not None and vc71 < 85.0:
        return "P0"
    if category == "arg_mismatch":
        return "P1"
    return "P2"


def build_ledger(categories: dict, existing: dict, batch_date: str,
                 summary_meta: dict) -> dict:
    """Merge this triage run into the persistent ledger.

    Auto-derived fields (category, bucket, evidence, seeds, coverage, VC71) are
    always refreshed. Human-owned fields (status once moved past its auto
    value, and notes) are preserved across runs.
    """
    vc71 = load_vc71_scores()
    prev_entries = existing.get("entries", {})
    entries = {}

    for cat, items in categories.items():
        bucket = BUCKETS.get(cat, "needs-evidence")
        for it in items:
            key = it["addr"] or it["name"]
            old = prev_entries.get(key, {})
            score = vc71.get(it["name"], {})
            match = score.get("match")
            if match is not None:
                match = round(match, 2)   # keep the committed file diff-stable

            # A fresh smoke log with no failures means the batch verdict is
            # outdated: the function does not diverge on current source.
            auto_status = "no-longer-diverging" if cat == "stale" else bucket

            status = old.get("status")
            # Only adopt the auto status while nobody has adjudicated it.
            if status is None or status == old.get("auto_status"):
                status = auto_status

            entries[key] = {
                "name": it["name"],
                "obj": it["obj"],
                "addr": it["addr"],
                "category": cat,
                "bucket": bucket,
                "auto_status": auto_status,
                "status": status,
                "priority": _priority(bucket, cat, match),
                "evidence": it["detail"],
                "seeds": it["pass_rate"],
                "coverage_pct": it["coverage"],
                "confidence": it["confidence"],
                "vc71_match": match,
                "vc71_source": score.get("source"),
                "evidence_log_date": it.get("log_date"),
                "evidence_stale": bool(
                    it.get("log_date") and batch_date
                    and it["log_date"] < batch_date
                ),
                "first_seen": old.get("first_seen", batch_date),
                "last_seen": batch_date,
                "notes": old.get("notes", ""),
            }

    # Entries that were in the ledger but no longer diverge: keep them with a
    # closed status so a fixed bug stays visible rather than silently vanishing.
    for key, old in prev_entries.items():
        if key in entries:
            continue
        closed = dict(old)
        if closed.get("status") not in ("fixed", "no-longer-diverging"):
            closed["status"] = "no-longer-diverging"
        closed["last_seen"] = old.get("last_seen", "")
        closed["resolved_in"] = batch_date
        entries[key] = closed

    by_cat = Counter(e["category"] for e in entries.values()
                     if not e.get("resolved_in"))
    by_bucket = Counter(e["bucket"] for e in entries.values()
                        if not e.get("resolved_in"))
    by_status = Counter(e["status"] for e in entries.values())
    by_priority = Counter(e["priority"] for e in entries.values()
                          if not e.get("resolved_in") and e["priority"] != "-")

    return {
        "schema": 1,
        "batch": summary_meta,
        "counts": {
            "open": sum(1 for e in entries.values() if not e.get("resolved_in")),
            "by_category": dict(sorted(by_cat.items())),
            "by_bucket": dict(sorted(by_bucket.items())),
            "by_status": dict(sorted(by_status.items())),
            "by_priority": dict(sorted(by_priority.items())),
        },
        "entries": dict(sorted(entries.items(), key=lambda kv: kv[1]["name"])),
    }


def print_bug_list(ledger: dict, limit: int = 0) -> None:
    """Prioritized suspect-real bug list, worst structural score first."""
    open_real = [e for e in ledger["entries"].values()
                 if not e.get("resolved_in") and e["bucket"] == "suspect-real"
                 and e["status"] not in ("fixed", "harness-artifact",
                                         "no-longer-diverging")]
    order = {"P0": 0, "P1": 1, "P2": 2, "-": 3}
    open_real.sort(key=lambda e: (order.get(e["priority"], 9),
                                  e["vc71_match"] if e["vc71_match"] is not None else 101.0))
    if limit:
        open_real = open_real[:limit]
    print(f"\n{'='*104}")
    print(f"PRIORITIZED BUG LIST ({len(open_real)} suspect-real)")
    print(f"{'='*104}")
    print(f"{'Pri':<4} {'Function':<48} {'VC71':>6}  {'Seeds':>6}  {'Category':<14} Evidence")
    print(f"{'-'*4} {'-'*48} {'-'*6}  {'-'*6}  {'-'*14} {'-'*20}")
    for e in open_real:
        vc = f"{e['vc71_match']:.1f}" if e["vc71_match"] is not None else "  n/a"
        print(f"{e['priority']:<4} {e['name'][:48]:<48} {vc:>6}  "
              f"{e['seeds']:>6}  {e['category']:<14} {e['evidence'][:44]}")


def main():
    parser = argparse.ArgumentParser(description="Triage batch_verify failures")
    parser.add_argument("--details", action="store_true",
                        help="Show detail line for each failure")
    parser.add_argument("--category", type=str, default=None,
                        help="Filter to one category")
    parser.add_argument("--summary-path", type=Path, default=SUMMARY)
    parser.add_argument("--ledger", action="store_true",
                        help="Write/merge the persistent divergence ledger")
    parser.add_argument("--ledger-path", type=Path, default=LEDGER)
    parser.add_argument("--bug-list", action="store_true",
                        help="Print the prioritized suspect-real bug list")
    parser.add_argument("--bug-limit", type=int, default=0,
                        help="Cap the bug list to N entries (0 = all)")
    args = parser.parse_args()

    summary = json.loads(args.summary_path.read_text(encoding="utf-8"))
    failures = [r for r in summary.get("rows", []) if r["status"] == "fail"]
    print(f"Triaging {len(failures)} failures...\n")

    batch_date = datetime.fromtimestamp(
        args.summary_path.stat().st_mtime).strftime("%Y-%m-%d")

    categories = defaultdict(list)
    for row in failures:
        name = row["name"]
        smoke_path = SMOKE_DIR / f"{name}_smoke.log"
        smoke = parse_smoke_log(smoke_path)
        cat, detail = classify(row, smoke)
        log_date = ""
        if smoke_path.exists():
            log_date = datetime.fromtimestamp(
                smoke_path.stat().st_mtime).strftime("%Y-%m-%d")
        categories[cat].append({
            "name": name,
            "addr": row.get("addr", ""),
            "obj": row.get("obj", ""),
            "detail": detail,
            "pass_rate": f"{row.get('seeds_passed', 0)}/{row.get('seeds_total', 0)}",
            "coverage": smoke.get("coverage", 0),
            "confidence": smoke.get("confidence", ""),
            "log_date": log_date,
        })

    # Print summary
    print(f"{'Category':<22} {'Count':>5}  Description")
    print(f"{'-'*22} {'-'*5}  {'-'*50}")
    order = ["arg_mismatch", "genuine", "call_seq", "load_width",
             "call_seq_truncated", "call_seq_shifted",
             "dirty_eax", "stub_residual", "leaf_mismatch",
             "insn_limit", "exec_asymmetry", "stack_divergence",
             "ftol2", "intrinsic", "assert_path", "assert_metadata",
             "assert_call_seq", "stack_ptr_args", "benign_arg_width",
             "globals_reloc_placement",
             "stub_asymmetry", "scratch_divergence", "coverage_limited",
             "stale", "unknown"]
    for cat in order:
        if cat not in categories:
            continue
        entries = categories[cat]
        desc = {
            "genuine": "Both sides complete normally, results differ — INVESTIGATE",
            "arg_mismatch": "Wrong arg to a named callee — INVESTIGATE (§10)",
            "call_seq": "Different callee at the same position — INVESTIGATE",
            "call_seq_truncated": "One side stopped early (shared calls all agree)",
            "call_seq_shifted": "Call sequences off by one (alignment)",
            "call_seq_inline_asymmetry":
                "One side CALLed what the other INLINED (not comparable)",
            "load_width": "Field read at different widths — INVESTIGATE (int vs int16/int8)",
            "assert_metadata": "Only assert message/__FILE__/__LINE__ args differ",
            "assert_call_seq": "Call-seq diverged around an assert path",
            "stack_ptr_args": "Only stack-pointer args differ (frame layout)",
            "benign_arg_width": "memset fill literal 0xff vs -1 — same bytes",
            "globals_reloc_placement": "DIR32 slot vs XBE address — verify same global",
            "dirty_eax": "Oracle uses mov ax (16-bit), upper EAX bits stale",
            "stub_residual": "Oracle EAX is callee stub garbage, lifted returns 0",
            "leaf_mismatch": "Lifted is leaf (same-TU callees), crashes on garbage",
            "insn_limit": "One/both sides hit 100K instruction limit",
            "exec_asymmetry": "Wildly different execution length (>10x ratio)",
            "stack_divergence": "Both sides have different abnormal ESP",
            "ftol2": "Oracle _ftol2 stub returns 0 — fix stub",
            "intrinsic": "Oracle calls CRT intrinsic — add stub",
            "assert_path": "Divergence on assert/halt path — low priority",
            "stub_asymmetry": "Oracle/lifted have different reloc patterns",
            "scratch_divergence": "Scratch buffer differs (constant/global)",
            "coverage_limited": "Low coverage, mostly passing — needs state",
            "stale": "Smoke log passes now — batch result outdated",
            "unknown": "Could not classify",
        }.get(cat, "")
        marker = " ←" if BUCKETS.get(cat) == "suspect-real" else ""
        print(f"{cat:<22} {len(entries):>5}  {desc}{marker}")

    if args.category:
        entries = categories.get(args.category, [])
        if not entries:
            print(f"\nNo failures in category '{args.category}'")
            return
        print(f"\n{'='*70}")
        print(f"Category: {args.category} ({len(entries)} functions)")
        print(f"{'='*70}")
        for e in sorted(entries, key=lambda x: x["coverage"], reverse=True):
            print(f"  {e['name']:45s} {e['pass_rate']:>6s}  cov={e['coverage']:5.1f}%  {e['confidence']:8s}  {e['obj']}")
            if args.details:
                print(f"    → {e['detail']}")
    elif args.details:
        for cat in order:
            if cat not in categories:
                continue
            entries = categories[cat]
            print(f"\n--- {cat} ({len(entries)}) ---")
            for e in sorted(entries, key=lambda x: x["coverage"], reverse=True):
                print(f"  {e['name']:45s} {e['pass_rate']:>6s}  cov={e['coverage']:5.1f}%  {e['detail']}")

    # Bucket rollup — what the categories mean for the investigation queue
    buckets = Counter()
    for cat, items in categories.items():
        buckets[BUCKETS.get(cat, "needs-evidence")] += len(items)
    total = sum(buckets.values())
    print(f"\n{'='*70}")
    print("BUCKETS:")
    for b in ("suspect-real", "needs-evidence", "benign-difference",
              "harness-artifact"):
        n = buckets.get(b, 0)
        pct = (100.0 * n / total) if total else 0.0
        print(f"  {b:<18} {n:>4}  ({pct:4.1f}%)")

    ledger = None
    if args.ledger or args.bug_list:
        existing = {}
        if args.ledger_path.exists():
            existing = json.loads(args.ledger_path.read_text(encoding="utf-8"))
        ledger = build_ledger(categories, existing, batch_date, {
            "summary_path": str(args.summary_path.relative_to(ROOT)
                                if args.summary_path.is_absolute() and
                                str(args.summary_path).startswith(str(ROOT))
                                else args.summary_path),
            "batch_date": batch_date,
            "results": summary.get("results", {}),
        })

    if args.ledger and ledger is not None:
        args.ledger_path.parent.mkdir(parents=True, exist_ok=True)
        args.ledger_path.write_text(
            json.dumps(ledger, indent=2, sort_keys=False) + "\n", encoding="utf-8")
        c = ledger["counts"]
        print(f"\nLedger written: {args.ledger_path.relative_to(ROOT)}")
        print(f"  open={c['open']}  by_bucket={c['by_bucket']}")
        print(f"  by_priority={c['by_priority']}  by_status={c['by_status']}")
        stale = sum(1 for e in ledger["entries"].values()
                    if e.get("evidence_stale") and not e.get("resolved_in"))
        if stale:
            print(f"  [warn] {stale} entries classified from smoke logs older "
                  f"than the batch summary ({batch_date}) — re-run those "
                  f"targets to refresh evidence")

    if args.bug_list and ledger is not None:
        print_bug_list(ledger, args.bug_limit)


if __name__ == "__main__":
    main()
