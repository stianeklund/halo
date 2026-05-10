#!/usr/bin/env python3
"""Build / refresh the semantic-retrieval index.

Commit 1: extract pseudocode + C source for every ported function in
kb.json and persist to DuckDB. Embeddings are populated separately by
`tools/retrieval/embed.py` (commit 2).

Usage:
    python3 tools/retrieval/build_index.py extract           # populate text
    python3 tools/retrieval/build_index.py stats             # show counts
    python3 tools/retrieval/build_index.py show <addr|name>  # dump one row
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

# venv shim — duckdb / numpy live in .venv
_REPO_ROOT = Path(__file__).resolve().parent.parent.parent
_VENV_SP = _REPO_ROOT / ".venv" / "lib" / "python3.12" / "site-packages"
if _VENV_SP.exists() and str(_VENV_SP) not in sys.path:
    sys.path.insert(0, str(_VENV_SP))

# Allow `python3 tools/retrieval/build_index.py` to import siblings as
# `tools.retrieval.*` regardless of cwd.
sys.path.insert(0, str(_REPO_ROOT))

from tools.retrieval import db as _db
from tools.retrieval import extract as _extract


def cmd_extract(args: argparse.Namespace) -> int:
    con = _db.connect()
    n_total = 0
    n_with_pseudo = 0
    n_with_c = 0
    for rec in _extract.iter_ported_records():
        n_total += 1
        if rec.pseudocode:
            n_with_pseudo += 1
        if rec.c_source:
            n_with_c += 1
        _db.upsert_record(con, _extract.to_db_dict(rec))
    con.close()
    print(f"extract: {n_total} ported functions written")
    print(f"  with pseudocode: {n_with_pseudo}")
    print(f"  with c_source  : {n_with_c}")
    return 0


def cmd_stats(args: argparse.Namespace) -> int:
    con = _db.connect(read_only=True)
    s = _db.stats(con)
    con.close()
    print(f"index: {_db.DB_PATH}")
    print(f"  total rows         : {s['total']}")
    print(f"  with pseudocode    : {s['with_pseudocode']}")
    print(f"  with c_source      : {s['with_c']}")
    print(f"  with embeddings    : {s['with_emb']}")
    print(f"  distinct objects   : {s['objects']}")
    print(f"  embedding models   : {s['models']}")
    return 0


def cmd_show(args: argparse.Namespace) -> int:
    target = args.target
    con = _db.connect(read_only=True)
    if target.startswith("0x") or target.lstrip("-").isdigit():
        norm = hex(int(target, 0)).lower()
        row = con.execute(
            "SELECT * FROM functions WHERE addr = ?", [norm]
        ).fetchone()
    else:
        row = con.execute(
            "SELECT * FROM functions WHERE name = ?", [target]
        ).fetchone()

    if not row:
        print(f"no record for {target}", file=sys.stderr)
        con.close()
        return 1

    cols = [d[0] for d in con.description]
    rec = dict(zip(cols, row))
    con.close()

    short_keys = ("addr", "name", "obj_name", "source_path",
                  "pseudocode_sha", "c_source_sha", "indexed_at", "emb_model")
    for k in short_keys:
        print(f"{k}: {rec.get(k)}")
    print(f"decl: {rec.get('decl')}")
    print(f"pseudocode: {len(rec['pseudocode']) if rec.get('pseudocode') else 0} chars")
    print(f"c_source  : {len(rec['c_source']) if rec.get('c_source') else 0} chars")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description="Build / inspect the semantic-retrieval index")
    sub = ap.add_subparsers(dest="cmd", required=True)

    sp_e = sub.add_parser("extract", help="Populate the index with text signals from kb.json + sources")
    sp_e.set_defaults(func=cmd_extract)

    sp_s = sub.add_parser("stats", help="Print row counts and coverage")
    sp_s.set_defaults(func=cmd_stats)

    sp_v = sub.add_parser("show", help="Dump a single record")
    sp_v.add_argument("target", help="addr (0x...) or function name")
    sp_v.set_defaults(func=cmd_show)

    args = ap.parse_args()
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
