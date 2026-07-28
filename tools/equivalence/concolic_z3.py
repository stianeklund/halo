"""Z3-backed concolic input generation for unicorn_diff (Phase 3).

`concolic.py` guesses branch-flipping values from hand-rolled Jcc patterns:
it looks at the CMP immediate next to a branch and tries `imm`, `imm+1`,
`imm-1`.  That works when the compared value is loaded directly from the
global it injects into, and fails as soon as anything sits in between --
an arithmetic step, a second condition, a value derived from two globals.

This module replaces the guess with a solve.  Given the concrete path a
passing seed actually walked (the union of visited PCs), it lifts that
path with the in-repo `x86_to_z3` lifter, accumulating the branch
condition at each step in the direction that was really taken, then
negates the condition at one uncovered branch and asks Z3 for initial
memory that satisfies the whole conjunction.

Soundness note: nothing here is a proof, and it does not need to be.  The
output is a *hypothesis* -- a set of memory overrides -- which the caller
re-runs through both emulators and measures.  A wrong hypothesis costs
seeds, never correctness.  That is why this module uses the lifter in
permissive mode (`strict=False`), where an unmodelled instruction is
skipped rather than fatal: a dropped instruction can only make the
constraint less accurate, and an inaccurate constraint yields an
injection that simply does not improve coverage.  Proof callers
(`z3_equiv`) must keep using `strict=True`; see `X86Lifter.__init__`.

Two approximations are deliberate and worth stating:

* **CALL is modelled as `EAX := 0`.**  In the harness almost every stubbed
  callee returns 0 (`DEFAULT_STUB_RETURNS` covers 13 of them), so this
  matches what the emulator actually does far more often than leaving EAX
  symbolic would.  The callee's stack cleanup is not modelled, so ESP
  drifts after a stdcall call -- reads through ESP past that point are
  unreliable, but the stack is not an injection target anyway.
* **When both sides of an earlier branch were visited** (the visited set is
  a union over seeds, not one trace), the walk takes the fallthrough.  That
  picks one real path out of several rather than the specific path of one
  specific seed.

The solver is asked for a model in which *at least one observed global
differs from the value it held during the run*.  Without that, Z3 is free
to flip the branch by choosing incoming register values -- which the
caller cannot inject, so the resulting model would be unusable.  With it,
UNSAT means "this branch is not reachable by changing memory alone", which
is a useful answer: that branch is skipped instead of burning seeds on it.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass, field
from typing import Optional

try:
    import z3
    _Z3_AVAILABLE = True
except ImportError:  # pragma: no cover - environment dependent
    _Z3_AVAILABLE = False

try:
    import capstone
    from capstone import x86_const as X86
    _CAPSTONE_AVAILABLE = True
except ImportError:  # pragma: no cover - environment dependent
    _CAPSTONE_AVAILABLE = False

from concolic import _JCC_IDS, _is_spurious_address

# Bounds.  These are per-call budgets, not global ones; the caller already
# caps how many injections it will actually execute.
MAX_BRANCHES = 8          # uncovered branches to attempt a solve for
MAX_PATH_INSNS = 4000     # instructions to walk before giving up on a path
MAX_INJECT_ADDRS = 6      # globals written by a single solved injection
SOLVER_TIMEOUT_MS = 5000  # per-branch solver budget


@dataclass
class SolveStats:
    """Per-run accounting, printed by the caller so the lane is legible."""
    attempted: int = 0
    unreached: int = 0
    sat: int = 0
    unsat: int = 0
    unknown: int = 0
    errors: int = 0
    lift_errors: int = 0
    register_only: int = 0
    no_globals: int = 0
    # Which filter rule rejected the addresses, when nothing was injectable.
    # "22 no-injectable-globals" is not actionable on its own; knowing they
    # were all DIR32 slots (correct, inapplicable) versus all NULL-page
    # artifacts (a capture problem) points at different fixes.
    reject_reasons: dict = field(default_factory=dict)

    def summary(self) -> str:
        # Every attempt lands in exactly one bucket.  An attempt that fell
        # through uncounted used to read as success in the aggregate; it is
        # the same "silence looks like a pass" failure this harness exists
        # to catch, so the buckets are kept exhaustive on purpose.
        return (f"{self.attempted} attempted, {self.sat} solved, "
                f"{self.unsat} unreachable-by-memory, "
                f"{self.register_only} register-gated, "
                f"{self.no_globals} no-injectable-globals, "
                f"{self.unreached} path-not-reached, "
                f"{self.unknown} timeout, "
                f"{self.errors + self.lift_errors} error"
                + (f" [rejected: {reasons}]"
                   if (reasons := ", ".join(
                       f"{n}x {r}" for r, n
                       in sorted(self.reject_reasons.items()))) else ""))

    def accounted(self) -> bool:
        """True when the buckets sum to `attempted` (self-check for reports)."""
        return self.attempted == (self.sat + self.unsat + self.register_only
                                  + self.no_globals + self.unreached
                                  + self.unknown + self.errors
                                  + self.lift_errors)


def available() -> bool:
    """True when both z3 and capstone are importable."""
    return _Z3_AVAILABLE and _CAPSTONE_AVAILABLE


@dataclass
class _SymbolicGlobals:
    """The observed globals, re-expressed as free Z3 variables."""
    syms: dict = field(default_factory=dict)      # addr -> (BitVec, size)
    observed: dict = field(default_factory=dict)  # addr -> int


def _reject_reason(addr: int, size: int) -> Optional[str]:
    """Why this observed read cannot be an injection target, or None."""
    if size not in (1, 2, 4):
        return "odd-size"
    if addr < 0x10000:
        return "null-page"
    if addr >= 0x80000000:
        return "kernel-range"
    if 0x500000 <= addr < 0x600000:
        return "dir32-arena"
    return None


def _symbolize_globals(state, global_reads: dict,
                       reject_reasons: dict = None) -> _SymbolicGlobals:
    """Store a fresh symbol at each concrete global address the run read.

    Reads of these addresses during lifting then resolve to the symbol
    rather than to an opaque `Select` on the base memory array, so the
    model gives a directly usable value per address.  Addresses the
    injection filter would reject are left alone -- symbolizing them
    would let the solver build a model we are then forbidden to use.
    """
    sg = _SymbolicGlobals()
    for addr, entry in global_reads.items():
        try:
            size, value = entry
        except (TypeError, ValueError):
            continue
        reason = _reject_reason(addr, size)
        if reason is not None:
            if reject_reasons is not None:
                reject_reasons[reason] = reject_reasons.get(reason, 0) + 1
            continue
        sym = z3.BitVec(f"g_{addr:08x}", size * 8)
        sg.syms[addr] = (sym, size)
        sg.observed[addr] = value & ((1 << (size * 8)) - 1)
        for i in range(size):
            byte = z3.Extract(8 * i + 7, 8 * i, sym)
            state.mem = z3.Store(state.mem, z3.BitVecVal(addr + i, 32), byte)
    return sg


def _decode_one(md, code: bytes, code_base: int, pc: int):
    """Decode the single instruction at `pc`, or None if out of range."""
    offset = pc - code_base
    if offset < 0 or offset >= len(code):
        return None
    for insn in md.disasm(code[offset:offset + 16], pc):
        return insn
    return None


def _build_cfg(md, code: bytes, code_base: int) -> dict:
    """Linear-sweep successor map for the function body.

    Good enough to answer "can control get from here to there", which is
    all the walk needs; indirect transfers simply have no successors and
    terminate a path.
    """
    cfg = {}
    for insn in md.disasm(code, code_base):
        nxt = insn.address + insn.size
        if insn.id == X86.X86_INS_RET:
            cfg[insn.address] = []
        elif insn.id == X86.X86_INS_JMP:
            if insn.operands and insn.operands[0].type == X86.X86_OP_IMM:
                cfg[insn.address] = [insn.operands[0].imm]
            else:
                cfg[insn.address] = []
        elif insn.id in _JCC_IDS and insn.operands:
            cfg[insn.address] = [nxt, insn.operands[0].imm]
        else:
            cfg[insn.address] = [nxt]
    return cfg


def _can_reach(cfg: dict, target: int) -> set:
    """Addresses from which `target` is reachable (reverse BFS over the CFG)."""
    preds = {}
    for addr, succs in cfg.items():
        for s in succs:
            preds.setdefault(s, []).append(addr)
    seen = {target}
    queue = [target]
    while queue:
        cur = queue.pop()
        for p in preds.get(cur, ()):
            if p not in seen:
                seen.add(p)
                queue.append(p)
    return seen


def _walk_to_branch(lifter, code: bytes, code_base: int, visited_pcs: dict,
                    target_addr: int, untaken_is_target: bool,
                    can_reach: set = None):
    """Lift the executed path up to `target_addr` and return its constraints.

    Returns `(constraints, flip_constraint)` where `constraints` are the
    conditions of the branches passed on the way, each oriented in the
    direction actually taken, and `flip_constraint` forces the direction
    that was never executed.  Returns `None` when the walk cannot reach
    the target branch (unmodelled control flow, a dead end, or a budget
    hit) -- the caller counts that as `unreached` rather than guessing.
    """
    constraints = []
    pc = code_base
    count = 0

    while count < MAX_PATH_INSNS:
        insn = _decode_one(lifter.md, code, code_base, pc)
        if insn is None:
            return None
        count += 1

        iid = insn.id

        if iid == X86.X86_INS_CALL:
            # Stubbed in the emulator; almost always returns 0.  Model the
            # return value so downstream conditions on it are faithful, and
            # step over the call.
            lifter.state.set_reg32("eax", z3.BitVecVal(0, 32))
            pc = insn.address + insn.size
            continue

        if iid == X86.X86_INS_RET:
            return None  # returned without reaching the target branch

        if iid == X86.X86_INS_JMP:
            if insn.operands and insn.operands[0].type == X86.X86_OP_IMM:
                pc = insn.operands[0].imm
                continue
            return None  # indirect jump: path not recoverable statically

        bp = lifter.lift_instruction(insn)

        if bp is None:
            pc = insn.address + insn.size
            continue

        # Conditional branch.
        if bp.address == target_addr:
            # `untaken_is_target` says the never-executed side is the jump
            # target, so forcing it means asserting the taken-condition.
            flip = bp.condition if untaken_is_target else z3.Not(bp.condition)
            return constraints, flip

        # Choose a successor.  Steering by *static reachability* first and
        # by what was observed second matters more than it looks: the
        # visited set is a union over seeds, not one trace, so "both sides
        # were seen" is common, and blindly preferring the fallthrough
        # walks into a RET whenever the target branch sits on the taken
        # side.  Taking an unobserved-but-reaching edge is still a feasible
        # path, so its constraint is still sound -- it just describes a
        # path we are asking for rather than one we watched.
        options = []
        for succ, cond in ((bp.fallthrough_target, z3.Not(bp.condition)),
                           (bp.taken_target, bp.condition)):
            if can_reach is not None and succ not in can_reach:
                continue
            options.append((succ in visited_pcs, succ, cond))
        if not options:
            return None  # target unreachable from here
        options.sort(key=lambda o: not o[0])  # observed edges first
        _, pc, cond = options[0]
        constraints.append(cond)

    return None


def _collect_var_names(expr, out: set, seen: set) -> None:
    """Gather the names of the free constants appearing in a Z3 expression."""
    eid = expr.get_id()
    if eid in seen:
        return
    seen.add(eid)
    if z3.is_const(expr) and expr.decl().kind() == z3.Z3_OP_UNINTERPRETED:
        out.add(expr.decl().name())
        return
    for child in expr.children():
        _collect_var_names(child, out, seen)


def _relevant_globals(sg: _SymbolicGlobals, exprs: list) -> dict:
    """Restrict the symbol set to globals the path condition actually reads.

    A branch gated purely on incoming registers is satisfiable by choosing
    those registers, and the solver would happily do so while flipping an
    unrelated global just to satisfy the "something in memory must differ"
    requirement.  That produces an injection with no causal link to the
    branch.  Narrowing to the globals that appear in the formula makes the
    requirement meaningful, and makes an empty result the honest answer for
    register-gated branches.
    """
    names = set()
    seen = set()
    for e in exprs:
        _collect_var_names(e, names, seen)
    return {addr: (sym, size) for addr, (sym, size) in sg.syms.items()
            if sym.decl().name() in names}


def _model_to_injection(model, sg: _SymbolicGlobals,
                        relevant: dict) -> Optional[dict]:
    """Turn a satisfying model into one `{addr: bytes}` override dict.

    Only addresses whose solved value differs from the value observed during
    the run are emitted -- the rest are already what the emulator will
    supply.  The model is a joint assignment, so all differing addresses go
    into a single dict and are injected together.
    """
    packers = {1: "<B", 2: "<H", 4: "<I"}
    injection = {}
    for addr, (sym, size) in sorted(relevant.items()):
        solved = model[sym]
        if solved is None:
            continue  # unconstrained: leave the emulator's own value alone
        val = solved.as_long() & ((1 << (size * 8)) - 1)
        if val == sg.observed.get(addr):
            continue
        if _is_spurious_address(addr):
            continue  # belt and braces: never inject into the DIR32 arena
        try:
            injection[addr] = struct.pack(packers[size], val)
        except (struct.error, KeyError):
            continue
        if len(injection) >= MAX_INJECT_ADDRS:
            break
    return injection or None


def solve_uncovered(code: bytes, code_base: int, visited_pcs: dict,
                    uncovered: list, global_reads: dict,
                    max_branches: int = MAX_BRANCHES,
                    timeout_ms: int = SOLVER_TIMEOUT_MS):
    """Solve for memory that reaches uncovered branches.

    `code` / `code_base` are the oracle's patched bytes and their base
    address, `visited_pcs` the union of PCs executed by the seed sweep,
    `uncovered` the `UncoveredBranch` list from `concolic.find_uncovered`,
    and `global_reads` the `{addr: (size, value)}` map the run recorded.

    Returns `(injections, stats)`.  `injections` is a list of
    `{int_address: bytes}` dicts in the same shape
    `concolic.generate_memory_injections` produces, so the two are
    interchangeable and can be concatenated.
    """
    stats = SolveStats()
    if not available() or not uncovered or not global_reads:
        return [], stats

    # Import here so a missing z3 degrades to the heuristic path rather
    # than breaking module import for every caller.
    from x86_to_z3 import X86State, X86Lifter, LiftError

    injections = []
    seen = set()
    cfg = None
    reach_cache = {}

    for ub in uncovered[:max_branches]:
        stats.attempted += 1
        try:
            state = X86State(prefix="c_")
            # Record the rejection breakdown once; the same read set is
            # re-symbolized every attempt and counting it each time would
            # just multiply by the branch count.
            sg = _symbolize_globals(
                state, global_reads,
                reject_reasons=(stats.reject_reasons
                                if stats.attempted == 1 else None))
            if not sg.syms:
                # Every address the run read was filtered out (DIR32 arena,
                # NULL page, kernel range).  There is nothing this lane is
                # permitted to inject into.
                stats.no_globals += 1
                continue
            lifter = X86Lifter(state, code_base=code_base, strict=False)

            if cfg is None:
                cfg = _build_cfg(lifter.md, code, code_base)
            can_reach = reach_cache.get(ub.branch.address)
            if can_reach is None:
                can_reach = _can_reach(cfg, ub.branch.address)
                reach_cache[ub.branch.address] = can_reach

            walked = _walk_to_branch(lifter, code, code_base, visited_pcs,
                                     ub.branch.address, ub.untaken_is_target,
                                     can_reach=can_reach)
            if walked is None:
                stats.unreached += 1
                continue
            constraints, flip = walked

            relevant = _relevant_globals(sg, constraints + [flip])
            if not relevant:
                stats.register_only += 1
                continue

            # Optimize rather than Solver so the model perturbs as few
            # globals as possible: every relevant global gets a soft
            # "keep the value you were observed to hold", and only the
            # ones the path condition forces to change do change.  A plain
            # solver satisfies the constraint with arbitrary values for
            # every global it touched, which injects gratuitous changes and
            # invites divergences that have nothing to do with the branch.
            solver = z3.Optimize()
            solver.set("timeout", timeout_ms)
            for c in constraints:
                solver.add(c)
            solver.add(flip)
            for addr, (sym, size) in relevant.items():
                solver.add_soft(sym == z3.BitVecVal(sg.observed[addr],
                                                    size * 8))
            # Require a memory-involving solution: a model that flips the
            # branch purely through incoming registers is not something the
            # caller is able to inject.
            solver.add(z3.Or([sym != z3.BitVecVal(sg.observed[addr],
                                                  size * 8)
                              for addr, (sym, size) in relevant.items()]))

            result = solver.check()
            if result == z3.unsat:
                stats.unsat += 1
                continue
            if result != z3.sat:
                stats.unknown += 1
                continue

            injection = _model_to_injection(solver.model(), sg, relevant)
            if injection is None:
                stats.unsat += 1
                continue

            stats.sat += 1
            key = tuple(sorted(injection.items()))
            if key not in seen:
                seen.add(key)
                injections.append(injection)

        except LiftError:
            stats.lift_errors += 1
        except Exception:
            # Any lifter/solver surprise falls back to the heuristics; this
            # lane is strictly additive and must never break a run.
            stats.errors += 1

    return injections, stats
