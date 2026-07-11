#!/usr/bin/env python3
"""
check_stdcall_ret.py — Cross-check kb.json calling conventions against the
RET instruction of each function in the pristine XBE.

Bug class (lift-learnings: stdcall-decl mismatch, found 2026-07-11): Ghidra
decompiles Xbox D3D wrappers like D3DDevice_SetRenderState_StencilEnable as
``void __cdecl f(void)`` even though the real function reads an argument at
[ESP+8] and ends with ``RET 4`` (stdcall, callee pops).  A kb.json decl that
inherits the bogus signature makes every lifted call site drift ESP by +4
per call — check_arg_counts.py cannot see this because stdcall callees have
no caller-side ADD ESP cleanup to compare against.  The 0x158df0 boot crash
(#PF CR2=0xfffffff5, RET onto the stack) was exactly this.

For every kb.json function decl we linearly disassemble the original body in
halo-patched/cachebeta.xbe until the first RET and compare:

    decl says __stdcall with N stack dwords  ->  expect RET 4*N
    decl says anything else (caller cleans)  ->  expect plain RET (C3)

Findings:
    ERROR  decl is cdecl/plain but binary RETs with an immediate (crash class)
    ERROR  decl is __stdcall but immediate differs from 4*stack_slots
    WARN   decl is __stdcall but binary uses plain RET

Usage:
    python3 tools/audit/check_stdcall_ret.py                    # full sweep report
    python3 tools/audit/check_stdcall_ret.py --check            # exit 1 on non-baselined ERRORs
    python3 tools/audit/check_stdcall_ret.py --addr 0x1ea300
    python3 tools/audit/check_stdcall_ret.py --update-baseline  # accept current ERRORs
    python3 tools/audit/check_stdcall_ret.py --self-test

Baseline: tools/audit/stdcall_ret_baseline.json holds the pre-existing ERRORs
(545 latent ``(void)``-decl-over-``RET n`` XDK/D3D functions, none called from
lifted C as of 2026-07-12).  ``--check`` gates only NEW mismatches — a new bad
decl, or an existing entry whose expected/observed pops changed (i.e. the decl
was edited but is still wrong).  Fix the decl instead of baselining whenever
the function is (about to be) called from lifted C.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from typing import Dict, List, NamedTuple, Optional, Tuple

import capstone

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(REPO_ROOT / "tools" / "audit"))

from check_arg_counts import (  # noqa: E402
    FuncEntry,
    _load_kb,
    _load_xbe,
    parse_decl,
)
import check_arg_counts as cac  # noqa: E402

REPORT_PATH = REPO_ROOT / "artifacts" / "audit" / "stdcall_ret_report.txt"
BASELINE_PATH = REPO_ROOT / "tools" / "audit" / "stdcall_ret_baseline.json"

_STDCALL_RE = re.compile(r"\b__stdcall\b")
_NORETURN_RE = re.compile(r"\b__noreturn\b|\bnoreturn\b")

MAX_BODY = 0x2000       # hard cap on linear scan length per function
MAX_THUNK_HOPS = 3


def _read_va(addr: int, size: int) -> Optional[bytes]:
    """Read raw bytes at a virtual address from the loaded XBE sections."""
    _load_xbe()
    for i, (vaddr, vsize, _raw_addr, raw_size) in enumerate(cac._xbe_sections):
        if vaddr <= addr < vaddr + min(vsize, raw_size):
            off = addr - vaddr
            data = cac._xbe_bytes_cache[i]
            return data[off: off + size]
    return None


class RetResult(NamedTuple):
    kind: str            # 'ret' | 'undecidable'
    pop_bytes: int       # RET immediate (0 for plain C3)
    ret_va: int


def find_first_ret(addr: int, bound: int) -> RetResult:
    """Linearly disassemble from addr; return the first RET's pop count.

    Follows up to MAX_THUNK_HOPS leading thunks: an unconditional direct JMP
    within the first few instructions whose target lies OUTSIDE the current
    body (covers plain ``jmp target`` and reg-loading thunks like the xnet
    wrappers' ``mov ecx,[glob]; jmp target``).  Any later out-of-body JMP is
    a tail-call — the RET belongs to the target's convention, so the scan
    stops as undecidable rather than decoding into the next function (that
    misattribution produced false ERRORs on xnet_send/xnet_recvfrom).
    In-body forward JMPs (to the shared epilogue) are passed over linearly.
    Stops as undecidable on decode failure, INT3 padding, or scan overrun —
    a jump table's inline data would derail linear decode, but MSVC places
    tables after the final RET, so the first RET is reached beforehand.
    """
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = False
    hops = 0
    while True:
        size = max(0x40, min(MAX_BODY, bound - addr)) if bound > addr else MAX_BODY
        code = _read_va(addr, size)
        if not code:
            return RetResult("undecidable", 0, 0)
        entry = addr
        insn_idx = 0
        pos = 0
        followed = False
        for insn in md.disasm(code, addr):
            pos = insn.address + insn.size - addr
            m = insn.mnemonic
            if m == "ret":
                imm = int(insn.op_str, 0) if insn.op_str else 0
                return RetResult("ret", imm, insn.address)
            if m == "int3":
                return RetResult("undecidable", 0, 0)
            if m == "jmp" and re.fullmatch(r"0x[0-9a-f]+", insn.op_str):
                target = int(insn.op_str, 16)
                if entry <= target < bound:
                    insn_idx += 1
                    continue  # internal flow (epilogue jump) — keep scanning
                if insn_idx <= 2:
                    # leading thunk (possibly after reg-arg loads) — follow it
                    hops += 1
                    if hops > MAX_THUNK_HOPS:
                        return RetResult("undecidable", 0, 0)
                    addr = target
                    bound = target + MAX_BODY
                    followed = True
                    break
                # mid-body tail-call: RET belongs to the callee's convention
                return RetResult("undecidable", 0, 0)
            insn_idx += 1
        else:
            return RetResult("undecidable", 0, 0)  # decode error / ran off window
        if followed:
            continue
        return RetResult("undecidable", 0, 0)


class Finding(NamedTuple):
    level: str
    entry: FuncEntry
    expected_pop: int
    observed_pop: int
    ret_va: int
    note: str


def sweep(only_addr: Optional[int] = None) -> Tuple[List[Finding], Dict[str, int]]:
    entries = sorted(_load_kb(), key=lambda e: e.addr)
    stats = {"total": 0, "checked": 0, "undecidable": 0, "ok": 0}
    findings: List[Finding] = []

    for i, e in enumerate(entries):
        if only_addr is not None and e.addr != only_addr:
            continue
        stats["total"] += 1
        if _NORETURN_RE.search(e.decl):
            stats["undecidable"] += 1
            continue
        stack_slots, _reg = parse_decl(e.decl)
        if stack_slots is None:  # varargs / unparseable — caller cleans, RET must be plain
            stack_slots = None
        is_stdcall = bool(_STDCALL_RE.search(e.decl))
        bound = entries[i + 1].addr if i + 1 < len(entries) else e.addr + MAX_BODY
        if bound <= e.addr:
            bound = e.addr + MAX_BODY

        res = find_first_ret(e.addr, bound)
        if res.kind != "ret":
            stats["undecidable"] += 1
            continue
        stats["checked"] += 1

        expected = 4 * stack_slots if (is_stdcall and stack_slots is not None) else 0
        if res.pop_bytes == expected:
            stats["ok"] += 1
            continue

        if not is_stdcall and res.pop_bytes > 0:
            # Crash class: caller thinks cdecl, callee pops its own args.
            findings.append(Finding(
                "ERROR", e, expected, res.pop_bytes, res.ret_va,
                "decl is caller-cleanup but binary RETs with an immediate "
                "(callee pops %d bytes) — every lifted call site drifts ESP" %
                res.pop_bytes))
        elif is_stdcall and res.pop_bytes == 0:
            findings.append(Finding(
                "WARN", e, expected, 0, res.ret_va,
                "decl is __stdcall but binary uses plain RET (caller must clean)"))
        else:
            findings.append(Finding(
                "ERROR", e, expected, res.pop_bytes, res.ret_va,
                "__stdcall pop mismatch: decl implies RET %d, binary has RET %d" %
                (expected, res.pop_bytes)))

    return findings, stats


def _self_test() -> None:
    # D3DDevice_SetRenderState_FillMode (0x1e99b0): __stdcall @4, RET 4 — OK
    r = find_first_ret(0x1e99b0, 0x1e9a30)
    assert r.kind == "ret" and r.pop_bytes == 4, r
    # StencilEnable original body ends RET 4 (the 0x158df0 crash class)
    r = find_first_ret(0x1ea300, 0x1ea380)
    assert r.kind == "ret" and r.pop_bytes == 4, r
    # FUN_00158ae0: cdecl, plain RET through a switch — first RET is C3
    r = find_first_ret(0x158ae0, 0x158df0)
    assert r.kind == "ret" and r.pop_bytes == 0, r
    # xnet_send: reg-loading thunk (mov ecx,[glob]; jmp send) — must follow
    # the tail JMP to send's RET 0x10, not decode past it (old false ERROR)
    r = find_first_ret(0x225c20, 0x225cd1)
    assert r.kind == "ret" and r.pop_bytes == 16, r
    print("self-test OK")


def _load_baseline() -> Dict[str, Dict]:
    import json
    if not BASELINE_PATH.exists():
        return {}
    return json.loads(BASELINE_PATH.read_text()).get("entries", {})


def _write_baseline(errors: List[Finding]) -> None:
    import json
    entries = {
        "0x%06x" % f.entry.addr: {
            "name": f.entry.name,
            "expected": f.expected_pop,
            "observed": f.observed_pop,
        }
        for f in errors
    }
    payload = {
        "_comment": (
            "Pre-existing stdcall-decl mismatches (lift-learnings §30) accepted "
            "as latent: not called from lifted C when baselined. --check gates "
            "only ERRORs absent from this file or whose expected/observed pops "
            "changed. Fix the kb.json decl (do NOT baseline) for any function "
            "called from lifted C. Regenerate: check_stdcall_ret.py --update-baseline"
        ),
        "entries": dict(sorted(entries.items())),
    }
    BASELINE_PATH.write_text(json.dumps(payload, indent=2) + "\n")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[1])
    ap.add_argument("--check", action="store_true",
                    help="exit 1 on ERRORs not covered by the baseline")
    ap.add_argument("--addr", type=lambda s: int(s, 16), help="check one function")
    ap.add_argument("--update-baseline", action="store_true",
                    help="write current ERRORs to %s" % BASELINE_PATH.name)
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()

    if args.self_test:
        _self_test()
        return

    findings, stats = sweep(args.addr)
    errors = [f for f in findings if f.level == "ERROR"]
    warns = [f for f in findings if f.level == "WARN"]

    if args.update_baseline:
        _write_baseline(errors)
        print("baseline written: %s (%d entries)" % (BASELINE_PATH, len(errors)))
        return

    baseline = _load_baseline()
    new_errors = []
    for f in errors:
        b = baseline.get("0x%06x" % f.entry.addr)
        if b is None or b.get("expected") != f.expected_pop \
                or b.get("observed") != f.observed_pop:
            new_errors.append(f)

    lines: List[str] = []
    for f in sorted(findings, key=lambda f: (f.level != "ERROR", f.entry.addr)):
        tag = " [baselined]" if (f.level == "ERROR" and f not in new_errors) else ""
        lines.append(
            "%-5s 0x%06x %-48s expected RET %-3d observed RET %-3d @0x%06x  %s%s"
            % (f.level, f.entry.addr, f.entry.name[:48], f.expected_pop,
               f.observed_pop, f.ret_va, f.note, tag))
    report = "\n".join(lines) + (
        "\n\n%d checked, %d ok, %d undecidable, %d ERROR (%d new, %d baselined), %d WARN\n"
        % (stats["checked"], stats["ok"], stats["undecidable"],
           len(errors), len(new_errors), len(errors) - len(new_errors), len(warns)))
    print(report)
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(report)
    print("report: %s" % REPORT_PATH)

    if args.check and new_errors:
        print("\n--check FAILED: %d non-baselined stdcall-decl mismatch(es). "
              "Fix the kb.json decl (see lift-learnings §30); only baseline "
              "with --update-baseline if the function is provably never "
              "called from lifted C." % len(new_errors))
        sys.exit(1)


if __name__ == "__main__":
    main()
