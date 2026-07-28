#!/usr/bin/env python3
"""Cross-check tag_block_get_element element-size literals for consistency.

`tag_block_get_element(block, index, element_size)` reads element `index` at
`block + index * element_size`.  The element size is a property of the tag
block, so every call site addressing the SAME block -- same tag group, same
offset within the tag -- must pass the SAME size.  A single wrong literal
silently reads mid-element for any index != 0, which is invisible to VC71 (one
immediate in a large function is aligned away by the LCS) and invisible at
runtime until content happens to use a non-zero index.

That is a real bug we shipped: `particle_systems.c` passed 0x6c for the 'obje'
particle_systems block at +0x140, where the original pushes 0x48 and both
`contrails.c` and `particles.c` independently use 0x48.  0x6c was the
`marker_buf` entry stride from a few lines below -- an unrelated number.  The
consequence: any attachment index != 0 produced a bogus string_id, so the
marker lookup failed or matched the wrong marker.

Detection groups call sites by (tag_group, block_offset) and reports any group
whose sites disagree on the size.  The tag group comes from the nearest
preceding `tag_get(0x<4cc>, ...)` in the same function, which is how these call
sites are actually written.  Sites whose group cannot be determined are grouped
under the offset alone and reported only at INFO.

Usage:
    check_tag_block_sizes.py                 # scan src/, human-readable
    check_tag_block_sizes.py --check         # exit 1 on non-baselined conflict
    check_tag_block_sizes.py --update-baseline
    check_tag_block_sizes.py --self-test
"""
import argparse
import json
import re
import sys
from collections import defaultdict
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent
SRC = REPO / "src"
BASELINE = Path(__file__).resolve().parent / "tag_block_size_baseline.json"

CALL = "tag_block_get_element"
# Trailing `+ 0xNNN` of the block expression -- the block's offset in the tag.
# Closing parens are allowed after it so casts wrap correctly:
# `(void *)(t + 0x140)` is as common in our lifts as a bare `t + 0x140`.
# Heuristic: this takes the LAST such offset, so a nested call like
# `f(g(p + 0x10), i, 0x48)` would attribute g's offset to the block. Those
# read as their own (group, offset) bucket and only matter if they collide.
OFF_RE = re.compile(r'\+\s*(0x[0-9a-fA-F]+)\s*\)*\s*$')
SIZE_RE = re.compile(r'^\s*(0x[0-9a-fA-F]+|\d+)\s*$')
# `x = (char *)tag_get(0x6f626a65, ...)` -- binds variable x to group 'obje'.
# Attribution is by VARIABLE, not by proximity: a function often resolves
# several tags, and taking the nearest preceding tag_get mis-attributed every
# one of the detector's first three findings (e.g. first_person_weapons.c:279
# indexes `mode_tag + 0xb8` but sits below an unrelated antr lookup).
TAG_ASSIGN_RE = re.compile(
    r'\b([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(?:\([^)]*\)\s*)*'
    r'tag_get\s*\(\s*(0x[0-9a-fA-F]{8})\b')
# Leading identifier of the block expression, after stripping casts/parens.
BASE_ID_RE = re.compile(r'([A-Za-z_][A-Za-z0-9_]*)\s*(?:\+|\[|$)')


def _fourcc(val):
    """0x6f626a65 -> 'obje' (big-endian char order, as written in source)."""
    try:
        b = int(val, 16).to_bytes(4, "big")
        s = b.decode("ascii")
        return s if s.isprintable() else val
    except Exception:
        return val


def _split_args(s):
    """Split a call's argument list on top-level commas."""
    args, depth, cur = [], 0, []
    for ch in s:
        if ch in "([":
            depth += 1
        elif ch in ")]":
            depth -= 1
        if ch == "," and depth == 0:
            args.append("".join(cur))
            cur = []
        else:
            cur.append(ch)
    if cur:
        args.append("".join(cur))
    return args


def _call_sites(text):
    """Yield (line_no, args_list) for each tag_block_get_element call.

    Brace-matched rather than line-based, so calls split across lines (most of
    them) are handled instead of silently skipped.
    """
    for m in re.finditer(re.escape(CALL) + r'\s*\(', text):
        i = m.end()
        depth, start = 1, i
        while i < len(text) and depth:
            if text[i] in "([":
                depth += 1
            elif text[i] in ")]":
                depth -= 1
            i += 1
        if depth:
            continue
        inner = text[start:i - 1]
        yield text.count("\n", 0, m.start()) + 1, _split_args(inner)


def _base_ident(block_expr):
    """Leading identifier of a block expression: `(void *)(obj_tag + 0x140)`
    -> `obj_tag`."""
    s = block_expr.strip()
    s = re.sub(r'\((?:void|char|unsigned char|int|short)\s*\*+\s*\)', '', s)
    s = s.lstrip("( \t*&")
    m = BASE_ID_RE.match(s)
    return m.group(1) if m else None


def _group_for(lines, line_no, block_expr, max_back=60):
    """Tag group of the VARIABLE the block expression is based on.

    Walks back for the most recent `<var> = ... tag_get(0x<4cc>, ...)` that
    assigns the same variable.  Returns None when the base identifier or its
    binding cannot be found -- an honest "unknown" rather than a guess, since a
    wrong group silently invents cross-tag conflicts.
    """
    base = _base_ident(block_expr)
    if not base:
        return None
    for k in range(min(line_no - 1, len(lines) - 1), -1, -1):
        if line_no - 1 - k > max_back:
            break
        m = TAG_ASSIGN_RE.search(lines[k])
        if m and m.group(1) == base:
            return _fourcc(m.group(2))
    return None


def scan(root=SRC):
    """-> {(group_or_None, offset): [(path, line, size_literal), ...]}"""
    groups = defaultdict(list)
    for path in sorted(root.rglob("*.c")):
        text = path.read_text(encoding="utf-8", errors="replace")
        if CALL not in text:
            continue
        lines = text.splitlines()
        for line_no, args in _call_sites(text):
            if len(args) != 3:
                continue
            om = OFF_RE.search(args[0].strip())
            sm = SIZE_RE.match(args[2])
            if not om or not sm:
                continue
            off = int(om.group(1), 16)
            size = int(sm.group(1), 16) if sm.group(1).startswith("0x") \
                else int(sm.group(1))
            try:
                rel = str(path.relative_to(REPO))
            except ValueError:
                rel = str(path)   # scanning outside the repo (self-test)
            grp = _group_for(lines, line_no, args[0])
            groups[(grp, off)].append((rel, line_no, size))
    return groups


def conflicts(groups):
    """Groups whose sites disagree on element size, worst (most sites) first.

    Only buckets with a KNOWN tag group are eligible.  An unresolved group
    means the offset alone identifies the bucket, and different tags reuse the
    same offsets freely -- `?+0x3c` merges a structure-BSP block (0xc), a
    sound-tag block (0x7c) and an actor block (0x14), which are simply
    different blocks, not a disagreement.  Comparing those produces pure noise
    and would drown the real finding.
    """
    out = []
    for (grp, off), sites in groups.items():
        if grp is None:
            continue
        sizes = {s for _, _, s in sites}
        if len(sizes) > 1:
            out.append((grp, off, sizes, sorted(sites)))
    out.sort(key=lambda c: (-len(c[3]), str(c[0]), c[1]))
    return out


def _key(grp, off):
    return f"{grp or '?'}+{off:#x}"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true",
                    help="exit 1 on any conflict not in the baseline")
    ap.add_argument("--update-baseline", action="store_true")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()

    if args.self_test:
        return self_test()

    groups = scan()
    confl = conflicts(groups)
    total = sum(len(v) for v in groups.values())
    print(f"{CALL}: {total} resolvable call site(s), "
          f"{len(groups)} distinct (group, offset) block(s)")

    base = {}
    if BASELINE.exists():
        base = json.loads(BASELINE.read_text(encoding="utf-8")).get("known", {})

    new = []
    for grp, off, sizes, sites in confl:
        k = _key(grp, off)
        seen = sorted(hex(s) for s in sizes)
        known = base.get(k)
        status = "BASELINED" if known == seen else "CONFLICT"
        if status == "CONFLICT":
            new.append((k, seen))
        print(f"\n  [{status}] block {k}: sizes {', '.join(seen)}")
        for path, line, size in sites:
            print(f"      {path}:{line}  size={size:#x}")

    if args.update_baseline:
        payload = {
            "_comment": "Known tag_block_get_element element-size disagreements. "
                        "A block reached at the same (tag_group, offset) should "
                        "have ONE element size; entries here are unresolved or "
                        "legitimately-distinct blocks that share an offset. "
                        "Never baseline a conflict you have not checked against "
                        "the original disassembly.",
            "known": {k: v for k, v in sorted(
                {_key(g, o): sorted(hex(s) for s in sz)
                 for g, o, sz, _ in confl}.items())},
        }
        BASELINE.write_text(json.dumps(payload, indent=2) + "\n",
                            encoding="utf-8")
        print(f"\nbaseline written: {len(payload['known'])} entry(ies)")
        return 0

    if args.check:
        if new:
            print(f"\nERROR: {len(new)} non-baselined element-size conflict(s).")
            print("A tag block has one element size; verify against the "
                  "original `PUSH <size>` at the call site before baselining.")
            return 1
        if confl:
            print(f"\n--check OK: {len(confl)} conflict(s), all baselined.")
        else:
            print("\n--check OK: no element-size conflicts.")
    return 0


def self_test():
    """Pins the detector on the real bug it was written for."""
    import tempfile
    ok = True

    # 1. The particle_systems bug shape: same (group, offset), two sizes.
    bug = '''
void a(void) {
  char *t = (char *)tag_get(0x6f626a65, idx);
  x = tag_block_get_element((void *)(t + 0x140), i, 0x6c);
}
void b(void) {
  char *t = (char *)tag_get(0x6f626a65, idx);
  y = tag_block_get_element(t + 0x140, j, 0x48);
}
'''
    with tempfile.TemporaryDirectory() as d:
        p = Path(d) / "x.c"
        p.write_text(bug, encoding="utf-8")
        c = conflicts(scan(Path(d)))
        if len(c) != 1 or c[0][0] != "obje" or c[0][1] != 0x140 \
                or c[0][2] != {0x6C, 0x48}:
            print(f"  FAIL detect conflict: got {c}")
            ok = False
        else:
            print("  PASS detects the obje+0x140 0x6c-vs-0x48 conflict")

    # 2. Agreement must NOT be reported.
    good = bug.replace("0x6c", "0x48")
    with tempfile.TemporaryDirectory() as d:
        p = Path(d) / "x.c"
        p.write_text(good, encoding="utf-8")
        if conflicts(scan(Path(d))):
            print("  FAIL agreeing sizes reported as a conflict")
            ok = False
        else:
            print("  PASS agreeing sizes are silent")

    # 3. Same offset under DIFFERENT tag groups is not a conflict.
    diff_group = bug.replace("tag_get(0x6f626a65, idx);\n  y", "z", 1)
    diff_group = '''
void a(void) {
  char *t = (char *)tag_get(0x6f626a65, idx);
  x = tag_block_get_element((void *)(t + 0x140), i, 0x6c);
}
void b(void) {
  char *t = (char *)tag_get(0x77656170, idx);
  y = tag_block_get_element(t + 0x140, j, 0x48);
}
'''
    with tempfile.TemporaryDirectory() as d:
        p = Path(d) / "x.c"
        p.write_text(diff_group, encoding="utf-8")
        if conflicts(scan(Path(d))):
            print("  FAIL different tag groups merged on shared offset")
            ok = False
        else:
            print("  PASS different tag groups are kept apart")

    # 4. Multi-line calls must be parsed (most real sites are wrapped).
    multi = '''
void a(void) {
  char *t = (char *)tag_get(0x6f626a65, idx);
  x = tag_block_get_element(
    (void *)(t + 0x140),
    i,
    0x6c);
}
void b(void) {
  char *t = (char *)tag_get(0x6f626a65, idx);
  y = tag_block_get_element(t + 0x140, j, 0x48);
}
'''
    with tempfile.TemporaryDirectory() as d:
        p = Path(d) / "x.c"
        p.write_text(multi, encoding="utf-8")
        if len(conflicts(scan(Path(d)))) != 1:
            print("  FAIL multi-line call site not parsed")
            ok = False
        else:
            print("  PASS multi-line call sites are parsed")

    print("\nself-test: " + ("OK" if ok else "FAILED"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
