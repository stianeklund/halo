#!/usr/bin/env python3
"""Mechanical struct refinement and offset->field rewriting.

The readability ladder's `struct-define` and `offset-to-field` rungs, done by a
program instead of by hand.  The split of responsibility is deliberate:

    a human/LLM decides ONE fact  ->  recovery/bindings.json  (small, reviewed)
    this tool applies that fact   ->  N source edits          (mechanical)
    the COFF guard proves it      ->  byte-identical .text    (or it is rejected)

Nothing here guesses.  Every offset is looked up in the authoritative record
layout emitted by the project's own compiler; an offset that does not resolve is
REFUSED with a reason, never approximated.  The refusals are the output that
matters most -- they are the ranked evidence worklist for human RE.

Why the compiler is the oracle for layout: hand-parsing C is how struct offsets
get silently wrong.  `clang -Xclang -fdump-record-layouts` reports what the
build actually lays out, including `#pragma pack(1)`.

Why proposing a field width is safe: the byte-identical gate arbitrates.  If a
proposed `field_34` is typed `int32_t` where the code needs `uint32_t`, the
generated code differs and the gate rejects the change.  The tool proposes; the
binary decides.

Subcommands (all emit JSON on stdout unless noted):

    layout   <struct>                 authoritative field table
    census   --source F --binding B   classify every deref site
    split    --census C [--apply]     pad_ runs -> field_XX at observed offsets
    rewrite  --census C [--apply]     *(T*)(p+0xNN) -> alias->field
    gate     --source F               build the TU and compare .text to baseline
    status   --census C               one-screen summary
"""

import argparse
import json
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

TOOL_VERSION = 1
SCHEMA = 1

TYPES_H = ROOT / "src" / "types.h"
BINDINGS = ROOT / "recovery" / "bindings.json"

# Mirrors CMakeLists.txt:99.  Layout depends on target/packing, so these must
# track the real build; a mismatch here silently produces wrong offsets.
CLANG_FLAGS = [
    "-target", "i386-pc-win32",
    "-march=pentium3",
    "-mno-sse",
    "-nostdlib",
    "-ffreestanding",
    "-fno-builtin",
    "-I", str(ROOT / "src"),
    "-I", str(ROOT / "third_party" / "xbox"),
    "-I", str(ROOT / "build" / "generated"),
    "-include", str(ROOT / "src" / "common.h"),
]


class StructizeError(Exception):
    """Expected, fail-closed workflow error."""


# --------------------------------------------------------------------------
# Single-TU compile + function-level object diff
#
# Rewriting a whole file and asking "did anything change?" is the wrong
# granularity: a single divergent function condemns 118 good ones.  We compile
# the TU once before and once after, diff at FUNCTION granularity, and revert
# only the functions that actually moved.  One extra compile, not one per
# function.
#
# Observed on actor_looking.c: 117/119 functions are byte-identical; two
# diverge because `#pragma pack(1)` gives a struct member alignment 1 while the
# original `*(int *)(actor + 0x340)` cast asserted alignment 4, and at -O3 that
# changes load scheduling.  Not an aliasing effect -- `-fno-strict-aliasing`
# makes no difference.
# --------------------------------------------------------------------------
FLAGS_MAKE = ROOT / "build" / "CMakeFiles" / "halo.dir" / "flags.make"


def build_flags():
    """The real compile flags, read from the build system rather than guessed.

    Hardcoding flags here would silently diverge from the build (the first
    attempt at this missed -O3 and concluded, wrongly, that nothing changed).
    """
    if not FLAGS_MAKE.is_file():
        raise StructizeError("no flags.make at %s -- configure/build first" % FLAGS_MAKE)
    flags, includes = "", ""
    for line in FLAGS_MAKE.read_text(encoding="utf-8").splitlines():
        if line.startswith("C_FLAGS ="):
            flags = line.split("=", 1)[1].strip()
        elif line.startswith("C_INCLUDES ="):
            includes = line.split("=", 1)[1].strip()
    if not flags:
        raise StructizeError("could not read C_FLAGS from %s" % FLAGS_MAKE)
    return (flags + " " + includes).split()


def compile_tu(source, out_obj, extra=()):
    """Compile one translation unit with the project's real flags."""
    source = Path(source)
    cmd = [shutil.which("clang") or "clang"]
    cmd += build_flags()
    # Quoted includes in the source are relative to its own directory.
    cmd += ["-I", str(source.parent)]
    cmd += list(extra) + ["-c", str(source), "-o", str(out_obj)]
    proc = subprocess.run(cmd, capture_output=True, text=True, cwd=str(ROOT))
    if proc.returncode != 0:
        raise StructizeError("compile failed for %s:\n%s"
                             % (source, (proc.stderr or "").strip()[:2000]))
    return out_obj


_SYM_LINE = re.compile(r"^[0-9a-f]+ <(.+?)>:")
_INSN_LINE = re.compile(r"^\s*[0-9a-f]+:\s*(.*?)\s*$")


def function_bodies(obj_path):
    """Normalized instruction list per function symbol.

    Branch/call targets and symbol comments are normalized away so that a pure
    address shift (caused by an earlier function changing size) is not mistaken
    for a real difference in a later one.
    """
    proc = subprocess.run(["objdump", "-d", "--no-show-raw-insn", str(obj_path)],
                          capture_output=True, text=True)
    if proc.returncode != 0:
        raise StructizeError("objdump failed on %s" % obj_path)
    bodies, current = {}, None
    for line in proc.stdout.splitlines():
        match = _SYM_LINE.match(line)
        if match:
            current = match.group(1).lstrip("_")
            bodies[current] = []
            continue
        match = _INSN_LINE.match(line)
        if match and current is not None:
            insn = re.sub(r"<[^>]*>", "", match.group(1))
            insn = re.sub(r"^(j\w+|call)\s+[0-9a-f]+", r"\1 T", insn)
            bodies[current].append(insn.strip())
    return bodies


def diverging_functions(before_obj, after_obj):
    """Function symbols whose code genuinely changed between two objects."""
    before, after = function_bodies(before_obj), function_bodies(after_obj)
    changed = sorted(name for name in before
                     if name in after and before[name] != after[name])
    appeared = sorted(set(after) - set(before))
    vanished = sorted(set(before) - set(after))
    return {"changed": changed, "appeared": appeared, "vanished": vanished,
            "total": len(before), "identical": len(before) - len(changed)}


# --------------------------------------------------------------------------
# Scalar type model
#
# `signed` is tri-state: True/False for integers, None for float and for
# `char` (whose signedness is implementation-defined and which the codebase
# uses as a raw byte).  A rewrite requires the cast and the field to agree on
# all three of kind/width/signed, because MOVSX and MOVZX are different
# instructions and int and float live in different register files.
# --------------------------------------------------------------------------
SCALARS = {
    "char": ("int", 1, None),
    "signed char": ("int", 1, True),
    "unsigned char": ("int", 1, False),
    "int8_t": ("int", 1, True),
    "uint8_t": ("int", 1, False),
    "bool": ("int", 1, False),
    "short": ("int", 2, True),
    "short int": ("int", 2, True),
    "unsigned short": ("int", 2, False),
    "int16_t": ("int", 2, True),
    "uint16_t": ("int", 2, False),
    "int": ("int", 4, True),
    "unsigned": ("int", 4, False),
    "unsigned int": ("int", 4, False),
    "long": ("int", 4, True),
    "unsigned long": ("int", 4, False),
    "int32_t": ("int", 4, True),
    "uint32_t": ("int", 4, False),
    "float": ("float", 4, None),
    "real": ("float", 4, None),
    "double": ("float", 8, None),
    "int64_t": ("int", 8, True),
    "uint64_t": ("int", 8, False),
}

# Canonical spelling emitted for a newly split field, keyed by (width, signed).
# Matches the house style already in src/types.h.
FIELD_SPELLING = {
    (1, None): "char",
    (1, True): "int8_t",
    (1, False): "uint8_t",
    (2, True): "int16_t",
    (2, False): "uint16_t",
    (4, True): "int32_t",
    (4, False): "uint32_t",
}


def scalar_of(text):
    """Parse a cast's type text into (kind, width, signed), or None.

    Pointers are deliberately NOT scalars here: `*(foo_t **)(p+N)` is a
    pointer-typed field and gets the same treatment as any 4-byte int only if
    the struct declares it that way, which the layout dump reports as a
    pointer.  Returning None routes it to a refusal instead of a bad match.
    """
    cleaned = " ".join(text.replace("const", " ").split())
    if not cleaned or "*" in cleaned:
        return None
    return SCALARS.get(cleaned)


def hex_width(struct_size):
    """Digits to use for offset-derived names.

    CLAUDE.md requires a minimum of 2; we widen to cover the struct so every
    name in one struct lines up (actor_t is 0x724, so `pad_002`/`field_018`).
    """
    return max(2, len("%x" % max(int(struct_size or 0) - 1, 0)))


def field_name_for(offset, width=2):
    """Canonical unknown-field name: field_<lowercase hex>, min 2 digits."""
    return "field_%0*x" % (width, offset)


def pad_name_for(offset, width=2):
    return "pad_%0*x" % (width, offset)


def _describe(entry):
    """Human-readable type of a proposed field, including signedness."""
    bits = entry["width"] * 8
    if entry["kind"] == "float":
        return "float%d" % bits
    if entry["signed"] is None:
        return "char%d" % bits
    return "%sint%d" % ("" if entry["signed"] else "u", bits)


# --------------------------------------------------------------------------
# Authoritative layout, straight from the compiler
# --------------------------------------------------------------------------
_LAYOUT_ROW = re.compile(r"^\s*(\d+)\s*\|\s+(.+?)\s+([A-Za-z_]\w*)\s*$", re.M)


def struct_layout(struct_name, clang=None):
    """Return the compiler's field table for `struct_name`.

    Each entry: offset -> dict(name, type, kind, width, count, is_pad, span).
    `span` is the total byte span of the field including array extent, so a
    caller can ask which field an arbitrary interior offset belongs to.
    """
    clang = clang or shutil.which("clang") or "clang"
    with tempfile.TemporaryDirectory() as tmp:
        probe = Path(tmp) / "probe.c"
        probe.write_text('#include "types.h"\n%s _structize_probe;\n' % struct_name,
                         encoding="ascii")
        cmd = [clang, "-fsyntax-only", "-Xclang", "-fdump-record-layouts"]
        cmd += CLANG_FLAGS + [str(probe)]
        try:
            proc = subprocess.run(cmd, capture_output=True, text=True, cwd=str(ROOT))
        except OSError as exc:
            raise StructizeError("unable to run clang: %s" % exc)
    if proc.returncode != 0 and not proc.stdout:
        raise StructizeError("clang failed for %s: %s"
                             % (struct_name, (proc.stderr or "").strip()[:400]))
    return _parse_layout(proc.stdout, struct_name)


def _parse_layout(dump, struct_name):
    marker = "| %s" % struct_name
    if marker not in dump:
        raise StructizeError("no record layout emitted for %s (is it a typedef'd "
                             "struct declared in types.h?)" % struct_name)
    block = dump[dump.index(marker):]
    end = block.find("sizeof=")
    tail = block[end:end + 120] if end >= 0 else ""
    block = block[:end] if end >= 0 else block
    size = None
    match = re.search(r"sizeof=(\d+)", tail)
    if match:
        size = int(match.group(1))

    fields = {}
    for row in _LAYOUT_ROW.finditer(block):
        offset = int(row.group(1))
        type_text = row.group(2).strip()
        name = row.group(3)
        count = 1
        array = re.search(r"\[(\d+)\]", type_text)
        if array:
            count = int(array.group(1))
        base = re.sub(r"\[\d+\]", "", type_text).strip()
        scalar = scalar_of(base)
        kind, width, signed = scalar if scalar else (None, None, None)
        if width is None:
            # A nested struct or pointer field.  We record its presence and span
            # so interior offsets resolve to it, but never rewrite through it.
            width = None
        fields[offset] = {
            "offset": offset,
            "name": name,
            "type": base,
            "kind": kind,
            "width": width,
            "signed": signed,
            "count": count,
            "is_pad": name.startswith("pad_"),
        }
    # Derive each field's byte span from the next field's offset; the last one
    # runs to sizeof.  This is more robust than trusting our own width model for
    # nested/pointer types.
    offsets = sorted(fields)
    for index, offset in enumerate(offsets):
        following = offsets[index + 1] if index + 1 < len(offsets) else size
        fields[offset]["span"] = (following - offset) if following is not None else None
    return {"struct": struct_name, "size": size, "fields": fields}


def field_covering(layout, offset):
    """The field whose byte span contains `offset`, or None."""
    best = None
    for start in sorted(layout["fields"]):
        if start > offset:
            break
        entry = layout["fields"][start]
        span = entry.get("span")
        if span is None or offset < start + span:
            best = entry
    if best is None:
        return None
    span = best.get("span")
    if span is not None and offset >= best["offset"] + span:
        return None
    return best


# --------------------------------------------------------------------------
# Source census
#
# Two syntaxes appear in the tree and both must be seen, otherwise a "rewrite
# everything" pass silently leaves a tail behind:
#     *(short *)(actor + 0x38)
#     *(short *)((char *)actor + 0x38)
# A third form -- passing `(float *)(actor + 0x12c)` as an argument without
# dereferencing -- is matched separately and always refused, because rewriting
# it would require taking the address of a packed field.
# --------------------------------------------------------------------------
def _deref_pattern(base):
    b = re.escape(base)
    return re.compile(
        r"\*\s*\(\s*(?P<vol>volatile\s+)?(?P<type>[A-Za-z_][\w ]*?)\s*\*\s*\)\s*"
        r"\(\s*(?:\(\s*char\s*\*\s*\)\s*)?(?P<base>%s)\s*\+\s*(?P<off>0[xX][0-9a-fA-F]+)\s*\)" % b)


def _addr_pattern(base):
    b = re.escape(base)
    return re.compile(
        r"(?<!\*)\(\s*(?P<type>[A-Za-z_][\w ]*?)\s*\*\s*\)\s*"
        r"\(\s*(?:\(\s*char\s*\*\s*\)\s*)?(?P<base>%s)\s*\+\s*(?P<off>0[xX][0-9a-fA-F]+)\s*\)" % b)


_FUNC_SIG = re.compile(r"^[A-Za-z_][\w \t\*]*?([A-Za-z_]\w*)\s*\(")


def function_spans(text):
    """Map line number -> enclosing function name, by brace depth.

    A definition is a top-level (depth 0) line whose last identifier before `(`
    names the function and whose body opens a brace.  Good enough for this
    codebase's style and, critically, only ever used to GROUP edits -- a
    misattribution can never corrupt a rewrite, only park the wrong unit.
    """
    spans = {}
    depth, current, pending = 0, None, None
    for number, line in enumerate(text.splitlines(), 1):
        stripped = line.rstrip()
        if depth == 0 and stripped and not stripped[0].isspace():
            match = _FUNC_SIG.match(stripped)
            if match and not stripped.lstrip().startswith("#"):
                pending = match.group(1)
        opened = line.count("{")
        closed = line.count("}")
        if depth == 0 and opened and pending:
            current = pending
            pending = None
        if current:
            spans[number] = current
        depth += opened - closed
        if depth <= 0:
            depth = 0
            current = None
    return spans


def census(source_path, base, struct_name, layout=None):
    """Classify every `base + 0xNN` site in `source_path` against the layout."""
    layout = layout or struct_layout(struct_name)
    try:
        text = Path(source_path).read_text(encoding="utf-8", errors="replace")
    except OSError as exc:
        raise StructizeError("unable to read source: %s" % exc)

    spans = function_spans(text)
    deref = _deref_pattern(base)
    addr = _addr_pattern(base)
    sites = []
    for line_number, line in enumerate(text.splitlines(), 1):
        seen = set()
        for match in deref.finditer(line):
            seen.add(match.span())
            sites.append(_classify(layout, line_number, line, match, taking_address=False))
        for match in addr.finditer(line):
            # An address-of use overlaps the deref match on the same span; only
            # record it when it is genuinely not a dereference.
            if any(s <= match.start() and match.end() <= e for s, e in seen):
                continue
            sites.append(_classify(layout, line_number, line, match, taking_address=True))
    for site in sites:
        site["function"] = spans.get(site["line"])

    return {
        "schema": SCHEMA,
        "tool_version": TOOL_VERSION,
        "source": Path(source_path).resolve().relative_to(ROOT).as_posix(),
        "base": base,
        "struct": struct_name,
        "struct_size": layout["size"],
        "sites": sites,
        "summary": summarize(sites),
    }


def _classify(layout, line_number, line, match, taking_address):
    offset = int(match.group("off"), 16)
    type_text = match.group("type").strip()
    volatile = bool(match.groupdict().get("vol"))
    scalar = scalar_of(type_text)
    site = {
        "line": line_number,
        "column": match.start() + 1,
        "offset": offset,
        "offset_hex": "0x%x" % offset,
        "cast": type_text,
        "volatile": volatile,
        "text": match.group(0),
        "verdict": None,
        "reason": None,
        "field": None,
    }

    size = layout.get("size")
    if size is not None and offset >= size:
        site["verdict"] = "refuse"
        site["reason"] = "offset beyond sizeof(%s)=0x%x -- binding is wrong" % (
            layout["struct"], size)
        return site
    if taking_address:
        site["verdict"] = "refuse"
        site["reason"] = "address taken, not dereferenced (&packed field)"
        return site
    if volatile:
        site["verdict"] = "refuse"
        site["reason"] = "volatile access must keep its qualifier"
        return site
    if scalar is None:
        site["verdict"] = "refuse"
        site["reason"] = "non-scalar or pointer cast `%s` (e.g. whole-struct copy)" % type_text
        return site

    kind, width, signed = scalar
    site["cast_kind"] = kind
    site["cast_width"] = width
    site["cast_signed"] = signed

    entry = field_covering(layout, offset)
    if entry is None:
        site["verdict"] = "refuse"
        site["reason"] = "offset not covered by any field"
        return site

    if entry["is_pad"]:
        site["verdict"] = "split"
        site["reason"] = "lands in %s -- accessed, so doctrine requires field_XX" % entry["name"]
        site["pad"] = entry["name"]
        site["pad_offset"] = entry["offset"]
        return site

    # A named field: it must agree on kind, width and signedness exactly.
    if entry["width"] is None:
        site["verdict"] = "refuse"
        site["reason"] = "field `%s` is a nested struct/pointer" % entry["name"]
        return site
    if entry["kind"] != kind:
        site["verdict"] = "refuse"
        site["reason"] = ("cast is %s but field `%s` is %s -- type pun, rewriting "
                          "would change codegen" % (kind, entry["name"], entry["kind"]))
        return site
    if entry["width"] != width:
        site["verdict"] = "refuse"
        site["reason"] = ("cast width %d != field `%s` width %d"
                          % (width, entry["name"], entry["width"]))
        return site
    if entry["signed"] is not None and signed is not None and entry["signed"] != signed:
        site["verdict"] = "refuse"
        site["reason"] = ("signedness differs from field `%s` (MOVSX vs MOVZX)"
                          % entry["name"])
        return site

    delta = offset - entry["offset"]
    if entry["count"] > 1:
        if delta % width:
            site["verdict"] = "refuse"
            site["reason"] = "offset is not on an element boundary of `%s`" % entry["name"]
            return site
        site["verdict"] = "rewrite"
        site["field"] = "%s[%d]" % (entry["name"], delta // width)
        return site
    if delta:
        site["verdict"] = "refuse"
        site["reason"] = "offset is interior to scalar field `%s`" % entry["name"]
        return site
    site["verdict"] = "rewrite"
    site["field"] = entry["name"]
    return site


def summarize(sites):
    out = {"total": len(sites), "rewrite": 0, "split": 0, "refuse": 0, "reasons": {}}
    for site in sites:
        out[site["verdict"]] += 1
        if site["verdict"] == "refuse":
            key = re.sub(r"`[^`]*`", "`X`", site["reason"] or "")
            key = re.sub(r"0x[0-9a-f]+|\d+", "N", key)
            out["reasons"][key] = out["reasons"].get(key, 0) + 1
    return out


# --------------------------------------------------------------------------
# Tool 1 -- pad_ runs become field_XX at every observed offset
# --------------------------------------------------------------------------
def plan_splits(census_data, layout=None):
    """Group `split` sites into concrete per-pad replacement plans.

    Refuses a pad whose observed accesses conflict (same offset read at two
    widths, or two accesses that overlap), because there is no single field
    layout that satisfies both.  Those become RE questions, not edits.
    """
    layout = layout or struct_layout(census_data["struct"])
    wanted = {}
    conflicts = []
    for site in census_data["sites"]:
        if site["verdict"] != "split":
            continue
        offset = site["offset"]
        key = (site["cast_width"], site["cast_signed"], site["cast_kind"])
        existing = wanted.get(offset)
        if existing is None:
            wanted[offset] = {"offset": offset, "width": site["cast_width"],
                              "signed": site["cast_signed"], "kind": site["cast_kind"],
                              "pad": site["pad"], "pad_offset": site["pad_offset"],
                              "sites": 1}
        elif (existing["width"], existing["signed"], existing["kind"]) != key:
            existing["sites"] += 1
            existing.setdefault("seen", set()).add(_describe(existing))
            existing["seen"].add(_describe({"kind": site["cast_kind"],
                                            "width": site["cast_width"],
                                            "signed": site["cast_signed"]}))
        else:
            existing["sites"] += 1

    # One conflict row per offset, naming every type it was accessed with, so
    # the worklist reads as a question ("which is it?") rather than as noise.
    conflicted = set()
    for offset, entry in sorted(wanted.items()):
        if entry.get("seen"):
            conflicted.add(offset)
            conflicts.append({
                "offset_hex": "0x%x" % offset,
                "sites": entry["sites"],
                "reason": "accessed as %s -- widths/signedness disagree"
                          % " and ".join(sorted(entry["seen"])),
            })
    ordered = [wanted[o] for o in sorted(wanted) if o not in conflicted]

    # Overlap check: a 4-byte field at 0x18 forbids any field at 0x19..0x1b.
    kept = []
    for entry in ordered:
        if kept and entry["offset"] < kept[-1]["offset"] + kept[-1]["width"]:
            conflicts.append({
                "offset_hex": "0x%x" % entry["offset"],
                "sites": entry["sites"],
                "reason": "overlaps the %s field proposed at 0x%x"
                          % (_describe(kept[-1]), kept[-1]["offset"]),
            })
            continue
        kept.append(entry)

    by_pad = {}
    for entry in kept:
        by_pad.setdefault(entry["pad"], []).append(entry)

    width = hex_width(layout.get("size"))
    plans = []
    for pad_name, entries in sorted(by_pad.items(), key=lambda kv: entries_key(kv[1])):
        pad = layout["fields"].get(entries[0]["pad_offset"])
        if pad is None or not pad["is_pad"]:
            continue
        plans.append(_plan_one_pad(pad, entries, width))
    return {
        "struct": census_data["struct"],
        "plans": plans,
        "conflicts": conflicts,
        "summary": {
            "pads_split": len(plans),
            "fields_added": sum(p["fields_added"] for p in plans),
            "offsets_conflicted": len(conflicts),
            "sites_blocked_by_conflict": sum(c.get("sites", 0) for c in conflicts),
        },
    }


def entries_key(entries):
    return entries[0]["pad_offset"]


def _plan_one_pad(pad, entries, width=2):
    """Render the replacement lines for one pad_ run.

    The run's total byte span is preserved exactly, so sizeof() and every
    following field offset are unchanged -- that is what makes the edit
    codegen-neutral and lets the cs()/co() asserts keep holding.
    """
    start = pad["offset"]
    end = start + pad["span"]
    lines = []
    cursor = start
    for entry in entries:
        if entry["offset"] > cursor:
            gap = entry["offset"] - cursor
            lines.append("  char %s[0x%x];" % (pad_name_for(cursor, width), gap))
            cursor += gap
        spelling = FIELD_SPELLING.get((entry["width"], entry["signed"]))
        if spelling is None:
            spelling = "int32_t" if entry["width"] == 4 else "char"
        if entry["kind"] == "float":
            spelling = "float"
        lines.append("  %-50s /* +0x%0*x  accessed %d%s, meaning unproven */"
                     % ("%s %s;" % (spelling, field_name_for(entry["offset"], width)),
                        width, entry["offset"], entry["sites"],
                        "x" if entry["sites"] != 1 else "x"))
        cursor += entry["width"]
    if cursor < end:
        lines.append("  char %s[0x%x];" % (pad_name_for(cursor, width), end - cursor))
    return {
        "pad": pad["name"],
        "offset_hex": "0x%x" % start,
        "span": pad["span"],
        "fields_added": len(entries),
        "replacement": lines,
    }


_PAD_LINE = re.compile(r"^[ \t]*char\s+(pad_[0-9a-fA-F]+)\s*\[[^\]]*\]\s*;.*$", re.M)


def apply_splits(plan, types_h=None, apply=False):
    """Rewrite the pad_ declarations in types.h according to `plan`."""
    path = Path(types_h or TYPES_H)
    text = path.read_text(encoding="utf-8")
    changed = []
    for entry in plan["plans"]:
        pattern = re.compile(
            r"^[ \t]*char\s+%s\s*\[[^\]]*\]\s*;[^\n]*$" % re.escape(entry["pad"]), re.M)
        matches = pattern.findall(text)
        if len(matches) != 1:
            raise StructizeError(
                "expected exactly one declaration of %s in %s, found %d"
                % (entry["pad"], path.name, len(matches)))
        text = pattern.sub("\n".join(entry["replacement"]), text, count=1)
        changed.append(entry["pad"])
    if apply:
        path.write_text(text, encoding="utf-8")
    return {"applied": bool(apply), "pads_changed": changed, "count": len(changed)}


# --------------------------------------------------------------------------
# Tool 2 -- call sites become alias->field
#
# Deliberately NOT retyping the base declaration.  Turning `char *actor` into
# `actor_t *actor` rescales every un-rewritten `actor + 0xNN` by sizeof(actor_t),
# so a partial rewrite would silently corrupt the sites it did not touch.  An
# alias variable is additive and safe at any coverage level.
# --------------------------------------------------------------------------
def apply_rewrites(census_data, alias=None, source=None, apply=False,
                   exclude=(), only=()):
    """Rewrite resolvable sites to a named field access.

    Default is an inline cast, `((actor_t *)actor)->field_38`, rather than an
    alias local.  An alias would need a new declaration, which (a) adds a local
    the optimiser may or may not coalesce and (b) shifts __LINE__ for every
    assert below it, so it is not codegen-neutral by construction.  The inline
    cast is purely local and provably neutral, and it composes: once a struct's
    coverage approaches 100%, a separate pass can retype the declaration and
    drop every cast at once -- safe only at that point, because a partial
    retype would rescale the un-rewritten `base + 0xNN` sites by sizeof().
    """
    struct = census_data["struct"]
    prefix = "((%s *)%s)->" % (struct, census_data["base"]) if not alias else "%s->" % alias
    path = Path(source or (ROOT / census_data["source"]))
    text = path.read_text(encoding="utf-8")
    lines = text.splitlines(keepends=True)

    edits = [s for s in census_data["sites"] if s["verdict"] == "rewrite"]
    if exclude:
        excluded = set(exclude)
        edits = [s for s in edits if s.get("function") not in excluded]
    if only:
        wanted = set(only)
        edits = [s for s in edits if s.get("function") in wanted]
    by_line = {}
    for site in edits:
        by_line.setdefault(site["line"], []).append(site)

    applied = 0
    for line_number, sites in by_line.items():
        original = lines[line_number - 1]
        updated = original
        # Right-to-left so earlier columns stay valid as the line shrinks.
        for site in sorted(sites, key=lambda s: s["column"], reverse=True):
            needle = site["text"]
            index = updated.find(needle)
            if index < 0:
                continue
            updated = (updated[:index] + prefix + site["field"]
                       + updated[index + len(needle):])
            applied += 1
        lines[line_number - 1] = updated

    if apply:
        path.write_text("".join(lines), encoding="utf-8")
    return {
        "applied": bool(apply),
        "mode": "alias" if alias else "inline-cast",
        "prefix": prefix,
        "sites_rewritten": applied,
        "sites_eligible": len(edits),
        "lines_touched": len(by_line),
        "functions_touched": sorted({s.get("function") for s in edits if s.get("function")}),
        "functions_excluded": sorted(exclude),
        "note": ("base declaration intentionally NOT retyped -- a partial retype "
                 "would rescale un-rewritten `%s + 0xNN` sites by sizeof(%s)"
                 % (census_data["base"], struct)),
    }


# --------------------------------------------------------------------------
# Gate -- reuses the existing COFF neutrality guard, no new verification logic
# --------------------------------------------------------------------------
def object_for(source_rel):
    return ROOT / "build" / "CMakeFiles" / "halo.dir" / (source_rel + ".obj")


def gate(source_rel, baseline_path, build=True):
    """Build the TU and compare its .text against a captured baseline."""
    from tools.recovery import coff_candidate_guard

    obj = object_for(source_rel)
    if build:
        try:
            proc = subprocess.run(["cmake", "--build", "build", "--target", "halo"],
                                  cwd=str(ROOT), capture_output=True, text=True)
        except OSError as exc:
            raise StructizeError("build failed to start: %s" % exc)
        # A failed build leaves the PREVIOUS object on disk. Comparing that
        # stale object against the baseline passes trivially and reports a
        # green gate for source that does not even compile, so the return
        # code has to be checked before the object is trusted.
        if proc.returncode != 0:
            raise StructizeError("build failed (exit %d); refusing to gate against "
                                 "a stale object:\n%s"
                                 % (proc.returncode,
                                    (proc.stderr or proc.stdout or "").strip()[:2000]))
    if not obj.is_file():
        raise StructizeError("object not built: %s" % obj)
    baseline = json.loads(Path(baseline_path).read_text(encoding="ascii"))
    return coff_candidate_guard.compare_snapshots(
        baseline, coff_candidate_guard.capture_object(str(obj)))


def converge(census_data, source=None, alias=None, manifest=None):
    """Rewrite everything that can be rewritten WITHOUT changing any function.

    The whole point of the tool in one call:

        1. compile the untouched TU                       -> reference
        2. apply every eligible rewrite, compile          -> candidate
        3. diff at function granularity
        4. if any function moved, re-apply excluding those functions
        5. compile again and prove the result is byte-identical

    Step 4 is what makes this usable.  Two divergent functions out of 119
    should cost you those two, not the other 117.  The excluded functions come
    back as a parked worklist with a real reason, which is exactly the shape
    the recovery manifest wants.

    The source file is restored on any failure, so a non-converging run leaves
    the tree exactly as it found it.
    """
    path = Path(source or (ROOT / census_data["source"]))
    original = path.read_text(encoding="utf-8")
    tmp = Path(tempfile.mkdtemp(prefix="structize-"))
    report = {"ok": False, "source": census_data["source"], "rounds": []}
    try:
        reference = compile_tu(path, tmp / "reference.obj")
        excluded = set()
        for round_index in range(2):
            path.write_text(original, encoding="utf-8")
            applied = apply_rewrites(census_data, alias=alias, source=path,
                                     apply=True, exclude=sorted(excluded))
            candidate = compile_tu(path, tmp / ("candidate%d.obj" % round_index))
            delta = diverging_functions(reference, candidate)
            report["rounds"].append({
                "round": round_index,
                "sites_rewritten": applied["sites_rewritten"],
                "functions_identical": delta["identical"],
                "functions_changed": delta["changed"],
                "excluded_this_round": sorted(excluded),
            })
            if not delta["changed"] and not delta["appeared"] and not delta["vanished"]:
                report["ok"] = True
                # Rewriting nothing is byte-identical by construction. That is a
                # true statement about a run that did no work, and reporting it
                # the same way as a real conversion invites a caller to read
                # "converged, byte-identical" as progress. Flag it explicitly;
                # the CLI gives it its own exit code.
                report["vacuous"] = applied["sites_rewritten"] == 0
                report["sites_rewritten"] = applied["sites_rewritten"]
                report["sites_eligible"] = len([s for s in census_data["sites"]
                                                if s["verdict"] == "rewrite"])
                report["parked_functions"] = sorted(excluded)
                report["park_reason"] = (  # noqa: E501 - assigned below, read by sync

                    "codegen moved, so the rewrite was withheld. Two causes produce "
                    "this identical signal and only inspection tells them apart: "
                    "(1) benign -- a pack(1) member has alignment 1 where the "
                    "original cast asserted natural alignment, which at -O3 shifts "
                    "instruction scheduling (a byte store sinking past adjacent "
                    "float stores) or register allocation (%edi -> %ebx); "
                    "(2) a real bug -- the field binding is wrong, and the rewrite "
                    "reads a different address than the cast did. The gate withholds "
                    "either way, so nothing wrong ships, but a park is a finding to "
                    "triage, not a result to accept. Suspect (2) when a park appears "
                    "in a function whose offsets were only recently split")
                if manifest:
                    report["manifest_sync"] = sync_manifest(
                        census_data, manifest, sorted(excluded),
                        report["park_reason"], apply=True)
                return report
            excluded |= set(delta["changed"])
        # Two rounds failed to converge -- restore and report honestly.
        path.write_text(original, encoding="utf-8")
        report["error"] = ("did not converge after excluding %d function(s); "
                           "source restored" % len(excluded))
        return report
    except Exception:
        path.write_text(original, encoding="utf-8")
        raise
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def site_outcomes(census_data, parked_functions=(), park_reason=""):
    """Per-source-line outcome, folding the converge report into the census.

    Line granularity, not per-occurrence: the manifest's own detector pattern is
    narrower than this tool's (it misses the `(char *)base + 0xNN` form and
    volatile), so occurrence indices do not line up between the two. Lines do.
    """
    parked = set(parked_functions)
    by_line = {}
    for site in census_data["sites"]:
        if site["verdict"] == "rewrite" and site.get("function") in parked:
            outcome, reason = "parked", park_reason or "function parked by converge"
        elif site["verdict"] == "rewrite":
            outcome, reason = "applied", None
        elif site["verdict"] == "split":
            outcome, reason = "parked", "offset unresolved: %s" % site["reason"]
        else:
            outcome, reason = "parked", site["reason"]
        entry = by_line.setdefault(site["line"], {"applied": 0, "parked": 0, "reason": None})
        entry[outcome] += 1
        if outcome == "parked" and entry["reason"] is None:
            entry["reason"] = reason
    return by_line


def sync_manifest(census_data, manifest_arg, parked_functions=(), park_reason="",
                  categories=("offset-to-field",), apply=False):
    """Write census/converge outcomes back into a source-recovery manifest.

    Conservative by construction: a line only counts as `applied` when EVERY
    site on it was rewritten. A line with any leftover raw deref stays pending
    or gets parked with the first concrete reason, because a half-converted
    line is not done.
    """
    from tools.recovery import source_recovery

    path, manifest = source_recovery._load(manifest_arg)
    if manifest.get("repo_relative_source") != census_data["source"]:
        raise StructizeError(
            "manifest is for %s but the census is for %s"
            % (manifest.get("repo_relative_source"), census_data["source"]))

    outcomes = site_outcomes(census_data, parked_functions, park_reason)
    wanted = set(categories)
    changes = {"applied": [], "parked": [], "untouched": 0}
    for item in list(manifest["items"]):
        if item.get("status") != "pending" or item.get("category") not in wanted:
            continue
        entry = outcomes.get(item.get("line"))
        if entry is None:
            changes["untouched"] += 1
            continue
        if entry["parked"] == 0 and entry["applied"] > 0:
            changes["applied"].append(item["id"])
            if apply:
                source_recovery._set_status(path, manifest, item["id"], "applied", None)
        else:
            changes["parked"].append({"id": item["id"], "reason": entry["reason"]})
            if apply:
                source_recovery._set_status(path, manifest, item["id"], "parked",
                                            entry["reason"])
    return {
        "applied": bool(apply),
        "manifest": str(path),
        "marked_applied": len(changes["applied"]),
        "marked_parked": len(changes["parked"]),
        "left_pending": changes["untouched"],
        "parked_detail": changes["parked"][:20],
    }


def run_all(source, base, struct, census_dir=None, manifest=None, alias=None,
            apply_splits_too=True):
    """census -> split -> re-census -> converge, in one call.

    The four-command sequence has one ordering trap: the census must be retaken
    after a split, because splitting is what turns `pad_` sites into resolvable
    fields. Skipping it silently leaves the newly unblocked sites unconverted,
    and the run still reports success. Making that impossible is better than
    documenting it, so this is the entry point callers should use.

    Returns a single summary; the intermediate census is still written to disk
    for inspection and for a later `sync`.
    """
    source = Path(source)
    census_dir = Path(census_dir or (ROOT / "recovery" / "census"))
    census_dir.mkdir(parents=True, exist_ok=True)
    census_path = census_dir / (source.stem + ".json")

    def take():
        data = census(source, base, struct)
        census_path.write_text(json.dumps(data, indent=1, sort_keys=True) + "\n",
                               encoding="utf-8")
        return data

    report = {"source": source.as_posix(), "struct": struct, "base": base,
              "census": census_path.as_posix(), "steps": []}

    data = take()
    report["steps"].append({"step": "census", **data["summary"]})

    plan = plan_splits(data)
    if apply_splits_too and plan["plans"]:
        result = apply_splits(plan, apply=True)
        report["steps"].append({"step": "split", "pads_changed": result["count"]})
        data = take()   # mandatory: the layout just changed
        report["steps"].append({"step": "re-census", **data["summary"]})
    else:
        report["steps"].append({"step": "split", "pads_changed": 0})

    # Unresolved conflicts are the RE questions worth a human's time, ranked by
    # how many call sites each one unblocks.
    report["conflicts"] = sorted(plan["conflicts"],
                                 key=lambda c: c.get("sites", 0), reverse=True)[:10]
    report["conflicts_total"] = len(plan["conflicts"])

    converged = converge(data, source=source, alias=alias, manifest=manifest)
    report["steps"].append({"step": "converge", "ok": converged["ok"],
                            "sites_rewritten": converged.get("sites_rewritten", 0),
                            "parked": converged.get("parked_functions", [])})
    report["ok"] = converged["ok"]
    report["vacuous"] = converged.get("vacuous", False)
    report["parked_functions"] = converged.get("parked_functions", [])
    report["park_reason"] = converged.get("park_reason", "")
    if not converged["ok"]:
        report["error"] = converged.get("error", "converge failed")
    return report


def capture_baseline(source_rel, out_path):
    from tools.recovery import coff_candidate_guard

    obj = object_for(source_rel)
    if not obj.is_file():
        raise StructizeError("object not built: %s (build first)" % obj)
    snapshot = coff_candidate_guard.capture_object(str(obj))
    Path(out_path).write_text(json.dumps(snapshot, indent=1, sort_keys=True) + "\n",
                              encoding="ascii")
    return {"ok": True, "object": str(obj), "baseline": str(out_path)}


# --------------------------------------------------------------------------
# Bindings
# --------------------------------------------------------------------------
def load_binding(name):
    if not BINDINGS.is_file():
        raise StructizeError("no bindings file at %s" % BINDINGS)
    data = json.loads(BINDINGS.read_text(encoding="utf-8"))
    for row in data.get("bindings", []):
        if row.get("id") == name:
            return row
    raise StructizeError("no binding named %r in %s" % (name, BINDINGS))


# --------------------------------------------------------------------------
# CLI
# --------------------------------------------------------------------------
def _emit(payload):
    json.dump(payload, sys.stdout, indent=1, sort_keys=True)
    sys.stdout.write("\n")


def _self_test():
    checks = []
    checks.append(("scalar widths", scalar_of("unsigned short") == ("int", 2, False)))
    checks.append(("pointer casts are not scalars", scalar_of("foo_t *") is None))
    checks.append(("const is ignored", scalar_of("const int") == ("int", 4, True)))
    checks.append(("field naming", field_name_for(0x38) == "field_38"))
    checks.append(("field naming pads to 2", field_name_for(0x2) == "field_02"))

    layout = {
        "struct": "t", "size": 0x20,
        "fields": {
            0x0: {"offset": 0, "name": "salt", "type": "int16_t", "kind": "int",
                  "width": 2, "signed": True, "count": 1, "is_pad": False, "span": 2},
            0x2: {"offset": 2, "name": "pad_02", "type": "char", "kind": "int",
                  "width": 1, "signed": None, "count": 0x0E, "is_pad": True, "span": 0x0E},
            0x10: {"offset": 0x10, "name": "vec", "type": "float", "kind": "float",
                   "width": 4, "signed": None, "count": 3, "is_pad": False, "span": 12},
            0x1C: {"offset": 0x1C, "name": "flags", "type": "uint32_t", "kind": "int",
                   "width": 4, "signed": False, "count": 1, "is_pad": False, "span": 4},
        },
    }

    def one(line, base="p", taking_address=False):
        pattern = _addr_pattern(base) if taking_address else _deref_pattern(base)
        match = pattern.search(line)
        assert match, line
        return _classify(layout, 1, line, match, taking_address)

    checks.append(("exact scalar match rewrites",
                   one("x = *(int16_t *)(p + 0x0);")["verdict"] == "rewrite"))
    checks.append(("array element resolves",
                   one("x = *(float *)(p + 0x14);")["field"] == "vec[1]"))
    checks.append(("float read of int field refused",
                   one("x = *(float *)(p + 0x1c);")["verdict"] == "refuse"))
    checks.append(("int read of float field refused",
                   one("x = *(int *)(p + 0x10);")["verdict"] == "refuse"))
    checks.append(("signedness mismatch refused",
                   one("x = *(int32_t *)(p + 0x1c);")["verdict"] == "refuse"))
    checks.append(("width mismatch refused",
                   one("x = *(char *)(p + 0x1c);")["verdict"] == "refuse"))
    checks.append(("interior offset refused",
                   one("x = *(int16_t *)(p + 0x1d);")["verdict"] == "refuse"))
    checks.append(("pad access asks for a split",
                   one("x = *(int16_t *)(p + 0x4);")["verdict"] == "split"))
    checks.append(("volatile refused",
                   one("x = *(volatile int16_t *)(p + 0x0);")["verdict"] == "refuse"))
    checks.append(("whole-struct cast refused",
                   one("x = *(vector3_t *)(p + 0x10);")["verdict"] == "refuse"))
    checks.append(("beyond sizeof refused (bad binding)",
                   one("x = *(int32_t *)(p + 0x40);")["verdict"] == "refuse"))
    checks.append(("char* form is seen",
                   one("x = *(int16_t *)((char *)p + 0x0);")["verdict"] == "rewrite"))
    checks.append(("address-taken refused",
                   one("f((float *)(p + 0x10));", taking_address=True)["verdict"] == "refuse"))
    checks.append(("plain deref is not counted as address-taken",
                   _deref_pattern("p").search("x = *(int16_t *)(p + 0x0);") is not None))

    # Split planning preserves the pad span exactly.
    data = {
        "struct": "t", "base": "p", "source": "x.c",
        "sites": [
            dict(one("x = *(int16_t *)(p + 0x4);"), pad="pad_02", pad_offset=2),
            dict(one("x = *(int16_t *)(p + 0x4);"), pad="pad_02", pad_offset=2),
            dict(one("x = *(int32_t *)(p + 0x8);"), pad="pad_02", pad_offset=2),
        ],
    }
    plan = plan_splits(data, layout)
    body = "\n".join(plan["plans"][0]["replacement"])
    checks.append(("split emits field_04", "field_04" in body))
    checks.append(("split emits field_08", "field_08" in body))
    total = 0
    for line in body.splitlines():
        array = re.search(r"\[(0x[0-9a-f]+)\]", line)
        if array:
            total += int(array.group(1), 16)
        elif "int16_t" in line:
            total += 2
        elif "int32_t" in line:
            total += 4
    checks.append(("split preserves the pad span", total == 0x0E))
    checks.append(("repeat accesses are counted once", plan["plans"][0]["fields_added"] == 2))

    conflicting = {
        "struct": "t", "base": "p", "source": "x.c",
        "sites": [
            dict(one("x = *(int16_t *)(p + 0x4);"), pad="pad_02", pad_offset=2),
            dict(one("x = *(int32_t *)(p + 0x4);"), pad="pad_02", pad_offset=2),
        ],
    }
    checks.append(("conflicting widths are refused, not guessed",
                   plan_splits(conflicting, layout)["conflicts"] != []))

    overlapping = {
        "struct": "t", "base": "p", "source": "x.c",
        "sites": [
            dict(one("x = *(int32_t *)(p + 0x4);"), pad="pad_02", pad_offset=2),
            dict(one("x = *(int16_t *)(p + 0x6);"), pad="pad_02", pad_offset=2),
        ],
    }
    checks.append(("overlapping fields are refused",
                   overlapping and plan_splits(overlapping, layout)["conflicts"] != []))

    failures = [name for name, ok in checks if not ok]
    for name, ok in checks:
        print("  %s %s" % ("ok  " if ok else "FAIL", name))
    return 1 if failures else 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--self-test", action="store_true")
    sub = parser.add_subparsers(dest="command")

    p = sub.add_parser("layout", help="authoritative field table for a struct")
    p.add_argument("struct")

    p = sub.add_parser("census", help="classify every base+offset site in a source file")
    p.add_argument("--source", required=True)
    p.add_argument("--base", required=True)
    p.add_argument("--struct", required=True)
    p.add_argument("-o", "--output")

    p = sub.add_parser("split", help="pad_ runs -> field_XX at observed offsets")
    p.add_argument("--census", required=True)
    p.add_argument("--apply", action="store_true")
    p.add_argument("--types-h")

    p = sub.add_parser("rewrite", help="*(T*)(p+0xNN) -> alias->field")
    p.add_argument("--census", required=True)
    p.add_argument("--alias")
    p.add_argument("--apply", action="store_true")
    p.add_argument("--exclude", nargs="*", default=[], metavar="FUNC")
    p.add_argument("--only", nargs="*", default=[], metavar="FUNC")

    p = sub.add_parser("converge",
                       help="rewrite everything that keeps every function byte-identical")
    p.add_argument("--census", required=True)
    p.add_argument("--alias")
    p.add_argument("--manifest", help="source-recovery manifest to update on success")

    p = sub.add_parser("sync", help="write census outcomes back into a manifest")
    p.add_argument("--census", required=True)
    p.add_argument("--manifest", required=True)
    p.add_argument("--parked", nargs="*", default=[], metavar="FUNC")
    p.add_argument("--apply", action="store_true")

    p = sub.add_parser("run", help="census -> split -> re-census -> converge (use this)")
    p.add_argument("--source", required=True)
    p.add_argument("--base", required=True)
    p.add_argument("--struct", required=True)
    p.add_argument("--manifest")
    p.add_argument("--alias")
    p.add_argument("--census-dir")
    p.add_argument("--no-split", action="store_true",
                   help="do not touch types.h; convert only what already resolves")

    p = sub.add_parser("baseline", help="capture the pre-change COFF snapshot")
    p.add_argument("--source", required=True)
    p.add_argument("-o", "--output", required=True)

    p = sub.add_parser("gate", help="rebuild and compare .text to the baseline")
    p.add_argument("--source", required=True)
    p.add_argument("--baseline", required=True)
    p.add_argument("--no-build", action="store_true")

    p = sub.add_parser("status", help="one-screen census summary")
    p.add_argument("--census", required=True)

    args = parser.parse_args(argv)
    if args.self_test:
        return _self_test()

    try:
        if args.command == "layout":
            layout = struct_layout(args.struct)
            _emit({"struct": layout["struct"], "size": layout["size"],
                   "fields": [layout["fields"][o] for o in sorted(layout["fields"])]})
            return 0
        if args.command == "census":
            data = census(args.source, args.base, args.struct)
            if args.output:
                Path(args.output).write_text(json.dumps(data, indent=1, sort_keys=True) + "\n",
                                             encoding="utf-8")
                _emit(data["summary"])
            else:
                _emit(data)
            return 0
        if args.command == "split":
            data = json.loads(Path(args.census).read_text(encoding="utf-8"))
            plan = plan_splits(data)
            result = apply_splits(plan, args.types_h, apply=args.apply)
            _emit({"plan": plan, "result": result})
            return 0
        if args.command == "rewrite":
            data = json.loads(Path(args.census).read_text(encoding="utf-8"))
            _emit(apply_rewrites(data, alias=args.alias, apply=args.apply,
                                 exclude=args.exclude, only=args.only))
            return 0
        if args.command == "converge":
            data = json.loads(Path(args.census).read_text(encoding="utf-8"))
            result = converge(data, alias=args.alias, manifest=args.manifest)
            _emit(result)
            if not result.get("ok"):
                return 1
            # 2 = converged but rewrote nothing, so a caller polling the exit
            # code can tell "no work available" from "work done".
            return 2 if result.get("vacuous") else 0
        if args.command == "sync":
            data = json.loads(Path(args.census).read_text(encoding="utf-8"))
            _emit(sync_manifest(data, args.manifest, args.parked,
                                "function parked by converge", apply=args.apply))
            return 0
        if args.command == "run":
            result = run_all(args.source, args.base, args.struct,
                             census_dir=args.census_dir, manifest=args.manifest,
                             alias=args.alias, apply_splits_too=not args.no_split)
            _emit(result)
            if not result.get("ok"):
                return 1
            return 2 if result.get("vacuous") else 0
        if args.command == "baseline":
            _emit(capture_baseline(args.source, args.output))
            return 0
        if args.command == "gate":
            result = gate(args.source, args.baseline, build=not args.no_build)
            _emit(result)
            return 0 if result["ok"] else 1
        if args.command == "status":
            data = json.loads(Path(args.census).read_text(encoding="utf-8"))
            _emit(data["summary"])
            return 0
        parser.error("a subcommand is required")
    except (OSError, ValueError, StructizeError) as exc:
        _emit({"ok": False, "errors": [str(exc)]})
        return 2


if __name__ == "__main__":
    sys.exit(main())
