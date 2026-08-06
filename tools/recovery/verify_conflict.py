#!/usr/bin/env python3
"""Verify structize conflicts against the delinked binary reference.

The conflict list from `structize.py campaign` reports what the LIFTED SOURCE
says about each offset's type.  The source can have decompiler errors -- the
Ghidra decompiler regularly widens byte accesses to int reads.  This script
checks the actual operand widths in the delinked COFF object (the MSVC 7.1
compiler output) and reports the ground-truth type.

Usage:
    # Verify all conflicts for a binding:
    python3 tools/recovery/verify_conflict.py --binding actor_t

    # Verify one offset:
    python3 tools/recovery/verify_conflict.py --binding actor_t --offset 0x9c

    # Use a specific delinked object:
    python3 tools/recovery/verify_conflict.py --delinked delinked/actor_looking.obj --offset 0x9c

Output is JSON with the binary-proven operand widths per function, a verdict
on the correct field type, and a confidence level.
"""

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

DELINKED = ROOT / "delinked"


def _find_delinked_objects(source_files):
    """Find delinked .obj files that cover the given source files."""
    objects = []
    for src in source_files:
        stem = Path(src).stem
        candidate = DELINKED / (stem + ".obj")
        if candidate.is_file():
            objects.append(candidate)
    if not objects:
        for obj in sorted(DELINKED.glob("*.obj")):
            if obj.stat().st_size > 1000:
                objects.append(obj)
    return objects


def _objdump_intel(obj_path):
    """Disassemble a COFF object with Intel syntax."""
    proc = subprocess.run(
        ["objdump", "-d", "-M", "intel", "--no-show-raw-insn", str(obj_path)],
        capture_output=True, text=True)
    if proc.returncode != 0:
        return None
    return proc.stdout


_SYM_LINE = re.compile(r"^[0-9a-f]+ <(.+?)>:")
_WIDTH_RE = re.compile(r"(BYTE|WORD|DWORD|QWORD)\s+PTR")


def _scan_offset(disasm, offset_hex, raw=False):
    """Find every instruction accessing [reg+offset] and its operand width.

    Returns a list of {function, width, instruction, file_offset} dicts.
    Also catches LEA (address-of) and ADD reg,offset (base computation).
    """
    offset_int = int(offset_hex, 16)
    offset_pattern = "0x%x]" % offset_int
    offset_pattern_alt = "0x%x," % offset_int
    offset_add = "$0x%x," % offset_int if raw else "0x%x" % offset_int

    results = []
    current_func = None

    for line in disasm.splitlines():
        sym = _SYM_LINE.match(line)
        if sym:
            current_func = sym.group(1).lstrip("_")
            continue
        if not current_func:
            continue

        lower = line.lower()
        is_access = offset_pattern in lower
        is_lea = "lea" in lower and offset_pattern in lower
        is_add = ("add" in lower and ("0x%x" % offset_int) in lower
                  and "ptr" not in lower)

        if not is_access and not is_lea and not is_add:
            continue

        parts = line.strip().split("\t") if "\t" in line else line.strip().split("  ")
        insn_text = parts[-1].strip() if parts else line.strip()
        addr_match = re.match(r"\s*([0-9a-f]+):", line)
        file_offset = addr_match.group(1) if addr_match else ""

        if is_lea:
            results.append({
                "function": current_func,
                "width": "address-of",
                "instruction": insn_text,
                "file_offset": file_offset,
            })
            continue

        if is_add and not is_access:
            results.append({
                "function": current_func,
                "width": "add-base",
                "instruction": insn_text,
                "file_offset": file_offset,
            })
            continue

        width_match = _WIDTH_RE.search(line)
        if width_match:
            width_str = width_match.group(1).lower()
            width_map = {"byte": 1, "word": 2, "dword": 4, "qword": 8}
            width = width_map.get(width_str, 0)
            mnemonic = insn_text.split()[0].lower() if insn_text.split() else ""
            is_fpu = mnemonic.startswith("f") and mnemonic not in ("find",)
            kind = "float" if is_fpu else "int"
            results.append({
                "function": current_func,
                "kind": kind,
                "width": width,
                "width_name": width_str,
                "instruction": insn_text,
                "file_offset": file_offset,
            })
        elif "call" not in lower:
            results.append({
                "function": current_func,
                "width": "unknown",
                "instruction": insn_text,
                "file_offset": file_offset,
            })

    return results


def _determine_verdict(accesses):
    """Determine the ground-truth field type from binary operand widths."""
    widths = {}
    kinds = {}
    for a in accesses:
        w = a["width"]
        if isinstance(w, int):
            widths[w] = widths.get(w, 0) + 1
            k = a.get("kind", "int")
            kinds[k] = kinds.get(k, 0) + 1

    if not widths:
        return {"verdict": "inconclusive", "reason": "no typed accesses found"}

    kind_verdict = None
    if "float" in kinds and "int" in kinds:
        if kinds["float"] > kinds["int"]:
            kind_verdict = "float"
        elif kinds["int"] > kinds["float"]:
            kind_verdict = "int"
        else:
            kind_verdict = "mixed"
    elif "float" in kinds:
        kind_verdict = "float"
    else:
        kind_verdict = "int"

    if len(widths) == 1:
        w = list(widths.keys())[0]
        return {
            "verdict": "uniform",
            "width": w,
            "kind": kind_verdict,
            "count": widths[w],
            "confidence": "high",
            "reason": "all %d accesses use %d-byte %s operands" % (
                widths[w], w,
                kind_verdict if kind_verdict != "mixed" else "float+int"),
            "kinds": kinds,
        }

    dominant = max(widths, key=widths.get)
    total = sum(widths.values())

    if len(widths) == 2 and 1 in widths and 2 in widths:
        return {
            "verdict": "int16-with-byte-optimization",
            "width": 2,
            "kind": kind_verdict,
            "confidence": "high",
            "reason": ("word (%d) and byte (%d) accesses only; the field is "
                       "int16_t -- byte accesses are low-byte writes (set to "
                       "0/1) which is a standard MSVC optimization"
                       % (widths[2], widths[1])),
            "all_widths": widths,
            "kinds": kinds,
        }

    return {
        "verdict": "genuine-conflict",
        "kind": kind_verdict,
        "confidence": "needs-investigation",
        "reason": ("multiple operand widths in the binary: %s -- this offset "
                   "is accessed at different granularities by different "
                   "functions. Likely a union, sub-struct, or mode-dependent "
                   "reinterpretation. DO NOT pick a single type; investigate "
                   "the functions listed below"
                   % ", ".join("%d-byte (%dx)" % (k, v)
                              for k, v in sorted(widths.items()))),
        "all_widths": widths,
        "kinds": kinds,
    }


def verify_offset(offset_hex, delinked_objects):
    """Verify one offset against all provided delinked objects."""
    all_accesses = []
    for obj in delinked_objects:
        disasm = _objdump_intel(obj)
        if disasm is None:
            continue
        accesses = _scan_offset(disasm, offset_hex)
        for a in accesses:
            a["object"] = obj.name
        all_accesses.extend(accesses)

    verdict = _determine_verdict(all_accesses)

    by_func = {}
    for a in all_accesses:
        fn = a["function"]
        by_func.setdefault(fn, []).append({
            "width": a["width"],
            "width_name": a.get("width_name", str(a["width"])),
            "instruction": a["instruction"],
        })

    return {
        "offset_hex": offset_hex,
        "accesses": len(all_accesses),
        "verdict": verdict,
        "functions": by_func,
    }


def verify_binding(binding_id, offset=None):
    """Verify all conflicts (or one offset) for a binding."""
    from tools.recovery.structize import load_binding, worklist as run_worklist

    binding = load_binding(binding_id)
    struct_name = binding["struct"]

    wl = run_worklist(binding_id)
    conflicts = wl["conflicts"]

    if offset:
        conflicts = [c for c in conflicts if c["offset_hex"] == offset]
        if not conflicts:
            return {"ok": False, "error": "no conflict at %s" % offset}

    source_files = set()
    for c in conflicts:
        source_files.update(c.get("files", []))
    delinked_objects = _find_delinked_objects(source_files)
    if not delinked_objects:
        return {"ok": False, "error": "no delinked objects found for %s"
                % ", ".join(source_files)}

    results = []
    for c in conflicts:
        result = verify_offset(c["offset_hex"], delinked_objects)
        result["struct"] = struct_name
        result["source_conflict"] = c["reason"]
        result["sites_blocked"] = c["sites_blocked"]
        results.append(result)

    return {
        "ok": True,
        "binding": binding_id,
        "struct": struct_name,
        "delinked_objects": [o.name for o in delinked_objects],
        "results": results,
    }


def verify_delinked(delinked_path, offset_hex):
    """Verify one offset against a specific delinked object."""
    obj = Path(delinked_path)
    if not obj.is_file():
        return {"ok": False, "error": "not found: %s" % obj}
    result = verify_offset(offset_hex, [obj])
    return {"ok": True, "delinked": obj.name, **result}


def _emit(payload):
    json.dump(payload, sys.stdout, indent=1, sort_keys=True)
    sys.stdout.write("\n")


def main(argv=None):
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--binding",
                        help="binding id from recovery/bindings.json")
    parser.add_argument("--offset",
                        help="single offset to verify (e.g. 0x9c)")
    parser.add_argument("--delinked",
                        help="specific delinked .obj file to use")
    args = parser.parse_args(argv)

    if args.delinked:
        if not args.offset:
            parser.error("--delinked requires --offset")
        _emit(verify_delinked(args.delinked, args.offset))
        return 0

    if args.binding:
        result = verify_binding(args.binding, offset=args.offset)
        _emit(result)
        return 0 if result.get("ok") else 1

    parser.error("--binding or --delinked is required")


if __name__ == "__main__":
    sys.exit(main())
