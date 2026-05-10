"""Nearest-neighbor query against the semantic-retrieval index.

At query time the agent has Ghidra pseudocode for an unported target. We
embed that pseudocode and search two columns:
  1. emb_pseudocode — same-modal (best quality when available)
  2. emb_c          — cross-modal fallback (always available)

Results are deduplicated by addr and ranked by the maximum cosine
similarity across both columns. The top-K neighbors are returned with
their full C source, so the agent can use them as worked examples in the
/lift prompt.

Usage as a CLI:
    python3 tools/retrieval/query.py "void FUN_00012345(int a) { ... }" --top 3

Usage as a library:
    from tools.retrieval.query import query_neighbors
    results = query_neighbors(pseudocode_text, top_k=3)
"""

from __future__ import annotations

import argparse
import json
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

# venv shim
_REPO_ROOT = Path(__file__).resolve().parent.parent.parent
_VENV_SP = _REPO_ROOT / ".venv" / "lib" / "python3.12" / "site-packages"
if _VENV_SP.exists() and str(_VENV_SP) not in sys.path:
    sys.path.insert(0, str(_VENV_SP))
sys.path.insert(0, str(_REPO_ROOT))

import numpy as np

from tools.retrieval import db as _db


@dataclass
class Neighbor:
    addr: str
    name: str
    obj_name: str
    source_path: Optional[str]
    decl: str
    c_source: Optional[str]
    pseudocode: Optional[str]
    similarity: float
    match_column: str  # "pseudocode" or "c_source"


def _cosine_topk(
    query_vec: np.ndarray,
    matrix: np.ndarray,
    k: int,
) -> list[tuple[int, float]]:
    """Return top-k (row_index, cosine_similarity) pairs, descending."""
    if matrix.shape[0] == 0:
        return []
    # query_vec and rows are already L2-normalized → cosine = dot product
    sims = matrix @ query_vec
    if k >= len(sims):
        indices = np.argsort(-sims)
    else:
        indices = np.argpartition(-sims, k)[:k]
        indices = indices[np.argsort(-sims[indices])]
    return [(int(i), float(sims[i])) for i in indices]


def query_neighbors(
    pseudocode: str,
    *,
    top_k: int = 3,
    min_similarity: float = 0.3,
) -> list[Neighbor]:
    """Embed `pseudocode` and return the top-K most-similar ported functions.

    Searches both emb_pseudocode and emb_c columns, deduplicates by addr,
    and takes the better score for each function.
    """
    from tools.retrieval.embed import Embedder

    embedder = Embedder()
    q_vec = embedder.embed_one(pseudocode)
    if q_vec is None:
        return []
    q_arr = np.array(q_vec, dtype=np.float32)

    con = _db.connect(read_only=True)
    rows = list(_db.iter_records(con, require_embeddings=True))
    con.close()

    if not rows:
        return []

    # Build matrices (some rows may have only one column embedded)
    pseudo_vecs = []
    c_vecs = []
    for r in rows:
        p = r.get("emb_pseudocode")
        c = r.get("emb_c")
        pseudo_vecs.append(np.array(p, dtype=np.float32) if p else np.zeros(len(q_vec), dtype=np.float32))
        c_vecs.append(np.array(c, dtype=np.float32) if c else np.zeros(len(q_vec), dtype=np.float32))

    pseudo_mat = np.stack(pseudo_vecs)
    c_mat = np.stack(c_vecs)

    # Score each row as max(pseudo_sim, c_sim) — best match across columns
    pseudo_sims = pseudo_mat @ q_arr
    c_sims = c_mat @ q_arr

    # For rows with a null embedding column the vector was zero → sim ~= 0;
    # mask those so they don't suppress the other column.
    has_pseudo = np.array([r.get("emb_pseudocode") is not None for r in rows])
    has_c = np.array([r.get("emb_c") is not None for r in rows])
    pseudo_sims[~has_pseudo] = -1.0
    c_sims[~has_c] = -1.0

    best_sims = np.maximum(pseudo_sims, c_sims)
    best_col = np.where(pseudo_sims >= c_sims, "pseudocode", "c_source")

    if top_k >= len(best_sims):
        indices = np.argsort(-best_sims)
    else:
        indices = np.argpartition(-best_sims, top_k)[:top_k]
        indices = indices[np.argsort(-best_sims[indices])]

    results: list[Neighbor] = []
    for idx in indices:
        sim = float(best_sims[idx])
        if sim < min_similarity:
            break
        r = rows[idx]
        results.append(Neighbor(
            addr=r["addr"],
            name=r["name"],
            obj_name=r.get("obj_name", ""),
            source_path=r.get("source_path"),
            decl=r.get("decl", ""),
            c_source=r.get("c_source"),
            pseudocode=r.get("pseudocode"),
            similarity=sim,
            match_column=str(best_col[idx]),
        ))
    return results[:top_k]


def format_for_prompt(neighbors: list[Neighbor], max_c_lines: int = 40) -> str:
    """Format neighbors as a markdown block ready to inject into a /lift prompt."""
    if not neighbors:
        return ""
    parts = ["## Similar already-ported functions\n"]
    for i, n in enumerate(neighbors, 1):
        parts.append(f"### {i}. {n.name} @ {n.addr} ({n.obj_name})")
        parts.append(f"Decl: `{n.decl}`  ")
        parts.append(f"Similarity: {n.similarity:.3f} (matched on {n.match_column})\n")
        if n.c_source:
            lines = n.c_source.splitlines()
            if len(lines) > max_c_lines:
                lines = lines[:max_c_lines] + [f"// ... ({len(lines) - max_c_lines} more lines)"]
            parts.append("```c")
            parts.append("\n".join(lines))
            parts.append("```\n")
    return "\n".join(parts)


def main() -> int:
    ap = argparse.ArgumentParser(description="Query the retrieval index for similar functions")
    ap.add_argument("pseudocode", nargs="?",
                    help="Pseudocode text to match (or read from --file)")
    ap.add_argument("--file", "-f",
                    help="Read pseudocode from a file instead of argument")
    ap.add_argument("--top", "-k", type=int, default=3)
    ap.add_argument("--min-sim", type=float, default=0.3)
    ap.add_argument("--json", action="store_true", help="Emit JSON output")
    ap.add_argument("--prompt", action="store_true",
                    help="Emit markdown suitable for injection into a /lift prompt")
    args = ap.parse_args()

    if args.file:
        text = Path(args.file).read_text(encoding="utf-8")
    elif args.pseudocode:
        text = args.pseudocode
    else:
        text = sys.stdin.read()

    if not text.strip():
        print("error: empty pseudocode input", file=sys.stderr)
        return 1

    neighbors = query_neighbors(text, top_k=args.top, min_similarity=args.min_sim)

    if args.json:
        out = [
            {
                "addr": n.addr,
                "name": n.name,
                "obj_name": n.obj_name,
                "source_path": n.source_path,
                "decl": n.decl,
                "similarity": round(n.similarity, 4),
                "match_column": n.match_column,
                "c_source_lines": len(n.c_source.splitlines()) if n.c_source else 0,
            }
            for n in neighbors
        ]
        print(json.dumps(out, indent=2))
    elif args.prompt:
        print(format_for_prompt(neighbors))
    else:
        for n in neighbors:
            print(f"{n.similarity:.3f}  {n.name:40s} @ {n.addr}  ({n.obj_name})")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
