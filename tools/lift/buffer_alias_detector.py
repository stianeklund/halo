#!/usr/bin/env python3
"""Buffer-alias detector for Ghidra pseudocode (Stage 3 of m2c-style draft tool).

Ghidra names every stack offset as an independent ``local_XX`` variable even
when the offset falls inside a local buffer declared elsewhere in the same
function.  After a call that takes a buffer pointer, subsequent reads from
those offsets are misattributed to a *different* variable — causing wrong
field reads at runtime (see CLAUDE.md "Buffer-alias confusion", hazard #5).

Algorithm:
  1. Parse all local variable declarations.  Track arrays (``local_XX[N]``)
     and scalar locals separately.
  2. For each array buffer of size >= MIN_BUFFER_SIZE, compute its stack
     range: [EBP-base, EBP-base+N)  i.e.  [offset_hex, offset_hex - N].
     A local_YY (EBP-YY) is INSIDE the buffer when:
         base - N  <  YY  <=  base          (strict upper, inclusive lower end)
     equivalently: YY >= (base - N + 1) and YY <= base, with YY != base
     (the base offset itself is the buffer start, not a field inside it).
  3. Record every function call in the body.  Note which buffer names appear
     as arguments to each call.
  4. On every line that references a scalar local_YY that is inside a known
     buffer's range, inject an inline comment.  Calls that passed the buffer
     pointer mark the subsequent suspicious reads as HIGH-RISK.

Arithmetic worked example (from CLAUDE.md):
  Buffer:  local_8c[0xac]   → base=0x8c, size=0xac
  Range:   EBP-0x8c  to  EBP-(0x8c-0xac) = EBP+0x20
           i.e. local_YY is inside when  YY < 0x8c  AND  YY > (0x8c-0xac) = -0x20
           Since offsets are unsigned positive hex: YY < 0x8C and (int)YY > -0x20
           But all Ghidra locals have YY > 0 for below-frame offsets, so the
           buffer that crosses the frame pointer is just caught by the
           ``top_offset  = base - size`` condition.  When top_offset <= 0 we
           clamp to 0 for the containment check.
  local_44 → 0x44 < 0x8c  ✓, and 0x44 > max(0, 0x8c-0xac) = 0  ✓  → INSIDE
  Offset inside buffer: base - YY = 0x8c - 0x44 = 0x48.

Usage:
  python3 tools/lift/buffer_alias_detector.py  <file.c>
  python3 tools/lift/buffer_alias_detector.py  --json <FUN_XXX.json>
  python3 tools/lift/buffer_alias_detector.py  --stdin
  python3 tools/lift/buffer_alias_detector.py  --self-test
"""

import json
import re
import sys
import textwrap
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

# Buffers smaller than this (bytes) are too small to plausibly contain other
# named locals that Ghidra would give separate local_XX names to.
MIN_BUFFER_SIZE = 4

# ---------------------------------------------------------------------------
# Regex patterns
# ---------------------------------------------------------------------------

# Matches:   undefined1 local_8c [0xac];
#            undefined4 local_78[32];
#            char acStack_140f0[82060];
#            int local_1244 [1024];
#            float local_b4 [15];
# Group 1: type keyword(s), Group 2: var name, Group 3: size (decimal or hex)
ARRAY_DECL_RE = re.compile(
    r'^\s*'
    r'(?:unsigned\s+|signed\s+)?'           # optional signedness
    r'(?:undefined\d*|char|short|int|long|float|double|byte|uint\w*|int\w*|ushort\w*)\s+'
    r'((?:[a-zA-Z_]\w*Stack_|local_)[0-9a-fA-F]+)'  # var name (local_ or *Stack_)
    r'\s*\[\s*(0x[0-9a-fA-F]+|\d+)\s*\]\s*;',
    re.IGNORECASE,
)

# Scalar local — undefined4 local_44;  int local_3c;  float *local_40;
#               undefined *local_40;  undefined4 *puVar;  (skip non-local names)
#               Also matches Ghidra's acStack_XXXX / puStack_XXXX naming.
# Group 1: var name (local_XX or *Stack_XX form)
SCALAR_DECL_RE = re.compile(
    r'^\s*'
    r'(?:unsigned\s+|signed\s+)?'
    r'(?:undefined\d*|char|short|int|long|float|double|byte|uint\w*|int\w*|ushort\w*)'
    r'(?:\s*\*+\s*|\s+)'           # pointer stars OR whitespace between type and name
    r'((?:[a-zA-Z_]\w*Stack_|local_)[0-9a-fA-F]+)\s*;',
    re.IGNORECASE,
)

# Any reference to local_XX or *Stack_XX in a non-declaration line.
# We look for the name not followed by '[' (to skip array-base uses inside
# declarations) but we DO want array indexing uses: local_xx[0x34].
LOCAL_REF_RE = re.compile(
    r'\b(?:local_([0-9a-fA-F]+)|(?:[a-zA-Z_]\w*Stack_)([0-9a-fA-F]+))\b'
)

# Function call on the line — very broad: word( or FUN_xxxxx(
CALL_RE = re.compile(r'\b(\w+)\s*\(')

# ---------------------------------------------------------------------------
# Data structures
# ---------------------------------------------------------------------------

@dataclass
class BufferInfo:
    name: str        # e.g. "local_8c"
    base: int        # EBP displacement (positive, e.g. 0x8c)
    size: int        # number of bytes
    decl_line: int   # 1-based line number of declaration

    @property
    def top(self) -> int:
        """Lower bound of the occupied stack region (may be negative if buffer
        crosses the frame pointer — we clamp to 0 for containment tests)."""
        return self.base - self.size

    def contains(self, local_offset: int) -> bool:
        """Return True if EBP-local_offset is strictly inside this buffer.

        The buffer occupies EBP-base .. EBP-(base-size).
        A local_YY (at EBP-YY) is inside when:
            top < YY <= base   (exclusive top, inclusive base)
        We use ``YY < base`` (not <=) so the buffer name itself is not flagged.
        """
        return local_offset < self.base and local_offset > max(0, self.top)

    def offset_of(self, local_offset: int) -> int:
        """Byte offset of local_YY from the start (low address) of the buffer."""
        return self.base - local_offset


@dataclass
class AliasHit:
    line_no: int         # 1-based
    local_name: str      # e.g. "local_44"
    local_offset: int    # 0x44
    buffer: BufferInfo
    post_call: bool      # True if a qualifying call preceded this line


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _extract_local_offset(name: str) -> Optional[int]:
    """Parse the hex offset out of a local_XX name.  Returns None if name
    doesn't match the expected pattern.
    Handles both Ghidra naming conventions:
      local_8c     → 0x8c  (EBP-relative)
      acStack_8c   → 0x8c  (EBP-relative, stack-frame prefix variant)
    """
    m = re.fullmatch(r'(?:[a-zA-Z_]\w*Stack_|local_)([0-9a-fA-F]+)', name, re.IGNORECASE)
    if m:
        return int(m.group(1), 16)
    return None


def _parse_size(s: str) -> int:
    if s.startswith('0x') or s.startswith('0X'):
        return int(s, 16)
    return int(s)


def _is_declaration_line(line: str) -> bool:
    """True if the line is a local variable declaration (array or scalar)."""
    return bool(ARRAY_DECL_RE.match(line) or SCALAR_DECL_RE.match(line))


def _line_has_call(line: str) -> bool:
    return bool(CALL_RE.search(line))


def _buffer_names_in_call(line: str, buffers: Dict[str, BufferInfo]) -> List[str]:
    """Return names of buffers that appear as arguments in a call on this line."""
    found = []
    if not _line_has_call(line):
        return found
    for name in buffers:
        if re.search(r'\b' + re.escape(name) + r'\b', line):
            found.append(name)
    return found


# ---------------------------------------------------------------------------
# Core analysis
# ---------------------------------------------------------------------------

def analyze(source: str) -> Tuple[List[AliasHit], str]:
    """Analyse raw Ghidra pseudocode and return (hits, annotated_source).

    The annotated source has inline comments injected immediately after the
    suspect local reference on each flagged line.
    """
    lines = source.splitlines(keepends=True)

    # -----------------------------------------------------------------------
    # Pass 1: collect buffer declarations and scalar locals
    # -----------------------------------------------------------------------
    buffers: Dict[str, BufferInfo] = {}   # name -> BufferInfo
    scalar_locals: Dict[str, int] = {}     # name -> EBP offset

    for lineno, line in enumerate(lines, 1):
        m = ARRAY_DECL_RE.match(line)
        if m:
            name = m.group(1)
            size = _parse_size(m.group(2))
            if size >= MIN_BUFFER_SIZE:
                offset = _extract_local_offset(name)
                if offset is not None:
                    buffers[name] = BufferInfo(
                        name=name,
                        base=offset,
                        size=size,
                        decl_line=lineno,
                    )
            continue

        m = SCALAR_DECL_RE.match(line)
        if m:
            name = m.group(1)
            offset = _extract_local_offset(name)
            if offset is not None:
                scalar_locals[name] = offset

    if not buffers:
        # Nothing to flag — return source unchanged
        return [], source

    # -----------------------------------------------------------------------
    # Pass 2: walk body lines, track which buffers have been passed to calls,
    # and flag suspect scalar local reads.
    # -----------------------------------------------------------------------
    hits: List[AliasHit] = []

    # Set of buffer names that have been observed as arguments in a prior call.
    # This is the "post-call high-risk" signal.
    buffers_passed_to_call: set = set()

    for lineno, line in enumerate(lines, 1):
        stripped = line.lstrip()
        # Skip comment lines and blank lines
        if stripped.startswith('//') or stripped.startswith('/*') or \
                stripped.startswith('*') or not stripped:
            continue

        # Skip declaration lines
        if _is_declaration_line(line):
            continue

        # Track which buffers are passed to calls on this line
        newly_passed = _buffer_names_in_call(line, buffers)
        # We record the call site BEFORE flagging reads on the SAME line —
        # the call passes the buffer; reads on subsequent lines are the risk.
        # But if the same line both passes AND reads, that's also suspicious.

        # Find every local_XX / *Stack_XX reference on the line
        for ref_m in LOCAL_REF_RE.finditer(line):
            # Group 1: local_XX hex digits; Group 2: *Stack_XX hex digits
            hex_digits = ref_m.group(1) or ref_m.group(2)
            ref_name = ref_m.group(0)   # full matched name (e.g. "local_44" or "acStack_44")
            ref_offset = int(hex_digits, 16)

            # Skip if this IS a buffer's own name (the buffer being passed as
            # an argument is expected; the alias risk is in OTHER locals)
            if ref_name in buffers:
                continue

            # Check whether this offset falls inside any known buffer
            for buf_name, buf in buffers.items():
                if buf.contains(ref_offset):
                    post_call = buf_name in buffers_passed_to_call
                    hits.append(AliasHit(
                        line_no=lineno,
                        local_name=ref_name,
                        local_offset=ref_offset,
                        buffer=buf,
                        post_call=post_call,
                    ))
                    # Only report the first matching buffer per reference per line
                    break

        # Update the set of buffers seen in call arguments AFTER processing reads
        buffers_passed_to_call.update(newly_passed)

    # -----------------------------------------------------------------------
    # Pass 3: inject annotations into a copy of the source
    # -----------------------------------------------------------------------
    if not hits:
        return [], source

    # Group hits by line number
    hits_by_line: Dict[int, List[AliasHit]] = {}
    for h in hits:
        hits_by_line.setdefault(h.line_no, []).append(h)

    annotated_lines = list(lines)  # copy

    for lineno, line_hits in hits_by_line.items():
        original = annotated_lines[lineno - 1]
        # Strip trailing newline for manipulation, reattach at end
        eol = ''
        if original.endswith('\r\n'):
            eol = '\r\n'
            original = original[:-2]
        elif original.endswith('\n'):
            eol = '\n'
            original = original[:-1]
        elif original.endswith('\r'):
            eol = '\r'
            original = original[:-1]

        # Build one combined comment for all hits on this line
        parts = []
        for h in line_hits:
            buf_offset = h.buffer.offset_of(h.local_offset)
            risk_tag = 'HIGH-RISK post-call' if h.post_call else 'verify'
            parts.append(
                f'{h.local_name} may be {h.buffer.name}+0x{buf_offset:x}'
                f' (inside {h.buffer.name}[0x{h.buffer.size:x}]) — {risk_tag}'
            )
        comment = '  /* AUTO: ' + '; '.join(parts) + ' */'
        annotated_lines[lineno - 1] = original + comment + eol

    return hits, ''.join(annotated_lines)


# ---------------------------------------------------------------------------
# Input ingestion
# ---------------------------------------------------------------------------

def _load_from_json(path: str) -> str:
    """Extract the decompile_c result field from a context cache JSON file."""
    with open(path, 'r', encoding='utf-8', errors='replace') as f:
        data = json.load(f)

    # Context cache format: { "decompile_c": "{\"result\": \"...\"}" }
    dc = data.get('decompile_c')
    if dc is None:
        raise ValueError(f"No 'decompile_c' key in {path}")

    if isinstance(dc, str):
        inner = json.loads(dc)
        source = inner.get('result', '')
    elif isinstance(dc, dict):
        source = dc.get('result', '')
    else:
        raise ValueError(f"Unexpected decompile_c type {type(dc)} in {path}")

    # Normalise Windows line endings
    return source.replace('\r\n', '\n')


def _load_from_file(path: str) -> str:
    with open(path, 'r', encoding='utf-8', errors='replace') as f:
        return f.read().replace('\r\n', '\n')


def _load_from_stdin() -> str:
    return sys.stdin.read().replace('\r\n', '\n')


# ---------------------------------------------------------------------------
# Reporting
# ---------------------------------------------------------------------------

def _report(hits: List[AliasHit], source_name: str) -> None:
    if not hits:
        print(f"[buffer_alias] {source_name}: no aliases detected")
        return

    high = [h for h in hits if h.post_call]
    low  = [h for h in hits if not h.post_call]

    print(f"[buffer_alias] {source_name}: {len(hits)} suspect alias(es) "
          f"({len(high)} HIGH-RISK post-call, {len(low)} low-risk)")

    for h in high:
        buf_offset = h.buffer.offset_of(h.local_offset)
        print(f"  HIGH-RISK  line {h.line_no:4d}: {h.local_name} "
              f"= {h.buffer.name}+0x{buf_offset:x} "
              f"(inside {h.buffer.name}[0x{h.buffer.size:x}])")

    for h in low:
        buf_offset = h.buffer.offset_of(h.local_offset)
        print(f"  low-risk   line {h.line_no:4d}: {h.local_name} "
              f"= {h.buffer.name}+0x{buf_offset:x} "
              f"(inside {h.buffer.name}[0x{h.buffer.size:x}])")


# ---------------------------------------------------------------------------
# Self-test
# ---------------------------------------------------------------------------

SELF_TEST_CASES = [
    # ------------------------------------------------------------------
    # Case 1: Clean function — non-overlapping locals, no annotations
    # ------------------------------------------------------------------
    (
        "case1_clean",
        """\
void FUN_clean(int param_1) {
  int local_8;
  int local_4;
  undefined1 local_c [4];

  local_8 = param_1 + 1;
  local_4 = local_8 * 2;
  local_c[0] = 0;
}
""",
        # expected: 0 hits
        0,
        # expected high-risk count
        0,
    ),

    # ------------------------------------------------------------------
    # Case 2: Buffer with overlapping local — post-call HIGH-RISK case
    # Buffer: local_8c[0xac]  covers EBP-0x8c..EBP-(0x8c-0xac) = EBP+0x20
    # local_44 is at EBP-0x44, inside the buffer at offset 0x48.
    # local_a0 is at EBP-0xa0, OUTSIDE the buffer (above the base).
    # The call FUN_00137d20(damage_params, ...) passes local_8c,
    # so local_44 read AFTER the call is HIGH-RISK.
    # ------------------------------------------------------------------
    (
        "case2_post_call_high_risk",
        """\
void FUN_000f90d0(void) {
  undefined1 local_8c [0xac];
  undefined4 local_44;
  int local_a0;

  FUN_00137d20(local_8c, 0, 0);
  local_a0 = (int)local_44 + 1;
}
""",
        # expected: 1 hit (local_44 inside local_8c[0xac]; local_a0 is above base so outside)
        1,
        # expected high-risk: 1 (post-call)
        1,
    ),

    # ------------------------------------------------------------------
    # Case 3: Buffer with overlapping local — NO call passes the buffer
    # (low-risk case: annotate but don't escalate)
    # Buffer: local_8c[0xac]  covers EBP-0x8c..EBP+0x20
    # local_44 at EBP-0x44 = buffer+0x48 → inside, but no call uses local_8c
    # ------------------------------------------------------------------
    (
        "case3_low_risk_no_call",
        """\
void FUN_low_risk(void) {
  undefined1 local_8c [0xac];
  undefined4 local_44;

  local_8c[0] = 0;
  local_44 = 99;
}
""",
        # expected: 1 hit
        1,
        # expected high-risk: 0 (no qualifying call)
        0,
    ),
]


def _run_self_tests() -> int:
    """Run built-in self-tests.  Returns 0 on pass, 1 on failure."""
    failures = 0
    for name, source, expected_hits, expected_high in SELF_TEST_CASES:
        hits, annotated = analyze(source)
        high = sum(1 for h in hits if h.post_call)
        ok_hits = (len(hits) == expected_hits)
        ok_high = (high == expected_high)
        status = "PASS" if (ok_hits and ok_high) else "FAIL"
        if status == "FAIL":
            failures += 1
        print(f"  [{status}] {name}: "
              f"{len(hits)} hit(s) (want {expected_hits}), "
              f"{high} high-risk (want {expected_high})")

        if status == "FAIL":
            # Show what was detected for debugging
            _report(hits, name)
            # Show annotated source for context
            print("  --- annotated source ---")
            for i, line in enumerate(annotated.splitlines(), 1):
                print(f"  {i:3}: {line}")

        # Verify annotation injection for the two non-zero cases
        if expected_hits > 0:
            has_auto_comment = '/* AUTO:' in annotated
            if not has_auto_comment:
                print(f"  [FAIL] {name}: expected AUTO comment in annotated output")
                failures += 1

        if expected_hits == 0:
            if annotated != source:
                print(f"  [FAIL] {name}: source should be unchanged for clean function")
                failures += 1

    return 0 if failures == 0 else 1


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> int:
    import argparse

    parser = argparse.ArgumentParser(
        description="Detect Ghidra buffer-alias hazards in pseudocode",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=textwrap.dedent("""\
            Examples:
              rtk python3 tools/lift/buffer_alias_detector.py src/halo/foo.c
              rtk python3 tools/lift/buffer_alias_detector.py --json artifacts/auto_lift/context_cache/FUN_000f90d0.json
              rtk python3 tools/lift/buffer_alias_detector.py --stdin < /tmp/pseudo.c
              rtk python3 tools/lift/buffer_alias_detector.py --self-test
        """),
    )
    parser.add_argument('input', nargs='?', help='C source file to analyse')
    parser.add_argument('--json', metavar='FILE',
                        help='Ghidra context cache JSON file')
    parser.add_argument('--stdin', action='store_true',
                        help='Read pseudocode from stdin')
    parser.add_argument('--self-test', action='store_true',
                        help='Run built-in self-tests and exit')
    parser.add_argument('--out', metavar='FILE',
                        help='Write annotated output to FILE instead of stdout')
    parser.add_argument('--quiet', action='store_true',
                        help='Suppress hit report; only write annotated source')
    parser.add_argument('--json-output', metavar='FILE',
                        help='Write machine-readable JSON hit list to FILE '
                             '(for pipeline consumption); suppresses annotated source output')

    args = parser.parse_args()

    if args.self_test:
        print("Running self-tests…")
        rc = _run_self_tests()
        if rc == 0:
            print("All self-tests passed.")
        else:
            print("One or more self-tests FAILED.", file=sys.stderr)
        return rc

    if args.json:
        source = _load_from_json(args.json)
        source_name = args.json
    elif args.stdin:
        source = _load_from_stdin()
        source_name = '<stdin>'
    elif args.input:
        source = _load_from_file(args.input)
        source_name = args.input
    else:
        parser.print_help(sys.stderr)
        return 2

    hits, annotated = analyze(source)

    # Machine-readable JSON output for pipeline consumption
    if args.json_output:
        hit_records = [
            {
                "line": h.line_no,
                "local": h.local_name,
                "local_offset": h.local_offset,
                "buffer": h.buffer.name,
                "buffer_base": h.buffer.base,
                "buffer_size": h.buffer.size,
                "field_offset": h.buffer.offset_of(h.local_offset),
                "post_call": h.post_call,
            }
            for h in hits
        ]
        json_path = args.json_output
        with open(json_path, 'w', encoding='utf-8') as f:
            json.dump(hit_records, f, indent=2)
        high_risk = sum(1 for h in hits if h.post_call)
        print(f"[buffer_alias] {len(hits)} hit(s), {high_risk} HIGH-RISK → {json_path}")
        if any(h.post_call for h in hits):
            return 1
        return 0

    if not args.quiet:
        _report(hits, source_name)

    if args.out:
        with open(args.out, 'w', encoding='utf-8') as f:
            f.write(annotated)
        print(f"[buffer_alias] annotated output written to {args.out}")
    else:
        sys.stdout.write(annotated)

    # Exit 1 if any HIGH-RISK hits found (for pipeline gate usage)
    if any(h.post_call for h in hits):
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
