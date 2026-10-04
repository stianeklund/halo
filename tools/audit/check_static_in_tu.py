#!/usr/bin/env python3
"""check_static_in_tu.py -- prove every kb.json ``"static_in_tu": true`` flag.

The flag tells the VC7.1 verifier compile (tools/verify/vc71_verify.py
``compile_vc71``, shared by the mnemonic scorer and the raw-XBE audit) to give
the function its original file-static linkage: ``static __declspec(noinline)``
on the definition and no decl.h prototype.  VC7.1 /O2 then gives it the same
private register convention and frameless body the original has
(docs/lift-learnings.md 32a).  Production ignores the flag: the clang build,
decl.h, thunks and patch.py are unchanged, so the lift stays an external,
patch-redirected function.

A wrong flag makes the verifier score a compile the original could not have
produced, so each one must hold, against the pristine XBE and the source:

  * ``ported`` is true and the kb object names a source file;
  * that file has exactly one top-level definition of the function;
  * the function is CALLed (E8) or tail-jumped (E9) at least once, and every
    such site lies inside a function of the SAME kb object (TU);
  * no dword in any XBE section equals the address (address never taken);
  * in source, every use of the name is a direct call that FOLLOWS the
    definition, and no other file under src/ names it (no forward declaration
    needed, no caller outside the TU, no address taken).

``--candidates`` evaluates every ported ``@<reg>`` function the same way and
prints the blockers, as JSON with ``--json``.

Usage:
  python3 tools/audit/check_static_in_tu.py                # report flags
  python3 tools/audit/check_static_in_tu.py --check        # exit 1 on a bad flag
  python3 tools/audit/check_static_in_tu.py --candidates --json out.json
"""

import argparse
import bisect
import json
import re
import struct
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT / "tools" / "verify"))
sys.path.insert(0, str(REPO_ROOT / "tools"))

from vc71_verify import _static_defs_in  # noqa: E402

XBE = REPO_ROOT / "halo-patched" / "cachebeta.xbe"
BOUNDS = REPO_ROOT / "tools" / "verify" / "function_bounds.json"


def strip_comments(text: str) -> str:
    """Blank comments and string literals, keeping offsets and newlines."""
    def blank(m):
        return re.sub(r"[^\n]", " ", m.group(0))
    return re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'',
                  blank, text, flags=re.S)


def function_name(decl: str):
    m = re.search(r"\b([A-Za-z_]\w*)\s*\(", decl or "")
    return m.group(1) if m else None


def source_problems(name: str, def_text: str, other_texts) -> list:
    """Source-side blockers for making ``name`` static in ``def_text``.

    ``other_texts`` is an iterable of (path, text) for every other src file."""
    problems = []
    if name not in _static_defs_in(def_text, {name}):
        return ["no top-level definition in its source"]
    code = strip_comments(def_text)
    defs = [m for m in re.finditer(
        rf"(?m)^[A-Za-z_][\w \t\*]*[ \t\*]{re.escape(name)}\s*\(", code)]
    def_pos = None
    for m in defs:
        depth, i = 1, m.end()
        while i < len(code) and depth:
            depth += {"(": 1, ")": -1}.get(code[i], 0)
            i += 1
        if code[i:].lstrip().startswith("{"):
            if def_pos is not None:
                problems.append("more than one definition")
            def_pos = m.start()
    decl_starts = {m.start() + m.group(0).rfind(name) for m in defs}
    for m in re.finditer(rf"\b{re.escape(name)}\b", code):
        if m.start() in decl_starts:
            continue
        line = code.count("\n", 0, m.start()) + 1
        if not re.match(r"\s*\(", code[m.end():]):
            problems.append(f"address taken in source (line {line})")
        elif def_pos is not None and m.start() < def_pos:
            problems.append(f"called before its definition (line {line})")
    for path, text in other_texts:
        if re.search(rf"\b{re.escape(name)}\b", strip_comments(text)):
            problems.append(f"named outside its TU: {path}")
    return problems


def xbe_sections(data: bytes):
    base = struct.unpack_from("<I", data, 0x104)[0]
    count = struct.unpack_from("<I", data, 0x11C)[0]
    hdr = struct.unpack_from("<I", data, 0x120)[0] - base
    out = []
    for i in range(count):
        flags, va, _vs, raw, size = struct.unpack_from("<5I", data, hdr + i * 0x38)
        out.append((va, bool(flags & 4), data[raw:raw + size]))
    return out


def binary_refs(sections, targets):
    """{target: {"calls": [(site, is_jmp)], "dwords": [va]}} for every target."""
    targets = set(targets)
    refs = {t: {"calls": [], "dwords": []} for t in targets}
    for va, executable, blob in sections:
        for t in targets:
            packed = struct.pack("<I", t)
            i = blob.find(packed)
            while i != -1:
                refs[t]["dwords"].append(va + i)
                i = blob.find(packed, i + 1)
        if not executable:
            continue
        for j in range(len(blob) - 4):
            if blob[j] in (0xE8, 0xE9):
                dest = (va + j + 5 + struct.unpack_from("<i", blob, j + 1)[0]) & 0xFFFFFFFF
                if dest in targets:
                    refs[dest]["calls"].append((va + j, blob[j] == 0xE9))
    return refs


def load_bounds():
    raw = json.loads(BOUNDS.read_text())
    starts = sorted(int(k, 16) for k in raw if k.startswith("0x"))
    ends = {int(k, 16): int(v["end"], 16) for k, v in raw.items() if k.startswith("0x")}
    return starts, ends


def containing(starts, ends, addr):
    i = bisect.bisect_right(starts, addr) - 1
    if i >= 0 and starts[i] <= addr < ends[starts[i]]:
        return starts[i]
    return None


def resolve_source(rel):
    if not rel:
        return None
    for base in (REPO_ROOT / "src" / "halo", REPO_ROOT / "src"):
        p = base / rel
        if p.is_file():
            return p
    return None


def evaluate(kb, entries, sources_cache=None):
    """Blockers for each (obj, fn) entry; returns list of result dicts."""
    by_addr = {}
    for obj in kb["objects"]:
        for fn in obj.get("functions", []):
            if fn.get("addr"):
                by_addr[int(fn["addr"], 16)] = (obj, fn)
    starts, ends = load_bounds()
    sections = xbe_sections(XBE.read_bytes())
    refs = binary_refs(sections, [int(fn["addr"], 16) for _o, fn in entries])
    texts = sources_cache if sources_cache is not None else {
        p: p.read_text(errors="replace")
        for p in (REPO_ROOT / "src").rglob("*") if p.suffix in (".c", ".h")}
    results = []
    for obj, fn in entries:
        addr = int(fn["addr"], 16)
        name = function_name(fn["decl"])
        problems = []
        if fn.get("ported") is not True:
            problems.append("not ported")
        src = resolve_source(obj.get("source"))
        if src is None:
            problems.append("kb object has no resolvable source")
        r = refs[addr]
        if r["dwords"]:
            problems.append("address taken in XBE at " +
                            ", ".join(hex(v) for v in r["dwords"][:4]))
        if not r["calls"]:
            problems.append("no direct call sites in XBE")
        callers = set()
        for site, _jmp in r["calls"]:
            c = containing(starts, ends, site)
            if c is None or c not in by_addr:
                problems.append(f"call site {site:#x} not in a kb function")
                continue
            callers.add(c)
            cobj, _cfn = by_addr[c]
            if cobj["name"] != obj["name"]:
                problems.append(f"caller {c:#x} in {cobj['name']}")
        if src is not None and name:
            others = ((str(p.relative_to(REPO_ROOT)), t) for p, t in texts.items()
                      if p != src and name in t)
            problems += source_problems(name, texts.get(src) or src.read_text(), others)
        results.append({
            "name": name, "addr": hex(addr), "object": obj["name"],
            "source": obj.get("source"),
            "callers": sorted(hex(c) for c in callers),
            "unported_callers": sorted(hex(c) for c in callers
                                       if by_addr[c][1].get("ported") is not True),
            "call_sites": len(r["calls"]),
            "problems": sorted(set(problems)),
        })
    return results


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--check", action="store_true", help="exit 1 on any bad flag")
    ap.add_argument("--candidates", action="store_true",
                    help="evaluate every unflagged ported @<reg> function")
    ap.add_argument("--json", type=Path, help="write results as JSON")
    ap.add_argument("--kb", type=Path, default=REPO_ROOT / "kb.json")
    args = ap.parse_args(argv)

    kb = json.loads(args.kb.read_text())
    pick = ((lambda f: "@<" in f.get("decl", "") and f.get("ported") is True
             and f.get("static_in_tu") is not True)
            if args.candidates else (lambda f: f.get("static_in_tu") is True))
    entries = [(o, f) for o in kb["objects"] for f in o.get("functions", [])
               if f.get("addr") and pick(f)]
    results = evaluate(kb, entries)
    if args.json:
        args.json.write_text(json.dumps(results, indent=1) + "\n")
    bad = [r for r in results if r["problems"]]
    if not args.candidates:
        for r in results:
            status = "FAIL" if r["problems"] else "ok"
            print(f"static_in_tu {status}: {r['name']} {r['addr']} "
                  f"({len(r['callers'])} callers, {r['call_sites']} sites)")
            for p in r["problems"]:
                print(f"    {p}")
        print(f"static_in_tu: {len(results)} flagged, {len(bad)} invalid")
    else:
        print(f"candidates: {len(results)} ported @<reg>, "
              f"{len(results) - len(bad)} without blockers")
    return 1 if (args.check and not args.candidates and bad) else 0


if __name__ == "__main__":
    sys.exit(main())
