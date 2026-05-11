#!/usr/bin/env python3
"""Per-function SQLite cache for vc71_verify.py.

Cache key: (source_sha256, ref_sha256, compiler_version, fn_decl_sha256, comparator_sha256)
Cache value: match_pct, fpu_warnings (JSON), diff_lines (JSON|NULL), created_utc

Safe to delete at any time — a fresh run rebuilds it.
"""

import hashlib
import json
import os
import re
import sqlite3
import time
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
CACHE_DIR = REPO_ROOT / "artifacts" / "verify_cache"
CACHE_DB = CACHE_DIR / "vc71.sqlite"

# The VC71 compiler binary whose mtime+size we embed in the version string
VC71_CL_WSL = "/mnt/c/Program Files (x86)/RXDK/xbox/bin/vc71/CL.Exe"

_DDL = """
CREATE TABLE IF NOT EXISTS fn_results (
    cache_key   TEXT PRIMARY KEY,
    fn_name     TEXT NOT NULL,
    source_path TEXT NOT NULL,
    ref_path    TEXT NOT NULL,
    match_pct   REAL NOT NULL,
    fpu_warnings TEXT NOT NULL,   -- JSON array of strings
    diff_lines  TEXT,             -- JSON array of strings, or NULL
    created_utc TEXT NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_source ON fn_results(source_path);
CREATE INDEX IF NOT EXISTS idx_fn     ON fn_results(fn_name);
"""


# ---------------------------------------------------------------------------
# Hashing helpers
# ---------------------------------------------------------------------------

def _sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def _sha256_str(s: str) -> str:
    return hashlib.sha256(s.encode()).hexdigest()


def compiler_version_token() -> str:
    """Return a stable string that changes whenever the compiler binary changes.

    Uses size + mtime of CL.Exe.  If the binary is not accessible (Linux-only
    CI, missing RXDK) fall back to the path string so the key is still unique.
    """
    try:
        st = os.stat(VC71_CL_WSL)
        return f"vc71:{st.st_size}:{int(st.st_mtime)}"
    except OSError:
        return f"vc71:unavailable:{VC71_CL_WSL}"


def fn_decl_sha256(fn_name: str) -> str:
    """Return SHA-256 of the kb.json decl string for fn_name, or empty-string hash.

    Reads kb.json once per process via a module-level cache.
    The function name may be either 'FUN_XXXXXXXX' or a renamed symbol.
    """
    kb = _load_kb()
    decl = kb.get(fn_name, "")
    return _sha256_str(decl)


# Module-level kb.json decl map  {fn_name: decl_string}
_KB_DECL_MAP: dict[str, str] | None = None


def _load_kb() -> dict[str, str]:
    global _KB_DECL_MAP
    if _KB_DECL_MAP is not None:
        return _KB_DECL_MAP
    _KB_DECL_MAP = {}
    kb_path = REPO_ROOT / "kb.json"
    try:
        with open(kb_path) as f:
            kb = json.load(f)
        for obj in kb.get("objects", []):
            for fn_entry in obj.get("functions", []):
                addr = fn_entry.get("addr", "")
                decl = fn_entry.get("decl", "")
                if not addr:
                    continue
                fun_name = f"FUN_{int(addr, 16):08x}"
                _KB_DECL_MAP[fun_name] = decl
                # Also index by declared name if different
                m = re.search(r"\b(\w+)\s*\(", decl)
                if m:
                    declared = m.group(1)
                    if declared != fun_name:
                        _KB_DECL_MAP[declared] = decl
    except Exception:
        pass
    return _KB_DECL_MAP


def make_cache_key(fn_name: str, source_path: Path, ref_path: Path) -> str:
    """Build the composite cache key for a single function."""
    src_sha = _sha256_file(source_path)
    ref_sha = _sha256_file(ref_path)
    cc_ver = compiler_version_token()
    decl_sha = fn_decl_sha256(fn_name)
    comparator_sha = _sha256_file(REPO_ROOT / "tools" / "verify" / "compare_obj.py")
    raw = f"{fn_name}|{src_sha}|{ref_sha}|{cc_ver}|{decl_sha}|{comparator_sha}"
    return hashlib.sha256(raw.encode()).hexdigest()


# ---------------------------------------------------------------------------
# Cache manager
# ---------------------------------------------------------------------------

class Vc71Cache:
    """Thin wrapper around the SQLite cache database."""

    def __init__(self, db_path: Path = CACHE_DB):
        self._db_path = db_path
        self._conn: sqlite3.Connection | None = None

    def _open(self) -> sqlite3.Connection:
        if self._conn is None:
            self._db_path.parent.mkdir(parents=True, exist_ok=True)
            conn = sqlite3.connect(str(self._db_path), timeout=10)
            conn.row_factory = sqlite3.Row
            conn.executescript(_DDL)
            conn.commit()
            self._conn = conn
        return self._conn

    def close(self):
        if self._conn:
            self._conn.close()
            self._conn = None

    def get(
        self,
        fn_name: str,
        source_path: Path,
        ref_path: Path,
    ) -> dict | None:
        """Return cached result or None on miss."""
        key = make_cache_key(fn_name, source_path, ref_path)
        conn = self._open()
        row = conn.execute(
            "SELECT match_pct, fpu_warnings, diff_lines, created_utc FROM fn_results WHERE cache_key = ?",
            (key,),
        ).fetchone()
        if row is None:
            return None
        return {
            "match_pct": row["match_pct"],
            "fpu_warnings": json.loads(row["fpu_warnings"]),
            "diff_lines": json.loads(row["diff_lines"]) if row["diff_lines"] else None,
            "created_utc": row["created_utc"],
        }

    def put(
        self,
        fn_name: str,
        source_path: Path,
        ref_path: Path,
        match_pct: float,
        fpu_warnings: list[str],
        diff_lines: list[str] | None,
    ) -> None:
        """Insert or replace a cache entry."""
        key = make_cache_key(fn_name, source_path, ref_path)
        created = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
        conn = self._open()
        conn.execute(
            """
            INSERT OR REPLACE INTO fn_results
              (cache_key, fn_name, source_path, ref_path,
               match_pct, fpu_warnings, diff_lines, created_utc)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?)
            """,
            (
                key,
                fn_name,
                str(source_path),
                str(ref_path),
                match_pct,
                json.dumps(fpu_warnings),
                json.dumps(diff_lines) if diff_lines is not None else None,
                created,
            ),
        )
        conn.commit()

    def invalidate(self, source_path: Path | None = None, ref_path: Path | None = None) -> int:
        """Delete entries matching source and/or ref path. Returns rows deleted."""
        conn = self._open()
        if source_path and ref_path:
            cur = conn.execute(
                "DELETE FROM fn_results WHERE source_path = ? AND ref_path = ?",
                (str(source_path), str(ref_path)),
            )
        elif source_path:
            cur = conn.execute(
                "DELETE FROM fn_results WHERE source_path = ?",
                (str(source_path),),
            )
        elif ref_path:
            cur = conn.execute(
                "DELETE FROM fn_results WHERE ref_path = ?",
                (str(ref_path),),
            )
        else:
            cur = conn.execute("DELETE FROM fn_results")
        conn.commit()
        return cur.rowcount

    def rebuild(self) -> None:
        """Drop all entries (alias for invalidate with no args)."""
        self.invalidate()

    def stats(self) -> dict:
        """Return basic statistics about the cache."""
        conn = self._open()
        total = conn.execute("SELECT COUNT(*) FROM fn_results").fetchone()[0]
        oldest = conn.execute("SELECT MIN(created_utc) FROM fn_results").fetchone()[0]
        newest = conn.execute("SELECT MAX(created_utc) FROM fn_results").fetchone()[0]
        by_source = conn.execute(
            "SELECT source_path, COUNT(*) as cnt FROM fn_results GROUP BY source_path ORDER BY cnt DESC LIMIT 10"
        ).fetchall()
        return {
            "total_entries": total,
            "oldest": oldest,
            "newest": newest,
            "top_sources": [(r["source_path"], r["cnt"]) for r in by_source],
            "db_path": str(self._db_path),
        }


# Module-level singleton for use within a single verify run
_default_cache: Vc71Cache | None = None


def get_default_cache() -> Vc71Cache:
    global _default_cache
    if _default_cache is None:
        _default_cache = Vc71Cache()
    return _default_cache
