#!/usr/bin/env python3
"""audit_candidate.py -- semantic audit of a decomp-permuter candidate.

WHY THIS EXISTS
---------------
The permuter optimizes an instruction-similarity metric.  Nothing in the search
loop knows what the C means, so a candidate that changes behaviour is perfectly
free to score higher than the faithful base -- and in practice does.

Measured on units.c (2026-08-13), three targets, three best-ranked candidates:

  * unit_impact_melee_damage    +4.6pp claimed -- semantically safe
  * unit_get_seat_enter_position +1.5pp        -- DROPS a def of `mode_index`
                                                  that is still read afterwards,
                                                  so the animation block is
                                                  indexed with the loop counter
  * unit_set_seat_state          +0.7pp        -- reads `new_var2` on two paths
                                                  that cannot have defined it

Two of three were broken.  Neither is caught by the campaign's own gates.  The
`unit_get_seat_enter_position` candidate was applied as a probe and measured:

  VC71 official match  92.4% -> 93.9%   (+1.5pp -- the gate REWARDS the bug)
  VC71 opnd            58.0% -> 67.1%   (+9.1pp)
  equivalence          60/60 seeds pass, 30.7% coverage, "moderate"
  stub-arg diff        420 calls, 0 mismatches
  clang -Wall -Werror  clean

Every automated gate in the campaign passed it.  The buggy call is behind a
`crt_stricmp` match inside a tag-block loop, which zero-fill and z3-branch seeds
never reach, so the equivalence lane never executes the changed code at all.
Only reading the diff catches it.

This script mechanises the half that is mechanisable.

WHAT IT CHECKS
--------------
1. LOST_DEF (ERROR) -- a local that loses an assignment relative to base while
   still being read.  This is the `mode_index` class: the permuter inlines a
   load into the comparison that consumed it and silently leaves every later
   read observing a stale value.

2. UNDEF_PATH (ERROR) -- a local whose every assignment sits inside a block
   that unconditionally leaves the function (return/break/continue/goto), yet
   which is read after that block.  This is the `new_var2` class: a guaranteed
   uninitialized read.  clang's -Wsometimes-uninitialized catches some of these
   and -Wconditional-uninitialized catches more, but only once the candidate has
   been merged into the real TU; this runs on the raw candidate.

3. NEW_LOCAL (INFO) -- permuter-introduced `new_var*` temporaries, listed so the
   reviewer knows where to look.  Not a finding on its own.

A clean report is NOT a correctness proof.  It means the two known-and-measured
silent-bug classes are absent; the reviewer still owns the diff.

USAGE
-----
    python3 tools/permuter/audit_candidate.py \
        --base   artifacts/.../work/base.c \
        --cand   artifacts/.../work/output-54-1/source.c \
        --function unit_impact_melee_damage

Exit status: 0 clean, 1 findings, 2 could not parse (audit inconclusive --
treat as "read the diff by hand", never as a pass).
"""

import argparse
import os
import sys

_PERMUTER_DIR = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "..", "..", "third_party",
    "decomp-permuter")
sys.path.insert(0, os.path.abspath(_PERMUTER_DIR))

try:
    from perm_pycparser import c_ast, c_parser
except ImportError:  # pragma: no cover
    sys.stderr.write(
        "audit_candidate: cannot import the permuter's vendored pycparser at\n"
        "  %s\n" % os.path.abspath(_PERMUTER_DIR))
    sys.exit(2)


# --------------------------------------------------------------------------
# Parsing
# --------------------------------------------------------------------------

def strip_comments(text):
    """Remove C comments, preserving line count and string/char literals.

    pycparser has no preprocessor, and run.py's base.c carries explanatory
    block comments, so they must go before parsing.  Newlines inside a comment
    are kept so reported line numbers still line up with the file.
    """
    out = []
    i = 0
    n = len(text)
    while i < n:
        ch = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if ch == '"' or ch == "'":
            quote = ch
            out.append(ch)
            i += 1
            while i < n:
                if text[i] == "\\" and i + 1 < n:
                    out.append(text[i:i + 2])
                    i += 2
                    continue
                out.append(text[i])
                if text[i] == quote:
                    i += 1
                    break
                i += 1
            continue
        if ch == "/" and nxt == "*":
            i += 2
            while i < n and not (text[i] == "*" and i + 1 < n
                                 and text[i + 1] == "/"):
                if text[i] == "\n":
                    out.append("\n")
                i += 1
            i += 2
            continue
        if ch == "/" and nxt == "/":
            while i < n and text[i] != "\n":
                i += 1
            continue
        out.append(ch)
        i += 1
    return "".join(out)


def strip_directives(text):
    """Drop surviving preprocessor lines (base.c keeps a few #ifndef guards)."""
    kept = []
    for line in text.split("\n"):
        kept.append("" if line.lstrip().startswith("#") else line)
    return "\n".join(kept)


def parse_c(path):
    """Parse a permuter base.c / candidate source.c.

    These files are already cpp-expanded by run.py and carry the typedef
    prelude pycparser needs, so no include resolution is required -- only
    comment and directive removal.
    """
    with open(path, "r", errors="replace") as fh:
        text = fh.read()
    text = strip_directives(strip_comments(text))
    return c_parser.CParser().parse(text, filename=path)


def find_func(ast, name):
    for ext in ast.ext:
        if isinstance(ext, c_ast.FuncDef) and ext.decl.name == name:
            return ext
    return None


# --------------------------------------------------------------------------
# Def/use collection
# --------------------------------------------------------------------------

class DefUse(c_ast.NodeVisitor):
    """Collect assignments to, and reads of, each simple identifier."""

    def __init__(self):
        self.defs = {}    # name -> assignment count
        self.reads = {}   # name -> read count
        self.declared = set()

    @staticmethod
    def _target_name(node):
        """Return the identifier a store lands on, or None.

        Only a bare `x = ...` counts as defining `x`.  `*p = ...` and
        `a[i] = ...` define memory, not the pointer, and `p` is read there.
        """
        if isinstance(node, c_ast.ID):
            return node.name
        return None

    def visit_Decl(self, node):
        if node.name:
            self.declared.add(node.name)
            if node.init is not None:
                self.defs[node.name] = self.defs.get(node.name, 0) + 1
                self.visit(node.init)
        for child in (node.type, node.bitsize):
            if child is not None:
                self.visit(child)

    def visit_Assignment(self, node):
        tgt = self._target_name(node.lvalue)
        if tgt is not None:
            self.defs[tgt] = self.defs.get(tgt, 0) + 1
            if node.op != "=":
                # `x += y` reads x as well
                self.reads[tgt] = self.reads.get(tgt, 0) + 1
        else:
            self.visit(node.lvalue)
        self.visit(node.rvalue)

    def visit_UnaryOp(self, node):
        if node.op in ("p++", "p--", "++", "--"):
            tgt = self._target_name(node.expr)
            if tgt is not None:
                self.defs[tgt] = self.defs.get(tgt, 0) + 1
                self.reads[tgt] = self.reads.get(tgt, 0) + 1
                return
        self.generic_visit(node)

    def visit_ID(self, node):
        self.reads[node.name] = self.reads.get(node.name, 0) + 1


def def_use(funcdef):
    du = DefUse()
    du.visit(funcdef.body)
    return du


# --------------------------------------------------------------------------
# Check 2: definitions confined to escaping blocks
# --------------------------------------------------------------------------

def _escapes(stmt):
    """True if this statement unconditionally leaves the enclosing block."""
    return isinstance(stmt, (c_ast.Return, c_ast.Break, c_ast.Continue,
                             c_ast.Goto))


def _block_escapes(node):
    """True if a compound statement ends by leaving the block."""
    if isinstance(node, c_ast.Compound):
        items = node.block_items or []
        return bool(items) and _escapes(items[-1])
    return _escapes(node)


def _names_defined_in(node):
    du = DefUse()
    du.visit(node)
    return set(du.defs)


def _names_read_in(node):
    du = DefUse()
    du.visit(node)
    return set(du.reads)


def escaping_only_defs(funcdef):
    """Locals whose every def is inside a block that leaves the function.

    Walks each `if` whose then/else branch escapes, collects what that branch
    defines, and reports names that are read outside it but never defined
    outside it.
    """
    body = funcdef.body
    all_du = def_use(funcdef)
    findings = []

    escaping_defs = set()

    class IfWalker(c_ast.NodeVisitor):
        def visit_If(self, node):
            for branch in (node.iftrue, node.iffalse):
                if branch is not None and _block_escapes(branch):
                    escaping_defs.update(_names_defined_in(branch))
            self.generic_visit(node)

    IfWalker().visit(body)

    for name in sorted(escaping_defs):
        if name not in all_du.declared:
            continue
        # How many defs live outside every escaping branch?
        outside = _outside_def_count(body, name)
        if outside == 0 and all_du.reads.get(name, 0) > 0:
            if _read_outside_escaping(body, name):
                findings.append(name)
    return findings


def _outside_def_count(body, name):
    """Count defs of `name` that are NOT inside an escaping if-branch."""
    total = def_use_count_defs(body, name)
    inside = 0

    class W(c_ast.NodeVisitor):
        def visit_If(self, node):
            nonlocal inside
            for branch in (node.iftrue, node.iffalse):
                if branch is not None and _block_escapes(branch):
                    inside += def_use_count_defs(branch, name)
                else:
                    if branch is not None:
                        self.visit(branch)
            self.visit(node.cond)

    W().visit(body)
    return max(0, total - inside)


def def_use_count_defs(node, name):
    du = DefUse()
    du.visit(node)
    return du.defs.get(name, 0)


def _read_outside_escaping(body, name):
    """True if `name` is read somewhere other than inside an escaping branch."""
    inside_reads = 0

    class W(c_ast.NodeVisitor):
        def visit_If(self, node):
            nonlocal inside_reads
            for branch in (node.iftrue, node.iffalse):
                if branch is not None and _block_escapes(branch):
                    inside_reads += len(
                        [n for n in _iter_ids(branch) if n == name])
                elif branch is not None:
                    self.visit(branch)
            self.visit(node.cond)

    W().visit(body)
    total_reads = len([n for n in _iter_ids(body) if n == name])
    return (total_reads - inside_reads) > 0


def _iter_ids(node):
    class W(c_ast.NodeVisitor):
        def __init__(self):
            self.names = []

        def visit_ID(self, n):
            self.names.append(n.name)

    w = W()
    w.visit(node)
    return w.names


# --------------------------------------------------------------------------
# Report
# --------------------------------------------------------------------------

def audit(base_path, cand_path, funcname):
    try:
        base_ast = parse_c(base_path)
        cand_ast = parse_c(cand_path)
    except Exception as exc:  # pycparser raises bare ParseError subclasses
        sys.stderr.write("audit_candidate: parse failed: %s\n" % exc)
        return 2, []

    base_fn = find_func(base_ast, funcname)
    cand_fn = find_func(cand_ast, funcname)
    if base_fn is None or cand_fn is None:
        sys.stderr.write(
            "audit_candidate: %s not found in %s\n"
            % (funcname, base_path if base_fn is None else cand_path))
        return 2, []

    base_du = def_use(base_fn)
    cand_du = def_use(cand_fn)

    findings = []

    # Check 1: LOST_DEF
    for name, n_base in sorted(base_du.defs.items()):
        if name not in base_du.declared:
            continue
        n_cand = cand_du.defs.get(name, 0)
        if n_cand < n_base and cand_du.reads.get(name, 0) > 0:
            findings.append((
                "ERROR", "LOST_DEF", name,
                "assigned %d time(s) in base, %d in candidate, still read %d "
                "time(s) -- later reads observe a stale value"
                % (n_base, n_cand, cand_du.reads.get(name, 0))))

    # Check 2: UNDEF_PATH
    for name in escaping_only_defs(cand_fn):
        findings.append((
            "ERROR", "UNDEF_PATH", name,
            "every assignment is inside a branch that leaves the function, "
            "but it is read outside that branch -- guaranteed uninitialized "
            "read"))

    # Check 3: NEW_LOCAL (informational)
    new_locals = sorted(cand_du.declared - base_du.declared)
    for name in new_locals:
        findings.append((
            "INFO", "NEW_LOCAL", name,
            "permuter-introduced temporary -- confirm it does not change "
            "evaluation order or lifetime"))

    errors = [f for f in findings if f[0] == "ERROR"]
    return (1 if errors else 0), findings


def main():
    ap = argparse.ArgumentParser(
        description="Semantic audit of a decomp-permuter candidate.")
    ap.add_argument("--base", required=True, help="permuter base.c")
    ap.add_argument("--cand", required=True,
                    help="candidate output-<penalty>/source.c")
    ap.add_argument("--function", required=True, help="target function name")
    ap.add_argument("--quiet", action="store_true",
                    help="print findings only, no header")
    args = ap.parse_args()

    rc, findings = audit(args.base, args.cand, args.function)

    if not args.quiet:
        print("audit_candidate: %s" % args.function)
        print("  base: %s" % args.base)
        print("  cand: %s" % args.cand)

    if rc == 2:
        print("  INCONCLUSIVE -- could not parse; review the diff by hand")
        return 2

    if not findings:
        print("  clean (no LOST_DEF / UNDEF_PATH; no new locals)")
        print("  NOTE: not a correctness proof -- still read the diff")
        return 0

    for level, kind, name, detail in findings:
        print("  [%s] %s %s: %s" % (level, kind, name, detail))

    if rc == 1:
        print("  REJECT -- semantic change detected; do not apply this "
              "candidate")
    else:
        print("  no errors; %d informational finding(s) -- read the diff"
              % len(findings))
    return rc


if __name__ == "__main__":
    sys.exit(main())
