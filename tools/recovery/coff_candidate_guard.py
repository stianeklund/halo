#!/usr/bin/env python3
"""Exact candidate-before/candidate-after guard for COFF objects.

This is a neutrality guard for two candidate objects.  It does not establish
equivalence with the original Xbox binary.
"""

import argparse
import json
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.equivalence.coff_loader import (  # noqa: E402
    COFF_HEADER_FMT,
    COFF_HEADER_SIZE,
    IMAGE_FILE_MACHINE_I386,
    RELOC_SIZE,
    SECTION_HEADER_FMT,
    SECTION_HEADER_SIZE,
    SYMBOL_ENTRY_SIZE,
    load_coff,
)


class GuardError(Exception):
    """An object or baseline cannot be trusted."""


def _strict_object(path):
    path = Path(path)
    if not path.is_file():
        raise GuardError("missing object: %s" % path)
    try:
        data = path.read_bytes()
        if len(data) < COFF_HEADER_SIZE:
            raise GuardError("object is too small: %s" % path)
        machine, section_count, _stamp, sym_ptr, sym_count, opt_size, _chars = \
            struct.unpack_from(COFF_HEADER_FMT, data, 0)
        if machine != IMAGE_FILE_MACHINE_I386:
            raise GuardError("unexpected COFF machine 0x%04x" % machine)
        section_start = COFF_HEADER_SIZE + opt_size
        section_end = section_start + section_count * SECTION_HEADER_SIZE
        if section_end > len(data):
            raise GuardError("truncated section table")
        if sym_count and (sym_ptr == 0 or sym_ptr + sym_count * SYMBOL_ENTRY_SIZE > len(data)):
            raise GuardError("truncated COFF symbol table")
        string_start = sym_ptr + sym_count * SYMBOL_ENTRY_SIZE
        if string_start + 4 <= len(data):
            string_size = struct.unpack_from("<I", data, string_start)[0]
            if string_size < 4 or string_start + string_size > len(data):
                raise GuardError("malformed COFF string table")
        elif sym_count:
            raise GuardError("missing COFF string table")

        for index in range(section_count):
            off = section_start + index * SECTION_HEADER_SIZE
            raw_name, _vs, _va, raw_size, raw_off, reloc_off, _lo, reloc_count, _ln, _ch = \
                struct.unpack_from(SECTION_HEADER_FMT, data, off)
            if raw_size and (raw_off == 0 or raw_off + raw_size > len(data)):
                raise GuardError("truncated raw data for section %d" % (index + 1))
            if reloc_count and (reloc_off == 0 or reloc_off + reloc_count * RELOC_SIZE > len(data)):
                raise GuardError("truncated relocations for section %d" % (index + 1))
            if raw_name.startswith(b"/"):
                try:
                    int(raw_name.rstrip(b"\0").decode("ascii")[1:])
                except (ValueError, UnicodeDecodeError):
                    raise GuardError("malformed long section name")
        try:
            sections, symbols, string_table = load_coff(str(path))
        except Exception as exc:
            raise GuardError("COFF parse error: %s" % exc)
        if len(sections) != section_count:
            raise GuardError("section count changed while parsing")
        return data, sections, symbols, string_table
    except GuardError:
        raise
    except (OSError, struct.error, IndexError, UnicodeError) as exc:
        raise GuardError("COFF parse error: %s" % exc)


def _section_ids(sections):
    counts = {}
    result = []
    for section in sections:
        ordinal = counts.get(section.name, 0)
        counts[section.name] = ordinal + 1
        result.append("%s#%d" % (section.name, ordinal))
    return result


def _target_identity(symbols, sections, section_ids, index):
    if index < 0 or index >= len(symbols):
        raise GuardError("relocation references missing symbol %d" % index)
    symbol = symbols[index]
    if symbol.section_num > 0:
        section_num = symbol.section_num - 1
        if section_num >= len(sections):
            raise GuardError("symbol references missing section %d" % symbol.section_num)
        return {"name": symbol.name, "section": section_ids[section_num], "value": symbol.value}
    return {"name": symbol.name, "section": symbol.section_num, "value": symbol.value}


def _defined_symbol_data(symbols, sections, section_ids, index):
    """Return the bounded bytes for a defined, non-executable symbol."""
    symbol = symbols[index]
    if symbol.section_num <= 0:
        return None
    section_index = symbol.section_num - 1
    if section_index >= len(sections):
        raise GuardError("symbol references missing section %d" % symbol.section_num)
    section = sections[section_index]
    executable = bool(section.characteristics & 0x20) or section.name.startswith(".text")
    if executable:
        return None
    if symbol.value < 0 or symbol.value >= len(section.data):
        raise GuardError("defined symbol %s has invalid data offset %d" %
                         (symbol.name, symbol.value))
    next_value = len(section.data)
    for other in symbols:
        if (other.section_num == symbol.section_num and
                other.value > symbol.value and other.value < next_value):
            next_value = other.value
    if next_value <= symbol.value or next_value > len(section.data):
        raise GuardError("invalid data bounds for symbol %s" % symbol.name)
    return {
        "section": section_ids[section_index],
        "value": symbol.value,
        "bytes_hex": section.data[symbol.value:next_value].hex(),
    }


def _text_at(section, offset):
    if offset < 0 or offset >= len(section.data):
        return None
    chunk = section.data[offset:offset + 512]
    end = chunk.find(b"\0")
    if end < 0:
        return None
    text = chunk[:end]
    if not text or any(byte < 0x20 or byte > 0x7e for byte in text):
        return None
    return text.decode("ascii")


def capture_object(path):
    raw, sections, symbols, _string_table = _strict_object(path)
    section_ids = _section_ids(sections)
    entries = []
    assertion_metadata = []
    marker = re.compile(r"assert|halt|panic|exception|__file__|line", re.IGNORECASE)
    for section, section_id in zip(sections, section_ids):
        executable = bool(section.characteristics & 0x20) or section.name.startswith(".text")
        relocations = []
        for n in range(section.num_relocs):
            off = section.reloc_offset + n * RELOC_SIZE
            virtual_address, symbol_index, reloc_type = struct.unpack_from("<IIH", raw, off)
            target = _target_identity(symbols, sections, section_ids, symbol_index)
            if executable:
                data_snapshot = _defined_symbol_data(symbols, sections, section_ids, symbol_index)
                if data_snapshot is not None:
                    target["data"] = data_snapshot
            relocations.append({"offset": virtual_address, "type": reloc_type, "target": target})
            if target["section"] not in (0, -1, -2):
                target_section = sections[symbols[symbol_index].section_num - 1]
                text = _text_at(target_section, symbols[symbol_index].value)
                if text is not None and marker.search(text):
                    assertion_metadata.append({
                        "section": section_id,
                        "offset": virtual_address,
                        "type": reloc_type,
                        "target": target,
                        "text": text,
                    })
        entries.append({
            "id": section_id,
            "name": section.name,
            "characteristics": section.characteristics,
            "raw_size": section.raw_size,
            "executable": executable,
            "raw_bytes_hex": section.data.hex() if executable else None,
            "relocations": sorted(relocations, key=lambda item: (item["offset"], item["type"], json.dumps(item["target"], sort_keys=True))),
        })
    return {
        "schema": 1,
        "kind": "coff-candidate-neutrality",
        "sections": entries,
        "assertion_metadata": sorted(assertion_metadata, key=lambda item: json.dumps(item, sort_keys=True)),
    }


def _reloc_sort_key(item):
    return (item["offset"], item["type"], json.dumps(item["target"], sort_keys=True))


def _validate_rename_map(rename_map):
    """A rename map must come from OUTSIDE this module and must be injective.

    Never infer a map by diffing the snapshots. Relocations are patched at link
    time, so retargeting a call from one external symbol to another leaves the
    .text bytes byte-identical, and external symbols all share section 0 /
    value 0 -- an inferred map could not tell that retarget apart from a rename
    and would wave through a genuine change of callee. The caller must supply a
    map it has already verified against the source diff (check_category_purity
    computes exactly such a bijective map for the rename categories).
    """
    if rename_map is None:
        return {}
    if not isinstance(rename_map, dict) or not all(
            isinstance(key, str) and isinstance(value, str)
            for key, value in rename_map.items()):
        raise GuardError("rename map must be a {old_name: new_name} string mapping")
    values = list(rename_map.values())
    if len(set(values)) != len(values):
        raise GuardError("rename map is not injective: two symbols map to one name")
    return rename_map


def _rename_target(target, rename_map):
    new_name = rename_map.get(target.get("name"))
    if new_name is None:
        return target
    return dict(target, name=new_name)


def _canonicalize(snapshot, rename_map):
    """Apply rename_map to the snapshot's symbol names and restore sort order.

    capture_object sorts relocations and assertion metadata by a key that
    includes the target name, so substituting names must be followed by a
    re-sort or an unchanged object would compare as reordered.
    """
    if not rename_map:
        return snapshot
    sections = []
    for section in snapshot["sections"]:
        relocations = [dict(relocation,
                            target=_rename_target(relocation["target"], rename_map))
                       for relocation in section["relocations"]]
        relocations.sort(key=_reloc_sort_key)
        sections.append(dict(section, relocations=relocations))
    metadata = []
    for item in snapshot["assertion_metadata"]:
        if isinstance(item, dict) and isinstance(item.get("target"), dict):
            item = dict(item, target=_rename_target(item["target"], rename_map))
        metadata.append(item)
    metadata.sort(key=lambda entry: json.dumps(entry, sort_keys=True))
    return dict(snapshot, sections=sections, assertion_metadata=metadata)


def compare_snapshots(before, after, rename_map=None):
    """Compare two captures for codegen neutrality.

    rename_map (optional) maps OLD symbol name -> NEW symbol name for renames
    the caller has already verified against the source diff. It excuses a
    difference in symbol NAME only: relocation offset, type, target section and
    target value, the .text bytes, and the assertion metadata must still match
    exactly, so a relocation pointed at a genuinely different symbol still
    fails, as does any name change the map does not account for.

    The map must describe exactly the renames present in the candidate object,
    no more: it is applied to `before`, so naming a rename that did NOT happen
    makes the comparison fail just as a missing entry does. Pass the map that
    check_category_purity derived from the SAME staged diff you are checking --
    not a whole-file map when you are committing one group of it.
    """
    _validate_snapshot(before)
    _validate_snapshot(after)
    before = _canonicalize(before, _validate_rename_map(rename_map))
    errors = []
    before_sections = {section["id"]: section for section in before["sections"]}
    after_sections = {section["id"]: section for section in after["sections"]}
    if set(before_sections) != set(after_sections):
        errors.append("section set changed")
    for section_id in sorted(set(before_sections) & set(after_sections)):
        left, right = before_sections[section_id], after_sections[section_id]
        if left["relocations"] != right["relocations"]:
            errors.append("relocations changed in %s" % section_id)
        if left["executable"] != right["executable"]:
            errors.append("executable classification changed in %s" % section_id)
        if left["executable"] and left["raw_bytes_hex"] != right["raw_bytes_hex"]:
            errors.append("code bytes changed in %s" % section_id)
    if before["assertion_metadata"] != after["assertion_metadata"]:
        errors.append("assertion metadata changed")
    return {"ok": not errors, "errors": errors}


def _validate_snapshot(snapshot):
    if not isinstance(snapshot, dict) or snapshot.get("schema") != 1 or snapshot.get("kind") != "coff-candidate-neutrality":
        raise GuardError("malformed baseline")
    if not isinstance(snapshot.get("sections"), list) or not isinstance(snapshot.get("assertion_metadata"), list):
        raise GuardError("malformed baseline")
    ids = [section.get("id") for section in snapshot["sections"]]
    if any(not isinstance(section, dict) or not section.get("id") for section in snapshot["sections"]):
        raise GuardError("malformed baseline")
    if len(ids) != len(set(ids)):
        raise GuardError("malformed baseline: duplicate section id")
    for section in snapshot["sections"]:
        required = ("relocations", "executable", "raw_bytes_hex")
        if any(key not in section for key in required) or not isinstance(section["relocations"], list):
            raise GuardError("malformed baseline")
        if not isinstance(section["executable"], bool):
            raise GuardError("malformed baseline")
        if section["executable"] and not isinstance(section["raw_bytes_hex"], str):
            raise GuardError("malformed baseline")
        if section["executable"]:
            try:
                bytes.fromhex(section["raw_bytes_hex"])
            except ValueError:
                raise GuardError("malformed baseline")
        for relocation in section["relocations"]:
            if not isinstance(relocation, dict) or not all(key in relocation for key in ("offset", "type", "target")):
                raise GuardError("malformed baseline")
            if not isinstance(relocation["target"], dict) or not all(key in relocation["target"] for key in ("name", "section", "value")):
                raise GuardError("malformed baseline")
            target = relocation["target"]
            if "data" in target:
                data = target["data"]
                if (not isinstance(data, dict) or
                        not all(key in data for key in ("section", "value", "bytes_hex")) or
                        not isinstance(data["bytes_hex"], str)):
                    raise GuardError("malformed baseline")
                try:
                    bytes.fromhex(data["bytes_hex"])
                except ValueError:
                    raise GuardError("malformed baseline")


def _write_json(value, path=None):
    text = json.dumps(value, indent=2, sort_keys=True) + "\n"
    if path:
        Path(path).write_text(text, encoding="ascii")
    else:
        sys.stdout.write(text)


def _self_test():
    base_section = {"id": ".text#0", "name": ".text", "characteristics": 32,
                    "raw_size": 2, "executable": True, "raw_bytes_hex": "9090",
                    "relocations": [{"offset": 0, "type": 6, "target": {"name": "_f", "section": 0, "value": 0}}]}
    base = {"schema": 1, "kind": "coff-candidate-neutrality", "sections": [base_section], "assertion_metadata": []}
    checks = [
        ("identical passes", compare_snapshots(base, json.loads(json.dumps(base)))["ok"]),
        ("one code byte fails", not compare_snapshots(base, dict(base, sections=[dict(base_section, raw_bytes_hex="9190")]))["ok"]),
        ("relocation target change fails", not compare_snapshots(base, dict(base, sections=[dict(base_section, relocations=[dict(base_section["relocations"][0], target={"name": "_g", "section": 0, "value": 0})])]))["ok"]),
    ]

    def variant(target=None, **section_overrides):
        section = dict(base_section, **section_overrides)
        if target is not None:
            section["relocations"] = [dict(base_section["relocations"][0], target=target)]
        return dict(base, sections=[section])

    renamed = variant(target={"name": "_g", "section": 0, "value": 0})
    # A verified rename is excused ...
    checks.append(("rename excused by map passes",
                   compare_snapshots(base, renamed, {"_f": "_g"})["ok"]))
    # ... but only that exact rename: everything else still fails.
    checks.append(("rename to an unmapped name fails",
                   not compare_snapshots(base, variant(target={"name": "_h", "section": 0, "value": 0}),
                                         {"_f": "_g"})["ok"]))
    checks.append(("map does not excuse changed code bytes",
                   not compare_snapshots(base, variant(target={"name": "_g", "section": 0, "value": 0},
                                                       raw_bytes_hex="9190"), {"_f": "_g"})["ok"]))
    checks.append(("map does not excuse a retarget to another address",
                   not compare_snapshots(base, variant(target={"name": "_g", "section": 0, "value": 4}),
                                         {"_f": "_g"})["ok"]))
    checks.append(("an unrelated map leaves the rename failing",
                   not compare_snapshots(base, renamed, {"_x": "_y"})["ok"]))
    try:
        compare_snapshots(base, renamed, {"_f": "_g", "_q": "_g"})
    except GuardError:
        checks.append(("non-injective map rejected", True))
    else:
        checks.append(("non-injective map rejected", False))
    try:
        compare_snapshots(base, renamed, {"_f": 3})
    except GuardError:
        checks.append(("non-string map rejected", True))
    else:
        checks.append(("non-string map rejected", False))
    try:
        compare_snapshots({}, base)
    except GuardError:
        checks.append(("malformed baseline fails", True))
    else:
        checks.append(("malformed baseline fails", False))
    failures = [name for name, passed in checks if not passed]
    for name, passed in checks:
        print("  %s %s" % ("ok  " if passed else "FAIL", name))
    return 1 if failures else 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    sub = parser.add_subparsers(dest="command")
    capture = sub.add_parser("capture")
    capture.add_argument("object")
    capture.add_argument("-o", "--output", required=True)
    check = sub.add_parser("check")
    check.add_argument("baseline")
    check.add_argument("object")
    check.add_argument("--rename-map", default=None,
                       help="path to a JSON {old_name: new_name} map, or inline JSON. "
                            "Excuses symbol-NAME differences only, and only for the "
                            "listed symbols. Supply a map you have verified against "
                            "the source diff -- never one derived from the objects.")
    args = parser.parse_args(argv)
    if args.self_test:
        return _self_test()
    try:
        if args.command == "capture":
            _write_json(capture_object(args.object), args.output)
            return 0
        if args.command == "check":
            with open(args.baseline, "r", encoding="ascii") as stream:
                baseline = json.load(stream)
            rename_map = None
            if args.rename_map:
                if Path(args.rename_map).is_file():
                    with open(args.rename_map, "r", encoding="ascii") as stream:
                        rename_map = json.load(stream)
                else:
                    rename_map = json.loads(args.rename_map)
            result = compare_snapshots(baseline, capture_object(args.object), rename_map)
            _write_json(result)
            return 0 if result["ok"] else 1
        parser.error("a subcommand is required")
    except (OSError, ValueError, GuardError) as exc:
        _write_json({"ok": False, "errors": [str(exc)]})
        return 2


if __name__ == "__main__":
    sys.exit(main())
