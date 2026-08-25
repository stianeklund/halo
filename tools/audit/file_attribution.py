#!/usr/bin/env python3
"""
file_attribution.py - recover per-function source-file attribution from the
pristine XBE's assert __FILE__ strings, and audit kb.json object naming
against it.

The debug build stamps `c:\\halo\\SOURCE\\<dir>\\<file>.c` literals into
.rdata and pushes them at every assert site.  A function that references
exactly one such literal was compiled from that translation unit, which makes
this a binary-proven attribution rather than a naming convention.

This is the cheap static counterpart to audit_object_provenance.py: that tool
proves attribution one address range at a time through the Ghidra delinker,
while this one sweeps the whole image in a single capstone pass and needs no
disassembler service.

Two questions are answered:

  1. Which translation unit did each function come from?  Emitted for every
     function with at least one __FILE__ reference.
  2. Where does kb.json's object grouping disagree with that evidence?
     A kb object named foo.obj whose members all stamp bar.c is a
     misnamed or merged TU; a mixed spread is usually cross-TU inlining and
     is reported at lower confidence.

Usage:
  python3 tools/audit/file_attribution.py                 # summary + top disagreements
  python3 tools/audit/file_attribution.py --report        # full disagreement table
  python3 tools/audit/file_attribution.py --unported      # lift queue grouped by TU
  python3 tools/audit/file_attribution.py --object units.obj
  python3 tools/audit/file_attribution.py --json artifacts/audit/file_attribution.json
  python3 tools/audit/file_attribution.py --self-test
"""

import argparse
import bisect
import json
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path
from typing import Dict, List, NamedTuple, Optional, Set, Tuple

sys.path.insert(0, str(Path(__file__).resolve().parent))
import check_arg_counts as cac   # XBE loader + capstone (self-contained there)

import capstone

REPO_ROOT = Path(__file__).resolve().parents[2]
KB_PATH = REPO_ROOT / "kb.json"
BOUNDS_PATH = REPO_ROOT / "tools" / "verify" / "function_bounds.json"
DEFAULT_JSON = REPO_ROOT / "artifacts" / "audit" / "file_attribution.json"

# A source literal looks like  c:\halo\SOURCE\ai\actors.c
SOURCE_RE = re.compile(rb"[A-Za-z]:\\[\x20-\x7e]{3,190}?\.(?:c|h|cpp|inl)\x00", re.IGNORECASE)

# Sections that hold code we scan for immediates.  Data-only sections still get
# scanned for the literals themselves.
CODE_SECTIONS_MIN_SIZE = 16


class Ref(NamedTuple):
    site_va: int      # VA of the instruction carrying the immediate
    string_va: int


class FuncInfo(NamedTuple):
    addr: int
    end: int
    name: str
    obj: Optional[str]
    ported: bool
    kb_source_path: Optional[str]


# ---------------------------------------------------------------------------
# XBE scanning
# ---------------------------------------------------------------------------

def _scan_source_literals() -> Dict[int, str]:
    """Return {string_va: literal} for every NUL-terminated source path."""
    cac._load_xbe()
    out: Dict[int, str] = {}
    for idx, (vaddr, _vsize, _raw_addr, _raw_size) in enumerate(cac._xbe_sections):
        blob = cac._xbe_bytes_cache[idx]
        for m in SOURCE_RE.finditer(blob):
            # The drive-letter prefix is the anchor: a literal cannot start
            # mid-path because these paths contain no second `X:\`.  Do not
            # require a preceding NUL - the linker packs some literals
            # directly against float constant data.
            text = m.group(0)[:-1].decode("ascii", "replace")
            out[vaddr + m.start()] = text
    return out


def _scan_immediate_refs(string_vas: Set[int]) -> List[Ref]:
    """Every instruction immediate that equals a source-literal VA."""
    cac._load_xbe()
    cs = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    cs.detail = True
    cs.skipdata = True

    refs: List[Ref] = []
    for idx, (vaddr, _vsize, _raw_addr, raw_size) in enumerate(cac._xbe_sections):
        if raw_size < CODE_SECTIONS_MIN_SIZE:
            continue
        blob = cac._xbe_bytes_cache[idx]
        for insn in cs.disasm(blob, vaddr):
            if insn.id == 0:      # SKIPDATA pseudo-instruction: no operand detail
                continue
            for op in insn.operands:
                if op.type == capstone.x86.X86_OP_IMM:
                    val = op.imm & 0xFFFFFFFF
                    if val in string_vas:
                        refs.append(Ref(insn.address, val))
                elif op.type == capstone.x86.X86_OP_MEM and op.mem.base == 0 \
                        and op.mem.index == 0:
                    val = op.mem.disp & 0xFFFFFFFF
                    if val in string_vas:
                        refs.append(Ref(insn.address, val))
    return refs


# ---------------------------------------------------------------------------
# kb.json / bounds
# ---------------------------------------------------------------------------

_NAME_RE = re.compile(r"(?P<name>\*?[A-Za-z_]\w*)\s*\(")


def _name_from_decl(decl: str) -> str:
    m = _NAME_RE.search(decl or "")
    return m.group("name").lstrip("*") if m else "?"


def _load_functions() -> Dict[int, FuncInfo]:
    kb = json.loads(KB_PATH.read_text(encoding="utf-8"))
    bounds = json.loads(BOUNDS_PATH.read_text(encoding="utf-8"))

    ends: Dict[int, int] = {}
    for key, val in bounds.items():
        if key == "_meta":
            continue
        try:
            ends[int(key, 16)] = int(val["end"], 16)
        except (ValueError, KeyError, TypeError):
            continue

    funcs: Dict[int, FuncInfo] = {}
    for obj in kb.get("objects", []):
        obj_name = obj.get("name")
        for fn in obj.get("functions") or []:
            addr_s = fn.get("addr")
            if not addr_s:
                continue
            try:
                addr = int(addr_s, 16)
            except ValueError:
                continue
            funcs[addr] = FuncInfo(
                addr=addr,
                end=ends.get(addr, 0),
                name=fn.get("name") or _name_from_decl(fn.get("decl", "")),
                obj=obj_name,
                ported=bool(fn.get("ported")),
                kb_source_path=fn.get("source_path"),
            )
    return funcs


def _owner_index(funcs: Dict[int, FuncInfo]):
    """Return (starts, lookup) where lookup(va) -> addr or None."""
    starts = sorted(funcs)

    def lookup(va: int) -> Optional[int]:
        i = bisect.bisect_right(starts, va) - 1
        if i < 0:
            return None
        addr = starts[i]
        end = funcs[addr].end
        # A zero end means the bounds table lacks the entry; fall back to the
        # next function start so the reference is still attributed, but only
        # within a sane distance.
        if end == 0:
            nxt = starts[i + 1] if i + 1 < len(starts) else addr + 0x1000
            end = min(nxt, addr + 0x4000)
        return addr if va < end else None

    return starts, lookup


# ---------------------------------------------------------------------------
# Attribution
# ---------------------------------------------------------------------------

def _basename(literal: str) -> str:
    return literal.replace("/", "\\").rsplit("\\", 1)[-1]


def _stem(name: str) -> str:
    return name.rsplit(".", 1)[0].lower()


def build_attribution() -> Tuple[Dict[int, Counter], Dict[int, str], Dict[int, FuncInfo]]:
    literals = _scan_source_literals()
    refs = _scan_immediate_refs(set(literals))
    funcs = _load_functions()
    _starts, owner = _owner_index(funcs)

    fn_files: Dict[int, Counter] = defaultdict(Counter)
    for ref in refs:
        addr = owner(ref.site_va)
        if addr is None:
            continue
        fn_files[addr][literals[ref.string_va]] += 1
    return fn_files, literals, funcs


# ---------------------------------------------------------------------------
# Reporting
# ---------------------------------------------------------------------------

class Disagreement(NamedTuple):
    obj: str
    agree: int
    disagree: int
    total_members: int
    dominant: str
    dominant_count: int
    distinct_foreign: int
    kind: str
    confidence: str
    addrs: List[int]


def _classify(agree: int, disagree: int, distinct_foreign: int,
              dominant_count: int, total_members: int) -> Tuple[str, str]:
    """Return (kind, confidence) for one object's disagreement pattern.

    The load-bearing fact is whether *any* member stamps the kb object's own
    name.  If none does, the name is not a translation unit the compiler ever
    saw; whether the fix is a rename or a split then depends on how many
    distinct foreign TUs the members span.

      misnamed - nothing supports the name and one or two TUs explain it:
                 rename the object.
      split    - nothing supports the name and members span 3+ TUs: the object
                 is an address-range bucket, not a TU; split it.
      merged   - the name is supported, but a block of >=5 members belongs to
                 one other TU: two TUs merged; split that block off.
      inlining - foreign references are thin and spread out, which is what
                 cross-TU inlining and header-resident inlines look like.
      noise    - a catch-all bucket with a handful of strays; no action.
    """
    if disagree == 0:
        return "clean", "clean"
    share = dominant_count / disagree
    # A catch-all bucket with only a trace of disagreement is not a finding.
    if total_members >= 20 and disagree / total_members < 0.05:
        return "noise", "low"
    if agree == 0:
        if distinct_foreign >= 3:
            return "split", "high" if disagree >= 10 else "medium"
        return "misnamed", "high" if dominant_count >= 3 else "medium"
    if dominant_count >= 5 and share >= 0.5:
        return "merged", "high" if dominant_count >= 10 else "medium"
    if distinct_foreign >= 4 and share < 0.5:
        return "inlining", "low"
    return ("merged", "medium") if dominant_count >= 3 else ("inlining", "low")


def collect_disagreements(fn_files: Dict[int, Counter],
                          funcs: Dict[int, FuncInfo]) -> List[Disagreement]:
    per_obj_agree: Dict[str, int] = Counter()
    per_obj_bad: Dict[str, List[Tuple[int, str]]] = defaultdict(list)
    per_obj_total: Dict[str, int] = Counter()

    for addr, info in funcs.items():
        if not info.obj:
            continue
        per_obj_total[info.obj] += 1
        files = fn_files.get(addr)
        if not files:
            continue
        stem = _stem(_basename(info.obj))
        member_files = {_basename(f) for f in files}
        if any(_stem(f) == stem for f in member_files):
            per_obj_agree[info.obj] += 1
        else:
            # Attribute the function to its most-referenced foreign file.
            top = files.most_common(1)[0][0]
            per_obj_bad[info.obj].append((addr, _basename(top)))

    out: List[Disagreement] = []
    for obj, bad in per_obj_bad.items():
        counts = Counter(f for _a, f in bad)
        dominant, dom_count = counts.most_common(1)[0]
        kind, conf = _classify(per_obj_agree.get(obj, 0), len(bad),
                               len(counts), dom_count, per_obj_total[obj])
        out.append(Disagreement(
            obj=obj,
            agree=per_obj_agree.get(obj, 0),
            disagree=len(bad),
            total_members=per_obj_total[obj],
            dominant=dominant,
            dominant_count=dom_count,
            distinct_foreign=len(counts),
            kind=kind,
            confidence=conf,
            addrs=sorted(a for a, _f in bad),
        ))
    krank = {"split": 0, "misnamed": 1, "merged": 2, "inlining": 3, "noise": 4}
    crank = {"high": 0, "medium": 1, "low": 2, "clean": 3}
    out.sort(key=lambda d: (krank[d.kind], crank[d.confidence],
                            -d.dominant_count, d.obj))
    return out


def print_summary(fn_files, funcs, disagreements, limit: int) -> None:
    evidenced = len(fn_files)
    unambiguous = sum(1 for c in fn_files.values() if len(c) == 1)
    unported = [a for a in fn_files if a in funcs and not funcs[a].ported]
    print("=== __FILE__ attribution ===")
    print("kb functions:                 %d" % len(funcs))
    print("with __FILE__ evidence:       %d" % evidenced)
    print("  unambiguous (1 distinct):   %d" % unambiguous)
    print("  still unported:             %d" % len(unported))
    print()
    by_kind = Counter(d.kind for d in disagreements)
    print("objects with disagreements:   %d  (%s)"
          % (len(disagreements),
             " ".join("%s=%d" % (k, by_kind[k])
                      for k in ("split", "misnamed", "merged", "inlining", "noise")
                      if by_kind[k])))
    print()
    print("%-42s %4s %4s %4s  %-9s %-7s %s"
          % ("kb object", "ok", "bad", "mem", "kind", "conf",
             "dominant foreign __FILE__"))
    for d in disagreements[:limit]:
        if d.kind in ("inlining", "noise"):
            continue
        print("%-42s %4d %4d %4d  %-9s %-7s %s (%d/%d, %d TUs)"
              % (d.obj, d.agree, d.disagree, d.total_members, d.kind,
                 d.confidence, d.dominant, d.dominant_count, d.disagree,
                 d.distinct_foreign))
    if len(disagreements) > limit:
        print("... %d more (use --report)" % (len(disagreements) - limit))


def print_object(obj_name: str, fn_files, funcs) -> None:
    members = sorted((a for a, i in funcs.items() if i.obj == obj_name))
    if not members:
        print("object not found: %s" % obj_name)
        return
    print("=== %s (%d functions) ===" % (obj_name, len(members)))
    for addr in members:
        info = funcs[addr]
        files = fn_files.get(addr)
        evidence = ", ".join("%s x%d" % (_basename(f), n)
                             for f, n in files.most_common()) if files else "-"
        print("  0x%06x %-46s %-8s %s"
              % (addr, info.name, "ported" if info.ported else "unported", evidence))


def print_unported(fn_files, funcs, limit: int) -> None:
    by_file: Dict[str, List[int]] = defaultdict(list)
    for addr, files in fn_files.items():
        info = funcs.get(addr)
        if info is None or info.ported or len(files) != 1:
            continue
        by_file[_basename(next(iter(files)))].append(addr)
    print("=== unported functions with unambiguous __FILE__ evidence ===")
    total = sum(len(v) for v in by_file.values())
    print("%d functions across %d translation units" % (total, len(by_file)))
    print()
    for name, addrs in sorted(by_file.items(), key=lambda kv: -len(kv[1]))[:limit]:
        print("%-52s %3d  %s" % (name, len(addrs),
                                 " ".join("0x%06x" % a for a in sorted(addrs)[:6])
                                 + (" ..." if len(addrs) > 6 else "")))


def print_source_paths(fn_files, funcs, limit: int) -> None:
    """Compare kb.json's repo-side source_path against the binary's own TU.

    source_path records where the lift *lives* in this repo, so a mismatch is
    not a defect in either field - it means a lifted function was filed under
    a .c file other than the translation unit it was compiled from.  Those are
    the candidates for moving a function to the right file during readability
    work.
    """
    rows = []
    for addr, info in funcs.items():
        if not info.kb_source_path or addr not in fn_files:
            continue
        kb_stem = _stem(_basename(info.kb_source_path.replace("\\", "/")
                                  .rsplit("/", 1)[-1]))
        files = {_stem(_basename(f)) for f in fn_files[addr]}
        if kb_stem not in files:
            rows.append((addr, info, sorted(files)))
    print("=== kb source_path vs binary translation unit ===")
    total = sum(1 for a, i in funcs.items()
                if i.kb_source_path and a in fn_files)
    print("%d of %d functions with both fields are filed under a foreign TU"
          % (len(rows), total))
    print()
    by_pair = defaultdict(list)
    for addr, info, files in rows:
        by_pair[(info.kb_source_path, files[0] if files else "?")].append(addr)
    for (repo, orig), addrs in sorted(by_pair.items(), key=lambda kv: -len(kv[1]))[:limit]:
        print("%-44s <- %-32s %3d  %s"
              % (repo, orig + ".c", len(addrs),
                 " ".join("0x%06x" % a for a in sorted(addrs)[:5])
                 + (" ..." if len(addrs) > 5 else "")))


def write_json(path: Path, fn_files, literals, funcs, disagreements) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    functions = {}
    for addr, files in sorted(fn_files.items()):
        info = funcs.get(addr)
        full = files.most_common(1)[0][0]
        functions["0x%x" % addr] = {
            "name": info.name if info else None,
            "kb_object": info.obj if info else None,
            "kb_source_path": info.kb_source_path if info else None,
            "ported": info.ported if info else None,
            "binary_source_path": full,
            "binary_source_file": _basename(full),
            "unambiguous": len(files) == 1,
            "refs": dict(sorted(((_basename(f), n) for f, n in files.items()),
                                key=lambda kv: -kv[1])),
        }
    payload = {
        "_meta": {
            "version": 1,
            "xbe": str(cac.XBE_PATH.relative_to(REPO_ROOT)),
            "source_literals": len(literals),
            "functions_with_evidence": len(fn_files),
        },
        "functions": functions,
        "object_disagreements": [d._asdict() | {"addrs": ["0x%x" % a for a in d.addrs]}
                                 for d in disagreements],
    }
    path.write_text(json.dumps(payload, indent=1), encoding="utf-8")
    print("wrote %s" % path.relative_to(REPO_ROOT))


# ---------------------------------------------------------------------------

def _self_test() -> int:
    literals = _scan_source_literals()
    failures = []
    if len(literals) < 200:
        failures.append("expected >=200 source literals, found %d" % len(literals))
    if not any(v.lower().endswith("action_alert.c") for v in literals.values()):
        failures.append("known literal action_alert.c not recovered")
    if not all(re.match(r"^[A-Za-z]:\\", v) for v in literals.values()):
        failures.append("a literal does not start with a drive prefix")

    # 0x12000 is the first .text function and asserts in ai/action_alert.c.
    fn_files, _lit, funcs = build_attribution()
    got = fn_files.get(0x12000)
    if not got:
        failures.append("0x12000 has no __FILE__ evidence")
    elif _basename(got.most_common(1)[0][0]).lower() != "action_alert.c":
        failures.append("0x12000 attributed to %s, expected action_alert.c"
                        % got.most_common(1)[0][0])

    if _classify(0, 39, 6, 12, 65)[0] != "split":
        failures.append("_classify missed a multi-TU bucket")
    if _classify(0, 10, 1, 10, 46)[0] != "misnamed":
        failures.append("_classify missed a misnamed object")
    if _classify(38, 28, 1, 28, 66)[0] != "merged":
        failures.append("_classify missed a merged TU pair")
    if _classify(0, 1, 1, 1, 1721)[0] != "noise":
        failures.append("_classify flagged a catch-all bucket stray")

    for f in failures:
        print("FAIL: %s" % f)
    print("self-test: %s (%d literals, %d attributed functions)"
          % ("FAIL" if failures else "ok", len(literals), len(fn_files)))
    return 1 if failures else 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--report", action="store_true",
                    help="print every object disagreement, not just the top ones")
    ap.add_argument("--unported", action="store_true",
                    help="list unported functions grouped by proven translation unit")
    ap.add_argument("--source-path", action="store_true",
                    help="list lifted functions filed under a foreign .c file")
    ap.add_argument("--object", metavar="NAME",
                    help="show per-function evidence for one kb object")
    ap.add_argument("--json", nargs="?", const=str(DEFAULT_JSON), metavar="PATH",
                    help="write the full attribution table as JSON")
    ap.add_argument("--limit", type=int, default=25,
                    help="rows to print in summary mode (default 25)")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()

    if args.self_test:
        return _self_test()

    fn_files, literals, funcs = build_attribution()
    disagreements = collect_disagreements(fn_files, funcs)

    if args.object:
        print_object(args.object, fn_files, funcs)
    elif args.source_path:
        print_source_paths(fn_files, funcs,
                           10 ** 9 if args.report else args.limit)
    elif args.unported:
        print_unported(fn_files, funcs, 10 ** 9 if args.report else args.limit)
    else:
        print_summary(fn_files, funcs, disagreements,
                      10 ** 9 if args.report else args.limit)

    if args.json:
        write_json(Path(args.json), fn_files, literals, funcs, disagreements)
    return 0


if __name__ == "__main__":
    sys.exit(main())
