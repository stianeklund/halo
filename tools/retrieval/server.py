#!/usr/bin/env python3
"""Persistent retrieval query server.

Loads the embedding model and index once, then serves queries over a
Unix socket at /tmp/retrieval_server.sock.

Start before a dev session:
    python3 tools/retrieval/server.py &

The decompile_hook.py connects to this socket instead of spawning a new
subprocess per query, reducing per-query latency from ~90s to ~1-2s.

Protocol: newline-terminated JSON lines.
  Request:  {"text": "<pseudocode>", "top_k": 3}
  Response: {"ok": true, "markdown": "..."} or {"ok": false, "error": "..."}
"""

from __future__ import annotations

import json
import os
import signal
import socket
import sys
import threading
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
SOCK_PATH = Path("/tmp/retrieval_server.sock")
PID_PATH = Path("/tmp/retrieval_server.pid")

# venv shim
_VENV_SP = REPO_ROOT / ".venv" / "lib" / "python3.12" / "site-packages"
if _VENV_SP.exists() and str(_VENV_SP) not in sys.path:
    sys.path.insert(0, str(_VENV_SP))
sys.path.insert(0, str(REPO_ROOT))


def _load() -> tuple:
    """Load embedder + index rows once."""
    import numpy as np
    from tools.retrieval.embed import Embedder
    from tools.retrieval import db as _db

    print("[server] loading embedder...", flush=True)
    embedder = Embedder()

    print("[server] loading index...", flush=True)
    con = _db.connect(read_only=True)
    rows = list(_db.iter_records(con, require_embeddings=True))
    con.close()

    # Pre-build numpy matrices
    if rows:
        dim = len(rows[0].get("emb_c") or rows[0].get("emb_pseudocode"))
        pseudo_vecs = []
        c_vecs = []
        has_pseudo = []
        has_c = []
        for r in rows:
            p = r.get("emb_pseudocode")
            c = r.get("emb_c")
            pseudo_vecs.append(np.array(p, dtype=np.float32) if p else np.zeros(dim, dtype=np.float32))
            c_vecs.append(np.array(c, dtype=np.float32) if c else np.zeros(dim, dtype=np.float32))
            has_pseudo.append(p is not None)
            has_c.append(c is not None)
        pseudo_mat = np.stack(pseudo_vecs)
        c_mat = np.stack(c_vecs)
        has_pseudo_arr = np.array(has_pseudo)
        has_c_arr = np.array(has_c)
    else:
        pseudo_mat = c_mat = has_pseudo_arr = has_c_arr = None

    print(f"[server] ready — {len(rows)} rows indexed", flush=True)
    return embedder, rows, pseudo_mat, c_mat, has_pseudo_arr, has_c_arr


def _query(text: str, top_k: int, state: tuple) -> str:
    """Run a query and return formatted markdown."""
    import numpy as np
    from tools.retrieval.query import Neighbor, format_for_prompt

    embedder, rows, pseudo_mat, c_mat, has_pseudo_arr, has_c_arr = state
    if not rows:
        return ""

    q_vec = embedder.embed_one(text)
    if q_vec is None:
        return ""
    q_arr = np.array(q_vec, dtype=np.float32)

    pseudo_sims = pseudo_mat @ q_arr
    c_sims = c_mat @ q_arr
    pseudo_sims[~has_pseudo_arr] = -1.0
    c_sims[~has_c_arr] = -1.0
    best_sims = np.maximum(pseudo_sims, c_sims)
    best_col = np.where(pseudo_sims >= c_sims, "pseudocode", "c_source")

    if top_k >= len(best_sims):
        indices = list(range(len(best_sims)))
        indices.sort(key=lambda i: -best_sims[i])
    else:
        import numpy as np
        indices = np.argpartition(-best_sims, top_k)[:top_k]
        indices = list(indices[np.argsort(-best_sims[indices])])

    MIN_SIM = 0.3
    neighbors = []
    for idx in indices:
        sim = float(best_sims[idx])
        if sim < MIN_SIM:
            break
        r = rows[idx]
        neighbors.append(Neighbor(
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

    return format_for_prompt(neighbors[:top_k])


def _handle(conn: socket.socket, state: tuple) -> None:
    with conn:
        buf = b""
        while True:
            chunk = conn.recv(65536)
            if not chunk:
                break
            buf += chunk
            if b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                try:
                    req = json.loads(line)
                    markdown = _query(req.get("text", ""), int(req.get("top_k", 3)), state)
                    resp = {"ok": True, "markdown": markdown}
                except Exception as exc:
                    resp = {"ok": False, "error": str(exc)}
                conn.sendall(json.dumps(resp).encode() + b"\n")
                break


def main() -> None:
    state = _load()

    # Remove stale socket
    if SOCK_PATH.exists():
        SOCK_PATH.unlink()

    PID_PATH.write_text(str(os.getpid()))

    server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    server.bind(str(SOCK_PATH))
    server.listen(8)
    SOCK_PATH.chmod(0o600)

    def _shutdown(sig, frame):
        print("[server] shutting down", flush=True)
        server.close()
        SOCK_PATH.unlink(missing_ok=True)
        PID_PATH.unlink(missing_ok=True)
        sys.exit(0)

    signal.signal(signal.SIGTERM, _shutdown)
    signal.signal(signal.SIGINT, _shutdown)

    print(f"[server] listening on {SOCK_PATH}", flush=True)
    while True:
        try:
            conn, _ = server.accept()
        except OSError:
            break
        threading.Thread(target=_handle, args=(conn, state), daemon=True).start()


if __name__ == "__main__":
    main()
