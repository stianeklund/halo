#!/usr/bin/env python3
"""Structural object diff helper used by lift_pipeline.py.

Runs objdiff-cli between a Ghidra-delinked reference object and the clang-built
candidate object, extracts mnemonic sequences, and computes an LCS match %.

Caching: results are keyed by (ref_sha256, cand_sha256, comparator_version) in
an SQLite database at artifacts/verify_cache/objdiff.sqlite.  A cache hit skips
the objdiff subprocess entirely — useful for repeated pipeline runs on unchanged
objects.  Use --no-cache to force recomputation.
"""

import sys, os
_tools_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)

import argparse
import hashlib
import json
import os
import shutil
import sqlite3
import subprocess
import sys
import tempfile
import time
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
KB_JSON = REPO_ROOT / "kb.json"

CACHE_DIR = REPO_ROOT / "artifacts" / "verify_cache"
OBJDIFF_CACHE_DB = CACHE_DIR / "objdiff.sqlite"
OBJDIFF_COMPARATOR_VERSION = "1"  # bump when extract_mnemonic_seqs / compute_match_pct changes


def _sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def _open_cache_db() -> sqlite3.Connection:
    CACHE_DIR.mkdir(parents=True, exist_ok=True)
    conn = sqlite3.connect(str(OBJDIFF_CACHE_DB))
    conn.execute(
        "CREATE TABLE IF NOT EXISTS objdiff_cache ("
        "  ref_sha256 TEXT NOT NULL,"
        "  cand_sha256 TEXT NOT NULL,"
        "  comparator_version TEXT NOT NULL,"
        "  match_pct REAL,"
        "  ref_insns INTEGER,"
        "  cand_insns INTEGER,"
        "  created_utc INTEGER NOT NULL,"
        "  PRIMARY KEY (ref_sha256, cand_sha256, comparator_version)"
        ")"
    )
    conn.commit()
    return conn


def _cache_lookup(conn: sqlite3.Connection, ref_sha: str, cand_sha: str) -> dict | None:
    row = conn.execute(
        "SELECT match_pct, ref_insns, cand_insns FROM objdiff_cache "
        "WHERE ref_sha256=? AND cand_sha256=? AND comparator_version=?",
        (ref_sha, cand_sha, OBJDIFF_COMPARATOR_VERSION),
    ).fetchone()
    if row:
        return {"match_pct": row[0], "ref_insns": row[1], "cand_insns": row[2]}
    return None


def _cache_store(conn: sqlite3.Connection, ref_sha: str, cand_sha: str,
                 match_pct, ref_insns: int, cand_insns: int):
    conn.execute(
        "INSERT OR REPLACE INTO objdiff_cache VALUES (?,?,?,?,?,?,?)",
        (ref_sha, cand_sha, OBJDIFF_COMPARATOR_VERSION,
         match_pct, ref_insns, cand_insns, int(time.time())),
    )
    conn.commit()


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
    ap.add_argument('--no-cache', action='store_true',
                    help='Skip cache lookup and force a fresh objdiff run')
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

    target = args.target or os.path.splitext(os.path.basename(candidate))[0]
    ref_sym, cand_sym = resolve_symbol_pair(args.symbol) if args.symbol else ('', '')

    # --- Cache lookup ---
    match_pct = None
    left_mnems = right_mnems = None
    cache_hit = False
    conn = None
    if not getattr(args, 'no_cache', False):
        try:
            conn = _open_cache_db()
            ref_sha = _sha256_file(reference)
            cand_sha = _sha256_file(candidate)
            cached = _cache_lookup(conn, ref_sha, cand_sha)
            if cached is not None:
                match_pct = cached["match_pct"]
                n_ref = cached["ref_insns"] or 0
                n_cand = cached["cand_insns"] or 0
                cache_hit = True
                print(f"  target: {target}")
                print(f"  [objdiff cache HIT] match: {f'{match_pct:.1f}%' if match_pct is not None else 'N/A'}"
                      f"  insns: {n_ref} ref / {n_cand} compiled")
        except Exception:
            conn = None  # cache unavailable, fall through to fresh run

    if not cache_hit:
        tool = shutil.which(args.tool) if os.path.sep not in args.tool else args.tool
        if not tool:
            print(f'objdiff tool not found: {args.tool}', file=sys.stderr)
            raise SystemExit(2)

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

        # Store in cache
        if conn and match_pct is not None:
            try:
                ref_sha_fresh = _sha256_file(reference)
                cand_sha_fresh = _sha256_file(candidate)
                _cache_store(conn, ref_sha_fresh, cand_sha_fresh,
                             match_pct,
                             len(left_mnems) if left_mnems else 0,
                             len(right_mnems) if right_mnems else 0)
            except Exception:
                pass

    if not cache_hit:
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

        exit_code = result.returncode

        if args.output_json:
            payload = {
                'target': target,
                'reference': reference,
                'candidate': candidate,
                'ref_symbol': ref_sym,
                'cand_symbol': cand_sym,
                'match_pct': match_pct,
                'exit_code': exit_code,
                'cache_hit': False,
            }
            with open(args.output_json, 'w') as f:
                json.dump(payload, f, indent=2)
                f.write('\n')

        if match_pct is not None and match_pct < 50.0:
            raise SystemExit(1)
        if exit_code != 0 and match_pct is None:
            raise SystemExit(exit_code)
    else:
        if args.output_json:
            payload = {
                'target': target,
                'reference': reference,
                'candidate': candidate,
                'ref_symbol': ref_sym,
                'cand_symbol': cand_sym,
                'match_pct': match_pct,
                'cache_hit': True,
            }
            with open(args.output_json, 'w') as f:
                json.dump(payload, f, indent=2)
                f.write('\n')
        if match_pct is not None and match_pct < 50.0:
            raise SystemExit(1)


if __name__ == '__main__':
    main()
