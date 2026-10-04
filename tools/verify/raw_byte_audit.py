#!/usr/bin/env python3
"""Strict literal raw-byte audit for one VC71-compiled function.

This is deliberately separate from VC71 mnemonic matching. It recompiles the
candidate, extracts the COFF symbol's literal section bytes, and compares them
with the committed function span in the pristine XBE. It performs no relocation
masking, instruction normalization, padding trimming, or disassembly.

Verdicts are exactly: ``raw-byte exact``, ``bytes differ``, and
``not comparable``. A COFF relocation in the candidate function makes it not
comparable; this first version does not try to reconcile it with the XBE.
"""

import argparse
import concurrent.futures
import hashlib
import json
import os
import shutil
import subprocess
import struct
import sys
import tempfile
from datetime import datetime, timezone
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(HERE))

import vc71_verify as vc71
import xbe_reference as xref

COFF_FILE_HEADER = struct.Struct("<HHIIIHH")
COFF_SECTION_HEADER = struct.Struct("<8sIIIIIIHHI")
COFF_SYMBOL = struct.Struct("<8sIhHBB")
IMAGE_SYM_DTYPE_FUNCTION = 0x20


class NotComparable(Exception):
    pass


def sha256_bytes(data):
    return hashlib.sha256(data).hexdigest()


def sha256_file(path):
    return sha256_bytes(path.read_bytes())


def _coff_name(raw, string_table):
    zeroes, offset = struct.unpack("<II", raw)
    if zeroes == 0 and offset:
        if offset < 4 or offset >= len(string_table):
            raise NotComparable("COFF string-table name offset is invalid")
        return string_table[offset:].split(b"\0", 1)[0].decode("ascii", "replace")
    return raw.split(b"\0", 1)[0].decode("ascii", "replace")


def _same_symbol(left, right):
    def undecorate(name):
        # __fastcall (register-argument) symbols are "@name@N".
        name = name[1:] if name.startswith(("_", "@")) else name
        if "@" in name and name.rsplit("@", 1)[1].isdigit():
            name = name.rsplit("@", 1)[0]
        return name
    # A C name that itself starts with "_" is decorated to "__name"; stripping
    # both sides would compare "_name" against "name" and never match.
    return undecorate(left) in (undecorate(right), right)


def prefer_exact_decoration(matches, function):
    """Narrow several _same_symbol matches to the one ``function`` itself compiles to.

    _same_symbol also accepts the symbol of the C name without its leading
    underscore, so a TU defining both ``_name`` and ``name`` yields two matches
    (``__name`` and ``_name``). The C name's own cdecl, stdcall or fastcall
    decoration is the only one that is unambiguous; with none, keep the matches.
    """
    def decorated(symbol):
        tail = symbol.rsplit("@", 1)[1] if "@" in symbol[1:] else ""
        return (symbol == "_" + function
                or (tail.isdigit() and symbol[:-len(tail) - 1] in ("_" + function, "@" + function)))
    exact = [s for s in matches if decorated(s["name"])]
    return exact if len(matches) > 1 and exact else matches


def extract_coff_function(obj_path, function):
    """Return literal candidate bytes and extraction provenance.

    A function must have a COFF function symbol with a bounded span in one
    section. A relocation overlapping that span is a non-comparable result.
    """
    data = obj_path.read_bytes()
    if len(data) < COFF_FILE_HEADER.size:
        raise NotComparable("candidate is smaller than a COFF file header")
    machine, section_count, _stamp, symbol_offset, symbol_count, optional_size, _chars = (
        COFF_FILE_HEADER.unpack_from(data, 0))
    if machine != 0x14C:
        raise NotComparable("candidate is not an i386 COFF object")
    section_offset = COFF_FILE_HEADER.size + optional_size
    section_end = section_offset + section_count * COFF_SECTION_HEADER.size
    if section_end > len(data):
        raise NotComparable("candidate has truncated COFF section headers")
    if symbol_offset + symbol_count * COFF_SYMBOL.size + 4 > len(data):
        raise NotComparable("candidate has truncated COFF symbol table")

    string_offset = symbol_offset + symbol_count * COFF_SYMBOL.size
    string_size = struct.unpack_from("<I", data, string_offset)[0]
    if string_size < 4 or string_offset + string_size > len(data):
        raise NotComparable("candidate has invalid COFF string table")
    strings = data[string_offset:string_offset + string_size]

    sections = []
    for index in range(section_count):
        raw = COFF_SECTION_HEADER.unpack_from(
            data, section_offset + index * COFF_SECTION_HEADER.size)
        name, _vsize, _va, raw_size, raw_offset, reloc_offset, _line_offset, reloc_count, _line_count, _flags = raw
        if raw_offset + raw_size > len(data):
            raise NotComparable("candidate has truncated section data")
        if reloc_count and reloc_offset + reloc_count * 10 > len(data):
            raise NotComparable("candidate has truncated relocation table")
        sections.append({
            "name": _coff_name(name, strings), "raw_size": raw_size,
            "raw_offset": raw_offset, "reloc_offset": reloc_offset,
            "reloc_count": reloc_count,
        })

    symbols = []
    index = 0
    while index < symbol_count:
        at = symbol_offset + index * COFF_SYMBOL.size
        raw_name, value, section, typ, storage, aux_count = COFF_SYMBOL.unpack_from(data, at)
        if index + 1 + aux_count > symbol_count:
            raise NotComparable("candidate has truncated COFF auxiliary symbols")
        total_size = None
        # IMAGE_AUX_SYMBOL_FUNCTION_DEF stores TotalSize at byte +4. It is the
        # only COFF-provided function extent; do not infer a span from the next
        # symbol when it is absent, because that would not be a literal audit.
        if aux_count and typ == IMAGE_SYM_DTYPE_FUNCTION:
            aux_at = at + COFF_SYMBOL.size
            total_size = struct.unpack_from("<I", data, aux_at + 4)[0]
        symbols.append({
            "name": _coff_name(raw_name, strings), "value": value,
            "section": section, "type": typ, "storage": storage,
            "total_size": total_size,
        })
        index += 1 + aux_count

    matches = [s for s in symbols if s["section"] > 0
               and s["type"] == IMAGE_SYM_DTYPE_FUNCTION
               and _same_symbol(s["name"], function)]
    matches = prefer_exact_decoration(matches, function)
    if len(matches) != 1:
        raise NotComparable("candidate has %d COFF function symbols for %s" %
                            (len(matches), function))
    symbol = matches[0]
    section_index = symbol["section"] - 1
    if section_index >= len(sections):
        raise NotComparable("candidate function symbol references no section")
    section = sections[section_index]
    start = symbol["value"]
    if start >= section["raw_size"]:
        raise NotComparable("candidate function starts outside its section")

    if symbol["total_size"]:
        end = start + symbol["total_size"]
    else:
        # VC71 stores an external function symbol without an auxiliary
        # definition record. Its /Gy output uses one COMDAT .text section per
        # function, so that section is an exact literal extent only when this
        # is its sole function symbol at offset zero. Reject every other case.
        section_functions = [s for s in symbols if s["section"] == symbol["section"]
                             and s["type"] == IMAGE_SYM_DTYPE_FUNCTION]
        if start != 0 or len(section_functions) != 1:
            raise NotComparable("candidate function has no unambiguous COFF extent")
        end = section["raw_size"]
    if end <= start or end > section["raw_size"]:
        raise NotComparable("candidate function has invalid COFF extent")

    for reloc_index in range(section["reloc_count"]):
        reloc_at = section["reloc_offset"] + reloc_index * 10
        virtual_address = struct.unpack_from("<I", data, reloc_at)[0]
        if start <= virtual_address < end:
            raise NotComparable("candidate function has COFF relocation at +0x%x" %
                                (virtual_address - start))

    candidate = data[section["raw_offset"] + start:section["raw_offset"] + end]
    return candidate, {
        "coff_symbol": symbol["name"],
        "coff_section": section["name"],
        "coff_offset": start,
        "coff_end": end,
        "coff_relocations": section["reloc_count"],
    }


def audit(candidate_obj, function, address, source=None):
    """Run one literal raw-byte comparison and return a JSON-safe record."""
    record = {
        "function": function,
        "address": "0x%08x" % address,
        "candidate": {"path": str(candidate_obj), "sha256": sha256_file(candidate_obj)},
        "reference": {"path": str(xref.XBE), "sha256": sha256_file(xref.XBE)},
        "generated_at": datetime.now(timezone.utc).isoformat(),
    }
    if source is not None:
        record["source"] = {"path": str(source), "sha256": sha256_file(source)}
    extent = xref.function_extent(address)
    reference, error = xref.function_bytes(address)
    if extent is None or reference is None:
        record["verdict"] = "not comparable"
        record["reason"] = error or "no committed XBE function bound"
        return record
    end, kind, bound_provenance = extent
    record["reference"].update({
        "start": "0x%08x" % address,
        "end": "0x%08x" % end,
        "length": len(reference),
        "sha256_span": sha256_bytes(reference),
        "bound_kind": kind,
        "bound_provenance": bound_provenance,
    })
    try:
        candidate, candidate_provenance = extract_coff_function(candidate_obj, function)
    except NotComparable as exc:
        record["verdict"] = "not comparable"
        record["reason"] = str(exc)
        return record
    record["candidate"].update(candidate_provenance)
    record["candidate"].update({"length": len(candidate), "sha256_span": sha256_bytes(candidate)})
    if candidate == reference:
        record["verdict"] = "raw-byte exact"
        return record
    record["verdict"] = "bytes differ"
    record["first_difference"] = next(
        (i for i, pair in enumerate(zip(candidate, reference)) if pair[0] != pair[1]),
        min(len(candidate), len(reference)))
    return record


def _write_record(record, output):
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_name(output.name + ".tmp")
    temporary.write_text(json.dumps(record, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    os.replace(temporary, output)


def _hash_path(path):
    return sha256_file(path) if path.is_file() else None


def _eligible_functions(source_filter=None):
    kb = json.loads((ROOT / "kb.json").read_text(encoding="utf-8"))
    if isinstance(source_filter, str):
        source_filter = {Path(source_filter).as_posix()}
    entries = list(kb.values()) if isinstance(kb, dict) else list(kb)
    for obj in kb.get("objects", []) if isinstance(kb, dict) else []:
        entries.extend(obj.get("functions", []))
    eligible = {}
    for entry in entries:
        if not isinstance(entry, dict) or entry.get("ported") is not True:
            continue
        source_path = entry.get("source_path") or entry.get("source")
        if not source_path or not entry.get("addr"):
            continue
        source_path = source_path if source_path.startswith("src/") else "src/halo/" + source_path
        if source_filter and Path(source_path).as_posix() not in source_filter:
            continue
        source = ROOT / source_path
        if not source.is_file():
            continue
        address = int(entry["addr"], 0)
        eligible[address] = {
            "function": entry.get("name") or "FUN_%08X" % address,
            "address": address,
            "source": source,
        }
    return list(eligible.values())


def _bounds_hash():
    return _hash_path(ROOT / "tools" / "verify" / "function_bounds.json")


def _decl_hash():
    return _hash_path(ROOT / "build" / "generated" / "decl.h")


def _compiler_token():
    return getattr(vc71, "VC71_CL_WSL", "VC71") + " /O2 /Oy- /GF /Gy /Gd /W0 /Zl /X"


def _record_error(item, reason, source_sha256=None):
    return {
        "function": item["function"],
        "address": "0x%08x" % item["address"],
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "verdict": "not comparable",
        "reason": reason,
        "source": {"path": str(item["source"]), "sha256": source_sha256 or _hash_path(item["source"])},
        "reference": {"path": str(xref.XBE), "sha256": _hash_path(xref.XBE)},
    }


def _audit_tu(source, items, artifact_dir, decl_hash):
    source_sha256 = _hash_path(source)
    candidate = artifact_dir / ("tu-%s.obj" % hashlib.sha256(str(source).encode()).hexdigest()[:16])
    if not vc71.compile_vc71(source, candidate):
        return [(item, _record_error(item, "VC71 compilation failed", source_sha256)) for item in items]
    records = []
    for item in items:
        try:
            record = audit(candidate, item["function"], item["address"], source)
        except OSError as exc:
            record = _record_error(item, str(exc), source_sha256)
        record["tool"] = {"version": "1", "compiler": _compiler_token(), "decl_sha256": decl_hash,
                          "bounds_sha256": _bounds_hash()}
        records.append((item, record))
    return records


def _write_summary(records, invocation, source_count, compile_failures, eligible=None):
    totals = {verdict: {"functions": 0, "original_bytes": 0}
              for verdict in ("raw-byte exact", "bytes differ", "not comparable", "not audited")}
    for record in records:
        verdict = record.get("verdict", "not audited")
        totals.setdefault(verdict, {"functions": 0, "original_bytes": 0})
        totals[verdict]["functions"] += 1
        totals[verdict]["original_bytes"] += record.get("reference", {}).get("length", 0) or 0
    audited_keys = {(record.get("address"), record.get("function")) for record in records}
    for item in eligible or []:
        key = ("0x%08x" % item["address"], item["function"])
        if key not in audited_keys:
            totals["not audited"]["functions"] += 1
            extent = xref.function_extent(item["address"])[0]
            if extent is not None:
                totals["not audited"]["original_bytes"] += extent - item["address"]
    exact = totals["raw-byte exact"]["functions"]
    comparable = exact + totals["bytes differ"]["functions"]
    summary = {
        "tool_version": "1",
        "invocation": invocation,
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "xbe_sha256": _hash_path(xref.XBE),
        "bounds_sha256": _bounds_hash(),
        "decl_sha256": _decl_hash(),
        "source_tu_count": source_count,
        "candidate_compile_failures": compile_failures,
        "totals": totals,
        "exact_among_comparable": exact / comparable if comparable else None,
        "exact_coverage_of_ported_functions": exact / len(records) if records else None,
    }
    output = ROOT / "artifacts" / "raw_byte_audit" / "summary.json"
    _write_record(summary, output)
    return summary


def _run_single(args):
    source = args.source.resolve()
    if not source.is_file():
        raise SystemExit("source does not exist: %s" % source)
    artifact_dir = ROOT / "artifacts" / "raw_byte_audit"
    candidate = artifact_dir / ("%08x-%s.obj" % (args.address, args.function))
    output = args.output or artifact_dir / ("%08x-%s.json" % (args.address, args.function))
    try:
        vc71.regen_decl_header(quiet=True)
        if not vc71.compile_vc71(source, candidate):
            return 2
        record = audit(candidate, args.function, args.address, source)
    except OSError as exc:
        record = _record_error({"function": args.function, "address": args.address, "source": source}, str(exc))
    _write_record(record, output)
    print("%s: %s (%s)" % (record["function"], record["verdict"], output))
    return {"raw-byte exact": 0, "bytes differ": 1, "not comparable": 2}[record["verdict"]]


def _changed_sources():
    try:
        base = subprocess.run(["git", "merge-base", "HEAD", "main"], cwd=ROOT,
                              capture_output=True, text=True, check=True).stdout.strip()
        changed = subprocess.run(["git", "diff", "--name-only", base, "--", "src", "kb.json",
                                  "tools/verify/function_bounds.json", "tools/verify/raw_byte_audit.py"],
                                 cwd=ROOT, capture_output=True, text=True, check=True).stdout.splitlines()
    except (OSError, subprocess.CalledProcessError):
        return None
    return {path for path in changed if path.endswith(".c")}


def _run_populate(args):
    items = _eligible_functions(args.source)
    grouped = {}
    for item in items:
        grouped.setdefault(item["source"], []).append(item)
    if not vc71.regen_decl_header(quiet=True):
        raise SystemExit("could not pin decl.h")
    decl_hash = _decl_hash()
    artifact_dir = ROOT / "artifacts" / "raw_byte_audit"
    records = []
    compile_failures = 0
    groups = sorted(grouped.items(), key=lambda pair: str(pair[0]))
    workers = max(1, min(args.workers, len(groups) or 1))
    with concurrent.futures.ProcessPoolExecutor(max_workers=workers) as executor:
        futures = [executor.submit(_audit_tu, source, source_items, artifact_dir, decl_hash)
                   for source, source_items in groups]
        for future in futures:
            results = future.result()
            if results and all(record.get("reason") == "VC71 compilation failed" for _, record in results):
                compile_failures += 1
            for item, record in results:
                output = artifact_dir / ("%08x-%s.json" % (item["address"], item["function"]))
                _write_record(record, output)
                records.append(record)
    summary = _write_summary(records, sys.argv[1:], len(grouped), compile_failures, eligible=items)
    print(json.dumps(summary["totals"], sort_keys=True))
    return 0 if not compile_failures else 2


def main():
    if len(sys.argv) > 1 and sys.argv[1] == "single":
        sys.argv.pop(1)
    if len(sys.argv) > 1 and sys.argv[1] not in ("populate", "check", "show"):
        parser = argparse.ArgumentParser(description=__doc__)
        parser.add_argument("source", type=Path, help="C source to compile freshly with VC71")
        parser.add_argument("--function", required=True, help="C/COFF function symbol")
        parser.add_argument("--address", required=True, type=lambda text: int(text, 0), help="Pristine XBE virtual address")
        parser.add_argument("--output", type=Path, help="JSON audit record path")
        args = parser.parse_args()
        return _run_single(args)
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    populate = subparsers.add_parser("populate")
    populate.add_argument("--source")
    populate.add_argument("--workers", type=int, default=1)
    check = subparsers.add_parser("check")
    check.add_argument("--changed", action="store_true")
    subparsers.add_parser("show")
    args = parser.parse_args()
    if args.command == "populate":
        return _run_populate(args)
    summary_path = ROOT / "artifacts" / "raw_byte_audit" / "summary.json"
    if not summary_path.is_file():
        print("no raw-byte audit summary")
        return 2
    summary = json.loads(summary_path.read_text(encoding="utf-8"))
    if args.command == "show":
        for verdict, totals in summary["totals"].items():
            print("%-18s %8d functions %10d bytes" % (verdict + ":", totals["functions"], totals["original_bytes"]))
        print("Exact among comparable: %s" % summary["exact_among_comparable"])
        print("Exact coverage of ported functions: %s" % summary["exact_coverage_of_ported_functions"])
        return 0
    if args.changed:
        changed = _changed_sources()
        if changed is None:
            print("could not determine merge-base changes", file=sys.stderr)
            return 2
        eligible = _eligible_functions()
        selected = {str(item["source"].relative_to(ROOT)) for item in eligible} & changed
        for source in sorted(selected):
            print("selected %s: source changed from merge base" % source)
        if not selected:
            print("no changed source TUs selected")
            return 0
        items = _eligible_functions(selected)
        old = {}
        artifact_dir = ROOT / "artifacts" / "raw_byte_audit"
        for item in items:
            path = artifact_dir / ("%08x-%s.json" % (item["address"], item["function"]))
            if path.is_file():
                try:
                    old[item["address"]] = json.loads(path.read_text(encoding="utf-8")).get("verdict")
                except (OSError, json.JSONDecodeError):
                    pass
        result = _run_populate(argparse.Namespace(source=selected, workers=1))
        regressions = []
        for item in items:
            path = artifact_dir / ("%08x-%s.json" % (item["address"], item["function"]))
            if not path.is_file():
                continue
            current = json.loads(path.read_text(encoding="utf-8")).get("verdict")
            if old.get(item["address"]) == "raw-byte exact" and current != "raw-byte exact":
                regressions.append(item["function"])
        if regressions:
            print("raw-byte exact regressions: %s" % ", ".join(regressions), file=sys.stderr)
            return 1
        return result
    return 0


if __name__ == "__main__":
    sys.exit(main())
