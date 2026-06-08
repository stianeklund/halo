"""DuckDB-backed semantic-retrieval index for Halo lift work.

Schema captures the three signals Mizuchi-style retrieval needs:
- pseudocode (cached Ghidra output) — the closest analog to what we'll see
  for a new target before lifting; used as the primary embedding signal.
- c_source — the final lifted C; used both as a secondary embedding and
  as the worked example we inject into the lift prompt.
- decl, addr, name — metadata for surfacing neighbors back to the agent.

Embedding columns are populated separately by `tools/retrieval/embed.py`
once a model is loaded. Commit 1 only fills the text columns.
"""

from __future__ import annotations

import sys
from pathlib import Path
from typing import Iterator

_REPO_ROOT = Path(__file__).resolve().parent.parent.parent
_VENV_SP = _REPO_ROOT / ".venv" / "lib" / "python3.12" / "site-packages"
if _VENV_SP.exists() and str(_VENV_SP) not in sys.path:
    sys.path.insert(0, str(_VENV_SP))

import duckdb

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
DB_PATH = Path(__file__).resolve().parent / "index.duckdb"

EMBED_DIM = 768  # jina-embeddings-v2-base-code

SCHEMA = f"""
CREATE TABLE IF NOT EXISTS functions (
    addr             VARCHAR PRIMARY KEY,
    name             VARCHAR NOT NULL,
    obj_name         VARCHAR,
    source_path      VARCHAR,
    decl             TEXT,
    pseudocode       TEXT,
    c_source         TEXT,
    pseudocode_sha   VARCHAR,
    c_source_sha     VARCHAR,
    indexed_at       TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    emb_pseudocode   FLOAT[{EMBED_DIM}],
    emb_c            FLOAT[{EMBED_DIM}],
    emb_model        VARCHAR
);
"""


def connect(read_only: bool = False) -> duckdb.DuckDBPyConnection:
    """Open the index DB and ensure the schema exists."""
    DB_PATH.parent.mkdir(parents=True, exist_ok=True)
    con = duckdb.connect(str(DB_PATH), read_only=read_only)
    if not read_only:
        con.execute(SCHEMA)
    return con


def upsert_record(con: duckdb.DuckDBPyConnection, rec: dict) -> None:
    """Insert or update a function record, preserving embeddings on unchanged rows.

    When content (pseudocode or C source) changes, embedding columns are nulled
    so the next ``embed`` run re-embeds them. Unchanged rows keep their embeddings.
    """
    existing = con.execute(
        "SELECT pseudocode_sha, c_source_sha FROM functions WHERE addr = ?",
        [rec["addr"]],
    ).fetchone()

    if existing:
        sha_unchanged = (
            existing[0] == rec.get("pseudocode_sha")
            and existing[1] == rec.get("c_source_sha")
        )
        if sha_unchanged:
            con.execute(
                "UPDATE functions SET indexed_at = CURRENT_TIMESTAMP WHERE addr = ?",
                [rec["addr"]],
            )
            return

        con.execute(
            """
            UPDATE functions SET
                name = ?, obj_name = ?, source_path = ?, decl = ?,
                pseudocode = ?, c_source = ?,
                pseudocode_sha = ?, c_source_sha = ?,
                emb_pseudocode = NULL, emb_c = NULL, emb_model = NULL,
                indexed_at = CURRENT_TIMESTAMP
            WHERE addr = ?
            """,
            [
                rec["name"], rec.get("obj_name"),
                rec.get("source_path"), rec.get("decl"),
                rec.get("pseudocode"), rec.get("c_source"),
                rec.get("pseudocode_sha"), rec.get("c_source_sha"),
                rec["addr"],
            ],
        )
    else:
        con.execute(
            """
            INSERT INTO functions
                (addr, name, obj_name, source_path, decl, pseudocode, c_source,
                 pseudocode_sha, c_source_sha, indexed_at)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, CURRENT_TIMESTAMP)
            """,
            [
                rec["addr"], rec["name"], rec.get("obj_name"),
                rec.get("source_path"), rec.get("decl"),
                rec.get("pseudocode"), rec.get("c_source"),
                rec.get("pseudocode_sha"), rec.get("c_source_sha"),
            ],
        )


def update_embeddings(
    con: duckdb.DuckDBPyConnection,
    addr: str,
    *,
    emb_pseudocode: list[float] | None,
    emb_c: list[float] | None,
    emb_model: str,
) -> None:
    """Set embedding vectors for an existing record."""
    con.execute(
        """
        UPDATE functions
        SET emb_pseudocode = ?, emb_c = ?, emb_model = ?
        WHERE addr = ?
        """,
        [emb_pseudocode, emb_c, emb_model, addr],
    )


def iter_records(
    con: duckdb.DuckDBPyConnection,
    *,
    require_pseudocode: bool = False,
    require_c: bool = False,
    require_embeddings: bool = False,
) -> Iterator[dict]:
    """Stream rows matching the requested filters."""
    where = []
    if require_pseudocode:
        where.append("pseudocode IS NOT NULL")
    if require_c:
        where.append("c_source IS NOT NULL")
    if require_embeddings:
        where.append("(emb_pseudocode IS NOT NULL OR emb_c IS NOT NULL)")
    sql = "SELECT * FROM functions"
    if where:
        sql += " WHERE " + " AND ".join(where)
    cursor = con.execute(sql)
    cols = [d[0] for d in cursor.description]
    for row in cursor.fetchall():
        yield dict(zip(cols, row))


def stats(con: duckdb.DuckDBPyConnection) -> dict:
    """Return summary counts for CLI reporting."""
    row = con.execute(
        """
        SELECT
            COUNT(*)                                            AS total,
            COUNT(pseudocode)                                   AS with_pseudocode,
            COUNT(c_source)                                     AS with_c,
            COUNT(*) FILTER (WHERE emb_pseudocode IS NOT NULL OR emb_c IS NOT NULL) AS with_emb,
            COUNT(DISTINCT obj_name)                            AS objects,
            COUNT(DISTINCT emb_model)                           AS models
        FROM functions
        """
    ).fetchone()
    return dict(zip(
        ("total", "with_pseudocode", "with_c", "with_emb", "objects", "models"),
        row,
    ))
