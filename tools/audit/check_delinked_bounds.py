#!/usr/bin/env python3
"""Detect BLOATED delinked references (lift-learnings: reference bounding).

A whole-object delinked reference gives each function symbol a span running to
the NEXT SYMBOL in that object.  When a function is followed by code that no
symbol covers -- an *unlisted* function, typically one with no xrefs because it
is reached indirectly -- that code is swallowed into the preceding symbol.  The
reference then holds far more instructions than the real function, and
vc71_verify scores our (correct) lift against it as a large mismatch.

This has produced several false "failed lift" verdicts:
  * FUN_00021270 (burst_parameters) -- unlisted FUN_00021310 -> 66.7%, true 84.9%
  * FUN_00174510 (transparent geom) -- unlisted FUN_00174690 -> 53.4%, true 100.0%
  * FUN_001a88b0                    -- 22.0%, true 100.0%
  * FUN_00103d30                    -- 48.8%, true  93.3%

vc71_verify already falls back to a per-function chunk when it detects the
whole-object reference is TRUNCATED.  It has no detector for the opposite
(bloated) case, which is what this script provides.

Truth comes from the pristine XBE, not from Ghidra: a function body closes at
the first terminator (ret / unconditional jmp) with no outstanding branch
target at or beyond it.  That second clause is what keeps us from cutting a
function short at an interior `ret` when MSVC has placed a tail block out of
line -- FUN_00174510 looks finished at 0x174622 until you notice the
`jl 0x174622` at 0x174608 proving the body continues.

Exit codes: 0 = no bloat found, 1 = bloat found (with --check), 2 = usage error.
"""
import argparse
import os
import re
import struct
import subprocess
import sys

DELINKED = "delinked"
XBE = "halo-patched/cachebeta.xbe"
MAX_SCAN = 0x4000
# Known (entry, true_end) pairs confirmed against Ghidra's reported body bounds.
SELF_TEST = [(0x174510, 0x174690), (0x103D30, 0x103D7C),
             (0x21FB0, 0x22008), (0x1C7D10, 0x1C7D70)]


def load_xbe(path):
    data = open(path, "rb").read()
    base = struct.unpack_from("<I", data, 0x104)[0]
    nsec = struct.unpack_from("<I", data, 0x11C)[0]
    shdr = struct.unpack_from("<I", data, 0x120)[0] - base
    secs = []
    for i in range(nsec):
        o = shdr + i * 0x38
        secs.append((struct.unpack_from("<I", data, o + 4)[0],    # vaddr
                     struct.unpack_from("<I", data, o + 8)[0],    # vsize
                     struct.unpack_from("<I", data, o + 12)[0]))  # raw
    return data, secs


def va_to_off(secs, va):
    for vaddr, vsize, raw in secs:
        if vaddr <= va < vaddr + vsize:
            return raw + (va - vaddr)
    return None


def true_end(data, secs, entry):
    """Return (end_address, warning_or_None) for the function at `entry`."""
    try:
        from capstone import CS_ARCH_X86, CS_MODE_32, Cs
    except ImportError:
        return None, "capstone not installed"
    off = va_to_off(secs, entry)
    if off is None:
        return None, "address not in any section"
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    targets = set()
    last = None
    for ins in md.disasm(data[off:off + MAX_SCAN], entry):
        if ins.mnemonic.startswith("j"):
            try:
                targets.add(int(ins.op_str, 16))
            except ValueError:
                pass  # indirect branch; the guard below simply won't fire early
        end = ins.address + ins.size
        terminator = ins.mnemonic == "ret" or (
            ins.mnemonic == "jmp" and not ins.op_str.startswith("dword"))
        if terminator and not any(t >= end for t in targets):
            return end, None
        last = end
    return last, "no terminator within 0x%x bytes" % MAX_SCAN


def obj_spans(path):
    """Return {symbol: (offset, span_to_next_or_None)} for section-1 symbols."""
    try:
        out = subprocess.run(["objdump", "-t", path],
                             capture_output=True, text=True, check=False).stdout
    except FileNotFoundError:
        print("error: objdump not found", file=sys.stderr)
        sys.exit(2)
    syms = []
    for line in out.splitlines():
        if "(sec  1)" not in line or "scl   2" not in line:
            continue
        m = re.search(r"(0x[0-9a-f]+)\s+(\S+)\s*$", line)
        if m:
            syms.append((int(m.group(1), 16), m.group(2)))
    syms.sort()
    out = {}
    for i, (off, name) in enumerate(syms):
        nxt = syms[i + 1][0] if i + 1 < len(syms) else None
        out[name] = (off, (nxt - off) if nxt is not None else None)
    return out


def text_size(path):
    out = subprocess.run(["objdump", "-h", path],
                         capture_output=True, text=True, check=False).stdout
    m = re.search(r"\.text\s+([0-9a-f]{8})", out)
    return int(m.group(1), 16) if m else None


def resolve(only):
    if only:
        p = only if os.path.exists(only) else os.path.join(DELINKED, only)
        if not os.path.exists(p):
            print("error: no such reference: %s" % only, file=sys.stderr)
            sys.exit(2)
        return [p]
    if not os.path.isdir(DELINKED):
        print("error: %s/ not found (run from repo root)" % DELINKED, file=sys.stderr)
        sys.exit(2)
    return [os.path.join(DELINKED, f)
            for f in sorted(os.listdir(DELINKED)) if f.endswith(".obj")]


def report(name, ref, true, min_excess):
    if ref is None:
        return "%-30s ref=last-symbol  true=0x%-6x  (unbounded)" % (name, true), False
    excess = ref - true
    bad = excess > min_excess
    verdict = "BLOAT +0x%x" % excess if bad else ("ok" if excess >= 0 else
                                                 "REF SHORTER %d" % excess)
    return ("%-30s ref=0x%-6x true=0x%-6x  %s" % (name, ref, true, verdict)), bad


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--object", help="single reference, e.g. rasterizer.obj "
                                     "or functions/00174510.obj")
    ap.add_argument("--symbol", help="only report this symbol")
    ap.add_argument("--entry", help="entry address (hex); needed when --symbol "
                                    "is a real name rather than FUN_<addr>")
    ap.add_argument("--xbe", default=XBE, help="pristine XBE (default %s)" % XBE)
    ap.add_argument("--min-excess", type=int, default=16,
                    help="ignore excess at or below this many bytes (alignment)")
    ap.add_argument("--check", action="store_true", help="exit 1 when bloat found")
    ap.add_argument("--self-test", action="store_true",
                    help="validate boundary detection against known-good bounds")
    args = ap.parse_args()

    if not os.path.exists(args.xbe):
        print("error: %s not found (run from repo root)" % args.xbe, file=sys.stderr)
        return 2
    data, secs = load_xbe(args.xbe)

    if args.self_test:
        bad = 0
        for entry, want in SELF_TEST:
            got, warn = true_end(data, secs, entry)
            ok = got == want
            bad += 0 if ok else 1
            print("%-12s expected %08x got %s  %s"
                  % ("%08x" % entry, want, ("%08x" % got) if got else warn,
                     "ok" if ok else "MISMATCH"))
        print("\nself-test: %d/%d" % (len(SELF_TEST) - bad, len(SELF_TEST)))
        return 1 if bad else 0

    found = 0
    if args.symbol:
        if not args.object:
            print("error: --symbol needs --object", file=sys.stderr)
            return 2
        if args.entry:
            entry = int(args.entry, 16)
        else:
            m = re.search(r"FUN_([0-9a-fA-F]+)$", args.symbol)
            if not m:
                print("error: cannot derive entry from %s -- pass --entry"
                      % args.symbol, file=sys.stderr)
                return 2
            entry = int(m.group(1), 16)
        path = resolve(args.object)[0]
        spans = obj_spans(path)
        if args.symbol not in spans:
            print("error: %s absent from %s" % (args.symbol, path), file=sys.stderr)
            return 2
        _, span = spans[args.symbol]
        if span is None:  # last symbol: the whole .text is its extent
            span = text_size(path)
        end, warn = true_end(data, secs, entry)
        if end is None:
            print("error: %s" % warn, file=sys.stderr)
            return 2
        line, bad = report(args.symbol, span, end - entry, args.min_excess)
        print(line)
        return 1 if (args.check and bad) else 0

    for path in resolve(args.object):
        spans = obj_spans(path)
        for name, (_, span) in sorted(spans.items()):
            m = re.search(r"FUN_([0-9a-fA-F]{6,8})$", name)
            if not m or span is None:
                continue  # named symbols need --entry; last symbol has no span
            entry = int(m.group(1), 16)
            end, _w = true_end(data, secs, entry)
            if end is None:
                continue
            line, bad = report(name, span, end - entry, args.min_excess)
            if bad:
                found += 1
                print("%-34s %s" % (os.path.basename(path), line))
    print("\n%d bloated reference span(s)" % found)
    return 1 if (args.check and found) else 0


if __name__ == "__main__":
    sys.exit(main())
