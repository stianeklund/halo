#!/usr/bin/env python3
"""
check_arg_counts.py — Cross-check kb.json stack-arg counts against ADD ESP,N
cleanup observed at original call sites in the pristine XBE.

Each cdecl callee's declared stack-arg count is compared against the actual
ADD ESP,N cleanup the original binary performs after each call.  Mismatches
indicate UNDER-DECLARED (callee gets more args than the decl says, the
"0xacff0 class") or OVER-DECLARED (callee gets fewer args than declared).

Usage:
    python3 tools/audit/check_arg_counts.py               # full run
    python3 tools/audit/check_arg_counts.py --callee 0xacff0
    python3 tools/audit/check_arg_counts.py --check       # exit 1 on any HIGH
    python3 tools/audit/check_arg_counts.py --self-test

Known limitations (verified false-positive mechanisms):
  1. fstp-to-[esp] float passing: MSVC passes floats via PUSH <dummy>; FSTP
     [ESP]; the PUSH is counted but the actual float value comes from the FPU.
     This does not affect stack-slot counting (the slot is still there) but
     the dummy push value is not the actual argument.
  2. Args pushed before an inner call (outside window): when a caller pushes
     args for callee A, then calls callee B, then calls callee A, the pushes
     for A fall outside the PUSH_SCAN_BACKWARD window and are missed.
  3. Combined cleanup of back-to-back calls: if two consecutive cdecl calls
     share one ADD ESP,N that cleans up both, the cleanup for the first call
     appears as 0 (no ADD ESP before the second CALL) and the cleanup for the
     second call is inflated by the first call's arg count.
  4. Callee-save pushes attributed as args: PUSH EBX/ESI/EDI immediately
     before a CALL in the backward scan window are miscounted as argument
     pushes.  This is rare because cf-terminators reset the scan.
  5. ADD ESP belonging to an adjacent call: the forward scan (up to
     CLEANUP_SCAN_FORWARD instructions) stops at the next CALL or branch, so
     this is largely mitigated, but tight back-to-back calls can still
     confuse the attribution.
    python3 tools/audit/check_arg_counts.py -v            # verbose per-site detail
"""

from __future__ import annotations

import argparse
import json
import os
import re
import struct
import sys
import time
from collections import defaultdict
from pathlib import Path
from typing import Dict, List, NamedTuple, Optional, Tuple

import capstone

# ---------------------------------------------------------------------------
# Repo paths
# ---------------------------------------------------------------------------
REPO_ROOT = Path(__file__).resolve().parent.parent.parent
KB_PATH = REPO_ROOT / "kb.json"
XBE_PATH = REPO_ROOT / "halo-patched" / "cachebeta.xbe"
REPORT_DIR = REPO_ROOT / "artifacts" / "audit"

# ---------------------------------------------------------------------------
# XBE loading — adapted from tools/analysis/classify_common.py (self-contained)
# ---------------------------------------------------------------------------

_xbe_base_addr: Optional[int] = None
_xbe_sections: Optional[List[Tuple[int, int, int, int]]] = None
_xbe_bytes_cache: Dict[int, bytes] = {}   # section_index -> raw section bytes


def _load_xbe() -> None:
    global _xbe_base_addr, _xbe_sections
    if _xbe_sections is not None:
        return
    with open(XBE_PATH, "rb") as f:
        data = f.read()

    # XBE image header layout (little-endian):
    #   0x000  DWORD  magic "XBEH"
    #   0x104  DWORD  base address
    #   0x11C  DWORD  number of sections
    #   0x120  DWORD  section header VA pointer
    if data[:4] != b"XBEH":
        raise ValueError("Not a valid XBE file: bad magic at offset 0")

    base_addr = struct.unpack_from("<I", data, 0x104)[0]
    section_count = struct.unpack_from("<I", data, 0x11C)[0]
    section_header_va = struct.unpack_from("<I", data, 0x120)[0]
    sh_file_off = section_header_va - base_addr

    sections: List[Tuple[int, int, int, int]] = []
    for i in range(section_count):
        off = sh_file_off + i * 56
        # Section header (56 bytes):  flags(4) vaddr(4) vsize(4) raw_addr(4) raw_size(4) ...
        vaddr    = struct.unpack_from("<I", data, off + 4)[0]
        vsize    = struct.unpack_from("<I", data, off + 8)[0]
        raw_addr = struct.unpack_from("<I", data, off + 12)[0]
        raw_size = struct.unpack_from("<I", data, off + 16)[0]
        sections.append((vaddr, vsize, raw_addr, raw_size))
        _xbe_bytes_cache[i] = data[raw_addr: raw_addr + raw_size]

    _xbe_base_addr = base_addr
    _xbe_sections = sections


# ---------------------------------------------------------------------------
# kb.json loading
# ---------------------------------------------------------------------------

class FuncEntry(NamedTuple):
    addr:     int    # integer VA
    addr_str: str    # original hex string from kb.json
    decl:     str
    name:     str
    ported:   bool


def _name_from_decl(decl: str) -> str:
    """Extract function name from a C declaration string."""
    m = re.search(r'\b([A-Za-z_][A-Za-z0-9_]*)\s*\(', decl)
    return m.group(1) if m else "?"


def _load_kb() -> List[FuncEntry]:
    """Load all function entries from kb.json into a flat list."""
    with open(KB_PATH, "r") as f:
        kb = json.load(f)

    entries: List[FuncEntry] = []

    def _walk(node: object) -> None:
        if isinstance(node, dict):
            addr_str = node.get("addr", "")
            decl = node.get("decl", "")
            if addr_str and decl and addr_str.startswith("0x"):
                try:
                    addr = int(addr_str, 16)
                except ValueError:
                    addr = -1
                if addr > 0:
                    name = node.get("name", "") or _name_from_decl(decl)
                    ported = bool(node.get("ported", False))
                    entries.append(FuncEntry(addr, addr_str, decl, name, ported))
            for v in node.values():
                _walk(v)
        elif isinstance(node, list):
            for item in node:
                _walk(item)

    _walk(kb)

    # Deduplicate by addr (keep first occurrence)
    seen: set = set()
    deduped: List[FuncEntry] = []
    for e in entries:
        if e.addr not in seen:
            seen.add(e.addr)
            deduped.append(e)
    return deduped


# ---------------------------------------------------------------------------
# Decl parser — stack-arg counting
# ---------------------------------------------------------------------------

# Matches @<reg> annotations in various styles that appear in kb.json:
#   int x @<ecx>
#   int x @<ecx>,
#   int x /* @<ecx> */
_REG_ANNOTATION_RE = re.compile(
    r'@<([a-zA-Z0-9]+)>'
)


_WIDE_TYPE_RE = re.compile(r"\b(double|__int64|u?int64_t|long\s+long)\b")


def _param_slots(param: str) -> int:
    """Stack dword slots one parameter occupies: 64-bit value types
    (double, __int64, int64_t, uint64_t, long long) take 2 slots;
    pointers (including pointers to 64-bit types) and everything else
    take 1.  ADD ESP,N counts slots, not parameters."""
    if "*" in param:
        return 1
    if _WIDE_TYPE_RE.search(param):
        return 2
    return 1


_FASTCALL_RE = re.compile(r'\b__fastcall\b')

# Non-floating, non-64-bit param types that __fastcall passes in ECX/EDX.
# A param is a "register candidate" when it is not a 64-bit type, not a float,
# not a double, and does not already carry a @<reg> annotation.
_FLOAT_TYPE_RE  = re.compile(r'\bfloat\b')


def _is_fastcall_reg_candidate(param: str) -> bool:
    """True when a parameter would be placed in ECX or EDX by __fastcall.

    __fastcall puts the first two integer/pointer-sized (<=32-bit) params in
    ECX and EDX.  64-bit values (double, __int64, etc.) and float params are
    passed on the stack even under __fastcall.
    """
    if _REG_ANNOTATION_RE.search(param):
        return False          # already annotated — don't double-count
    if "*" in param:
        return True           # pointers always fit in a register
    if _WIDE_TYPE_RE.search(param):
        return False          # 64-bit → stack
    if _FLOAT_TYPE_RE.search(param):
        return False          # float → stack (FPU-passed under fastcall too)
    return True


def parse_decl(decl: str) -> Tuple[Optional[int], Optional[int]]:
    """
    Parse a C function declaration and return (stack_slots, reg_args).

    Returns (None, None) for varargs functions or unparseable decls.
    stack_slots = sum of dword slots of non-@reg params (64-bit types = 2).

    For __fastcall declarations the first two integer/pointer-typed params
    (that are not already @<reg>-annotated) are counted as register args and
    excluded from the stack slot total, matching the x86 __fastcall ABI
    (ECX/EDX receive the first two eligible params; callers do not push them).
    """
    paren_start = decl.find("(")
    paren_end = decl.rfind(")")
    if paren_start == -1 or paren_end == -1:
        return None, None

    params_raw = decl[paren_start + 1: paren_end]

    # Skip varargs entirely — caller-cleanup amount is unknowable from decl alone
    if "..." in params_raw:
        return None, None

    params_stripped = params_raw.strip()

    # Empty or "void" param list → 0 stack args
    if params_stripped == "" or params_stripped == "void":
        return 0, 0

    params = _split_params(params_stripped)
    # Filter out empty segments that can arise from trailing commas
    params = [p for p in params if p.strip()]

    is_fastcall = bool(_FASTCALL_RE.search(decl))

    if is_fastcall:
        # Under __fastcall, the first two register-eligible params go in
        # ECX/EDX and are NOT pushed by the caller.
        fastcall_reg_budget = 2
        reg_count_from_annotations = sum(
            1 for p in params if _REG_ANNOTATION_RE.search(p))
        stack_args = 0
        extra_reg = 0
        for p in params:
            if _REG_ANNOTATION_RE.search(p):
                # Explicit @<reg> annotation — already a register arg.
                continue
            if fastcall_reg_budget > 0 and _is_fastcall_reg_candidate(p):
                extra_reg += 1
                fastcall_reg_budget -= 1
            else:
                stack_args += _param_slots(p)
        reg_count = reg_count_from_annotations + extra_reg
    else:
        reg_count = sum(1 for p in params if _REG_ANNOTATION_RE.search(p))
        stack_args = sum(
            _param_slots(p) for p in params if not _REG_ANNOTATION_RE.search(p))

    return stack_args, reg_count


def _split_params(params_str: str) -> List[str]:
    """Split a parameter string by top-level commas (respects parens for fn-ptrs)."""
    parts: List[str] = []
    depth = 0
    current: List[str] = []
    for ch in params_str:
        if ch == "(":
            depth += 1
            current.append(ch)
        elif ch == ")":
            depth -= 1
            current.append(ch)
        elif ch == "," and depth == 0:
            parts.append("".join(current).strip())
            current = []
        else:
            current.append(ch)
    if current:
        last = "".join(current).strip()
        if last:
            parts.append(last)
    return parts


# ---------------------------------------------------------------------------
# Disassembly and call-site analysis
# ---------------------------------------------------------------------------

CLEANUP_SCAN_FORWARD  = 8   # max instructions forward from CALL looking for ADD ESP
PUSH_SCAN_BACKWARD    = 64  # max instructions backward to count pushes before CALL

# Capstone singleton
_cs: Optional[capstone.Cs] = None


def _get_cs() -> capstone.Cs:
    global _cs
    if _cs is None:
        _cs = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        _cs.detail = False    # detail not needed; saves memory
        _cs.skipdata = True   # skip undecodable bytes; essential for XBE sections
                              # that contain data interleaved with code
    return _cs


def _is_cf_terminator(mnemonic: str) -> bool:
    """True for instructions that bound a linear scan window."""
    return mnemonic in (
        "ret", "retn", "retf",
        "jmp", "jmpe",
        "ja", "jae", "jb", "jbe", "je", "jne",
        "jg", "jge", "jl", "jle",
        "js", "jns", "jp", "jnp", "jo", "jno",
        "jcxz", "jecxz",
        "call",   # stop backward scan at previous CALL
        "int", "hlt", "ud2",
    )


class CallSite(NamedTuple):
    caller_va:       int            # VA of the CALL instruction
    callee_va:       int            # decoded immediate target
    push_count:      int            # pushes in linear window before CALL
    cleanup_dwords:  Optional[int]  # N/4 from ADD ESP,N; None if not found
    high_confidence: bool           # cleanup found AND push_count == cleanup_dwords


def _disassemble_section(sec_idx: int) -> List[Tuple[int, str, str]]:
    """
    Return a list of (va, mnemonic, op_str) for all instructions in section sec_idx.
    """
    vaddr, vsize, _, _ = _xbe_sections[sec_idx]
    code = _xbe_bytes_cache[sec_idx]
    cs = _get_cs()
    return [(i.address, i.mnemonic, i.op_str) for i in cs.disasm(code, vaddr)]


def _collect_call_sites(
    insns: List[Tuple[int, str, str]],
    target_addrs: frozenset,
) -> List[CallSite]:
    """
    Walk a linear instruction list and return CallSite objects for every
    CALL imm32 whose target is in target_addrs.
    """
    sites: List[CallSite] = []
    n = len(insns)

    for i, (va, mnem, ops) in enumerate(insns):
        if mnem != "call":
            continue

        # Must be a direct immediate call
        try:
            target = int(ops, 16)
        except ValueError:
            continue
        if target not in target_addrs:
            continue

        # Count pushes in the linear window immediately before this CALL.
        # We walk backward and stop at any control-flow instruction or start.
        push_count = 0
        for j in range(i - 1, max(i - PUSH_SCAN_BACKWARD, -1), -1):
            _, prev_mnem, _ = insns[j]
            if _is_cf_terminator(prev_mnem):
                break
            if prev_mnem == "push":
                push_count += 1

        # Scan forward (up to CLEANUP_SCAN_FORWARD instructions) for ADD ESP,N.
        # Stop at any control-flow instruction (including the next CALL).
        cleanup_dwords: Optional[int] = None
        for j in range(i + 1, min(i + CLEANUP_SCAN_FORWARD + 1, n)):
            _, fwd_mnem, fwd_ops = insns[j]
            if fwd_mnem in ("ret", "retn", "retf", "jmp") or fwd_mnem.startswith("j"):
                break
            if fwd_mnem == "call":
                break
            if fwd_mnem == "add":
                m = re.match(r"esp\s*,\s*(0x[0-9a-f]+|\d+)", fwd_ops, re.IGNORECASE)
                if m:
                    try:
                        n_bytes = int(m.group(1), 0)
                        if n_bytes > 0 and n_bytes % 4 == 0:
                            cleanup_dwords = n_bytes // 4
                    except ValueError:
                        pass
                break   # first ADD ESP after the call is all we examine

        high_conf = (cleanup_dwords is not None and push_count == cleanup_dwords)
        sites.append(CallSite(
            caller_va=va,
            callee_va=target,
            push_count=push_count,
            cleanup_dwords=cleanup_dwords,
            high_confidence=high_conf,
        ))

    return sites


# ---------------------------------------------------------------------------
# Per-callee result
# ---------------------------------------------------------------------------

class CalleeResult(NamedTuple):
    addr:             int
    addr_str:         str
    name:             str
    declared_stack:   int
    total_sites:      int
    conclusive_sites: int
    observed:         Dict[int, int]   # cleanup_dwords → count-of-sites
    verdict:          str              # OK / UNDER-DECLARED / OVER-DECLARED / NO-SITES / INCONCLUSIVE
    severity:         str              # HIGH / INFO / OK
    details:          List[str]        # per-site human-readable lines


# ---------------------------------------------------------------------------
# Main analysis
# ---------------------------------------------------------------------------

def _analyze(
    entries: List[FuncEntry],
    callee_filter: Optional[int] = None,
) -> Tuple[List[CalleeResult], Dict]:
    """
    Disassemble the pristine XBE and cross-check stack-arg counts.
    Returns (results, stats_dict).
    """
    _load_xbe()

    addr_to_entry: Dict[int, FuncEntry] = {e.addr: e for e in entries}

    # Parse declared stack args; skip varargs
    callee_stack: Dict[int, int] = {}
    skipped_varargs = 0
    for e in entries:
        s, r = parse_decl(e.decl)
        if s is None:
            skipped_varargs += 1
            continue
        callee_stack[e.addr] = s

    callable_targets = frozenset(callee_stack.keys())

    # Disassemble all XBE sections and collect call sites
    all_sites: List[CallSite] = []
    for sec_idx in range(len(_xbe_sections)):
        try:
            insns = _disassemble_section(sec_idx)
        except Exception:
            continue
        all_sites.extend(_collect_call_sites(insns, callable_targets))

    # Group sites by callee address
    by_callee: Dict[int, List[CallSite]] = defaultdict(list)
    for site in all_sites:
        by_callee[site.callee_va].append(site)

    # Determine which callees to report
    report_addrs = [callee_filter] if callee_filter is not None else sorted(callable_targets)

    results: List[CalleeResult] = []
    stats: Dict = {
        "total_callees_audited": 0,
        "conclusive_sites":      0,
        "total_sites":           0,
        "findings_high":         0,
        "findings_info":         0,
        "skipped_varargs":       skipped_varargs,
    }

    for addr in report_addrs:
        e = addr_to_entry.get(addr)
        if e is None:
            continue
        s = callee_stack.get(addr)
        if s is None:
            continue   # varargs — already counted above

        stats["total_callees_audited"] += 1
        sites = by_callee.get(addr, [])
        stats["total_sites"] += len(sites)

        observed: Dict[int, int] = defaultdict(int)
        conclusive = 0
        detail_lines: List[str] = []
        verdict = "OK"
        severity = "OK"

        # First pass: collect high-confidence results.
        hc_mismatches = 0
        hc_matches = 0
        for site in sites:
            if site.high_confidence:
                if site.cleanup_dwords != s:
                    hc_mismatches += 1
                else:
                    hc_matches += 1

        # If high-confidence sites exist and ALL of them confirm the declared
        # stack count (zero HC mismatches), treat the declared count as
        # independently verified.  Low-confidence cleanup < declared is then
        # a combined-cleanup artifact, not a real over-declaration.
        hc_confirms_declared = (hc_mismatches == 0 and hc_matches > 0)

        for site in sites:
            if site.cleanup_dwords is not None:
                observed[site.cleanup_dwords] += 1
                mismatch = (site.cleanup_dwords != s)
                tag = "*** MISMATCH ***" if mismatch else "ok"

                if site.high_confidence:
                    conclusive += 1
                    stats["conclusive_sites"] += 1
                    if mismatch:
                        # High-confidence mismatch → HIGH severity
                        verdict = "UNDER-DECLARED" if site.cleanup_dwords > s else "OVER-DECLARED"
                        severity = "HIGH"

                else:
                    # Cleanup found but push_count != cleanup_dwords.
                    # One-directional sound signal: cleanup < declared is normally
                    # always a real finding (combined cleanups can only be >=).
                    # Exception: if HC sites independently confirm the declared
                    # count, LC cleanup < declared is a combined-cleanup artifact.
                    if mismatch and site.cleanup_dwords < s:
                        if not hc_confirms_declared and severity != "HIGH":
                            verdict = "OVER-DECLARED"
                            severity = "HIGH"
                        # else: HC confirms declared, skip this LC artifact
                    elif mismatch and site.cleanup_dwords > s and severity not in ("HIGH",):
                        if verdict == "OK":
                            verdict = "UNDER-DECLARED"
                            severity = "INFO"

                detail_lines.append(
                    f"  0x{site.caller_va:08x}: push={site.push_count:2d} "
                    f"cleanup={site.cleanup_dwords} high_conf={site.high_confidence}  {tag}"
                )
            else:
                detail_lines.append(
                    f"  0x{site.caller_va:08x}: push={site.push_count:2d} "
                    f"cleanup=none (inconclusive)"
                )

        if not sites:
            verdict = "NO-SITES"
            severity = "OK"
        elif not any(site.cleanup_dwords is not None for site in sites):
            verdict = "INCONCLUSIVE"
            severity = "OK"

        if severity == "HIGH":
            stats["findings_high"] += 1
        elif severity == "INFO":
            stats["findings_info"] += 1

        results.append(CalleeResult(
            addr=addr,
            addr_str=e.addr_str,
            name=e.name,
            declared_stack=s,
            total_sites=len(sites),
            conclusive_sites=conclusive,
            observed=dict(observed),
            verdict=verdict,
            severity=severity,
            details=detail_lines,
        ))

    # Sort: HIGH first, then INFO, then OK; within each group by addr
    _sev_order = {"HIGH": 0, "INFO": 1, "OK": 2}
    results.sort(key=lambda r: (_sev_order.get(r.severity, 9), r.addr))
    return results, stats


# ---------------------------------------------------------------------------
# Output helpers
# ---------------------------------------------------------------------------

def _fmt_result(r: CalleeResult) -> str:
    obs_str = ", ".join(f"{k}:{v}" for k, v in sorted(r.observed.items())) or "none"
    return (
        f"{r.severity:6s}  {r.addr_str:12s}  {r.name:50s}  "
        f"declared_stack={r.declared_stack}  sites={r.total_sites}  "
        f"conclusive={r.conclusive_sites}  observed={{{obs_str}}}  "
        f"verdict={r.verdict}"
    )


def _print_report(results: List[CalleeResult], stats: Dict, verbose: bool = False) -> None:
    findings = [r for r in results if r.severity in ("HIGH", "INFO")]
    if findings:
        print(f"\n=== FINDINGS ({len(findings)}) ===")
        for r in findings:
            print(_fmt_result(r))
            if verbose:
                for d in r.details:
                    print(d)
    else:
        print("No findings.")

    print("\n=== SUMMARY ===")
    print(f"  Callees audited:   {stats['total_callees_audited']}")
    print(f"  Total call sites:  {stats['total_sites']}")
    print(f"  Conclusive sites:  {stats['conclusive_sites']}")
    print(f"  Skipped (varargs): {stats['skipped_varargs']}")
    print(f"  Findings HIGH:     {stats['findings_high']}")
    print(f"  Findings INFO:     {stats['findings_info']}")


def _write_full_report(results: List[CalleeResult], stats: Dict, path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w") as f:
        f.write("# check_arg_counts.py — full report\n\n")
        f.write("## Summary\n")
        for k, v in stats.items():
            f.write(f"  {k}: {v}\n")
        f.write("\n## Per-callee results\n\n")
        for r in results:
            f.write(_fmt_result(r) + "\n")
            for d in r.details:
                f.write(d + "\n")


# ---------------------------------------------------------------------------
# Self-test
# ---------------------------------------------------------------------------

def _self_test() -> None:
    print("=== Self-test: decl parser ===")

    cases = [
        # (decl, expected_stack, expected_reg, label)
        ("void foo(void);",
         0, 0, "void params"),
        ("void foo();",
         0, 0, "empty params"),
        ("int printf(const char *fmt, ...);",
         None, None, "varargs"),
        ("void foo(int a, int b, int c);",
         3, 0, "3 stack params"),
        ("float FUN_00012ad0(int actor_handle @<ebx>, int action_type @<esi>, void *charge_state);",
         1, 2, "2 inline @<reg> + 1 stack"),
        ("void FUN_00036890(int actor_handle /* @<eax> */, int *position_a /* @<ecx> */, "
         "short priority /* @<edx> */, int *position_b /* @<ebx> */, int param5);",
         1, 4, "4 comment @<reg> + 1 stack"),
        ("int* get_thing(int handle, int flags);",
         2, 0, "pointer return type"),
        ("void FUN_000d3080(int crosshair_overlay @<eax>, int hud_globals @<ecx>, "
         "int bitmap_data @<edx>, int param_1, short *param_2, float scale, "
         "int param_4, int color, int flags, char use_bitmap_size, char param_8);",
         8, 3, "3 @<reg> + 8 stack"),
        ("void foo(int a, void (*callback)(int), int c);",
         3, 0, "function pointer param"),
        ("void bar(int a, int b@<esi>, int c@<ecx>, int d);",
         2, 2, "mixed inline annotations"),
        ("double floor(double x);",
         2, 0, "double param = 2 dword slots"),
        ("void baz(double a, float b, double *c, __int64 d);",
         6, 0, "64-bit slot counting (2+1+1+2)"),
        # __fastcall: first two integer/pointer params → ECX/EDX (2 reg args),
        # remaining params go on stack (2 stack slots for uint32_t + uint32_t *).
        ("void __fastcall f(int a, int32_t *b, uint32_t c, uint32_t *d);",
         2, 2, "__fastcall: 2 reg + 2 stack"),
    ]

    all_pass = True
    for decl, exp_s, exp_r, label in cases:
        s, r = parse_decl(decl)
        ok = (s == exp_s and r == exp_r)
        status = "PASS" if ok else "FAIL"
        print(f"  {status}: {label} → got ({s}, {r}), expected ({exp_s}, {exp_r})")
        if not ok:
            all_pass = False

    if not all_pass:
        print("\nSelf-test FAILED (decl parser)")
        sys.exit(1)

    print("\n=== Self-test: real XBE analysis for 0xacff0 ===")

    if not XBE_PATH.exists():
        print(f"  SKIP: XBE not found at {XBE_PATH}")
        print("\nDecl-parser self-tests all PASSED (XBE tests skipped).")
        return

    if not KB_PATH.exists():
        print(f"  SKIP: kb.json not found at {KB_PATH}")
        print("\nDecl-parser self-tests all PASSED (XBE tests skipped).")
        return

    entries = _load_kb()
    addr_map = {e.addr: e for e in entries}
    target_addr = 0xacff0

    # --- Part A: real kb.json decl (currently 5 params) → expect no HIGH ---
    real_entry = addr_map.get(target_addr)
    if real_entry is None:
        print("  SKIP: 0xacff0 not present in kb.json")
    else:
        s_real, r_real = parse_decl(real_entry.decl)
        print(f"  Real decl has declared_stack={s_real} reg={r_real}")
        results_a, _ = _analyze(entries, callee_filter=target_addr)
        tr = next((r for r in results_a if r.addr == target_addr), None)
        if tr is None:
            print("  WARN: no result produced for 0xacff0 — no call sites found in XBE?")
        else:
            print(f"  Real decl result: {_fmt_result(tr)}")
            for d in tr.details:
                print(f"   {d.strip()}")
            assert tr.severity != "HIGH", \
                f"FAIL: expected no HIGH for real decl, got {tr.severity}/{tr.verdict}"
            print("  PASS: real 5-param decl → no HIGH finding")

    # --- Part B: override decl to 4 params → expect HIGH UNDER-DECLARED ---
    overridden_entries = []
    for e in entries:
        if e.addr == target_addr:
            fake_decl = (
                "void game_show_score_you_ally_enemy"
                "(int param_1, int param_2, int param_3, int param_4);"
            )
            overridden_entries.append(
                FuncEntry(e.addr, e.addr_str, fake_decl, e.name, e.ported)
            )
        else:
            overridden_entries.append(e)

    results_b, _ = _analyze(overridden_entries, callee_filter=target_addr)
    tr2 = next((r for r in results_b if r.addr == target_addr), None)
    if tr2 is None:
        print("  WARN: no result for 4-param override test — check XBE call sites")
    else:
        print(f"  4-param override result: {_fmt_result(tr2)}")
        for d in tr2.details:
            print(f"   {d.strip()}")
        assert tr2.severity == "HIGH" and tr2.verdict == "UNDER-DECLARED", \
            f"FAIL: expected HIGH/UNDER-DECLARED for 4-param override, got {tr2.severity}/{tr2.verdict}"
        print("  PASS: 4-param override → HIGH UNDER-DECLARED")

    print("\nAll self-tests PASSED.")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> None:
    ap = argparse.ArgumentParser(
        description="Cross-check kb.json stack-arg counts against XBE call-site ADD ESP cleanup.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    ap.add_argument("--callee", metavar="ADDR",
                    help="Filter to one callee (hex, e.g. 0xacff0)")
    ap.add_argument("--check", action="store_true",
                    help="Exit 1 if any HIGH finding is present")
    ap.add_argument("--self-test", action="store_true",
                    help="Run self-tests and exit")
    ap.add_argument("--verbose", "-v", action="store_true",
                    help="Print per-site details for findings")
    ap.add_argument("--no-report", action="store_true",
                    help="Skip writing artifacts/audit/arg_count_report.txt")
    args = ap.parse_args()

    if args.self_test:
        _self_test()
        sys.exit(0)

    for p, label in [(KB_PATH, "kb.json"), (XBE_PATH, "pristine XBE")]:
        if not p.exists():
            print(f"ERROR: {label} not found at {p}", file=sys.stderr)
            sys.exit(1)

    callee_filter: Optional[int] = None
    if args.callee:
        try:
            callee_filter = int(args.callee, 16)
        except ValueError:
            print(f"ERROR: Invalid hex address: {args.callee}", file=sys.stderr)
            sys.exit(1)

    print(f"Loading kb.json ... ", end="", flush=True)
    entries = _load_kb()
    print(f"{len(entries)} functions loaded.")

    print(f"Loading XBE and disassembling ... ", end="", flush=True)
    t0 = time.time()
    results, stats = _analyze(entries, callee_filter=callee_filter)
    elapsed = time.time() - t0
    print(f"done ({elapsed:.1f}s)")

    if callee_filter is not None:
        # For a single callee always show verbose details
        print(f"\n=== Results for {args.callee} ===")
        for r in results:
            if r.addr == callee_filter:
                print(_fmt_result(r))
                for d in r.details:
                    print(d)
        print(f"\nRuntime: {elapsed:.1f}s")
    else:
        _print_report(results, stats, verbose=args.verbose)
        if not args.no_report:
            report_path = REPORT_DIR / "arg_count_report.txt"
            _write_full_report(results, stats, report_path)
            print(f"\nFull report written to: {report_path}")
        print(f"Runtime: {elapsed:.1f}s")

    if args.check and stats["findings_high"] > 0:
        sys.exit(1)


if __name__ == "__main__":
    main()
