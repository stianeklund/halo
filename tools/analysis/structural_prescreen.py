#!/usr/bin/env python3
"""Structural pre-screener for auto-lift target selection.

Analyzes delinked reference .obj files to predict VC71 match difficulty
BEFORE spending tokens on a full lift. Extracts instruction-level features
that correlate with low match percentages.

Usage:
    python3 tools/analysis/structural_prescreen.py                    # screen all delinked refs
    python3 tools/analysis/structural_prescreen.py --function FUN_X   # screen one function
    python3 tools/analysis/structural_prescreen.py --json             # machine-readable output
    python3 tools/analysis/structural_prescreen.py --validate         # backtest against known failures
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Optional

ROOT = Path(__file__).resolve().parent.parent.parent
DELINKED_DIR = ROOT / "delinked"
OBJDIFF_JSON = ROOT / "objdiff.json"
KB_JSON = ROOT / "kb.json"

_CALL_TARGET_RE = re.compile(r"calll?\s+.*?<([A-Za-z_][A-Za-z0-9_@$.]*)")
FAILURES_DIR = ROOT / "artifacts" / "auto_lift" / "failures"

FPU_MNEMONICS = frozenset({
    'fld', 'flds', 'fldl', 'fldt', 'fild', 'fst', 'fstp', 'fsts', 'fstps',
    'fmul', 'fmuls', 'fmulp', 'fdiv', 'fdivs', 'fdivp', 'fdivr', 'fdivrs', 'fdivrp',
    'fadd', 'fadds', 'faddp', 'fsub', 'fsubs', 'fsubp', 'fsubr', 'fsubrs', 'fsubrp',
    'fabs', 'fchs', 'fsqrt', 'fsin', 'fcos', 'fxch', 'fcom', 'fcomp', 'fcompp',
    'fucom', 'fucomp', 'fucompp', 'fnstsw', 'fnstcw', 'fldcw',
})


@dataclass
class FunctionFeatures:
    name: str
    object_name: str
    addr: str
    insn_count: int
    fpu_count: int
    fpu_pct: float
    call_count: int
    branch_count: int
    param_slot_reuse_count: int
    max_fpu_block_len: int
    fpu_block_count: int
    difficulty: str            # "easy", "medium", "hard", "reject"
    difficulty_score: int      # 0-100, higher = harder
    risk_factors: list[str] = field(default_factory=list)


def disassemble_obj(obj_path: str) -> dict[str, list[str]]:
    """Disassemble a COFF object, return {symbol: [instruction_lines]}."""
    result = subprocess.run(
        ["llvm-objdump", "-d", "--no-show-raw-insn", "--no-leading-addr", obj_path],
        capture_output=True, text=True,
    )
    if result.returncode != 0:
        return {}

    functions: dict[str, list[str]] = {}
    current_func: Optional[str] = None
    current_lines: list[str] = []

    for raw_line in result.stdout.splitlines():
        line = raw_line.rstrip()
        m = re.match(r'^(?:[0-9a-f]+ )?<([^>]+)>:', line)
        if m:
            sym = m.group(1)
            if sym.startswith("LAB_") or sym.startswith("switch"):
                continue
            if current_func and current_lines:
                functions[current_func] = current_lines
            current_func = sym.lstrip("_")
            current_lines = []
            continue
        stripped = line.strip()
        if not stripped or stripped.startswith("..."):
            continue
        parts = stripped.split(None, 1)
        if parts:
            insn = stripped
            if re.match(r'^[0-9a-f]+:', parts[0]):
                insn = parts[1] if len(parts) > 1 else ""
            if insn:
                current_lines.append(insn)

    if current_func and current_lines:
        functions[current_func] = current_lines

    for fn in functions:
        lines = functions[fn]
        while lines and lines[-1].strip().split()[0].startswith("nop"):
            lines.pop()

    return functions


def _count_param_slot_reuse(insns: list[str]) -> int:
    """Count stores to EBP+positive offsets (parameter slots reused as locals).

    MSVC optimizes by reusing parameter stack slots for intermediate values
    after the parameter is consumed. Clang doesn't do this, causing structural
    divergence that can't be expressed in C89.

    In AT&T syntax: ``movl %reg, 0xN(%ebp)`` is a store (memory is last
    operand). ``fstps 0xN(%ebp)`` is always a store.
    """
    count = 0
    for insn in insns:
        stripped = insn.strip()
        mnem = stripped.split()[0].lower()
        if mnem.startswith("fst"):
            m = re.search(r'0x([0-9a-f]+)\(%ebp\)', stripped)
            if m:
                offset = int(m.group(1), 16)
                if 0x8 <= offset <= 0x40:
                    count += 1
        elif mnem.startswith("mov"):
            m = re.match(r'mov\w*\s+.+,\s*0x([0-9a-f]+)\(%ebp\)', stripped)
            if m:
                offset = int(m.group(1), 16)
                if 0x8 <= offset <= 0x40:
                    count += 1
    return count


def _extract_fpu_blocks(insns: list[str]) -> list[int]:
    """Return lengths of contiguous FPU instruction blocks (>= 3 insns)."""
    blocks = []
    run = 0
    for insn in insns:
        m = insn.split()[0].lower()
        if m in FPU_MNEMONICS:
            run += 1
        else:
            if run >= 3:
                blocks.append(run)
            run = 0
    if run >= 3:
        blocks.append(run)
    return blocks


def _load_callee_reg_arg_names() -> frozenset[str]:
    """Return the set of function names in kb.json that have register args."""
    try:
        kb = json.loads(KB_JSON.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return frozenset()
    reg_arg_re = re.compile(r"@<\w+>")
    names: set[str] = set()
    for obj in kb.get("objects", []):
        for func in obj.get("functions", []):
            decl = func.get("decl", "")
            if not reg_arg_re.search(decl):
                continue
            m = re.search(r"(\w+)\s*\(", re.sub(r"@<\w+>", "", decl))
            if m:
                names.add(m.group(1))
    return frozenset(names)


def _extract_call_targets(insns: list[str]) -> list[str]:
    """Extract callee function names from call instructions."""
    targets = []
    for insn in insns:
        if not insn.lstrip().startswith(("call", "calll")):
            continue
        m = _CALL_TARGET_RE.search(insn)
        if m:
            targets.append(m.group(1))
    return targets


def analyze_function(
    name: str,
    insns: list[str],
    object_name: str = "",
    addr: str = "",
    callee_reg_arg_names: frozenset[str] = frozenset(),
) -> FunctionFeatures:
    """Extract structural features from a disassembled function."""
    mnemonics = [i.split()[0].lower() for i in insns]
    n = len(mnemonics)

    fpu_count = sum(1 for m in mnemonics if m in FPU_MNEMONICS)
    fpu_pct = (fpu_count / n * 100) if n else 0.0
    call_count = sum(1 for m in mnemonics if m in ("calll", "call"))
    branch_count = sum(1 for m in mnemonics if m.startswith("j"))
    param_reuse = _count_param_slot_reuse(insns)
    fpu_blocks = _extract_fpu_blocks(insns)
    max_fpu_block = max(fpu_blocks) if fpu_blocks else 0

    risk_factors = []
    difficulty_score = 0

    if n > 500:
        difficulty_score += 30
        risk_factors.append(f"very_large_func({n})")
    elif n > 200:
        difficulty_score += 15
        risk_factors.append(f"large_func({n})")
    elif n > 100:
        difficulty_score += 5

    if fpu_pct > 30:
        difficulty_score += 25
        risk_factors.append(f"high_fpu({fpu_pct:.0f}%)")
    elif fpu_pct > 20:
        difficulty_score += 15
        risk_factors.append(f"moderate_fpu({fpu_pct:.0f}%)")
    elif fpu_pct > 10:
        difficulty_score += 5

    if param_reuse > 5:
        difficulty_score += 20
        risk_factors.append(f"param_slot_reuse({param_reuse})")
    elif param_reuse > 2:
        difficulty_score += 10
        risk_factors.append(f"some_param_reuse({param_reuse})")

    if max_fpu_block > 15:
        difficulty_score += 15
        risk_factors.append(f"long_fpu_block({max_fpu_block})")
    elif max_fpu_block > 8:
        difficulty_score += 8
        risk_factors.append(f"fpu_block({max_fpu_block})")

    fpu_x_branch = (fpu_pct / 100) * branch_count
    if fpu_x_branch > 15:
        difficulty_score += 10
        risk_factors.append(f"fpu_x_branch({fpu_x_branch:.0f})")

    if callee_reg_arg_names:
        call_targets = _extract_call_targets(insns)
        reg_callees = [t for t in call_targets if t in callee_reg_arg_names]
        if reg_callees:
            n_reg = len(set(reg_callees))
            if n_reg >= 3:
                difficulty_score += 20
                risk_factors.append(f"callee_reg_args({n_reg})")
            else:
                difficulty_score += 10
                risk_factors.append(f"callee_reg_args({n_reg})")

    if difficulty_score >= 60:
        difficulty = "reject"
    elif difficulty_score >= 35:
        difficulty = "hard"
    elif difficulty_score >= 15:
        difficulty = "medium"
    else:
        difficulty = "easy"

    return FunctionFeatures(
        name=name,
        object_name=object_name,
        addr=addr,
        insn_count=n,
        fpu_count=fpu_count,
        fpu_pct=round(fpu_pct, 1),
        call_count=call_count,
        branch_count=branch_count,
        param_slot_reuse_count=param_reuse,
        max_fpu_block_len=max_fpu_block,
        fpu_block_count=len(fpu_blocks),
        difficulty=difficulty,
        difficulty_score=difficulty_score,
        risk_factors=risk_factors,
    )


def load_unported_functions() -> dict[str, dict]:
    """Load unported FUN_ functions from kb.json, keyed by name."""
    kb = json.loads(KB_JSON.read_text(encoding="utf-8"))
    result = {}
    for obj in kb.get("objects", []):
        obj_name = obj.get("name", "")
        source = obj.get("source", "")
        for func in obj.get("functions", []):
            if func.get("ported"):
                continue
            decl = func.get("decl", "")
            m = re.search(r"(\w+)\s*\(", re.sub(r"@<\w+>", "", decl))
            if not m:
                continue
            name = m.group(1)
            addr = func.get("addr", "")
            result[name] = {
                "addr": addr,
                "object_name": obj_name,
                "source": source,
            }
    return result


def load_objdiff_map() -> dict[str, str]:
    """Map source paths to delinked .obj paths."""
    if not OBJDIFF_JSON.exists():
        return {}
    data = json.loads(OBJDIFF_JSON.read_text(encoding="utf-8"))
    result = {}
    for u in data.get("units", []):
        src = u.get("metadata", {}).get("source_path", "")
        base = u.get("base_path", "")
        if src and base:
            full_base = str(ROOT / base)
            if os.path.exists(full_base):
                result[src] = full_base
    return result


def screen_all(function_filter: Optional[str] = None) -> list[FunctionFeatures]:
    """Screen all unported functions that have delinked references."""
    unported = load_unported_functions()
    objdiff = load_objdiff_map()
    callee_reg_arg_names = _load_callee_reg_arg_names()

    source_to_funcs: dict[str, list[str]] = {}
    for name, info in unported.items():
        src = info["source"]
        if not src:
            continue
        source_path = f"src/halo/{src}" if not src.startswith("src/") else src
        source_to_funcs.setdefault(source_path, []).append(name)

    disasm_cache: dict[str, dict[str, list[str]]] = {}
    results = []

    for source_path, func_names in source_to_funcs.items():
        obj_path = objdiff.get(source_path)
        if not obj_path:
            continue

        if obj_path not in disasm_cache:
            disasm_cache[obj_path] = disassemble_obj(obj_path)
        funcs = disasm_cache[obj_path]

        for name in func_names:
            if function_filter and name != function_filter:
                continue
            insns = funcs.get(name)

            # Function not in TU obj (split TU) — try per-function delinked ref
            if not insns:
                info = unported[name]
                addr = info.get("addr", "")
                if addr:
                    try:
                        addr_hex = f"{int(addr, 16):08x}"
                        per_func = str(DELINKED_DIR / "functions" / f"{addr_hex}.obj")
                        if per_func not in disasm_cache:
                            disasm_cache[per_func] = disassemble_obj(per_func)
                        insns = disasm_cache[per_func].get(name)
                    except (ValueError, OSError):
                        pass

            if not insns:
                continue
            info = unported[name]
            features = analyze_function(
                name, insns,
                object_name=info["object_name"],
                addr=info["addr"],
                callee_reg_arg_names=callee_reg_arg_names,
            )
            results.append(features)

    results.sort(key=lambda f: f.difficulty_score)
    return results


def validate_against_failures(results: list[FunctionFeatures]) -> None:
    """Backtest: check if known failures would have been flagged."""
    if not FAILURES_DIR.exists():
        print("No failure records found.")
        return

    failures = {}
    for f in FAILURES_DIR.glob("*.json"):
        data = json.loads(f.read_text())
        failures[data["target"]] = data

    screened = {r.name: r for r in results}

    print("\n=== Validation against known failures ===\n")
    for fname, fdata in sorted(failures.items()):
        if fname in screened:
            feat = screened[fname]
            caught = feat.difficulty in ("hard", "reject")
            status = "CAUGHT" if caught else "MISSED"
            print(f"  {status} {fname}: difficulty={feat.difficulty} "
                  f"(score={feat.difficulty_score}) "
                  f"risks=[{', '.join(feat.risk_factors)}]")
            if not caught:
                print(f"    ^ Would NOT have been filtered. "
                      f"Failure was: {fdata['attempts'][-1].get('error_summary', '')[:80]}")
        else:
            print(f"  N/A  {fname}: not in delinked references (can't prescreen)")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--function", "-f", help="Screen a specific function")
    ap.add_argument("--json", action="store_true", help="JSON output")
    ap.add_argument("--validate", action="store_true",
                    help="Backtest against known auto-lift failures")
    ap.add_argument("--difficulty", "-d", choices=["easy", "medium", "hard", "reject"],
                    help="Filter by difficulty level")
    ap.add_argument("--max-difficulty-score", type=int, default=None,
                    help="Only show functions at or below this difficulty score")
    args = ap.parse_args()

    results = screen_all(function_filter=args.function)

    if args.difficulty:
        results = [r for r in results if r.difficulty == args.difficulty]
    if args.max_difficulty_score is not None:
        results = [r for r in results if r.difficulty_score <= args.max_difficulty_score]

    if args.validate:
        validate_against_failures(results)
        return

    if args.json:
        print(json.dumps([asdict(r) for r in results], indent=2))
        return

    print(f"{'Score':>5}  {'Diff':<7}  {'Insns':>5}  {'FPU%':>5}  "
          f"{'Calls':>5}  {'Br':>4}  {'PSlot':>5}  {'FBlk':>5}  "
          f"{'Object':<30}  {'Name':<30}  {'Risk Factors'}")
    print("-" * 160)
    for r in results:
        risks = ", ".join(r.risk_factors) if r.risk_factors else "-"
        print(f"{r.difficulty_score:>5}  {r.difficulty:<7}  {r.insn_count:>5}  "
              f"{r.fpu_pct:>4.0f}%  {r.call_count:>5}  {r.branch_count:>4}  "
              f"{r.param_slot_reuse_count:>5}  {r.max_fpu_block_len:>5}  "
              f"{r.object_name:<30}  {r.name:<30}  {risks}")

    easy = sum(1 for r in results if r.difficulty == "easy")
    medium = sum(1 for r in results if r.difficulty == "medium")
    hard = sum(1 for r in results if r.difficulty == "hard")
    reject = sum(1 for r in results if r.difficulty == "reject")
    print(f"\n{len(results)} functions screened: "
          f"{easy} easy, {medium} medium, {hard} hard, {reject} reject")


if __name__ == "__main__":
    main()
