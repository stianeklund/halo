#!/usr/bin/env python3
"""Detect inline asm blocks that reference symbols with @<reg> thunks.

If a function has a register-arg thunk in kb.json, calling it via inline
asm is almost certainly wrong — the thunk overwrites the registers the asm
set up.  Call the function directly (the thunk bridges the ABI) or use the
__xbe raw pointer if you genuinely need manual control.
"""

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
KB_PATH = ROOT / "kb.json"
SRC_DIR = ROOT / "src"

REG_ANNO_RE = re.compile(r"@<\w+>")
ASM_BLOCK_RE = re.compile(
    r"__asm__\s*__volatile__\s*\(", re.MULTILINE
)


def get_thunked_symbols():
    """Return set of symbol names that have @<reg> annotations."""
    with open(KB_PATH) as f:
        kb = json.load(f)

    symbols = set()
    for obj in kb.get("objects", []):
        for func in obj.get("functions", []):
            decl = func.get("decl", "")
            if REG_ANNO_RE.search(decl):
                match = re.match(r"[\w\s*]+?\b(\w+)\s*\(", decl)
                if match:
                    symbols.add(match.group(1))
    return symbols


def find_asm_blocks(source_text):
    """Yield (line_number, block_text) for each inline asm block."""
    for m in ASM_BLOCK_RE.finditer(source_text):
        start = m.start()
        depth = 0
        i = m.end() - 1
        while i < len(source_text):
            if source_text[i] == "(":
                depth += 1
            elif source_text[i] == ")":
                depth -= 1
                if depth == 0:
                    break
            i += 1
        block = source_text[m.start() : i + 1]
        line_no = source_text[:start].count("\n") + 1
        yield line_no, block


def check_file(path, thunked_symbols):
    """Return list of (path, line, symbol) violations."""
    text = path.read_text(errors="replace")
    violations = []
    for line_no, block in find_asm_blocks(text):
        for sym in thunked_symbols:
            if re.search(r"\b" + re.escape(sym) + r"\b", block):
                violations.append((path, line_no, sym))
    return violations


def main():
    thunked = get_thunked_symbols()
    if not thunked:
        return 0

    violations = []
    for src_file in SRC_DIR.rglob("*.c"):
        violations.extend(check_file(src_file, thunked))

    if violations:
        print("ERROR: inline asm references thunked @<reg> symbols:", file=sys.stderr)
        print("", file=sys.stderr)
        for path, line, sym in violations:
            rel = path.relative_to(ROOT)
            print(
                f"  {rel}:{line}: '{sym}' has a register-arg thunk — "
                f"call it directly or use {sym}__xbe",
                file=sys.stderr,
            )
        print("", file=sys.stderr)
        print(
            "The thunk bridges the calling convention automatically. "
            "Inline asm that also sets registers will conflict.",
            file=sys.stderr,
        )
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
