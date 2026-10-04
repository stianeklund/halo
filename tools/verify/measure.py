#!/usr/bin/env python3
"""List dormant lifts and rank them by raw-XBE byte accuracy.

A dormant lift is a kb.json function with ported != true whose implementation
already exists in src/.  Each one is compiled with VC71 and compared against
the pristine raw XBE with raw_xbe_structural.audit().  Read-only for kb.json,
src/ and artifacts/raw_xbe_structural/: records go to
artifacts/dormant/<addr>-<name>.json, a ranked report to
artifacts/dormant/report.json and a prioritized worklist to
artifacts/dormant/worklist.md (+ .json).

Beyond the structural record, every function gets:

  * a resolved definition: the real C definition (not an extern prototype, not
    code under `#if 0`, only files that src/CMakeLists.txt compiles), and the
    exact COFF symbol (`_f`, `__f` for a C name with a leading underscore,
    `@f@N` for a register-argument function compiled as __fastcall);
  * a second alignment (the same DP run over the reversed instruction lists,
    which picks the other extreme among tied optimal paths).  When both agree
    the score does not depend on tie-breaking and is not provisional;
  * alignment-independent behaviour signals: the CALL/tail-JMP target
    sequence, writable-global and code-pointer operands, `ret N` forms, frame
    size, conditional-branch and instruction counts;
  * a bucket: exact / exact_pending_identity / suspect_bug / codegen /
    not_measurable.

Incremental: a record is reused when its key (tool hashes, TU source hash,
src/ header hash, decl.h hash, function_bounds.json hash, kb decl, opt, COFF
symbol) is unchanged.  --force re-measures everything.

    rtk python3 tools/verify/measure.py --list
    rtk python3 tools/verify/measure.py --workers 6
    rtk python3 tools/verify/measure.py --object director.obj --force
    rtk python3 tools/verify/measure.py --report
"""
import argparse
import collections
import concurrent.futures
import hashlib
import json
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "tools" / "verify"))
sys.path.insert(0, str(ROOT / "tools" / "lift"))  # llm_auto_lift
OUT = ROOT / "artifacts" / "dormant"
SRC = ROOT / "src"
# Bump when audit_tu/_signals change what a record contains; report-only
# changes (summarize, worklist) need no re-measure.
TOOL_VERSION = "dormant-6"
CRT_HELPERS = {"FUN_001d90e0", "FUN_001d9068", "_chkstk", "_ftol2"}
XDK_OBJECT = "<xdk_stubs>"
EXACT = 1.0 - 1e-12


# --------------------------------------------------------------------------
# kb.json
# --------------------------------------------------------------------------

def _decl_name(decl):
    match = re.search(r"(\w+)\s*\(", re.sub(r"@<\w+>", "", decl or ""))
    return match.group(1) if match else ""


def load_kb():
    return json.loads((ROOT / "kb.json").read_text(encoding="utf-8"))


def kb_function_maps(kb):
    """name -> address for unique kb function names, and address -> name."""
    by_name = collections.defaultdict(set)
    by_addr = {}
    for obj in kb["objects"]:
        for func in obj.get("functions", []):
            try:
                addr = int(func.get("addr", ""), 16)
            except ValueError:
                continue
            name = _decl_name(func.get("decl", ""))
            if name:
                by_name[name].add(addr)
                by_addr.setdefault(addr, name)
    return {n: next(iter(a)) for n, a in by_name.items() if len(a) == 1}, by_addr


# --------------------------------------------------------------------------
# Source definitions
# --------------------------------------------------------------------------

_KEYWORDS = {"if", "while", "for", "switch", "return", "sizeof", "do", "else"}


def compiled_sources():
    text = (SRC / "CMakeLists.txt").read_text(encoding="utf-8")
    found = set()
    for match in re.finditer(r'"([^"\n]+?\.c)"|([^\s"()]+\.c)\b', text):
        rel = match.group(1) or match.group(2)
        path = (SRC / rel).resolve()
        if path.is_file():
            found.add(path)
    return found


def _strip_comments_and_literals(text):
    """Blank comments and string/char literals, keeping every newline."""
    out = []
    i, n = 0, len(text)
    while i < n:
        ch = text[i]
        if ch == "/" and i + 1 < n and text[i + 1] == "*":
            end = text.find("*/", i + 2)
            end = n if end < 0 else end + 2
            out.append(re.sub(r"[^\n]", " ", text[i:end]))
            i = end
        elif ch == "/" and i + 1 < n and text[i + 1] == "/":
            end = text.find("\n", i)
            end = n if end < 0 else end
            out.append(" " * (end - i))
            i = end
        elif ch in "\"'":
            j = i + 1
            while j < n and text[j] != ch and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            out.append(ch + " " * (min(j, n) - i - 1) + (ch if j < n else ""))
            i = j + 1
        else:
            out.append(ch)
            i += 1
    return "".join(out)


def _preprocess_active(text):
    """Return (active_text, full_text): `#if 0` regions blanked in the first.

    Only a literal `#if 0` / `#if 1` is evaluated; every other condition is
    treated as active, which is how the build sees this repository's
    configuration-free sources.  Directive lines are blanked in both texts.
    """
    active, full = [], []
    stack = []  # {"outer": enclosing region active, "on": this branch active, "literal"}

    def enabled():
        return not stack or (stack[-1]["outer"] and stack[-1]["on"])

    continuation = False
    for line in text.split("\n"):
        if continuation:  # body of a multi-line #define
            continuation = line.rstrip().endswith("\\")
            active.append("")
            full.append("")
            continue
        directive = re.match(r"\s*#\s*(\w+)\s*(.*)", line)
        if directive:
            continuation = line.rstrip().endswith("\\")
            word, rest = directive.group(1), directive.group(2).strip()
            if word in ("if", "ifdef", "ifndef"):
                literal = word == "if" and rest in ("0", "1")
                stack.append({"outer": enabled(), "on": not (literal and rest == "0"),
                              "literal": literal})
            elif word == "else" and stack:
                stack[-1]["on"] = not stack[-1]["on"] if stack[-1]["literal"] else True
            elif word == "elif" and stack:
                stack[-1].update(on=True, literal=False)
            elif word == "endif" and stack:
                stack.pop()
            active.append("")
            full.append("")
            continue
        active.append(line if enabled() else "")
        full.append(line)
    return "\n".join(active), "\n".join(full)


def _function_name_of_header(header):
    """The name in `... name(params)` when the header ends in a parameter list."""
    text = header.rstrip()
    if not text.endswith(")"):
        return None, None
    depth = 0
    for i in range(len(text) - 1, -1, -1):
        if text[i] == ")":
            depth += 1
        elif text[i] == "(":
            depth -= 1
            if depth == 0:
                match = re.search(r"(\w+)\s*$", text[:i])
                if not match:
                    return None, None
                return match.group(1), match.start(1)
    return None, None


def _definitions(text):
    """Yield (name, line, header) for every file-scope function definition."""
    depth = 0
    header_start = 0
    i = 0
    n = len(text)
    while i < n:
        ch = text[i]
        if ch == "{":
            if depth == 0:
                header = text[header_start:i]
                name, start = _function_name_of_header(header)
                if name and name not in _KEYWORDS and "=" not in header[:start]:
                    at = header_start + start
                    yield name, text.count("\n", 0, at) + 1, header
            depth += 1
        elif ch == "}":
            depth = max(0, depth - 1)
            if depth == 0:
                header_start = i + 1
        elif ch == ";" and depth == 0:
            header_start = i + 1
        i += 1


def source_index():
    """name -> list of definition sites over every src/**/*.c file."""
    compiled = compiled_sources()
    index = collections.defaultdict(list)
    for path in sorted(SRC.rglob("*.c")):
        try:
            text = _strip_comments_and_literals(path.read_text(encoding="utf-8", errors="ignore"))
        except OSError:
            continue
        active, full = _preprocess_active(text)
        live = {(name, line) for name, line, _h in _definitions(active)}
        for name, line, header in _definitions(full):
            index[name].append({
                "source": str(path.relative_to(ROOT)), "line": line,
                "compiled": path.resolve() in compiled,
                "disabled": (name, line) not in live,
                "naked": "naked" in header})
    return index


def resolve_definition(obj, addr, name, index):
    """Pick the definition the build would link, or say why there is none."""
    fun = "FUN_%08x" % addr
    names = [name] + ([fun] if fun != name else [])
    sites = [dict(site, symbol=n) for n in names for site in index.get(n, [])]
    live = [s for s in sites if s["compiled"] and not s["disabled"]]
    result = {"sites": sites}
    if not live:
        result["status"] = "not_implemented"
        if sites:
            why = sorted({"disabled (#if 0)" if s["disabled"] else "file not in src/CMakeLists.txt"
                          for s in sites})
            result["reason"] = "definition exists only %s" % " / ".join(why)
        else:
            result["reason"] = "no C definition in src/ (prototype or mention only)"
        return result
    own = [s for s in live if s["symbol"] == name] or live
    stem = obj.rsplit(".", 1)[0]
    own.sort(key=lambda s: (Path(s["source"]).stem != stem, s["source"], s["line"]))
    chosen = own[0]
    result.update(status="implemented", source=chosen["source"], line=chosen["line"],
                  symbol=chosen["symbol"], naked=chosen["naked"])
    if chosen["symbol"] != name:
        result["stale_source_name"] = True
    if len({(s["source"], s["symbol"]) for s in live}) > 1:
        result["duplicate_definitions"] = ["%s:%d %s" % (s["source"], s["line"], s["symbol"])
                                           for s in live]
    return result


_SOURCE_INDEX = {}


def defined_in(name, source):
    """True when `name` has a live definition in the given TU."""
    return any(site["source"] == source and not site["disabled"]
               for site in _SOURCE_INDEX.get(name, []))


def enumerate_dormant(object_filter=None):
    """Every non-ported kb function that has a definition or a source mention."""
    sys.argv = ["measure"]  # llm_auto_lift parses argv on import in some paths
    import llm_auto_lift as lift

    kb = load_kb()
    index = source_index()
    _SOURCE_INDEX.clear()
    _SOURCE_INDEX.update(index)
    rows = []
    for obj in kb["objects"]:
        if object_filter and obj.get("name") != object_filter:
            continue
        for func in obj.get("functions", []):
            if func.get("ported"):
                continue
            decl = func.get("decl", "")
            name = _decl_name(decl)
            try:
                addr = int(func.get("addr", ""), 16)
            except ValueError:
                continue
            if not name:
                continue
            definition = resolve_definition(obj["name"], addr, name, index)
            mention = lift._is_already_in_source("0x%x" % addr, name)
            if definition["status"] != "implemented" and not mention:
                continue
            row = {"object": obj["name"], "addr": "0x%x" % addr, "name": name, "decl": decl,
                   "definition": definition}
            if definition["status"] == "implemented":
                row["source"], row["line"] = definition["source"], definition["line"]
            elif mention:
                path, _, line = mention.rpartition(":")
                row["mention"] = mention
            rows.append(row)
    return sorted(rows, key=lambda r: (r["object"], int(r["addr"], 0)))


# --------------------------------------------------------------------------
# Measurement
# --------------------------------------------------------------------------

def record_path(row):
    return OUT / ("%08x-%s.json" % (int(row["addr"], 0), row["name"]))


def _sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def environment_hashes():
    headers = hashlib.sha256()
    for path in sorted(SRC.rglob("*.h")):
        headers.update(str(path.relative_to(ROOT)).encode())
        headers.update(path.read_bytes())
    return {"tool": TOOL_VERSION,
            "structural": _sha(ROOT / "tools" / "verify" / "raw_xbe_structural.py"),
            "headers": headers.hexdigest(),
            "decl": _sha(ROOT / "build" / "generated" / "decl.h"),
            "bounds": _sha(ROOT / "tools" / "verify" / "function_bounds.json"),
            "cmake": _sha(SRC / "CMakeLists.txt")}


def measurement_key(row, env, opt):
    parts = dict(env, source=_sha(ROOT / row["source"]), decl_text=row["decl"], opt=opt,
                 symbol=row["definition"]["symbol"])
    return hashlib.sha256(json.dumps(parts, sort_keys=True).encode()).hexdigest()


def _c_name(name):
    if name and name[0] in "_@":
        name = name[1:]
    if name and "@" in name and name.rsplit("@", 1)[1].isdigit():
        name = name.rsplit("@", 1)[0]
    return name


def _coff_function_symbols(path):
    import raw_xbe_structural as rxs
    data = Path(path).read_bytes()
    _m, _n, _t, symbol_offset, symbol_count, _o, _c = rxs.COFF_FILE_HEADER.unpack_from(data, 0)
    symbol_end = symbol_offset + symbol_count * rxs.COFF_SYMBOL.size
    strings = data[symbol_end:symbol_end + struct.unpack_from("<I", data, symbol_end)[0]]
    names = []
    index = 0
    while index < symbol_count:
        raw, _v, section, typ, _s, aux = rxs.COFF_SYMBOL.unpack_from(
            data, symbol_offset + index * rxs.COFF_SYMBOL.size)
        if section > 0 and typ == rxs.IMAGE_SYM_DTYPE_FUNCTION:
            names.append(rxs._coff_name(raw, strings))
        index += 1 + aux
    return names


def _reversed_alignment(original):
    def align(candidate_insns, reference_insns):
        pairs, method, ambiguous = original(candidate_insns[::-1], reference_insns[::-1])
        return pairs[::-1], method + "+reversed", ambiguous
    return align


def _second_alignment(rxs, candidate, relocs, reference, address):
    original = rxs._instruction_alignment
    rxs._instruction_alignment = _reversed_alignment(original)
    try:
        return rxs.aligned_byte_compare(candidate, relocs, reference, address)
    finally:
        rxs._instruction_alignment = original


def _xbe_ranges(rxs):
    sys.path.insert(0, str(ROOT / "tools" / "equivalence"))
    import xbe_image
    _raw, sections = xbe_image.load_xbe(str(rxs.xref.XBE))
    return {s.name: (s.va, s.va + s.vsize) for s in sections}


_XBE = None


def _xbe_read(address, size):
    """Raw XBE bytes at a virtual address (any initialized section), else None."""
    global _XBE
    if _XBE is None:
        sys.path.insert(0, str(ROOT / "tools" / "equivalence"))
        import xbe_image
        import raw_xbe_structural as rxs
        _XBE = (xbe_image, xbe_image.load_xbe(str(rxs.xref.XBE)))
    xbe_image, (raw, sections) = _XBE
    section = xbe_image.section_at(sections, address)
    if section is None or address + size > section.va + section.raw_size:
        return None
    start = section.raw_off + (address - section.va)
    return raw[start:start + size]


def _in(ranges, name, value):
    low, high = ranges.get(name, (0, 0))
    return low <= value < high


def _signals(rxs, candidate, relocs, reference, address, maps, ranges):
    """Alignment-independent behaviour evidence for one function."""
    name_to_addr, addr_to_name = maps

    def resolve(symbol_name):
        norm = _c_name(symbol_name or "")
        fun = re.fullmatch(r"FUN_([0-9a-fA-F]{8})", norm or "")
        if fun:
            return int(fun.group(1), 16)
        if norm in name_to_addr:
            return name_to_addr[norm]
        if norm in rxs._CRT_HELPER_ADDRESSES:
            return rxs._CRT_HELPER_ADDRESSES[norm]
        found = rxs._raw_addresses_for_name(symbol_name)
        if len(found) == 1:
            return found[0]
        data = rxs._kb_data_addresses()
        if norm in data:
            return data[norm]
        return "?" + (norm or "")

    def side(code, relocations, is_candidate):
        insns = rxs._decode_instructions(code) or []
        fields = rxs._decode_fields(code) or {}
        reloc_at = {r["offset"]: r for r in relocations}
        calls, data_refs, rets = [], [], []
        frame = None
        jcc = 0
        for position, insn in enumerate(insns):
            mnemonic = insn["mnemonic"]
            first = insn["bytes"][0] if insn["bytes"] else None
            if mnemonic.startswith("j") and mnemonic != "jmp":
                jcc += 1
            if mnemonic == "ret":
                rets.append(insn["op_str"] or "0")
            if (frame is None and position < 8 and mnemonic == "sub" and
                    insn["op_str"].startswith("esp, ")):
                frame = insn["op_str"][5:]
            if mnemonic in ("call", "jmp") and first in (0xE8, 0xE9):
                operand = insn["offset"] + 1
                if is_candidate:
                    reloc = reloc_at.get(operand)
                    if reloc is None or reloc.get("local_offset") is not None:
                        continue  # intra-function branch
                    target = resolve((reloc.get("symbol") or {}).get("name"))
                else:
                    target = (address + insn["offset"] + insn["size"] +
                              struct.unpack_from("<i", code, operand)[0]) & 0xFFFFFFFF
                    if (mnemonic == "jmp" and target != address and
                            address <= target < address + len(code)):
                        continue  # intra-function jump; a call to self is recursion
                calls.append(("tail " if mnemonic == "jmp" else "") +
                              (addr_to_name.get(target, "0x%x" % target)
                               if isinstance(target, int) else target))
            elif mnemonic == "call":
                calls.append("indirect")
        for offset, field in fields.items():
            if field["branch"]:
                continue
            reloc = reloc_at.get(offset) if is_candidate else None
            if reloc is not None:
                if reloc.get("literal") is not None or reloc.get("local_offset") is not None:
                    continue
                base = resolve((reloc.get("symbol") or {}).get("name"))
                if not isinstance(base, int):
                    data_refs.append(base)
                    continue
                value = (base + struct.unpack_from("<I", code, offset)[0]) & 0xFFFFFFFF
            else:
                value = struct.unpack_from("<I", code, offset)[0]
            if _in(ranges, ".data", value):
                data_refs.append("0x%x" % value)
            elif _in(ranges, ".text", value) and value in addr_to_name:
                # Only function pointers: other .text operands are this
                # function's own switch tables and labels.
                data_refs.append("&" + addr_to_name[value])
        return {"calls": calls, "data_refs": sorted(data_refs), "rets": sorted(set(rets)),
                "frame": frame, "jcc": jcc, "instructions": len(insns)}

    c = side(candidate, relocs, True)
    r = side(reference, [], False)
    counter = collections.Counter

    # A switch table whose case targets lie outside [address, end) means the
    # committed bound cut the function short: the reference is truncated and
    # every score against it is a tooling artifact.
    escaped = set()
    for insn in rxs._decode_instructions(reference) or []:
        table = re.fullmatch(r"dword ptr \[e\w\w\*4 \+ (0x[0-9a-f]+)\]", insn["op_str"])
        if insn["mnemonic"] != "jmp" or not table:
            continue
        base = int(table.group(1), 16)
        for index in range(256):
            entry = _xbe_read(base + 4 * index, 4)
            if entry is None:
                break
            target = struct.unpack("<I", entry)[0]
            if not _in(ranges, ".text", target) or abs(target - address) > 0x10000:
                break
            if not address <= target < address + len(reference):
                escaped.add("0x%x" % target)

    # Read-only literals (strings, float/double constants): each candidate
    # literal must be found at some .rdata operand of the original, with the
    # same bytes.  A literal with no such home is a different constant.
    rdata_targets = set()
    for offset, field in (rxs._decode_fields(reference) or {}).items():
        if not field["branch"]:
            value = struct.unpack_from("<I", reference, offset)[0]
            if _in(ranges, ".rdata", value):
                rdata_targets.add(value)
    literal_missing = []
    literal_equal = []
    rdata_values = set()
    for target in rdata_targets:
        for fmt, size in (("<f", 4), ("<d", 8)):
            raw = _xbe_read(target, size)
            if raw is not None:
                rdata_values.add(struct.unpack(fmt, raw)[0])
    for reloc in relocs:
        literal = reloc.get("literal")
        if literal is None:
            continue
        content = literal["content"]
        addend = struct.unpack_from("<I", candidate, reloc["offset"])[0]
        found = any(rxs._xbe_rdata_bytes((t - addend) & 0xFFFFFFFF, len(content)) == content
                    for t in rdata_targets)
        if not found and len(content) in (4, 8):
            # A float/double constant whose value (or negation) the original
            # loads in another width, or 0/±1 (FLDZ/FLD1), is an encoding
            # difference, not a different constant.
            value = struct.unpack("<f" if len(content) == 4 else "<d", content)[0]
            if value in (0.0, 1.0, -1.0) or value in rdata_values or -value in rdata_values:
                literal_equal.append("%s=%r" % (content.hex(), value))
                continue
        if not found:
            text = content.rstrip(b"\0")
            printable = text and all(32 <= b < 127 for b in text)
            literal_missing.append(repr(text.decode("ascii")) if printable else content.hex())
    counter = collections.Counter
    out = {"candidate": c, "reference": r,
           "calls_missing": sorted((counter(r["calls"]) - counter(c["calls"])).elements()),
           "calls_extra": sorted((counter(c["calls"]) - counter(r["calls"])).elements()),
           "call_order_differs": (counter(c["calls"]) == counter(r["calls"]) and
                                  c["calls"] != r["calls"]),
           "data_missing": sorted((counter(r["data_refs"]) - counter(c["data_refs"])).elements()),
           "data_extra": sorted((counter(c["data_refs"]) - counter(r["data_refs"])).elements()),
           "literals_missing": literal_missing,
           "literals_value_equal": literal_equal,
           "reference_truncated": sorted(escaped),
           "rdata_ref_delta": (sum(1 for x in relocs if x.get("literal") is not None) -
                               sum(1 for off, f in (rxs._decode_fields(reference) or {}).items()
                                   if not f["branch"] and _in(ranges, ".rdata", struct.unpack_from(
                                       "<I", reference, off)[0]))),
           "ret_differs": c["rets"] != r["rets"],
           "frame_differs": c["frame"] != r["frame"],
           "jcc_delta": c["jcc"] - r["jcc"],
           "instruction_delta": c["instructions"] - r["instructions"]}
    return out


def audit_tu(source, items, decl_hash, maps):
    """Compile one TU per opt, then audit every requested function in it."""
    import raw_xbe_structural as rxs
    from datetime import datetime, timezone

    ranges = _xbe_ranges(rxs)
    source_sha = rxs._hash_path(source)
    results = []
    by_opt = collections.defaultdict(list)
    for item in items:
        by_opt[item["opt"]].append(item)
    for opt, opt_items in sorted(by_opt.items()):
        obj = rxs._candidate_object(OUT, source, opt)
        if not rxs.vc71.compile_vc71(source, obj, opt=opt):
            for item in opt_items:
                record = rxs._record_error(item, "VC71 compilation failed at %s" % opt, source_sha)
                results.append((item, record))
            continue
        symbols = _coff_function_symbols(obj)
        for item in opt_items:
            matches = [s for s in symbols if _c_name(s) == item["symbol"]]
            if len(matches) != 1:
                record = rxs._record_error(
                    item, "candidate has %d COFF function symbols for %s (%s)" %
                    (len(matches), item["symbol"], ", ".join(matches) or "none"), source_sha)
                results.append((item, record))
                continue
            symbol = matches[0]
            try:
                record = rxs.audit(obj, symbol, item["address"], source)
                candidate, relocs, _prov = rxs._parse_coff(obj, symbol)
                reference, _error = rxs.reference_code(item["address"])
                extra = {}
                if reference is not None:
                    second = _second_alignment(rxs, candidate, relocs, reference, item["address"])
                    extra["second_alignment"] = {
                        key: second.get(key) for key in (
                            "method", "byte_accuracy", "byte_accuracy_upper_bound",
                            "matching_bytes", "compared_bytes", "difference_classes",
                            "unpaired_relocations", "uncertain_relocations")}
                    extra["signals"] = _signals(rxs, candidate, relocs, reference,
                                                item["address"], maps, ranges)
            except (OSError, rxs.strict.NotComparable) as exc:
                record = rxs._record_error(item, str(exc), source_sha)
                extra = {}
            record["function"] = item["function"]
            record["coff_symbol"] = symbol
            record.update(extra)
            record["generated_at"] = datetime.now(timezone.utc).isoformat()
            record["tool"] = {"version": "2", "compiler": rxs._compiler_token(), "opt": opt,
                              "decl_sha256": decl_hash, "bounds_sha256": rxs._bounds_hash(),
                              "reference_authority": "pristine raw XBE + bounds"}
            results.append((item, record))
    return results


def measure_all(rows, workers, force):
    """Audit stale rows, one VC71 compile per (TU, opt), TUs in parallel.

    Never writes artifacts/raw_xbe_structural/summary.json.
    """
    import raw_xbe_structural as rxs

    if not rxs.vc71.regen_decl_header(quiet=True):
        sys.exit("declaration header regeneration failed")
    decl_hash = rxs._decl_hash()
    env = environment_hashes()
    kb = load_kb()
    maps = kb_function_maps(kb)
    by_source = collections.defaultdict(list)
    skipped = 0
    for row in rows:
        if row["definition"]["status"] != "implemented":
            continue
        source = ROOT / row["source"]
        opt = rxs._function_opt(source, row["definition"]["symbol"])
        key = measurement_key(row, env, opt)
        path = record_path(row)
        if not force and path.is_file():
            try:
                old = json.loads(path.read_text(encoding="utf-8"))
            except ValueError:
                old = {}
            if (old.get("dormant") or {}).get("key") == key:
                skipped += 1
                continue
        by_source[source].append({"function": row["name"], "symbol": row["definition"]["symbol"],
                                  "address": int(row["addr"], 0), "source": source,
                                  "opt": opt, "key": key})
    print("measuring %d functions in %d TUs (%d unchanged, reused)" %
          (sum(len(v) for v in by_source.values()), len(by_source), skipped), flush=True)
    done = 0
    with concurrent.futures.ProcessPoolExecutor(max_workers=workers) as pool:
        futures = {pool.submit(audit_tu, src, items, decl_hash, maps): (src, items)
                   for src, items in sorted(by_source.items(), key=lambda p: str(p[0]))}
        for fut in concurrent.futures.as_completed(futures):
            src, items = futures[fut]
            try:
                results = fut.result()
            except Exception as exc:  # one bad TU must not sink the run
                results = [(i, rxs._record_error(i, "TU worker failed: %s" % exc)) for i in items]
            for item, record in results:
                record["function"] = item["function"]
                record["dormant"] = {"key": item["key"], "symbol": item["symbol"]}
                rxs._write_record(record, OUT / ("%08x-%s.json" % (item["address"],
                                                                   item["function"])))
            done += 1
            print("[%d/%d] %s (%d functions)" % (done, len(futures),
                                                 src.relative_to(ROOT), len(results)), flush=True)


# --------------------------------------------------------------------------
# Classification and report
# --------------------------------------------------------------------------

def _literal_status(rec, aligned):
    literal = rec.get("literal_byte_match")
    if literal == "exact":
        return "exact"
    if rec.get("verdict") == "structural exact":
        return "exact_mod_relocations"
    if literal == "different" or (aligned.get("byte_accuracy") or 0) < EXACT:
        return "different"
    return "unverified"


def _codegen_kind(classes, signals):
    """Name the dominant codegen difference family for triage."""
    kinds = []
    if signals and signals.get("frame_differs"):
        kinds.append("frame")
    if classes.get("register", 0) + classes.get("candidate_only:register_save", 0) + \
            classes.get("reference_only:register_save", 0):
        kinds.append("register_alloc")
    if classes.get("stack_offset"):
        kinds.append("stack_layout")
    if signals and signals.get("jcc_delta"):
        kinds.append("branch_shape")
    if classes.get("candidate_only:instruction") or classes.get("reference_only:instruction"):
        kinds.append("instruction_count")
    if classes.get("immediate") or classes.get("operands") or classes.get("displacement"):
        kinds.append("operands")
    if classes.get("operand_order"):
        kinds.append("operand_order")
    return kinds or ["other"]


def behaviour_evidence(signals, reasons, register_argument=False, source=None):
    """Turn raw signals into bug evidence, demoting known non-behavioural shapes.

    Demoted to reasons (not evidence): call-count noise from CRT helpers
    (_chkstk/_ftol2), a direct call written as a call through the callee's
    absolute address, tail-call vs call, lift callees whose symbol cannot be
    mapped to an address (paired by count with original-only callees), global
    access-count differences (a reload is codegen), and string-literal text
    (assert/__FILE__ text is cosmetic).  What remains is a callee, a global or
    a numeric constant that one side uses and the other never does, or a
    different `ret N` (callee-popped argument bytes).
    """
    counter = collections.Counter
    cand = [c[5:] if c.startswith("tail ") else c for c in signals["candidate"]["calls"]]
    ref = [c[5:] if c.startswith("tail ") else c for c in signals["reference"]["calls"]]
    if signals["candidate"]["calls"] != cand or signals["reference"]["calls"] != ref:
        if counter(cand) == counter(ref) and \
                counter(signals["candidate"]["calls"]) != counter(signals["reference"]["calls"]):
            reasons.append("tail-call vs call shape differs")
    cand_data = list(signals["candidate"]["data_refs"])
    # `((fn)0xADDR)()` compiles to an indirect call through a code constant.
    for name in sorted(set(ref) - set(cand)):
        while ("&" + name) in cand_data and "indirect" in cand and \
                counter(ref)[name] > counter(cand)[name]:
            cand_data.remove("&" + name)
            cand.remove("indirect")
            cand.append(name)
            if "calls through an absolute function address" not in reasons:
                reasons.append("calls through an absolute function address")
    missing = counter(ref) - counter(cand)
    extra = counter(cand) - counter(ref)
    for helper in CRT_HELPERS:
        if missing.pop(helper, 0) or extra.pop(helper, 0):
            reasons.append("CRT helper call count differs (%s)" % helper)
    unresolved = [c for c in extra.elements() if c.startswith("?")]
    if unresolved:
        reasons.append("lift callees with no raw address: %s" % sorted(set(unresolved)))
        for name in unresolved:
            del extra[name]
        paired = min(len(unresolved), sum(missing.values()))
        for name in sorted(missing.elements())[:paired]:
            missing[name] -= 1
        missing = +missing
    evidence = []
    strong = False
    # The original's TUs never inlined across files; ours co-locate some
    # callees in the caller's TU, where VC71 /O2 inlines them.  The call then
    # disappears and its body's calls appear instead.
    inlined = sorted(name for name in set(missing) - set(cand)
                     if source and defined_in(name, source))
    if inlined:
        reasons.append("VC71 inlined same-TU callee(s) %s that the original calls" % inlined)
        for name in inlined:
            del missing[name]
        if not missing and extra:
            reasons.append("lift-only calls attributed to the inlined bodies: %s" %
                           sorted(extra.elements()))
            extra = collections.Counter()
    if missing or extra:
        # A callee only one side ever calls is strong; the same callees at
        # different counts can also be VC71 cross-jumping identical calls.
        if (set(missing) - set(cand)) or (set(extra) - set(ref)):
            strong = True
            evidence.append("calls: original-only %s, lift-only %s" %
                            (sorted(missing.elements()), sorted(extra.elements())))
        else:
            evidence.append("call counts: original has extra %s, lift has extra %s" %
                            (sorted(missing.elements()), sorted(extra.elements())))
    ref_data = set(signals["reference"]["data_refs"])
    cand_data = set(cand_data)

    def near(value, others):
        # The compiler folds a constant offset into a table base either way
        # (0x2eebac[i+1] vs 0x2eebb0[i]), so a nearby address in the same
        # function is the same object, reported as a reason only.
        if not value.startswith("0x"):
            return False
        v = int(value, 16)
        return any(o.startswith("0x") and abs(int(o, 16) - v) <= 0x40 for o in others)

    ref_only = sorted(v for v in ref_data - cand_data if not near(v, cand_data))
    cand_only = sorted(v for v in cand_data - ref_data if not near(v, ref_data))
    if ref_only or cand_only:
        strong = True
        evidence.append("globals: original-only %s, lift-only %s" % (ref_only, cand_only))
    if (ref_data - cand_data or cand_data - ref_data) and not (ref_only or cand_only):
        reasons.append("same global objects at folded offsets: original %s, lift %s" %
                       (sorted(ref_data - cand_data), sorted(cand_data - ref_data)))
    elif counter(signals["reference"]["data_refs"]) != counter(signals["candidate"]["data_refs"]):
        reasons.append("global access counts differ")
    if signals.get("ret_differs"):
        text = "ret forms differ: lift %s, original %s" % (signals["candidate"]["rets"],
                                                          signals["reference"]["rets"])
        if register_argument:
            reasons.append(text + " (register-argument ABI: VC71 compiles it __fastcall)")
        else:
            strong = True
            evidence.append(text)
    literals = signals.get("literals_missing") or []
    numbers = [x for x in literals if not x.startswith(("'", '"'))]
    strings = [x for x in literals if x.startswith(("'", '"'))]
    if numbers:
        strong = True
        evidence.append("numeric constants absent from the original: %s" % numbers[:6])
    if strings:
        reasons.append("string text differs: %s" % [s[:60] for s in strings[:4]])
    if signals.get("literals_value_equal"):
        reasons.append("same constant value in another encoding (float/double/negated/FLD1): %s"
                       % signals["literals_value_equal"][:4])
    return evidence, strong


def summarize(row):
    out = {"object": row["object"], "addr": row["addr"], "name": row["name"],
           "source": row.get("source") and "%s:%s" % (row["source"], row["line"]),
           "bucket": None, "verdict": "unmeasured", "accuracy": None, "upper": None,
           "second_accuracy": None, "provisional": None, "literal": None,
           "raw_operand_accuracy": None, "bytes": None, "reasons": []}
    definition = row["definition"]
    if definition.get("stale_source_name"):
        out["reasons"].append("source defines %s, kb names it %s: rename before activation "
                              "(patch.py requires the kb name in EXE exports)"
                              % (definition["symbol"], row["name"]))
    if definition.get("duplicate_definitions"):
        out["reasons"].append("duplicate definitions: " +
                              "; ".join(definition["duplicate_definitions"]))
    if row["object"] == XDK_OBJECT:
        out.update(bucket="not_measurable", verdict="xdk import")
        out["reasons"].insert(0, "XDK kernel/xapi import; source holds only its prototype")
        return out
    if definition["status"] != "implemented":
        out.update(bucket="not_measurable", verdict="not implemented")
        out["reasons"].insert(0, definition["reason"] +
                              (" (mention at %s)" % row["mention"] if row.get("mention") else ""))
        return out
    if definition.get("naked"):
        out["reasons"].append("__declspec(naked)")
    path = record_path(row)
    if not path.is_file():
        out["bucket"] = "unmeasured"
        return out
    rec = json.loads(path.read_text(encoding="utf-8"))
    out["verdict"] = rec.get("verdict", "unknown")
    aligned = rec.get("aligned_byte_match") or {}
    if aligned.get("status") != "scored":
        out["bucket"] = "not_measurable"
        out["reasons"].insert(0, rec.get("reason") or aligned.get("reason") or "not scored")
        return out
    second = rec.get("second_alignment") or {}
    signals = rec.get("signals") or {}
    lower = aligned.get("byte_accuracy")
    upper = aligned.get("byte_accuracy_upper_bound")
    alt = second.get("byte_accuracy")
    out.update(accuracy=lower, upper=upper, second_accuracy=alt,
               raw_operand_accuracy=aligned.get("raw_operand_accuracy"),
               bytes=aligned.get("compared_bytes"),
               literal=_literal_status(rec, aligned),
               register_argument=bool(rec.get("register_argument")),
               difference_classes=aligned.get("difference_classes"))
    unpaired = aligned.get("unpaired_relocations") or 0
    uncertain = aligned.get("uncertain_relocations") or 0
    # Provisional means the number itself is uncertain: a relocation target is
    # unresolved (the score is a range), or the two tied optimal alignments
    # disagree by half a point or more.  Relocations on unpaired instructions
    # are not uncertainty: those instructions already count as mismatches.
    spread = abs((alt if alt is not None else lower) - lower)
    out["alignment_spread"] = spread
    out["provisional"] = bool(uncertain or alt is None or spread >= 0.005)
    if unpaired:
        out["reasons"].append("%d relocation(s) on unpaired instructions" % unpaired)
    if uncertain:
        out["reasons"].append("%d relocation target(s) unresolved: accuracy is a range" % uncertain)
    if spread >= 0.005:
        out["reasons"].append("tied alignments disagree (%.2f%% vs %.2f%%)" %
                              (100 * lower, 100 * alt))
    out["best_accuracy"] = max(lower, alt or 0)

    evidence, strong = [], False
    if signals:
        evidence, strong = behaviour_evidence(signals, out["reasons"],
                                              bool(rec.get("register_argument")),
                                              row.get("source"))
    out["severity"] = ("strong" if strong else "weak") if evidence else None
    if signals.get("reference_truncated"):
        out["bug_evidence"] = evidence
        out["bucket"] = "tooling_artifact"
        out["reasons"].insert(0, "function_bounds.json cuts the original short: switch cases "
                                 "at %s lie past the bound (needs a table_data override)"
                              % signals["reference_truncated"][:4])
        return out
    if aligned.get("mismatched_relocations"):
        # Usually a switch-table label resolved through a layout that already
        # differs, so it is a reason, not evidence; real wrong targets show up
        # in the call/global/literal signals above.
        out["reasons"].append("%d relocation(s) resolve to a different target" %
                              aligned["mismatched_relocations"])
    if signals.get("call_order_differs"):
        out["reasons"].append("same calls in a different order")
    out["bug_evidence"] = evidence
    if lower >= EXACT and not uncertain and not evidence:
        out["bucket"] = "exact"
    elif (upper or 0) >= EXACT and not evidence:
        out["bucket"] = "exact_pending_identity"
    elif evidence:
        out["bucket"] = "suspect_bug"
    else:
        out["bucket"] = "codegen"
    if out["bucket"] in ("codegen", "suspect_bug"):
        out["codegen_kind"] = _codegen_kind(aligned.get("difference_classes") or {}, signals)
    return out


BUCKET_ORDER = {"exact": 0, "exact_pending_identity": 1, "suspect_bug": 2, "codegen": 3,
                "tooling_artifact": 4, "not_measurable": 5, "unmeasured": 6}


def write_worklist(summary):
    """Prioritized fix list: suspected lift bugs first, then near-exact codegen."""
    bugs = [s for s in summary if s["bucket"] == "suspect_bug"]
    bugs.sort(key=lambda s: (s.get("severity") != "strong", -len(s["bug_evidence"]),
                             -(s["best_accuracy"] or 0)))
    codegen = [s for s in summary if s["bucket"] == "codegen"]
    codegen.sort(key=lambda s: -(s["best_accuracy"] or 0))
    pending = [s for s in summary if s["bucket"] == "exact_pending_identity"]
    blocked = [s for s in summary if s["bucket"] in ("not_measurable", "tooling_artifact")]
    exact = [s for s in summary if s["bucket"] == "exact"]
    stale = [s for s in summary if any("rename before activation" in r for r in s["reasons"])]
    lines = ["# Dormant lift worklist", "",
             "Generated by tools/verify/measure.py. Accuracy is aligned raw-XBE bytes "
             "(lower bound; `alt` is the reversed-tie alignment).", ""]

    def row(s, extra=""):
        acc = "-" if s["accuracy"] is None else "%.1f%%" % (100 * s["accuracy"])
        alt = "-" if s.get("second_accuracy") is None else "%.1f%%" % (100 * s["second_accuracy"])
        return "| %s | %s | %s | %s | %s | %s |" % (s["addr"], s["name"], acc, alt,
                                                    s["source"] or "-", extra)

    header = ["| addr | name | acc | alt | source | evidence |", "|---|---|---|---|---|---|"]
    lines += ["## 1. Suspected lift bugs (%d)" % len(bugs), "",
              "Call targets, global operands or `ret N` differ from the original. "
              "Verify against disassembly before any score work.", ""] + header
    lines += [row(s, "**%s**: " % s.get("severity") + "<br>".join(s["bug_evidence"]))
              for s in bugs]
    lines += ["", "## 2. Activation blockers outside the bytes (%d)" % len(stale), ""] + header
    lines += [row(s, "; ".join(s["reasons"])) for s in stale]
    lines += ["", "## 3. Exact once relocation identity is proven (%d)" % len(pending), ""] + header
    lines += [row(s, "; ".join(s["reasons"])) for s in pending]
    lines += ["", "## 4. Codegen-only differences, best first (%d)" % len(codegen), ""] + header
    lines += [row(s, ", ".join(s.get("codegen_kind") or []) +
                  ("; " + "; ".join(s["reasons"]) if s["reasons"] else "")) for s in codegen]
    lines += ["", "## 5. Not measurable / tooling artifact (%d)" % len(blocked), ""] + header
    lines += [row(s, s["bucket"] + ": " + "; ".join(s["reasons"])) for s in blocked]
    lines += ["", "## 6. Exact, eligible to activate (%d)" % len(exact), ""] + header
    lines += [row(s, "literal " + (s["literal"] or "-")) for s in exact]
    (OUT / "worklist.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    (OUT / "worklist.json").write_text(json.dumps(
        {"suspect_bug": bugs, "activation_blockers": stale, "exact_pending_identity": pending,
         "codegen": codegen, "not_measurable": blocked, "exact": exact}, indent=1) + "\n",
        encoding="utf-8")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--object", help="restrict to one object, e.g. director.obj")
    ap.add_argument("--list", action="store_true", help="enumerate only, no measuring")
    ap.add_argument("--report", action="store_true",
                    help="rank from existing records, no measuring")
    ap.add_argument("--force", action="store_true", help="re-measure unchanged functions too")
    ap.add_argument("--limit", type=int, default=0, help="measure at most N functions")
    ap.add_argument("--workers", type=int, default=6, help="parallel TUs (default 6)")
    args = ap.parse_args()

    rows = enumerate_dormant(args.object)
    if args.list:
        for r in rows:
            d = r["definition"]
            where = ("%s:%s" % (r["source"], r["line"]) if d["status"] == "implemented"
                     else "%s: %s" % (d["status"], d["reason"]))
            print("%s\t%s\t%s\t%s" % (r["object"], r["addr"], r["name"], where))
        print("%d dormant lifts" % len(rows), file=sys.stderr)
        return 0

    OUT.mkdir(parents=True, exist_ok=True)
    if not args.report:
        todo = rows[:args.limit] if args.limit else rows
        measure_all(todo, max(1, args.workers), args.force)

    summary = [summarize(r) for r in rows]
    summary.sort(key=lambda s: (BUCKET_ORDER.get(s["bucket"], 9),
                                -(s["accuracy"] if s["accuracy"] is not None else -1),
                                s["object"], s["addr"]))
    if not args.object:
        (OUT / "report.json").write_text(json.dumps(summary, indent=1) + "\n", encoding="utf-8")
        write_worklist(summary)
    print("\n%-22s %-7s %-7s %-7s %-4s %-21s %-9s %-40s %s" % (
        "bucket", "acc", "upper", "alt", "prov", "literal", "addr", "name", "object"))
    for s in summary:
        def pct(v):
            return "%.1f%%" % (100 * v) if v is not None else "-"
        print("%-22s %-7s %-7s %-7s %-4s %-21s %-9s %-40s %s" % (
            s["bucket"], pct(s["accuracy"]), pct(s["upper"]), pct(s["second_accuracy"]),
            {True: "yes", False: "no", None: "-"}[s["provisional"]], s["literal"] or "-",
            s["addr"], s["name"][:40], s["object"]))
    counts = collections.Counter(s["bucket"] for s in summary)
    print("\n" + ", ".join("%s %d" % (b, counts[b]) for b in sorted(counts, key=BUCKET_ORDER.get)),
          "| provisional %d" % sum(1 for s in summary if s["provisional"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
