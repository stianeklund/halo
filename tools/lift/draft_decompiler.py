#!/usr/bin/env python3
"""draft_decompiler.py — Stage 1+2 Ghidra pseudocode rewriter.

Converts raw Ghidra decompiler output into a cleaner C draft suitable as a
starting point for the lift agent.  Two stages are combined in a single pass:

  Stage 1: Type/op canonicalization
    - undefined/undefined1/2/4/8 -> canonical integer types
    - float10 -> long double
    - CONCAT44/22/11/84/etc. -> explicit shift/or expressions
    - SUB44/48/42/21 -> explicit shift/cast expressions
    - ZEXT14/24/28/48 -> explicit zero-extend casts
    - SEXT14/24/28/48 -> explicit sign-extend casts
    - extraout_<reg>  -> preserved verbatim with ABI-hazard comment
    - in_<reg> / in_FS_OFFSET -> preserved verbatim with ABI-hazard comment

  Stage 2: MSVC intrinsic rewriter
    - _ftol2(x)        -> (int)(x)
    - _chkstk(...)     -> removed (comment only)
    - _allmul(a, b)    -> ((int64_t)(a) * (int64_t)(b))
    - _aullshr(v, sh)  -> ((uint64_t)(v) >> (sh))
    - _aulldiv(a, b)   -> ((uint64_t)(a) / (uint64_t)(b))
    - _aullrem(a, b)   -> ((uint64_t)(a) % (uint64_t)(b))
    - __SEH_prolog/epilog -> flag comment only (body rewrap is Stage 4)

Design notes:
  - Pure regex/string transforms; no AST.  Each pattern is conservative: when
    the match is ambiguous the original text is left unchanged.
  - Transforms are applied left-to-right in a single ordered pass so later
    patterns operate on already-rewritten text (e.g. undefined4 is gone before
    CONCAT processing).
  - CONCAT/SUB/ZEXT/SEXT are matched greedily with a balanced-parens helper so
    nested calls are handled correctly.  Depth limit: 8 levels.
  - False-positive risk: identifier substrings.  All patterns are word-boundary
    or call-site anchored (followed by '(' or whitespace) to avoid matching
    inside longer names such as 'CONCATENATE'.

Usage:
    python3 tools/lift/draft_decompiler.py <file.c>
    python3 tools/lift/draft_decompiler.py --json  /path/to/FUN_XXXX.json
    python3 tools/lift/draft_decompiler.py --stdin
    python3 tools/lift/draft_decompiler.py --self-test
    python3 tools/lift/draft_decompiler.py --stats   (implies --stdin or last positional arg)
"""

import argparse
import json
import re
import sys
from collections import Counter


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _extract_args(text, start):
    """Extract comma-separated arguments from a balanced-parens call starting
    with '(' at position `start` in `text`.  Returns (args_list, end_index)
    where end_index is one past the closing ')'.  Returns (None, start) on
    failure (unbalanced or not starting with '(').
    """
    if start >= len(text) or text[start] != '(':
        return None, start
    depth = 0
    arg_start = start + 1
    args = []
    i = start
    while i < len(text):
        ch = text[i]
        if ch == '(':
            depth += 1
        elif ch == ')':
            depth -= 1
            if depth == 0:
                args.append(text[arg_start:i].strip())
                return args, i + 1
        elif ch == ',' and depth == 1:
            args.append(text[arg_start:i].strip())
            arg_start = i + 1
        i += 1
    return None, start  # unbalanced


def _replace_calls(text, pattern, replacer):
    """Replace all non-overlapping occurrences of `pattern` (a compiled regex
    that matches a function name) followed by a balanced-paren argument list,
    applying `replacer(match_name, args_list) -> str | None`.

    If replacer returns None the original text is left unchanged for that site.
    """
    result = []
    pos = 0
    for m in pattern.finditer(text):
        result.append(text[pos:m.start()])
        args, end = _extract_args(text, m.end())
        if args is None:
            # No balanced parens found — leave unchanged.
            result.append(m.group(0))
            pos = m.end()
            continue
        replacement = replacer(m.group(0), args)
        if replacement is None:
            result.append(text[m.start():end])
        else:
            result.append(replacement)
        pos = end
    result.append(text[pos:])
    return ''.join(result)


# ---------------------------------------------------------------------------
# Transform registry
# ---------------------------------------------------------------------------

class Transform:
    """A named regex-based transform with a counter."""

    def __init__(self, name, apply_fn):
        self.name = name
        self.apply_fn = apply_fn
        self.count = 0

    def apply(self, text):
        new_text, n = self.apply_fn(text)
        self.count += n
        return new_text


# ---------------------------------------------------------------------------
# Stage 1: Type canonicalization
# ---------------------------------------------------------------------------

# undefined* type name replacements (word-boundary safe)
# Order matters: longer names first to avoid partial matches.
_UNDEF_TYPE_MAP = [
    (re.compile(r'\bundefined8\b'), 'uint64_t'),
    (re.compile(r'\bundefined4\b'), 'uint32_t'),
    (re.compile(r'\bundefined3\b'), 'uint32_t /* was undefined3 */'),
    (re.compile(r'\bundefined2\b'), 'uint16_t'),
    (re.compile(r'\bundefined1\b'), 'uint8_t'),
    # Plain 'undefined' only when used as a return type or local-var type
    # (i.e. followed by space+identifier or '*') — avoids touching longer names.
    (re.compile(r'\bundefined(?=[* \t\r\n])'), 'uint8_t'),
]

def _apply_undef_types(text):
    count = 0
    for pat, repl in _UNDEF_TYPE_MAP:
        new = pat.sub(repl, text)
        count += len(pat.findall(text))
        text = new
    return text, count


def _make_undef_transform():
    def fn(text):
        return _apply_undef_types(text)
    return Transform('undefined_types', fn)


def _make_float10_transform():
    pat = re.compile(r'\bfloat10\b')
    def fn(text):
        new = pat.sub('long double', text)
        return new, len(pat.findall(text))
    return Transform('float10', fn)


# CONCAT — covers CONCAT11, CONCAT22, CONCAT44, CONCAT84, CONCAT48, etc.
# Pattern: CONCATxy where x = high bytes, y = low bytes (both decimal digits).
# Expansion: (((uint<x*8>_t)(hi) << (y*8)) | (uint<y*8>_t)(lo))
_CONCAT_PAT = re.compile(r'\bCONCAT(\d)(\d)\b')

def _concat_replacer(name, args):
    m = _CONCAT_PAT.match(name)
    if m is None or len(args) != 2:
        return None
    hi_bytes = int(m.group(1))
    lo_bytes = int(m.group(2))
    total_bytes = hi_bytes + lo_bytes
    total_bits = total_bytes * 8
    lo_bits = lo_bytes * 8
    lo_type = 'uint%d_t' % lo_bits if lo_bits in (8, 16, 32, 64) else 'uint32_t'
    total_type = 'uint%d_t' % total_bits if total_bits in (8, 16, 32, 64) else 'uint64_t'
    hi, lo = args[0], args[1]
    return ('((%s)(%s) << %d) | (%s)(%s) /* AUTO: was %s */'
            % (total_type, hi, lo_bits, lo_type, lo, name))

def _make_concat_transform():
    pat = re.compile(r'\bCONCAT\d\d\b')
    def fn(text):
        count_cell = [0]
        def counting_replacer(name, args):
            result = _concat_replacer(name, args)
            if result is not None:
                count_cell[0] += 1
            return result
        new = _replace_calls(text, pat, counting_replacer)
        return new, count_cell[0]
    return Transform('CONCAT', fn)


# SUBxy — extract sub-word from a wider value.
# SUB44(val, off) -> ((uint32_t)((val) >> ((off)*8)))
# SUB48(val, off) -> ((uint32_t)((val) >> ((off)*8)))  [high 4 of 8]
# SUB42(val, off) -> ((uint16_t)((val) >> ((off)*8)))
# SUB21(val, off) -> ((uint8_t)((val)  >> ((off)*8)))
_SUB_PAT = re.compile(r'\bSUB(\d)(\d)\b')
_SUB_RESULT_TYPE = {1: 'uint8_t', 2: 'uint16_t', 4: 'uint32_t', 8: 'uint64_t'}

def _sub_replacer(name, args):
    m = _SUB_PAT.match(name)
    if m is None or len(args) != 2:
        return None
    result_bytes = int(m.group(1))
    rtype = _SUB_RESULT_TYPE.get(result_bytes, 'uint32_t')
    val, off = args[0], args[1]
    return ('((%s)((%s) >> ((%s) * 8))) /* AUTO: was %s */'
            % (rtype, val, off, name))

def _make_sub_transform():
    pat = re.compile(r'\bSUB\d\d\b')
    def fn(text):
        count_cell = [0]
        def counting_replacer(name, args):
            result = _sub_replacer(name, args)
            if result is not None:
                count_cell[0] += 1
            return result
        new = _replace_calls(text, pat, counting_replacer)
        return new, count_cell[0]
    return Transform('SUB', fn)


# ZEXTxy — zero-extend x bytes to y bytes.
# ZEXT14(b) -> ((uint32_t)(b))
_ZEXT_PAT = re.compile(r'\bZEXT(\d)(\d)\b')
_ZEXT_RESULT_TYPE = {1: 'uint8_t', 2: 'uint16_t', 4: 'uint32_t', 8: 'uint64_t'}

def _zext_replacer(name, args):
    m = _ZEXT_PAT.match(name)
    if m is None or len(args) != 1:
        return None
    result_bytes = int(m.group(2))
    rtype = _ZEXT_RESULT_TYPE.get(result_bytes, 'uint32_t')
    return ('((%s)(%s)) /* AUTO: was %s */' % (rtype, args[0], name))

def _make_zext_transform():
    pat = re.compile(r'\bZEXT\d\d\b')
    def fn(text):
        count_cell = [0]
        def counting_replacer(name, args):
            result = _zext_replacer(name, args)
            if result is not None:
                count_cell[0] += 1
            return result
        new = _replace_calls(text, pat, counting_replacer)
        return new, count_cell[0]
    return Transform('ZEXT', fn)


# SEXTxy — sign-extend x bytes to y bytes.
# SEXT14(b) -> ((int32_t)(int8_t)(b))
_SEXT_PAT = re.compile(r'\bSEXT(\d)(\d)\b')
_SEXT_SRC_TYPE  = {1: 'int8_t',  2: 'int16_t', 4: 'int32_t', 8: 'int64_t'}
_SEXT_DST_TYPE  = {1: 'int8_t',  2: 'int16_t', 4: 'int32_t', 8: 'int64_t'}

def _sext_replacer(name, args):
    m = _SEXT_PAT.match(name)
    if m is None or len(args) != 1:
        return None
    src_bytes = int(m.group(1))
    dst_bytes = int(m.group(2))
    stype = _SEXT_SRC_TYPE.get(src_bytes, 'int8_t')
    dtype = _SEXT_DST_TYPE.get(dst_bytes, 'int32_t')
    return ('((%s)(%s)(%s)) /* AUTO: was %s */' % (dtype, stype, args[0], name))

def _make_sext_transform():
    pat = re.compile(r'\bSEXT\d\d\b')
    def fn(text):
        count_cell = [0]
        def counting_replacer(name, args):
            result = _sext_replacer(name, args)
            if result is not None:
                count_cell[0] += 1
            return result
        new = _replace_calls(text, pat, counting_replacer)
        return new, count_cell[0]
    return Transform('SEXT', fn)


# extraout_<reg> — register-escape from previous CALL.
# Mark the first occurrence in each function body with a TODO comment.
# Subsequent occurrences are left untouched (the comment is visible above).
_EXTRAOUT_PAT = re.compile(r'\bextraout_(\w+)\b')

def _make_extraout_transform():
    def fn(text):
        seen = set()
        count = 0
        def replacer(m):
            name = m.group(0)
            if name not in seen:
                seen.add(name)
                count_cell[0] += 1
                return name + ' /* TODO: register-escape from previous CALL — see disasm */'
            return name
        count_cell = [0]
        new = _EXTRAOUT_PAT.sub(replacer, text)
        return new, count_cell[0]
    return Transform('extraout', fn)


# in_<reg> and in_FS_OFFSET — implicit register inputs, ABI hazard.
_IN_REG_PAT = re.compile(r'\bin_(FS_OFFSET|\w+)\b')

def _make_in_reg_transform():
    def fn(text):
        seen = set()
        count_cell = [0]
        def replacer(m):
            name = m.group(0)
            if name not in seen:
                seen.add(name)
                count_cell[0] += 1
                return name + ' /* TODO: implicit register input — ABI hazard, see disasm */'
            return name
        new = _IN_REG_PAT.sub(replacer, text)
        return new, count_cell[0]
    return Transform('in_reg', fn)


# ---------------------------------------------------------------------------
# Stage 2: MSVC intrinsic rewriter
# ---------------------------------------------------------------------------

def _make_ftol2_transform():
    pat = re.compile(r'\b_ftol2\b')
    def fn(text):
        count_cell = [0]
        def replacer(name, args):
            if len(args) != 1:
                return None
            count_cell[0] += 1
            return '(int)(%s) /* AUTO: was _ftol2() */' % args[0]
        new = _replace_calls(text, pat, replacer)
        return new, count_cell[0]
    return Transform('_ftol2', fn)


def _make_chkstk_transform():
    """Remove _chkstk(...) call statements, leaving only a comment.

    Strategy: use _replace_calls to find each call site, then decide per-site:
      - Statement-level (entire expression before ';'): replace with comment.
      - Sub-expression context: leave call, append inline TODO comment.

    The per-site decision is done via context inspection of the surrounding
    text.  A call is 'statement-level' when the text between the line start
    and the call start is only whitespace.
    """
    pat = re.compile(r'\b_chkstk\b')

    def fn(text):
        count_before = len(pat.findall(text))
        if count_before == 0:
            return text, 0

        result = []
        pos = 0
        changed = 0
        for m in pat.finditer(text):
            result.append(text[pos:m.start()])
            args, end = _extract_args(text, m.end())
            if args is None:
                # No balanced parens; leave unchanged.
                result.append(m.group(0))
                pos = m.end()
                continue

            # Determine if this is a statement-level call.
            # Look backward from m.start() to find the start of the line.
            line_start = text.rfind('\n', 0, m.start())
            line_start = line_start + 1 if line_start != -1 else 0
            prefix = text[line_start:m.start()]
            # Look forward from `end` for the ';' that terminates the statement.
            tail = text[end:]
            semi_match = re.match(r'\s*;', tail)

            arg_text = ', '.join(args)
            if prefix.strip() == '' and semi_match is not None:
                # Statement-level: consume through the semicolon + optional newline.
                skip = end + semi_match.end()
                # Also eat a trailing newline if present.
                if skip < len(text) and text[skip] in '\r\n':
                    if text[skip] == '\r' and skip + 1 < len(text) and text[skip + 1] == '\n':
                        skip += 2
                    else:
                        skip += 1
                result.append('/* AUTO: removed _chkstk(%s) — declare locals normally */\n' % arg_text)
                pos = skip
            else:
                # Sub-expression context: preserve call, add inline note.
                result.append('%s(%s) /* TODO: _chkstk in expression context — verify ABI */'
                              % (m.group(0), arg_text))
                pos = end
            changed += 1

        result.append(text[pos:])
        return ''.join(result), changed

    return Transform('_chkstk', fn)


def _make_allmul_transform():
    pat = re.compile(r'\b_allmul\b')
    def fn(text):
        count_cell = [0]
        def replacer(name, args):
            if len(args) < 2:
                return None
            count_cell[0] += 1
            a, b = args[0], args[1]
            return ('((int64_t)(%s) * (int64_t)(%s)) /* AUTO: was _allmul() */' % (a, b))
        new = _replace_calls(text, pat, replacer)
        return new, count_cell[0]
    return Transform('_allmul', fn)


def _make_aullshr_transform():
    pat = re.compile(r'\b_aullshr\b')
    def fn(text):
        count_cell = [0]
        def replacer(name, args):
            if len(args) < 2:
                return None
            count_cell[0] += 1
            val, sh = args[0], args[1]
            return ('((uint64_t)(%s) >> (%s)) /* AUTO: was _aullshr() */' % (val, sh))
        new = _replace_calls(text, pat, replacer)
        return new, count_cell[0]
    return Transform('_aullshr', fn)


def _make_aulldiv_transform():
    pat = re.compile(r'\b_aulldiv\b')
    def fn(text):
        count_cell = [0]
        def replacer(name, args):
            if len(args) < 2:
                return None
            count_cell[0] += 1
            a, b = args[0], args[1]
            return ('((uint64_t)(%s) / (uint64_t)(%s)) /* AUTO: was _aulldiv() */' % (a, b))
        new = _replace_calls(text, pat, replacer)
        return new, count_cell[0]
    return Transform('_aulldiv', fn)


def _make_aullrem_transform():
    pat = re.compile(r'\b_aullrem\b')
    def fn(text):
        count_cell = [0]
        def replacer(name, args):
            if len(args) < 2:
                return None
            count_cell[0] += 1
            a, b = args[0], args[1]
            return ('((uint64_t)(%s) %% (uint64_t)(%s)) /* AUTO: was _aullrem() */' % (a, b))
        new = _replace_calls(text, pat, replacer)
        return new, count_cell[0]
    return Transform('_aullrem', fn)


def _make_seh_transform():
    """Stage 2/4 SEH handling.

    Stage 4 (preferred): when a single __SEH_prolog/__SEH_epilog pair brackets
    the function body, rewrite both as native __try/__except syntax which
    clang accepts on -target i386-pc-win32 (see docs/seh-handling.md).

      __SEH_prolog(...); ...body...  __SEH_epilog();
        becomes
      __try { ...body... }
      __except(EXCEPTION_EXECUTE_HANDLER) { return 0; /* TODO: ... */ }

    Stage 2 (fallback): when prolog/epilog are unpaired or appear multiple
    times in one input, fall back to flag comments only — the lift agent
    will rewrap manually.
    """
    # Match the full statement, tolerant of trailing comments on the same line.
    # `__SEH_prolog ( ... );  optional trailing junk up to newline`
    prolog_stmt = re.compile(
        r'__SEH_prolog\s*\([^)]*\)\s*;[^\n]*')
    epilog_stmt = re.compile(
        r'__SEH_epilog\s*\([^)]*\)\s*;[^\n]*')
    # Flag-only fallback patterns (just the bare call name).
    prolog_name = re.compile(r'\b__SEH_prolog\b')
    epilog_name = re.compile(r'\b__SEH_epilog\b')

    def fn(text):
        prolog_count = len(prolog_stmt.findall(text))
        epilog_count = len(epilog_stmt.findall(text))

        # Stage 4: exactly one pair found → rewrap as __try/__except.
        if prolog_count == 1 and epilog_count == 1:
            new = prolog_stmt.sub(
                '__try { /* AUTO: was __SEH_prolog — body wrapped */', text)
            new = epilog_stmt.sub(
                '} __except(EXCEPTION_EXECUTE_HANDLER) { '
                'return 0; /* TODO: pick correct return value '
                '(0 / 0.0f / NULL / void) */ } '
                '/* AUTO: was __SEH_epilog — pair end */',
                new)
            return new, 2

        # Stage 2 fallback: flag only.
        count = (len(prolog_name.findall(text)) +
                 len(epilog_name.findall(text)))

        def prolog_repl(m):
            return (m.group(0) +
                    ' /* SEH-PROLOG: wrap function body in '
                    '__try { ... } __except(1) { return 0; } '
                    'per docs/seh-handling.md */')

        def epilog_repl(m):
            return (m.group(0) +
                    ' /* SEH-EPILOG: paired with SEH_prolog above */')

        new = prolog_name.sub(prolog_repl, text)
        new = epilog_name.sub(epilog_repl, new)
        return new, count
    return Transform('SEH', fn)


# ---------------------------------------------------------------------------
# Pipeline
# ---------------------------------------------------------------------------

TRANSFORMS = [
    # Stage 1
    _make_undef_transform(),
    _make_float10_transform(),
    _make_concat_transform(),
    _make_sub_transform(),
    _make_zext_transform(),
    _make_sext_transform(),
    _make_extraout_transform(),
    _make_in_reg_transform(),
    # Stage 2
    _make_ftol2_transform(),
    _make_chkstk_transform(),
    _make_allmul_transform(),
    _make_aullshr_transform(),
    _make_aulldiv_transform(),
    _make_aullrem_transform(),
    _make_seh_transform(),
]


def rewrite(text):
    """Apply all transforms in order and return the rewritten text."""
    for t in TRANSFORMS:
        text = t.apply(text)
    return text


# ---------------------------------------------------------------------------
# Hazard annotation pass (comment-only; does NOT rewrite code)
# ---------------------------------------------------------------------------
import re as _re

_LOOP_KW_ANN = _re.compile(r'\b(for|while|do)\b')
_LOCAL_DECL_ANN = _re.compile(
    r'^\s+(?:undefined\d*|char|short|int|long|float|double)\s+'
    r'(local_[0-9a-fA-F]+|[a-zA-Z_]\w*Stack_[0-9a-fA-F]+)\s*(?:\[\d+\])?\s*;'
)
_PTR_FROM_INT_ANN = _re.compile(
    r'=\s*\(\s*(?:short|char|int|uint\w*|int\w*)\s*\*\s*\)\s*\(\s*(?:unsigned\s+)?int\s*\)\s*'
)
_ADDR_OF_LOCAL_ANN = _re.compile(r'&(local_[0-9a-fA-F]+|[a-zA-Z_]\w*Stack_[0-9a-fA-F]+)')
_FLOAT_ZERO_ANN = _re.compile(r'\(float\)\s*0\b|= 0\.0f\b|= 0\.0\b')
_FILD_LOAD_ANN = _re.compile(r'FILD\s+\[EBP')


def annotate_hazards(text):
    """Inject HAZARD comments for patterns the rewriter cannot safely rewrite.

    This is an ADVISORY pass: comments only, no code changes.
    Patterns detected:
      §6: float bits stored via (T*)(int)(expr) assignment
      §8: possible running accumulator (loop body with float 0 / FILD load)
      §2: &local_XX passed to a call with adjacent locals in declared range

    Returns the annotated text (idempotent).
    """
    lines = text.splitlines(keepends=True)
    out = []

    # Build local decl table for §2 check
    local_offsets = {}
    for line in lines:
        m = _LOCAL_DECL_ANN.match(line)
        if m:
            name = m.group(1)
            suffix_m = _re.search(r'([0-9a-fA-F]+)$', name)
            if suffix_m:
                local_offsets[name] = int(suffix_m.group(1), 16)

    in_loop = 0
    for i, line in enumerate(lines):
        stripped = line.rstrip('\r\n')
        ann = None

        # §6: bit-smuggling assignment
        if _PTR_FROM_INT_ANN.search(line) and '/* HAZARD' not in line:
            ann = '/* HAZARD §6: float bits smuggled through pointer cast — use memcpy/union */'

        # §8: accumulator pattern inside loop
        if in_loop > 0 and not ann and '/* HAZARD' not in line:
            if _FLOAT_ZERO_ANN.search(line):
                ann = '/* HAZARD §8: (float)0 or 0.0f inside loop — verify this is not a running accumulator seed; see lift-learnings §8 */'

        # §2: address-of-local with call context
        if not ann and '/* HAZARD' not in line:
            addr_m = _ADDR_OF_LOCAL_ANN.search(line)
            if addr_m and '(' in line:  # passed to a call
                local_name = addr_m.group(1)
                if local_name in local_offsets:
                    base = local_offsets[local_name]
                    # Check if any adjacent locals exist in range [base-0x80, base+0x80]
                    adjacent = [
                        n for n, off in local_offsets.items()
                        if n != local_name and abs(off - base) < 0x80
                    ]
                    if adjacent:
                        ann = '/* HAZARD §2: address-of-local passed to callee — verify buffer contiguity/frame map; adjacent locals: %s */' % ', '.join(adjacent[:4])

        # Brace tracking for loop detection
        open_braces = line.count('{')
        close_braces = line.count('}')
        if _LOOP_KW_ANN.search(line):
            in_loop += open_braces - close_braces + 1
        else:
            in_loop = max(0, in_loop + open_braces - close_braces)

        if ann:
            eol = '\r\n' if line.endswith('\r\n') else '\n' if line.endswith('\n') else ''
            out.append(stripped + '  ' + ann + eol)
        else:
            out.append(line)

    return ''.join(out)


def stats_report():
    """Return a human-readable table of transform counts."""
    lines = ['Transform counts:']
    for t in TRANSFORMS:
        if t.count > 0:
            lines.append('  %-20s %d' % (t.name, t.count))
    if all(t.count == 0 for t in TRANSFORMS):
        lines.append('  (none)')
    return '\n'.join(lines)


def reset_counts():
    for t in TRANSFORMS:
        t.count = 0


# ---------------------------------------------------------------------------
# Input loading
# ---------------------------------------------------------------------------

def load_from_json(path):
    """Load Ghidra pseudocode from a context-cache JSON file.

    Expected shape (as used by tools/llm_auto_lift.py):
      { "decompile_c": <str or dict with "result" key>, ... }
    """
    with open(path, 'r', encoding='utf-8') as f:
        data = json.load(f)
    dc = data.get('decompile_c', '')
    if isinstance(dc, str):
        try:
            dc = json.loads(dc)
        except (json.JSONDecodeError, ValueError):
            return dc
    if isinstance(dc, dict):
        return dc.get('result', '')
    return str(dc)


def load_from_c(path):
    with open(path, 'r', encoding='utf-8', errors='replace') as f:
        return f.read()


# ---------------------------------------------------------------------------
# Self-test
# ---------------------------------------------------------------------------

SELF_TESTS = [
    # (transform_name, input_fragment, expected_substring_in_output)
    (
        'undefined_types',
        'undefined4 foo;\nundefined1 bar;\nundefined8 baz;',
        'uint32_t foo',
    ),
    (
        'float10',
        'float10 fVar15;\nfVar15 = (float10)some_func(x);',
        'long double fVar15',
    ),
    (
        'CONCAT',
        'return CONCAT44(hiWord, loWord);',
        '/* AUTO: was CONCAT44 */',
    ),
    (
        'CONCAT22_nested',
        'x = CONCAT22(CONCAT11(a, b), loShort);',
        '/* AUTO: was CONCAT22 */',
    ),
    (
        'ZEXT',
        'uVar5 = ZEXT48(local_868) << 0x20;',
        '/* AUTO: was ZEXT48 */',
    ),
    (
        'SEXT',
        'iVar2 = SEXT14(cVar7);',
        '(int32_t)(int8_t)(cVar7)',
    ),
    (
        'SUB',
        'sVar2 = SUB42(iVar4, 0);',
        '/* AUTO: was SUB42 */',
    ),
    (
        'extraout',
        'uVar7 = extraout_var;\nuVar9 = extraout_var;',
        '/* TODO: register-escape from previous CALL — see disasm */',
    ),
    (
        'in_reg',
        'x = in_FS_OFFSET + 4;',
        '/* TODO: implicit register input — ABI hazard, see disasm */',
    ),
    (
        '_ftol2',
        'iVar3 = _ftol2(fVar2 * 3.0);',
        '(int)(fVar2 * 3.0) /* AUTO: was _ftol2() */',
    ),
    (
        '_chkstk_removed',
        '  _chkstk(0x800);\n  int x = 0;',
        '/* AUTO: removed _chkstk(',
    ),
    (
        '_allmul',
        'result = _allmul(a, b);',
        '(int64_t)(a) * (int64_t)(b)',
    ),
    (
        '_aullshr',
        'val = _aullshr(input, 3);',
        '(uint64_t)(input) >> (3)',
    ),
    (
        '_aulldiv',
        'q = _aulldiv(num, denom);',
        '(uint64_t)(num) / (uint64_t)(denom)',
    ),
    (
        '_aullrem',
        'r = _aullrem(num, denom);',
        '(uint64_t)(num) % (uint64_t)(denom)',
    ),
    (
        'SEH-wrap-paired',
        '__SEH_prolog(0x10, 0);\n  body();\n__SEH_epilog();',
        '__try',
    ),
    (
        'SEH-wrap-paired-except',
        '__SEH_prolog(0x10, 0);\n  body();\n__SEH_epilog();',
        '__except(EXCEPTION_EXECUTE_HANDLER)',
    ),
    (
        'SEH-flag-unpaired',
        '__SEH_prolog(0x10, 0); /* no epilog in this fragment */',
        'SEH-PROLOG:',
    ),
    (
        'undefined_return',
        'undefined FUN_001234(int x) {\n  return 0;\n}',
        'uint8_t FUN_001234',
    ),
]


def run_self_test():
    passed = 0
    failed = 0
    for name, inp, expected in SELF_TESTS:
        reset_counts()
        out = rewrite(inp)
        if expected in out:
            passed += 1
            print('  PASS  %s' % name)
        else:
            failed += 1
            print('  FAIL  %s' % name)
            print('        input:    %r' % inp[:80])
            print('        expected: %r' % expected)
            print('        got:      %r' % out[:120])
    print('\n%d/%d tests passed.' % (passed, passed + failed))
    reset_counts()
    return failed == 0


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description='Ghidra pseudocode first-draft rewriter (Stage 1+2).'
    )
    parser.add_argument('input', nargs='?',
                        help='Input .c file (or omit with --json/--stdin).')
    parser.add_argument('--json', metavar='FILE',
                        help='Load pseudocode from a context-cache JSON file.')
    parser.add_argument('--stdin', action='store_true',
                        help='Read pseudocode from stdin.')
    parser.add_argument('--self-test', action='store_true',
                        help='Run built-in self-tests and exit.')
    parser.add_argument('--stats', action='store_true',
                        help='Print transform statistics after rewriting.')
    parser.add_argument('-o', '--output', metavar='FILE',
                        help='Write output to FILE instead of stdout.')
    parser.add_argument('--no-annotate', action='store_true',
                        help='Skip hazard annotation pass (rewrite only).')
    args = parser.parse_args()

    if args.self_test:
        print('Running self-tests ...')
        ok = run_self_test()
        sys.exit(0 if ok else 1)

    # Load input
    if args.json:
        text = load_from_json(args.json)
    elif args.stdin:
        text = sys.stdin.read()
    elif args.input:
        if args.input.endswith('.json'):
            text = load_from_json(args.input)
        else:
            text = load_from_c(args.input)
    else:
        parser.print_help()
        sys.exit(1)

    reset_counts()
    out = rewrite(text)
    if not args.no_annotate:
        out = annotate_hazards(out)

    if args.output:
        with open(args.output, 'w', encoding='utf-8') as f:
            f.write(out)
    else:
        sys.stdout.write(out)

    if args.stats:
        sys.stderr.write('\n' + stats_report() + '\n')


if __name__ == '__main__':
    main()
