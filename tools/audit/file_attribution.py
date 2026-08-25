#!/usr/bin/env python3
"""
file_attribution.py - recover per-function source-file attribution from the
pristine XBE's assert __FILE__ strings, and audit kb.json object naming
against it.

The debug build stamps `c:\\halo\\SOURCE\\<dir>\\<file>.c` literals into
.rdata and pushes them at every assert site.  A function that references
exactly one such *.c* literal was compiled from that translation unit, which
makes this a binary-proven attribution rather than a naming convention.

Header literals are excluded from that rule: a .h is never a compiland, so an
assert inside a header-resident inline names the inline's home, not the
function's TU.  46 functions reference only a header and are left for
bracketing to resolve.

Attribution comes in two tiers and they are NOT equal evidence:

  proven   - the function's own assert names the TU (26.0% of kb functions)
  inferred - the function lies between two evidenced functions naming the
             same TU, and MSVC emits each TU as one contiguous run (52.8%)

The contiguity premise is checked on every run: all 295 observed TUs occupy
exactly one maximal run each.  Bracketing is refused if any TU splits.  The
blind spot is a TU with no asserts anywhere - invisible to the check and
silently absorbed by a neighbour.  Chao1 on the evidence distribution puts
the true TU count near 327, so roughly 30 TUs are unseen.

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
import struct
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

# XBE section flags: bit 2 = executable.  Only executable sections are
# disassembled for immediates; scanning .rdata/.data as code decodes pointer
# table words as instructions and manufactures references (bug: two .rdata
# words at 0x253383/0x253736 decoded as je/jo immediates).
XBE_SECTION_EXECUTABLE = 0x00000004


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


def _executable_sections() -> Set[int]:
    """Indices of sections marked executable in the XBE section headers."""
    cac._load_xbe()
    with open(cac.XBE_PATH, "rb") as f:
        data = f.read()
    count = struct.unpack_from("<I", data, 0x11C)[0]
    sh_va = struct.unpack_from("<I", data, 0x120)[0]
    base = struct.unpack_from("<I", data, 0x104)[0]
    off0 = sh_va - base
    out = set()
    for i in range(count):
        flags = struct.unpack_from("<I", data, off0 + i * 56)[0]
        if flags & XBE_SECTION_EXECUTABLE:
            out.add(i)
    return out


def _scan_immediate_refs(string_vas: Set[int]) -> List[Ref]:
    """Every instruction immediate that equals a source-literal VA."""
    cac._load_xbe()
    executable = _executable_sections()
    cs = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    cs.detail = True
    cs.skipdata = True

    refs: List[Ref] = []
    for idx, (vaddr, _vsize, _raw_addr, raw_size) in enumerate(cac._xbe_sections):
        if idx not in executable or not raw_size:
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
    """Return (starts, lookup) where lookup(va) -> addr or None.

    1124 of 9133 kb functions have no function_bounds.json entry.  The old
    fallback (next start, capped at +0x4000) let the LAST function of a section
    swallow the following section: 0x252df4 ends its XPP section at 0x253064
    but claimed through 0x253df4, absorbing .rdata pointer words as if they
    were its own code.  Both references it captured that way were wrong.  Clamp
    every fallback to the end of the function's own section.
    """
    cac._load_xbe()
    starts = sorted(funcs)
    sec_ends = sorted((va, va + max(vsize, raw))
                      for va, vsize, _ra, raw in cac._xbe_sections)

    def section_end(va: int) -> Optional[int]:
        for lo, hi in sec_ends:
            if lo <= va < hi:
                return hi
        return None

    def lookup(va: int) -> Optional[int]:
        i = bisect.bisect_right(starts, va) - 1
        if i < 0:
            return None
        addr = starts[i]
        end = funcs[addr].end
        if end == 0:
            nxt = starts[i + 1] if i + 1 < len(starts) else addr + 0x1000
            end = min(nxt, addr + 0x4000)
            limit = section_end(addr)
            if limit is not None:
                end = min(end, limit)
        # A reference must also live in the same section as its owner.
        if section_end(addr) != section_end(va):
            return None
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
# Layout bracketing
# ---------------------------------------------------------------------------
#
# Only a function that contains an assert pushes a __FILE__ literal, so direct
# evidence tops out near 27% of the image and no amount of further lifting
# raises it -- the 305 literals are all the binary will ever hold.
#
# MSVC emits each translation unit as one contiguous run in .text, so an
# unevidenced function lying strictly between two evidenced functions that
# name the SAME TU belongs to that TU by layout.  That is reading the linker's
# own grouping, not extrapolating.
#
# The model is falsifiable and is checked on every run: if TU layout were not
# contiguous, evidenced functions would interleave (A B A) in address order.
# _layout_violations() counts those.  Bracketing is refused if the count
# exceeds MAX_LAYOUT_VIOLATIONS.

# A TU that occupies more than one maximal run falsifies contiguous layout.
# The old A-B-A test only saw interruptions of length exactly one: it could
# fire at most 49 times in this image against a null expectation of 19, and
# missed A-BB-A entirely.  Run count per TU sees every break.
MAX_SPLIT_TUS = 0


def _is_header(name: str) -> bool:
    return name.lower().endswith((".h", ".inl"))


def direct_tu(fn_files: Dict[int, Counter], addr: int) -> Optional[str]:
    """The single TU a function's own asserts name, or None if 0 or 2+.

    Header literals are excluded.  A .h is never a translation unit: an assert
    inside a header-resident inline is stamped into whichever .c included it,
    so `foo.h` names the inline's home, not the compiland.  Treating a header
    as a TU produced the one apparent layout violation in the whole image --
    hs_evaluate_wake stamping hs_library_internal_runtime.h between two
    hs_runtime.c functions -- which is inlining, not a discontiguous TU.
    Header-only functions fall through to bracketing, which recovers the
    including .c from the surrounding run.
    """
    counts = fn_files.get(addr)
    if not counts:
        return None
    tus = {_basename(f) for f in counts if not _is_header(_basename(f))}
    if len(tus) != 1:
        return None
    return next(iter(tus))


def _tu_runs(labels: List[Optional[str]]) -> Dict[str, int]:
    """Maximal contiguous run count per TU, over evidenced positions only."""
    runs: Dict[str, int] = Counter()
    prev = None
    for t in labels:
        if t is None:
            continue
        if t != prev:
            runs[t] += 1
        prev = t
    return dict(runs)


def _split_tus(labels: List[Optional[str]]) -> Dict[str, int]:
    """TUs occupying more than one run.  Empty dict means layout is contiguous."""
    return {t: n for t, n in _tu_runs(labels).items() if n > 1}


class Bracketing(NamedTuple):
    addrs: List[int]                    # kb functions in address order
    direct: List[Optional[str]]         # TU from the function's own asserts
    filled: List[Optional[str]]         # direct + bracketed
    split_tus: Dict[str, int]           # TU -> run count, for TUs with >1 run
    n_tus: int
    refused: bool


def bracket_layout(fn_files: Dict[int, Counter],
                   funcs: Dict[int, FuncInfo]) -> Bracketing:
    addrs = sorted(funcs)
    direct = [direct_tu(fn_files, a) for a in addrs]
    runs = _tu_runs(direct)
    split = _split_tus(direct)
    refused = len(split) > MAX_SPLIT_TUS

    filled = list(direct)
    if not refused:
        marked = [i for i, t in enumerate(direct) if t]
        for lo, hi in zip(marked, marked[1:]):
            if direct[lo] == direct[hi]:
                for k in range(lo + 1, hi):
                    filled[k] = direct[lo]
    return Bracketing(addrs, direct, filled, split, len(runs), refused)


def print_bracket(fn_files, funcs, br: Bracketing, limit: int) -> None:
    total = len(br.addrs)
    n_direct = sum(1 for t in br.direct if t)
    n_filled = sum(1 for t in br.filled if t)
    print("=== layout bracketing ===")
    print("contiguity: %d TUs occupy %d maximal runs; %d TU(s) split across runs"
          % (br.n_tus, br.n_tus + sum(n - 1 for n in br.split_tus.values()),
             len(br.split_tus)))
    if br.refused:
        print("REFUSED: TU layout is not contiguous, so bracketing is unsafe.")
    for tu, n in sorted(br.split_tus.items(), key=lambda kv: -kv[1]):
        print("   split TU: %-44s %d runs" % (tu, n))
    print()
    print("direct __FILE__ push (proven)   %5d  %.1f%%"
          % (n_direct, 100.0 * n_direct / total))
    print("+ bracketed (layout-inferred)   %5d  %.1f%%"
          % (n_filled, 100.0 * n_filled / total))
    print()
    print("NOTE: the two tiers are not equal evidence.  Only functions that")
    print("contain an assert push a __FILE__ literal, so a translation unit")
    print("with no asserts is invisible to this method AND is silently")
    print("absorbed by bracketing.  A Chao1 estimate on the per-TU evidence")
    print("distribution puts the true TU count near 327 against %d observed,"
          % br.n_tus)
    print("i.e. roughly 30 assert-free TUs that bracketing cannot see.")
    print()
    gaps = _gap_sizes(br)
    if gaps:
        big = [g for g in gaps if g >= 10]
        print("bracketed gaps: %d gaps, %d functions; %d gaps of >=10 fill %d"
              % (len(gaps), sum(gaps), len(big), sum(big)))
        print("largest gap fills %d functions" % max(gaps))
        print()
    gained = Counter(br.filled[i] for i in range(total)
                     if br.filled[i] and not br.direct[i])
    print("functions gained by bracketing, by TU:")
    for name, n in gained.most_common(limit):
        print("   %-48s +%d" % (name, n))


def _gap_sizes(br: Bracketing) -> List[int]:
    marked = [i for i, t in enumerate(br.direct) if t]
    out = []
    for lo, hi in zip(marked, marked[1:]):
        if br.direct[lo] == br.direct[hi] and hi - lo > 1:
            out.append(hi - lo - 1)
    return out


class ObjVerdict(NamedTuple):
    index: int                  # kb objects[] index; names are NOT unique
    name: str
    source: Optional[str]       # kb objects[].source, the TU kb already claims
    dominant: Optional[str]     # proven dominant TU after bracketing
    dominant_n: int
    labelled: int               # members carrying evidence (direct + bracketed)
    direct_n: int               # members whose OWN assert names a TU
    supports_source: int        # members whose OWN assert names `source`
    members: int
    distinct_tus: int
    collides_with: Optional[str]  # kb object already named for the proven TU
    verdict: str


def object_verdicts(funcs: Dict[int, FuncInfo], br: Bracketing,
                    kb_objects: List[dict],
                    min_share: float = 0.9,
                    min_direct: int = 3) -> List[ObjVerdict]:
    """Compare each kb object's own `source` against the proven dominant TU.

    Three rules keep this from manufacturing claims:

      * `objects[].source` is the claim under test, not the .obj name.  kb
        already records the TU for 190 of 232 objects in the same form this
        tool recovers.
      * Share is computed over members that CARRY evidence.  Dividing by all
        members scores absence of evidence as disagreement.
      * Direct evidence outranks bracketed evidence.  If ANY member's own
        assert names the recorded source, the object cannot contradict it --
        bracketing may legitimately pull neighbours from an adjacent TU into
        an object that really does contain some of `source`.  cinematics.obj
        was reported as contradicting `cinematics.c` even though
        cinematic_initialize stamps exactly that file.

    A contradiction also needs `min_direct` members whose OWN asserts name the
    replacement, so a verdict is never carried by bracketing alone.
    """
    pos = {a: i for i, a in enumerate(br.addrs)}
    # Objects already named for a TU: renaming into one of these would create
    # a duplicate name, and batch_delink writes both to the same path -- the
    # smaller object exports last and destroys the larger delinked reference.
    by_stem: Dict[str, str] = {}
    for obj in kb_objects:
        nm = obj.get("name") or ""
        if nm:
            by_stem.setdefault(_stem(_basename(nm)), nm)
        if obj.get("source"):
            by_stem.setdefault(_stem(_basename(obj["source"])), nm or "<unnamed>")

    out: List[ObjVerdict] = []
    for idx, obj in enumerate(kb_objects):
        addrs = []
        for fn in obj.get("functions") or []:
            try:
                addrs.append(int(fn["addr"], 16))
            except (KeyError, ValueError, TypeError):
                continue
        if not addrs:
            continue
        labels, directs = [], []
        for a in addrs:
            i = pos.get(a)
            if i is None:
                continue
            if br.filled[i]:
                labels.append(br.filled[i])
            if br.direct[i]:
                directs.append(br.direct[i])
        counts = Counter(labels)
        direct_counts = Counter(directs)
        src = obj.get("source")
        src_stem = _stem(_basename(src)) if src else None
        supports = sum(n for t, n in direct_counts.items()
                       if src_stem and _stem(t) == src_stem)

        if not counts:
            verdict, dom, dom_n = "no-evidence", None, 0
        else:
            dom, dom_n = counts.most_common(1)[0]
            share = dom_n / len(labels)
            dom_direct = direct_counts.get(dom, 0)
            if src_stem is None:
                verdict = "no-source"
            elif _stem(dom) == src_stem:
                verdict = "confirms"
            elif supports:
                # kb's recorded source is directly attested by at least one
                # member; the object contains some of it, whatever else it has.
                verdict = "mixed"
            elif share >= min_share and len(counts) <= 2 and dom_direct >= min_direct:
                verdict = "contradicts"
            else:
                verdict = "mixed"
        collides = None
        if verdict == "contradicts" and dom:
            hit = by_stem.get(_stem(dom))
            if hit and hit != obj.get("name"):
                collides = hit
        out.append(ObjVerdict(idx, obj.get("name") or "<unnamed>", src, dom,
                              dom_n, len(labels), len(directs), supports,
                              len(addrs), len(counts), collides, verdict))
    rank = {"contradicts": 0, "mixed": 1, "no-source": 2,
            "confirms": 3, "no-evidence": 4}
    out.sort(key=lambda v: (rank[v.verdict], -v.dominant_n))
    return out


def print_objects_verdict(funcs, br: Bracketing, kb_objects, limit: int) -> None:
    vs = object_verdicts(funcs, br, kb_objects)
    tally = Counter(v.verdict for v in vs)
    print("=== kb objects[].source vs proven TU ===")
    print("objects scored: %d" % len(vs))
    for k in ("confirms", "contradicts", "mixed", "no-source", "no-evidence"):
        print("   %-12s %d" % (k, tally[k]))
    print()
    print("CONTRADICTS - no member's own assert names the recorded source, and")
    print(">=3 members' own asserts name one replacement covering >=90% of the")
    print("evidence.  A rename is NOT the remedy where a collision is flagged:")
    print("kb object names must stay unique or batch_delink writes two objects")
    print("to one path and the smaller destroys the larger delinked reference.")
    print()
    print("   %-26s %-28s %-26s %-14s %s"
          % ("object", "kb .source", "proven TU", "direct/evid", "collision"))
    for v in vs:
        if v.verdict != "contradicts":
            continue
        print("   %-26s %-28s %-26s %2d/%-11d %s"
              % (v.name, v.source, v.dominant, v.direct_n, v.labelled,
                 ("MERGE into %s" % v.collides_with) if v.collides_with
                 else "none - name is free"))
    print()
    print("MIXED - evidence spans several TUs, or the recorded source is")
    print("directly attested by some members.  No single .source is right, and")
    print("no rename is warranted:")
    n = 0
    for v in vs:
        if v.verdict != "mixed":
            continue
        n += 1
        if n > limit:
            continue
        print("   %-30s %-26s dominant %-24s %d/%d of %d (%d TUs, %d support .source)"
              % (v.name, v.source or "-", v.dominant, v.dominant_n,
                 v.labelled, v.members, v.distinct_tus, v.supports_source))
    if n > limit:
        print("   ... %d more (use --report)" % (n - limit))


def print_summary(fn_files, funcs, br: Bracketing, kb_objects, limit: int) -> None:
    total = len(br.addrs)
    n_direct = sum(1 for t in br.direct if t)
    n_filled = sum(1 for t in br.filled if t)
    print("=== __FILE__ attribution ===")
    print("kb functions                    %5d" % total)
    print("proven by own assert            %5d  %.1f%%"
          % (n_direct, 100.0 * n_direct / total))
    print("+ layout-inferred (bracketed)   %5d  %.1f%%"
          % (n_filled, 100.0 * n_filled / total))
    print("translation units observed      %5d   (%d split across runs)"
          % (br.n_tus, len(br.split_tus)))
    unported = sum(1 for i, a in enumerate(br.addrs)
                   if br.filled[i] and a in funcs and not funcs[a].ported)
    print("attributed but still unported   %5d" % unported)
    print()
    print_objects_verdict(funcs, br, kb_objects, limit)


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


def write_json(path: Path, fn_files, literals, funcs) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    functions = {}
    for addr, files in sorted(fn_files.items()):
        info = funcs.get(addr)
        # Use the same header-filtered determination the bracket lane uses, so
        # the JSON and the reports cannot disagree about what a TU is.
        tu = direct_tu(fn_files, addr)
        full = next((f for f in files if _basename(f) == tu), None) \
            if tu else None
        functions["0x%x" % addr] = {
            "name": info.name if info else None,
            "kb_object": info.obj if info else None,
            "kb_source_path": info.kb_source_path if info else None,
            "ported": info.ported if info else None,
            "binary_source_path": full,
            "binary_source_file": _basename(full) if full else None,
            "determines_tu": tu is not None,
            "header_only": tu is None and bool(files),
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

    br = bracket_layout(fn_files, funcs)
    if br.refused:
        failures.append("bracketing refused: %d split TUs" % len(br.split_tus))
    n_direct = sum(1 for t in br.direct if t)
    n_filled = sum(1 for t in br.filled if t)
    if n_filled <= n_direct:
        failures.append("bracketing gained nothing")
    if br.split_tus:
        failures.append("TUs split across runs: %s" % sorted(br.split_tus))
    # The contiguity test must catch a break the old A-B-A test could not.
    if _split_tus(["x.c", "y.c", "y.c", "x.c"]) != {"x.c": 2}:
        failures.append("_split_tus missed an A-BB-A break")
    if _split_tus(["x.c", None, "x.c", "y.c"]) != {}:
        failures.append("_split_tus reported a false break across a gap")
    if _split_tus(["x.c", "y.c", "x.c"]) != {"x.c": 2}:
        failures.append("_split_tus missed an A-B-A break")
    # object_verdicts must score share over EVIDENCED members, not all members,
    # or an object with sparse but unanimous evidence reads as a multi-TU bucket.
    kb_objects = json.loads(KB_PATH.read_text(encoding="utf-8"))["objects"]
    verdicts = {v.name: v for v in object_verdicts(funcs, br, kb_objects)}
    players = verdicts.get("players.obj")
    if players is None:
        failures.append("players.obj missing from verdicts")
    elif players.verdict != "confirms":
        failures.append("players.obj scored %s; its evidence is unanimous "
                        "players.c and it must confirm" % players.verdict)
    ai = verdicts.get("ai_profile.obj")
    if ai is None or ai.verdict != "contradicts":
        failures.append("ai_profile.obj must contradict its recorded source")

    # Regression guard for the section-clamped owner index: a literal in
    # .rdata must never be attributed to the last function of a code section.
    _lits = _scan_source_literals()
    _starts, _owner = _owner_index(funcs)
    for _va in _lits:
        if _owner(_va) is not None:
            failures.append("a source literal at 0x%x was attributed to a "
                            "function; literals live in data" % _va)
            break

    for f in failures:
        print("FAIL: %s" % f)
    print("self-test: %s (%d literals, %d direct, %d after bracketing, "
          "%d TUs, %d split)"
          % ("FAIL" if failures else "ok", len(literals), n_direct, n_filled,
             br.n_tus, len(br.split_tus)))
    return 1 if failures else 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--report", action="store_true",
                    help="print every object disagreement, not just the top ones")
    ap.add_argument("--unported", action="store_true",
                    help="list unported functions grouped by proven translation unit")
    ap.add_argument("--bracket", action="store_true",
                    help="fill unevidenced functions bracketed by an agreeing TU")
    ap.add_argument("--objects", action="store_true",
                    help="rename vs split verdict per kb object, after bracketing")
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

    if args.bracket:
        print_bracket(fn_files, funcs, bracket_layout(fn_files, funcs),
                      10 ** 9 if args.report else args.limit)
    elif args.objects:
        kb_objects = json.loads(KB_PATH.read_text(encoding="utf-8"))["objects"]
        print_objects_verdict(funcs, bracket_layout(fn_files, funcs),
                              kb_objects,
                              10 ** 9 if args.report else args.limit)
    elif args.object:
        print_object(args.object, fn_files, funcs)
    elif args.source_path:
        print_source_paths(fn_files, funcs,
                           10 ** 9 if args.report else args.limit)
    elif args.unported:
        print_unported(fn_files, funcs, 10 ** 9 if args.report else args.limit)
    else:
        kb_objects = json.loads(KB_PATH.read_text(encoding="utf-8"))["objects"]
        print_summary(fn_files, funcs, bracket_layout(fn_files, funcs),
                      kb_objects, 10 ** 9 if args.report else args.limit)

    if args.json:
        write_json(Path(args.json), fn_files, literals, funcs)
    return 0


if __name__ == "__main__":
    sys.exit(main())
