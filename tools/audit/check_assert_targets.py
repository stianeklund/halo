#!/usr/bin/env python3
"""
check_assert_targets.py — verify assert-tail CALL targets in lifted C against
the pristine XBE (lift-learnings §29).

The binary's assert tails are either `PUSH -1; CALL 0x8e2f0` (= system_exit(-1))
or `CALL 0x1029a0` (= halt_and_catch_fire), sometimes through a one-hop JMP
thunk.  Lift agents repeatedly substituted halt_and_catch_fire() where the
binary calls system_exit(-1) — hit 3x in rasterizer_decals.obj alone, costing
+5 to +11 VC71 points each time.  This detector makes the capstone
verification permanent:

  For every ported kb.json function with a source_path, count direct CALLs to
  system_exit / halt_and_catch_fire in its original body (thunk hops resolved)
  and compare against the call counts in the lifted C function body.

  ERROR = the lifted source uses one where the binary calls the other (swap).
  WARN  = same targets but call-site counts differ (can be legitimate MSVC
          cross-jump sharing of one assert tail; review, don't auto-fail).

Usage:
  python3 tools/audit/check_assert_targets.py               # full sweep, report
  python3 tools/audit/check_assert_targets.py --check       # exit 1 on ERROR
  python3 tools/audit/check_assert_targets.py --target FUN_00158df0
  python3 tools/audit/check_assert_targets.py --target 0x1595c0
  python3 tools/audit/check_assert_targets.py --self-test   # decoder regression
"""

import argparse
import bisect
import json
import re
import sys
from pathlib import Path
from typing import Dict, List, NamedTuple, Optional, Tuple

sys.path.insert(0, str(Path(__file__).resolve().parent))
import check_arg_counts as cac   # XBE loader + capstone (self-contained there)

REPO_ROOT = Path(__file__).resolve().parents[2]
KB_PATH = REPO_ROOT / "kb.json"

SYSTEM_EXIT_VA = 0x8E2F0    # void __noreturn system_exit(int)
HCF_VA         = 0x1029A0   # void halt_and_catch_fire(void)
TARGET_NAMES = {SYSTEM_EXIT_VA: "system_exit", HCF_VA: "halt_and_catch_fire"}

MAX_THUNK_HOPS = 2


class KbFunc(NamedTuple):
    addr: int
    name: str
    source_path: str
    ported: bool


def _load_kb_funcs() -> List[KbFunc]:
    with open(KB_PATH, "r") as f:
        kb = json.load(f)
    out: List[KbFunc] = []
    seen: set = set()

    def _walk(node):
        if isinstance(node, dict):
            addr_str = node.get("addr", "")
            decl = node.get("decl", "")
            if isinstance(addr_str, str) and addr_str.startswith("0x") and decl:
                try:
                    addr = int(addr_str, 16)
                except ValueError:
                    addr = -1
                if addr > 0 and addr not in seen:
                    seen.add(addr)
                    m = re.search(r"\b([A-Za-z_][A-Za-z0-9_]*)\s*\(", decl)
                    name = node.get("name") or (m.group(1) if m else "?")
                    out.append(KbFunc(
                        addr, name,
                        node.get("source_path") or "",
                        bool(node.get("ported", False)),
                    ))
            for v in node.values():
                _walk(v)
        elif isinstance(node, list):
            for item in node:
                _walk(item)

    _walk(kb)
    out.sort(key=lambda e: e.addr)
    return out


# ---------------------------------------------------------------------------
# Binary side: one linear decode per section, CALLs attributed by bisect
# ---------------------------------------------------------------------------

_thunk_cache: Dict[int, Optional[int]] = {}


def _read_va(va: int, size: int) -> bytes:
    for i, (vaddr, vsize, _raw_addr, raw_size) in enumerate(cac._xbe_sections):
        if vaddr <= va < vaddr + min(vsize, raw_size):
            off = va - vaddr
            return cac._xbe_bytes_cache[i][off: off + size]
    return b""


def _resolve_thunk(target: int) -> Optional[int]:
    """Follow up to MAX_THUNK_HOPS direct JMPs; return a known assert target
    VA or None."""
    if target in _thunk_cache:
        return _thunk_cache[target]
    resolved: Optional[int] = None
    cur = target
    for _ in range(MAX_THUNK_HOPS + 1):
        if cur in TARGET_NAMES:
            resolved = cur
            break
        raw = _read_va(cur, 5)
        if len(raw) == 5 and raw[0] == 0xE9:   # jmp rel32
            rel = int.from_bytes(raw[1:5], "little", signed=True)
            cur = cur + 5 + rel
        else:
            break
    _thunk_cache[target] = resolved
    return resolved


CACHE_PATH = REPO_ROOT / "artifacts" / "audit" / "assert_target_counts.json"


def _cache_key(funcs: List[KbFunc]) -> str:
    import hashlib
    st = cac.XBE_PATH.stat()
    h = hashlib.sha1(",".join(hex(f.addr) for f in funcs).encode()).hexdigest()
    return "%d:%d:%s" % (st.st_size, int(st.st_mtime), h[:16])


def _binary_counts(funcs: List[KbFunc]) -> Dict[int, Dict[str, int]]:
    """addr -> {"system_exit": n, "halt_and_catch_fire": n} for every kb
    function containing at least one direct CALL to either target.
    Cached on disk: the pristine XBE never changes, so the full-section
    decode (~30s) happens once per kb function-list change."""
    key = _cache_key(funcs)
    try:
        with open(CACHE_PATH) as f:
            cached = json.load(f)
        if cached.get("key") == key:
            return {int(a): c for a, c in cached["counts"].items()}
    except (OSError, ValueError, KeyError):
        pass
    counts = _binary_counts_uncached(funcs)
    try:
        CACHE_PATH.parent.mkdir(parents=True, exist_ok=True)
        with open(CACHE_PATH, "w") as f:
            json.dump({"key": key,
                       "counts": {str(a): c for a, c in counts.items()}}, f)
    except OSError:
        pass
    return counts


def _binary_counts_uncached(funcs: List[KbFunc]) -> Dict[int, Dict[str, int]]:
    cac._load_xbe()
    addrs = [f.addr for f in funcs]
    counts: Dict[int, Dict[str, int]] = {}
    cs = cac._get_cs()
    for i, (vaddr, _vsize, _raw, _rawsz) in enumerate(cac._xbe_sections):
        code = cac._xbe_bytes_cache[i]
        for insn in cs.disasm(code, vaddr):
            if insn.mnemonic != "call":
                continue
            try:
                target = int(insn.op_str, 16)
            except ValueError:
                continue
            resolved = _resolve_thunk(target)
            if resolved is None:
                continue
            pos = bisect.bisect_right(addrs, insn.address) - 1
            if pos < 0:
                continue
            owner = funcs[pos]
            # Only attribute when the CALL plausibly sits inside the owner's
            # body: before the next kb function.
            if pos + 1 < len(funcs) and insn.address >= funcs[pos + 1].addr:
                continue
            counts.setdefault(owner.addr, {"system_exit": 0,
                                           "halt_and_catch_fire": 0})
            counts[owner.addr][TARGET_NAMES[resolved]] += 1
    return counts


# ---------------------------------------------------------------------------
# Source side
# ---------------------------------------------------------------------------

def _blank_comments_and_strings(text: str) -> str:
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == "/" and i + 1 < n and text[i + 1] == "*":
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append(" " * (j - i))
            i = j
        elif c == "/" and i + 1 < n and text[i + 1] == "/":
            j = text.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
        elif c in "\"'":
            q = c
            j = i + 1
            while j < n and text[j] != q:
                j += 2 if text[j] == "\\" else 1
            j = min(j + 1, n)
            out.append(q + " " * (j - i - 2) + (q if j - i >= 2 else ""))
            i = j
        else:
            out.append(c)
            i += 1
    return "".join(out)


def _function_body(blanked: str, name: str) -> Optional[str]:
    """Extract the definition body of `name` from comment/string-blanked
    source text (definition line starts at column 0 in this codebase)."""
    for m in re.finditer(r"(?m)^[A-Za-z_][^\n=;]*\b%s\s*\(" % re.escape(name),
                         blanked):
        brace = blanked.find("{", m.end())
        semi = blanked.find(";", m.end())
        if brace < 0 or (0 <= semi < brace):
            continue   # prototype, not a definition
        depth, j = 0, brace
        while j < len(blanked):
            if blanked[j] == "{":
                depth += 1
            elif blanked[j] == "}":
                depth -= 1
                if depth == 0:
                    return blanked[brace:j + 1]
            j += 1
    return None


_src_cache: Dict[str, Optional[str]] = {}
_def_index: Optional[Dict[str, List[str]]] = None


def _blanked_file(path: str) -> Optional[str]:
    if path not in _src_cache:
        try:
            with open(path, "r", errors="replace") as f:
                _src_cache[path] = _blank_comments_and_strings(f.read())
        except OSError:
            _src_cache[path] = None
    return _src_cache[path]


def _build_def_index() -> Dict[str, List[str]]:
    """name -> candidate .c files where a column-0 `name(` line appears.
    Prototypes are included; _function_body() rejects them later."""
    global _def_index
    if _def_index is not None:
        return _def_index
    _def_index = {}
    for p in sorted((REPO_ROOT / "src").rglob("*.c")):
        blanked = _blanked_file(str(p))
        if blanked is None:
            continue
        for m in re.finditer(
                r"(?m)^[A-Za-z_][^\n=;{}]*\b([A-Za-z_][A-Za-z0-9_]*)\s*\(",
                blanked):
            _def_index.setdefault(m.group(1), []).append(str(p))
    return _def_index


def _candidate_paths(func: KbFunc) -> List[str]:
    candidates: List[str] = []
    if func.source_path:
        # Older kb.json entries store source_path without the src/ (or
        # src/halo/) prefix.
        rel = func.source_path
        if not (REPO_ROOT / rel).exists():
            for prefix in ("src/", "src/halo/"):
                if (REPO_ROOT / (prefix + func.source_path)).exists():
                    rel = prefix + func.source_path
                    break
        candidates.append(str(REPO_ROOT / rel))
    # Fallback for null/stale source_path: definition index over src/**/*.c.
    candidates += [p for p in _build_def_index().get(func.name, [])
                   if p not in candidates]
    return candidates


def _source_counts(func: KbFunc) -> Optional[Dict[str, int]]:
    body = None
    for path in _candidate_paths(func):
        blanked = _blanked_file(path)
        if blanked is None:
            continue
        body = _function_body(blanked, func.name)
        if body is not None:
            break
    if body is None:
        return None
    # assert_halt/assert_halt_msg (src/common.h, src/xdk_common.h) expand to
    # display_assert(...) + system_exit(-1) — count each as one system_exit.
    return {
        "system_exit":
            len(re.findall(r"\bsystem_exit\s*\(", body)) +
            len(re.findall(r"\bassert_halt(?:_msg)?\s*\(", body)),
        "halt_and_catch_fire":
            len(re.findall(r"\bhalt_and_catch_fire\s*\(", body)),
    }


# ---------------------------------------------------------------------------
# Comparison
# ---------------------------------------------------------------------------

class Finding(NamedTuple):
    level: str
    func: KbFunc
    bin_counts: Dict[str, int]
    src_counts: Dict[str, int]


def _compare(func: KbFunc, b: Dict[str, int],
             s: Dict[str, int]) -> Optional[Finding]:
    if b == s:
        return None
    swap = ((b["system_exit"] > 0 and s["halt_and_catch_fire"] > b["halt_and_catch_fire"]) or
            (b["halt_and_catch_fire"] > 0 and s["system_exit"] > b["system_exit"]))
    if swap:
        return Finding("ERROR", func, b, s)
    # Same-kind drift: source > binary is normal MSVC cross-jump sharing of
    # one assert tail (multiple checks JMP into a single PUSH -1; CALL block)
    # -> INFO.  Source < binary means the lift may have DROPPED an assert
    # -> WARN.
    dropped = (s["system_exit"] < b["system_exit"] or
               s["halt_and_catch_fire"] < b["halt_and_catch_fire"])
    return Finding("WARN" if dropped else "INFO", func, b, s)


def _staged_c_files() -> set:
    import subprocess
    out = subprocess.run(
        ["git", "diff", "--cached", "--name-only", "--diff-filter=ACMR"],
        cwd=str(REPO_ROOT), capture_output=True, text=True).stdout
    return {str(REPO_ROOT / line) for line in out.splitlines()
            if line.endswith(".c")}


def run(target: Optional[str] = None,
        staged_only: bool = False) -> List[Finding]:
    funcs = _load_kb_funcs()
    candidates = [f for f in funcs if f.ported]
    if staged_only:
        staged = _staged_c_files()
        if not staged:
            return []
        candidates = [f for f in candidates
                      if any(p in staged for p in _candidate_paths(f))]
        if not candidates:
            return []
    if target:
        if target.startswith("0x"):
            want = int(target, 16)
            candidates = [f for f in candidates if f.addr == want]
        else:
            candidates = [f for f in candidates if f.name == target]
        if not candidates:
            print("target not found among ported functions: %s" % target)
            return []
    bin_counts = _binary_counts(funcs)
    findings: List[Finding] = []
    zero = {"system_exit": 0, "halt_and_catch_fire": 0}
    for f in candidates:
        b = bin_counts.get(f.addr, zero)
        if b == zero and not target:
            continue   # fast path: nothing to verify in this function
        s = _source_counts(f)
        if s is None:
            # Body not found at source_path: only report when the binary side
            # has assert calls to verify (stale source_path or renamed def).
            if b != zero:
                findings.append(Finding("WARN", f, b,
                                        {"body": "no source definition found"}))
            continue
        fnd = _compare(f, b, s)
        if fnd:
            findings.append(fnd)
        elif target:
            print("OK %s @0x%x  binary=%r source=%r" % (f.name, f.addr, b, s))
    return findings


def self_test() -> int:
    """Decoder regression against known assert tails fixed in
    rasterizer_decals (2026-07-11): binary-side counts must match what the
    session's manual capstone verification established."""
    cac._load_xbe()
    funcs = _load_kb_funcs()
    counts = _binary_counts(funcs)
    expected = {
        # Capstone-verified body counts (call 0x8e2f0 sites at 0x158e15,
        # 0x158e3e, 0x158f57 / 0x1595xx x2).
        0x158DF0: {"system_exit": 3, "halt_and_catch_fire": 0},
        0x1595C0: {"system_exit": 2, "halt_and_catch_fire": 0},
    }
    ok = True
    zero = {"system_exit": 0, "halt_and_catch_fire": 0}
    for addr, want in expected.items():
        got = counts.get(addr, zero)
        status = "ok" if got == want else "FAIL"
        if got != want:
            ok = False
        print("self-test 0x%x: want=%r got=%r %s" % (addr, want, got, status))
    return 0 if ok else 1


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[1])
    ap.add_argument("--check", action="store_true",
                    help="exit non-zero if any ERROR finding")
    ap.add_argument("--target", help="single function name or 0x-address")
    ap.add_argument("--staged-only", action="store_true",
                    help="only check functions defined in git-staged .c files")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()

    if args.self_test:
        return self_test()

    if not cac.XBE_PATH.exists():
        print("pristine XBE not found (%s) — skipping assert-target audit"
              % cac.XBE_PATH)
        return 0

    findings = run(args.target, staged_only=args.staged_only)
    errors = 0
    for f in findings:
        if f.level == "ERROR":
            errors += 1
        print("[%s] %s @0x%x (%s): binary calls %s, source calls %s" % (
            f.level, f.func.name, f.func.addr, f.func.source_path,
            f.bin_counts, f.src_counts))
    if not findings:
        print("assert-target audit clean")
    else:
        print("%d finding(s), %d ERROR" % (len(findings), errors))
    return 1 if (args.check and errors) else 0


if __name__ == "__main__":
    sys.exit(main())
