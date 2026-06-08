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


def _is_index_stale() -> bool:
    """Return True if the retrieval index needs rebuilding.

    Compares kb.json mtime (and the latest git commit touching kb.json or src/)
    against the DuckDB index file's mtime.  If input sources are newer, the
    index is stale.
    """
    db_path = REPO_ROOT / "tools" / "retrieval" / "index.duckdb"
    kb_path = REPO_ROOT / "kb.json"

    if not db_path.exists():
        return True

    import subprocess as _subprocess

    # Latest git commit touching inputs
    result = _subprocess.run(
        ["git", "log", "-1", "--format=%ct", "--", "kb.json", "src/"],
        capture_output=True, text=True, cwd=str(REPO_ROOT),
    )
    git_ts = int(result.stdout.strip()) if result.stdout.strip() else 0

    # Latest file mtime on kb.json (covers uncommitted changes)
    kb_ts = kb_path.stat().st_mtime

    latest_input = max(kb_ts, float(git_ts))
    index_mtime = db_path.stat().st_mtime

    return latest_input > index_mtime + 1.0  # 1-second fudge for fs resolution


def _rebuild_index() -> None:
    """Run extract + embed in a subprocess to rebuild the index."""
    build_index = REPO_ROOT / "tools" / "retrieval" / "build_index.py"
    log_path = Path("/tmp/retrieval_auto_rebuild.log")

    venv_python = REPO_ROOT / ".venv" / "bin" / "python3"
    py = str(venv_python) if venv_python.exists() else sys.executable

    import subprocess as _subprocess
    import time as _time
    t0 = _time.monotonic()

    chain = (
        f"{py} {build_index} extract >> {log_path} 2>&1 && "
        f"{py} {build_index} embed >> {log_path} 2>&1"
    )
    ret = _subprocess.run(chain, shell=True, cwd=str(REPO_ROOT))

    elapsed = _time.monotonic() - t0
    if ret.returncode == 0:
        print(f"[server] index rebuilt ({elapsed:.1f}s)", flush=True)
    else:
        print(f"[server] index rebuild FAILED (exit={ret.returncode}, {elapsed:.1f}s) — "
              f"see {log_path}", flush=True)


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
    # Clean up stale socket from any previous instance
    if SOCK_PATH.exists():
        SOCK_PATH.unlink()
    if PID_PATH.exists():
        PID_PATH.unlink()

    # Rebuild index if input sources are newer than the last index
    if _is_index_stale():
        print("[server] index is stale — rebuilding...", flush=True)
        _rebuild_index()
    else:
        print("[server] index is fresh", flush=True)

    state = _load()

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
