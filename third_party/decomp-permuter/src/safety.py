"""[halo] Conservative effect analysis for semantics-preserving mutations.

The upstream randomization passes move statements and expressions using only
direct variable writes (perm_temp_for_expr says so in a TODO).  They ignore
memory writes, calls, aliasing, conditional evaluation and loop back-edges.
Under a raw-byte objective that matters: dropping or reordering work often
saves bytes, so behavior-changing candidates win.  Measured on RGBToColor:
perm_reorder_stmts moved a struct store above the write of its last field.

When PERMUTER_SAFE_MUTATIONS=1 (set by tools/permuter/run.py), the guarded
passes call into this module and reject any move it cannot prove safe.  The
model is deliberately coarse:

* A "tracked" variable is a parameter or non-array local whose address is
  never taken.  Everything else, including every global, is "memory".
* Accesses to tracked variables are recorded as paths: `s`, `s.f`,
  `s.arr[3]`.  Paths overlap when one is a prefix of the other.  A subscript
  is only part of a path when the base is a real array member (from the type
  map) and the index is a constant; otherwise the access covers the whole
  member, or is memory when the member is a pointer.
* A call reads and writes memory.  `*p`, `->` and pointer subscripts read or
  write memory.
* return/break/continue/goto/labels are barriers.
"""

import os
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Set

from perm_pycparser import c_ast as ca

from .ast_types import TypeMap, build_typemap, expr_type, resolve_typedefs


def enabled() -> bool:
    return os.environ.get("PERMUTER_SAFE_MUTATIONS") == "1"


class Tracked(set):  # type: ignore[type-arg]
    """Tracked variable names, plus the type map used to tell array members
    from pointer members."""

    typemap: Optional[TypeMap] = None


@dataclass
class Effects:
    reads: Set[str] = field(default_factory=set)
    writes: Set[str] = field(default_factory=set)
    mem_read: bool = False
    mem_write: bool = False
    barrier: bool = False


def overlaps(a: Set[str], b: Set[str]) -> bool:
    """True if any path in a overlaps any path in b."""
    for x in a:
        for y in b:
            if x == y:
                return True
            short, long = (x, y) if len(x) < len(y) else (y, x)
            if long.startswith(short) and long[len(short)] in ".[":
                return True
    return False


def tracked_vars(fn: ca.FuncDef, ast: Optional[ca.FileAST] = None) -> Tracked:
    names: Set[str] = set()
    untracked: Set[str] = set()

    class Visitor(ca.NodeVisitor):
        def visit_Decl(self, node: ca.Decl) -> None:
            if node.name:
                names.add(node.name)
                if isinstance(node.type, ca.ArrayDecl):
                    untracked.add(node.name)
            self.generic_visit(node)

        def visit_UnaryOp(self, node: ca.UnaryOp) -> None:
            if node.op == "&":
                root = _root_id(node.expr)
                if root is not None:
                    untracked.add(root)
            self.generic_visit(node)

    Visitor().visit(fn.decl)
    Visitor().visit(fn.body)
    tracked = Tracked(names - untracked)
    if ast is not None:
        try:
            tracked.typemap = build_typemap(ast, fn)
        except Exception:
            tracked.typemap = None
    return tracked


def _root_id(node: ca.Node) -> Optional[str]:
    """The variable at the root of a `.`/`[]` chain, if any."""
    while True:
        if isinstance(node, ca.ID):
            return node.name
        if isinstance(node, ca.StructRef) and node.type == ".":
            node = node.name
        elif isinstance(node, ca.ArrayRef):
            node = node.name
        else:
            return None


def _is_array(node: ca.Node, tracked: Set[str]) -> bool:
    typemap = getattr(tracked, "typemap", None)
    if typemap is None:
        return False
    try:
        return isinstance(resolve_typedefs(expr_type(node, typemap), typemap), ca.ArrayDecl)
    except Exception:
        return False


def _path(node: ca.Node, tracked: Set[str], eff: Effects) -> Optional[str]:
    """Path of a tracked-variable access (recording the reads needed to form
    it), or None if the access goes through memory."""
    if isinstance(node, ca.ID):
        return node.name if node.name in tracked else None
    if isinstance(node, ca.StructRef) and node.type == ".":
        base = _path(node.name, tracked, eff)
        return None if base is None else "%s.%s" % (base, node.field.name)
    if isinstance(node, ca.ArrayRef) and _is_array(node.name, tracked):
        base = _path(node.name, tracked, eff)
        if base is None:
            return None
        if isinstance(node.subscript, ca.Constant):
            return "%s[%s]" % (base, node.subscript.value)
        _visit(node.subscript, tracked, eff)
        return base
    return None


def effects(node: Optional[ca.Node], tracked: Set[str]) -> Effects:
    eff = Effects()
    if node is not None:
        _visit(node, tracked, eff)
    return eff


def _write_lvalue(lv: ca.Node, tracked: Set[str], eff: Effects, also_read: bool) -> None:
    path = _path(lv, tracked, eff)
    if path is not None:
        eff.writes.add(path)
        if also_read:
            eff.reads.add(path)
        return
    eff.mem_write = True
    eff.mem_read |= also_read
    _visit_address(lv, tracked, eff)


def _visit_address(lv: ca.Node, tracked: Set[str], eff: Effects) -> None:
    """Reads needed to compute an lvalue's address."""
    if isinstance(lv, ca.ID):
        return
    if isinstance(lv, ca.StructRef):
        if lv.type == "->":
            _visit(lv.name, tracked, eff)
        else:
            _visit_address(lv.name, tracked, eff)
    elif isinstance(lv, ca.ArrayRef):
        if _is_array(lv.name, tracked):
            _visit_address(lv.name, tracked, eff)
        else:
            _visit(lv.name, tracked, eff)
        _visit(lv.subscript, tracked, eff)
    elif isinstance(lv, ca.UnaryOp) and lv.op == "*":
        _visit(lv.expr, tracked, eff)
    elif isinstance(lv, ca.Cast):
        _visit_address(lv.expr, tracked, eff)
    else:
        _visit(lv, tracked, eff)


def _visit(node: ca.Node, tracked: Set[str], eff: Effects) -> None:
    if isinstance(node, (ca.ID, ca.StructRef, ca.ArrayRef)):
        path = _path(node, tracked, eff)
        if path is not None:
            eff.reads.add(path)
            return
        eff.mem_read = True
        if isinstance(node, ca.StructRef) and node.type == "->":
            _visit(node.name, tracked, eff)
        elif isinstance(node, ca.StructRef):
            _visit_address(node.name, tracked, eff)
        elif isinstance(node, ca.ArrayRef):
            _visit_address(node, tracked, eff)
    elif isinstance(node, ca.Assignment):
        _write_lvalue(node.lvalue, tracked, eff, also_read=node.op != "=")
        _visit(node.rvalue, tracked, eff)
    elif isinstance(node, ca.UnaryOp):
        if node.op in ("p++", "p--", "++", "--"):
            _write_lvalue(node.expr, tracked, eff, also_read=True)
        elif node.op == "&":
            _visit_address(node.expr, tracked, eff)
        elif node.op == "sizeof":
            pass
        else:
            if node.op == "*":
                eff.mem_read = True
            _visit(node.expr, tracked, eff)
    elif isinstance(node, ca.FuncCall):
        eff.mem_read = True
        eff.mem_write = True
        if node.args is not None:
            _visit(node.args, tracked, eff)
        if not isinstance(node.name, ca.ID):
            _visit(node.name, tracked, eff)
    elif isinstance(node, ca.Decl):
        if node.init is not None:
            _visit(node.init, tracked, eff)
            if node.name:
                _write_lvalue(ca.ID(node.name), tracked, eff, also_read=False)
    elif isinstance(node, (ca.Return, ca.Break, ca.Continue, ca.Goto, ca.Label)):
        eff.barrier = True
        for _, child in node.children():
            _visit(child, tracked, eff)
    elif isinstance(node, (ca.Typename, ca.Constant, ca.Pragma)):
        pass
    else:
        for _, child in node.children():
            _visit(child, tracked, eff)


def conflict(a: Effects, b: Effects) -> bool:
    """True if a and b may not be swapped."""
    return bool(
        a.barrier
        or b.barrier
        or overlaps(a.writes, b.reads | b.writes)
        or overlaps(b.writes, a.reads)
        or (a.mem_write and (b.mem_read or b.mem_write))
        or (b.mem_write and a.mem_read)
    )


def parent_map(root: ca.Node) -> Dict[int, ca.Node]:
    parents: Dict[int, ca.Node] = {}

    def rec(node: ca.Node) -> None:
        for _, child in node.children():
            parents[id(child)] = node
            rec(child)

    rec(root)
    return parents


def in_loop(node: ca.Node, parents: Dict[int, ca.Node]) -> bool:
    cur = parents.get(id(node))
    while cur is not None:
        if isinstance(cur, (ca.For, ca.While, ca.DoWhile)):
            return True
        cur = parents.get(id(cur))
    return False


def unconditional_in(stmt: ca.Node, target: ca.Node, parents: Dict[int, ca.Node]) -> bool:
    """True if `target`, inside `stmt`, is evaluated exactly once whenever
    `stmt` starts: not in a branch, a loop, a short-circuit right operand, a
    ternary arm or a nested block."""
    child = target
    cur = parents.get(id(target))
    while child is not stmt:
        if cur is None:
            return False
        if isinstance(cur, (ca.Compound, ca.For, ca.While, ca.DoWhile, ca.Switch, ca.Case,
                            ca.Default, ca.Label)):
            if not (isinstance(cur, ca.Switch) and child is cur.cond):
                return False
        if isinstance(cur, ca.If) and child is not cur.cond:
            return False
        if isinstance(cur, ca.TernaryOp) and child is not cur.cond:
            return False
        if isinstance(cur, ca.BinaryOp) and cur.op in ("&&", "||") and child is cur.right:
            return False
        child = cur
        cur = parents.get(id(cur))
    return True


def contains(root: ca.Node, target: ca.Node) -> bool:
    if root is target:
        return True
    return any(contains(child, target) for _, child in root.children())


def index_of_containing(stmts: List[ca.Node], target: ca.Node) -> Optional[int]:
    for i, stmt in enumerate(stmts):
        if contains(stmt, target):
            return i
    return None


def can_cross(moved: Effects, stmts: List[ca.Node], tracked: Set[str]) -> bool:
    for stmt in stmts:
        if conflict(moved, effects(stmt, tracked)):
            return False
    return True


def _effects_without_top_store(stmt: ca.Node, tracked: Set[str]) -> Effects:
    """Effects of stmt other than its own top-level `x = ...` store, which C
    sequences after its operands are evaluated."""
    if isinstance(stmt, ca.Assignment):
        eff = Effects()
        _visit_address(stmt.lvalue, tracked, eff)
        _visit(stmt.rvalue, tracked, eff)
        return eff
    if isinstance(stmt, ca.Decl):
        return effects(stmt.init, tracked)
    return effects(stmt, tracked)


def _whole_value(stmt: ca.Node, expr: ca.Node) -> bool:
    return (
        stmt is expr
        or (isinstance(stmt, ca.Assignment) and stmt.rvalue is expr)
        or (isinstance(stmt, ca.Decl) and stmt.init is expr)
        or (isinstance(stmt, ca.Return) and stmt.expr is expr)
    )


def can_hoist(
    stmts: List[ca.Node],
    place_idx: int,
    expr: ca.Node,
    tracked: Set[str],
    parents: Dict[int, ca.Node],
) -> Optional[int]:
    """If expr (inside stmts[k], k >= place_idx) can be evaluated before
    stmts[place_idx] instead, return k; otherwise None."""
    k = index_of_containing(stmts, expr)
    if k is None or k < place_idx:
        return None
    stmt = stmts[k]
    if not unconditional_in(stmt, expr, parents):
        return None
    eff = effects(expr, tracked)
    if eff.barrier:
        return None
    if eff.writes or eff.mem_write:
        # Other side effects of the same statement could be sequenced on
        # either side of it; only move it when it is the statement's value.
        if not _whole_value(stmt, expr):
            return None
    else:
        other = _effects_without_top_store(stmt, tracked)
        if (
            overlaps(other.writes, eff.reads)
            or (other.mem_write and eff.mem_read)
            or other.barrier
        ):
            return None
    if not can_cross(eff, stmts[place_idx:k], tracked):
        return None
    return k


def value_stable(
    stmts: List[ca.Node],
    start: int,
    end: int,
    expr: ca.Node,
    tracked: Set[str],
) -> bool:
    """True if expr (pure) has the same value throughout stmts[start:end+1],
    including inside loops there (whole statements are checked)."""
    eff = effects(expr, tracked)
    if eff.writes or eff.mem_write or eff.barrier:
        return False
    for stmt in stmts[start : end + 1]:
        other = effects(stmt, tracked)
        if overlaps(other.writes, eff.reads) or (other.mem_write and eff.mem_read):
            return False
        if _has_label(stmt):
            return False
    return True


def _has_label(node: ca.Node) -> bool:
    if isinstance(node, ca.Label):
        return True
    return any(_has_label(child) for _, child in node.children())


def _arith_info(names: List[str]) -> Optional[tuple]:
    """(kind, width, signed) for a basic arithmetic type; MSVC char is signed."""
    if "float" in names or "double" in names:
        if "long" in names:
            return None
        return ("float", 8 if "double" in names else 4, True)
    if "char" in names:
        width = 1
    elif "short" in names:
        width = 2
    elif names.count("long") == 2 or "__int64" in names:
        width = 8
    else:
        width = 4
    return ("int", width, "unsigned" not in names)


def value_preserving_cast(expr: ca.Node, new_names: List[str], typemap: TypeMap) -> bool:
    """True if (new_names)expr has the same value and promoted type as expr.

    Integer expressions narrower than int promote to int, so any cast that
    represents every source value and still promotes to int is invisible.
    Anything int-sized or wider, and any float, must cast to its own type.
    """
    try:
        src = resolve_typedefs(expr_type(expr, typemap), typemap)
    except Exception:
        return False
    if not isinstance(src, ca.TypeDecl) or not isinstance(src.type, ca.IdentifierType):
        return False
    s = _arith_info(src.type.names)
    d = _arith_info(new_names)
    if s is None or d is None:
        return False
    if s == d:
        return True
    if s[0] != "int" or d[0] != "int" or s[1] >= 4:
        return False
    s_width, s_signed = s[1], s[2]
    d_width, d_signed = d[1], d[2]
    if d_width == 4 and not d_signed:
        return False  # promotes to unsigned int
    if d_width > 4:
        return False
    if s_signed == d_signed:
        return d_width >= s_width
    return not s_signed and d_width > s_width
