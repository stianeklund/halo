#!/usr/bin/env python3
"""Generate/check `function_bounds.json` -- the committed authority for spans.

WHY THIS EXISTS
---------------
`vc71_verify._func_span()` derives a function's length from kb.json alone: the
distance to the next *listed* function start.  kb.json is not a complete
listing of the binary, so wherever the listing has a hole that distance
overshoots -- by a lot.  Measured: FUN_0015c2d0 is 102 bytes, its kb gap is
800, because its real neighbour at 0x15c340 was never listed.  The verifier
then rejects that function's CORRECT reference as "truncated", and any
reference wide enough to satisfy the span is a bloated one that scores the
function against several hundred bytes of somebody else's code.

The fix is not a better heuristic, it is a table.  Bounds are a property of
the binary, they change only when kb.json gains a function, and they are worth
reviewing by eye -- so they are computed once, committed, and gated in CI
(`--check`) rather than recomputed silently inside the scorer.

THE RULE
--------
    end = min(capstone true_end, next kb.json function start)

Both halves are load-bearing (same reasoning as `xbe_reference.function_bytes`):

  * `true_end` alone OVER-RUNS on a tail-jump thunk.  0x18e300 is the 5 bytes
    `e9 db1c0000`; scanned with a wide window it treats the jump target as an
    outstanding branch and keeps going -- 7985 bytes for a one-instruction
    function.  Two things prevent that here: the scan window is the kb gap
    (see `true_end_offset`), and a leading unconditional `jmp` is classified
    as a `thunk` before the scan is consulted at all.
  * the kb bound alone OVER-RUNS across every listing hole (above).

Neither half sees data that the compiler emitted *after* the final `ret` --
a switch jump table lives in `.text` and belongs to the function, but decodes
as garbage.  Those are the `table_data` entries in `OVERRIDES`, which is also
the escape hatch for any future hand-verified bound.

Every emitted entry is validated at generation time: the bytes `[start, end)`
must decode cleanly from `start` to EXACTLY `end`, and the last instruction
must be a terminator or the `kind` must say why not.  An entry that would
slice mid-instruction is backed off to the last real instruction boundary and
flagged, never emitted silently.

Usage:
    function_bounds.py                      # regenerate + sanity report
    function_bounds.py --check              # CI gate: fail on drift
    function_bounds.py --addrs 0x18e300 ..  # inspect, do not write
"""
import argparse
import bisect
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
XBE = REPO_ROOT / "halo-patched" / "cachebeta.xbe"
KB = REPO_ROOT / "kb.json"
TABLE = Path(__file__).resolve().parent / "function_bounds.json"

sys.path.insert(0, str(REPO_ROOT / "tools" / "audit"))

# Bump when the generation RULE changes (not when kb.json grows), so a stale
# table is distinguishable from a merely out-of-date one.
VERSION = 1

# Bytes a linker leaves between functions.  `8b ff` (mov edi,edi) also shows up
# as 2-byte alignment inside MSVC output, but it decodes as a real instruction
# and is deliberately NOT treated as padding here -- see `_gap_is_padding`.
PAD_BYTES = frozenset((0x90, 0xCC))

# A gap this size or smaller between one function's end and the next listed
# start is alignment; anything larger is a listing hole (unlisted code).
ALIGN_GAP = 16

# Scan window for the LAST kb.json function, which has no successor to bound it.
MAX_SCAN = 0x4000

# Hand-verified bounds that no automatic rule can reach.  Keyed by entry
# address; `end` is exclusive.  Kept in the generator (not the table) so
# regeneration is deterministic and every override is reviewable in one place.
OVERRIDES = {
    0x84520: {
        "end": 0x8473A,
        "kind": "table_data",
        "note": "switch table after the final ret at 0x84711: 3 dword targets "
                "at 0x84714 (0x8469d/0x846a8/0x846b7) + 26-byte index table "
                "at 0x84720",
    },
    0x12A2D0: {
        "end": 0x12A4F4,
        "kind": "table_data",
        "note": "network_game_client_start_frame: switch table after the final "
                "ret at 0x12a4df -- 5 dword targets at 0x12a4e0 "
                "(0x12a3c2/0x12a3eb/0x12a414/0x12a43d/0x12a462)",
    },
    0x1A8E10: {
        "end": 0x1A8EE0,
        "kind": "table_data",
        "note": "unit_handle_weapon_state_change: true_end stopped at the "
                "default-case ret at 0x1a8e5d; remaining case bodies run to "
                "the ret at 0x1a8ebd, then 8 dword jump-table slots at "
                "0x1a8ec0 (0x1a8e72/0x1a8e85/0x1a8e4c/0x1a8e5f/0x1a8e24/"
                "0x1a8e38/0x1a8e98/0x1a8eab). Next function is 0x1a8ee0.",
    },
}

_MD = None


def _md():
    global _MD
    if _MD is None:
        from capstone import CS_ARCH_X86, CS_MODE_32, Cs
        _MD = Cs(CS_ARCH_X86, CS_MODE_32)
    return _MD


_XBE_CACHE = None


def _xbe():
    global _XBE_CACHE
    if _XBE_CACHE is None:
        from check_delinked_bounds import load_xbe
        _XBE_CACHE = load_xbe(str(XBE))
    return _XBE_CACHE


def verify_xbe() -> str:
    """Return the XBE md5 after checking it against kb.json's recorded hash.

    A bounds table generated from the wrong binary is worse than no table: the
    spans look plausible and are wrong everywhere.
    """
    if not XBE.exists():
        raise SystemExit("error: %s not found (run from repo root)" % XBE)
    got = hashlib.md5(XBE.read_bytes()).hexdigest()
    want = subprocess.run(["jq", "-r", ".md5", str(KB)],
                          capture_output=True, text=True).stdout.strip()
    if want and want != "null" and got != want:
        raise SystemExit("error: XBE md5 %s != kb.json md5 %s -- refusing to "
                         "generate bounds from the wrong binary" % (got, want))
    return got


def _decl_name(decl: str) -> str | None:
    """Function name from a kb.json decl (the linkable symbol, not `.name`).

    kb.json's optional `name` field carries aspirational/documentation names
    that disagree with the decl for 5 functions; the decl is what the build and
    the scorer use, so that is what the table records.
    """
    m = re.search(r"\b(\w+)\s*\(", decl or "")
    return m.group(1) if m else None


def load_kb_functions() -> tuple[list[tuple[int, str]], int]:
    """[(addr, name)] sorted by address, deduped, plus the duplicate count."""
    out = subprocess.run(
        ["jq", "-r", '.objects[].functions[] | "\\(.addr)\\t\\(.decl)"', str(KB)],
        capture_output=True, text=True)
    if out.returncode != 0:
        raise SystemExit("error: jq failed on %s: %s" % (KB, out.stderr.strip()))
    seen: dict[int, str] = {}
    dups = 0
    for line in out.stdout.splitlines():
        addr, _, decl = line.partition("\t")
        if not addr.startswith("0x"):
            continue
        a = int(addr, 16)
        if a in seen:
            dups += 1
            continue
        seen[a] = _decl_name(decl) or "FUN_%08x" % a
    return sorted(seen.items()), dups


def true_end_offset(data, secs, addr: int, limit: int) -> int | None:
    """Byte size of the function at `addr`, read from the pristine XBE.

    A body closes at the first terminator (`ret`, or an unconditional `jmp`
    that is not an indirect/table dispatch) with NO outstanding branch target
    at or beyond it.  That second clause is what keeps this from cutting a
    function short at an interior `ret` when MSVC has placed a tail block out
    of line: FUN_00174510 looks finished at 0x174622 until you notice the
    `jl 0x174622` at 0x174608 proving the body continues.

    `limit` is BOTH the scan window and the definition of "outstanding": only a
    target inside [addr, addr+limit) can be a later block of THIS function, so
    a branch past the window is a tail call and must not hold the body open.
    Callers pass the kb.json gap, which makes the window exactly as wide as the
    region the function could possibly occupy.

    Returns None when no terminator is found within `limit` bytes -- callers
    must treat that as "no opinion" and fall back to the kb.json gap.

    ORIGIN: this is `tools/verify/vc71_verify.py::_true_end_offset`, lifted
    rather than imported to keep the future rewiring acyclic (vc71_verify will
    consume this module).  `tools/audit/check_delinked_bounds.py::true_end` is
    the same algorithm with the window pinned at 0x4000 instead of the caller's
    limit, which is why it reports "no terminator" for a function ending in a
    tail `jmp` to a target under 16KB away (errors_initialize, 0x8f370).  All
    three copies should collapse onto this one; `test_function_bounds.py` pins
    this one to vc71_verify's until they do.
    """
    from check_delinked_bounds import va_to_off
    off = va_to_off(secs, addr)
    if off is None:
        return None
    targets: set[int] = set()
    for ins in _md().disasm(data[off:off + limit], addr):
        if ins.mnemonic.startswith("j"):
            try:
                t = int(ins.op_str, 16)
                if addr <= t < addr + limit:
                    targets.add(t)
            except ValueError:
                pass  # indirect branch; the guard below just won't fire early
        end = ins.address + ins.size
        terminator = ins.mnemonic == "ret" or (
            ins.mnemonic == "jmp" and not ins.op_str.startswith("dword"))
        if terminator and not any(t >= end for t in targets):
            return end - addr
    return None


def _walk(data, secs, start: int, end: int):
    """Decode [start, end).  Returns (clean_end, last_mnemonic, problem)."""
    from check_delinked_bounds import va_to_off
    off = va_to_off(secs, start)
    if off is None:
        return start, None, "address not in any section"
    last_end, last_mnem = start, None
    # +16 so an instruction straddling `end` is decoded (and caught) rather
    # than looking like a stall.
    for ins in _md().disasm(data[off:off + (end - start) + 16], start):
        if ins.address >= end:
            break
        if ins.address + ins.size > end:
            return last_end, last_mnem, "cuts the instruction at 0x%x" % ins.address
        last_end, last_mnem = ins.address + ins.size, ins.mnemonic
        if last_end == end:
            return end, last_mnem, None
    if last_end != end:
        return last_end, last_mnem, "decode stalled at 0x%x" % last_end
    return end, last_mnem, None


def _first_insn(data, secs, addr: int):
    from check_delinked_bounds import va_to_off
    off = va_to_off(secs, addr)
    if off is None:
        return None
    for ins in _md().disasm(data[off:off + 16], addr):
        return ins
    return None


def _is_terminator(mnem: str | None) -> bool:
    return mnem in ("ret", "retf", "iret", "jmp")


def _gap_is_padding(data, secs, end: int, nxt: int | None) -> bool | None:
    """True/False for the bytes in [end, nxt), or None when there is no gap."""
    from check_delinked_bounds import va_to_off
    if nxt is None or nxt <= end:
        return None
    off = va_to_off(secs, end)
    if off is None:
        return None
    return all(b in PAD_BYTES for b in data[off:off + (nxt - end)])


def _gap_shape(data, secs, start: int, end: int, nxt: int) -> str:
    """Classify a non-padding gap so a human can triage it quickly.

    "jumptable" means every dword in the gap points back inside the function --
    i.e. the compiler put a switch table after the final ret and the bound
    should probably become a `table_data` override.  "code" means real
    instructions follow, which is either an unlisted function or a tail block
    that `true_end` cut away.
    """
    from check_delinked_bounds import va_to_off
    off = va_to_off(secs, end)
    n = nxt - end
    if off is None or n < 4:
        return "code"
    words = [int.from_bytes(data[off + i:off + i + 4], "little")
             for i in range(0, n - (n % 4), 4)]
    if words and all(start <= w < end for w in words):
        return "jumptable"
    return "code"


def _trim_padding(data, secs, start: int, end: int) -> int | None:
    """Pull `end` back off trailing alignment filler, or None if that is unsafe.

    Only reachable when the bound came from the next kb.json start rather than
    from `true_end`, so `end` is wherever the NEXT function begins -- alignment
    filler included.  The trim is accepted only if the shorter body still
    decodes cleanly and now ends on a terminator; anything else (a real `int3`
    after a noreturn call, a body whose decode is out of sync because data is
    embedded in it) keeps the conservative bound and stays flagged.
    """
    from check_delinked_bounds import va_to_off
    off = va_to_off(secs, start)
    if off is None:
        return None
    body = data[off:off + (end - start)]
    k = 0
    while k < len(body) and body[len(body) - 1 - k] in PAD_BYTES:
        k += 1
    if k == 0 or k == len(body):
        return None
    cand = end - k
    _, mnem, problem = _walk(data, secs, start, cand)
    if problem or not _is_terminator(mnem):
        return None
    return cand


def compute(addr: int, name: str, nxt: int | None, data, secs) -> tuple[dict, dict]:
    """(entry, provenance) for one function.  `nxt` is the next kb.json start.

    `provenance` never reaches the table -- it feeds the sanity sweep, which
    reports which half of the rule actually decided each bound.
    """
    from check_delinked_bounds import va_to_off

    ov = OVERRIDES.get(addr)
    if ov is not None:
        e = dict(end="0x%x" % ov["end"], kind=ov["kind"], name=name)
        # An override is human-verified, so a body that does not decode to its
        # end is expected (that is the point of `table_data`) -- record the
        # reason, never rewrite the bound.
        e["note"] = ov["note"]
        return e, {"source": "override", "true_end": None}

    if va_to_off(secs, addr) is None:
        return ({"end": "0x%x" % addr, "kind": "unmapped", "name": name,
                 "note": "address is not inside any XBE section"},
                {"source": "unmapped", "true_end": None})

    # A leading unconditional jmp IS the whole function: nothing after it can
    # be reached (there is no earlier instruction to branch past it).  Decided
    # before true_end, which would otherwise chase the jump target.
    first = _first_insn(data, secs, addr)
    if first is not None and first.mnemonic == "jmp":
        thunk_end = addr + first.size
        if nxt is None or nxt >= thunk_end:
            return ({"end": "0x%x" % thunk_end, "kind": "thunk", "name": name},
                    {"source": "thunk", "true_end": thunk_end})

    limit = (nxt - addr) if nxt is not None else MAX_SCAN
    size = true_end_offset(data, secs, addr, limit)
    te = addr + size if size else None
    warn = "no terminator within 0x%x bytes" % limit
    info = {"source": ("kb_next" if te is None else
                       "agree" if te == nxt else "true_end"),
            "true_end": te}
    ends = [x for x in (te, nxt) if x is not None and x > addr]
    if not ends:
        return ({"end": "0x%x" % addr, "kind": "no_terminator", "name": name,
                 "note": warn + " and no successor in kb.json"},
                {"source": "unbounded", "true_end": None})

    end = min(ends)
    kind = "auto" if te is not None else "no_terminator"
    note = None if te is not None else warn

    if end != te:
        trimmed = _trim_padding(data, secs, addr, end)
        if trimmed is not None:
            note = ("bound is the next kb.json start; trimmed %d byte(s) of "
                    "alignment filler back to the terminator" % (end - trimmed))
            if te is None:
                note += " (true_end: %s)" % warn
            end, kind = trimmed, "auto"

    clean, last_mnem, problem = _walk(data, secs, addr, end)
    if problem:
        # Never emit a slice that ends inside an instruction.  Back off to the
        # last real boundary and say so; the entry becomes a review item.
        note = "%s; bound backed off from 0x%x" % (problem, end)
        end, kind = clean, "no_terminator"
        _, last_mnem, _ = _walk(data, secs, addr, end)
    elif not _is_terminator(last_mnem) and kind == "auto":
        kind = "no_terminator"
        note = "last instruction is %s, not a terminator" % (last_mnem or "?")

    entry = {"end": "0x%x" % end, "kind": kind, "name": name}
    if note:
        entry["note"] = note
    return entry, info


_BUCKETS = ((0, "0 (bound touches the next start)"), (16, "1-16 (alignment)"),
            (64, "17-64"), (256, "65-256"), (1024, "257-1024"))


def _bucket(gap: int) -> str:
    for hi, label in _BUCKETS:
        if gap <= hi:
            return label
    return ">1024"


def build() -> tuple[dict, dict]:
    """(table, stats).  `table` is exactly what gets serialized."""
    md5 = verify_xbe()
    funcs, dups = load_kb_functions()
    starts = [a for a, _ in funcs]
    data, secs = _xbe()

    entries: dict[str, dict] = {}
    stats = {"kinds": {}, "flagged": [], "dups": dups, "sizes": [],
             "sources": {}, "slack": {}, "listing_holes": 0,
             "unpadded_gaps": []}

    for addr, name in funcs:
        i = bisect.bisect_right(starts, addr)
        nxt = starts[i] if i < len(starts) else None
        entry, info = compute(addr, name, nxt, data, secs)
        entries["0x%x" % addr] = entry

        kind = entry["kind"]
        stats["kinds"][kind] = stats["kinds"].get(kind, 0) + 1
        stats["sources"][info["source"]] = stats["sources"].get(
            info["source"], 0) + 1
        if kind in ("no_terminator", "unmapped"):
            stats["flagged"].append((addr, entry))

        end = int(entry["end"], 16)
        stats["sizes"].append((end - addr, addr, name, kind))

        if nxt is None:
            continue
        gap = nxt - end
        # How much the kb-gap rule would have over-counted, bucketed.  This is
        # the size of the listing-hole class -- the whole reason for the table.
        stats["slack"][_bucket(gap)] = stats["slack"].get(_bucket(gap), 0) + 1
        if gap > ALIGN_GAP:
            stats["listing_holes"] += 1
        elif gap > 0 and _gap_is_padding(data, secs, end, nxt) is False:
            stats["unpadded_gaps"].append(
                (addr, name, end, nxt, _gap_shape(data, secs, addr, end, nxt)))

    table = {"_meta": {"xbe_md5": md5, "version": VERSION,
                       "entries": len(entries)}}
    table.update(entries)
    return table, stats


def serialize(table: dict) -> str:
    """One line per entry: a one-function change is a one-line diff."""
    lines = [' "_meta": ' + json.dumps(table["_meta"], sort_keys=True)]
    for key in sorted((k for k in table if k != "_meta"),
                      key=lambda k: int(k, 16)):
        lines.append(' "%s": %s' % (key, json.dumps(table[key])))
    return "{\n" + ",\n".join(lines) + "\n}\n"


def report(stats: dict, total: int) -> None:
    print("function bounds: %d entries" % total)
    if stats["dups"]:
        print("  (%d duplicate kb.json address(es) collapsed)" % stats["dups"])
    print("\nby kind:")
    for kind, n in sorted(stats["kinds"].items(), key=lambda kv: -kv[1]):
        print("  %-14s %5d" % (kind, n))

    print("\nflagged (no_terminator / unmapped): %d" % len(stats["flagged"]))
    for addr, e in stats["flagged"][:40]:
        print("  0x%-8x %-40s %s" % (addr, e["name"], e.get("note", "")))
    if len(stats["flagged"]) > 40:
        print("  ... %d more" % (len(stats["flagged"]) - 40))

    print("\ntop 20 largest:")
    for size, addr, name, kind in sorted(stats["sizes"], reverse=True)[:20]:
        print("  0x%-8x %6d  %-10s %s" % (addr, size, kind, name))

    print("\nwhich half of the rule decided the bound:")
    for src, n in sorted(stats["sources"].items(), key=lambda kv: -kv[1]):
        print("  %-14s %5d" % (src, n))
    print("  (true_end = binary won, tighter than the kb gap; agree = both "
          "landed on the same address; kb_next = no terminator in the window)")

    print("\nbytes the kb-gap rule would have over-counted:")
    order = [label for _, label in _BUCKETS] + [">1024"]
    for label in order:
        if label in stats["slack"]:
            print("  %-30s %5d" % (label, stats["slack"][label]))

    gaps = stats["unpadded_gaps"]
    tables = [g for g in gaps if g[4] == "jumptable"]
    print("\nnon-padding gaps of <=%d bytes before the next listed start: %d  "
          "(%d look like switch tables -- override candidates)"
          % (ALIGN_GAP, len(gaps), len(tables)))
    for addr, name, end, nxt, shape in tables[:20] + [g for g in gaps
                                                     if g[4] != "jumptable"][:20]:
        print("  0x%-8x end=0x%-8x next=0x%-8x %-10s %s"
              % (addr, end, nxt, shape, name))
    if len(gaps) > 40:
        print("  ... %d more" % (len(gaps) - 40))

    print("\nlisting holes (>%d bytes to the next listed start): %d  "
          "(unlisted code -- exactly what the kb-gap rule mis-attributed)"
          % (ALIGN_GAP, stats["listing_holes"]))


def load_table(path: Path = TABLE) -> dict:
    return json.loads(path.read_text())


def check() -> int:
    if not TABLE.exists():
        print("error: %s missing -- run function_bounds.py to generate it"
              % TABLE, file=sys.stderr)
        return 1
    table, _ = build()
    want, got = serialize(table), TABLE.read_text()
    if want == got:
        print("function_bounds.json up to date (%d entries)"
              % table["_meta"]["entries"])
        return 0

    old = load_table()
    new = table
    old_keys = {k for k in old if k != "_meta"}
    new_keys = {k for k in new if k != "_meta"}
    added, removed = sorted(new_keys - old_keys), sorted(old_keys - new_keys)
    changed = [k for k in sorted(new_keys & old_keys) if new[k] != old[k]]
    print("function_bounds.json is STALE")
    if old.get("_meta") != new.get("_meta"):
        print("  _meta: %s -> %s" % (old.get("_meta"), new.get("_meta")))
    print("  added   %d" % len(added))
    for k in added[:20]:
        print("    + %s %s" % (k, json.dumps(new[k])))
    print("  removed %d" % len(removed))
    for k in removed[:20]:
        print("    - %s %s" % (k, json.dumps(old[k])))
    print("  changed %d" % len(changed))
    for k in changed[:20]:
        print("    ~ %s\n        was %s\n        now %s"
              % (k, json.dumps(old[k]), json.dumps(new[k])))
    if not (added or removed or changed):
        print("  (entries identical -- only formatting differs; regenerate)")
    print("\nrun: rtk python3 tools/verify/function_bounds.py")
    return 1


def show(addrs: list[str]) -> int:
    verify_xbe()
    funcs, _ = load_kb_functions()
    starts = [a for a, _ in funcs]
    names = dict(funcs)
    data, secs = _xbe()
    rc = 0
    for text in addrs:
        addr = int(text, 16)
        if addr not in names:
            print("%-12s NOT IN kb.json" % text)
            rc = 1
            continue
        i = bisect.bisect_right(starts, addr)
        nxt = starts[i] if i < len(starts) else None
        e, info = compute(addr, names[addr], nxt, data, secs)
        print('"0x%x": %s   (size %d, next kb start %s, bound from %s)'
              % (addr, json.dumps(e), int(e["end"], 16) - addr,
                 "0x%x" % nxt if nxt else "none", info["source"]))
    return rc


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--check", action="store_true",
                    help="recompute and fail on drift (CI gate); writes nothing")
    ap.add_argument("--addrs", nargs="+", metavar="0xADDR",
                    help="print entries for these addresses; writes nothing")
    ap.add_argument("-o", "--out", type=Path, default=TABLE)
    args = ap.parse_args()

    if args.addrs:
        return show(args.addrs)
    if args.check:
        return check()

    table, stats = build()
    args.out.write_text(serialize(table))
    report(stats, table["_meta"]["entries"])
    print("\nwrote %s" % args.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
