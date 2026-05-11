#!/usr/bin/env python3
import sys, os
_tools_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
KB_JSON = REPO_ROOT / "kb.json"


def parse_args() -> argparse.Namespace:
    ap = argparse.ArgumentParser(
        description='Run objdiff between a delinked reference object and a compiled candidate object.'
    )
    ap.add_argument('--target', default='',
                    help='Human-readable target name for reports')
    ap.add_argument('--reference', required=True,
                    help='Path to delinked reference object exported from Ghidra')
    ap.add_argument('--candidate', required=True,
                    help='Path to compiled candidate object from the Halo build')
    ap.add_argument('--symbol', '-s', default='',
                    help='Function symbol to diff (e.g. vector_to_angles or FUN_0010cc00)')
    ap.add_argument('--output-json', default='',
                    help='Optional path to save a small JSON summary')
    ap.add_argument('--output-text', default='',
                    help='Optional path to save stdout from objdiff')
    repo_root = str(REPO_ROOT)
    default_tool = os.path.join(repo_root, 'tools', 'objdiff-cli-linux-x86_64')
    if not os.path.isfile(default_tool):
        default_tool = 'objdiff'
    ap.add_argument('--tool', default=default_tool,
                    help='objdiff executable name or path')
    return ap.parse_args()


def resolve_symbol_pair(name: str) -> tuple[str, str]:
    """Resolve a function name to (reference_sym, candidate_sym) via kb.json.

    Returns (fun_name, declared_name) where fun_name is the FUN_XXXXXXXX form
    found in delinked references and declared_name is the C name in compiled objects.
    If the input is already a FUN_ name, swap the return order.
    """
    try:
        with open(KB_JSON) as f:
            kb = json.load(f)
    except (OSError, json.JSONDecodeError):
        return (name, name)

    import re
    m = re.search(r'\b(\w+)\s*\(', name)
    bare = m.group(1) if m else name

    for obj in kb.get('objects', []):
        for entry in obj.get('functions', []):
            addr = entry.get('addr', '')
            decl = entry.get('decl', '')
            dm = re.search(r'\b(\w+)\s*\(', decl)
            if not (addr and dm):
                continue
            declared = dm.group(1)
            fun_name = f"FUN_{int(addr, 16):08x}"
            if bare == declared:
                return (fun_name, declared)
            if bare == fun_name:
                return (fun_name, declared)
    return (name, name)


def extract_mnemonic_seqs(objdiff_json: dict) -> dict[str, list[str]]:
    """Extract {symbol: [mnemonic, ...]} from objdiff JSON output."""
    result = {}
    for side in ('left', 'right'):
        for sym in objdiff_json.get(side, {}).get('symbols', []):
            name = sym.get('name', '').lstrip('_')
            mnems = []
            for insn_obj in sym.get('instructions', []):
                insn = insn_obj.get('instruction', {})
                parts = insn.get('parts', [])
                for part in parts:
                    op = part.get('opcode')
                    if op and 'mnemonic' in op:
                        mnems.append(op['mnemonic'])
                        break
            if mnems:
                result[name] = mnems
    return result


def compute_match_pct(left: list[str], right: list[str]) -> float:
    """LCS-based match percentage between two mnemonic sequences."""
    from difflib import SequenceMatcher
    if not left and not right:
        return 100.0
    if not left or not right:
        return 0.0
    return SequenceMatcher(None, left, right, autojunk=False).ratio() * 100


def main():
    args = parse_args()
    reference = os.path.abspath(args.reference)
    candidate = os.path.abspath(args.candidate)

    for path, label in [(reference, 'Reference object'), (candidate, 'Candidate object')]:
        if not os.path.exists(path):
            print(f'{label} does not exist: {path}', file=sys.stderr)
            raise SystemExit(2)

    tool = shutil.which(args.tool) if os.path.sep not in args.tool else args.tool
    if not tool:
        print(f'objdiff tool not found: {args.tool}', file=sys.stderr)
        raise SystemExit(2)

    target = args.target or os.path.splitext(os.path.basename(candidate))[0]
    ref_sym, cand_sym = resolve_symbol_pair(args.symbol) if args.symbol else ('', '')

    tmp_out = tempfile.mktemp(suffix='.json')
    command = [
        tool, "diff",
        "-1", reference,
        "-2", candidate,
        "--format", "json-pretty",
        "-o", tmp_out,
    ]
    if ref_sym:
        command.append(ref_sym)

    result = subprocess.run(command, capture_output=True, text=True)

    stderr_lines = [l for l in (result.stderr or '').splitlines()
                    if not l.strip().startswith('[')]
    clean_stderr = '\n'.join(stderr_lines).strip()

    match_pct = None
    left_mnems = right_mnems = None
    if os.path.exists(tmp_out) and os.path.getsize(tmp_out) > 0:
        try:
            with open(tmp_out) as f:
                data = json.load(f)
            seqs = extract_mnemonic_seqs(data)
            if ref_sym in seqs and cand_sym in seqs:
                left_mnems = seqs[ref_sym]
                right_mnems = seqs[cand_sym]
                match_pct = compute_match_pct(left_mnems, right_mnems)
        except (json.JSONDecodeError, OSError):
            pass
        finally:
            try:
                os.unlink(tmp_out)
            except OSError:
                pass

    match_str = f"{match_pct:.1f}%" if match_pct is not None else "N/A"
    print(f"  target: {target}")
    print(f"  reference: {reference} ({ref_sym})")
    print(f"  candidate: {candidate} ({cand_sym})")
    print(f"  match: {match_str}")
    if match_pct is not None:
        n_left = len(left_mnems) if left_mnems else 0
        n_right = len(right_mnems) if right_mnems else 0
        print(f"  insns: {n_left} ref / {n_right} compiled")
    if clean_stderr:
        print(f"  stderr: {clean_stderr[:200]}")

    if args.output_json:
        payload = {
            'target': target,
            'reference': reference,
            'candidate': candidate,
            'ref_symbol': ref_sym,
            'cand_symbol': cand_sym,
            'match_pct': match_pct,
            'exit_code': result.returncode,
        }
        with open(args.output_json, 'w') as f:
            json.dump(payload, f, indent=2)
            f.write('\n')

    if match_pct is not None and match_pct < 50.0:
        raise SystemExit(1)
    if result.returncode != 0 and match_pct is None:
        raise SystemExit(result.returncode)


if __name__ == '__main__':
    main()
