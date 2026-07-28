#!/usr/bin/env python3
"""Persistent retrieval query server.

Loads the embedding model and index once, then serves queries over a
Unix socket at /tmp/retrieval_server.sock.

Start before a dev session:
    python3 tools/retrieval/server.py &

The decompile_hook.py connects to this socket instead of spawning a new
subprocess per query, reducing per-query latency from ~90s to ~1-2s.

Protocol: newline-terminated JSON lines.
  Request:  {"text": "<pseudocode>", "top_k": 3, "obj_name": "...", "include_hazards": true}
  Response: {"ok": true, "markdown": "...", "hazard_warnings": "...", "hazard_sections": [...]}
"""

from __future__ import annotations

import json
import os
import signal
import socket
import subprocess
import sys
import threading
from pathlib import Path


def _canonical_repo_root() -> Path:
    """Resolve the canonical repo root that actually holds .venv + the index.

    `__file__`-relative resolution breaks when this script runs from a linked
    git worktree (or a sibling checkout): the worktree has no `.venv`, so the
    interpreter falls back to bare `python3` and numpy import fails. Prefer, in
    order: an explicit HALO_REPO_ROOT override, the git common dir's parent
    (the main worktree, shared by every linked worktree), the file-relative
    root, then the pinned dev-box path — accepting the first that carries both
    `.venv/bin/python3` and `tools/retrieval/`."""
    here = Path(__file__).resolve()
    candidates: list[Path] = []
    env = os.environ.get("HALO_REPO_ROOT")
    if env:
        candidates.append(Path(env))
    try:
        r = subprocess.run(
            ["git", "rev-parse", "--git-common-dir"],
            cwd=here.parent, capture_output=True, text=True, timeout=3,
        )
        if r.returncode == 0 and r.stdout.strip():
            common = Path(r.stdout.strip())
            if not common.is_absolute():
                common = (here.parent / common).resolve()
            candidates.append(common.parent)
    except Exception:
        pass
    candidates.append(here.parent.parent.parent)
    candidates.append(Path("/mnt/g/dev/halo"))
    for c in candidates:
        try:
            if (c / ".venv" / "bin" / "python3").exists() and (c / "tools" / "retrieval").is_dir():
                return c
        except OSError:
            pass
    return here.parent.parent.parent


REPO_ROOT = _canonical_repo_root()
SOCK_PATH = Path("/tmp/retrieval_server.sock")
PID_PATH = Path("/tmp/retrieval_server.pid")
# Held for the process lifetime by the one server that wins startup.  flock is
# released by the kernel when the holder dies -- including SIGKILL and OOM-kill
# -- so unlike a PID file it cannot go stale and wedge every future start.
LOCK_PATH = Path("/tmp/retrieval_server.lock")
_lock_fd = None  # module-level: keeps the descriptor (and the lock) alive


def acquire_singleton_lock():
    """Take the exclusive server lock, or return None if one is already up.

    This is the readiness signal the socket cannot be, because the socket is
    only created after the index rebuild -- minutes and ~1.3 GB later.  Anything
    probing for a live server during that window sees nothing and starts another
    one, which starts its own rebuild, which widens the window further.  The
    lock is taken before any of that work, so a starting server is visible from
    its first instant.
    """
    global _lock_fd
    import fcntl
    fd = os.open(str(LOCK_PATH), os.O_RDWR | os.O_CREAT, 0o600)
    try:
        fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except OSError:
        os.close(fd)
        return None
    _lock_fd = fd
    os.ftruncate(fd, 0)
    os.write(fd, str(os.getpid()).encode())
    return fd


def singleton_holder_pid():
    """PID of the running/starting server, or None if the lock is free.

    Used by would-be spawners to answer "is one already coming up?" without
    racing.  Probing is non-destructive: it never touches SOCK_PATH or PID_PATH,
    so it cannot orphan a live server the way a careless start would.
    """
    import fcntl
    try:
        fd = os.open(str(LOCK_PATH), os.O_RDWR | os.O_CREAT, 0o600)
    except OSError:
        return None
    try:
        try:
            fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError:
            try:
                return int(os.pread(fd, 32, 0).decode().strip() or 0) or -1
            except (ValueError, OSError):
                return -1  # held, but the pid is unreadable
        fcntl.flock(fd, fcntl.LOCK_UN)
        return None
    finally:
        os.close(fd)

# venv shim
_VENV_SP = REPO_ROOT / ".venv" / "lib" / "python3.12" / "site-packages"
if _VENV_SP.exists() and str(_VENV_SP) not in sys.path:
    sys.path.insert(0, str(_VENV_SP))
sys.path.insert(0, str(REPO_ROOT))


def _is_index_stale() -> bool:
    """Return True if the retrieval index needs rebuilding."""
    db_path = REPO_ROOT / "tools" / "retrieval" / "index.duckdb"
    kb_path = REPO_ROOT / "kb.json"

    if not db_path.exists():
        return True

    import subprocess as _subprocess

    result = _subprocess.run(
        ["git", "log", "-1", "--format=%ct", "--", "kb.json", "src/"],
        capture_output=True, text=True, cwd=str(REPO_ROOT),
    )
    git_ts = int(result.stdout.strip()) if result.stdout.strip() else 0

    kb_ts = kb_path.stat().st_mtime

    latest_input = max(kb_ts, float(git_ts))
    index_mtime = db_path.stat().st_mtime

    return latest_input > index_mtime + 1.0


def _rebuild_index() -> None:
    """Run extract + outcomes + embed in a subprocess to rebuild the index."""
    build_index = REPO_ROOT / "tools" / "retrieval" / "build_index.py"
    log_path = Path("/tmp/retrieval_auto_rebuild.log")

    venv_python = REPO_ROOT / ".venv" / "bin" / "python3"
    py = str(venv_python) if venv_python.exists() else sys.executable

    import subprocess as _subprocess
    import time as _time
    t0 = _time.monotonic()

    chain = (
        f"{py} {build_index} extract >> {log_path} 2>&1 && "
        f"{py} {build_index} outcomes >> {log_path} 2>&1 && "
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
    """Load embedder + index rows once, including quality columns."""
    import numpy as np
    from tools.retrieval.embed import Embedder
    from tools.retrieval import db as _db

    print("[server] loading embedder...", flush=True)
    embedder = Embedder()

    print("[server] loading index...", flush=True)
    con = _db.connect(read_only=True)
    rows = list(_db.iter_records(con, require_embeddings=True))
    con.close()

    if rows:
        dim = len(rows[0].get("emb_c") or rows[0].get("emb_pseudocode"))
        pseudo_vecs = []
        c_vecs = []
        has_pseudo = []
        has_c = []
        vc71_scores = []
        has_c_source = []
        obj_names = []
        verdicts = []
        hazard_flags_list = []
        for r in rows:
            p = r.get("emb_pseudocode")
            c = r.get("emb_c")
            pseudo_vecs.append(np.array(p, dtype=np.float32) if p else np.zeros(dim, dtype=np.float32))
            c_vecs.append(np.array(c, dtype=np.float32) if c else np.zeros(dim, dtype=np.float32))
            has_pseudo.append(p is not None)
            has_c.append(c is not None)
            vc71_scores.append(r.get("vc71_score") or 0.0)
            has_c_source.append(bool(r.get("c_source")))
            obj_names.append(r.get("obj_name", ""))
            verdicts.append(r.get("verdict", ""))
            hazard_flags_list.append(r.get("hazard_flags", ""))
        pseudo_mat = np.stack(pseudo_vecs)
        c_mat = np.stack(c_vecs)
        has_pseudo_arr = np.array(has_pseudo)
        has_c_arr = np.array(has_c)
        vc71_arr = np.array(vc71_scores, dtype=np.float32)
        has_c_source_arr = np.array(has_c_source)
    else:
        pseudo_mat = c_mat = has_pseudo_arr = has_c_arr = None
        vc71_arr = has_c_source_arr = None
        obj_names = []
        verdicts = []
        hazard_flags_list = []

    print(f"[server] ready — {len(rows)} rows indexed", flush=True)
    return (embedder, rows, pseudo_mat, c_mat, has_pseudo_arr, has_c_arr,
            vc71_arr, has_c_source_arr, obj_names, verdicts, hazard_flags_list)


def _query(text: str, top_k: int, state: tuple,
           obj_name: str = "", min_vc71: float = 85.0) -> str:
    """Run a neighbor query with quality filtering and return formatted markdown."""
    import numpy as np
    from tools.retrieval.query import Neighbor, format_for_prompt

    (embedder, rows, pseudo_mat, c_mat, has_pseudo_arr, has_c_arr,
     vc71_arr, has_c_source_arr, obj_names, verdicts, hazard_flags_list) = state
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

    # Quality mask
    mask = has_c_source_arr.copy()
    mask &= (vc71_arr >= min_vc71) | (vc71_arr == 0.0)  # 0.0 = no score, allow
    best_sims[~mask] = -1.0

    # Same-TU bonus
    if obj_name:
        for i, oname in enumerate(obj_names):
            if oname == obj_name:
                best_sims[i] += 0.05

    MIN_SIM = 0.5
    if top_k >= len(best_sims):
        indices = list(range(len(best_sims)))
        indices.sort(key=lambda i: -best_sims[i])
    else:
        indices = np.argpartition(-best_sims, top_k)[:top_k]
        indices = list(indices[np.argsort(-best_sims[indices])])

    best_col = np.where(pseudo_sims >= c_sims, "pseudocode", "c_source")
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
            vc71_score=r.get("vc71_score"),
        ))

    return format_for_prompt(neighbors[:top_k])


def _query_hazards(text: str, top_k: int, state: tuple) -> str:
    """Query for hazard warnings from similar failed functions."""
    import numpy as np
    from tools.retrieval.query import HazardWarning, format_hazard_warnings
    import json as _json

    (embedder, rows, pseudo_mat, c_mat, has_pseudo_arr, has_c_arr,
     vc71_arr, has_c_source_arr, obj_names, verdicts, hazard_flags_list) = state
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

    # Only keep failures or rows with hazard flags
    mask = np.zeros(len(rows), dtype=bool)
    for i in range(len(rows)):
        if verdicts[i] in ("fail", "reject") or hazard_flags_list[i]:
            mask[i] = True
    best_sims[~mask] = -1.0

    MIN_SIM = 0.4
    if top_k >= len(best_sims):
        indices = list(range(len(best_sims)))
        indices.sort(key=lambda i: -best_sims[i])
    else:
        indices = np.argpartition(-best_sims, top_k)[:top_k]
        indices = list(indices[np.argsort(-best_sims[indices])])

    warnings = []
    for idx in indices:
        sim = float(best_sims[idx])
        if sim < MIN_SIM:
            break
        r = rows[idx]
        flags_raw = r.get("hazard_flags", "")
        try:
            flags = _json.loads(flags_raw) if flags_raw else []
        except (_json.JSONDecodeError, TypeError):
            flags = []
        warnings.append(HazardWarning(
            source_name=r.get("name", ""),
            source_addr=r.get("addr", ""),
            similarity=sim,
            vc71_score=r.get("vc71_score"),
            verdict=r.get("verdict", ""),
            failure_reason=r.get("failure_reason"),
            hazard_flags=flags,
        ))

    return format_hazard_warnings(warnings[:top_k])


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
                    text = req.get("text", "")
                    top_k = int(req.get("top_k", 3))
                    obj_name = req.get("obj_name", "")
                    include_hazards = req.get("include_hazards", False)

                    markdown = _query(text, top_k, state, obj_name=obj_name)
                    hazard_md = ""
                    if include_hazards:
                        hazard_md = _query_hazards(text, 3, state)

                    resp = {
                        "ok": True,
                        "markdown": markdown,
                        "hazard_warnings": hazard_md,
                    }
                except Exception as exc:
                    resp = {"ok": False, "error": str(exc)}
                conn.sendall(json.dumps(resp).encode() + b"\n")
                break


def main() -> None:
    # Claim singleton status BEFORE any other action.  Two separate bugs made
    # this file leak ~1.3 GB per accidental start until the box ran out of RAM:
    #   1. the socket -- the only liveness signal spawners had -- was created
    #      after the index rebuild, so a starting server looked absent for
    #      minutes and every probe in that window started another one;
    #   2. this function then unlinked SOCK_PATH/PID_PATH unconditionally, so
    #      each new server severed the previous one's socket, leaving it running
    #      and unreachable forever.  Started with start_new_session=True, those
    #      orphans reparent to init and never exit.
    # Both are closed by taking the lock first and only cleaning up files once
    # we know no one else owns them.
    if acquire_singleton_lock() is None:
        holder = singleton_holder_pid()
        print(f"[server] another server is running or starting (pid {holder}); "
              "exiting without touching its socket", flush=True)
        return

    if SOCK_PATH.exists():
        SOCK_PATH.unlink()
    if PID_PATH.exists():
        PID_PATH.unlink()

    # Publish our pid before the slow part so probes can see a starting server.
    PID_PATH.write_text(str(os.getpid()))

    if _is_index_stale():
        print("[server] index is stale — rebuilding...", flush=True)
        _rebuild_index()
    else:
        print("[server] index is fresh", flush=True)

    state = _load()

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
