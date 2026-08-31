#!/usr/bin/env python3
"""Prove bit-exact equivalence of two branchless SSE function bodies.

Symbolically executes two AT&T-syntax instruction streams (candidate vs
reference) over an abstract domain of 4-lane XMM expression trees, then
compares the expression stored to each output memory slot.

For a BRANCHLESS function whose arithmetic is entirely single-precision
(mulps/addps, plus x87 fld/fmul/fstps which rounds once to single), identical
expression trees imply bit-identical results for EVERY possible input --
IEEE-754 single-precision arithmetic is deterministic. This is a proof, not a
sample, and it is strictly stronger than seeded equivalence with a ULP
tolerance.

Register-to-register `movaps` copies and `movaps` spill/reload pairs are
value-preserving and are modelled exactly (not skipped), so a mis-routed copy
would show up as a differing expression tree rather than being hidden.

Usage:
  sse_dataflow_equiv.py --cand cand.asm --cand-parm-base ebx \
                        --ref  ref.asm  --ref-parm-base  ebp
"""
import argparse
import re
import sys

# ---------------------------------------------------------------- expressions
# Leaf:  ('mem', sym, off)   -- the float at <sym>[off/4]
#        ('zero',)
# Node:  ('mul', x, y) / ('add', x, y)
# Pointer value in a GPR: ('ptr', sym, off)


def ppr(e):
    """Render an expression tree as readable infix."""
    if e is None:
        return "?"
    k = e[0]
    if k == "mem":
        return f"{e[1]}[{e[2] // 4}]"
    if k == "zero":
        return "0"
    if k == "mul":
        return f"({ppr(e[1])}*{ppr(e[2])})"
    if k == "add":
        return f"({ppr(e[1])}+{ppr(e[2])})"
    return str(e)


def eq_exact(x, y):
    """Structural equality -- same ops in the same operand order."""
    if x is None or y is None:
        return x is y
    if x[0] != y[0]:
        return False
    if x[0] == "mem":
        return x[1] == y[1] and x[2] == y[2]
    if x[0] == "zero":
        return True
    return eq_exact(x[1], y[1]) and eq_exact(x[2], y[2])


def eq_commutative(x, y):
    """Equality allowing add/mul operand swap.

    IEEE addition and multiplication are commutative and bit-exact under
    swap, so a difference detected only here is still a proof of
    bit-identical results.
    """
    if x is None or y is None:
        return x is y
    if x[0] != y[0]:
        return False
    if x[0] == "mem":
        return x[1] == y[1] and x[2] == y[2]
    if x[0] == "zero":
        return True
    a, b = x[1], x[2]
    c, d = y[1], y[2]
    return (eq_commutative(a, c) and eq_commutative(b, d)) or (
        eq_commutative(a, d) and eq_commutative(b, c)
    )


# ------------------------------------------------------------------- machine
class Machine(object):
    """Abstract machine for a branchless SSE body."""

    def __init__(self, parm_base):
        # parm_base: which GPR holds the frame pointer that param homes are
        # addressed off (ebp for a normal frame, ebx after cl.exe's
        # stack-realignment prologue).
        self.parm_base = parm_base
        self.xmm = {i: [None] * 4 for i in range(8)}
        # GPRs holding pointer values
        self.gpr = {}
        # Memory: key -> value.
        #   ('ptr', sym, off) for float stores through a recovered pointer
        #   ('PARM', off)     for the incoming parameter homes / scratch
        #   ('LOC', off)      for frame locals (negative ebp offsets)
        self.mem = {
            ("PARM", 8): ("ptr", "a", 0),
            ("PARM", 0xC): ("ptr", "b", 0),
            ("PARM", 0x10): ("ptr", "out", 0),
        }
        self.x87 = []
        self.stores = {}  # ('ptr',sym,off) -> expr, in program order
        self.order = []
        self.unhandled = []

    # -- address resolution ------------------------------------------------
    def addr(self, disp, base):
        """Resolve disp(%base) to an abstract address key."""
        if base == self.parm_base and disp > 0:
            return ("PARM", disp)
        if base in ("ebp", "esp") and disp < 0:
            return ("LOC", disp)
        if base == self.parm_base and disp < 0:
            return ("LOC", disp)
        p = self.gpr.get(base)
        if p is None or p[0] != "ptr":
            return ("UNKNOWN", base, disp)
        return ("ptr", p[1], p[2] + disp)

    def load_float(self, disp, base, extra=0):
        k = self.addr(disp, base)
        if k[0] == "ptr":
            return ("mem", k[1], k[2] + extra)
        # a reload of a spilled xmm lane
        return self.mem.get((k[0],) + tuple(k[1:]) + (extra,), None)

    def store_float(self, disp, base, val, extra=0):
        k = self.addr(disp, base)
        if k[0] == "ptr":
            key = ("ptr", k[1], k[2] + extra)
            self.stores[key] = val
            self.order.append(key)
        else:
            self.mem[(k[0],) + tuple(k[1:]) + (extra,)] = val


_SUFFIX = {
    "movl": "mov",
    "leal": "lea",
    "pushl": "push",
    "popl": "pop",
    "retl": "ret",
    "subl": "sub",
    "addl": "add",
    "andl": "and",
    "movw": "mov",
    "movb": "mov",
}

XMM = re.compile(r"^%xmm(\d)$")
MEMOP = re.compile(r"^(-?(?:0x)?[0-9a-fA-F]*)\((%e[a-z][a-z])\)$")


def parse_mem(op):
    m = MEMOP.match(op)
    if not m:
        return None
    d = m.group(1)
    disp = 0 if d in ("", "-") else int(d, 16) if d.startswith(("0x", "-0x")) else int(d, 16)
    return disp, m.group(2)[1:]


def run(lines, parm_base, verbose=False):
    M = Machine(parm_base)
    for raw in lines:
        # "  12a:\taddps  %xmm6,%xmm3"
        body = raw.split(":", 1)[1].strip() if ":" in raw else raw.strip()
        body = body.split("#")[0].strip()
        if not body:
            continue
        parts = body.split(None, 1)
        op = parts[0]
        # llvm-objdump emits l-suffixed AT&T mnemonics (movl/leal/pushl/...)
        op = _SUFFIX.get(op, op)
        args = [a.strip() for a in parts[1].split(",")] if len(parts) > 1 else []

        # ---- control / frame: no effect on values we track
        if op in ("push", "pop", "ret", "sub", "and", "nop", "int3", "leave"):
            continue
        if op == "mov":
            src, dst = args[0], args[1]
            if src == "%esp" or dst == "%esp":
                continue
            sm = parse_mem(src)
            dm = parse_mem(dst)
            if sm and dst.startswith("%e"):  # load pointer
                M.gpr[dst[1:]] = M.mem.get(M.addr(*sm), None)
            elif dm and src.startswith("%e"):  # store pointer
                M.mem[M.addr(*dm)] = M.gpr.get(src[1:])
            elif src.startswith("%e") and dst.startswith("%e"):
                M.gpr[dst[1:]] = M.gpr.get(src[1:])
            continue
        if op == "lea":
            sm = parse_mem(args[0])
            if sm and args[1].startswith("%e"):
                base = M.gpr.get(sm[1])
                if base and base[0] == "ptr":
                    M.gpr[args[1][1:]] = ("ptr", base[1], base[2] + sm[0])
            continue
        if op == "add" and args[0].startswith("$") and args[1].startswith("%e"):
            r = args[1][1:]
            p = M.gpr.get(r)
            if p and p[0] == "ptr":
                M.gpr[r] = ("ptr", p[1], p[2] + int(args[0][1:], 16))
            continue

        # ---- SSE
        if op == "movss":
            if XMM.match(args[1]):  # load
                n = int(XMM.match(args[1]).group(1))
                mm = parse_mem(args[0])
                M.xmm[n] = [M.load_float(*mm), ("zero",), ("zero",), ("zero",)]
            else:  # store lane 0
                n = int(XMM.match(args[0]).group(1))
                mm = parse_mem(args[1])
                M.store_float(mm[0], mm[1], M.xmm[n][0])
            continue
        if op in ("movhps", "movhpd"):
            if XMM.match(args[1]):  # load lanes 2,3
                n = int(XMM.match(args[1]).group(1))
                mm = parse_mem(args[0])
                M.xmm[n][2] = M.load_float(mm[0], mm[1], 0)
                M.xmm[n][3] = M.load_float(mm[0], mm[1], 4)
            else:  # store lanes 2,3
                n = int(XMM.match(args[0]).group(1))
                mm = parse_mem(args[1])
                M.store_float(mm[0], mm[1], M.xmm[n][2], 0)
                M.store_float(mm[0], mm[1], M.xmm[n][3], 4)
            continue
        if op in ("movlps",):
            if XMM.match(args[1]):
                n = int(XMM.match(args[1]).group(1))
                mm = parse_mem(args[0])
                M.xmm[n][0] = M.load_float(mm[0], mm[1], 0)
                M.xmm[n][1] = M.load_float(mm[0], mm[1], 4)
            else:
                n = int(XMM.match(args[0]).group(1))
                mm = parse_mem(args[1])
                M.store_float(mm[0], mm[1], M.xmm[n][0], 0)
                M.store_float(mm[0], mm[1], M.xmm[n][1], 4)
            continue
        if op in ("movaps", "movups"):
            s, d = args[0], args[1]
            if XMM.match(s) and XMM.match(d):  # register copy
                M.xmm[int(XMM.match(d).group(1))] = list(
                    M.xmm[int(XMM.match(s).group(1))]
                )
            elif XMM.match(d):  # reload all 4 lanes
                n = int(XMM.match(d).group(1))
                mm = parse_mem(s)
                M.xmm[n] = [M.load_float(mm[0], mm[1], 4 * i) for i in range(4)]
            else:  # spill all 4 lanes
                n = int(XMM.match(s).group(1))
                mm = parse_mem(d)
                for i in range(4):
                    M.store_float(mm[0], mm[1], M.xmm[n][i], 4 * i)
            continue
        if op == "shufps":
            imm = int(args[0][1:], 16)
            src = int(XMM.match(args[1]).group(1))
            dst = int(XMM.match(args[2]).group(1))
            s, d = M.xmm[src], M.xmm[dst]
            M.xmm[dst] = [
                d[imm & 3],
                d[(imm >> 2) & 3],
                s[(imm >> 4) & 3],
                s[(imm >> 6) & 3],
            ]
            continue
        if op in ("mulps", "addps"):
            kind = "mul" if op == "mulps" else "add"
            if XMM.match(args[0]):
                s = M.xmm[int(XMM.match(args[0]).group(1))]
            else:
                mm = parse_mem(args[0])
                s = [M.load_float(mm[0], mm[1], 4 * i) for i in range(4)]
            dst = int(XMM.match(args[1]).group(1))
            M.xmm[dst] = [(kind, M.xmm[dst][i], s[i]) for i in range(4)]
            continue

        # ---- x87 (the single scalar out[0] = a[0] * b[0])
        if op == "flds":
            mm = parse_mem(args[0])
            M.x87.insert(0, M.load_float(*mm))
            continue
        if op == "fmuls":
            mm = parse_mem(args[0])
            M.x87[0] = ("mul", M.x87[0], M.load_float(*mm))
            continue
        if op == "fstps":
            mm = parse_mem(args[0])
            M.store_float(mm[0], mm[1], M.x87.pop(0))
            continue

        M.unhandled.append(body)
    return M


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cand", required=True)
    ap.add_argument("--ref", required=True)
    ap.add_argument("--cand-parm-base", default="ebp")
    ap.add_argument("--ref-parm-base", default="ebp")
    a = ap.parse_args()

    C = run(open(a.cand).read().splitlines(), a.cand_parm_base)
    R = run(open(a.ref).read().splitlines(), a.ref_parm_base)

    for tag, M in (("candidate", C), ("reference", R)):
        if M.unhandled:
            print(f"[WARN] {tag}: {len(M.unhandled)} unmodelled instruction(s):")
            for u in sorted(set(M.unhandled)):
                print(f"          {u}")

    keys = sorted(
        set(C.stores) | set(R.stores), key=lambda k: (str(k[1]), k[2])
    )
    exact = commut = diff = 0
    print(f"\n{'output slot':<12} {'verdict':<10} expression")
    print("-" * 78)
    for k in keys:
        cv, rv = C.stores.get(k), R.stores.get(k)
        if eq_exact(cv, rv):
            v, exact = "EXACT", exact + 1
        elif eq_commutative(cv, rv):
            v, commut = "COMMUTED", commut + 1
        else:
            v, diff = "**DIFF**", diff + 1
        slot = f"{k[1]}[{k[2] // 4}]"
        print(f"{slot:<12} {v:<10} {ppr(cv)}")
        if v == "**DIFF**":
            print(f"{'':<12} {'ref:':<10} {ppr(rv)}")

    only_c = set(C.stores) - set(R.stores)
    only_r = set(R.stores) - set(C.stores)
    print("-" * 78)
    print(f"exact={exact} commuted={commut} differing={diff}")
    if only_c:
        print(f"[WARN] stores only in candidate: {sorted(only_c)}")
    if only_r:
        print(f"[WARN] stores only in reference: {sorted(only_r)}")

    ok = (
        diff == 0
        and not only_c
        and not only_r
        and not C.unhandled
        and not R.unhandled
    )
    print(
        "\nRESULT: bit-exact for ALL inputs (branchless, single-precision)"
        if ok
        else "\nRESULT: NOT PROVEN -- investigate above"
    )
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
