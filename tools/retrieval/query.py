"""Nearest-neighbor query against the semantic-retrieval index.

At query time the agent has Ghidra pseudocode for an unported target. We
embed that pseudocode and search two columns:
  1. emb_pseudocode — same-modal (best quality when available)
  2. emb_c          — cross-modal fallback (always available)

Results are deduplicated by addr and ranked by the maximum cosine
similarity across both columns. The top-K neighbors are returned with
their full C source, so the agent can use them as worked examples in the
/lift prompt.

Quality filtering (v2): only return neighbors with proven high VC71 match
(>=85%), prefer same-TU neighbors, require C source.

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
    vc71_score: Optional[float] = None


@dataclass
class HazardWarning:
    source_name: str
    source_addr: str
    similarity: float
    vc71_score: Optional[float]
    verdict: str
    failure_reason: Optional[str]
    hazard_flags: list[str]


SAME_TU_BONUS = 0.05


def query_neighbors(
    pseudocode: str,
    *,
    top_k: int = 3,
    min_similarity: float = 0.5,
    min_vc71_score: float = 85.0,
    prefer_obj_name: str | None = None,
    require_c_source: bool = True,
) -> list[Neighbor]:
    """Embed `pseudocode` and return the top-K most-similar ported functions.

    Filters to proven-good lifts (vc71_score >= min_vc71_score), prefers
    same-TU matches, and requires C source for worked examples.
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

    pseudo_vecs = []
    c_vecs = []
    for r in rows:
        p = r.get("emb_pseudocode")
        c = r.get("emb_c")
        pseudo_vecs.append(np.array(p, dtype=np.float32) if p else np.zeros(len(q_vec), dtype=np.float32))
        c_vecs.append(np.array(c, dtype=np.float32) if c else np.zeros(len(q_vec), dtype=np.float32))

    pseudo_mat = np.stack(pseudo_vecs)
    c_mat = np.stack(c_vecs)

    pseudo_sims = pseudo_mat @ q_arr
    c_sims = c_mat @ q_arr

    has_pseudo = np.array([r.get("emb_pseudocode") is not None for r in rows])
    has_c = np.array([r.get("emb_c") is not None for r in rows])
    pseudo_sims[~has_pseudo] = -1.0
    c_sims[~has_c] = -1.0

    best_sims = np.maximum(pseudo_sims, c_sims)
    best_col = np.where(pseudo_sims >= c_sims, "pseudocode", "c_source")

    # Quality mask: only return proven-good lifts with code
    mask = np.ones(len(rows), dtype=bool)
    for i, r in enumerate(rows):
        score = r.get("vc71_score")
        if score is not None and score < min_vc71_score:
            mask[i] = False
        if require_c_source and not r.get("c_source"):
            mask[i] = False

    best_sims[~mask] = -1.0

    # Same-TU bonus
    if prefer_obj_name:
        for i, r in enumerate(rows):
            if r.get("obj_name") == prefer_obj_name:
                best_sims[i] += SAME_TU_BONUS

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
            vc71_score=r.get("vc71_score"),
        ))
    return results[:top_k]


def query_hazard_warnings(
    pseudocode: str,
    *,
    top_k: int = 5,
    min_similarity: float = 0.4,
) -> list[HazardWarning]:
    """Find functions similar to the target that FAILED or had hazards.

    Returns anti-patterns from similar functions, not code examples.
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

    c_vecs = []
    pseudo_vecs = []
    for r in rows:
        p = r.get("emb_pseudocode")
        c = r.get("emb_c")
        pseudo_vecs.append(np.array(p, dtype=np.float32) if p else np.zeros(len(q_vec), dtype=np.float32))
        c_vecs.append(np.array(c, dtype=np.float32) if c else np.zeros(len(q_vec), dtype=np.float32))

    pseudo_mat = np.stack(pseudo_vecs)
    c_mat = np.stack(c_vecs)

    pseudo_sims = pseudo_mat @ q_arr
    c_sims = c_mat @ q_arr

    has_pseudo = np.array([r.get("emb_pseudocode") is not None for r in rows])
    has_c = np.array([r.get("emb_c") is not None for r in rows])
    pseudo_sims[~has_pseudo] = -1.0
    c_sims[~has_c] = -1.0
    best_sims = np.maximum(pseudo_sims, c_sims)

    # Only keep rows with a failure verdict or hazard flags
    mask = np.zeros(len(rows), dtype=bool)
    for i, r in enumerate(rows):
        verdict = r.get("verdict")
        hazards = r.get("hazard_flags")
        if verdict in ("fail", "reject") or hazards:
            mask[i] = True
    best_sims[~mask] = -1.0

    if top_k >= len(best_sims):
        indices = np.argsort(-best_sims)
    else:
        indices = np.argpartition(-best_sims, top_k)[:top_k]
        indices = indices[np.argsort(-best_sims[indices])]

    results: list[HazardWarning] = []
    for idx in indices:
        sim = float(best_sims[idx])
        if sim < min_similarity:
            break
        r = rows[idx]
        flags_raw = r.get("hazard_flags")
        try:
            flags = json.loads(flags_raw) if flags_raw else []
        except (json.JSONDecodeError, TypeError):
            flags = []
        results.append(HazardWarning(
            source_name=r.get("name", ""),
            source_addr=r.get("addr", ""),
            similarity=sim,
            vc71_score=r.get("vc71_score"),
            verdict=r.get("verdict", ""),
            failure_reason=r.get("failure_reason"),
            hazard_flags=flags,
        ))
    return results[:top_k]


def format_for_prompt(neighbors: list[Neighbor], max_c_lines: int = 40) -> str:
    """Format neighbors as a markdown block ready to inject into a /lift prompt."""
    if not neighbors:
        return ""
    parts = ["## Similar already-ported functions\n"]
    for i, n in enumerate(neighbors, 1):
        parts.append(f"### {i}. {n.name} @ {n.addr} ({n.obj_name})")
        score_info = f"VC71: {n.vc71_score:.1f}%" if n.vc71_score is not None else "VC71: N/A"
        parts.append(f"Decl: `{n.decl}`  ")
        parts.append(f"Similarity: {n.similarity:.3f} (matched on {n.match_column}) | {score_info}\n")
        if n.c_source:
            lines = n.c_source.splitlines()
            if len(lines) > max_c_lines:
                lines = lines[:max_c_lines] + [f"// ... ({len(lines) - max_c_lines} more lines)"]
            parts.append("```c")
            parts.append("\n".join(lines))
            parts.append("```\n")
    return "\n".join(parts)


def format_hazard_warnings(warnings: list[HazardWarning], max_chars: int = 1000) -> str:
    """Format hazard warnings as compact markdown for injection."""
    if not warnings:
        return ""
    parts = ["## Hazard warnings from similar functions\n"]
    for w in warnings:
        score_str = f"{w.vc71_score:.1f}%" if w.vc71_score is not None else "N/A"
        parts.append(
            f"- **{w.source_name}** ({w.similarity:.2f} similar, "
            f"{w.verdict.upper()}, {score_str} VC71)"
        )
        if w.failure_reason:
            reason = w.failure_reason[:150]
            parts.append(f"  Cause: {reason}")
        if w.hazard_flags:
            parts.append(f"  Hazards: {', '.join(w.hazard_flags)}")
    text = "\n".join(parts)
    if len(text) > max_chars:
        text = text[:max_chars - 20] + "\n  ... (truncated)"
    return text


def main() -> int:
    ap = argparse.ArgumentParser(description="Query the retrieval index for similar functions")
    ap.add_argument("pseudocode", nargs="?",
                    help="Pseudocode text to match (or read from --file)")
    ap.add_argument("--file", "-f",
                    help="Read pseudocode from a file instead of argument")
    ap.add_argument("--top", "-k", type=int, default=3)
    ap.add_argument("--min-sim", type=float, default=0.5)
    ap.add_argument("--min-vc71", type=float, default=85.0)
    ap.add_argument("--obj-name", help="Prefer neighbors from this TU")
    ap.add_argument("--hazards", action="store_true", help="Show hazard warnings instead of neighbors")
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

    if args.hazards:
        warnings = query_hazard_warnings(text, top_k=args.top, min_similarity=args.min_sim)
        if args.json:
            out = [
                {
                    "name": w.source_name,
                    "addr": w.source_addr,
                    "similarity": round(w.similarity, 4),
                    "vc71_score": w.vc71_score,
                    "verdict": w.verdict,
                    "hazard_flags": w.hazard_flags,
                }
                for w in warnings
            ]
            print(json.dumps(out, indent=2))
        elif args.prompt:
            print(format_hazard_warnings(warnings))
        else:
            for w in warnings:
                flags = ",".join(w.hazard_flags) if w.hazard_flags else "-"
                print(f"{w.similarity:.3f}  {w.source_name:40s} {w.verdict:6s} hazards={flags}")
        return 0

    neighbors = query_neighbors(
        text, top_k=args.top, min_similarity=args.min_sim,
        min_vc71_score=args.min_vc71, prefer_obj_name=args.obj_name,
    )

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
                "vc71_score": n.vc71_score,
                "c_source_lines": len(n.c_source.splitlines()) if n.c_source else 0,
            }
            for n in neighbors
        ]
        print(json.dumps(out, indent=2))
    elif args.prompt:
        print(format_for_prompt(neighbors))
    else:
        for n in neighbors:
            score = f"{n.vc71_score:.1f}%" if n.vc71_score is not None else "N/A"
            print(f"{n.similarity:.3f}  {n.name:40s} @ {n.addr}  ({n.obj_name})  VC71={score}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
