#!/usr/bin/env python3
"""Committed struct-recovery evidence tables: validate and render C89 skeletons.

`struct-recovery` used to emit its evidence table as chat text, so the layout
facts behind a struct were not replayable or reviewable. The table is now a
committed JSON artifact under `recovery/evidence/<struct>.json`; this tool is
its schema validator and its deterministic C-skeleton renderer.

    validate <path.json>   schema + consistency checks (non-zero on failure)
    render   <path.json>   print the C89 struct + cs()/co() asserts to stdout
    --self-test            synthetic positive/negative tables for every rule

The rendered C is a STARTING SKELETON for the operator to paste and review.
Nothing here writes to src/ — the artifact is the source of truth, and any
divergence between artifact and placed struct is a defect (fix the artifact and
re-render).
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import sys
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

TOOL_VERSION = "evidence-table/1"
SCHEMA = 1
EVIDENCE_DIR = "recovery/evidence"

TOP_KEYS = frozenset({"schema", "struct", "size", "stride", "fields", "unions",
                      "notes", "sources"})
REQUIRED_TOP = ("schema", "struct", "fields", "sources")
FIELD_KEYS = frozenset({"offset", "width", "signed", "kind", "array_len", "name",
                        "confidence", "evidence", "points_to"})
REQUIRED_FIELD = ("offset", "width", "confidence")
UNION_KEYS = frozenset({"offset", "size", "name", "discriminator", "arms", "evidence"})
REQUIRED_UNION = ("offset", "size", "name", "discriminator", "arms")
ARM_KEYS = frozenset({"name", "width", "signed", "kind", "array_len", "evidence",
                      "points_to"})
REQUIRED_ARM = ("name", "width", "evidence")

KINDS = frozenset({"int", "float", "float64", "pointer"})
CONFIDENCES = frozenset({"named", "typed", "gap"})
WIDTHS = frozenset({1, 2, 4, 8})

IDENT = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
OFFSET = re.compile(r"^0x[0-9a-fA-F]+$")
POINTEE = re.compile(r"^(const\s+)?[A-Za-z_][A-Za-z0-9_]*(\s*\*)*$")

CONCAT_HINT = ("width 8 with kind 'int' is a packed pair, not one integer: "
               "split it into two fields (lift-learnings §13 CONCAT packing) or "
               "set kind='float64' if the evidence proves a double")


class EvidenceError(ValueError):
    """The artifact could not be loaded or is not a table at all."""


# --------------------------------------------------------------------------- #
# helpers
# --------------------------------------------------------------------------- #

def _fmt_off(value: int) -> str:
    """Offset as it appears in comments and co() literals: 0x0A, 0x11C."""
    return "0x%02X" % value


def _name_suffix(value: int) -> str:
    """Offset as it appears in identifiers: field_0a, pad_11c (lowercase)."""
    return "%02x" % value


def _parse_offset(raw: Any) -> int:
    if not isinstance(raw, str) or not OFFSET.match(raw):
        raise ValueError("offset must be a hex string like \"0x1c\", got %r" % (raw,))
    return int(raw, 16)


def _parse_size(raw: Any) -> int:
    if isinstance(raw, bool):
        raise ValueError("size/stride value must be an integer or hex string")
    if isinstance(raw, int):
        return raw
    if isinstance(raw, str) and OFFSET.match(raw):
        return int(raw, 16)
    raise ValueError("size/stride value must be an integer or hex string, got %r" % (raw,))


def _span(entry: dict) -> int:
    """Bytes occupied by a field or union arm."""
    return int(entry["width"]) * int(entry.get("array_len") or 1)


def _unknown(keys: Any, allowed: frozenset, where: str) -> list[str]:
    if not isinstance(keys, dict):
        return ["%s: expected an object, got %s" % (where, type(keys).__name__)]
    extra = sorted(set(keys) - allowed)
    if extra:
        return ["%s: unknown key(s) %s (allowed: %s)"
                % (where, ", ".join(extra), ", ".join(sorted(allowed)))]
    return []


def _nonempty_str(value: Any) -> bool:
    return isinstance(value, str) and value.strip() != ""


# --------------------------------------------------------------------------- #
# validation
# --------------------------------------------------------------------------- #

def _check_widthkind(entry: dict, where: str, errors: list[str],
                     require_signed: bool = True) -> None:
    width = entry.get("width")
    kind = entry.get("kind", "int")
    if isinstance(width, bool) or not isinstance(width, int) or width not in WIDTHS:
        errors.append("%s: width must be one of 1, 2, 4, 8 bytes, got %r" % (where, width))
        return
    if kind not in KINDS:
        errors.append("%s: kind must be one of %s, got %r"
                      % (where, ", ".join(sorted(KINDS)), kind))
        return
    if kind == "int":
        if width == 8:
            errors.append("%s: %s" % (where, CONCAT_HINT))
        elif require_signed and "signed" not in entry:
            errors.append("%s: kind 'int' requires an explicit \"signed\" boolean "
                          "(MOVSX=signed, MOVZX=unsigned; never guess)" % where)
    if "signed" in entry and not isinstance(entry["signed"], bool):
        errors.append("%s: \"signed\" must be a boolean" % where)
    if kind == "float" and width != 4:
        errors.append("%s: kind 'float' requires width 4, got %d" % (where, width))
    if kind == "float64" and width != 8:
        errors.append("%s: kind 'float64' requires width 8, got %d" % (where, width))
    if kind == "pointer" and width != 4:
        errors.append("%s: kind 'pointer' requires width 4 (32-bit Xbox), got %d"
                      % (where, width))
    if "points_to" in entry:
        if kind != "pointer":
            errors.append("%s: \"points_to\" only applies to kind 'pointer'" % where)
        elif not _nonempty_str(entry["points_to"]) or not POINTEE.match(entry["points_to"].strip()):
            errors.append("%s: \"points_to\" must be a simple C type expression, got %r"
                          % (where, entry.get("points_to")))
    length = entry.get("array_len")
    if length is not None:
        if isinstance(length, bool) or not isinstance(length, int) or length < 1:
            errors.append("%s: array_len must be a positive integer, got %r" % (where, length))


def _check_field(field: Any, index: int, errors: list[str]) -> None:
    where = "fields[%d]" % index
    problems = _unknown(field, FIELD_KEYS, where)
    if problems:
        errors.extend(problems)
        return
    missing = [key for key in REQUIRED_FIELD if key not in field]
    if missing:
        errors.append("%s: missing required key(s) %s" % (where, ", ".join(missing)))
    if "offset" in field:
        try:
            where = "fields[%d] (offset=%s)" % (index, field["offset"])
            _parse_offset(field["offset"])
        except ValueError as exc:
            errors.append("fields[%d]: %s" % (index, exc))
    if "width" in field:
        # A gap renders as uint8_t pad_XX[n], so its signedness is meaningless.
        _check_widthkind(field, where, errors,
                         require_signed=field.get("confidence") != "gap")

    confidence = field.get("confidence")
    if confidence is not None and confidence not in CONFIDENCES:
        errors.append("%s: confidence must be one of %s, got %r"
                      % (where, ", ".join(sorted(CONFIDENCES)), confidence))
    name = field.get("name")
    if name is not None and (not _nonempty_str(name) or not IDENT.match(name)):
        errors.append("%s: name must be a C identifier, got %r" % (where, name))
    evidence = field.get("evidence")
    if confidence in ("named", "typed") and not _nonempty_str(evidence):
        errors.append("%s: confidence '%s' requires a one-line \"evidence\" citation"
                      % (where, confidence))
    if confidence == "named" and not _nonempty_str(name):
        errors.append("%s: confidence 'named' requires a \"name\" (string/PDB evidence); "
                      "drop to 'typed' for a mechanical name or omit the name entirely"
                      % where)
    if confidence == "gap" and _nonempty_str(name):
        errors.append("%s: confidence 'gap' must not carry a name (%r) - a gap renders as "
                      "pad_XX; name it and raise the confidence, or leave it unnamed"
                      % (where, name))
    if evidence is not None and not _nonempty_str(evidence):
        errors.append("%s: \"evidence\" must be a non-empty one-line citation" % where)
    if _nonempty_str(evidence) and "\n" in evidence:
        errors.append("%s: \"evidence\" must be a single line" % where)


def _check_union(union: Any, index: int, errors: list[str]) -> None:
    where = "unions[%d]" % index
    problems = _unknown(union, UNION_KEYS, where)
    if problems:
        errors.extend(problems)
        return
    missing = [key for key in REQUIRED_UNION if key not in union]
    if missing:
        errors.append("%s: missing required key(s) %s" % (where, ", ".join(missing)))
        return
    where = "unions[%d] (%s)" % (index, union["name"])
    try:
        _parse_offset(union["offset"])
    except ValueError as exc:
        errors.append("%s: %s" % (where, exc))
    if not _nonempty_str(union["name"]) or not IDENT.match(union["name"]):
        errors.append("%s: union member name must be a C identifier" % where)
    if not _nonempty_str(union["discriminator"]):
        errors.append("%s: \"discriminator\" is required - a union without a proven "
                      "discriminator is a guess" % where)
    try:
        size = _parse_size(union["size"])
    except ValueError as exc:
        errors.append("%s: %s" % (where, exc))
        return
    arms = union["arms"]
    if not isinstance(arms, list) or len(arms) < 2:
        errors.append("%s: a union needs at least 2 arms (both observed types)" % where)
        return
    seen: set[str] = set()
    spans: list[int] = []
    aligns: list[int] = []
    for position, arm in enumerate(arms):
        arm_where = "%s arms[%d]" % (where, position)
        problems = _unknown(arm, ARM_KEYS, arm_where)
        if problems:
            errors.extend(problems)
            continue
        arm_missing = [key for key in REQUIRED_ARM if key not in arm]
        if arm_missing:
            errors.append("%s: missing required key(s) %s" % (arm_where, ", ".join(arm_missing)))
            continue
        if not _nonempty_str(arm["name"]) or not IDENT.match(arm["name"]):
            errors.append("%s: arm name must be a C identifier, got %r" % (arm_where, arm["name"]))
        elif arm["name"] in seen:
            errors.append("%s: duplicate arm name %r" % (arm_where, arm["name"]))
        else:
            seen.add(arm["name"])
        if not _nonempty_str(arm["evidence"]):
            errors.append("%s: every arm needs its own \"evidence\" citation" % arm_where)
        before = len(errors)
        _check_widthkind(arm, arm_where, errors)
        if len(errors) == before:
            spans.append(_span(arm))
            aligns.append(int(arm["width"]))
            if _span(arm) > size:
                errors.append("%s: arm spans %d bytes but the union footprint is %d"
                              % (arm_where, _span(arm), size))
    if spans:
        align = max(aligns)
        natural = ((max(spans) + align - 1) // align) * align
        if natural != size:
            errors.append("%s: declared footprint %d != the C sizeof of the widest arm "
                          "(%d, largest arm %d aligned to %d). Add an explicit "
                          "uint8_t raw[N] arm if the evidence proves a bigger region."
                          % (where, size, natural, max(spans), align))


def _check_layout(table: dict, errors: list[str], warnings: list[str]) -> None:
    """Ordering, overlap, uniqueness and size containment across the merged layout.

    Only reached once every entry has passed its own checks, so offsets, widths
    and union footprints are known-parseable here.
    """
    entries: list[dict] = []
    names: dict[str, str] = {}
    for index, field in enumerate(table.get("fields") or []):
        entries.append({"offset": _parse_offset(field["offset"]), "span": _span(field),
                        "align": min(int(field["width"]), 4),
                        "label": "fields[%d]" % index, "member": _member_name(field)})
    for index, union in enumerate(table.get("unions") or []):
        entries.append({"offset": _parse_offset(union["offset"]),
                        "span": _parse_size(union["size"]),
                        "align": min(max(int(arm["width"]) for arm in union["arms"]), 4),
                        "label": "unions[%d]" % index, "member": union["name"]})

    for entry in entries:
        if entry["member"] in names:
            errors.append("%s: duplicate member name %r (also %s)"
                          % (entry["label"], entry["member"], names[entry["member"]]))
        else:
            names[entry["member"]] = entry["label"]

    ordered = sorted(entries, key=lambda item: (item["offset"], item["label"]))
    if [item["label"] for item in ordered] != [item["label"] for item in entries]:
        errors.append("layout: entries must be listed in strictly increasing offset order "
                      "(fields and unions are merged by offset; reorder the artifact)")
    previous: dict | None = None
    for entry in ordered:
        if previous is not None:
            if entry["offset"] == previous["offset"]:
                errors.append("%s: offset %s duplicates %s - offsets must be strictly "
                              "increasing (model a real overlay as a union)"
                              % (entry["label"], _fmt_off(entry["offset"]), previous["label"]))
            elif entry["offset"] < previous["offset"] + previous["span"]:
                errors.append("%s: offset %s overlaps %s which spans %s..%s"
                              % (entry["label"], _fmt_off(entry["offset"]), previous["label"],
                                 _fmt_off(previous["offset"]),
                                 _fmt_off(previous["offset"] + previous["span"] - 1)))
        previous = entry
        if entry["align"] > 1 and entry["offset"] % entry["align"]:
            warnings.append("%s: offset %s is not %d-byte aligned - if that is real the "
                            "struct needs #pragma pack and a loud note"
                            % (entry["label"], _fmt_off(entry["offset"]), entry["align"]))

    if "size" in table and ordered:
        size = _parse_size(table["size"]["value"])
        last = ordered[-1]
        end = last["offset"] + last["span"]
        if end > size:
            errors.append("%s: ends at %s which is past the declared size %s"
                          % (last["label"], _fmt_off(end), _fmt_off(size)))
        widest = max(item["align"] for item in ordered)
        if widest > 1 and size % widest:
            warnings.append("size %s is not a multiple of the widest member (%d bytes); "
                            "cs() will fail unless the struct is packed"
                            % (_fmt_off(size), widest))


def validate(table: Any) -> tuple[list[str], list[str]]:
    """Return (errors, warnings). An empty error list means the table is usable."""
    errors: list[str] = []
    warnings: list[str] = []
    if not isinstance(table, dict):
        return (["top level: expected a JSON object, got %s" % type(table).__name__], [])

    errors.extend(_unknown(table, TOP_KEYS, "top level"))
    for key in REQUIRED_TOP:
        if key not in table:
            errors.append("top level: missing required key %r" % key)

    if table.get("schema") != SCHEMA:
        errors.append("top level: \"schema\" must be %d, got %r" % (SCHEMA, table.get("schema")))
    name = table.get("struct")
    if not _nonempty_str(name) or not IDENT.match(name):
        errors.append("top level: \"struct\" must be a C identifier (a real name or "
                      "unknown_<addr>), got %r" % (name,))

    for key in ("size", "stride"):
        if key not in table:
            continue
        blob = table[key]
        if not isinstance(blob, dict) or set(blob) != {"value", "evidence"}:
            errors.append("%s: must be an object with exactly \"value\" and \"evidence\"" % key)
            continue
        try:
            value = _parse_size(blob["value"])
            if value <= 0:
                errors.append("%s: value must be positive" % key)
        except ValueError as exc:
            errors.append("%s: %s" % (key, exc))
        if not _nonempty_str(blob["evidence"]):
            errors.append("%s: requires an \"evidence\" citation (memset length, pool "
                          "element size, IMUL stride, assert string, ...)" % key)

    fields = table.get("fields")
    if not isinstance(fields, list) or not fields:
        errors.append("\"fields\": must be a non-empty list")
    else:
        for index, field in enumerate(fields):
            _check_field(field, index, errors)

    unions = table.get("unions")
    if unions is not None:
        if not isinstance(unions, list):
            errors.append("\"unions\": must be a list")
        else:
            for index, union in enumerate(unions):
                _check_union(union, index, errors)

    notes = table.get("notes")
    if notes is not None and (not isinstance(notes, list)
                              or not all(_nonempty_str(item) for item in notes)):
        errors.append("\"notes\": must be a list of non-empty strings")

    sources = table.get("sources")
    if sources is not None and (not isinstance(sources, list) or not sources
                                or not all(_nonempty_str(item) for item in sources)):
        errors.append("\"sources\": must be a non-empty list of provenance strings "
                      "(functions/addresses consulted)")

    if not errors:
        _check_layout(table, errors, warnings)
    if "size" not in table:
        warnings.append("no \"size\" evidence: render will omit cs() and emit the "
                        "\"size unproven\" comment instead")
    return (errors, warnings)


# --------------------------------------------------------------------------- #
# rendering
# --------------------------------------------------------------------------- #

def _member_name(field: dict) -> str:
    if _nonempty_str(field.get("name")):
        return field["name"]
    suffix = _name_suffix(_parse_offset(field["offset"]))
    if field.get("confidence") == "gap":
        return "pad_%s" % suffix
    return "field_%s" % suffix


def _type_name(entry: dict) -> str:
    kind = entry.get("kind", "int")
    width = int(entry["width"])
    if kind == "float":
        return "float"
    if kind == "float64":
        return "double"
    return ("int%d_t" if entry.get("signed") else "uint%d_t") % (width * 8)


def _declaration(entry: dict, member: str) -> str:
    length = entry.get("array_len")
    suffix = "[%d]" % int(length) if length else ""
    if entry.get("kind") == "pointer":
        return "%s *%s%s" % (entry.get("points_to", "void").strip(), member, suffix)
    return "%s %s%s" % (_type_name(entry), member, suffix)


def _pad_declaration(offset: int, span: int) -> str:
    return "uint8_t pad_%s[%d]" % (_name_suffix(offset), span)


def _members(table: dict) -> list[dict]:
    """Merged, offset-ordered render plan: declared entries plus derived padding."""
    plan: list[dict] = []
    for field in table.get("fields") or []:
        offset = _parse_offset(field["offset"])
        member = _member_name(field)
        gap = field.get("confidence") == "gap"
        plan.append({
            "offset": offset,
            "span": _span(field),
            "member": member,
            "decl": _pad_declaration(offset, _span(field)) if gap else _declaration(field, member),
            "comment": field.get("evidence") or ("declared padding" if gap else ""),
            "assert": True,
            "union": None,
        })
    for union in table.get("unions") or []:
        offset = _parse_offset(union["offset"])
        plan.append({
            "offset": offset,
            "span": _parse_size(union["size"]),
            "member": union["name"],
            "decl": None,
            "comment": union.get("evidence") or "",
            "assert": True,
            "union": union,
        })
    plan.sort(key=lambda item: item["offset"])

    filled: list[dict] = []
    cursor = 0
    for item in plan:
        if item["offset"] > cursor:
            span = item["offset"] - cursor
            filled.append({
                "offset": cursor,
                "span": span,
                "member": "pad_%s" % _name_suffix(cursor),
                "decl": _pad_declaration(cursor, span),
                "comment": "gap - never observed accessed",
                "assert": False,
                "union": None,
            })
        filled.append(item)
        cursor = item["offset"] + item["span"]
    if "size" in table:
        size = _parse_size(table["size"]["value"])
        if size > cursor:
            filled.append({
                "offset": cursor,
                "span": size - cursor,
                "member": "pad_%s" % _name_suffix(cursor),
                "decl": _pad_declaration(cursor, size - cursor),
                "comment": "trailing gap - never observed accessed",
                "assert": False,
                "union": None,
            })
    return filled


def _comment(offset: int, text: str) -> str:
    body = "///< offset=%s" % _fmt_off(offset)
    if text:
        body += "  %s" % text
    return body


def _render_union(union: dict, offset: int, indent: str) -> list[str]:
    arms = []
    for arm in union["arms"]:
        arms.append((_declaration(arm, arm["name"]) + ";", arm["evidence"]))
    width = max(len(decl) for decl, _evidence in arms)
    lines = ["%sunion {  /* discriminator: %s */" % (indent, union["discriminator"])]
    for decl, evidence in arms:
        lines.append("%s    %-*s %s" % (indent, width, decl, _comment(offset, evidence)))
    lines.append("%s}" % indent)
    return lines


def render(table: dict) -> str:
    """Deterministic C89 skeleton: same table in, byte-identical text out."""
    name = table["struct"]
    members = _members(table)
    out: list[str] = []

    if "size" in table:
        out.append("/// size=%s  (%s)" % (_fmt_off(_parse_size(table["size"]["value"])),
                                          table["size"]["evidence"]))
    if "stride" in table:
        out.append("/// stride=%s  (%s)" % (_fmt_off(_parse_size(table["stride"]["value"])),
                                            table["stride"]["evidence"]))
    out.append("/// Recovered layout - evidence artifact: %s/%s.json" % (EVIDENCE_DIR, name))
    for note in table.get("notes") or []:
        out.append("/// %s" % note)
    for source in table.get("sources") or []:
        out.append("/// evidence: %s" % source)

    simple = [item for item in members if item["decl"] is not None]
    width = max((len(item["decl"]) + 1 for item in simple), default=1)
    out.append("typedef struct %s {" % name)
    for item in members:
        if item["union"] is not None:
            out.extend(_render_union(item["union"], item["offset"], "    "))
            out.append("    %-*s %s" % (width, "} %s;" % item["member"],
                                        _comment(item["offset"], item["comment"])))
        else:
            out.append("    %-*s %s" % (width, item["decl"] + ";",
                                        _comment(item["offset"], item["comment"])))
    out.append("} %s;" % name)

    if "size" in table:
        out.append("cs(%s, %s);" % (name, _fmt_off(_parse_size(table["size"]["value"]))))
    else:
        largest = max(item["offset"] + item["span"] for item in members)
        out.append("/* size unproven - largest observed access at %s */" % _fmt_off(largest))
    for item in members:
        if item["assert"]:
            out.append("co(%s, %s, %s);" % (name, item["member"], _fmt_off(item["offset"])))
    return "\n".join(out) + "\n"


# --------------------------------------------------------------------------- #
# CLI
# --------------------------------------------------------------------------- #

def load(path: str | Path) -> dict:
    target = Path(path)
    try:
        with target.open(encoding="utf-8") as stream:
            table = json.load(stream)
    except FileNotFoundError:
        raise EvidenceError("no such evidence table: %s" % target)
    except json.JSONDecodeError as exc:
        raise EvidenceError("%s: invalid JSON: %s" % (target, exc))
    if not isinstance(table, dict):
        raise EvidenceError("%s: expected a JSON object at the top level" % target)
    return table


def _report(path: str, errors: list[str], warnings: list[str]) -> int:
    for warning in warnings:
        print("WARN  %s: %s" % (path, warning))
    for error in errors:
        print("ERROR %s: %s" % (path, error), file=sys.stderr)
    if errors:
        print("%s: %d error(s)" % (path, len(errors)), file=sys.stderr)
        return 1
    print("ok    %s" % path)
    return 0


def _self_test() -> int:
    def valid(table: dict) -> bool:
        return not validate(table)[0]

    def fails(table: dict, needle: str) -> bool:
        errors = validate(table)[0]
        return any(needle in error for error in errors)

    base = {
        "schema": 1,
        "struct": "sample_t",
        "size": {"value": "0x10", "evidence": "csmemset 0x10 at FUN_00001000"},
        "sources": ["FUN_00001000 init site"],
        "fields": [
            {"offset": "0x00", "width": 2, "signed": True, "name": "count",
             "confidence": "named", "evidence": "assert \"count>=0\""},
            {"offset": "0x04", "width": 4, "kind": "float", "confidence": "typed",
             "evidence": "FLD [esi+4] @FUN_00001010"},
        ],
    }

    def mutate(**changes: Any) -> dict:
        table = json.loads(json.dumps(base))
        table.update(changes)
        return table

    def with_fields(*fields: dict) -> dict:
        return mutate(fields=[json.loads(json.dumps(f)) for f in fields])

    named = base["fields"][0]
    union_table = mutate(fields=[named], unions=[{
        "offset": "0x04", "size": 4, "name": "value",
        "discriminator": "type at 0x00",
        "arms": [
            {"name": "as_index", "width": 4, "signed": True, "evidence": "MOV path A"},
            {"name": "as_scale", "width": 4, "kind": "float", "evidence": "FLD path B"},
        ],
    }])

    rendered = render(base)
    unproven = mutate()
    del unproven["size"]

    checks = [
        (TOOL_VERSION.startswith("evidence-table/"), "versioned tool"),
        (valid(base), "minimal table validates"),
        (valid(union_table), "union table validates"),
        (valid(unproven), "size is optional"),
        (fails({"schema": 2, "struct": "x", "fields": [], "sources": ["s"]}, "\"schema\" must be 1"),
         "schema version pinned"),
        (fails(mutate(struct="not a name"), "must be a C identifier"), "struct name is an identifier"),
        (fails(mutate(bogus=1), "unknown key(s) bogus"), "unknown top-level key rejected"),
        (fails({"schema": 1, "struct": "x", "fields": base["fields"]}, "missing required key 'sources'"),
         "sources required"),
        (fails(mutate(sources=[]), "non-empty list of provenance"), "empty sources rejected"),
        (fails(mutate(size={"value": "0x10"}), "exactly \"value\" and \"evidence\""),
         "size needs evidence"),
        (fails(mutate(fields=[]), "non-empty list"), "at least one field"),
        (fails(with_fields({"offset": 4, "width": 4, "signed": False, "confidence": "typed",
                            "evidence": "e"}), "offset must be a hex string"),
         "integer offset rejected"),
        (fails(with_fields({"offset": "0x00", "width": 3, "signed": False,
                            "confidence": "typed", "evidence": "e"}), "width must be one of"),
         "odd width rejected"),
        (fails(with_fields({"offset": "0x00", "width": 4, "confidence": "typed", "evidence": "e"}),
                "requires an explicit \"signed\""), "signedness must be explicit for ints"),
        (fails(with_fields({"offset": "0x00", "width": 8, "signed": False,
                            "confidence": "typed", "evidence": "e"}), "CONCAT packing"),
         "width 8 int points at lift-learnings §13"),
        (valid(with_fields({"offset": "0x00", "width": 8, "kind": "float64",
                            "confidence": "typed", "evidence": "FLD qword"})),
         "width 8 allowed with kind float64"),
        (fails(with_fields({"offset": "0x00", "width": 2, "kind": "float",
                            "confidence": "typed", "evidence": "e"}), "kind 'float' requires width 4"),
         "float must be 4 bytes"),
        (fails(with_fields({"offset": "0x00", "width": 2, "kind": "pointer",
                            "confidence": "typed", "evidence": "e"}), "kind 'pointer' requires width 4"),
         "pointer must be 4 bytes"),
        (fails(with_fields({"offset": "0x00", "width": 4, "signed": False,
                            "confidence": "named", "evidence": "e"}), "requires a \"name\""),
         "named confidence requires a name"),
        (fails(with_fields({"offset": "0x00", "width": 4, "signed": False,
                            "confidence": "named", "name": "n"}), "requires a one-line \"evidence\""),
         "named confidence requires evidence"),
        (fails(with_fields({"offset": "0x00", "width": 4, "signed": False,
                            "confidence": "typed", "name": "n"}), "requires a one-line \"evidence\""),
         "typed confidence requires evidence"),
        (fails(with_fields({"offset": "0x00", "width": 4, "confidence": "gap", "name": "junk"}),
               "must not carry a name"), "named gap rejected"),
        (valid(with_fields({"offset": "0x00", "width": 4, "confidence": "gap"})),
         "unnamed gap needs no evidence"),
        (fails(with_fields(named, {"offset": "0x01", "width": 2, "signed": False,
                                   "confidence": "typed", "evidence": "e"}), "overlaps"),
         "overlapping fields rejected"),
        (fails(with_fields(named, {"offset": "0x00", "width": 2, "signed": False,
                                   "confidence": "typed", "evidence": "e"}), "strictly "),
         "duplicate offset rejected"),
        (fails(with_fields({"offset": "0x08", "width": 4, "signed": False, "confidence": "typed",
                            "evidence": "e"}, named), "increasing offset order"),
         "out-of-order fields rejected"),
        (fails(with_fields(named, {"offset": "0x04", "width": 4, "array_len": 4, "signed": False,
                                   "confidence": "typed", "evidence": "e"}),
               "past the declared size"), "last field must fit the declared size"),
        (valid(with_fields(named, {"offset": "0x04", "width": 4, "array_len": 3, "signed": False,
                                   "confidence": "typed", "evidence": "e"})),
         "array field fits exactly"),
        (fails(with_fields(named, {"offset": "0x04", "width": 4, "array_len": 0, "signed": False,
                                   "confidence": "typed", "evidence": "e"}),
               "array_len must be a positive integer"), "zero array_len rejected"),
        (fails(with_fields({"offset": "0x00", "width": 2, "signed": True, "name": "count",
                            "confidence": "named", "evidence": "e"},
                           {"offset": "0x04", "width": 2, "signed": True, "name": "count",
                            "confidence": "named", "evidence": "e"}), "duplicate member name"),
         "duplicate member name rejected"),
        (fails(mutate(fields=[named], unions=[dict(union_table["unions"][0], arms=[
            union_table["unions"][0]["arms"][0]])]), "at least 2 arms"),
         "single-arm union rejected"),
        (fails(mutate(fields=[named], unions=[dict(union_table["unions"][0], discriminator="")]),
               "\"discriminator\" is required"), "union needs a discriminator"),
        (fails(mutate(fields=[named], unions=[dict(union_table["unions"][0], size=8)]),
               "declared footprint 8 != the C sizeof"), "union footprint must match its arms"),
        (fails(mutate(fields=[named], unions=[dict(union_table["unions"][0], offset="0x00")]),
               "duplicates"), "union overlapping a field rejected"),
        (rendered == render(json.loads(json.dumps(base))), "render is deterministic"),
        ("cs(sample_t, 0x10);" in rendered, "cs() emitted when size is proven"),
        ("co(sample_t, count, 0x00);" in rendered, "co() emitted per field"),
        ("uint8_t pad_02[2];" in rendered, "implicit gap rendered as explicit padding"),
        ("uint8_t pad_08[8];" in rendered, "trailing gap rendered up to the declared size"),
        ("co(sample_t, pad_02, " not in rendered, "derived padding carries no co()"),
        ("field_04" in rendered, "unnamed field renders as field_<offset>"),
        ("size unproven - largest observed access at 0x08" in render(unproven),
         "size-unproven comment replaces cs()"),
        ("union {  /* discriminator: type at 0x00 */" in render(union_table),
         "union renders C89 named member with discriminator"),
        ("co(sample_t, value, 0x04);" in render(union_table), "union offset asserted"),
        (any("size unproven" in warning for warning in validate(unproven)[1]),
         "missing size warns"),
        (any("pragma pack" in warning for warning in validate(with_fields(
            {"offset": "0x01", "width": 4, "signed": False, "confidence": "typed",
             "evidence": "e"}))[1]),
         "misaligned field warns about packing"),
    ]
    for passed, label in checks:
        print("  %s %s" % ("ok  " if passed else "FAIL", label))
    failures = [label for passed, label in checks if not passed]
    if failures:
        print("%d self-test failure(s)" % len(failures), file=sys.stderr)
        return 1
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--self-test", action="store_true")
    sub = parser.add_subparsers(dest="command")
    check = sub.add_parser("validate", help="schema + consistency checks")
    check.add_argument("path")
    draw = sub.add_parser("render", help="print the C89 struct skeleton")
    draw.add_argument("path")
    args = parser.parse_args(argv)

    if args.self_test:
        return _self_test()
    if args.command is None:
        parser.error("a subcommand is required")
    try:
        table = load(args.path)
    except EvidenceError as exc:
        print("ERROR %s" % exc, file=sys.stderr)
        return 2
    errors, warnings = validate(table)
    if args.command == "validate":
        return _report(args.path, errors, warnings)
    if errors:
        print("ERROR %s: refusing to render an invalid table" % args.path, file=sys.stderr)
        _report(args.path, errors, [])
        return 1
    sys.stdout.write(render(table))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
