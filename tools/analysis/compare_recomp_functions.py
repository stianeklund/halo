#!/usr/bin/env python3
"""Compare xboxrecomp function discovery against kb.json.

xboxrecomp's disasm inventory is larger than ours because it scans XDK
sections (D3D, DSOUND, ...). Its func_id/identified_functions.json is *not*
a function list: it stamps ~5k vtable-slot addresses as size-32 "thunks",
many mid-instruction. This tool diffs the real disasm list only, and
refuses the vtable file.

Usage:
    rtk python3 tools/analysis/compare_recomp_functions.py
    rtk python3 tools/analysis/compare_recomp_functions.py --import-xdk
    rtk python3 tools/analysis/compare_recomp_functions.py --import-xdk --apply
    rtk python3 tools/analysis/compare_recomp_functions.py \\
        --import-xdk --sections all --min-size 1 --include-call-targets --apply
"""
from __future__ import annotations

import argparse
import json
import sys
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
KB_JSON = ROOT / "kb.json"
BOUNDS_JSON = ROOT / "tools" / "verify" / "function_bounds.json"

DEFAULT_THEIRS = (
    Path("/mnt/g/dev/xboxrecomp/halo_debug_recomp/analysis/disasm/functions.json"),
)

XDK_SECTIONS = frozenset({
    "D3D", "D3DX", "DSOUND", "XNET", "XPP",
    "BINK", "BINK32", "BINK32A", "BINK16", "BINK4444", "BINK5551",
    "BINK16MX", "BINK16X2", "BINK16M", "BINK32MX", "BINK32X2", "BINK32M",
})
DEFAULT_IMPORT_SECTIONS = ("D3D", "DSOUND")
VTABLE_METHODS = frozenset({"vtable_thunk", "vtable_scan", "vtable_ctor"})
XDK_STUBS = "<xdk_stubs>"


class VtableInventoryError(ValueError):
    """Raised when the input is func_id/identified_functions.json."""


def _is_vtable_record(rec: dict) -> bool:
    method = rec.get("method")
    if method in VTABLE_METHODS:
        return True
    if "vtable_addr" in rec or "vtable_index" in rec:
        return True
    if rec.get("category") in ("game_vtable",):
        return True
    return False


def load_theirs(path: Path) -> list[dict]:
    """Load xboxrecomp disasm/functions.json. Refuses the vtable ID dump."""
    with path.open() as f:
        data = json.load(f)
    if isinstance(data, dict) and isinstance(data.get("functions"), list):
        data = data["functions"]
    if not isinstance(data, list) or not data:
        raise ValueError("%s: expected a non-empty list of function records" % path)
    vtable_hits = sum(1 for rec in data if isinstance(rec, dict) and _is_vtable_record(rec))
    if vtable_hits:
        raise VtableInventoryError(
            "%s looks like func_id/identified_functions.json (%d vtable records). "
            "That file is ~5k false-positive vtable-slot addresses, not functions. "
            "Pass analysis/disasm/functions.json instead."
            % (path, vtable_hits)
        )
    if "start" not in data[0] and "detection_method" not in data[0]:
        raise ValueError("%s: unrecognized function-list shape (need 'start')" % path)
    return data


def load_ours(kb: dict) -> dict[int, dict]:
    """addr -> {obj, decl} from kb.json objects plus stray top-level entries."""
    ours: dict[int, dict] = {}
    for key, val in kb.items():
        if key in ("md5", "objects") or not isinstance(val, dict):
            continue
        addr_s = val.get("addr") or key
        try:
            addr = int(addr_s, 16)
        except (TypeError, ValueError):
            continue
        ours[addr] = {
            "obj": val.get("obj"),
            "decl": val.get("decl"),
        }
    for obj in kb.get("objects") or []:
        oname = obj.get("name")
        for fn in obj.get("functions") or []:
            addr_s = fn.get("addr")
            if not addr_s:
                continue
            ours[int(addr_s, 16)] = {
                "obj": oname,
                "decl": fn.get("decl"),
            }
    return ours


def load_bounds(path: Path) -> dict[int, int]:
    """addr -> end (exclusive) from function_bounds.json, if present."""
    if not path.is_file():
        return {}
    with path.open() as f:
        data = json.load(f)
    out: dict[int, int] = {}
    for key, val in data.items():
        if key == "_meta" or not isinstance(val, dict):
            continue
        end = val.get("end")
        if not end:
            continue
        out[int(key, 16)] = int(end, 16)
    return out


def covering(addr: int, ours_sorted: list[int], bounds: dict[int, int]) -> int | None:
    """Return our function start that contains addr, or None."""
    import bisect
    i = bisect.bisect_right(ours_sorted, addr) - 1
    if i < 0:
        return None
    start = ours_sorted[i]
    end = bounds.get(start)
    if end is not None and start <= addr < end and start != addr:
        return start
    return None


def section_of(rec: dict) -> str:
    return rec.get("section") or "other"


# False splits / padding, never catalogued as functions.
_SKIP_METHODS = frozenset({"seed_vtable_thunk"})
# Real bodies even without `push ebp; mov ebp, esp` (fastcall, naked, leaf).
_BODY_METHODS = frozenset({"prologue", "call_target", "entry_point"})


def xdk_candidates(theirs: list[dict], ours: dict[int, dict],
                   sections: set[str], min_size: int,
                   require_prologue: bool = True) -> list[dict]:
    """XDK functions we do not already catalog.

    Default keeps the conservative prologue-only filter. ``require_prologue=False``
    also takes call-target bodies (no frame, still a real entry). Never takes
    ``seed_vtable_thunk`` mid-function splits or ``.text`` (caller must omit it
    from ``sections``).
    """
    out = []
    for rec in theirs:
        addr = int(rec["start"], 16)
        if addr in ours:
            continue
        if section_of(rec) not in sections:
            continue
        method = rec.get("detection_method")
        if method in _SKIP_METHODS:
            continue
        if require_prologue:
            if not rec.get("has_prologue"):
                continue
        elif method not in _BODY_METHODS and not rec.get("has_prologue"):
            continue
        if (rec.get("size") or 0) < min_size:
            continue
        out.append(rec)
    out.sort(key=lambda r: int(r["start"], 16))
    return out


def stub_decl(addr: int) -> str:
    return "void FUN_%08x(void);" % addr


def apply_import(kb: dict, candidates: list[dict]) -> dict:
    """Append candidates to the last <xdk_stubs> object. Mutates kb."""
    stubs = None
    for obj in kb.get("objects") or []:
        if obj.get("name") == XDK_STUBS:
            stubs = obj
    if stubs is None:
        stubs = {"name": XDK_STUBS, "functions": []}
        kb.setdefault("objects", []).append(stubs)
    existing = {int(fn["addr"], 16) for fn in stubs.get("functions") or []}
    added = 0
    skipped = 0
    funcs = stubs.setdefault("functions", [])
    for rec in candidates:
        addr = int(rec["start"], 16)
        if addr in existing:
            skipped += 1
            continue
        funcs.append({
            "addr": "0x%x" % addr,
            "decl": stub_decl(addr),
        })
        existing.add(addr)
        added += 1
    return {"added": added, "skipped": skipped, "object": XDK_STUBS}


def save_kb(path: Path, kb: dict) -> None:
    with path.open("w") as f:
        json.dump(kb, f, indent=1)
        f.write("\n")


def compare(theirs: list[dict], ours: dict[int, dict],
            bounds: dict[int, int]) -> dict:
    ours_set = set(ours)
    ours_sorted = sorted(ours_set)
    theirs_by_addr = {int(r["start"], 16): r for r in theirs}
    theirs_set = set(theirs_by_addr)

    only_them = sorted(theirs_set - ours_set)
    only_us = sorted(ours_set - theirs_set)
    both = theirs_set & ours_set

    by_section = defaultdict(lambda: Counter())
    for rec in theirs:
        sec = section_of(rec)
        addr = int(rec["start"], 16)
        by_section[sec]["theirs"] += 1
        if addr in ours_set:
            by_section[sec]["both"] += 1
        else:
            by_section[sec]["only_them"] += 1
            if rec.get("has_prologue"):
                by_section[sec]["only_them_prologue"] += 1
    for addr in ours_set:
        rec = theirs_by_addr.get(addr)
        sec = section_of(rec) if rec else (
            "D3D" if 0x1E69E0 <= addr < 0x1FE8E0 else
            "D3DX" if 0x1FE8E0 <= addr < 0x203600 else
            "DSOUND" if 0x203600 <= addr < 0x222DA0 else
            "XNET" if 0x222DA0 <= addr < 0x22DE60 else
            "BINK" if 0x22DE60 <= addr < 0x24B200 else
            "XPP" if 0x24B200 <= addr < 0x253000 else
            ".text"
        )
        by_section[sec]["ours"] += 1
        if addr not in theirs_set:
            by_section[sec]["only_us"] += 1

    text_only_them = [a for a in only_them
                      if section_of(theirs_by_addr[a]) == ".text"]
    text_interior = [a for a in text_only_them if covering(a, ours_sorted, bounds)]
    text_gaps = [a for a in text_only_them if a not in set(text_interior)]
    text_gap_prologue = sum(
        1 for a in text_gaps if theirs_by_addr[a].get("has_prologue")
    )

    return {
        "ours": len(ours_set),
        "theirs": len(theirs_set),
        "both": len(both),
        "only_us": len(only_us),
        "only_them": len(only_them),
        "by_section": {k: dict(v) for k, v in sorted(by_section.items())},
        "text_only_them": len(text_only_them),
        "text_interior": len(text_interior),
        "text_gaps": len(text_gaps),
        "text_gap_prologue": text_gap_prologue,
    }


def format_report(summary: dict, candidates: list[dict] | None,
                  stats: dict | None, dry_run: bool) -> str:
    lines = []
    lines.append("Recomp function inventory vs kb.json")
    lines.append("  ours %d  theirs(disasm) %d  both %d  only-us %d  only-them %d"
                 % (summary["ours"], summary["theirs"], summary["both"],
                    summary["only_us"], summary["only_them"]))
    lines.append("  .text only-them %d (interior splits %d, gaps %d, gap prologues %d)"
                 % (summary["text_only_them"], summary["text_interior"],
                    summary["text_gaps"], summary["text_gap_prologue"]))
    lines.append("")
    lines.append("  %-10s %6s %6s %6s %9s %10s %8s"
                 % ("section", "ours", "theirs", "both", "only-us", "only-them", "prolog"))
    for sec, row in summary["by_section"].items():
        lines.append("  %-10s %6d %6d %6d %9d %10d %8d"
                     % (sec, row.get("ours", 0), row.get("theirs", 0),
                        row.get("both", 0), row.get("only_us", 0),
                        row.get("only_them", 0), row.get("only_them_prologue", 0)))
    if candidates is not None:
        lines.append("")
        action = "Would add" if dry_run else "Added"
        lines.append("  XDK prologue import: %d candidates"
                     % len(candidates))
        by_sec = Counter(section_of(r) for r in candidates)
        for sec in sorted(by_sec):
            lines.append("    %-10s %d" % (sec, by_sec[sec]))
        if stats:
            lines.append("  %s %d to %s (skipped %d)"
                         % (action, stats["added"], stats["object"], stats["skipped"]))
        if dry_run and candidates:
            lines.append("  NOTE: pass --apply to write kb.json")
        show = candidates[:12]
        if show:
            lines.append("  sample:")
            for rec in show:
                addr = int(rec["start"], 16)
                lines.append("    0x%08x  %-8s size=%-4d  %s"
                             % (addr, section_of(rec), rec.get("size") or 0,
                                stub_decl(addr)))
    return "\n".join(lines)


def default_theirs_path() -> Path | None:
    for path in DEFAULT_THEIRS:
        if path.is_file():
            return path
    return None


def parse_sections(arg: str) -> set[str]:
    if arg.strip().lower() == "all":
        return set(XDK_SECTIONS)
    out = {p.strip() for p in arg.split(",") if p.strip()}
    unknown = out - XDK_SECTIONS
    if unknown:
        raise SystemExit("unknown section(s) %s; known: %s"
                         % (",".join(sorted(unknown)), ",".join(sorted(XDK_SECTIONS))))
    return out


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(
        description="Compare xboxrecomp disasm functions against kb.json")
    ap.add_argument("--theirs", type=Path, default=None,
                    help="Path to disasm/functions.json")
    ap.add_argument("--kb", type=Path, default=KB_JSON)
    ap.add_argument("--bounds", type=Path, default=BOUNDS_JSON)
    ap.add_argument("--import-xdk", action="store_true",
                    help="Select prologue XDK functions missing from kb.json")
    ap.add_argument("--apply", action="store_true",
                    help="Write selected XDK functions into <xdk_stubs> (implies --import-xdk)")
    ap.add_argument("--sections", default=",".join(DEFAULT_IMPORT_SECTIONS),
                    help="Comma list of XDK sections to import, or 'all' (default D3D,DSOUND)")
    ap.add_argument("--min-size", type=int, default=16,
                    help="Minimum function size for XDK import (default 16)")
    ap.add_argument("--include-call-targets", action="store_true",
                    help="Also import XDK call-target bodies that lack a frame prologue")
    ap.add_argument("--json", action="store_true", dest="json_out",
                    help="Machine-readable JSON instead of the text report")
    args = ap.parse_args(argv)

    theirs_path = args.theirs or default_theirs_path()
    if theirs_path is None:
        print("error: no disasm/functions.json found; pass --theirs", file=sys.stderr)
        return 2

    try:
        theirs = load_theirs(theirs_path)
    except VtableInventoryError as exc:
        print("error: %s" % exc, file=sys.stderr)
        return 1

    with args.kb.open() as f:
        kb = json.load(f)
    ours = load_ours(kb)
    bounds = load_bounds(args.bounds)
    summary = compare(theirs, ours, bounds)

    do_import = args.import_xdk or args.apply
    candidates = None
    stats = None
    if do_import:
        sections = parse_sections(args.sections)
        candidates = xdk_candidates(
            theirs, ours, sections, args.min_size,
            require_prologue=not args.include_call_targets,
        )
        if args.apply:
            stats = apply_import(kb, candidates)
            save_kb(args.kb, kb)

    if args.json_out:
        payload = dict(summary)
        payload["theirs_path"] = str(theirs_path)
        if candidates is not None:
            payload["import_candidates"] = [
                {"addr": "0x%x" % int(r["start"], 16),
                 "section": section_of(r),
                 "size": r.get("size"),
                 "decl": stub_decl(int(r["start"], 16))}
                for r in candidates
            ]
        if stats is not None:
            payload["import_stats"] = stats
        print(json.dumps(payload, indent=2))
    else:
        print(format_report(summary, candidates, stats, dry_run=not args.apply))
    return 0


if __name__ == "__main__":
    sys.exit(main())
