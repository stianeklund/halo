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
# changes load scheduling.  For THAT pair, `-fno-strict-aliasing` makes no
# difference.
#
# It is not the only cause, and assuming it is misleads.  Measured on
# actor_moving.c (`triage`), three parked functions had two other causes and
# none was alignment: two were pure TBAA -- a `*(char *)` cast aliases
# everything and pins the surrounding accesses, while the field access carries
# a precise struct-path tag and frees the scheduler, so the rewrite is
# byte-identical again under `-fno-strict-aliasing` -- and the third came down
# to two adjacent byte loads where LLVM kept the raw offset in a register for
# an indexed load but canonicalised the struct GEP into a `lea`.  Use `triage`
# to find out which; do not guess.
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


def _scratch(oracle, prefix):
    """A temp dir the chosen compiler can actually write to.

    VC71 is Windows cl.exe reached through WSL interop: its output path is
    translated with wsl_to_win, and /tmp has no Windows equivalent, so a
    default mkdtemp yields `C1083: cannot open compiler generated file`. Under
    the vc71 oracle the scratch has to live on the mapped drive.
    """
    if oracle == "vc71":
        base = ROOT / "build" / "structize-scratch"
        base.mkdir(parents=True, exist_ok=True)
        return Path(tempfile.mkdtemp(prefix=prefix, dir=str(base)))
    return Path(tempfile.mkdtemp(prefix=prefix))


def compile_tu_vc71(source, out_obj):
    """Compile one translation unit with Visual C++ 7.1 -- the ONLY oracle that counts.

    The shipping binary was built by MSVC 7.1, so "did my edit change the
    generated code?" is only a meaningful question when MSVC 7.1 is the one
    answering it. Clang agreeing with itself across an edit is a different
    compiler's opinion about a different code generator; it can hold while VC71
    diverges, and it can diverge while VC71 holds. Measured on actor_moving.c:
    three functions the clang gate parked are byte-identical under VC71, so the
    clang oracle cost 196 sites for nothing.

    Reuses vc71_verify.compile_vc71 rather than re-deriving the command line --
    it already models the kb.json register ABI (the __fastcall rewrite for
    @<ecx>/@<edx> functions), which a naive cl.exe invocation gets wrong.
    """
    from tools.verify.vc71_verify import compile_vc71
    out_obj = Path(out_obj)
    if not compile_vc71(Path(source), out_obj):
        raise StructizeError(
            "VC71 compile failed for %s. Without it there is no evidence the "
            "edit is neutral for the real compiler; fix the build or pass "
            "--oracle clang and accept that the result proves less." % source)
    return out_obj


def compile_for(oracle, source, out_obj, extra=()):
    """Dispatch to the requested oracle. `extra` is clang-only (diagnostic flags)."""
    if oracle == "vc71":
        return compile_tu_vc71(source, out_obj)
    if oracle == "clang":
        return compile_tu(source, out_obj, extra=extra)
    raise StructizeError("unknown oracle %r (expected 'vc71' or 'clang')" % oracle)


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
                              "sites": 1,
                              "functions": {site.get("function")}}
        elif (existing["width"], existing["signed"], existing["kind"]) != key:
            existing["sites"] += 1
            existing.setdefault("seen", set()).add(_describe(existing))
            existing["seen"].add(_describe({"kind": site["cast_kind"],
                                            "width": site["cast_width"],
                                            "signed": site["cast_signed"]}))
            existing.setdefault("type_functions", {})
            desc = _describe({"kind": site["cast_kind"], "width": site["cast_width"],
                              "signed": site["cast_signed"]})
            existing["type_functions"].setdefault(desc, set()).add(site.get("function"))
            own_desc = _describe(existing)
            existing["type_functions"].setdefault(own_desc, set()).update(
                existing.get("functions", set()))
        else:
            existing["sites"] += 1
            existing.setdefault("functions", set()).add(site.get("function"))

    # One conflict row per offset, naming every type it was accessed with, so
    # the worklist reads as a question ("which is it?") rather than as noise.
    conflicted = set()
    for offset, entry in sorted(wanted.items()):
        if entry.get("seen"):
            conflicted.add(offset)
            type_fns = {}
            for desc, fns in entry.get("type_functions", {}).items():
                type_fns[desc] = sorted(f for f in fns if f)
            conflicts.append({
                "offset_hex": "0x%x" % offset,
                "sites": entry["sites"],
                "reason": "accessed as %s -- widths/signedness disagree"
                          % " and ".join(sorted(entry["seen"])),
                "functions_by_type": type_fns,
            })
    ordered = [wanted[o] for o in sorted(wanted) if o not in conflicted]

    # Overlap check: a 4-byte field at 0x18 forbids any field at 0x19..0x1b.
    # Boundary check: a field must fit entirely within its pad_ run.
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
        pad = layout["fields"].get(entry["pad_offset"])
        if pad is not None:
            pad_end = pad["offset"] + (pad.get("span") or 0)
            if entry["offset"] + entry["width"] > pad_end:
                conflicts.append({
                    "offset_hex": "0x%x" % entry["offset"],
                    "sites": entry["sites"],
                    "reason": "%s at 0x%x extends past %s boundary (0x%x)"
                              % (_describe(entry), entry["offset"],
                                 pad["name"], pad_end),
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
                   exclude=(), only=(), offsets=None):
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
    if offsets is not None:
        # Offset-level selection exists for `triage`, which has to ask "which
        # of these sites is the one that moves the codegen?" -- a question that
        # cannot be answered at function granularity.
        keep = set(offsets)
        edits = [s for s in edits if s["offset_hex"] in keep]
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


def converge(census_data, source=None, alias=None, manifest=None, oracle="vc71"):
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
    tmp = _scratch(oracle, "structize-")
    report = {"ok": False, "source": census_data["source"], "oracle": oracle,
              "rounds": []}
    if oracle != "vc71":
        # Recorded in the payload, not just printed, so a downstream reader
        # cannot mistake a clang-gated result for evidence about the real
        # compiler. The binary was built by MSVC 7.1; nothing else arbitrates.
        report["oracle_warning"] = (
            "gated with %r, NOT Visual C++ 7.1. This does not show the edit is "
            "neutral for the compiler that built the binary." % oracle)
    try:
        reference = compile_for(oracle, path, tmp / "reference.obj")
        excluded = set()
        for round_index in range(2):
            path.write_text(original, encoding="utf-8")
            applied = apply_rewrites(census_data, alias=alias, source=path,
                                     apply=True, exclude=sorted(excluded))
            candidate = compile_for(oracle, path,
                                    tmp / ("candidate%d.obj" % round_index))
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

                    "codegen moved, so the rewrite was withheld. Several causes "
                    "produce this identical signal, and guessing between them has "
                    "already been wrong once: (1) TBAA -- the `char *` cast aliases "
                    "everything and pins ordering, the field access does not; "
                    "(2) address form -- LLVM keeps a raw `base + 0xNN` constant in a "
                    "register for an indexed load but canonicalises the struct GEP to "
                    "a `lea`; (3) alignment -- a pack(1) member has alignment 1 where "
                    "the cast asserted natural alignment, shifting -O3 scheduling; "
                    "(4) a real bug -- the binding is wrong and the rewrite reads a "
                    "different address. The gate withholds in every case, so nothing "
                    "wrong ships. Do not assume which one applies: run "
                    "`structize.py triage --census <census>`, which separates (1) by "
                    "recompiling with -fno-strict-aliasing and bisects the rest down "
                    "to the responsible offsets")
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


CAUSE_TBAA = "tbaa"
CAUSE_ADDRESS_FORM = "address-form-or-alignment"
CAUSE_COMBINATION = "only-in-combination"

_CAUSE_TEXT = {
    CAUSE_TBAA:
        "TBAA. The raw form casts through `char *`, which aliases everything and "
        "pins the surrounding loads and stores in place; the field access carries a "
        "precise struct-path TBAA tag, so the optimiser is free to reorder across "
        "it. Same address, same width, same accesses -- only the scheduling "
        "freedom differs. Confirmed by the rewrite being byte-identical under "
        "-fno-strict-aliasing.",
    CAUSE_ADDRESS_FORM:
        "Address materialisation or member alignment, NOT aliasing (it survives "
        "-fno-strict-aliasing). Observed form: the raw `base + 0xNN` lets LLVM keep "
        "the constant in a register and use an indexed load (`mov $0x426,%ecx`), "
        "while the struct GEP is canonicalised to a computed address (`lea "
        "0x426(%edi),%ecx`); the pack(1) alignment-1 member is the other known "
        "cause. Both read the same byte. Inspect the culprit offsets below before "
        "accepting.",
    CAUSE_COMBINATION:
        "This function is byte-identical when rewritten alone, so it is not itself "
        "the problem -- it only moves once other functions in the TU are rewritten "
        "too (inlining or cross-function scheduling). Re-run converge with the "
        "other parked functions resolved before treating this as a finding.",
}


def _culprit_offsets(census_data, function, path, original, reference, tmp,
                     extra=(), oracle="vc71"):
    """Narrow one function's rewrites to the offsets that actually move codegen.

    Peels one culprit at a time rather than stopping at the first: several
    independent offsets can each move the same function, and reporting only one
    would send the reader off to explain a divergence that survives fixing it.
    """
    sites = [s for s in census_data["sites"]
             if s["function"] == function and s["verdict"] == "rewrite"]
    offsets = sorted({s["offset_hex"] for s in sites}, key=lambda h: int(h, 16))
    counter = [0]

    def moves(subset):
        counter[0] += 1
        path.write_text(original, encoding="utf-8")
        apply_rewrites(census_data, source=path, apply=True,
                       only=[function], offsets=subset)
        obj = compile_for(oracle, path, tmp / ("triage%d.obj" % counter[0]),
                          extra=extra)
        return function in diverging_functions(reference, obj)["changed"]

    # Delta-debug on the REMAINING set with found culprits excluded. Including
    # them would make every probe move by construction, so the loop would peel
    # every offset and report the whole function -- which is what it did before
    # this was fixed, and is indistinguishable from a real answer in the output.
    culprits, remaining, interacting = [], list(offsets), []
    while remaining and moves(set(remaining)):
        window = list(remaining)
        while len(window) > 1:
            mid = len(window) // 2
            first, second = window[:mid], window[mid:]
            if moves(set(first)):
                window = first
            elif moves(set(second)):
                window = second
            else:
                # Neither half moves alone but the whole does: the cause is an
                # interaction across the split, not any single offset. Report
                # the window rather than looping or picking one arbitrarily.
                interacting.append(list(window))
                window = None
                break
        if window is None:
            break
        culprits.append(window[0])
        remaining = [o for o in remaining if o != window[0]]
    return culprits, counter[0], interacting


def triage(census_data, functions, source=None, oracle="vc71"):
    """Explain each parked function: benign scheduling artefact, or a real finding?

    `converge` parks at function granularity, which is safe but silent about
    cause -- and the two causes that matter look identical from outside. This
    answers the question mechanically:

      * recompile the function's rewrites with -fno-strict-aliasing. If the
        divergence vanishes, it was the `char *` aliasing barrier and nothing
        more.
      * if it survives, bisect down to the individual offsets responsible, so
        a human inspects two lines instead of a hundred.

    Measured on actor_moving.c: of three parked functions, two were pure TBAA
    and the third reduced to two adjacent byte loads whose address form changed.
    None was a wrong binding. The source file is restored unconditionally.
    """
    path = Path(source or (ROOT / census_data["source"]))
    original = path.read_text(encoding="utf-8")
    tmp = _scratch(oracle, "structize-triage-")
    nsa = ("-fno-strict-aliasing",)
    findings = []
    try:
        reference = compile_for(oracle, path, tmp / "ref.obj")
        # The TBAA probe is `-fno-strict-aliasing`, a clang switch with no VC71
        # equivalent. Under the VC71 oracle the aliasing question is simply not
        # askable, so that verdict is withheld rather than faked.
        reference_nsa = (compile_tu(path, tmp / "ref_nsa.obj", extra=nsa)
                         if oracle == "clang" else None)
        for function in functions:
            path.write_text(original, encoding="utf-8")
            applied = apply_rewrites(census_data, source=path, apply=True,
                                     only=[function])
            moved = function in diverging_functions(
                reference, compile_for(oracle, path, tmp / "c.obj"))["changed"]
            moved_nsa = True
            if reference_nsa is not None:
                moved_nsa = function in diverging_functions(
                    reference_nsa,
                    compile_tu(path, tmp / "c_nsa.obj", extra=nsa))["changed"]

            entry = {"function": function, "sites": applied["sites_rewritten"],
                     "oracle": oracle}
            if not moved:
                entry["cause"] = CAUSE_COMBINATION
            elif not moved_nsa:
                entry["cause"] = CAUSE_TBAA
            else:
                culprits, probes, interacting = _culprit_offsets(
                    census_data, function, path, original,
                    reference_nsa if reference_nsa is not None else reference,
                    tmp, nsa if oracle == "clang" else (), oracle)
                entry["cause"] = CAUSE_ADDRESS_FORM
                entry["culprit_offsets"] = culprits
                entry["clean_offsets"] = len({
                    s["offset_hex"] for s in census_data["sites"]
                    if s["function"] == function and s["verdict"] == "rewrite"
                }) - len(culprits)
                entry["probe_compiles"] = probes
                if interacting:
                    entry["interacting_offsets"] = interacting
            entry["explanation"] = _CAUSE_TEXT[entry["cause"]]
            findings.append(entry)
        return {
            "ok": True,
            "source": census_data["source"],
            "findings": findings,
            "note": ("%r is a proof, %r is a lead. Byte-identical codegen under "
                     "-fno-strict-aliasing means the rewrite performs the same "
                     "accesses at the same addresses, so a wrong binding cannot "
                     "reach that verdict -- those parks are explained and benign. "
                     "%r only says 'not aliasing' and names the offsets involved; "
                     "a genuinely wrong binding lands here too, because reading the "
                     "wrong address also changes codegen and also survives the "
                     "flag. Read the culprit offsets against the disassembly before "
                     "calling them benign."
                     % (CAUSE_TBAA, CAUSE_ADDRESS_FORM, CAUSE_ADDRESS_FORM)),
        }
    finally:
        path.write_text(original, encoding="utf-8")
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
            apply_splits_too=True, oracle="vc71"):
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

    converged = converge(data, source=source, alias=alias, manifest=manifest,
                         oracle=oracle)
    report["oracle"] = oracle
    if converged.get("oracle_warning"):
        report["oracle_warning"] = converged["oracle_warning"]
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


def list_bindings():
    if not BINDINGS.is_file():
        return []
    data = json.loads(BINDINGS.read_text(encoding="utf-8"))
    return [row["id"] for row in data.get("bindings", []) if "id" in row]


# --------------------------------------------------------------------------
# Auto-discovery, campaign runner, and worklist
#
# These close the automation loop: an LLM agent calls `campaign --binding X`,
# gets a JSON report with `next_actions`, resolves one conflict, calls
# `campaign` again, and repeats until conflicts reach 0.  Every piece is
# deterministic; the only non-determinism is the LLM's RE judgement on each
# conflict, which the VC71 gate catches if wrong.
# --------------------------------------------------------------------------
def discover(binding_id):
    """Find every source file with raw offset dereferences for a binding.

    Returns a sorted list (most sites first, ties broken by filename) so that
    a campaign processes the highest-value files first and the order is stable.
    """
    binding = load_binding(binding_id)
    struct = binding["struct"]
    bases = binding.get("bases", [])
    if not bases:
        raise StructizeError("binding %r has no base variable names" % binding_id)
    glob_pattern = binding.get("sources_glob", "src/**/*.c")
    files = {}
    for source in sorted(ROOT.glob(glob_pattern)):
        if not source.is_file() or source.suffix != ".c":
            continue
        try:
            text = source.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        for base in bases:
            pattern = _deref_pattern(base)
            count = len(pattern.findall(text))
            if count > 0:
                rel = source.resolve().relative_to(ROOT).as_posix()
                entry = files.get(rel)
                if entry is None:
                    files[rel] = {"source": rel, "base": base, "sites": count}
                else:
                    entry["sites"] += count

    ordered = sorted(files.values(),
                     key=lambda f: (-f["sites"], f["source"]))
    return {
        "binding": binding_id,
        "struct": struct,
        "bases": bases,
        "files": ordered,
        "total_files": len(ordered),
        "total_sites": sum(f["sites"] for f in ordered),
    }


def _conflict_hint(conflict):
    """Generate a resolution hint from the conflict's reason string."""
    reason = conflict.get("reason", "")
    if "float" in reason and "int" in reason:
        return ("Check instruction class at accesses to this offset: "
                "FLD/FSTP (float) vs MOV (integer) -- may be a union")
    if "widths" in reason:
        return ("Check operand size in disassembly at accesses to this "
                "offset: dword (32-bit) vs word (16-bit) vs byte (8-bit)")
    if "signedness" in reason:
        return ("Check sign extension: MOVSX (signed) vs MOVZX (unsigned) "
                "at accesses to this offset")
    return "Inspect disassembly at accesses to this offset to determine the correct type"


def _aggregate_conflicts(per_file, struct_name):
    """Deduplicate conflicts across files, summing blocked sites."""
    by_offset = {}
    for source, conflicts in per_file:
        for c in conflicts:
            key = c["offset_hex"]
            if key not in by_offset:
                by_offset[key] = {
                    "offset_hex": key,
                    "struct": struct_name,
                    "reason": c.get("reason", ""),
                    "sites_blocked": 0,
                    "files": [],
                    "functions_by_type": {},
                }
            entry = by_offset[key]
            entry["sites_blocked"] += c.get("sites", 0)
            if source not in entry["files"]:
                entry["files"].append(source)
            for desc, fns in c.get("functions_by_type", {}).items():
                merged = entry["functions_by_type"].setdefault(desc, [])
                for fn in fns:
                    if fn and fn not in merged:
                        merged.append(fn)
    for entry in by_offset.values():
        entry["resolution_hint"] = _conflict_hint(entry)
    return sorted(by_offset.values(),
                  key=lambda c: (-c["sites_blocked"], c["offset_hex"]))


def _next_actions(conflicts, parked):
    """Generate a ranked list of actionable items for the LLM."""
    actions = []
    for c in conflicts:
        actions.append("resolve conflict at %s+%s: %s (%d sites blocked)"
                       % (c["struct"], c["offset_hex"],
                          c["reason"].split(" -- ")[0] if " -- " in c["reason"]
                          else c["reason"], c["sites_blocked"]))
    for source, funcs in sorted(parked.items()):
        for fn in funcs:
            actions.append("triage parked function %s in %s" % (fn, source))
    return actions


def campaign(binding_id, oracle="vc71", dry_run=False, census_dir=None):
    """Run structize across every source file that touches a binding's struct.

    This is the self-feeding loop entry point.  An LLM agent calls it, reads
    the `next_actions` list, resolves one item, calls it again, and repeats.
    """
    info = discover(binding_id)
    binding = load_binding(binding_id)
    struct = info["struct"]
    census_dir = Path(census_dir or (ROOT / "recovery" / "census"))
    census_dir.mkdir(parents=True, exist_ok=True)

    report = {
        "ok": True,
        "binding": binding_id,
        "struct": struct,
        "oracle": oracle,
        "files_discovered": info["total_files"],
        "total_sites_discovered": info["total_sites"],
    }

    if dry_run:
        report["dry_run"] = True
        report["files"] = info["files"]
        return report

    results = []
    all_conflicts = []
    all_parked = {}
    total_rewritten = 0
    total_eligible = 0
    errors = []

    for entry in info["files"]:
        source = ROOT / entry["source"]
        base = entry["base"]
        alias = binding.get("alias")
        try:
            result = run_all(str(source), base, struct,
                             census_dir=str(census_dir), alias=alias,
                             oracle=oracle)
        except StructizeError as exc:
            errors.append({"source": entry["source"], "error": str(exc)})
            results.append({"source": entry["source"], "base": base,
                            "ok": False, "error": str(exc)})
            continue

        rewritten = 0
        parked_fns = []
        for step in result.get("steps", []):
            if step.get("step") == "converge":
                rewritten = step.get("sites_rewritten", 0)
                parked_fns = step.get("parked", [])

        total_rewritten += rewritten
        total_eligible += result.get("steps", [{}])[-1].get("rewrite", 0)

        results.append({
            "source": entry["source"],
            "base": base,
            "sites_rewritten": rewritten,
            "parked_functions": parked_fns,
            "ok": result.get("ok", False),
        })

        if result.get("conflicts"):
            all_conflicts.append((entry["source"], result["conflicts"]))
        if parked_fns:
            all_parked[entry["source"]] = parked_fns

    conflicts = _aggregate_conflicts(all_conflicts, struct)
    report["files_processed"] = len(results)
    report["total_sites_rewritten"] = total_rewritten
    report["results"] = results
    report["conflicts"] = conflicts
    report["conflicts_total"] = len(conflicts)
    report["sites_blocked_by_conflicts"] = sum(c["sites_blocked"]
                                               for c in conflicts)
    report["parked_functions"] = all_parked
    report["parked_functions_total"] = sum(len(v) for v in all_parked.values())
    report["next_actions"] = _next_actions(conflicts, all_parked)
    if errors:
        report["errors"] = errors
    return report


def worklist(binding_id):
    """Census-only conflict and refuse aggregation -- no compiles.

    Cheaper than `campaign`: runs `census()` + `plan_splits()` per file to
    report what is left to resolve, without modifying any files.
    """
    info = discover(binding_id)
    struct = info["struct"]
    layout = struct_layout(struct)

    all_conflicts = []
    per_file = []
    total_rewrite = 0
    total_split = 0
    total_refuse = 0

    for entry in info["files"]:
        source = ROOT / entry["source"]
        base = entry["base"]
        try:
            data = census(str(source), base, struct, layout=layout)
        except StructizeError:
            continue
        plan = plan_splits(data, layout=layout)
        summary = data.get("summary", {})
        total_rewrite += summary.get("rewrite", 0)
        total_split += summary.get("split", 0)
        total_refuse += summary.get("refuse", 0)

        if plan.get("conflicts"):
            all_conflicts.append((entry["source"], plan["conflicts"]))

        per_file.append({
            "source": entry["source"],
            "base": base,
            "rewrite": summary.get("rewrite", 0),
            "split": summary.get("split", 0),
            "refuse": summary.get("refuse", 0),
        })

    conflicts = _aggregate_conflicts(all_conflicts, struct)
    return {
        "binding": binding_id,
        "struct": struct,
        "files": per_file,
        "total_rewrite": total_rewrite,
        "total_split": total_split,
        "total_refuse": total_refuse,
        "conflicts": conflicts,
        "conflicts_total": len(conflicts),
        "sites_blocked_by_conflicts": sum(c["sites_blocked"]
                                          for c in conflicts),
        "next_actions": _next_actions(conflicts, {}),
    }


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
    p.add_argument("--oracle", choices=("vc71", "clang"), default="vc71",
                   help="compiler that arbitrates neutrality. Default vc71 -- the "
                        "binary was built by MSVC 7.1 and nothing else is evidence.")

    p = sub.add_parser("triage",
                       help="explain WHY converge parked a function (and which offsets)")
    p.add_argument("--census", required=True)
    p.add_argument("--functions", nargs="*",
                   help="parked function names; default is every function that "
                        "diverges when the whole census is applied")
    p.add_argument("--oracle", choices=("vc71", "clang"), default="vc71",
                   help="compiler that arbitrates neutrality. Default vc71 -- the "
                        "binary was built by MSVC 7.1 and nothing else is evidence.")

    p = sub.add_parser("sync", help="write census outcomes back into a manifest")
    p.add_argument("--census", required=True)
    p.add_argument("--manifest", required=True)
    p.add_argument("--parked", nargs="*", default=[], metavar="FUNC")
    p.add_argument("--apply", action="store_true")

    p = sub.add_parser("run", help="census -> split -> re-census -> converge (use this)")
    p.add_argument("--source", required=True)
    p.add_argument("--base", default=None)
    p.add_argument("--struct", default=None)
    p.add_argument("--binding",
                   help="look up --base/--struct from recovery/bindings.json")
    p.add_argument("--manifest")
    p.add_argument("--alias")
    p.add_argument("--census-dir")
    p.add_argument("--no-split", action="store_true",
                   help="do not touch types.h; convert only what already resolves")
    p.add_argument("--oracle", choices=("vc71", "clang"), default="vc71",
                   help="compiler that arbitrates neutrality (default vc71)")

    p = sub.add_parser("baseline", help="capture the pre-change COFF snapshot")
    p.add_argument("--source", required=True)
    p.add_argument("-o", "--output", required=True)

    p = sub.add_parser("gate", help="rebuild and compare .text to the baseline")
    p.add_argument("--source", required=True)
    p.add_argument("--baseline", required=True)
    p.add_argument("--no-build", action="store_true")

    p = sub.add_parser("status", help="one-screen census summary")
    p.add_argument("--census", required=True)

    p = sub.add_parser("discover",
                       help="find source files with raw offset dereferences for a binding")
    p.add_argument("--binding", required=True,
                   help="binding id from recovery/bindings.json")

    p = sub.add_parser("campaign",
                       help="run structize across every file touching a binding's struct")
    p.add_argument("--binding", required=True,
                   help="binding id from recovery/bindings.json")
    p.add_argument("--dry-run", action="store_true",
                   help="discover files and report the plan without executing")
    p.add_argument("--oracle", choices=("vc71", "clang"), default="vc71")
    p.add_argument("--census-dir")

    p = sub.add_parser("worklist",
                       help="census-only conflict aggregation without compiling")
    p.add_argument("--binding", required=True,
                   help="binding id from recovery/bindings.json")

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
            result = converge(data, alias=args.alias, manifest=args.manifest,
                              oracle=args.oracle)
            _emit(result)
            if not result.get("ok"):
                return 1
            # 2 = converged but rewrote nothing, so a caller polling the exit
            # code can tell "no work available" from "work done".
            return 2 if result.get("vacuous") else 0
        if args.command == "triage":
            data = json.loads(Path(args.census).read_text(encoding="utf-8"))
            targets = args.functions
            if not targets:
                # Discovering the parked set here means the caller does not have
                # to re-run converge (which rewrites the file) just to name it.
                path = ROOT / data["source"]
                original = path.read_text(encoding="utf-8")
                scratch = _scratch(args.oracle, "structize-find-")
                try:
                    reference = compile_for(args.oracle, path, scratch / "ref.obj")
                    apply_rewrites(data, source=path, apply=True)
                    candidate = compile_for(args.oracle, path, scratch / "all.obj")
                    targets = diverging_functions(reference, candidate)["changed"]
                finally:
                    path.write_text(original, encoding="utf-8")
                    shutil.rmtree(scratch, ignore_errors=True)
            if not targets:
                _emit({"ok": True, "findings": [],
                       "note": "nothing parked -- every eligible site converges"})
                return 0
            _emit(triage(data, sorted(targets), oracle=args.oracle))
            return 0
        if args.command == "sync":
            data = json.loads(Path(args.census).read_text(encoding="utf-8"))
            _emit(sync_manifest(data, args.manifest, args.parked,
                                "function parked by converge", apply=args.apply))
            return 0
        if args.command == "run":
            base = args.base
            struct = args.struct
            alias = args.alias
            if args.binding:
                b = load_binding(args.binding)
                struct = struct or b["struct"]
                base = base or b["bases"][0]
                alias = alias or b.get("alias")
            if not base or not struct:
                parser.error("--base/--struct or --binding required")
            result = run_all(args.source, base, struct,
                             census_dir=args.census_dir, manifest=args.manifest,
                             alias=alias, apply_splits_too=not args.no_split,
                             oracle=args.oracle)
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
        if args.command == "discover":
            _emit(discover(args.binding))
            return 0
        if args.command == "campaign":
            result = campaign(args.binding, oracle=args.oracle,
                              dry_run=args.dry_run, census_dir=args.census_dir)
            _emit(result)
            if not result.get("ok"):
                return 1
            return 0
        if args.command == "worklist":
            _emit(worklist(args.binding))
            return 0
        parser.error("a subcommand is required")
    except (OSError, ValueError, StructizeError) as exc:
        _emit({"ok": False, "errors": [str(exc)]})
        return 2


if __name__ == "__main__":
    sys.exit(main())
