#!/usr/bin/env python3
"""check_callee_reg_args.py: Detect calls from ported code to unported functions
that have implicit register arguments not yet annotated in kb.json.

The canonical failure mode: an unported callee uses a register (e.g. EAX) as an
input by reading it before writing it.  When ported C calls that function without
the @<reg> annotation, the compiler passes the argument on the stack; the callee
reads garbage from the unrelated register instead.

Detection strategy (no Ghidra required):
  1. Parse kb.json for all ported function bodies (source file + address range).
  2. For each ported source body, extract calls to unported kb.json functions.
  3. For each such callee without @<reg>, disassemble the first N instructions
     from the XBE and check for registers read before written.

Limitations / false-positive sources (all require human confirmation):
  - _chkstk / stack probes: MSVC loads EAX with the frame size before calling
    _chkstk. These show up as "EAX read-before-write" but are NOT register
    inputs — they are compiler-generated and already handled by declaring locals.
    This tool marks them with a [chkstk?] tag so you can skip them quickly.
  - Tail-call optimised functions whose first instruction is a JMP: the analysis
    reads only the trampoline bytes and misses the real function. Rare in practice.

Usage:
    python3 tools/audit/check_callee_reg_args.py
    python3 tools/audit/check_callee_reg_args.py --check   # exit 1 on findings
"""

import argparse
import json
import re
import struct
import sys
from pathlib import Path

try:
    import capstone
    HAS_CAPSTONE = True
except ImportError:
    HAS_CAPSTONE = False

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
KB_PATH   = REPO_ROOT / "kb.json"
XBE_PATH  = REPO_ROOT / "halo-patched" / "default.xbe"
SRC_ROOT  = REPO_ROOT / "src" / "halo"

# Registers we track for implicit input detection.
TRACKED = {"eax", "ecx", "edx", "ebx", "esi", "edi"}

# Map any sub-register to its 32-bit parent.
REG32 = {
    "al":"eax","ah":"eax","ax":"eax","eax":"eax",
    "cl":"ecx","ch":"ecx","cx":"ecx","ecx":"ecx",
    "dl":"edx","dh":"edx","dx":"edx","edx":"edx",
    "bl":"ebx","bh":"ebx","bx":"ebx","ebx":"ebx",
    "si":"esi","esi":"esi",
    "di":"edi","edi":"edi",
}

# EBX/ESI/EDI pushed in prologue = callee-save, NOT a register input.
CALLEE_SAVE_PUSH = {"ebx", "esi", "edi", "ebp"}

# Known MSVC stack-probe entry points: EAX holds frame size (not a user arg).
CHKSTK_ADDRS = {0x1d90e0}   # _chkstk at the known XBE address

# XBE address ranges that are SDK/library code (D3D, Bink, XNet, CRT).
# Register reads in these functions are internal implementation details,
# NOT input arguments that a ported caller needs to set.
# Any callee whose address falls in these ranges is excluded.
SDK_RANGES = [
    (0x1d1000, 0x1e69e0),   # XDK / XAPI / CRT (SetFileTime, XNet, qsort, str*)
    (0x1e69e0, 0x7fffffff), # D3D, DSOUND, XNet, Bink — everything above game .text
]

def is_sdk_addr(addr: int) -> bool:
    return any(lo <= addr < hi for lo, hi in SDK_RANGES)


# ---------------------------------------------------------------------------
# XBE utilities
# ---------------------------------------------------------------------------

def load_xbe(xbe_path: Path):
    """Return (raw_bytes, sections) where sections = list of (va, vsize, raw_off, raw_size)."""
    data = xbe_path.read_bytes()
    base      = struct.unpack_from("<I", data, 0x104)[0]
    n_sects   = struct.unpack_from("<I", data, 0x11C)[0]
    hdrs_va   = struct.unpack_from("<I", data, 0x120)[0]
    hdr_off   = hdrs_va - base
    sections  = []
    for i in range(n_sects):
        off = hdr_off + i * 0x38
        va       = struct.unpack_from("<I", data, off + 0x04)[0]
        vsize    = struct.unpack_from("<I", data, off + 0x08)[0]
        raw_off  = struct.unpack_from("<I", data, off + 0x0C)[0]
        raw_size = struct.unpack_from("<I", data, off + 0x10)[0]
        sections.append((va, vsize, raw_off, raw_size))
    return data, sections


def read_va(xbe_data: bytes, sections, va: int, length: int = 128) -> bytes | None:
    for (sec_va, sec_vsize, raw_off, raw_size) in sections:
        if sec_va <= va < sec_va + sec_vsize:
            off_in_sec = va - sec_va
            file_off   = raw_off + off_in_sec
            avail      = raw_size - off_in_sec
            if avail <= 0:
                return None
            return xbe_data[file_off : file_off + min(length, avail)]
    return None


# ---------------------------------------------------------------------------
# Register-input detection
# ---------------------------------------------------------------------------

def reg_inputs(code: bytes, va: int, max_insns: int = 50) -> tuple[set[str], bool]:
    """
    Return (inputs, chkstk_candidate):
      inputs            – set of 32-bit register names read before written
      chkstk_candidate  – True if the read pattern looks like a _chkstk probe
                          (EAX loaded then immediately used in a call/jmp)
    """
    if not HAS_CAPSTONE:
        return set(), False

    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True

    written:  set[str] = set()
    inputs:   set[str] = set()
    prologue  = True   # still in standard frame-setup sequence
    chkstk    = False

    prev_eax_write = False  # did last instruction write EAX?

    for n, insn in enumerate(md.disasm(code, va)):
        if n >= max_insns:
            break

        mn  = insn.mnemonic.lower()
        ops = insn.op_str.lower()

        # ---- Prologue suppression ----------------------------------------
        if prologue:
            # push ebp  →  skip
            if mn == "push" and ops == "ebp":
                written.add("ebp"); continue
            # push callee-save reg (ebx/esi/edi)  →  NOT a register input
            if mn == "push" and ops in CALLEE_SAVE_PUSH:
                written.add(ops); continue
            # mov ebp, esp  →  skip
            if mn == "mov" and ops in ("ebp, esp", "ebp,esp"):
                written.update({"ebp", "esp"}); continue
            # sub/and esp  →  stack allocation, skip
            if mn in ("sub", "and") and "esp" in ops:
                written.add("esp"); continue
            # xor eax,eax / xor ecx,ecx etc. → zeroing write, not read
            if mn == "xor":
                parts = [p.strip() for p in ops.split(",")]
                if len(parts) == 2 and parts[0] == parts[1] and parts[0] in TRACKED:
                    written.add(parts[0]); continue
            prologue = False   # first non-prologue instruction

        # ---- Reads ---------------------------------------------------------
        regs_read: set[str] = set()
        for rid in insn.regs_read:
            name = insn.reg_name(rid).lower()
            if name in REG32:
                regs_read.add(REG32[name])

        # ---- Writes --------------------------------------------------------
        regs_written: set[str] = set()
        for rid in insn.regs_write:
            name = insn.reg_name(rid).lower()
            if name in REG32:
                regs_written.add(REG32[name])

        # xor r,r idiom: treat as write only
        if mn == "xor":
            parts = [p.strip() for p in ops.split(",")]
            if len(parts) == 2 and parts[0] == parts[1] and parts[0] in TRACKED:
                regs_read.discard(parts[0])
                regs_written.add(parts[0])

        # ---- Classify ------------------------------------------------------
        for reg in regs_read:
            if reg in TRACKED and reg not in written:
                inputs.add(reg)

        # Check for _chkstk pattern: EAX was just loaded (mov eax, imm/local)
        # and now we see a call — EAX is the frame size, not a user arg.
        if mn in ("call", "jmp") and "eax" in inputs:
            # Resolve call target if possible
            target = None
            try:
                target = int(ops.replace("0x", ""), 16)
            except ValueError:
                pass
            if target in CHKSTK_ADDRS:
                chkstk = True
            elif prev_eax_write and "eax" in inputs:
                # Heuristic: EAX was just written then immediately used in a
                # call — strong _chkstk pattern even if we don't know the addr.
                chkstk = True

        prev_eax_write = "eax" in regs_written

        for reg in regs_written:
            if reg in TRACKED:
                written.add(reg)

    return inputs, chkstk


# ---------------------------------------------------------------------------
# kb.json parsing
# ---------------------------------------------------------------------------

def parse_kb():
    """
    Returns:
      funcs  : {addr_int: {name, decl, ported, has_reg, source}}
      obj_sources: {obj_name: source_path_str}
    """
    with open(KB_PATH) as f:
        kb = json.load(f)

    funcs = {}

    def walk_obj(obj, source=None):
        if isinstance(obj, list):
            for item in obj:
                walk_obj(item, source)
            return
        if not isinstance(obj, dict):
            return

        src = obj.get("source", source)

        # Object-level functions list
        for fn in obj.get("functions", []):
            addr_s = fn.get("addr")
            decl   = fn.get("decl", "")
            if not addr_s:
                continue
            try:
                addr = int(addr_s, 16)
            except ValueError:
                continue
            m = re.search(r'\b(\w+)\s*\(', decl)
            name = m.group(1) if m else "?"
            funcs[addr] = {
                "name":    name,
                "decl":    decl,
                "ported":  fn.get("ported", False),
                "has_reg": "@<" in decl,
                "source":  src,
            }

        for v in obj.values():
            if isinstance(v, (dict, list)):
                walk_obj(v, src)

    walk_obj(kb)
    return funcs


# ---------------------------------------------------------------------------
# Source scanning
# ---------------------------------------------------------------------------

# Matches FUN_XXXXXXXX( or a named kb symbol followed by (
_FUN_PAT = re.compile(r'\b(FUN_[0-9A-Fa-f]{8})\s*\(')


def extract_ported_bodies(src_path: Path, ported_names: set[str]) -> str:
    """
    Return a concatenation of the source text that belongs to ported functions.
    Heuristic: grab everything between `name(` opening brace and the matching
    closing brace.  Not a real C parser but good enough for call detection.
    """
    text = src_path.read_text(errors="replace")
    bodies = []
    for name in ported_names:
        # Find function definition: look for the name followed by ( not preceded
        # by another word char (to avoid matching FUN_001154a0 inside FUN_0011540)
        for m in re.finditer(r'(?<!\w)' + re.escape(name) + r'\s*\(', text):
            # Scan forward to find the opening {
            pos = m.end()
            brace_pos = text.find('{', pos)
            if brace_pos < 0:
                continue
            # Walk to matching closing brace
            depth = 0
            i = brace_pos
            while i < len(text):
                if text[i] == '{':
                    depth += 1
                elif text[i] == '}':
                    depth -= 1
                    if depth == 0:
                        bodies.append(text[brace_pos : i + 1])
                        break
                i += 1
    return "\n".join(bodies)


def calls_in_text(text: str, known_names: dict[str, int]) -> set[int]:
    """Return set of callee addresses referenced in text."""
    addrs = set()
    for m in _FUN_PAT.finditer(text):
        name = m.group(1)
        if name in known_names:
            addrs.add(known_names[name])
    # Also match renamed symbols
    for name, addr in known_names.items():
        if name.startswith("FUN_"):
            continue
        if re.search(r'(?<!\w)' + re.escape(name) + r'\s*\(', text):
            addrs.add(addr)
    return addrs


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                  formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--check", action="store_true",
                    help="Exit 1 if any findings (for CI gating)")
    ap.add_argument("--xbe", default=str(XBE_PATH), metavar="PATH",
                    help="Patched XBE to disassemble (default: halo-patched/default.xbe)")
    ap.add_argument("--max-insns", type=int, default=50, metavar="N",
                    help="Max instructions to disassemble per callee (default: 50)")
    args = ap.parse_args()

    if not HAS_CAPSTONE:
        print("ERROR: capstone not installed. Run: pip install capstone", file=sys.stderr)
        sys.exit(2)

    xbe_path = Path(args.xbe)
    if not xbe_path.exists():
        print(f"ERROR: XBE not found: {xbe_path}", file=sys.stderr)
        sys.exit(2)

    # Load assets
    xbe_data, sections = load_xbe(xbe_path)
    funcs = parse_kb()

    # Build lookup: name → addr (for renamed symbols)
    name_to_addr = {info["name"]: addr for addr, info in funcs.items()}
    # Also add addr-hex → addr for FUN_ names
    for addr, info in funcs.items():
        name_to_addr[info["name"]] = addr

    # Group ported functions by source file
    src_to_ported: dict[str, set[str]] = {}
    for addr, info in funcs.items():
        if not info["ported"] or not info["source"]:
            continue
        src_to_ported.setdefault(info["source"], set()).add(info["name"])

    # Collect all callee addresses referenced from ported bodies
    candidate_callees: set[int] = set()
    for src_rel, ported_names in src_to_ported.items():
        src_path = SRC_ROOT / src_rel
        if not src_path.exists():
            continue
        body = extract_ported_bodies(src_path, ported_names)
        candidate_callees.update(calls_in_text(body, name_to_addr))

    # Filter to unported callees without @<reg> and not in SDK address ranges
    to_check = [
        (addr, funcs[addr])
        for addr in sorted(candidate_callees)
        if addr in funcs
        and not funcs[addr]["ported"]
        and not funcs[addr]["has_reg"]
        and not is_sdk_addr(addr)
    ]

    findings = []
    for addr, info in to_check:
        code = read_va(xbe_data, sections, addr, args.max_insns * 8)
        if not code:
            continue
        inputs, chkstk = reg_inputs(code, addr, args.max_insns)
        if inputs:
            findings.append({
                "addr":    addr,
                "name":    info["name"],
                "decl":    info["decl"],
                "inputs":  sorted(inputs),
                "chkstk":  chkstk,
            })

    if not findings:
        print(f"OK — checked {len(to_check)} unported callee(s), no implicit register inputs found.")
        sys.exit(0)

    real = [f for f in findings if not f["chkstk"]]
    maybe = [f for f in findings if f["chkstk"]]

    if real:
        print(f"\n{'='*60}")
        print(f"MISSING @<reg>: {len(real)} callee(s) read registers before writing them.")
        print(f"These need @<reg> annotations in kb.json before porting their callers.\n")
        for f in real:
            regs = "  ".join(f"@<{r}>" for r in f["inputs"])
            print(f"  {f['addr']:08x}  {f['name']}")
            print(f"    possible register inputs: {regs}")
            print(f"    current decl: {f['decl'][:90]}")
            print()

    if maybe:
        print(f"[chkstk?] {len(maybe)} callee(s) show EAX read-before-write but pattern")
        print(f"suggests _chkstk / stack-probe (EAX = frame size, NOT a user arg).")
        print(f"Verify before adding @<eax>:\n")
        for f in maybe:
            print(f"  {f['addr']:08x}  {f['name']}  — {f['decl'][:70]}")
        print()

    if args.check and real:
        sys.exit(1)
    sys.exit(0)


if __name__ == "__main__":
    main()
