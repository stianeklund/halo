#!/usr/bin/env python3
"""strip_dup_typedefs.py — remove redundant typedefs from a permuter candidate.

The permuter (permuter.py) parses base.c with pycparser, which requires the
type definitions to be present (it has no access to types.h). It then
re-serializes the AST to the candidate source — but pycparser drops all
preprocessor directives, so the `#ifndef TYPES_H` guard that protected those
typedefs in base.c is GONE. When compile.sh then force-includes types.h via
`/FI`, every typedef the candidate carries that types.h also defines triggers
`error C2371: redefinition` and the candidate fails to compile (0 iterations,
vacuous "no improvements").

This pass removes exactly those top-level typedefs from the candidate whose name
is already defined in types.h, so types.h's definitions win. Typedefs that
types.h does NOT define (e.g. vector4_t, uint64_t) are kept, since `/FI` won't
supply them. Keeping the candidate in the SAME `/FI` environment as the real
build preserves codegen fidelity, which is what the permuter's score depends on.

Usage:
    strip_dup_typedefs.py <candidate.c> <types.h> <output.c>

Fails safe: any typedef whose name cannot be parsed is KEPT (worst case a
visible C2371, never a silent wrong-compile). Exit 0 on success.
"""

import re
import sys


def typedef_names(types_h_path):
    """Collect typedef'd identifiers defined in types.h."""
    try:
        with open(types_h_path, "r", errors="replace") as f:
            txt = f.read()
    except OSError as e:
        print(f"[strip_dup_typedefs] cannot read {types_h_path}: {e}", file=sys.stderr)
        return set()
    names = set()
    # struct/union/enum typedef closers:  } NAME ;
    for m in re.finditer(r"\}\s*(\w+)\s*;", txt):
        names.add(m.group(1))
    # simple typedefs:  typedef <stuff-without-braces> NAME ;
    for m in re.finditer(r"typedef\s+[^;{}]+?\b(\w+)\s*;", txt):
        names.add(m.group(1))
    return names


def strip(src, names):
    """Remove top-level `typedef ...;` statements whose name is in `names`."""
    out = []
    i = 0
    n = len(src)
    while i < n:
        # A top-level typedef starts at a statement boundary.
        at_boundary = i == 0 or src[i - 1] in "\n;}\t "
        if at_boundary and src.startswith("typedef", i) and (
            i + 7 >= n or not (src[i + 7].isalnum() or src[i + 7] == "_")
        ):
            # Consume to the matching ';' at brace depth 0.
            depth = 0
            j = i
            while j < n:
                c = src[j]
                if c == "{":
                    depth += 1
                elif c == "}":
                    depth -= 1
                elif c == ";" and depth == 0:
                    break
                j += 1
            stmt = src[i : j + 1]
            mname = re.search(r"(\w+)\s*;\s*$", stmt)
            name = mname.group(1) if mname else None
            if name is not None and name in names:
                # Drop this typedef (and one trailing newline for tidiness).
                i = j + 1
                if i < n and src[i] == "\n":
                    i += 1
                continue
            out.append(stmt)
            i = j + 1
            continue
        out.append(src[i])
        i += 1
    return "".join(out)


def main():
    if len(sys.argv) != 4:
        print("usage: strip_dup_typedefs.py <candidate.c> <types.h> <output.c>",
              file=sys.stderr)
        return 2
    cand_path, types_path, out_path = sys.argv[1], sys.argv[2], sys.argv[3]
    try:
        with open(cand_path, "r", errors="replace") as f:
            src = f.read()
    except OSError as e:
        print(f"[strip_dup_typedefs] cannot read {cand_path}: {e}", file=sys.stderr)
        return 1
    names = typedef_names(types_path)
    result = strip(src, names) if names else src
    try:
        with open(out_path, "w") as f:
            f.write(result)
    except OSError as e:
        print(f"[strip_dup_typedefs] cannot write {out_path}: {e}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
