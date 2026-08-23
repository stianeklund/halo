#!/usr/bin/env python3
"""Audit kb.json object attribution with narrow delinker exports.

Each supplied range is exported independently.  A range with one relocated
__FILE__ path yields a high-confidence cluster candidate, not per-function
proof; ranges with zero or multiple paths remain unknown rather than guessed.
"""

import argparse
import json
import os
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / "tools"
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))

from analysis.classify_common import _delink_range, _derive_obj_name, _parse_source_paths


FUNCTION_NAME_RE = re.compile(r"(?P<name>\*?[A-Za-z_]\w*)\s*\(")


def parse_addr(value):
    return int(value.strip().lower().removeprefix("0x"), 16)


def parse_range(value):
    """Parse LABEL:START-END or START-END into (label, start, end)."""
    label, separator, bounds = value.partition(":")
    if not separator:
        bounds = label
        label = "range_%s" % bounds.replace("-", "_")
    start, dash, end = bounds.partition("-")
    if not dash:
        raise ValueError("range must be LABEL:START-END or START-END")
    lo = parse_addr(start)
    hi = parse_addr(end)
    if hi < lo:
        raise ValueError("range end precedes start: %s" % value)
    return label, lo, hi


def function_name(decl):
    match = FUNCTION_NAME_RE.search(decl or "")
    return match.group("name") if match else "?"


def load_object(kb_path, object_name):
    with open(kb_path, encoding="utf-8") as source:
        kb = json.load(source)
    for obj in kb.get("objects", []):
        if obj.get("name") == object_name:
            return obj
    raise ValueError("object not found: %s" % object_name)


def normalize_source(source):
    return source.replace("\\", "/")


def build_report(obj, ranges, probe):
    """Build report using probe(label, lo, hi) -> [(reloc_offset, source)]."""
    object_name = obj["name"]
    functions = []
    for entry in obj.get("functions", []):
        addr = entry.get("addr")
        if addr is None:
            continue
        functions.append({
            "addr": parse_addr(addr),
            "addr_text": "0x%x" % parse_addr(addr),
            "function": function_name(entry.get("decl")),
            "decl": entry.get("decl", ""),
        })
    functions.sort(key=lambda item: item["addr"])

    assigned = set()
    range_rows = []
    function_rows = []
    mixed_ranges = []
    for label, lo, hi in ranges:
        members = [item for item in functions if lo <= item["addr"] <= hi]
        overlaps = [item["addr_text"] for item in members if item["addr"] in assigned]
        if overlaps:
            raise ValueError("overlapping supplied ranges assign %s" % ", ".join(overlaps))
        assigned.update(item["addr"] for item in members)

        relocations = probe(label, lo, hi)
        sources = sorted({normalize_source(source) for _, source in relocations})
        if len(sources) == 1:
            source = sources[0]
            confidence = "high"
            evidence = "single source-path relocation in narrow cluster"
            source_object = _derive_obj_name(source)
        elif not sources:
            source = None
            source_object = None
            confidence = "unknown"
            evidence = "no source-path relocation in narrow export"
        else:
            source = None
            source_object = None
            confidence = "uncertain"
            evidence = "multiple source-path relocations in one supplied range"
            mixed_ranges.append(label)

        range_rows.append({
            "label": label,
            "range": "0x%x-0x%x" % (lo, hi),
            "function_count": len(members),
            "sources": sources,
            "relocation_count": len(relocations),
            "confidence": confidence,
            "evidence": evidence,
        })
        for item in members:
            function_rows.append({
                "addr": item["addr_text"],
                "current_object": object_name,
                "kb_function": item["function"],
                "source_file": source,
                "source_object": source_object,
                "delinker_sources": sources,
                "confidence": confidence,
                "evidence": evidence,
            })

    function_rows.sort(key=lambda item: int(item["addr"], 16))
    uncovered = [
        {
            "addr": item["addr_text"],
            "current_object": object_name,
            "kb_function": item["function"],
            "confidence": "unknown",
            "evidence": "not included in a supplied narrow range",
        }
        for item in functions if item["addr"] not in assigned
    ]
    mismatches = [
        item for item in function_rows
        if item["source_object"] and item["source_object"] != object_name
    ]
    return {
        "object": object_name,
        "current_source": obj.get("source"),
        "function_count": len(functions),
        "ranges": range_rows,
        "functions": function_rows,
        "uncovered_functions": uncovered,
        "mixed_source_ranges": mixed_ranges,
        "attribution_mismatches": mismatches,
        "check_passed": not mixed_ranges and not mismatches,
    }


def markdown(report):
    lines = [
        "# Object Provenance: `%s`" % report["object"],
        "",
        "Current source: `%s`" % (report["current_source"] or "unknown"),
        "",
        "## Ranges",
        "",
        "| Range | Functions | Source paths | Confidence | Evidence |",
        "|---|---:|---|---|---|",
    ]
    for row in report["ranges"]:
        sources = ", ".join("`%s`" % source for source in row["sources"]) or "-"
        lines.append("| `%s` | %d | %s | %s | %s |" % (
            row["range"], row["function_count"], sources,
            row["confidence"], row["evidence"]))
    lines.extend([
        "",
        "## Functions",
        "",
        "| Address | Function | Source | Confidence | Evidence |",
        "|---|---|---|---|---|",
    ])
    for row in report["functions"] + report["uncovered_functions"]:
        lines.append("| `%s` | `%s` | %s | %s | %s |" % (
            row["addr"], row["kb_function"],
            "`%s`" % row.get("source_file") if row.get("source_file") else "-",
            row["confidence"], row["evidence"]))
    lines.extend([
        "",
        "Check: %s" % ("PASS" if report["check_passed"] else "MIXED"),
    ])
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--object", required=True, help="kb.json object name")
    parser.add_argument("--range", action="append", required=True,
                        help="narrow LABEL:START-END range; may repeat")
    parser.add_argument("--kb", default=str(ROOT / "kb.json"))
    parser.add_argument("--markdown", action="store_true", help="emit Markdown instead of JSON")
    parser.add_argument("--check", action="store_true",
                        help="fail if any narrow range proves another object source")
    args = parser.parse_args()

    try:
        ranges = [parse_range(value) for value in args.range]
        obj = load_object(args.kb, args.object)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        parser.error(str(error))

    def probe(label, lo, hi):
        output = _delink_range(label, lo, hi)
        return _parse_source_paths(output) if output else []

    report = build_report(obj, ranges, probe)
    print(markdown(report) if args.markdown else json.dumps(report, indent=2))
    return 0 if not args.check or report["check_passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
