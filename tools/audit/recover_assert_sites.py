#!/usr/bin/env python3
"""
recover_assert_sites.py -- rewrite implicit asserts into byte-faithful asserts.

`assert_halt(cond)` and `assert_halt_msg(cond, msg)` stamp OUR `__FILE__` and
`__LINE__`, so every such site pushes the wrong line immediate and references a
wrong path string (src/common.h explains the faithful `_at` forms).  The
original binary states both values outright at each call:

    push 1 ; push <line> ; push <file string> ; push <assert text> ; call display_assert

For every kb.json function with exactly one C definition, this tool decodes
those call sites from the pristine XBE, matches each source assert to a site by
its assert text, and rewrites the call:

    assert_halt(cond)          ->  assert_halt_at(FILE, LINE, cond)
    assert_halt_msg(cond, msg) ->  assert_halt_msg_at(msg, FILE, LINE, cond)

When the original text differs from ours only in spacing (clang-format spacing
inside a macro argument), the original literal is kept through
`assert_halt_msg_at`.  A source assert with no text match is left alone and
reported: pairing by position alone would be a guess.

`--positional` pairs the remaining sites of one function by order, using the
original `__LINE__` immediates as the original source order.  It only acts
when every display_assert call in the function decoded, the leftover source
and binary counts are equal, and the complete pairing (text matches included)
has binary lines rising in our source order.  Those sites always take the
original literal through `assert_halt_msg_at`.

Replacements never add or remove lines, so `__LINE__` at the remaining
implicit sites does not move.

Usage:
  python3 tools/audit/recover_assert_sites.py              # report only
  python3 tools/audit/recover_assert_sites.py --apply      # rewrite sources
  python3 tools/audit/recover_assert_sites.py --verbose    # list unmatched sites
  python3 tools/audit/recover_assert_sites.py --path src/halo/game/game_time.c
  python3 tools/audit/recover_assert_sites.py --check      # exit 1 if any site is rewritable
  python3 tools/audit/recover_assert_sites.py --positional --apply --path <file.c>
"""

import argparse
import ast
import json
import re
import sys
from pathlib import Path
from typing import Dict, List, NamedTuple, Optional, Tuple

HERE = Path(__file__).resolve().parent
REPO_ROOT = HERE.parents[1]
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(REPO_ROOT / "tools" / "verify"))
sys.path.insert(0, str(REPO_ROOT / "tools" / "equivalence"))

import check_assert_targets as cat  # noqa: E402  (comment/string blanking)

DISPLAY_ASSERT_VA = 0x8D9F0   # void display_assert(const char *, const char *, int, bool)
IMPLICIT_RE = re.compile(r"\b(assert_halt|assert_halt_msg)\s*\(")
_FUNC_NAME_RE = re.compile(r"(\w+)\s*\(")


class BinarySite(NamedTuple):
    text: str
    file: str
    line: int
    call_va: int


class SourceSite(NamedTuple):
    macro: str        # assert_halt | assert_halt_msg
    start: int        # offset of the macro name
    end: int          # offset just past the closing parenthesis
    cond: str         # verbatim condition text
    msg: Optional[str]  # verbatim message argument (assert_halt_msg only)
    original: str = ""  # the whole invocation, as written
    indent: str = ""    # continuation indent for re-broken lines
    eol: str = "\n"     # the file's line ending


class Edit(NamedTuple):
    start: int
    end: int
    text: str
    kind: str         # exact | spacing | positional


def stringize(text: str) -> str:
    """What `#arg` produces for a comment-free macro argument."""
    return re.sub(r"\s+", " ", text).strip()


def c_string_literal(value: str) -> str:
    """Encode `value` as a C string literal that reproduces its bytes."""
    out = []
    for ch in value:
        if ch == "\\":
            out.append("\\\\")
        elif ch == '"':
            out.append('\\"')
        elif ch == "\n":
            out.append("\\n")
        elif ch == "\t":
            out.append("\\t")
        elif 0x20 <= ord(ch) < 0x7F:
            out.append(ch)
        else:
            out.append("\\%03o" % (ord(ch) & 0xFF))
    return '"' + "".join(out) + '"'


def decode_c_literal(text: str) -> Optional[str]:
    """Value of a single simple C string literal, or None when it is not one."""
    text = text.strip()
    if not re.fullmatch(r'"(?:[^"\\\n]|\\[\\"nt\'])*"', text):
        return None
    return ast.literal_eval(text)


def _split_top_level(blanked: str, start: int, end: int) -> List[Tuple[int, int]]:
    """Top-level comma-separated argument spans of blanked[start:end]."""
    spans, depth, begin = [], 0, start
    for i in range(start, end):
        c = blanked[i]
        if c in "([{":
            depth += 1
        elif c in ")]}":
            depth -= 1
        elif c == "," and depth == 0:
            spans.append((begin, i))
            begin = i + 1
    spans.append((begin, end))
    return spans


def source_sites(text: str, blanked: str, body_start: int, body_end: int) -> List[SourceSite]:
    """Implicit assert invocations inside one function body, in source order."""
    sites = []
    for m in IMPLICIT_RE.finditer(blanked, body_start, body_end):
        open_paren = m.end() - 1
        depth, j = 0, open_paren
        while j < body_end:
            if blanked[j] == "(":
                depth += 1
            elif blanked[j] == ")":
                depth -= 1
                if depth == 0:
                    break
            j += 1
        if depth != 0:
            continue
        args = _split_top_level(blanked, open_paren + 1, j)
        macro = m.group(1)
        if (macro == "assert_halt" and len(args) != 1) or (
                macro == "assert_halt_msg" and len(args) != 2):
            continue
        cond = text[args[0][0]:args[0][1]].strip()
        msg = text[args[1][0]:args[1][1]].strip() if macro == "assert_halt_msg" else None
        line_start = text.rfind("\n", 0, m.start()) + 1
        indent = re.match(r"[ \t]*", text[line_start:]).group(0) + "    "
        eol = "\r\n" if "\r\n" in text else "\n"
        sites.append(SourceSite(macro, m.start(), j + 1, cond, msg,
                                text[m.start():j + 1], indent, eol))
    return sites


def _keep_line_count(parts: List[str], site: SourceSite) -> str:
    """Join `macro(arg, ...)` parts, breaking lines so the call spans as many
    lines as the original.  A rewrite that joined lines would move `__LINE__`
    at every later implicit assert in the file."""
    head, args = parts[0], parts[1:]
    joined = head + "(" + ", ".join(args) + ")"
    missing = site.original.count("\n") - joined.count("\n")
    if missing <= 0:
        return joined
    breaks = set(range(len(args) - 1, len(args) - 1 - missing, -1))
    extra = max(0, missing - len(args))
    out = head + "("
    for k, arg in enumerate(args):
        if k:
            out += ","
        if k in breaks or (k == len(args) - 1 and extra):
            out += (site.eol + site.indent) * (1 + (extra if k == len(args) - 1 else 0))
        elif k:
            out += " "
        out += arg
    return out + ")"


def _expected_text(site: SourceSite) -> Optional[str]:
    """The assert text our build emits for a site, or None when not derivable."""
    if site.macro == "assert_halt":
        if any(token in site.cond for token in ('"', "'", "/*", "//")):
            return None
        return stringize(site.cond)
    return decode_c_literal(site.msg)


def _rewrite(site: SourceSite, chosen: BinarySite, kind: str) -> Edit:
    file_literal = c_string_literal(chosen.file)
    line = "0x%x" % chosen.line
    if site.macro == "assert_halt" and kind == "exact":
        text = _keep_line_count(["assert_halt_at", file_literal, line, site.cond], site)
    else:
        msg = site.msg if kind == "exact" else c_string_literal(chosen.text)
        text = _keep_line_count(["assert_halt_msg_at", msg, file_literal, line, site.cond],
                                site)
    return Edit(site.start, site.end, text, kind)


def plan_edits(sites: List[SourceSite], binary: List[BinarySite], positional: bool = False,
               undecoded: int = 0) -> Tuple[List[Edit], List[Tuple[SourceSite, str]]]:
    """Match source sites to original sites by assert text and build rewrites.

    With `positional`, pair the sites left over by text matching in original
    line order, under the guards described in the module docstring."""
    unused = sorted(binary, key=lambda b: (b.line, b.call_va))
    edits, unmatched, pairs = [], [], []
    for site in sites:
        expected = _expected_text(site)
        if expected is None:
            unmatched.append((site, "assert text is not derivable from the source"))
            continue
        chosen, kind = None, None
        for candidate in unused:
            if candidate.text == expected:
                chosen, kind = candidate, "exact"
                break
        if chosen is None:
            squeezed = re.sub(r"\s+", "", expected)
            for candidate in unused:
                if re.sub(r"\s+", "", candidate.text) == squeezed:
                    chosen, kind = candidate, "spacing"
                    break
        if chosen is None:
            unmatched.append((site, "no original assert has this text"))
            continue
        unused.remove(chosen)
        pairs.append((site, chosen))
        edits.append(_rewrite(site, chosen, kind))
    if positional and unmatched and undecoded == 0 and len(unmatched) == len(unused):
        extra = list(zip([site for site, _ in unmatched], unused))
        combined = sorted(pairs + extra, key=lambda pair: pair[0].start)
        lines = [chosen.line for _, chosen in combined]
        if all(a < b for a, b in zip(lines, lines[1:])):
            edits.extend(_rewrite(site, chosen, "positional") for site, chosen in extra)
            unmatched = []
    return edits, unmatched


# ---------------------------------------------------------------- binary side

_XBE = None


def _read_va(va: int, size: int) -> bytes:
    global _XBE
    if _XBE is None:
        import xbe_image
        import xbe_reference
        _XBE = (xbe_image, xbe_image.load_xbe(str(xbe_reference.XBE)))
    xbe_image, (raw, secs) = _XBE
    return xbe_image.read_va(raw, secs, va, size)


def _c_string_at(va: int) -> Optional[str]:
    data = _read_va(va, 1024)
    end = data.find(b"\0")
    if end < 0:
        return None
    return data[:end].decode("latin-1")


def binary_sites(address: int) -> Optional[Tuple[List[BinarySite], int]]:
    """Decode fatal display_assert call sites with four immediate arguments.
    Returns (sites, number of display_assert calls that did not decode)."""
    import xbe_reference
    from capstone import CS_ARCH_X86, CS_MODE_32, Cs
    from capstone import x86 as X
    code, _reason = xbe_reference.function_bytes(address)
    if code is None:
        return None
    decoder = Cs(CS_ARCH_X86, CS_MODE_32)
    decoder.detail = True
    insns = list(decoder.disasm(code, address))
    sites, calls = [], 0
    for k, insn in enumerate(insns):
        if insn.mnemonic != "call":
            continue
        ops = insn.operands
        if len(ops) != 1 or ops[0].type != X.X86_OP_IMM or ops[0].imm != DISPLAY_ASSERT_VA:
            continue
        calls += 1
        if k < 4:
            continue
        pushed = []
        for prior in insns[k - 4:k][::-1]:
            p_ops = prior.operands
            if prior.mnemonic != "push" or len(p_ops) != 1 or p_ops[0].type != X.X86_OP_IMM:
                break
            pushed.append(p_ops[0].imm & 0xFFFFFFFF)
        if len(pushed) != 4 or pushed[3] != 1:
            continue
        expr_va, file_va, line, _halt = pushed
        text, path = _c_string_at(expr_va), _c_string_at(file_va)
        if text is None or path is None:
            continue
        sites.append(BinarySite(text, path, line, insn.address))
    return sites, calls - len(sites)


# ---------------------------------------------------------------- driver

def _kb_functions() -> Dict[str, int]:
    """name -> address for kb.json functions whose name is unique."""
    kb = json.loads((REPO_ROOT / "kb.json").read_text(encoding="utf-8"))
    seen: Dict[str, set] = {}
    for obj in kb.get("objects", []):
        for fn in obj.get("functions") or []:
            decl = re.sub(r"@<\w+>", "", fn.get("decl") or "")
            m = _FUNC_NAME_RE.search(decl)
            if m and fn.get("addr"):
                seen.setdefault(m.group(1), set()).add(int(fn["addr"], 16))
    return {name: next(iter(addrs)) for name, addrs in seen.items() if len(addrs) == 1}


def _definition_span(blanked: str, name: str) -> Optional[Tuple[int, int]]:
    """(start, end) of the single column-0 definition body of `name`."""
    spans = []
    for m in re.finditer(r"(?m)^[A-Za-z_][^\n=;]*\b%s\s*\(" % re.escape(name), blanked):
        brace = blanked.find("{", m.end())
        semi = blanked.find(";", m.end())
        if brace < 0 or (0 <= semi < brace):
            continue
        depth, j = 0, brace
        while j < len(blanked):
            if blanked[j] == "{":
                depth += 1
            elif blanked[j] == "}":
                depth -= 1
                if depth == 0:
                    spans.append((brace, j + 1))
                    break
            j += 1
    return spans[0] if len(spans) == 1 else None


def run(paths: Optional[List[Path]], apply: bool, verbose: bool,
        positional: bool = False) -> dict:
    functions = _kb_functions()
    files = paths or sorted((REPO_ROOT / "src").rglob("*.c"))
    stats = {"files_scanned": 0, "implicit_sites": 0, "rewritten_exact": 0,
             "rewritten_spacing": 0, "rewritten_positional": 0, "unmatched": 0, "outside_known_function": 0,
             "files_changed": 0, "unmatched_detail": []}
    for path in files:
        # newline="" keeps CRLF files CRLF; offsets then include the \r bytes.
        with open(path, encoding="utf-8", errors="surrogateescape", newline="") as handle:
            text = handle.read()
        if not IMPLICIT_RE.search(text):
            continue
        stats["files_scanned"] += 1
        blanked = cat._blank_comments_and_strings(text)
        total_here = len(list(IMPLICIT_RE.finditer(blanked)))
        stats["implicit_sites"] += total_here
        covered = 0
        edits: List[Edit] = []
        names = set(re.findall(r"(?m)^[A-Za-z_][^\n=;{}]*\b([A-Za-z_]\w*)\s*\(", blanked))
        for name in sorted(names):
            address = functions.get(name)
            if address is None:
                continue
            span = _definition_span(blanked, name)
            if span is None:
                continue
            sites = source_sites(text, blanked, span[0], span[1])
            if not sites:
                continue
            covered += len(sites)
            decoded = binary_sites(address)
            if decoded is None:
                for site in sites:
                    stats["unmatched"] += 1
                    stats["unmatched_detail"].append(
                        (str(path.relative_to(REPO_ROOT)), name, stringize(site.cond),
                         "original has no bounds"))
                continue
            original, undecoded = decoded
            planned, unmatched = plan_edits(sites, original, positional, undecoded)
            edits.extend(planned)
            for site, reason in unmatched:
                stats["unmatched"] += 1
                stats["unmatched_detail"].append(
                    (str(path.relative_to(REPO_ROOT)), name, stringize(site.cond), reason))
        stats["outside_known_function"] += total_here - covered
        for edit in edits:
            stats["rewritten_" + edit.kind] += 1
        if edits and apply:
            for edit in sorted(edits, key=lambda e: e.start, reverse=True):
                text = text[:edit.start] + edit.text + text[edit.end:]
            with open(path, "w", encoding="utf-8", errors="surrogateescape", newline="") as handle:
                handle.write(text)
        if edits:
            stats["files_changed"] += 1
    return stats


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--apply", action="store_true", help="rewrite the source files")
    ap.add_argument("--check", action="store_true", help="exit 1 if any site is rewritable")
    ap.add_argument("--verbose", action="store_true", help="list unmatched sites")
    ap.add_argument("--path", action="append", type=Path, help="limit to these .c files")
    ap.add_argument("--positional", action="store_true",
                    help="pair leftover sites by original line order (guarded)")
    args = ap.parse_args()
    paths = [p if p.is_absolute() else REPO_ROOT / p for p in args.path] if args.path else None
    stats = run(paths, args.apply, args.verbose, args.positional)
    detail = stats.pop("unmatched_detail")
    for key, value in stats.items():
        print("%-24s %d" % (key, value))
    if args.verbose:
        for path, name, cond, reason in detail:
            print("  UNMATCHED %s %s: %s (%s)" % (path, name, cond[:80], reason))
    rewritable = (stats["rewritten_exact"] + stats["rewritten_spacing"]
                  + stats["rewritten_positional"])
    return 1 if args.check and rewritable else 0


if __name__ == "__main__":
    sys.exit(main())
