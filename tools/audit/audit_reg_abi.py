#!/usr/bin/env python3
"""Audit register-argument ABI risks before lifting a function.

This is intentionally conservative. The kb.json declaration is authoritative;
caller disassembly is optional evidence used to catch likely mismatches before
we spend time porting a function.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

_tools_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)


ROOT = Path(__file__).resolve().parent.parent.parent
TOOLS = ROOT / "tools"
sys.path.insert(0, str(TOOLS))

from analysis.knowledge import Function, KnowledgeBase  # noqa: E402


REG32_BITS = {
    "eax": 0, "ecx": 1, "edx": 2, "ebx": 3,
    "esp": 4, "ebp": 5, "esi": 6, "edi": 7,
}
REG_PARENT = {
    "eax": "eax", "ax": "eax", "al": "eax", "ah": "eax",
    "ecx": "ecx", "cx": "ecx", "cl": "ecx", "ch": "ecx",
    "edx": "edx", "dx": "edx", "dl": "edx", "dh": "edx",
    "ebx": "ebx", "bx": "ebx", "bl": "ebx", "bh": "ebx",
    "esi": "esi", "si": "esi",
    "edi": "edi", "di": "edi",
    "ebp": "ebp", "bp": "ebp",
}
SCRATCH_FOR_ARG = ["eax", "ecx", "edx"]
# Max register args the reverse-thunk generator supports (3 scratch slots +
# up to 3 callee-saved slots). See tools/build/patch.py:generate_reverse_thunk.
MAX_REG_ARGS = 6
ARG_REGS = {"eax", "ecx", "edx", "ebx", "esi", "edi", "ebp"}


@dataclass
class CallerEvidence:
    line_no: int
    call_line: str
    written_regs: list[str]
    reg_sources: dict[str, str] = None  # reg -> source classification (param_1, local_N, imm, reg)

    def __post_init__(self):
        if self.reg_sources is None:
            self.reg_sources = {}


# Source classification patterns
_EBP_PARAM_RE = re.compile(r'\[EBP\s*\+\s*(0x[0-9a-fA-F]+|\d+)\]', re.I)
_EBP_LOCAL_RE = re.compile(r'\[EBP\s*-\s*(0x[0-9a-fA-F]+|\d+)\]', re.I)
_IMM_RE        = re.compile(r'^-?(?:0x[0-9a-fA-F]+|\d+)$')


def _classify_reg_source(line: str) -> str:
    """Classify the source of a register write from a single disassembly line."""
    s = strip_disasm_prefix(line)
    # MOV reg, [EBP+N] — stack parameter
    m = _EBP_PARAM_RE.search(s)
    if m:
        offset = int(m.group(1), 16) if m.group(1).startswith('0x') else int(m.group(1))
        if offset >= 8:
            param_n = (offset - 8) // 4 + 1
            return f'param_{param_n}'
        return f'ebp+{offset:#x}'
    # LEA reg, [EBP-N] — local address
    m = _EBP_LOCAL_RE.search(s)
    if m:
        return f'local_addr'
    # Immediate
    if _IMM_RE.search(s.split(',')[-1].strip() if ',' in s else ''):
        return 'imm'
    # Another register (source is the last operand if it's a register name)
    src = s.split(',')[-1].strip().split()[0] if ',' in s else ''
    if src.lower() in REG_PARENT:
        return f'reg:{canon32(src)}'
    return 'other'


def normalize_addr(value: str | int) -> str:
    if isinstance(value, int):
        return hex(value)
    return hex(int(str(value), 0))


def canon32(reg: str) -> str:
    return REG_PARENT.get(reg.lower(), reg.lower())


def parse_target(kb: KnowledgeBase, target: str) -> Function:
    target_l = target.lower()
    try:
        target_addr = normalize_addr(target_l)
    except ValueError:
        target_addr = ""

    for sym in kb.symbols:
        if not isinstance(sym, Function):
            continue
        addr = normalize_addr(sym.addr) if sym.addr is not None else ""
        if sym.name == target or addr == target_addr:
            return sym

    raise ValueError(f"target not found in kb.json: {target}")


def find_staging_cycles(reg_args: list[tuple[int, str]]) -> list[list[int]]:
    moves = []
    for i, (_, src_reg) in enumerate(reg_args):
        if i >= len(SCRATCH_FOR_ARG):
            break
        moves.append((SCRATCH_FOR_ARG[i], canon32(src_reg)))

    edges: dict[int, list[int]] = {i: [] for i in range(len(moves))}
    for i, (dst_i, _) in enumerate(moves):
        for j, (_, src_j) in enumerate(moves):
            if i != j and dst_i == src_j:
                edges[i].append(j)

    cycles: list[list[int]] = []
    seen_keys: set[tuple[int, ...]] = set()

    def visit(start: int, cur: int, path: list[int]) -> None:
        for nxt in edges[cur]:
            if nxt == start and len(path) > 1:
                key = tuple(sorted(path))
                if key not in seen_keys:
                    seen_keys.add(key)
                    cycles.append(path[:])
                continue
            if nxt in path:
                continue
            visit(start, nxt, path + [nxt])

    for i in range(len(moves)):
        visit(i, i, [i])
    return cycles


def target_call_patterns(sym: Function) -> list[re.Pattern[str]]:
    patterns = [re.compile(rf"\bcall\w*\b.*\b{re.escape(sym.name)}\b", re.I)]
    if sym.addr is not None:
        addr = int(sym.addr)
        patterns.extend([
            re.compile(rf"\bcall\w*\b.*\b0x0*{addr:x}\b", re.I),
            re.compile(rf"\bcall\w*\b.*\b0*{addr:x}\b", re.I),
        ])
    return patterns


def strip_disasm_prefix(line: str) -> str:
    # Handles common forms like "001234: MOV ECX, EAX" and
    # "001234  8b c1  mov ecx,eax".
    s = line.strip()
    s = re.sub(r"^[0-9a-fA-F`]+:\s*", "", s)
    s = re.sub(r"^[0-9a-fA-F`]+\s+(?:[0-9a-fA-F]{2}\s+){1,12}", "", s)
    s = re.sub(r"^[0-9a-fA-F`]{4,16}\s+", "", s)
    return s.strip()


def written_register(line: str) -> str | None:
    s = strip_disasm_prefix(line)
    s = s.split(";", 1)[0].strip()
    s = s.split("#", 1)[0].strip()
    if not s:
        return None

    reg_names = "|".join(sorted(REG_PARENT, key=len, reverse=True))
    intel = re.match(
        rf"(?i)\b(?:mov|lea|movzx|movsx|xor|sub|add|and|or|pop)\w*\s+%?({reg_names})\b",
        s,
    )
    if intel:
        return canon32(intel.group(1))

    att = re.match(
        rf"(?i)\b(?:mov|lea|movz|movs|xor|sub|add|and|or)\w*\s+.*,\s*%({reg_names})\b",
        s,
    )
    if att:
        return canon32(att.group(1))

    return None


def load_text(*, text: str, file_path: str, cmd: str) -> str:
    provided = [bool(text), bool(file_path), bool(cmd)]
    if sum(1 for v in provided if v) > 1:
        raise ValueError("pass only one caller disassembly source")
    if text:
        return text
    if file_path:
        path = Path(file_path)
        if not path.is_absolute():
            path = ROOT / path
        return path.read_text(encoding="utf-8", errors="replace")
    if cmd:
        proc = subprocess.run(["bash", "-lc", cmd], cwd=str(ROOT),
                              capture_output=True, text=True)
        if proc.returncode != 0:
            raise RuntimeError(
                f"caller disassembly command failed ({proc.returncode})\n"
                f"stdout:\n{proc.stdout}\nstderr:\n{proc.stderr}"
            )
        return proc.stdout
    return ""


def scan_caller_evidence(sym: Function, disasm: str, window: int) -> list[CallerEvidence]:
    if not disasm.strip():
        return []

    patterns = target_call_patterns(sym)
    lines = disasm.splitlines()
    evidence: list[CallerEvidence] = []
    for i, line in enumerate(lines):
        if not any(p.search(line) for p in patterns):
            continue
        start = max(0, i - window)
        regs: list[str] = []
        seen: set[str] = set()
        reg_sources: dict[str, str] = {}
        for prev in lines[start:i]:
            reg = written_register(prev)
            if reg in ARG_REGS and reg not in seen:
                seen.add(reg)
                regs.append(reg)
                reg_sources[reg] = _classify_reg_source(prev)
        evidence.append(CallerEvidence(i + 1, line.strip(), regs, reg_sources))
    return evidence


def audit(sym: Function, evidence: list[CallerEvidence], strict_callers: bool) -> tuple[int, dict[str, object]]:
    reg_args = sym.register_args
    expected_regs = [canon32(reg) for _, reg in reg_args]
    expected_set = set(expected_regs)
    cycles = find_staging_cycles(reg_args)
    warnings: list[str] = []
    errors: list[str] = []

    if len(reg_args) > MAX_REG_ARGS:
        errors.append(
            f"{sym.name} has {len(reg_args)} register args; reverse thunks support "
            f"only {MAX_REG_ARGS}"
        )
    # Callee-saved slots (4th+ reg args) must be full 32-bit registers — the
    # thunk PUSHes them whole. Sub-32-bit sources (bx, si, di, bh, bl, etc.)
    # are only valid in scratch slots where MOVZX widens them into EAX/ECX/EDX.
    if len(reg_args) > len(SCRATCH_FOR_ARG):
        for slot_i, (param_idx, src_reg) in enumerate(reg_args[len(SCRATCH_FOR_ARG):],
                                                       start=len(SCRATCH_FOR_ARG)):
            if src_reg.lower() not in {"ebx", "esi", "edi", "ebp"}:
                errors.append(
                    f"{sym.name} arg{param_idx} @<{src_reg}> in callee-save slot "
                    f"{slot_i} — must be a full 32-bit register (ebx/esi/edi/ebp)"
                )

    for cycle in cycles:
        parts = []
        for idx in cycle:
            param_idx, src_reg = reg_args[idx]
            parts.append(f"arg{param_idx}@<{src_reg}>->{SCRATCH_FOR_ARG[idx]}")
        warnings.append("register staging cycle: " + " -> ".join(parts))

    for param_idx, src_reg in reg_args:
        if src_reg not in REG32_BITS and canon32(src_reg) in SCRATCH_FOR_ARG:
            warnings.append(
                f"sub-register source arg{param_idx}@<{src_reg}> shares scratch "
                f"parent {canon32(src_reg)}; keep exact thunk coverage"
            )

    caller_reports = []
    for item in evidence:
        written = set(item.written_regs)
        missing = sorted(expected_set - written)
        unexpected = sorted(written - expected_set)

        # §10 register order / value mapping check
        # Build expected reg→param_N mapping from kb.json @<reg> order
        # (position in reg_args list = C argument position)
        order_problems: list[str] = []
        if item.reg_sources and reg_args:
            # expected_order: reg -> param_n (1-indexed C arg)
            expected_order: dict[str, int] = {}
            for c_arg_idx, (param_idx, src_reg) in enumerate(reg_args):
                expected_order[canon32(src_reg)] = param_idx  # param_idx is 1-based

            # For each observed register, check if the source matches the
            # expected param_N for that register
            for reg, source in item.reg_sources.items():
                if reg not in expected_order:
                    continue
                expected_param = expected_order[reg]
                if source.startswith('param_'):
                    observed_param = int(source.split('_')[1])
                    if observed_param != expected_param:
                        # Find which reg is expected to hold observed_param
                        expected_reg_for_observed = next(
                            (canon32(r) for _, (p, r) in enumerate(reg_args) if p == observed_param),
                            None
                        )
                        order_problems.append(
                            f'reg {reg} carries param_{observed_param} but '
                            f'kb.json expects param_{expected_param}; '
                            f'param_{observed_param} should go to '
                            f'{expected_reg_for_observed or "?"}'
                        )

        caller_reports.append({
            "line_no": item.line_no,
            "call_line": item.call_line,
            "written_regs": item.written_regs,
            "reg_sources": item.reg_sources,
            "missing_expected_regs": missing,
            "unexpected_recent_regs": unexpected,
            "reg_order_problems": order_problems,
        })
        for prob in order_problems:
            errors.append(f"§10 REG_ORDER caller line {item.line_no}: {prob}")
        if missing:
            msg = (
                f"caller line {item.line_no}: no recent write observed for "
                f"expected register(s) {', '.join(missing)}"
            )
            if strict_callers:
                errors.append(msg)
            else:
                warnings.append(msg)
        if unexpected:
            warnings.append(
                f"caller line {item.line_no}: recent write(s) to non-KB register(s) "
                f"{', '.join(unexpected)} before call"
            )

    result = {
        "target": sym.name,
        "addr": normalize_addr(sym.addr) if sym.addr is not None else None,
        "decl": sym.decl,
        "register_args": [
            {"param_index": idx, "reg": reg, "parent": canon32(reg)}
            for idx, reg in reg_args
        ],
        "expected_parent_regs": expected_regs,
        "staging_cycles": cycles,
        "caller_evidence": caller_reports,
        "warnings": warnings,
        "errors": errors,
    }
    return (1 if errors else 0), result


def main() -> int:
    parser = argparse.ArgumentParser(description="Audit register-argument ABI risks")
    parser.add_argument("--target", required=True,
                        help="Function name or address from kb.json")
    parser.add_argument("--caller-disasm-text", default="")
    parser.add_argument("--caller-disasm-file", default="")
    parser.add_argument("--caller-disasm-cmd", default="")
    parser.add_argument("--caller-window", type=int, default=12,
                        help="Number of lines before CALL to inspect")
    parser.add_argument("--strict-callers", action="store_true",
                        help="Fail if caller evidence misses expected registers")
    parser.add_argument("--json", action="store_true",
                        help="Print machine-readable JSON")
    args = parser.parse_args()

    logging_configured = bool(os.environ.get("HALO_VERBOSE_KB"))
    if not logging_configured:
        import logging
        logging.basicConfig(level=logging.ERROR)

    try:
        kb = KnowledgeBase.deserialize()
        sym = parse_target(kb, args.target)
        disasm = load_text(
            text=args.caller_disasm_text,
            file_path=args.caller_disasm_file,
            cmd=args.caller_disasm_cmd,
        )
        evidence = scan_caller_evidence(sym, disasm, args.caller_window)
        rc, result = audit(sym, evidence, args.strict_callers)
    except Exception as exc:
        print(f"abi-audit: FAIL ({exc})", file=sys.stderr)
        return 1

    if args.json:
        print(json.dumps(result, indent=2))
    else:
        reg_text = ", ".join(
            f"arg{item['param_index']}@<{item['reg']}>" for item in result["register_args"]
        ) or "none"
        print(f"abi-audit: {result['target']} {result['addr']} regs={reg_text}")
        for warning in result["warnings"]:
            print(f"warning: {warning}")
        for error in result["errors"]:
            print(f"error: {error}", file=sys.stderr)
        if not result["warnings"] and not result["errors"]:
            print("abi-audit: no register-ABI hazards detected")

    return rc


if __name__ == "__main__":
    raise SystemExit(main())
