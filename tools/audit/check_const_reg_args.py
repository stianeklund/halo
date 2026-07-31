#!/usr/bin/env python3
"""check_const_reg_args.py — constant-for-register-argument audit.

Detects the defect class where a lift passes a hardcoded literal (in practice
almost always ``0``) as a call argument at a site where the ORIGINAL pushes a
register holding one of the function's parameters.

Why this needs its own checker
------------------------------
This bug is invisible to every gate we already have:

* It never crashes and never trips an assert -- the callee gets a valid-looking
  handle (datum handle 0 is a real player), just the wrong one.
* VC71 byte-match does NOT catch it and frequently *rewards* it: the wrong
  ``push 0`` is one instruction, while the correct ``push [ebp+N]`` can be two,
  and neither matches the original's ``push ebx`` when the parameter arrives in
  a register. Fixing FUN_000acd00 on 2026-07-31 LOWERED its score 85.7 -> 82.1.
  A score-only workflow will happily keep the broken version.
* check_lift_hazards.py cannot host it: every check there is source-only, with
  the signature (filepath, content, lines). This one has to disassemble the
  pristine XBE, so it lives with the other binary sweeps (check_arg_counts.py,
  check_stdcall_ret.py, check_callee_reg_args.py).

Found on 2026-07-31 (commits 0b1f5ce3, 8a773bd9, 2c3725b8):
  FUN_000ae920  3 sites  scoreboard title showed player slot 0's score
  FUN_000acd00  1 site   post-spawn invisibility clear hit the wrong unit
  FUN_000b39a0  1 site   Race best-lap event: wrong type AND null target
  FUN_000adb20  1 site   undeclared @<eax> param; team spawn rating mis-scored

Method
------
For each ported kb.json function with a source file:
  1. Extract the C body (brace match from its definition).
  2. Strip comments/strings, find call expressions, split arguments at
     top-level commas, and count arguments whose text is exactly ``0``.
     Parsing real argument position is what makes this precise -- a naive
     regex such as ``0\\s*[,)]`` also matches ``!= 0)`` / ``== 0)`` and
     produced 99 hits (mostly noise) versus 5 for this approach.
  3. Disassemble the original function (bounded by the next kb.json address)
     and count zero-valued pushes: ``push 0`` plus ``xor r,r`` / ``sub r,r``
     followed by ``push r`` with no intervening write to r.
  4. Report when the C count exceeds the assembly count.

Known false-positive shapes (documented, and why --check needs a baseline):
  * An inlined ``csmemset(buf, 0, N)`` becomes ``xor edx,edx`` + N ``mov``
    stores with no push at all (FUN_000ac3e0).
  * Jump tables let several C source sites share ONE ``xor eax,eax`` tail
    block -- game_engine_remap_weapon has 4 sites folding into 0xa9888.
  * Linear disassembly can walk into jump-table DATA and fabricate counts;
    functions whose disassembly hits an invalid instruction are skipped.

Usage
-----
    python3 tools/audit/check_const_reg_args.py                # report all
    python3 tools/audit/check_const_reg_args.py --check        # gate
    python3 tools/audit/check_const_reg_args.py --addr 0xadb20 # one function
    python3 tools/audit/check_const_reg_args.py --changed-only # touched files
    python3 tools/audit/check_const_reg_args.py --self-test
    python3 tools/audit/check_const_reg_args.py --update-baseline

--check exits non-zero on findings that are absent from
tools/audit/const_reg_args_baseline.json, or whose counts CHANGED since it was
written (a finding that changes re-fires rather than staying forgiven -- the
same rule test_baseline_gates.py pins for the other audits).

Never baseline a function you are actively lifting. Confirm against the
disassembly first: the fix is to pass the parameter, and the giveaways in
Ghidra are an ``in_EAX``-style implicit input in the decompile and a
``mov <reg>, <param reg>`` immediately before the CALL in the caller.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import struct
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Tuple

REPO_ROOT = Path(__file__).resolve().parents[2]
KB_PATH = REPO_ROOT / "kb.json"
XBE_PATH = REPO_ROOT / "halo-patched" / "cachebeta.xbe"
BASELINE_PATH = REPO_ROOT / "tools" / "audit" / "const_reg_args_baseline.json"
REPORT_PATH = REPO_ROOT / "artifacts" / "audit" / "const_reg_args_report.txt"

# Statement keywords that look like calls but are not.
NOT_CALLS = {
    "if", "while", "for", "switch", "sizeof", "return", "do", "else", "case",
    "defined", "assert",
}

MAX_FUNC_BYTES = 0x3000

REG_PARENT = {
    "ax": "eax", "al": "eax", "ah": "eax",
    "bx": "ebx", "bl": "ebx", "bh": "ebx",
    "cx": "ecx", "cl": "ecx", "ch": "ecx",
    "dx": "edx", "dl": "edx", "dh": "edx",
    "si": "esi", "di": "edi", "bp": "ebp", "sp": "esp",
}


def _canon(reg: str) -> str:
    return REG_PARENT.get(reg, reg)


# ---------------------------------------------------------------------------
# XBE
# ---------------------------------------------------------------------------

class Xbe:
    """Minimal XBE section walker: virtual address -> file offset."""

    def __init__(self, path: Path):
        self.data = path.read_bytes()
        base = struct.unpack_from("<I", self.data, 0x104)[0]
        nsec = struct.unpack_from("<I", self.data, 0x11C)[0]
        sec = struct.unpack_from("<I", self.data, 0x120)[0] - base
        self.sections = [
            struct.unpack_from("<IIII", self.data, sec + i * 0x38 + 4)
            for i in range(nsec)
        ]

    def va_to_off(self, va: int) -> Optional[int]:
        for vaddr, vsize, raw, rsize in self.sections:
            if vaddr <= va < vaddr + vsize and (va - vaddr) < rsize:
                return raw + (va - vaddr)
        return None

    def read(self, va: int, n: int) -> Optional[bytes]:
        off = self.va_to_off(va)
        if off is None:
            return None
        return self.data[off:off + n]


# ---------------------------------------------------------------------------
# C source parsing
# ---------------------------------------------------------------------------

def strip_comments_and_strings(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    text = re.sub(r"//[^\n]*", "", text)
    text = re.sub(r"L?'(\\.|[^'\\])*'", "''", text)
    text = re.sub(r'L?"(\\.|[^"\\])*"', '""', text)
    return text


def function_name(decl: str) -> Optional[str]:
    m = re.search(r"([A-Za-z_]\w*)\s*\(", decl)
    return m.group(1) if m else None


def extract_body(src: str, name: str) -> Optional[str]:
    """Return the brace-delimited body of `name`'s definition, or None."""
    pattern = re.compile(r"(?m)^[A-Za-z_][\w \*\t]*\b" + re.escape(name) + r"\s*\(")
    for m in pattern.finditer(src):
        open_brace = src.find("{", m.end())
        if open_brace < 0:
            continue
        # A ';' between the signature and '{' means this was a declaration.
        if ";" in src[m.end():open_brace]:
            continue
        depth = 0
        for i in range(open_brace, len(src)):
            if src[i] == "{":
                depth += 1
            elif src[i] == "}":
                depth -= 1
                if depth == 0:
                    return src[open_brace:i + 1]
        return src[open_brace:]
    return None


def split_top_level_args(inner: str) -> List[str]:
    args, depth, cur = [], 0, []
    for ch in inner:
        if ch in "([":
            depth += 1
        elif ch in ")]":
            depth -= 1
        if ch == "," and depth == 0:
            args.append("".join(cur))
            cur = []
        else:
            cur.append(ch)
    args.append("".join(cur))
    return args


def count_literal_zero_args(body: str) -> Tuple[int, List[str]]:
    """Count arguments that are exactly `0` in call expressions."""
    total, callees = 0, []
    for m in re.finditer(r"([A-Za-z_]\w*)\s*\(", body):
        name = m.group(1)
        if name in NOT_CALLS:
            continue
        depth, i = 1, m.end()
        while i < len(body) and depth > 0:
            if body[i] == "(":
                depth += 1
            elif body[i] == ")":
                depth -= 1
            i += 1
        if depth != 0:
            continue
        for arg in split_top_level_args(body[m.end():i - 1]):
            if arg.strip() == "0":
                total += 1
                callees.append(name)
    return total, callees


# ---------------------------------------------------------------------------
# Disassembly
# ---------------------------------------------------------------------------

def count_zero_pushes(md, code: bytes, va: int) -> Optional[int]:
    """Count pushes of a zero value. None if the disassembly is unusable."""
    zeroed: set = set()
    count = 0
    seen = 0
    for ins in md.disasm(code, va):
        seen += 1
        mn, ops = ins.mnemonic, ins.op_str
        if mn == "push":
            if ops in ("0", "0x0"):
                count += 1
            elif _canon(ops) in zeroed:
                count += 1
        elif mn in ("xor", "sub"):
            parts = [p.strip() for p in ops.split(",")]
            if len(parts) == 2 and parts[0] == parts[1]:
                zeroed.add(_canon(parts[0]))
            elif len(parts) == 2:
                zeroed.discard(_canon(parts[0]))
        elif mn in ("mov", "movzx", "movsx", "lea", "pop", "add", "or", "and",
                    "inc", "dec", "imul", "shl", "shr", "sar", "not", "neg"):
            parts = [p.strip() for p in ops.split(",")]
            if parts:
                zeroed.discard(_canon(parts[0]))
        elif mn == "call":
            # Calls clobber the caller-saved scratch registers.
            zeroed -= {"eax", "ecx", "edx"}
    if seen == 0:
        return None
    return count


# ---------------------------------------------------------------------------
# Scan
# ---------------------------------------------------------------------------

@dataclass
class Finding:
    addr: str
    name: str
    src: str
    c_zeros: int
    asm_zeros: int
    reg_args: bool
    callees: List[str] = field(default_factory=list)

    @property
    def delta(self) -> int:
        return self.c_zeros - self.asm_zeros

    def key(self) -> str:
        return self.addr


def iter_kb_functions(kb: dict) -> List[dict]:
    out: List[dict] = []

    def walk(node):
        if isinstance(node, dict):
            if "addr" in node and "decl" in node:
                out.append(node)
            for v in node.values():
                walk(v)
        elif isinstance(node, list):
            for v in node:
                walk(v)

    walk(kb)
    return out


def scan(only_addr: Optional[str] = None,
         only_sources: Optional[set] = None) -> List[Finding]:
    try:
        import capstone
    except ImportError:
        print("error: capstone is required (pip install capstone)", file=sys.stderr)
        raise SystemExit(2)

    if not XBE_PATH.exists():
        print(f"error: pristine XBE not found at {XBE_PATH}", file=sys.stderr)
        raise SystemExit(2)

    xbe = Xbe(XBE_PATH)
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    kb = json.loads(KB_PATH.read_text())

    by_addr: Dict[int, dict] = {}
    for fn in iter_kb_functions(kb):
        try:
            by_addr[int(fn["addr"], 16)] = fn
        except (ValueError, TypeError):
            continue
    addrs = sorted(by_addr)

    want = int(only_addr, 16) if only_addr else None
    src_cache: Dict[str, str] = {}
    findings: List[Finding] = []

    for idx, addr in enumerate(addrs):
        fn = by_addr[addr]
        if want is not None and addr != want:
            continue
        if not fn.get("ported"):
            continue
        src = fn.get("src")
        if not src:
            continue
        if only_sources is not None and src not in only_sources:
            continue
        src_path = REPO_ROOT / src
        if not src_path.exists():
            continue
        name = function_name(fn["decl"])
        if not name:
            continue

        if src not in src_cache:
            src_cache[src] = src_path.read_text(encoding="utf-8", errors="replace")
        body = extract_body(src_cache[src], name)
        if body is None:
            continue

        c_zeros, callees = count_literal_zero_args(strip_comments_and_strings(body))
        if c_zeros == 0:
            continue

        end = addrs[idx + 1] if idx + 1 < len(addrs) else addr + 0x400
        length = min(end - addr, MAX_FUNC_BYTES)
        if length <= 0:
            continue
        code = xbe.read(addr, length)
        if not code:
            continue
        asm_zeros = count_zero_pushes(md, code, addr)
        if asm_zeros is None:
            continue

        if c_zeros > asm_zeros:
            findings.append(Finding(
                addr=fn["addr"], name=name, src=src,
                c_zeros=c_zeros, asm_zeros=asm_zeros,
                reg_args="@<" in fn["decl"],
                callees=sorted(set(callees)),
            ))

    findings.sort(key=lambda f: (-f.delta, f.addr))
    return findings


# ---------------------------------------------------------------------------
# Baseline
# ---------------------------------------------------------------------------

def load_baseline() -> Dict[str, Dict]:
    if not BASELINE_PATH.exists():
        return {}
    return json.loads(BASELINE_PATH.read_text()).get("entries", {})


def write_baseline(findings: List[Finding]) -> None:
    # Merge-preserving: a human-written "reason" explains WHY an entry is
    # accepted, and must survive regeneration. Losing it forces the next
    # reader to re-derive the false-positive analysis from scratch.
    previous = load_baseline()
    entries = {}
    for f in findings:
        entry = {
            "name": f.name,
            "src": f.src,
            "c_zeros": f.c_zeros,
            "asm_zeros": f.asm_zeros,
        }
        reason = previous.get(f.key(), {}).get("reason")
        if reason:
            entry["reason"] = reason
        entries[f.key()] = entry
    payload = {
        "_comment": (
            "Pre-existing constant-for-register-argument findings accepted as "
            "latent or verified false-positive. --check gates only findings "
            "absent from this file or whose c_zeros/asm_zeros CHANGED -- a "
            "changed finding re-fires rather than staying forgiven. Do NOT "
            "baseline a function you are lifting: confirm against the "
            "disassembly and pass the parameter instead. "
            "Regenerate: check_const_reg_args.py --update-baseline"
        ),
        "entries": dict(sorted(entries.items())),
    }
    BASELINE_PATH.parent.mkdir(parents=True, exist_ok=True)
    BASELINE_PATH.write_text(json.dumps(payload, indent=2) + "\n")


def new_findings(findings: List[Finding],
                 baseline: Dict[str, Dict]) -> List[Finding]:
    out = []
    for f in findings:
        b = baseline.get(f.key())
        if (b is None
                or b.get("c_zeros") != f.c_zeros
                or b.get("asm_zeros") != f.asm_zeros):
            out.append(f)
    return out


# ---------------------------------------------------------------------------
# Changed-file selection
# ---------------------------------------------------------------------------

def _git(args: List[str]) -> List[str]:
    try:
        out = subprocess.run(["git", "-C", str(REPO_ROOT)] + args,
                             capture_output=True, text=True, check=False)
    except OSError:
        return []
    return [ln.strip() for ln in out.stdout.splitlines() if ln.strip()]


def changed_sources(staged_only: bool) -> set:
    paths = set(_git(["diff", "--cached", "--name-only"]))
    if not staged_only:
        paths |= set(_git(["diff", "--name-only"]))
        paths |= set(_git(["ls-files", "--others", "--exclude-standard"]))
    return {p for p in paths if p.endswith(".c")}


# ---------------------------------------------------------------------------
# Self-test
# ---------------------------------------------------------------------------

def self_test() -> int:
    failures = []

    def check(label, got, want):
        if got != want:
            failures.append(f"{label}: got {got!r}, want {want!r}")

    # Argument parsing must not be fooled by comparisons or loop initialisers.
    n, _ = count_literal_zero_args("{ if (x == 0) { y = 0; } }")
    check("comparison/assignment are not args", n, 0)

    n, _ = count_literal_zero_args("{ for (i = 0; i < n; i++) f(a); }")
    check("for-initialiser is not an arg", n, 0)

    n, callees = count_literal_zero_args("{ foo(a, 0); }")
    check("trailing zero arg", n, 1)
    check("callee recorded", callees, ["foo"])

    n, _ = count_literal_zero_args("{ foo(0, b); }")
    check("leading zero arg", n, 1)

    n, _ = count_literal_zero_args("{ foo(bar(0), 0); }")
    check("nested call zero args", n, 2)

    n, _ = count_literal_zero_args("{ foo(a + 0, b); }")
    check("expression containing 0 is not a literal arg", n, 0)

    n, _ = count_literal_zero_args('{ foo("0", 0); }')
    check("string is stripped before counting", n, 1)

    # sizeof/if/while must not be treated as calls.
    n, _ = count_literal_zero_args("{ while (f(0)) ; }")
    check("call inside while still counts", n, 1)

    # Body extraction must skip prototypes.
    src = "void f(int a);\n\nvoid f(int a)\n{\n  g(0);\n}\n"
    body = extract_body(src, "f")
    check("body extracted past prototype", body is not None and "g(0)" in body, True)

    # Zero-push accounting.
    try:
        import capstone
    except ImportError:
        print("SKIP: capstone unavailable, disassembly tests not run")
    else:
        md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        # push 0 ; ret
        check("push imm0 counted", count_zero_pushes(md, b"\x6a\x00\xc3", 0x1000), 1)
        # xor eax,eax ; push eax ; ret
        check("xor+push counted",
              count_zero_pushes(md, b"\x31\xc0\x50\xc3", 0x1000), 1)
        # xor eax,eax ; mov eax,ebx ; push eax ; ret  -> no longer zero
        check("write clears zeroed reg",
              count_zero_pushes(md, b"\x31\xc0\x89\xd8\x50\xc3", 0x1000), 0)
        # push ebx ; ret -> a register arg, not a zero
        check("register push not counted",
              count_zero_pushes(md, b"\x53\xc3", 0x1000), 0)

    # Baseline diff semantics: unchanged stays quiet, changed re-fires.
    f = Finding(addr="0xdead", name="f", src="a.c", c_zeros=2, asm_zeros=0,
                reg_args=True)
    base_same = {"0xdead": {"c_zeros": 2, "asm_zeros": 0}}
    base_diff = {"0xdead": {"c_zeros": 1, "asm_zeros": 0}}
    check("baselined finding stays quiet", new_findings([f], base_same), [])
    check("changed finding re-fires", len(new_findings([f], base_diff)), 1)
    check("unknown finding fires", len(new_findings([f], {})), 1)

    if failures:
        print("SELF-TEST FAILED:")
        for msg in failures:
            print(f"  {msg}")
        return 1
    print("self-test OK")
    return 0


# ---------------------------------------------------------------------------
# Reporting
# ---------------------------------------------------------------------------

def format_findings(findings: List[Finding]) -> str:
    if not findings:
        return "no findings\n"
    lines = [
        f"{'delta':>5}  {'C0':>3}  {'asm0':>4}  {'addr':>10}  "
        f"{'reg':>3}  name / source / zero-arg callees",
    ]
    for f in findings:
        lines.append(
            f"{f.delta:>5}  {f.c_zeros:>3}  {f.asm_zeros:>4}  {f.addr:>10}  "
            f"{'yes' if f.reg_args else '  -':>3}  {f.name}"
        )
        lines.append(f"{'':>32}{f.src}  callees={f.callees[:6]}")
    return "\n".join(lines) + "\n"


def main() -> int:
    ap = argparse.ArgumentParser(
        description="Audit lifted C for literal constants passed where the "
                    "original pushes a register parameter.")
    ap.add_argument("--check", action="store_true",
                    help="Exit non-zero on findings not in the baseline")
    ap.add_argument("--addr", metavar="ADDR",
                    help="Audit a single function by kb.json address")
    ap.add_argument("--changed-only", action="store_true",
                    help="Only scan .c files you have touched "
                         "(staged + unstaged + untracked)")
    ap.add_argument("--staged-only", action="store_true",
                    help="Only scan staged .c files (used by the pre-commit hook)")
    ap.add_argument("--update-baseline", action="store_true",
                    help=f"Write current findings to {BASELINE_PATH.name}")
    ap.add_argument("--self-test", action="store_true",
                    help="Run internal consistency tests and exit")
    ap.add_argument("--no-report", action="store_true",
                    help=f"Skip writing {REPORT_PATH.relative_to(REPO_ROOT)}")
    args = ap.parse_args()

    if args.self_test:
        return self_test()

    only_sources = None
    if args.changed_only or args.staged_only:
        only_sources = changed_sources(staged_only=args.staged_only)
        if not only_sources:
            print("no changed .c files; nothing to audit")
            return 0

    findings = scan(only_addr=args.addr, only_sources=only_sources)

    if args.update_baseline:
        write_baseline(findings)
        print(f"baseline written: {BASELINE_PATH} ({len(findings)} entries)")
        return 0

    report = format_findings(findings)
    print(report, end="")

    reg_arg_count = sum(1 for f in findings if f.reg_args)
    print(f"\n{len(findings)} finding(s), {reg_arg_count} in functions with "
          f"@<reg> parameters")

    if not args.no_report:
        REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
        REPORT_PATH.write_text(report)
        print(f"report: {REPORT_PATH}")

    if args.check:
        fresh = new_findings(findings, load_baseline())
        if fresh:
            print(f"\nERROR: {len(fresh)} finding(s) not in the baseline:",
                  file=sys.stderr)
            for f in fresh:
                print(f"  {f.addr} {f.name} ({f.src}): "
                      f"{f.c_zeros} literal-0 arg(s) in C vs "
                      f"{f.asm_zeros} zero push(es) in the original",
                      file=sys.stderr)
            print("\n  Confirm against the disassembly before acting. If the "
                  "original pushes a register there, pass the parameter.\n"
                  "  Verified false positive (inlined memset, shared "
                  "jump-table tail block)? Add it with --update-baseline.",
                  file=sys.stderr)
            return 1
        print("--check OK: no new constant-for-register-arg findings.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
