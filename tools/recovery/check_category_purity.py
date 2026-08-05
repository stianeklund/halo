#!/usr/bin/env python3
"""Category-purity checker for source-recovery commits.

The source-recovery ladder mandates ONE COMMIT PER CATEGORY (SKILL.md
"Separation rule"): a reviewer must be able to say "commit 3 is renames-only"
and skim it.  Until now that was convention only.  This tool makes it a
detector, in the spirit of the repo's learnings-must-ship-a-detector rule.

    rtk python3 tools/recovery/check_category_purity.py local-renames --staged

Exit codes
    0  pure          -- the diff matches the declared category's edit shape
    1  violation     -- something in the diff is not an allowed edit for it
    2  not checkable -- the category's shape is not mechanically decidable
                        (`expr-simplify`, `control-flow`); callers treat as pass

How it works
    A small dependency-free C tokenizer (identifiers, pp-numbers, strings,
    chars, comments, punctuation) turns old and new file content into token
    streams, and the streams are compared per changed file.  There is
    deliberately NO AST library: the sources carry MSVC/clang extensions
    (`__declspec`, `__try`, `__asm`, `__int64`) that trip C parsers, while
    lexical comparison is robust and sufficient for "did anything other than
    <allowed shape> change".

    The checker FAILS CLOSED.  Any token change it cannot positively classify
    as an allowed edit for the declared category is a violation with "split the
    commit" guidance.  False rejections are acceptable (split the commit, or
    declare the later ladder category); false passes are not.

Scope limits (documented, not bugs)
    * Only `.c`/`.h` (and `.cpp`/`.cc`/`.hpp`) files are inspected.  Manifests
      under `recovery/`, docs, and JSON are ignored with a note line -- a
      recovery commit legitimately carries its manifest status flip alongside
      the source edit, and a manifest is not C.
    * Renames are not tracked (`--no-renames`): a moved file reads as
      delete+add and therefore fails, which is the fail-closed answer.
    * Merge commits are not checked (exit 2 with a note).
    * Value fidelity is NOT this tool's job.  `const-enum` purity only proves
      the *shape* (a literal became an identifier); that the identifier equals
      the literal is owned by the vc71 `[IMM-WARN]` lane and the COFF
      neutrality guard.

Why there is no git hook
    Recovery commits are not identifiable at pre-commit time -- nothing in a
    staged diff declares "this is the local-renames commit", and the ladder
    category is a human intent, not a file property.  A repo-wide hook would
    therefore have to guess, and a misfiring guess would block ordinary lift
    work (which mixes renames, constants, and control flow by design, and must
    keep doing so).  The trade-off taken here: the checker is invoked
    explicitly from the source-recovery workflow, once per category commit.
"""

from __future__ import annotations

import argparse
import difflib
import re
from pathlib import Path
import subprocess
import sys
from typing import Iterable, NamedTuple, Sequence


ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

TOOL_VERSION = "category-purity/1"

CATEGORIES = (
    "comments",
    "local-renames",
    "symbol-names",
    "const-enum",
    "struct-define",
    "offset-to-field",
    "expr-simplify",
    "control-flow",
)
# Codegen-moving categories: their allowed edit shape is "whatever the
# behavioural gate accepts", which is not a lexical property.  Mirrors the
# docs/lift-learnings.md "Automation: not mechanically detectable because ..."
# convention rather than pretending to check them.
UNCHECKABLE = {
    "expr-simplify": (
        "an expression rewrite changes tokens by definition; only the VC71 "
        "codegen gate and the behavioural oracle can judge it"
    ),
    "control-flow": (
        "restructuring loops/gotos changes tokens by definition; only the VC71 "
        "codegen gate and the behavioural oracle can judge it"
    ),
}
# Aliased: the two rename categories differ only in the SCOPE of the rename
# (function-local vs. global symbol), which is a review matter, not a lexical
# one.  Same mechanical shape, checked by one implementation.
RENAME_CATEGORIES = {"local-renames", "symbol-names"}

SOURCE_SUFFIXES = {".c", ".h", ".cpp", ".cc", ".hpp", ".hxx"}

RULES = {
    "comments": "token streams (comments stripped) must be IDENTICAL; whitespace/blank lines free",
    "local-renames": "token streams identical except identifier->identifier substitution, consistent and injective per file",
    "symbol-names": "same shape as local-renames (rename scope is a review matter, not a lexical one)",
    "const-enum": "added #define/enum/int-typedef only, plus numeric-literal -> identifier substitutions",
    "struct-define": "additions only at file scope: typedef/struct/union blocks, cs()/co() asserts, #include, include guards",
    "offset-to-field": "raw-deref (cast + '+ literal') -> member access substitutions, plus added typed-pointer locals and #include",
    "expr-simplify": "NOT MECHANICALLY CHECKABLE (codegen/behavioural gates own it)",
    "control-flow": "NOT MECHANICALLY CHECKABLE (codegen/behavioural gates own it)",
}


# --------------------------------------------------------------------------
# Tokenizer
# --------------------------------------------------------------------------

class Token(NamedTuple):
    kind: str  # id | num | str | chr | punct | comment
    text: str
    line: int

    @property
    def key(self) -> tuple[str, str]:
        return (self.kind, self.text)


# Longest-first so '<<=' beats '<<' beats '<'.
_PUNCT = sorted(
    (
        "...", "<<=", ">>=",
        "->", "++", "--", "<<", ">>", "<=", ">=", "==", "!=", "&&", "||",
        "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "##",
        "[", "]", "(", ")", "{", "}", ".", "&", "*", "+", "-", "~", "!",
        "/", "%", "<", ">", "^", "|", "?", ":", ";", "=", ",", "#",
    ),
    key=len,
    reverse=True,
)

_ID_START = set("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_$")
_ID_BODY = _ID_START | set("0123456789")
_DIGITS = set("0123456789")

# C89 keywords plus the MSVC/clang extensions this tree actually uses.  A
# keyword is NEVER treated as a renameable identifier: `int` -> `float` must
# read as a type change (violation), not as a rename.
KEYWORDS = frozenset(
    """
    auto break case char const continue default do double else enum extern
    float for goto if int long register return short signed sizeof static
    struct switch typedef union unsigned void volatile while
    inline restrict _Bool _Complex _Imaginary static_assert _Static_assert
    __asm __cdecl __declspec __fastcall __forceinline __inline __int8 __int16
    __int32 __int64 __stdcall __try __except __finally __leave __naked
    __attribute__ __builtin_offsetof __restrict __volatile__ asm typeof
    """.split()
)

# Type-ish words allowed inside a cast or a typedef of an integer type.
INT_TYPE_WORDS = frozenset(
    """
    char short int long signed unsigned const volatile
    int8_t int16_t int32_t int64_t uint8_t uint16_t uint32_t uint64_t
    uintptr_t intptr_t size_t ptrdiff_t byte word dword bool __int8 __int16
    __int32 __int64 wchar_t
    """.split()
)

# Punctuation permitted inside a #define body / enumerator initialiser.
_CONST_EXPR_PUNCT = frozenset("( ) << >> | & + - * / % ^ ~ , u U l L".split())

_PP_CONDITIONALS = frozenset({"if", "ifdef", "ifndef", "else", "elif", "endif"})


def tokenize(text: str) -> list[Token]:
    """Split C source into tokens.  Never raises; unterminated literals and
    comments are consumed to end-of-input so a malformed file still diffs."""
    out: list[Token] = []
    i = 0
    line = 1
    n = len(text)
    while i < n:
        ch = text[i]
        if ch == "\n":
            line += 1
            i += 1
            continue
        if ch in " \t\r\f\v":
            i += 1
            continue
        if ch == "\\" and i + 1 < n and text[i + 1] == "\n":  # line continuation
            line += 1
            i += 2
            continue
        if text.startswith("//", i):
            j = text.find("\n", i)
            j = n if j < 0 else j
            out.append(Token("comment", text[i:j], line))
            i = j
            continue
        if text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            body = text[i:j]
            out.append(Token("comment", body, line))
            line += body.count("\n")
            i = j
            continue
        if ch in ('"', "'"):
            kind = "str" if ch == '"' else "chr"
            j = i + 1
            while j < n and text[j] != ch:
                if text[j] == "\\":
                    j += 1
                if j < n and text[j] == "\n":
                    line += 1
                j += 1
            j = min(j + 1, n)
            out.append(Token(kind, text[i:j], line))
            i = j
            continue
        if ch in _DIGITS or (ch == "." and i + 1 < n and text[i + 1] in _DIGITS):
            # pp-number: digits, letters, dots, and exponent signs.
            j = i
            while j < n:
                c = text[j]
                if c in _ID_BODY or c == ".":
                    j += 1
                elif c in "+-" and j > i and text[j - 1] in "eEpP":
                    j += 1
                else:
                    break
            out.append(Token("num", text[i:j], line))
            i = j
            continue
        if ch in _ID_START:
            j = i
            while j < n and text[j] in _ID_BODY:
                j += 1
            out.append(Token("id", text[i:j], line))
            i = j
            continue
        for op in _PUNCT:
            if text.startswith(op, i):
                out.append(Token("punct", op, line))
                i += len(op)
                break
        else:
            # Unknown byte (e.g. stray '@' or non-ASCII); keep it so it cannot
            # silently vanish from the comparison.
            out.append(Token("punct", ch, line))
            i += 1
    return out


def strip_comments(tokens: Sequence[Token]) -> list[Token]:
    return [t for t in tokens if t.kind != "comment"]


# --------------------------------------------------------------------------
# Results
# --------------------------------------------------------------------------

class Violation(NamedTuple):
    line: int          # line in the NEW file (0 when only the old side exists)
    message: str

    def format(self) -> str:
        where = f":{self.line}" if self.line else ""
        return f"  line{where}: {self.message}"


class PairResult(NamedTuple):
    status: str                  # pure | violation | unchecked
    violations: list[Violation]
    notes: list[str]

    @property
    def ok(self) -> bool:
        return self.status != "violation"


def _pure(notes: Iterable[str] = ()) -> PairResult:
    return PairResult("pure", [], list(notes))


def _bad(violations: Sequence[Violation], notes: Iterable[str] = ()) -> PairResult:
    return PairResult("violation", list(violations), list(notes))


# --------------------------------------------------------------------------
# Shared helpers
# --------------------------------------------------------------------------

def _line_of(tokens: Sequence[Token], index: int) -> int:
    if not tokens:
        return 0
    if index >= len(tokens):
        return tokens[-1].line
    return tokens[index].line


def _show(tokens: Sequence[Token], limit: int = 8) -> str:
    text = " ".join(t.text for t in tokens[:limit])
    if len(tokens) > limit:
        text += " ..."
    return text or "<nothing>"


def _opcodes(old: Sequence[Token], new: Sequence[Token]):
    matcher = difflib.SequenceMatcher(
        a=[t.key for t in old], b=[t.key for t in new], autojunk=False)
    return matcher.get_opcodes()


def _hunks(old: Sequence[Token], new: Sequence[Token], max_gap: int = 2):
    """Coalesced non-equal regions.

    difflib fragments a single edit whenever a token survives inside it:
    `*(int *)((int)actor + 0x10)` -> `actor->mode` reads as delete + equal
    (`actor`) + replace, and judging those pieces separately is nonsense.  Equal
    runs of at most `max_gap` tokens are absorbed into the surrounding hunk so
    one source edit is judged as one edit.  The gap stays small on purpose: a
    long equal run means two independent edits, which must be judged
    separately (that is how an unrelated rename smuggled into a rewrite is
    still caught)."""
    raw = [(i1, i2, j1, j2)
           for tag, i1, i2, j1, j2 in _opcodes(old, new) if tag != "equal"]
    merged: list[tuple[int, int, int, int]] = []
    for span in raw:
        if merged:
            prev = merged[-1]
            if 0 <= span[0] - prev[1] <= max_gap:
                merged[-1] = (prev[0], span[1], prev[2], span[3])
                continue
        merged.append(span)
    return merged


class Unit(NamedTuple):
    """One top-level construct: a declaration, a definition, a preprocessor
    directive, or a whole function body."""
    tokens: tuple[Token, ...]
    line: int

    @property
    def key(self) -> tuple[tuple[str, str], ...]:
        return tuple(t.key for t in self.tokens)


def _split_top_level(tokens: Sequence[Token]) -> list[Unit]:
    """Cut a comment-stripped token stream into top-level units.

    Diffing at token granularity fragments edits in ways that produce nonsense
    verdicts: inserting one `co(actor_t, field, 0x4);` between two existing
    `co(...)` lines aligns as "field name replaced, offset replaced, tail
    inserted", which reads as a modification of existing code when it is a pure
    addition.  Cutting at depth-0 `;` and at the `}` that closes a definition
    puts the diff on construct boundaries, so an addition looks like an
    addition.  It is also much faster: hundreds of units instead of tens of
    thousands of tokens through SequenceMatcher."""
    units: list[Unit] = []
    current: list[Token] = []
    depth = 0
    paren = 0
    index = 0
    total = len(tokens)

    def flush() -> None:
        if current:
            units.append(Unit(tuple(current), current[0].line))
            current.clear()

    while index < total:
        tok = tokens[index]
        if tok.kind == "punct" and tok.text == "#" and depth == 0 and paren == 0 \
                and not current:
            line = tok.line
            directive = [tok]
            index += 1
            while index < total and tokens[index].line == line:
                directive.append(tokens[index])
                index += 1
            units.append(Unit(tuple(directive), line))
            continue
        current.append(tok)
        index += 1
        if tok.kind != "punct":
            continue
        if tok.text in "([":
            paren += 1
        elif tok.text in ")]":
            paren = max(0, paren - 1)
        elif tok.text == "{":
            depth += 1
        elif tok.text == "}":
            depth = max(0, depth - 1)
            if depth == 0 and paren == 0:
                # A type definition (`typedef struct {...} name;`) or an
                # initialiser (`int t[] = {...};`) continues to its `;`; a
                # function body ends at the brace.
                first = current[0]
                if first.kind == "id" and first.text in (
                        "typedef", "struct", "union", "enum"):
                    continue
                # An initialiser (`int t[] = {...};`): the `=` must appear
                # BEFORE the opening brace.  A function body is full of `=`
                # after its brace and must NOT be treated as an initialiser --
                # otherwise the unit runs on and swallows whatever follows.
                head = current[:next(
                    (k for k, t in enumerate(current) if _is_punct(t, "{")),
                    len(current))]
                if any(_is_punct(t, "=") for t in head):
                    continue
                flush()
        elif tok.text == ";" and depth == 0 and paren == 0:
            flush()
    flush()
    return units


def _unit_opcodes(old: Sequence[Unit], new: Sequence[Unit]):
    matcher = difflib.SequenceMatcher(
        a=[u.key for u in old], b=[u.key for u in new], autojunk=False)
    return matcher.get_opcodes()


def _split_statements(tokens: Sequence[Token]) -> list[list[Token]]:
    """Split a token run into `;`-terminated statements and directive lines.

    A run beginning with '#' is cut at the end of its physical line (a
    preprocessor directive ends at newline, and newlines are not tokens)."""
    chunks: list[list[Token]] = []
    current: list[Token] = []
    index = 0
    total = len(tokens)
    while index < total:
        tok = tokens[index]
        if not current and tok.kind == "punct" and tok.text == "#":
            line = tok.line
            directive = [tok]
            index += 1
            while index < total and tokens[index].line == line:
                directive.append(tokens[index])
                index += 1
            chunks.append(directive)
            continue
        current.append(tok)
        index += 1
        if tok.kind == "punct" and tok.text == ";" and _balanced(current):
            chunks.append(current)
            current = []
    if current:
        chunks.append(current)
    return chunks


def _balanced(tokens: Sequence[Token]) -> bool:
    depth = 0
    for tok in tokens:
        if tok.kind == "punct":
            if tok.text in "({[":
                depth += 1
            elif tok.text in ")}]":
                depth -= 1
    return depth == 0


def _flat(units: Sequence[Unit]) -> list[Token]:
    return [tok for unit in units for tok in unit.tokens]


def _first_token_change(old_units: Sequence[Unit], new_units: Sequence[Unit]) -> str:
    old_tokens = _flat(old_units)
    new_tokens = _flat(new_units)
    for i1, i2, j1, j2 in _hunks(old_tokens, new_tokens, max_gap=0):
        return f"{_show(old_tokens[i1:i2])!r} -> {_show(new_tokens[j1:j2])!r}"
    return f"{_show(old_tokens)!r} -> {_show(new_tokens)!r}"


def _align_replace(old_run: Sequence[Unit], new_run: Sequence[Unit]):
    """Split a modified region into (pairs, additions), or None.

    difflib reports "1 construct became 2" as a single replace when a new
    construct is inserted next to a modified one -- adding `#define FOO 3`
    above the function that starts using `FOO` is exactly that shape.  Every
    old unit must still be accounted for by exactly one new unit (returns None
    otherwise, so a deletion or a split can never read as an addition); extra
    new units are additions.  Alignment is a small edit-distance DP over
    token-stream similarity; the regions are a handful of units."""
    old_count, new_count = len(old_run), len(new_run)
    if new_count < old_count:
        return None
    infinity = float("inf")
    cost = [[infinity] * (new_count + 1) for _ in range(old_count + 1)]
    back: list[list[tuple[str, int, int] | None]] = [
        [None] * (new_count + 1) for _ in range(old_count + 1)]
    cost[0][0] = 0.0
    for i in range(old_count + 1):
        for j in range(new_count + 1):
            here = cost[i][j]
            if here == infinity:
                continue
            if j < new_count:  # new_run[j] is a pure addition
                candidate = here + 1.0
                if candidate < cost[i][j + 1]:
                    cost[i][j + 1] = candidate
                    back[i][j + 1] = ("add", i, j)
            if i < old_count and j < new_count:  # pair them up
                ratio = difflib.SequenceMatcher(
                    a=old_run[i].key, b=new_run[j].key, autojunk=False).ratio()
                candidate = here + (1.0 - ratio)
                if candidate < cost[i + 1][j + 1]:
                    cost[i + 1][j + 1] = candidate
                    back[i + 1][j + 1] = ("pair", i, j)
    if cost[old_count][new_count] == infinity:
        return None
    pairs: list[tuple[Unit, Unit]] = []
    additions: list[Unit] = []
    i, j = old_count, new_count
    while (i, j) != (0, 0):
        step = back[i][j]
        if step is None:
            return None
        kind, i, j = step
        if kind == "add":
            additions.append(new_run[j])
        else:
            pairs.append((old_run[i], new_run[j]))
    pairs.reverse()
    additions.reverse()
    return pairs, additions


def _is_id(tok: Token, text: str | None = None) -> bool:
    return tok.kind == "id" and (text is None or tok.text == text)


def _is_punct(tok: Token, text: str) -> bool:
    return tok.kind == "punct" and tok.text == text


def _directive_name(chunk: Sequence[Token]) -> str | None:
    if len(chunk) >= 2 and _is_punct(chunk[0], "#") and chunk[1].kind == "id":
        return chunk[1].text
    return None


# --------------------------------------------------------------------------
# Category: comments
# --------------------------------------------------------------------------

def _check_comments(old: Sequence[Token], new: Sequence[Token]) -> PairResult:
    old_code = strip_comments(old)
    new_code = strip_comments(new)
    if [t.key for t in old_code] == [t.key for t in new_code]:
        return _pure([f"{len(new_code)} code tokens identical; only "
                      f"comments/whitespace moved"])
    old_units = _split_top_level(old_code)
    new_units = _split_top_level(new_code)
    violations: list[Violation] = []
    for tag, i1, i2, j1, j2 in _unit_opcodes(old_units, new_units):
        if tag == "equal":
            continue
        old_run = old_units[i1:i2]
        new_run = new_units[j1:j2]
        line = new_run[0].line if new_run else (old_run[0].line if old_run else 0)
        detail = _first_token_change(old_run, new_run)
        violations.append(Violation(
            line, f"code token change in a 'comments' commit: {detail}"))
    if violations:
        return _bad(violations)
    # Unit keys matched but the flat streams did not: fail closed rather than
    # claim purity on a difference we could not localise.
    return _bad([Violation(0, "token streams differ but no unit-level change "
                              "was localised; treat as impure and split")])


# --------------------------------------------------------------------------
# Category: local-renames / symbol-names
# --------------------------------------------------------------------------

def _check_renames(old: Sequence[Token], new: Sequence[Token]) -> PairResult:
    old_code = strip_comments(old)
    new_code = strip_comments(new)
    if len(old_code) != len(new_code):
        return _bad([Violation(
            _line_of(new_code, 0),
            f"token count changed {len(old_code)} -> {len(new_code)}; a rename "
            f"cannot add or remove tokens")])

    violations: list[Violation] = []
    forward: dict[str, str] = {}
    backward: dict[str, str] = {}
    for old_tok, new_tok in zip(old_code, new_code):
        if old_tok.key == new_tok.key:
            continue
        if old_tok.kind != "id" or new_tok.kind != "id":
            violations.append(Violation(
                new_tok.line,
                f"non-identifier token changed: {old_tok.text!r} ({old_tok.kind})"
                f" -> {new_tok.text!r} ({new_tok.kind})"))
            continue
        if old_tok.text in KEYWORDS or new_tok.text in KEYWORDS:
            violations.append(Violation(
                new_tok.line,
                f"keyword/type token changed: {old_tok.text!r} -> {new_tok.text!r}"
                f" (that is a type or storage-class edit, not a rename)"))
            continue
        prior = forward.get(old_tok.text)
        if prior is not None and prior != new_tok.text:
            violations.append(Violation(
                new_tok.line,
                f"inconsistent rename: {old_tok.text!r} maps to both {prior!r}"
                f" and {new_tok.text!r}"))
            continue
        source = backward.get(new_tok.text)
        if source is not None and source != old_tok.text:
            violations.append(Violation(
                new_tok.line,
                f"non-injective rename: {source!r} and {old_tok.text!r} both map"
                f" to {new_tok.text!r} (distinct names collapsed)"))
            continue
        forward[old_tok.text] = new_tok.text
        backward[new_tok.text] = old_tok.text

    if violations:
        return _bad(violations)
    if not forward:
        return _pure(["no identifier changed (comments/whitespace only)"])
    pairs = ", ".join(f"{a}->{b}" for a, b in sorted(forward.items())[:6])
    more = "" if len(forward) <= 6 else f" (+{len(forward) - 6} more)"
    return _pure([f"{len(forward)} consistent injective rename(s): {pairs}{more}"])


# --------------------------------------------------------------------------
# Category: const-enum
# --------------------------------------------------------------------------

def _const_expr_ok(tokens: Sequence[Token]) -> bool:
    for tok in tokens:
        if tok.kind == "num":
            continue
        if tok.kind == "id" and tok.text not in KEYWORDS:
            continue  # reference to another already-defined constant
        if tok.kind == "punct" and tok.text in _CONST_EXPR_PUNCT:
            continue
        if tok.kind == "chr":
            continue  # #define FOO 'x'
        return False
    return True


def _validate_const_enum_insert(chunk: Sequence[Token]) -> str | None:
    """Return an error message, or None when the inserted chunk is allowed."""
    name = _directive_name(chunk)
    if name is not None:
        if name != "define":
            return f"added '#{name}' directive is not const/enum work"
        if len(chunk) < 3 or chunk[2].kind != "id":
            return "malformed '#define' (expected '#define NAME <literal-expr>')"
        if not _const_expr_ok(chunk[3:]):
            return (f"'#define {chunk[2].text}' body is not a literal expression: "
                    f"{_show(chunk[3:])!r}")
        return None
    if not chunk:
        return None
    first = chunk[0]
    if _is_id(first, "enum"):
        return None if _balanced(chunk) else "unterminated enum block"
    if _is_id(first, "typedef"):
        if not _balanced(chunk):
            return "unterminated typedef"
        if any(_is_id(tok, "enum") for tok in chunk):
            return None
        body = [tok for tok in chunk[1:] if tok.kind == "id"]
        if len(body) >= 2 and all(tok.text in INT_TYPE_WORDS for tok in body[:-1]):
            return None
        return (f"typedef is not of an integer type or enum: {_show(chunk)!r} "
                f"(that is struct-define work)")
    return f"added code is not a #define/enum/int-typedef: {_show(chunk)!r}"


def _check_const_enum(old: Sequence[Token], new: Sequence[Token]) -> PairResult:
    old_units = _split_top_level(strip_comments(old))
    new_units = _split_top_level(strip_comments(new))
    violations: list[Violation] = []
    substitutions = 0
    additions = 0
    for tag, i1, i2, j1, j2 in _unit_opcodes(old_units, new_units):
        if tag == "equal":
            continue
        old_run = old_units[i1:i2]
        new_run = new_units[j1:j2]
        if tag == "delete":
            violations.append(Violation(
                old_run[0].line,
                f"code removed in a 'const-enum' commit: "
                f"{_show(_flat(old_run))!r}"))
            continue
        if tag == "insert":
            for unit in new_run:
                error = _validate_const_enum_insert(unit.tokens)
                if error:
                    violations.append(Violation(unit.line, error))
                else:
                    additions += 1
            continue
        aligned = _align_replace(old_run, new_run)
        if aligned is None:
            violations.append(Violation(
                new_run[0].line,
                f"{len(old_run)} construct(s) became {len(new_run)}; a "
                f"const/enum commit only renames literals in place: "
                f"{_first_token_change(old_run, new_run)}"))
            continue
        pairs, extra = aligned
        for unit in extra:
            error = _validate_const_enum_insert(unit.tokens)
            if error:
                violations.append(Violation(unit.line, error))
            else:
                additions += 1
        for old_unit, new_unit in pairs:
            for i, j, k, m in _hunks(old_unit.tokens, new_unit.tokens, max_gap=0):
                old_hunk = old_unit.tokens[i:j]
                new_hunk = new_unit.tokens[k:m]
                if len(old_hunk) != len(new_hunk):
                    violations.append(Violation(
                        new_unit.tokens[k].line if k < len(new_unit.tokens)
                        else new_unit.line,
                        f"replacement is not 1-for-1: {_show(old_hunk)!r} -> "
                        f"{_show(new_hunk)!r}; only 'literal -> NAME' is allowed"))
                    continue
                for old_tok, new_tok in zip(old_hunk, new_hunk):
                    if old_tok.kind == "num" and new_tok.kind == "id" \
                            and new_tok.text not in KEYWORDS:
                        substitutions += 1
                        continue
                    violations.append(Violation(
                        new_tok.line,
                        f"not a 'numeric literal -> constant name' substitution: "
                        f"{old_tok.text!r} ({old_tok.kind}) -> {new_tok.text!r} "
                        f"({new_tok.kind})"))
    if violations:
        return _bad(violations)
    return _pure([f"{substitutions} literal->name substitution(s), "
                  f"{additions} added definition(s)"])


# --------------------------------------------------------------------------
# Category: struct-define
# --------------------------------------------------------------------------

def _validate_struct_define_insert(chunk: Sequence[Token]) -> str | None:
    name = _directive_name(chunk)
    if name is not None:
        if name == "include":
            return None
        if name in _PP_CONDITIONALS:
            return None  # include guard / build-flavour scaffolding
        if name == "define":
            # Only an empty-body define (an include guard) is neutral here; a
            # valued define is const-enum work.
            if len(chunk) == 3 and chunk[2].kind == "id":
                return None
            return ("added '#define' with a value belongs in the 'const-enum' "
                    "commit, not 'struct-define'")
        if name == "undef":
            return "added '#undef' is not a struct definition"
        return f"added '#{name}' directive is not a struct definition"
    if not chunk:
        return None
    if not _balanced(chunk):
        return f"unterminated definition: {_show(chunk)!r}"
    first = chunk[0]
    if _is_id(first, "typedef") or _is_id(first, "struct") or _is_id(first, "union") \
            or _is_id(first, "enum"):
        return None
    if first.kind == "id" and first.text in ("cs", "co") and len(chunk) > 1 \
            and _is_punct(chunk[1], "("):
        return None
    if _is_id(first, "static_assert") or _is_id(first, "_Static_assert"):
        return None
    return (f"added code is not a type definition or cs()/co() assert: "
            f"{_show(chunk)!r}")


def _check_struct_define(old: Sequence[Token], new: Sequence[Token]) -> PairResult:
    old_units = _split_top_level(strip_comments(old))
    new_units = _split_top_level(strip_comments(new))
    violations: list[Violation] = []
    additions = 0
    pad_narrowings = 0
    for tag, i1, i2, j1, j2 in _unit_opcodes(old_units, new_units):
        if tag == "equal":
            continue
        old_run = old_units[i1:i2]
        new_run = new_units[j1:j2]
        if tag == "insert":
            for unit in new_run:
                error = _validate_struct_define_insert(unit.tokens)
                if error:
                    violations.append(Violation(unit.line, error))
                else:
                    additions += 1
            continue
        if tag == "delete":
            violations.append(Violation(
                old_run[0].line,
                f"'struct-define' is additions-only, but existing code was "
                f"removed: {_show(_flat(old_run))!r}"))
            continue
        # A modified region: new units alongside it are additions, but every
        # paired unit must be byte-for-byte the same construct.
        aligned = _align_replace(old_run, new_run)
        if aligned is None:
            violations.append(Violation(
                new_run[0].line,
                f"'struct-define' is additions-only, but existing code was "
                f"removed or split: {_first_token_change(old_run, new_run)}"))
            continue
        pairs, extra = aligned
        for unit in extra:
            error = _validate_struct_define_insert(unit.tokens)
            if error:
                violations.append(Violation(unit.line, error))
            else:
                additions += 1
        for old_unit, new_unit in pairs:
            if old_unit.key == new_unit.key:
                continue
            # When the only change inside an existing construct is a type
            # definition, name that mistake precisely: it belongs in a header
            # or the declarations region, not inside a function body.
            inner = _inner_definition([old_unit], [new_unit])
            if inner:
                violations.append(Violation(
                    new_unit.line,
                    f"definition inserted inside an existing top-level "
                    f"construct (a function body, brace depth > 0): {inner}; "
                    f"definitions belong in the declarations region or a header"))
                continue
            if _is_pad_narrowing(old_unit, new_unit):
                # Subdividing a pad_ run is the one legitimate modification in
                # this category: naming an offset that the lifted source
                # demonstrably reads means shrinking the pad that covered it.
                # Doctrine requires it (CLAUDE.md: a pad_ field that turns out
                # to be read is a recovery bug), and it cannot be expressed as
                # a pure addition. Total span is not re-derived here -- the
                # cs()/co() asserts and the byte-identical codegen gate already
                # prove it, and duplicating that check here would be weaker.
                pad_narrowings += 1
                continue
            violations.append(Violation(
                new_unit.line,
                f"'struct-define' is additions-only, but existing code was "
                f"modified: {_first_token_change([old_unit], [new_unit])}"))
    if violations:
        return _bad(violations)
    notes = [f"{additions} added definition(s); no pre-existing construct touched"]
    if pad_narrowings:
        notes.append(f"{pad_narrowings} pad_ run(s) subdivided to name accessed offsets")
    return _pure(notes)


_UNKNOWN_FIELD = re.compile(r"^(?:pad|field)_[0-9a-f]+$")
_SCALAR_TYPE = frozenset({
    "char", "signed", "unsigned", "short", "int", "long", "float", "double",
    "int8_t", "uint8_t", "int16_t", "uint16_t", "int32_t", "uint32_t",
    "int64_t", "uint64_t", "bool", "real",
})


def _is_pad_narrowing(old_unit: Unit, new_unit: Unit) -> bool:
    """True when a struct body changed ONLY by subdividing `pad_` runs.

    Subdividing a pad is the one legitimate modification in this category, and
    it cannot be expressed as a pure addition: naming an offset the lifted
    source demonstrably reads means shrinking the pad that used to cover it.

    Comparing raw tokens is too weak (it lets `int32_t x` -> `float x` pass as
    "two scalar type tokens"), so the check works on parsed DECLARATIONS:

      * every REMOVED declaration must be a `pad_` run -- an existing NAMED
        field can never be touched, only unknown filler;
      * every INSERTED declaration must be `pad_`/`field_` with a scalar type
        -- no semantic name, no nested struct, no code rides along; and
      * the removed and inserted byte spans must be EQUAL -- a subdivision
        redistributes bytes, it never invents or drops them.
    """
    old_decls = _struct_declarations(strip_comments(old_unit.tokens))
    new_decls = _struct_declarations(strip_comments(new_unit.tokens))
    if old_decls is None or new_decls is None:
        return False
    for tag, i1, i2, j1, j2 in _opcodes(old_decls, new_decls):
        if tag == "equal":
            continue
        removed, inserted = old_decls[i1:i2], new_decls[j1:j2]
        if not all(d.name.startswith("pad_") for d in removed):
            return False
        if not all(_UNKNOWN_FIELD.fullmatch(d.name) and d.width for d in inserted):
            return False
        if sum(d.span for d in removed) != sum(d.span for d in inserted):
            return False
    return True


class _Decl(NamedTuple):
    type_name: str
    name: str
    count: int
    width: int

    @property
    def span(self) -> int:
        return self.width * max(self.count, 1)

    @property
    def key(self) -> str:
        return "%s %s[%d]" % (self.type_name, self.name, self.count)


_DECL_WIDTH = {
    "char": 1, "signed char": 1, "unsigned char": 1, "int8_t": 1, "uint8_t": 1,
    "bool": 1, "short": 2, "unsigned short": 2, "int16_t": 2, "uint16_t": 2,
    "int": 4, "unsigned int": 4, "unsigned": 4, "long": 4, "unsigned long": 4,
    "int32_t": 4, "uint32_t": 4, "float": 4, "real": 4,
    "double": 8, "int64_t": 8, "uint64_t": 8,
}


def _struct_declarations(tokens: Sequence[Token]) -> list[_Decl] | None:
    """Parse a struct body's field declarations, or None if it is not one.

    Returns None (rather than guessing) for anything with a pointer, bitfield,
    nested brace, or multi-declarator field -- callers treat None as "not a
    plain subdivision", which fails closed.
    """
    texts = [t.text for t in tokens]
    try:
        start = texts.index("{")
    except ValueError:
        return None
    depth, end = 0, None
    for index in range(start, len(texts)):
        if texts[index] == "{":
            depth += 1
        elif texts[index] == "}":
            depth -= 1
            if depth == 0:
                end = index
                break
    if end is None:
        return None
    body = texts[start + 1:end]
    if "{" in body or "*" in body or ":" in body or "," in body:
        return None
    decls: list[_Decl] = []
    for chunk in " ".join(body).split(";"):
        parts = chunk.split()
        if not parts:
            continue
        count = 1
        if parts[-1].endswith("]"):
            match = re.fullmatch(r"([A-Za-z_]\w*)\[(0[xX][0-9a-fA-F]+|\d+)\]",
                                 "".join(parts[-1:]) if "[" in parts[-1]
                                 else "".join(parts[-2:]))
            if not match:
                joined = "".join(parts[1:])
                match = re.fullmatch(r"([A-Za-z_]\w*)\[(0[xX][0-9a-fA-F]+|\d+)\]", joined)
            if not match:
                return None
            name, count = match.group(1), int(match.group(2), 0)
            type_name = " ".join(parts[:-1]) if "[" in parts[-1] else " ".join(parts[:-2])
            type_name = type_name.rsplit(name, 1)[0].strip() or type_name
        else:
            if len(parts) < 2:
                return None
            name = parts[-1]
            type_name = " ".join(parts[:-1])
        decls.append(_Decl(type_name, name, count, _DECL_WIDTH.get(type_name, 0)))
    return decls


def _inner_definition(old_run: Sequence[Unit], new_run: Sequence[Unit]) -> str | None:
    """When a modified unit's only change is an inserted type definition,
    return a short rendering of it (used for a precise diagnostic)."""
    old_tokens = _flat(old_run)
    new_tokens = _flat(new_run)
    for i1, i2, j1, j2 in _hunks(old_tokens, new_tokens, max_gap=0):
        if i1 != i2:
            return None
        run = new_tokens[j1:j2]
        # difflib often shifts an insertion boundary onto the preceding `;` or
        # `}`; skip that punctuation before looking for the keyword.
        while run and run[0].kind == "punct" and run[0].text in (";", "}", "{"):
            run = run[1:]
        if not run:
            return None
        if not (_is_id(run[0], "typedef") or _is_id(run[0], "struct")
                or _is_id(run[0], "union") or _is_id(run[0], "enum")):
            return None
        return _show(run)
    return None


# --------------------------------------------------------------------------
# Category: offset-to-field
# --------------------------------------------------------------------------

def _has_pointer_cast(tokens: Sequence[Token]) -> bool:
    """True when the run contains `( <type-ish tokens> * )`."""
    for index, tok in enumerate(tokens):
        if not _is_punct(tok, "("):
            continue
        cursor = index + 1
        saw_type = False
        while cursor < len(tokens):
            current = tokens[cursor]
            if current.kind == "id":
                saw_type = True
                cursor += 1
                continue
            if _is_punct(current, "*"):
                cursor += 1
                continue
            break
        if saw_type and cursor < len(tokens) and _is_punct(tokens[cursor], ")") \
                and _is_punct(tokens[cursor - 1], "*"):
            return True
    return False


def _has_literal_offset_add(tokens: Sequence[Token]) -> bool:
    """True when the run contains `+ <literal>` (the raw byte offset)."""
    for index in range(len(tokens) - 1):
        if _is_punct(tokens[index], "+") and tokens[index + 1].kind == "num":
            return True
    return False


def _has_member_access(tokens: Sequence[Token]) -> bool:
    return any(_is_punct(tok, "->") or _is_punct(tok, ".") for tok in tokens)


def _validate_offset_insert(chunk: Sequence[Token]) -> str | None:
    name = _directive_name(chunk)
    if name is not None:
        if name == "include":
            return None
        return (f"added '#{name}' directive is not part of an offset->field "
                f"rewrite")
    if not chunk:
        return None
    if not chunk[-1].kind == "punct" or chunk[-1].text != ";":
        return f"added code is not a declaration statement: {_show(chunk)!r}"
    if any(_is_punct(tok, "{") for tok in chunk):
        return f"added code contains a block: {_show(chunk)!r}"
    if not (chunk[0].kind == "id"):
        return (f"added statement does not start with a type name: "
                f"{_show(chunk)!r}")
    if not any(_is_punct(tok, "*") for tok in chunk):
        return (f"added statement is not a typed pointer declaration: "
                f"{_show(chunk)!r}")
    return None


def _check_offset_to_field(old: Sequence[Token], new: Sequence[Token]) -> PairResult:
    old_units = _split_top_level(strip_comments(old))
    new_units = _split_top_level(strip_comments(new))
    violations: list[Violation] = []
    counters = {"rewrites": 0, "additions": 0}
    for tag, i1, i2, j1, j2 in _unit_opcodes(old_units, new_units):
        if tag == "equal":
            continue
        old_run = old_units[i1:i2]
        new_run = new_units[j1:j2]
        if tag == "insert":
            for unit in new_run:
                error = _validate_offset_insert(unit.tokens)
                if error:
                    violations.append(Violation(unit.line, error))
                else:
                    counters["additions"] += 1
            continue
        if tag == "delete":
            violations.append(Violation(
                old_run[0].line,
                f"code removed without a replacement member access: "
                f"{_show(_flat(old_run))!r}"))
            continue
        aligned = _align_replace(old_run, new_run)
        if aligned is None:
            violations.append(Violation(
                new_run[0].line,
                f"{len(old_run)} construct(s) became {len(new_run)}; an "
                f"offset->field commit rewrites derefs in place: "
                f"{_first_token_change(old_run, new_run)}"))
            continue
        pairs, extra = aligned
        for unit in extra:
            error = _validate_offset_insert(unit.tokens)
            if error:
                violations.append(Violation(unit.line, error))
            else:
                counters["additions"] += 1
        for old_unit, new_unit in pairs:
            _check_offset_unit(old_unit, new_unit, violations, counters)
    if violations:
        return _bad(violations)
    return _pure([f"{counters['rewrites']} offset->field rewrite(s), "
                  f"{counters['additions']} added declaration(s)/include(s)"])


def _retains_raw_offset(tokens: Sequence[Token]) -> bool:
    """True when a run still casts a pointer AND adds a literal byte offset."""
    return _has_pointer_cast(tokens) and _has_literal_offset_add(tokens)


# Explains the "field at +0x00 reached through a cast base" rejection.  Retyping
# a deref -- `*(int16 *)(p + 0x1a)` -> `((tag_block *)(p + 0x1a))->count` -- keeps
# both the cast and the raw `+ 0x1a`, so no raw offset was actually eliminated and
# the token hunks fragment around the shared base.  The per-hunk violations then
# read as tool bugs ("no '+ <literal>' byte offset", "added code is not a
# declaration statement") and send the reader looking for a spelling that works.
# There is none: the offset is only removable once a struct covers the BASE.
_CAST_BASE_HINT = (
    "hint: the replacement still casts and adds a raw byte offset, so this is a "
    "retype, not an offset->field conversion (the field sits at +0x00 of a type "
    "reached through a cast base, e.g. '((tag_block *)(p + 0x1a))->count'). No "
    "spelling of this passes: the '+ literal' only disappears once a struct with "
    "cs()/co() asserts covers the BASE, making the offset a named member. That is "
    "struct-recovery/struct-assert (ladder 5) work, not this category."
)


def _check_offset_unit(old_unit: Unit, new_unit: Unit,
                       violations: list[Violation], counters: dict) -> None:
    before = len(violations)
    _check_offset_hunks(old_unit, new_unit, violations, counters)
    # Additive diagnosis only -- the unit has already failed; this appends the
    # reason rather than changing any verdict.
    if len(violations) > before and _retains_raw_offset(new_unit.tokens) \
            and _retains_raw_offset(old_unit.tokens):
        violations.append(Violation(new_unit.line, _CAST_BASE_HINT))


def _check_offset_hunks(old_unit: Unit, new_unit: Unit,
                        violations: list[Violation], counters: dict) -> None:
    for i1, i2, j1, j2 in _hunks(old_unit.tokens, new_unit.tokens):
        old_run = old_unit.tokens[i1:i2]
        new_run = new_unit.tokens[j1:j2]
        line = new_unit.tokens[j1].line if j1 < len(new_unit.tokens) else new_unit.line
        if not old_run:  # pure insertion inside the body
            for chunk in _split_statements(new_run):
                error = _validate_offset_insert(chunk)
                if error:
                    violations.append(Violation(chunk[0].line if chunk else line,
                                                error))
                else:
                    counters["additions"] += 1
            continue
        if not new_run:
            violations.append(Violation(
                line, f"code removed without a replacement member access: "
                      f"{_show(old_run)!r}"))
            continue
        # Substitution: the old side must be the raw-deref shape and the new
        # side a member access.  A hunk that mixes anything else is rejected.
        if not _has_pointer_cast(old_run):
            violations.append(Violation(
                line, f"replaced code is not a raw pointer-cast deref (no "
                      f"'( type * )' cast): {_show(old_run)!r} -> "
                      f"{_show(new_run)!r}"))
            continue
        if not _has_literal_offset_add(old_run):
            violations.append(Violation(
                line, f"replaced code has no '+ <literal>' byte offset: "
                      f"{_show(old_run)!r} -> {_show(new_run)!r}"))
            continue
        if not _has_member_access(new_run):
            violations.append(Violation(
                line, f"replacement has no '->' or '.' member access: "
                      f"{_show(old_run)!r} -> {_show(new_run)!r}"))
            continue
        counters["rewrites"] += 1


# --------------------------------------------------------------------------
# Public core
# --------------------------------------------------------------------------

_CHECKERS = {
    "comments": _check_comments,
    "local-renames": _check_renames,
    "symbol-names": _check_renames,
    "const-enum": _check_const_enum,
    "struct-define": _check_struct_define,
    "offset-to-field": _check_offset_to_field,
}


def check_pair(category: str, old_text: str, new_text: str) -> PairResult:
    """Compare one file's before/after content against a ladder category.

    Git-free, so self-tests and unit tests need no scratch repository."""
    if category in UNCHECKABLE:
        return PairResult("unchecked", [], [
            f"'{category}' is not mechanically checkable: {UNCHECKABLE[category]}"])
    checker = _CHECKERS.get(category)
    if checker is None:
        raise ValueError(f"unknown category {category!r}; expected one of "
                         f"{', '.join(CATEGORIES)}")
    return checker(tokenize(old_text), tokenize(new_text))


# --------------------------------------------------------------------------
# Git plumbing
# --------------------------------------------------------------------------

def _git(*args: str) -> subprocess.CompletedProcess:
    # Deliberately inherits the process CWD instead of pinning `ROOT`: recovery
    # sessions run inside git worktrees, and `--staged` must mean "the index of
    # the repository I am standing in".
    return subprocess.run(["git", *args], capture_output=True, text=True)


def _git_text(*args: str) -> str | None:
    result = _git(*args)
    if result.returncode != 0:
        return None
    return result.stdout


class ChangedFile(NamedTuple):
    path: str
    old_text: str
    new_text: str


def _collect_staged(paths: Sequence[str]) -> list[ChangedFile]:
    listing = _git_text("diff", "--cached", "--name-only", "--no-renames", *paths)
    if listing is None:
        raise SystemExit("[purity] error: `git diff --cached` failed")
    files = []
    for name in [line.strip() for line in listing.splitlines() if line.strip()]:
        old = _git_text("show", f"HEAD:{name}") or ""
        new = _git_text("show", f":{name}") or ""
        files.append(ChangedFile(name, old, new))
    return files


def _collect_revision(revision: str, paths: Sequence[str]) -> list[ChangedFile]:
    parents = _git_text("rev-list", "--parents", "-n", "1", revision)
    if parents is None:
        raise SystemExit(f"[purity] error: unknown revision {revision!r}")
    if len(parents.split()) > 2:
        raise _Unchecked(f"{revision} is a merge commit; merge diffs are not checked")
    listing = _git_text(
        "diff-tree", "--no-commit-id", "--name-only", "-r", "--no-renames",
        revision, "--", *paths)
    if listing is None:
        raise SystemExit(f"[purity] error: cannot diff {revision!r}")
    files = []
    for name in [line.strip() for line in listing.splitlines() if line.strip()]:
        old = _git_text("show", f"{revision}^:{name}") or ""
        new = _git_text("show", f"{revision}:{name}") or ""
        files.append(ChangedFile(name, old, new))
    return files


class _Unchecked(Exception):
    """Raised when the diff itself cannot be judged (exit 2, not a failure)."""


def _is_source(path: str) -> bool:
    return Path(path).suffix.lower() in SOURCE_SUFFIXES


# --------------------------------------------------------------------------
# CLI
# --------------------------------------------------------------------------

def _explain(category: str | None) -> None:
    print(f"[purity] {TOOL_VERSION} -- allowed edit shape per ladder category")
    for name in CATEGORIES:
        if category and name != category:
            continue
        marker = "  " if name not in UNCHECKABLE else "! "
        print(f"{marker}{name:<16} {RULES[name]}")
    print("  (exit 0 pure / 1 violation / 2 not mechanically checkable)")


def _run(category: str, files: Sequence[ChangedFile], source_label: str,
         explain: bool) -> int:
    if explain:
        _explain(category)
    if category in UNCHECKABLE:
        print(f"[purity] NOT MECHANICALLY CHECKABLE: '{category}' -- "
              f"{UNCHECKABLE[category]}")
        print("[purity] treated as PASS; the VC71 codegen gate and the "
              "behavioural oracle are the gates for this category")
        return 2

    ignored = [f.path for f in files if not _is_source(f.path)]
    sources = [f for f in files if _is_source(f.path)]
    print(f"[purity] category={category} source={source_label} "
          f"files={len(sources)}")
    if ignored:
        shown = ", ".join(ignored[:4]) + (" ..." if len(ignored) > 4 else "")
        print(f"[purity] note: ignored {len(ignored)} non-source file(s) "
              f"({shown}); only {'/'.join(sorted(SOURCE_SUFFIXES))} are checked "
              f"-- recovery manifests and docs are not C")
    if not sources:
        print("[purity] PASS -- no source files in the diff, nothing to check")
        return 0

    failures = 0
    for changed in sources:
        result = check_pair(category, changed.old_text, changed.new_text)
        if result.ok:
            note = f" -- {result.notes[0]}" if result.notes else ""
            print(f"[purity] PURE {changed.path}{note}")
            continue
        failures += len(result.violations)
        print(f"[purity] VIOLATION {changed.path}")
        for violation in result.violations[:20]:
            print(violation.format())
        if len(result.violations) > 20:
            print(f"  ... and {len(result.violations) - 20} more")

    if failures:
        print(f"[purity] FAIL -- {failures} disallowed change(s) for category "
              f"'{category}'")
        print("[purity] split the commit: one commit per ladder category "
              "(source-recovery SKILL.md Separation rule), or declare the "
              "category that actually covers these edits")
        return 1
    print(f"[purity] PASS -- diff matches the '{category}' edit shape")
    return 0


# --------------------------------------------------------------------------
# Self-test
# --------------------------------------------------------------------------

BASE_C = """\
/* observer camera */
#include "types.h"

void update(float *up, actor_t *actor) {
  float fVar1;
  int local_8;

  fVar1 = up[0] * 2.0f;
  local_8 = *(int *)((int)actor + 0x10);
  if (local_8 == 3) {
    up[1] = fVar1;
  }
}
"""


def _cases() -> list[tuple[str, str, str, str, str]]:
    """(label, category, old, new, expected-status)."""
    cases: list[tuple[str, str, str, str, str]] = []

    # ---- comments -------------------------------------------------------
    cases.append((
        "comments: added block comment + blank lines",
        "comments", BASE_C,
        BASE_C.replace("  fVar1 = up[0] * 2.0f;",
                       "\n  /* scale by the near-plane factor */\n"
                       "  fVar1 = up[0] * 2.0f;  // evidence: 0x8c150\n"),
        "pure"))
    cases.append((
        "comments: literal changed under cover of a comment edit",
        "comments", BASE_C,
        BASE_C.replace("* 2.0f;", "* 3.0f; /* tweak */"),
        "violation"))
    cases.append((
        "comments: identifier renamed",
        "comments", BASE_C, BASE_C.replace("fVar1", "scale"),
        "violation"))

    # ---- local-renames / symbol-names -----------------------------------
    cases.append((
        "local-renames: consistent rename plus comment refresh",
        "local-renames", BASE_C,
        BASE_C.replace("fVar1", "near_plane_scale").replace(
            "/* observer camera */", "/* observer camera (near-plane scale) */"),
        "pure"))
    cases.append((
        "symbol-names: same shape, aliased implementation",
        "symbol-names", BASE_C, BASE_C.replace("update", "observer_update"),
        "pure"))
    cases.append((
        "local-renames: inconsistent mapping for one old name",
        "local-renames", BASE_C,
        BASE_C.replace("fVar1 = up[0]", "scale = up[0]", 1).replace(
            "up[1] = fVar1;", "up[1] = factor;", 1).replace(
            "float fVar1;", "float scale;", 1),
        "violation"))
    cases.append((
        "local-renames: two names collapsed into one (non-injective)",
        "local-renames", BASE_C,
        BASE_C.replace("fVar1", "value").replace("local_8", "value"),
        "violation"))
    cases.append((
        "local-renames: type changed (int -> short) is not a rename",
        "local-renames", BASE_C, BASE_C.replace("int local_8;", "short local_8;"),
        "violation"))
    cases.append((
        "local-renames: extra statement smuggled in",
        "local-renames", BASE_C,
        BASE_C.replace("  int local_8;", "  int local_8;\n  up[2] = 0.0f;"),
        "violation"))

    # ---- const-enum -----------------------------------------------------
    cases.append((
        "const-enum: define added and literal replaced by the name",
        "const-enum",
        BASE_C,
        BASE_C.replace('#include "types.h"',
                       '#include "types.h"\n#define ACTOR_STATE_DEAD 3')
              .replace("local_8 == 3", "local_8 == ACTOR_STATE_DEAD"),
        "pure"))
    cases.append((
        "const-enum: enum block added",
        "const-enum", BASE_C,
        BASE_C.replace('#include "types.h"',
                       '#include "types.h"\nenum { STATE_IDLE = 0, STATE_DEAD = 3 };'),
        "pure"))
    cases.append((
        "const-enum: literal value changed instead of named",
        "const-enum", BASE_C, BASE_C.replace("local_8 == 3", "local_8 == 4"),
        "violation"))
    cases.append((
        "const-enum: control flow edited alongside the define",
        "const-enum", BASE_C,
        BASE_C.replace('#include "types.h"',
                       '#include "types.h"\n#define ACTOR_STATE_DEAD 3')
              .replace("if (local_8 == 3) {", "if (local_8 != ACTOR_STATE_DEAD) {"),
        "violation"))
    cases.append((
        "const-enum: struct definition added (wrong category)",
        "const-enum", BASE_C,
        BASE_C.replace('#include "types.h"',
                       '#include "types.h"\ntypedef struct { int a; } thing_t;'),
        "violation"))

    # ---- struct-define --------------------------------------------------
    cases.append((
        "struct-define: typedef + cs()/co() asserts appended",
        "struct-define", BASE_C,
        BASE_C + "\ntypedef struct observer_s { int32_t mode; } observer_t;\n"
                 "cs(observer_t, 4);\nco(observer_t, mode, 0x000);\n",
        "pure"))
    cases.append((
        "struct-define: new header with include guard",
        "struct-define", "",
        "#ifndef OBSERVER_H\n#define OBSERVER_H\n#include \"types.h\"\n"
        "typedef struct { int32_t mode; } observer_t;\ncs(observer_t, 4);\n#endif\n",
        "pure"))
    cases.append((
        "struct-define: a pre-existing line was modified too",
        "struct-define",
        BASE_C,
        BASE_C.replace("float fVar1;", "float scale;")
        + "\ntypedef struct { int32_t mode; } observer_t;\n",
        "violation"))
    cases.append((
        "struct-define: definition inserted inside a function body",
        "struct-define", BASE_C,
        BASE_C.replace("  int local_8;",
                       "  int local_8;\n  typedef struct { int a; } inner_t;"),
        "violation"))
    cases.append((
        "struct-define: valued #define belongs to const-enum",
        "struct-define", BASE_C,
        BASE_C.replace('#include "types.h"',
                       '#include "types.h"\n#define OBSERVER_MODE_MAX 4'),
        "violation"))

    # ---- offset-to-field ------------------------------------------------
    cases.append((
        "offset-to-field: raw deref becomes member access",
        "offset-to-field", BASE_C,
        BASE_C.replace("*(int *)((int)actor + 0x10)", "actor->mode"),
        "pure"))
    cases.append((
        "offset-to-field: rewrite plus a typed base-pointer local",
        "offset-to-field", BASE_C,
        BASE_C.replace("  int local_8;", "  int local_8;\n  observer_t *obs;")
              .replace("*(int *)((int)actor + 0x10)", "obs->mode"),
        "pure"))
    cases.append((
        "offset-to-field: offset silently changed while rewriting",
        "offset-to-field", BASE_C,
        BASE_C.replace("*(int *)((int)actor + 0x10)",
                       "*(int *)((int)actor + 0x14)"),
        "violation"))
    cases.append((
        "offset-to-field: unrelated expression rewritten",
        "offset-to-field", BASE_C, BASE_C.replace("up[0] * 2.0f", "2.0f * up[0]"),
        "violation"))
    cases.append((
        "offset-to-field: rename smuggled into the rewrite",
        "offset-to-field", BASE_C,
        BASE_C.replace("*(int *)((int)actor + 0x10)", "actor->mode")
              .replace("fVar1", "scale"),
        "violation"))
    cases.append((
        "offset-to-field: retype through a cast base is still a violation",
        "offset-to-field", BASE_C,
        BASE_C.replace("*(int *)((int)actor + 0x10)",
                       "((tag_block *)((int)actor + 0x10))->count"),
        "violation"))

    # ---- uncheckable ----------------------------------------------------
    for category in sorted(UNCHECKABLE):
        cases.append((
            f"{category}: reported as not mechanically checkable (identical)",
            category, BASE_C, BASE_C, "unchecked"))
        cases.append((
            f"{category}: reported as not mechanically checkable (rewritten)",
            category, BASE_C,
            BASE_C.replace("if (local_8 == 3) {", "if (local_8 != 3) goto done;"),
            "unchecked"))
        cases.append((
            f"{category}: reported as not mechanically checkable (emptied)",
            category, BASE_C, "", "unchecked"))
    return cases


def _self_test() -> int:
    failures = 0
    cases = _cases()
    for label, category, old, new, expected in cases:
        result = check_pair(category, old, new)
        if result.status != expected:
            failures += 1
            print(f"[purity] SELFTEST FAIL {label}: expected {expected}, got "
                  f"{result.status}")
            for violation in result.violations[:3]:
                print(f"    {violation.format()}")
        else:
            print(f"[purity] ok {label}")

    # Tokenizer sanity: the shapes most likely to break a hand-rolled lexer.
    tricky = tokenize(
        'int x = 0x1p3 + 1.5e-3f; char *s = "a/*b\\"c"; char c = \'\\\'\';'
        ' /* /* not nested */ y >>= 1; // tail\n')
    kinds = [t.kind for t in tricky]
    for expectation, actual in (
        ("has a string token", "str" in kinds),
        ("has a char token", "chr" in kinds),
        ("has two comments", kinds.count("comment") == 2),
        ("hex float stays one token", any(t.text == "0x1p3" for t in tricky)),
        ("exponent stays one token", any(t.text == "1.5e-3f" for t in tricky)),
        ("compound operator kept", any(t.text == ">>=" for t in tricky)),
    ):
        if not actual:
            failures += 1
            print(f"[purity] SELFTEST FAIL tokenizer: {expectation}")
        else:
            print(f"[purity] ok tokenizer {expectation}")

    checked = {c[1] for c in cases}
    missing = set(CATEGORIES) - checked
    if missing:
        failures += 1
        print(f"[purity] SELFTEST FAIL no cases for: {', '.join(sorted(missing))}")

    print(f"[purity] self-test: {len(cases)} category case(s), "
          f"{failures} failure(s)")
    return 1 if failures else 0


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        prog="check_category_purity.py",
        description="Verify a diff matches its declared source-recovery ladder "
                    "category's allowed edit shape.")
    parser.add_argument("category", nargs="?", choices=CATEGORIES,
                        help="declared ladder category for this commit")
    scope = parser.add_mutually_exclusive_group()
    scope.add_argument("--staged", action="store_true",
                       help="check the staged diff (default)")
    scope.add_argument("--revision", metavar="SHA",
                       help="check an existing commit against its parent")
    parser.add_argument("--source", action="append", default=[], metavar="PATH",
                        help="limit the check to these paths (repeatable)")
    parser.add_argument("--explain", action="store_true",
                        help="print the allowed edit shape before checking")
    parser.add_argument("--self-test", action="store_true",
                        help="run built-in synthetic cases; no git needed")
    args = parser.parse_args(argv)

    if args.self_test:
        return _self_test()
    if not args.category:
        if args.explain:
            _explain(None)
            return 0
        parser.error("a category is required (or use --self-test/--explain)")

    try:
        if args.revision:
            files = _collect_revision(args.revision, args.source)
            label = args.revision
        else:
            files = _collect_staged(args.source)
            label = "staged"
    except _Unchecked as exc:
        print(f"[purity] NOT CHECKABLE: {exc}")
        return 2

    return _run(args.category, files, label, args.explain)


if __name__ == "__main__":
    sys.exit(main())
