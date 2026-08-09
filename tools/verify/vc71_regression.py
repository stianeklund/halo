#!/usr/bin/env python3
"""
vc71_regression.py — Track VC71 match scores and detect regressions.

STATUS: Active.  Generates and updates tools/verify/vc71_scores.json which is
committed to the repo.  The scores file is consumed by tools/analysis/frontier.py,
tools/llm_auto_lift.py (liftability scoring), and tools/equivalence/batch_equivalence.py
(priority queue).  No auto-callers; run manually after bulk lifts.

Manages tools/verify/vc71_scores.json (committed to repo), which records the
expected minimum VC71 match percentage for each ported function. Used to catch
edits that silently degrade byte-match quality.

Commands:
    update --source src/halo/game/game.c [...]
        Run vc71_verify on the given source file(s), update the stored
        floor scores. Raises stored score when a function improves; never
        lowers it automatically (use --force to lower).

    check [--source src/...] [--threshold N]
        Run vc71_verify and compare against stored floors.
        Exits 1 if any function dropped more than --threshold pp (default 2).
        Limits to specific files when --source is given.

    show
        Print the current baseline in a human-readable table.

    populate
        Re-derive scores for every ported function in kb.json by scanning
        all relevant source files (slow, but useful for initial population
        or after bulk changes). Writes new floors for any function not yet
        in the baseline.
"""

import argparse
import bisect
import hashlib
import json
import os
import re
import subprocess
import sys
import threading
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
# This module runs two ways: as a script (`python3 tools/verify/vc71_regression.py`,
# where sys.path[0] IS tools/verify) and imported as `tools.verify.vc71_regression`
# by tools/recovery/source_recovery.py, where it is not.  _func_span delegates to
# vc71_verify._func_span; on the imported path that bare import silently raised
# ImportError and the gate fell back to the kb.json listing gap -- overshooting the
# span, so a correct short reference read as truncated and the function was reported
# "invalid delinked reference".  Measured on FUN_000d8b70/FUN_000d8b80: kb gap 16
# bytes vs a true span of 1, failing a check that passes identically via the CLI.
_VERIFY_DIR = str(Path(__file__).resolve().parent)
if _VERIFY_DIR not in sys.path:
    sys.path.insert(0, _VERIFY_DIR)
# Floored regression tripwire (committed; consumed by frontier.py, llm_auto_lift.py,
# batch_equivalence.py and CI's `check`).  Only raised on improvement / lowered
# deliberately with --force.
#
# KEYED BY DELINKED-REFERENCE SYMBOL NAME, not by our C identifier (hence entries
# like "D3D8::D3DResource_IsBusy").  A symbol-names recovery pass that renames C
# functions must leave these keys ALONE: they only change when the delinked
# reference is re-exported.  Hand-renaming them to match the new C names decouples
# the floor from what is actually measured and breaks `check`.
BASELINE_PATH = Path(__file__).parent / "vc71_scores.json"
# Honest current scores (bidirectional, gated on reference validity).  Written by
# `populate`; preferred by the dashboard so it reflects present truth, not a
# high-water mark.  Regenerated each run — not a committed baseline.
CURRENT_PATH = Path(__file__).parent / "vc71_current.json"
# Functions whose delinked reference failed validation (truncated/absent) and were
# therefore NOT scored.  These need re-delinking before their score is meaningful.
VALIDITY_PATH = REPO_ROOT / "artifacts" / "audit" / "reference_validity.json"
# Per-TU input fingerprints for `populate --incremental` (host-local, gitignored).
# Lets a dashboard refresh re-verify only the TUs whose inputs changed.
POPULATE_STATE_PATH = REPO_ROOT / "artifacts" / "audit" / "populate_state.json"
# Whole-TU measurement memo shared by check/update/populate (host-local, gitignored).
# Keyed by the same fingerprint populate uses -- source bytes, whole-object delinked
# reference, the TU's slice of the generated decl.h, and the tool epoch (verify
# tooling shas + kb address signature + per-function chunk directory).  Those are
# every input that can move a score, so a hit is sound; anything else forces a real
# re-measure.  Exists because the pre-commit hook runs `check` and then `update`
# over the same staged file list -- two full recompiles of identical inputs -- and
# because a hook that times out is otherwise re-run from scratch.
MEASURE_CACHE_PATH = REPO_ROOT / "artifacts" / "audit" / "vc71_measure_cache.json"
# Cheap stat-keyed cache for the delinked/functions/*.obj content signature; see
# _chunk_dir_signature.  Host-local, gitignored.
CHUNK_SIG_CACHE_PATH = REPO_ROOT / "artifacts" / "audit" / "vc71_chunk_sig.json"
VC71_VERIFY = REPO_ROOT / "tools" / "verify" / "vc71_verify.py"
# Generated header the VC71 compile includes; produced from kb.json by knowledge.py.
# Nothing on the populate path used to regenerate it, so a stale header (disagreeing
# with kb.json) silently failed the compile and dropped the whole TU.  cmd_populate
# now regenerates it up front and folds its per-TU content into the fingerprint.
KNOWLEDGE_PY = REPO_ROOT / "tools" / "analysis" / "knowledge.py"
DECL_H = REPO_ROOT / "build" / "generated" / "decl.h"

_LINE_RE = re.compile(
    r"(?:PASS|FAIL)\s+(\S+):\s+([\d.]+)%\s+match\s+\((\d+)/(\d+)\s+insns\)"
)
# vc71_verify emits one of these per compiled, kb.json-tracked function it could
# not score against any valid reference (no whole-object symbol and no valid
# per-function chunk).  These never produce a _LINE_RE score line, so the runner
# surfaces them separately to keep the re-delink queue complete.
_DROP_RE = re.compile(
    r"DROP\s+(\S+):\s+no valid reference\s+—\s+(.+?)\s+\(span\s+(\d+)\s+bytes\)"
)
# Advisory operand-normalized score, appended by vc71_verify to the same status
# line AFTER the optional reg/fpu/loadw/imm tags:
#   "PASS FUN_x: 97.8% match (699/698 insns) | opnd 90.3% (operand-normalized)"
# Kept as its own pattern rather than an optional tail on _LINE_RE so the
# primary score parse is unchanged and cached lines without the token still
# parse.  Absent => opnd_percent is None everywhere downstream.
_OPND_RE = re.compile(r"\|\s*opnd\s+([\d.]+)%")
# Emitted once per function scored against a reference SYNTHESIZED from the
# pristine XBE rather than delinked by Ghidra (see tools/verify/xbe_reference.py).
# Provenance matters because the two are not identical: measured agreement across
# 1,464 existing chunks is 95.9%.  A score with no delinked reference behind it
# should be identifiable as such in the baseline, not silently equivalent.
_SYNTHREF_RE = re.compile(r"^\s*SYNTHREF\s+(\S+)\s*$")


# ---------------------------------------------------------------------------
# Baseline I/O
# ---------------------------------------------------------------------------

def load_baseline() -> dict[str, dict]:
    """Return {fn_name: {"score": float, "source": str}} from the baseline file."""
    if not BASELINE_PATH.exists():
        return {}
    try:
        data = json.loads(BASELINE_PATH.read_text())
        return data.get("scores", {})
    except (json.JSONDecodeError, OSError):
        return {}


def save_baseline(scores: dict[str, dict]) -> None:
    data = {"version": 1, "scores": scores}
    BASELINE_PATH.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n")


def save_current(scores: dict[str, dict]) -> None:
    """Write honest current scores (dashboard source of truth)."""
    data = {"version": 1,
            "note": "Honest current VC71 scores, gated on reference validity. "
                    "Regenerated by `populate`; not a committed floor.",
            "scores": scores}
    CURRENT_PATH.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n")


def save_validity(flagged: list[dict]) -> None:
    """Write the list of functions skipped because their reference failed
    validation (truncated/absent) — a re-delink work queue."""
    VALIDITY_PATH.parent.mkdir(parents=True, exist_ok=True)
    VALIDITY_PATH.write_text(
        json.dumps({"version": 1, "flagged": flagged}, indent=2, sort_keys=True) + "\n")


# ---------------------------------------------------------------------------
# Incremental populate: per-TU input fingerprints
# ---------------------------------------------------------------------------

def _sha_bytes(*chunks) -> str:
    h = hashlib.sha1()
    for c in chunks:
        if c is None:
            continue
        if isinstance(c, str):
            c = c.encode("utf-8", "replace")
        h.update(c)
        h.update(b"\0")
    return h.hexdigest()


def _sha_file(path: Path) -> str:
    try:
        return _sha_bytes(path.read_bytes())
    except OSError:
        return ""


def regen_decl_header() -> bool:
    """Regenerate build/generated/decl.h from kb.json (the actual VC71 compile input).

    The CMake build only regenerates this header when kb.json/knowledge.py change,
    so a manual `populate` could compile every TU against a stale header — the
    class of failure that silently blanked network_game_globals on the dashboard.
    Running the generator here (cheap, idempotent, deterministic for an unchanged
    kb.json) pins decl.h == kb.json before any TU is fingerprinted or compiled.

    Returns True on success; on failure prints a warning and returns False so
    populate still runs (against whatever header exists).
    """
    DECL_H.parent.mkdir(parents=True, exist_ok=True)
    try:
        r = subprocess.run(
            [sys.executable, str(KNOWLEDGE_PY), "--gen-header", str(DECL_H)],
            capture_output=True, text=True, cwd=REPO_ROOT)
    except OSError as e:
        print(f"  ⚠ could not regenerate decl.h ({e}); using existing header",
              file=sys.stderr)
        return False
    if r.returncode != 0:
        tail = (r.stderr or r.stdout or "").strip().splitlines()[-3:]
        print("  ⚠ decl.h regeneration failed; using existing header:",
              file=sys.stderr)
        for l in tail:
            print(f"      {l}", file=sys.stderr)
        return False
    return True


# Parsed decl.h index: {symbol_name: declaration_line}.  Rebuilt once per process
# (after regen_decl_header, so it reflects the freshly generated header).
_DECL_INDEX: dict[str, str] | None = None
_IDENT_RE = re.compile(r"[A-Za-z_]\w*")


def _decl_index() -> dict[str, str]:
    global _DECL_INDEX
    if _DECL_INDEX is not None:
        return _DECL_INDEX
    idx: dict[str, str] = {}
    try:
        text = DECL_H.read_text(errors="replace")
    except OSError:
        _DECL_INDEX = idx
        return idx
    for line in text.splitlines():
        s = line.strip()
        if not s or s.startswith("#") or s.startswith("//"):
            continue
        # A prototype line: capture the declared symbol (identifier before "(").
        m = re.search(r"([A-Za-z_]\w*)\s*\(", s)
        if m:
            idx.setdefault(m.group(1), s)
    _DECL_INDEX = idx
    return idx


def _tu_decl_digest(src: Path) -> str:
    """Digest of the decl.h lines this TU actually compiles against.

    We must fingerprint the *header content*, not kb.json: the header can drift
    from kb.json (stale generation), and only the header is the compile input.
    We subset to the symbols the source textually references so a lift that
    changes one function's signature invalidates only the TUs that define or
    call it — the TU's own defined functions are always textually present
    (their definitions), so a self-signature mismatch is always caught.
    """
    idx = _decl_index()
    if not idx:
        return ""
    try:
        text = src.read_text(errors="replace")
    except OSError:
        return ""
    referenced = {tok for tok in _IDENT_RE.findall(text) if tok in idx}
    lines = sorted(idx[name] for name in referenced)
    return _sha_bytes("\n".join(lines))


def _kb_addr_signature() -> str:
    """Hash of the sorted kb.json function-address set only.

    The full kb.json changes on every lift (decls), so hashing it into the tool
    epoch defeats incrementality.  Per-TU decl content is captured by
    _tu_decl_digest instead; the only remaining kb input to *scoring* is the
    address layout feeding the reference-validity span gate (_func_span), which
    changes rarely (a re-address implies a re-delink → chunk-dir epoch bump too).
    """
    addrs, _ = _kb_maps()
    return _sha_bytes(",".join(f"{a:x}" for a in addrs))


_TOOL_EPOCH = None


def _tool_epoch() -> str:
    """Hash of every input shared by *all* TUs — the verify/compare tooling,
    kb.json (spans, names, aliases, ported flags), and the delinked per-function
    chunk directory (the fallback oracle).  When any of these change, the scoring
    semantics can move for any function, so a changed epoch forces a full
    re-verify.  This is what makes today's fold-fix (a tool change) correctly
    invalidate the whole cache while a lone source edit does not.
    """
    global _TOOL_EPOCH
    if _TOOL_EPOCH is not None:
        return _TOOL_EPOCH
    parts = []
    for rel in ("tools/verify/vc71_verify.py",
                "tools/verify/compare_obj.py",
                "tools/verify/vc71_regression.py",
                "tools/verify/vc71_cache.py",
                # The reference side of every score: the synthesizer and the
                # committed bounds table it cuts from.  A regenerated table
                # moves scores without touching any other input here.
                "tools/verify/xbe_reference.py",
                "tools/verify/function_bounds.json"):
        parts.append(_sha_file(REPO_ROOT / rel))
    # kb.json is deliberately NOT hashed whole here: it changes on every lift
    # (decls), which would force a full re-verify each time.  Its per-TU-relevant
    # content is captured elsewhere — decls via _tu_decl_digest (from the generated
    # header), address layout via the light signature below (span gate input).
    parts.append(_kb_addr_signature())
    parts.append(_chunk_dir_signature())
    _TOOL_EPOCH = _sha_bytes(*parts)
    return _TOOL_EPOCH


def _delinked_obj_stats() -> list:
    """[(name, size, mtime_ns, Path)] for every delinked reference .obj: the
    whole-object refs in delinked/ and the per-function fallback chunks in
    delinked/functions/.

    Uses os.scandir rather than Path.glob + Path.stat because it reads size and
    mtime from the directory entry instead of issuing a stat syscall per file.
    On this repo's ~2000 references over a WSL2 /mnt/g mount that is the whole
    difference between 7.4s and well under a second -- and it was being paid on
    every invocation, including pure cache hits.
    """
    out = []
    root = REPO_ROOT / "delinked"
    for d in (root, root / "functions"):
        try:
            with os.scandir(d) as it:
                for e in it:
                    if not e.name.endswith(".obj"):
                        continue
                    try:
                        st = e.stat()
                    except OSError:
                        continue
                    out.append((e.name, st.st_size, st.st_mtime_ns, Path(e.path)))
        except (OSError, FileNotFoundError):
            continue
    out.sort()
    return out


def _chunk_dir_signature() -> str:
    """Content signature (name+size+sha) of every delinked reference .obj.

    Content, not mtime: a re-delink of any reference must invalidate the cache,
    but a bare `touch`/checkout of an unchanged .obj must not force a full pass.
    Hashing the whole tree costs ~19s, which every invocation of this script was
    paying -- more than the actual measurement on a one-TU lift.  So the content
    hash is itself cached under a cheap stat signature.  A checkout that only
    moves mtimes misses the stat key, recomputes, and lands on the identical
    content hash, so the epoch is unchanged: the stat signature is a cache key,
    never an input to the answer.

    Covering BOTH directories is what lets the per-TU measurement memo key on
    the epoch alone.  objdiff.json carries one unit per per-function chunk, so
    there is no single `base_path` that identifies a TU's reference.
    """
    entries = _delinked_obj_stats()
    if not entries:
        return _sha_bytes("")
    stat_key = _sha_bytes("\n".join(
        f"{name}:{size}:{mtime}" for name, size, mtime, _ in entries))

    cache = {}
    if CHUNK_SIG_CACHE_PATH.exists():
        try:
            cache = json.loads(CHUNK_SIG_CACHE_PATH.read_text())
        except (json.JSONDecodeError, OSError):
            cache = {}
    if cache.get("stat_key") == stat_key and cache.get("content_sig"):
        return cache["content_sig"]

    sig = []
    for name, size, _mtime, path in entries:
        try:
            sig.append(f"{name}:{size}:{_sha_file(path)}")
        except OSError:
            continue
    content_sig = _sha_bytes("\n".join(sig))
    try:
        CHUNK_SIG_CACHE_PATH.parent.mkdir(parents=True, exist_ok=True)
        _atomic_write_json(CHUNK_SIG_CACHE_PATH,
                           {"stat_key": stat_key, "content_sig": content_sig})
    except OSError:
        pass
    return content_sig


def _tu_fingerprint(src: Path, ref: Path) -> str:
    """Fingerprint one TU by its own compile inputs: source, whole-object
    reference, and the subset of the generated decl.h it references.  Including
    the decl digest is what makes a regenerated/stale header invalidate this TU's
    cached slice (the network_game_globals blank), while keeping incrementality —
    a lift only invalidates TUs that reference the changed signature.
    The tool/kb-addr/chunk epoch is tracked separately at the state-file level."""
    return _sha_bytes(_sha_file(src), _sha_file(ref), _tu_decl_digest(src))


_MEASURE_MEMO: dict | None = None
_MEASURE_MEMO_DIRTY = False
_MEASURE_MEMO_LOCK = threading.Lock()


def _measure_key(src: Path) -> str | None:
    """Fingerprint one TU's measurement inputs, or None if it cannot be pinned
    down.  A None key disables the memo for that TU -- it re-measures, which is
    always correct, just slower.

    Three components cover every input that can move a score: the tool epoch
    (verify tooling, kb.json address signature, and the content of every
    delinked reference), this TU's source bytes, and this TU's slice of the
    generated decl.h.  The floor itself is deliberately absent -- the memo
    stores measurements, and the caller compares them to a freshly-loaded
    baseline."""
    try:
        if not src.is_file():
            return None
        return _sha_bytes(_tool_epoch(), _sha_file(src), _tu_decl_digest(src))
    except (OSError, ValueError):
        return None


def _load_measure_memo() -> dict:
    global _MEASURE_MEMO
    if _MEASURE_MEMO is not None:
        return _MEASURE_MEMO
    _MEASURE_MEMO = {}
    if MEASURE_CACHE_PATH.exists():
        try:
            data = json.loads(MEASURE_CACHE_PATH.read_text())
            if data.get("version") == 1:
                _MEASURE_MEMO = data.get("entries", {})
        except (json.JSONDecodeError, OSError):
            pass
    return _MEASURE_MEMO


def flush_measure_memo() -> None:
    """Persist the memo if anything new was measured.  Bounded so a long-lived
    checkout does not grow it without limit: entries are keyed by content, so
    stale ones are simply unreachable, and we keep the most recent 4000."""
    if not _MEASURE_MEMO_DIRTY or _MEASURE_MEMO is None:
        return
    try:
        entries = _MEASURE_MEMO
        if len(entries) > 4000:
            entries = dict(list(entries.items())[-4000:])
        MEASURE_CACHE_PATH.parent.mkdir(parents=True, exist_ok=True)
        _atomic_write_json(MEASURE_CACHE_PATH, {"version": 1, "entries": entries})
    except OSError:
        pass


def _atomic_write_json(path: Path, payload: dict) -> None:
    """Write via temp-file + rename.  Several agents run gates concurrently in
    this checkout; a torn write would leave unparseable JSON that every later
    process silently discards (the loader falls back to an empty cache), turning
    a shared speedup into a permanent cold start."""
    tmp = path.with_suffix(path.suffix + f".{os.getpid()}.tmp")
    try:
        tmp.write_text(json.dumps(payload) + "\n")
        os.replace(tmp, path)
    finally:
        try:
            tmp.unlink()
        except OSError:
            pass


def load_populate_state() -> dict:
    if not POPULATE_STATE_PATH.exists():
        return {}
    try:
        return json.loads(POPULATE_STATE_PATH.read_text())
    except (json.JSONDecodeError, OSError):
        return {}


def save_populate_state(epoch: str, tus: dict) -> None:
    POPULATE_STATE_PATH.parent.mkdir(parents=True, exist_ok=True)
    data = {"version": 1, "tool_epoch": epoch, "tus": tus}
    POPULATE_STATE_PATH.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n")


# ---------------------------------------------------------------------------
# Reference-validity gate
# ---------------------------------------------------------------------------

_KB_MAPS = None


def _kb_maps():
    """Return (sorted_addrs, name->addr) from kb.json.

    Used to bound a function's expected instruction count against its byte span
    in the binary (next function start - this function start), so a truncated or
    bloated delinked reference can be detected before its score is trusted.
    """
    global _KB_MAPS
    if _KB_MAPS is None:
        addrs: set[int] = set()
        name2addr: dict[str, int] = {}
        try:
            data = json.loads((REPO_ROOT / "kb.json").read_text())
        except (OSError, json.JSONDecodeError):
            _KB_MAPS = ([], {})
            return _KB_MAPS
        for obj in data.get("objects", []):
            for fn in obj.get("functions") or []:
                a = fn.get("addr")
                if not a:
                    continue
                try:
                    ai = int(a, 16)
                except (ValueError, TypeError):
                    continue
                addrs.add(ai)
                name2addr[f"FUN_{ai:08x}"] = ai
                m = re.search(r"([A-Za-z_]\w*)\s*\(", fn.get("decl", "") or "")
                if m:
                    name2addr.setdefault(m.group(1), ai)
        _KB_MAPS = (sorted(addrs), name2addr)
    return _KB_MAPS


_KB_SOURCE_FUNCS = None


def _kb_source_funcs() -> dict[str, list[dict]]:
    """Map a normalized source path → list of that TU's ported functions.

    Keyed by the object's ``source`` (e.g. ``networking/network_game_globals.c``)
    and, when present, a function's own ``source_path`` override.  Each entry is
    ``{"name", "addr"}`` for a ported function, where name is the declared symbol
    (matching how vc71_verify labels its score lines) or ``FUN_<addr>``.
    Used to enumerate the functions a TU was expected to score, so a whole-TU
    compile failure can be flagged per function rather than vanishing silently.
    """
    global _KB_SOURCE_FUNCS
    if _KB_SOURCE_FUNCS is not None:
        return _KB_SOURCE_FUNCS
    out: dict[str, list[dict]] = {}
    try:
        data = json.loads((REPO_ROOT / "kb.json").read_text())
    except (OSError, json.JSONDecodeError):
        _KB_SOURCE_FUNCS = out
        return out
    for obj in data.get("objects", []):
        obj_src = obj.get("source") or ""
        for fn in obj.get("functions") or []:
            if not fn.get("ported"):
                continue
            a = fn.get("addr")
            if not a:
                continue
            try:
                ai = int(a, 16)
            except (ValueError, TypeError):
                continue
            name = f"FUN_{ai:08x}"
            m = re.search(r"([A-Za-z_]\w*)\s*\(", fn.get("decl", "") or "")
            if m:
                name = m.group(1)
            src = fn.get("source_path") or obj_src
            if not src:
                continue
            out.setdefault(src, []).append({"name": name, "addr": f"0x{ai:x}"})
    _KB_SOURCE_FUNCS = out
    return out


def _expected_ported_functions(src_rel: str) -> list[dict]:
    """Ported functions expected from a TU, matched by suffix on the kb source
    (src_rel is repo-root-relative, e.g. ``src/halo/networking/x.c``; kb sources
    are src/halo-relative, e.g. ``networking/x.c``)."""
    table = _kb_source_funcs()
    norm = src_rel.replace("\\", "/")
    for key, fns in table.items():
        k = key.replace("\\", "/")
        if norm == k or norm.endswith("/" + k) or norm.endswith(k):
            return fns
    return []


def _func_span(fn_name: str):
    """Byte span of a function, or None when it is not tracked in kb.json.

    Delegates to vc71_verify._func_span, which reads the committed bounds table
    (tools/verify/function_bounds.json).  The delegation is load-bearing: the
    verifier CUTS each reference from that same table entry, so a gate using a
    different notion of a function's size would be judging a reference against a
    span the reference was never built from.  The old fallback below -- the
    kb.json listing gap -- overshoots wherever the listing has a hole, which is
    exactly how correct-but-short references used to read as truncated; it
    survives only for the case where the import is unavailable.
    """
    try:
        from vc71_verify import _func_span as _verify_func_span
        return _verify_func_span(fn_name)
    except ImportError:
        pass
    addrs, name2addr = _kb_maps()
    addr = name2addr.get(fn_name)
    if addr is None:
        m = re.match(r"FUN_0*([0-9a-fA-F]+)$", fn_name)
        if m:
            addr = int(m.group(1), 16)
    if addr is None or not addrs:
        return None
    i = bisect.bisect_right(addrs, addr)
    return (addrs[i] - addr) if i < len(addrs) else None


def _reference_valid(n_r, span):
    """Whether a delinked reference is a usable byte-match oracle.

    A reference's instruction count must plausibly match the function's known
    byte size:
    - n_r * 15 (max x86 instruction length) < span  => truncated (too few insns
      to cover the function's bytes; e.g. a 144-byte function whose reference is
      a single instruction).
    - n_r > span                                     => bloated (more insns than
      bytes is impossible for real code — the reference swallowed neighbours).
    Both are hard bounds with no false positives on legitimately tiny functions
    (their span is small too).  Returns (ok: bool, reason: str).
    """
    if not n_r:
        return False, "reference symbol empty or absent"
    if span and n_r * 15 < span:
        return False, (f"reference/span inconsistent: {n_r} insns cannot fill "
                       f"{span} bytes (reference truncated, or the span is "
                       f"wrong -- check delinked bounds)")
    if span and n_r > span:
        return False, f"bloated reference: {n_r} insns exceed {span} bytes"
    return True, ""


# ---------------------------------------------------------------------------
# vc71_verify runner
# ---------------------------------------------------------------------------

def run_vc71_verify(source: Path, no_cache: bool = True,
                    function: str | None = None,
                    drops_out: list | None = None,
                    meta_out: dict | None = None) -> dict[str, dict]:
    """Run vc71_verify on a source file; return {fn_name: {score, n_c, n_r}}.

    Defaults to --no-cache so stale .obj files from previous compilations do
    not mask source changes: vc71_verify's fast-path will read a stale .obj
    when the source hash misses the SQLite cache, producing phantom cache hits.
    The compile step is fast (~0.1s/file) so the accuracy cost is acceptable.

    If ``drops_out`` is provided, functions vc71_verify reported it could not
    score against any valid reference (DROP lines) are appended to it as
    ``{"function", "reason", "span_bytes"}`` dicts.  These never appear in the
    returned score dict — they exist only in ``drops_out``.

    Whole-TU calls (``function is None``) are memoized across processes by
    ``_measure_key`` -- see MEASURE_CACHE_PATH for why and for what makes a hit
    sound.  This is NOT the vc71_verify SQLite cache the --no-cache flag above
    disables: that one is keyed by source hash alone and can serve a stale .obj;
    this one is keyed by every input that can move a score, and stores the parsed
    result rather than an object file.  Set VC71_NO_MEASURE_MEMO=1 to bypass.
    """
    memo_key = None
    if function is None and not os.environ.get("VC71_NO_MEASURE_MEMO"):
        memo_key = _measure_key(source)
    if memo_key is not None:
        hit = _load_measure_memo().get(memo_key)
        if hit is not None:
            if drops_out is not None:
                drops_out.extend(hit.get("drops", []))
            if meta_out is not None:
                meta_out.update(hit.get("meta", {}))
            return hit.get("out", {})

    drops_start = len(drops_out) if drops_out is not None else 0
    cmd = [sys.executable, str(VC71_VERIFY), str(source), "--quiet"]
    if function:
        cmd.extend(["--function", function])
    if no_cache:
        cmd.append("--no-cache")
    result = subprocess.run(cmd, capture_output=True, text=True, cwd=REPO_ROOT)
    if meta_out is not None:
        combined = (result.stderr or result.stdout or "")
        meta_out["returncode"] = result.returncode
        meta_out["stderr_tail"] = [l for l in combined.strip().splitlines()[-6:]]
    out = {}
    synth_refs: set[str] = set()
    for line in (result.stdout + result.stderr).splitlines():
        m = _LINE_RE.search(line)
        if m:
            o = _OPND_RE.search(line)
            out[m.group(1)] = {
                "score": float(m.group(2)),
                "n_c": int(m.group(3)),
                "n_r": int(m.group(4)),
                "opnd_percent": float(o.group(1)) if o else None,
            }
            continue
        s = _SYNTHREF_RE.match(line)
        if s:
            synth_refs.add(s.group(1))
            continue
        if drops_out is not None:
            d = _DROP_RE.search(line)
            if d:
                drops_out.append({
                    "function": d.group(1),
                    "reason": d.group(2).strip(),
                    "span_bytes": int(d.group(3)),
                })
    # Tag provenance. Absence of the key means "delinked reference", the normal
    # case; only the synthesized minority is marked, so existing consumers and
    # existing baseline entries keep their meaning unchanged.  A score line can
    # be keyed by the reference's own symbol rather than the source name, so
    # fall back to the trailing-component match used elsewhere in this file.
    for fn in synth_refs:
        if fn in out:
            out[fn]["ref"] = "synth"
            continue
        for k in out:
            if k.rsplit("::", 1)[-1] == fn:
                out[k]["ref"] = "synth"
                break

    if meta_out is not None:
        if result.returncode != 0:
            meta_out["status"] = "subprocess_failed"
        elif not out and not (drops_out or []):
            meta_out["status"] = "parse_failed" if (result.stdout + result.stderr).strip() else "compile_failed"
        else:
            meta_out["status"] = "ok"

    if memo_key is not None:
        # Only cache a run that actually produced a measurement.  A compile
        # failure or unparseable output must re-measure next time -- caching it
        # would turn a transient toolchain hiccup into a permanent verdict.
        # Note this keys off parsed output, NOT the exit code: vc71_verify exits
        # 1 whenever any function scores below its threshold, which is a normal
        # result with 164 perfectly good score lines attached.
        new_drops = drops_out[drops_start:] if drops_out is not None else []
        # A TU with no delinked reference at all scores nothing, deterministically,
        # and there is no cheaper way to find out than asking -- vc71_verify exits
        # 1 for this exactly as it does for a compile failure, so the exit code
        # cannot tell them apart.  Match the structural marker instead: "no unit
        # in objdiff.json" is a fact about the repo, safe to remember, while a
        # compile failure may be a transient toolchain hiccup that a re-run fixes
        # (and would NOT invalidate this key, since the key covers source and
        # tooling but not the wine/CL environment).  11 of the 26 TUs in the
        # maintain commit were in this state, each paying a full subprocess twice
        # per hook to learn nothing.
        structural_miss = (
            not out and not new_drops
            and any("No usable objdiff.json unit" in l
                    for l in (meta_out or {}).get("stderr_tail", []))
        )
        if out or new_drops or structural_miss:
            global _MEASURE_MEMO_DIRTY
            entry = {
                "out": out,
                "drops": list(new_drops),
                "meta": {k: meta_out[k] for k in ("returncode", "stderr_tail", "status")
                         if meta_out is not None and k in meta_out},
            }
            with _MEASURE_MEMO_LOCK:
                _load_measure_memo()[memo_key] = entry
                _MEASURE_MEMO_DIRTY = True
    return out


# ---------------------------------------------------------------------------
# update command
# ---------------------------------------------------------------------------

def _measure_source(src: Path):
    """Measure one source TU with no shared-state mutation and no live printing.

    Returns ``(src_rel, honest_slice, flagged_slice, scored, log)``:
    - ``honest_slice``: ``{fn: {score, source, n_c, n_r}}`` for functions scored
      against a valid reference.
    - ``flagged_slice``: re-delink/attention queue entries for this TU.
    - ``scored``: ``[(fn_name, score)]`` valid scored functions, for the caller's
      serial floor merge.
    - ``log``: status lines to print (drops / invalid-reference / compile-fail).

    Thread-safe: only reads module caches that the caller pre-warms.  The floor
    merge and all baseline mutation happen serially in ``_apply_floor``.
    """
    honest_slice: dict = {}
    flagged_slice: list = []
    scored: list = []
    log: list = []
    drops: list = []
    meta: dict = {}
    results = run_vc71_verify(src, drops_out=drops, meta_out=meta)
    src_rel = str(src.relative_to(REPO_ROOT))

    # Compile-failure gate: a TU that produced neither a score line nor a DROP
    # line, yet was expected to score ported functions, almost certainly failed
    # to compile (e.g. a stale decl.h C2371).  Such a TU used to vanish silently
    # from vc71_current.json and render as a blank "—".  Flag each expected
    # function loudly into the re-delink/attention queue instead.
    if not results and not drops:
        expected = _expected_ported_functions(src_rel)
        if expected:
            rc = meta.get("returncode")
            tail = meta.get("stderr_tail") or []
            reason = f"vc71 compile/score failed (rc={rc})"
            log.append(f"  ✗ {src_rel}: {reason}; {len(expected)} function(s) "
                       f"flagged (compile_failed)")
            for l in tail:
                log.append(f"      {l}")
            for fn in sorted(expected, key=lambda x: x["name"]):
                flagged_slice.append({
                    "function": fn["name"], "source": src_rel,
                    "addr": fn.get("addr"), "n_r": None, "span_bytes": None,
                    "state": "compile_failed", "reason": reason,
                })
            return src_rel, honest_slice, flagged_slice, scored, log

    # Functions vc71_verify could not score against any valid reference never
    # produce a score line, so the gate below never sees them.  Record them
    # directly in the re-delink queue so it is complete — not just the
    # gate-flagged (present-but-invalid-reference) subset.
    for d in sorted(drops, key=lambda x: x["function"]):
        flagged_slice.append({"function": d["function"], "source": src_rel,
                              "n_r": None, "span_bytes": d.get("span_bytes"),
                              "state": "no_reference", "reason": d["reason"]})
        log.append(f"  ⚠ {d['function']}: no valid reference — {d['reason']}; "
                   f"skipped (flagged for re-delink)")

    for fn_name, info in sorted(results.items()):
        new_score = info["score"]

        # Reference-validity gate: never trust a score computed against a
        # truncated/absent reference — skip it and flag for re-delink so a
        # broken reference cannot pollute the floor or the dashboard.
        n_r = info.get("n_r")
        span = _func_span(fn_name)
        ok, reason = _reference_valid(n_r, span)
        if not ok:
            flagged_slice.append({"function": fn_name, "source": src_rel,
                                  "n_r": n_r, "span_bytes": span,
                                  "state": "no_reference", "reason": reason})
            log.append(f"  ⚠ {fn_name}: reference invalid — {reason}; "
                       f"skipped (flagged for re-delink)")
            continue

        honest_slice[fn_name] = {"score": new_score, "source": src_rel,
                                 "n_c": info.get("n_c"), "n_r": n_r}
        # Advisory operand-normalized score; omitted entirely when vc71_verify
        # did not print one (older cached lines), never written as null noise.
        if info.get("opnd_percent") is not None:
            honest_slice[fn_name]["opnd_percent"] = info["opnd_percent"]
        # Reference provenance; only the synthesized minority is tagged, so an
        # absent key still means "delinked" for every pre-existing entry.
        if info.get("ref"):
            honest_slice[fn_name]["ref"] = info["ref"]
        scored.append((fn_name, new_score))

    return src_rel, honest_slice, flagged_slice, scored, log


def _apply_floor(baseline: dict, src_rel: str, scored: list, force: bool,
                 opnd_map: dict = None, ref_map: dict = None):
    """Apply the raise-only floor merge for one TU's scored functions into
    ``baseline`` (in place).  Serial and order-independent for distinct
    functions.  Returns ``(n_changed, log)``.

    ``opnd_map`` optionally supplies the advisory operand-normalized score per
    function (``{fn_name: pct}``).  It rides along on entries this call already
    rewrites — it never causes a rewrite of its own, so untouched entries in the
    committed floor stay byte-identical.
    """
    n_changed = 0
    log: list = []

    def _entry(fn_name, score):
        e = {"score": score, "source": src_rel}
        opnd = (opnd_map or {}).get(fn_name)
        if opnd is not None:
            e["opnd_percent"] = opnd
        # Provenance rides along on entries this call already rewrites, exactly
        # like opnd_percent: it never triggers a rewrite of its own, so
        # untouched floor entries stay byte-identical.
        ref = (ref_map or {}).get(fn_name)
        if ref:
            e["ref"] = ref
        return e

    for fn_name, new_score in scored:
        old_entry = baseline.get(fn_name)
        old_score = old_entry["score"] if old_entry else None

        if old_score is None:
            baseline[fn_name] = _entry(fn_name, new_score)
            log.append(f"  + {fn_name}: {new_score:.1f}% (new)")
            n_changed += 1
        elif new_score > old_score + 0.1:
            # Improvement: always raise the floor
            baseline[fn_name] = _entry(fn_name, new_score)
            log.append(f"  ↑ {fn_name}: {old_score:.1f}% → {new_score:.1f}%")
            n_changed += 1
        elif new_score < old_score - 0.1:
            if force:
                baseline[fn_name] = _entry(fn_name, new_score)
                log.append(f"  ↓ {fn_name}: {old_score:.1f}% → {new_score:.1f}% (forced lower)")
                n_changed += 1
            else:
                log.append(f"  ! {fn_name}: {old_score:.1f}% → {new_score:.1f}%"
                           f" (drop; use --force to lower floor)")
        # else: unchanged within tolerance, nothing to do
    return n_changed, log


def _verify_source(src: Path, baseline: dict, force: bool):
    """Verify one source TU serially: measure, print, apply the floor merge into
    ``baseline`` (in place), and return ``(n_changed, honest_slice, flagged_slice)``.
    Used by ``update`` (single-file, live output).  ``populate`` uses the
    ``_measure_source`` / ``_apply_floor`` split directly for parallelism."""
    print(f"Verifying {src.relative_to(REPO_ROOT)} ...", flush=True)
    src_rel, honest_slice, flagged_slice, scored, log = _measure_source(src)
    for l in log:
        print(l)
    n_changed, floor_log = _apply_floor(
        baseline, src_rel, scored, force,
        opnd_map={k: v.get("opnd_percent") for k, v in honest_slice.items()},
        ref_map={k: v.get("ref") for k, v in honest_slice.items()})
    for l in floor_log:
        print(l)
    return n_changed, honest_slice, flagged_slice


def cmd_update(args, honest_out: dict | None = None,
               flagged_out: list | None = None) -> int:
    if not args.source:
        print("Error: --source is required for update.", file=sys.stderr)
        return 1

    baseline = load_baseline()
    total_changed = 0

    for src_str in args.source:
        src = Path(src_str)
        if not src.is_absolute():
            src = REPO_ROOT / src
        if not src.exists():
            print(f"  SKIP {src_str} (not found)", file=sys.stderr)
            continue

        n, honest_slice, flagged_slice = _verify_source(src, baseline, args.force)
        total_changed += n
        if honest_out is not None:
            honest_out.update(honest_slice)
        if flagged_out is not None:
            flagged_out.extend(flagged_slice)

    save_baseline(baseline)
    if total_changed:
        print(f"\nBaseline updated: {total_changed} function(s) changed → {BASELINE_PATH.name}")
    else:
        print("\nBaseline unchanged.")
    return 0


# ---------------------------------------------------------------------------
# check command
# ---------------------------------------------------------------------------

def cmd_check(args) -> int:
    strict = getattr(args, "strict", False)
    threshold = 0 if strict else args.threshold
    sources_filter = set(args.source) if args.source else None

    baseline = load_baseline()
    if not baseline:
        print("Baseline is empty. Run 'update' first.")
        return 1 if strict else 0

    # Group baseline entries by source file
    by_source: dict[str, list[str]] = {}
    for fn_name, entry in baseline.items():
        src = entry.get("source", "")
        if sources_filter:
            # Accept both relative and absolute forms in the filter
            if src not in sources_filter and str(REPO_ROOT / src) not in sources_filter:
                continue
        by_source.setdefault(src, []).append(fn_name)

    if not by_source:
        print("No baseline entries match the given --source filter.")
        return 1 if strict else 0

    regressions = []
    improvements = []
    skipped = 0
    ref_flagged = 0
    strict_failures = []
    checked = 0

    # Measure every source concurrently BEFORE the comparison loop.  Each
    # run_vc71_verify is a subprocess (compile + compare) that releases the GIL,
    # and it dominates this command: on a 29-file staged set the pre-commit hook
    # spent ~6 of its ~7 minutes here, single-threaded, while cmd_update further
    # down this same file already had a ThreadPoolExecutor.  Nothing below reads
    # another source's results, so measuring up-front is order-independent; the
    # comparison loop still walks sorted(by_source) serially, so output ordering
    # and every regression/improvement verdict are unchanged.
    to_measure = [
        (src_rel, REPO_ROOT / src_rel)
        for src_rel in sorted(by_source)
        if (REPO_ROOT / src_rel).exists() and not (strict and not (REPO_ROOT / src_rel).is_file())
    ]
    premeasured: dict[str, tuple] = {}
    if to_measure:
        # Pre-warm lazy module caches so worker threads never race on first init.
        _kb_maps(); _kb_source_funcs(); _decl_index()
        n_workers = min(len(to_measure), max(1, (os.cpu_count() or 4) - 2), 8)

        def _measure(src_path):
            meta: dict = {}
            drops: list = []
            try:
                return run_vc71_verify(src_path, drops_out=drops, meta_out=meta), drops, meta, None
            except Exception as exc:  # re-raised or recorded serially below
                return None, drops, meta, exc

        if n_workers > 1:
            if not args.quiet:
                print(f"Verifying {len(to_measure)} source file(s) "
                      f"({n_workers} workers)...", flush=True)
            with ThreadPoolExecutor(max_workers=n_workers) as ex:
                futs = {ex.submit(_measure, p): rel for rel, p in to_measure}
                for fut, rel in futs.items():
                    premeasured[rel] = fut.result()
        else:
            for rel, p in to_measure:
                premeasured[rel] = _measure(p)

    for src_rel, fn_names in sorted(by_source.items()):
        src_path = REPO_ROOT / src_rel
        if not src_path.exists() or (strict and not src_path.is_file()):
            skipped += len(fn_names)
            if not args.quiet:
                print(f"  SKIP {src_rel} (file not found)")
            if strict:
                strict_failures.append(f"{src_rel or '<missing source>'}: file not found")
            continue

        results, drops, meta, exc = premeasured[src_rel]
        if exc is not None:
            if not strict:
                raise exc
            strict_failures.append(f"{src_rel}: vc71_verify failed: {exc}")
            continue

        if strict and meta.get("status") not in (None, "ok"):
            strict_failures.append(f"{src_rel}: vc71_verify {meta['status']}")
            continue

        if strict and drops:
            for drop in drops:
                strict_failures.append(
                    f"{drop['function']}: invalid or missing delinked reference: "
                    f"{drop['reason']}"
                )

        for fn_name in fn_names:
            current = results.get(fn_name)
            if current is None:
                # Function not found in compiled output — may be unported or renamed
                if strict:
                    strict_failures.append(f"{fn_name}: no parsed vc71_verify result")
                continue
            # Reference-validity gate: a truncated/absent reference produces a
            # spuriously low current score.  Skip it (don't raise a false
            # regression) — it is tracked separately in the re-delink queue.
            ok, _ = _reference_valid(current.get("n_r"), _func_span(fn_name))
            if not ok:
                ref_flagged += 1
                if not args.quiet:
                    print(f"  ⚠ {fn_name}: reference invalid — skipped "
                          f"(flagged for re-delink, not a regression)")
                if strict:
                    strict_failures.append(f"{fn_name}: invalid delinked reference")
                continue
            try:
                baseline_score = float(baseline[fn_name]["score"])
                current_score = float(current["score"])
            except (KeyError, TypeError, ValueError):
                if strict:
                    strict_failures.append(f"{fn_name}: invalid baseline or parsed score")
                continue
            checked += 1
            delta = current_score - baseline_score
            if delta < -threshold:
                regressions.append((fn_name, baseline_score, current_score, src_rel))
            elif delta > threshold and not args.quiet:
                improvements.append((fn_name, baseline_score, current_score, src_rel))

    if improvements:
        print(f"Improvements ({len(improvements)}):")
        for fn, base, curr, src in improvements:
            print(f"  ↑ {fn}: {base:.1f}% → {curr:.1f}% in {src}")
        print()

    if regressions:
        print(f"REGRESSIONS ({len(regressions)}):")
        for fn, base, curr, src in regressions:
            drop = base - curr
            print(f"  ✗ {fn}: {base:.1f}% → {curr:.1f}% (-{drop:.1f}pp) in {src}")
        print(
            "\nHint: investigate the change, fix the regression, then re-run:\n"
            "  python3 tools/verify/vc71_regression.py update --source <file>"
        )
    if strict and (strict_failures or checked == 0):
        if checked == 0:
            strict_failures.append("zero functions checked")
        print("STRICT CHECK FAILED:")
        for failure in strict_failures:
            print(f"  ✗ {failure}")
        return 1

    if regressions:
        return 1

    # `checked` counts functions we actually compared against the floor.  It used
    # to be overwritten here with (baseline entries - skipped - ref_flagged),
    # which silently counted every function whose TU produced NO measurement at
    # all -- a TU with no delinked reference scores nothing, each of its
    # functions hits the `current is None` continue above, and the recomputed
    # total still reported them as passing.  Observed on the 26-TU maintain
    # commit: "1589 functions" reported, 948 actually measured.  Report both, so
    # a green gate cannot be mistaken for coverage it does not have.
    expected = sum(len(v) for v in by_source.values())
    unmeasured = expected - checked - skipped - ref_flagged
    notes = []
    if ref_flagged:
        notes.append(f"{ref_flagged} skipped for invalid reference")
    if unmeasured > 0:
        notes.append(f"{unmeasured} not measured (no reference or not in "
                     f"compiled output)")
    if skipped:
        notes.append(f"{skipped} source file(s) missing")
    note = (", " + ", ".join(notes)) if notes else ""
    print(f"OK — no regressions ({checked} of {expected} functions measured in "
          f"{len(by_source)} source file(s){note}).")
    return 0


# ---------------------------------------------------------------------------
# show command
# ---------------------------------------------------------------------------

def cmd_show(args) -> int:
    baseline = load_baseline()
    if not baseline:
        print("Baseline is empty.")
        return 0

    by_source: dict[str, list] = {}
    for fn_name, entry in baseline.items():
        by_source.setdefault(entry.get("source", "?"), []).append(
            (fn_name, entry["score"])
        )

    total = len(baseline)
    perfect = 0
    above90 = 0

    for src in sorted(by_source):
        fns = sorted(by_source[src], key=lambda x: x[1])
        print(f"\n{src}:")
        for fn_name, score in fns:
            marker = " ✓" if score >= 99.9 else ""
            print(f"  {fn_name:<44s} {score:6.1f}%{marker}")
            if score >= 99.9:
                perfect += 1
            if score >= 90.0:
                above90 += 1

    print(
        f"\nTotal: {total} functions | "
        f"{perfect} at 100% | {above90} at ≥90%"
    )
    return 0


# ---------------------------------------------------------------------------
# populate command
# ---------------------------------------------------------------------------

def cmd_populate(args) -> int:
    """Scan objdiff.json for source files with a delinked reference and refresh
    the floored baseline, the honest current scores, and the re-delink queue.

    With ``--incremental`` only TUs whose inputs changed since the last populate
    are re-verified; unchanged TUs carry their cached honest/flagged results
    forward (from ``populate_state.json``).  A change to the verify/compare
    tooling, kb.json, or the delinked chunk directory bumps the tool epoch and
    forces a full re-verify.  Without the flag, every TU is re-verified (the
    conservative default used by CI).
    """
    # Pin decl.h == kb.json before fingerprinting or compiling any TU.  The
    # per-TU decl digest (in _tu_fingerprint) reads this freshly generated
    # header, so a header that was stale on disk is corrected here and the
    # affected TUs are re-verified rather than reusing a stale/empty slice.
    print("Regenerating build/generated/decl.h from kb.json ...", flush=True)
    regen_decl_header()

    objdiff = REPO_ROOT / "objdiff.json"
    if not objdiff.exists():
        print("objdiff.json not found.", file=sys.stderr)
        return 1

    data = json.loads(objdiff.read_text())
    units = data.get("units", [])

    # Collect (src_abs, src_rel, ref_abs) for TUs whose reference exists on disk.
    tus = []
    seen = set()
    for unit in units:
        src = unit.get("metadata", {}).get("source_path", "")
        ref = unit.get("base_path", "")
        if not (src and ref):
            continue
        ref_abs = REPO_ROOT / ref
        full = REPO_ROOT / src
        if full.exists() and ref_abs.exists() and str(full) not in seen:
            seen.add(str(full))
            tus.append((full, str(Path(src)), ref_abs))

    if not tus:
        print("No source files with delinked references found in objdiff.json.")
        return 1

    incremental = getattr(args, "incremental", False)
    force = getattr(args, "force", False)
    epoch = _tool_epoch()
    prev = load_populate_state() if incremental else {}
    prev_ok = incremental and prev.get("tool_epoch") == epoch
    prev_tus = prev.get("tus", {}) if prev_ok else {}
    if incremental and prev and not prev_ok:
        print("Tool/kb.json/delinked epoch changed → full re-verify "
              "(incremental cache invalidated).")

    mode = "incremental" if incremental else "full"
    print(f"Populating baseline from {len(tus)} source files ({mode})...")

    baseline = load_baseline()
    honest: dict[str, dict] = {}
    flagged: list[dict] = []
    new_state: dict[str, dict] = {}
    total_changed = 0
    n_cached = 0
    n_verified = 0

    # Partition into unchanged (reuse cached slice) and changed (re-verify).
    to_verify: list = []
    for src_abs, src_rel, ref_abs in tus:
        fp = _tu_fingerprint(src_abs, ref_abs)
        cached = prev_tus.get(src_rel)
        if cached and cached.get("fp") == fp:
            # Inputs unchanged since last populate → reuse this TU's results.
            # The floor is already correct (untouched); we only need to carry
            # the honest + flagged slices forward so the rewritten current/queue
            # files stay complete.
            for fn, rec in cached.get("honest", {}).items():
                honest[fn] = rec
            flagged.extend(cached.get("flagged", []))
            new_state[src_rel] = cached
            n_cached += 1
            continue
        to_verify.append((src_abs, src_rel, ref_abs, fp))

    # Measure changed TUs concurrently (each verify is subprocess-bound, so the
    # GIL is released during the compile+compare).  The floor merge and all
    # baseline mutation happen serially below in deterministic TU order, so the
    # result is byte-identical to a serial run.
    measured: dict = {}
    if to_verify:
        # Pre-warm lazy module caches so worker threads never race on first init.
        _kb_maps(); _kb_source_funcs(); _decl_index()
        n_workers = min(len(to_verify), max(1, (os.cpu_count() or 4) - 2), 8)
        print(f"Re-verifying {len(to_verify)} changed TU(s) "
              f"({n_workers} workers)...", flush=True)
        with ThreadPoolExecutor(max_workers=n_workers) as ex:
            futs = {ex.submit(_measure_source, item[0]): item[1]
                    for item in to_verify}
            for fut, key in futs.items():
                measured[key] = fut.result()

    for src_abs, src_rel, ref_abs, fp in to_verify:
        res = measured.get(src_rel)
        if res is None:
            continue
        _msrc_rel, honest_slice, flagged_slice, scored, log = res
        print(f"Verifying {Path(src_rel)} ...", flush=True)
        for l in log:
            print(l)
        n, floor_log = _apply_floor(
            baseline, src_rel, scored, force,
            opnd_map={k: v.get("opnd_percent") for k, v in honest_slice.items()},
        ref_map={k: v.get("ref") for k, v in honest_slice.items()})
        for l in floor_log:
            print(l)
        total_changed += n
        honest.update(honest_slice)
        flagged.extend(flagged_slice)
        new_state[src_rel] = {
            "fp": fp,
            "ref": str(ref_abs.relative_to(REPO_ROOT)),
            "honest": honest_slice,
            "flagged": flagged_slice,
        }
        n_verified += 1

    # Backfill reference provenance onto floor entries whose score is unchanged.
    # `_apply_floor` only stamps entries it rewrites, so a function scored from a
    # synthesized reference before this field existed -- or one whose score has
    # simply not moved since -- would sit in the COMMITTED floor with no way to
    # tell it apart from a Ghidra-delinked score.  vc71_current.json is
    # gitignored, so the floor is the only artifact a reviewer actually sees.
    # Adds a key only; never touches `score`, so the raise-only guarantee holds.
    n_prov = 0
    for fn_name, cur in honest.items():
        ref = cur.get("ref")
        entry = baseline.get(fn_name)
        if ref and entry is not None and entry.get("ref") != ref:
            entry["ref"] = ref
            n_prov += 1

    save_baseline(baseline)
    if total_changed:
        print(f"\nBaseline updated: {total_changed} function(s) changed → {BASELINE_PATH.name}")
    else:
        print("\nBaseline unchanged.")
    if n_prov:
        print(f"Reference provenance stamped on {n_prov} floor entry(ies) "
              f"(scored against an XBE-synthesized reference).")

    # Honest current scores drive the dashboard; the floored baseline stays the
    # CI tripwire.  The validity report is the re-delink work queue.
    save_current(honest)
    save_validity(flagged)
    if incremental:
        save_populate_state(epoch, new_state)
        print(f"Incremental: {n_verified} TU(s) re-verified, {n_cached} cached "
              f"→ {POPULATE_STATE_PATH.relative_to(REPO_ROOT)}")
    print(f"\nHonest current scores: {len(honest)} functions → {CURRENT_PATH.name}")
    if flagged:
        n_compile = sum(1 for f in flagged if f.get("state") == "compile_failed")
        n_ref = len(flagged) - n_compile
        print(f"Attention queue: {len(flagged)} function(s) not scored "
              f"→ {VALIDITY_PATH.relative_to(REPO_ROOT)}")
        if n_ref:
            print(f"  {n_ref} with broken/truncated delinked reference "
                  f"(re-delink before their scores can be trusted).")
        if n_compile:
            # A whole-TU VC71 compile failure (often a clang-ism the C89 CL.Exe
            # rejects: __attribute__, inline in a header, C99 mixed decls).  These
            # have NO byte-match evidence until the TU compiles under VC71.
            compile_srcs = sorted({f["source"] for f in flagged
                                   if f.get("state") == "compile_failed"})
            print(f"  {n_compile} in {len(compile_srcs)} TU(s) that FAIL to "
                  f"compile under VC71 (no byte-match evidence):")
            for s in compile_srcs[:12]:
                print(f"    {s}")
            if len(compile_srcs) > 12:
                print(f"    ... and {len(compile_srcs) - 12} more (see report)")
    return 0


# ---------------------------------------------------------------------------
# loadw command -- retroactive load-width (int vs int16/int8) sweep
# ---------------------------------------------------------------------------

def _discover_ref_sources() -> list[str]:
    """Source files that have a delinked reference (from objdiff.json)."""
    objdiff = REPO_ROOT / "objdiff.json"
    if not objdiff.exists():
        return []
    data = json.loads(objdiff.read_text())
    srcs = []
    for unit in data.get("units", []):
        src = unit.get("metadata", {}).get("source_path", "")
        ref = unit.get("base_path", "")
        if src and ref and (REPO_ROOT / ref).exists():
            full = REPO_ROOT / src
            if full.exists() and str(full) not in srcs:
                srcs.append(str(full))
    return srcs


def run_vc71_loadw(source: Path) -> list[str]:
    """Run vc71_verify --loadw-only on a source file; return the report lines
    (function headers + LOADW detail lines). Empty list means no load-width
    differences (or the file failed to compile/compare)."""
    cmd = [sys.executable, str(VC71_VERIFY), str(source),
           "--loadw-only", "--no-cache"]
    result = subprocess.run(cmd, capture_output=True, text=True, cwd=REPO_ROOT)
    lines = []
    for line in (result.stdout + result.stderr).splitlines():
        s = line.rstrip()
        if "[LOADW-WARN]" in s or s.strip().startswith("LOADW:"):
            lines.append(s)
    return lines


def cmd_loadw(args) -> int:
    """Sweep every source file with a delinked reference for load-width
    (int vs int16_t/int8_t) differences and write a report. This finds the
    same class as the c10 tag_groups:3089 pg_surf crash across the whole tree.
    """
    if args.source:
        sources = [str(REPO_ROOT / s if not Path(s).is_absolute() else s)
                   for s in args.source]
    else:
        sources = _discover_ref_sources()
    if not sources:
        print("No source files with delinked references found.", file=sys.stderr)
        return 1

    out_path = REPO_ROOT / "artifacts" / "audit" / "loadw_sweep.txt"
    out_path.parent.mkdir(parents=True, exist_ok=True)

    total_hits = 0
    files_with_hits = 0
    report = []
    print(f"Sweeping {len(sources)} source file(s) for load-width differences...\n",
          flush=True)
    for src_str in sorted(sources):
        src = Path(src_str)
        rel = src.relative_to(REPO_ROOT) if src.is_absolute() and str(src).startswith(str(REPO_ROOT)) else src
        lines = run_vc71_loadw(src)
        hits = [l for l in lines if l.strip().startswith("LOADW:")]
        if lines:
            files_with_hits += 1
            total_hits += len(hits)
            header = f"=== {rel} ({len(hits)} finding(s)) ==="
            print(header, flush=True)
            for l in lines:
                print(l)
            report.append(header)
            report.extend(lines)
            report.append("")

    summary = (f"\nLoad-width sweep: {total_hits} finding(s) across "
               f"{files_with_hits}/{len(sources)} file(s).")
    print(summary)
    out_path.write_text("\n".join(report) + summary + "\n")
    print(f"Report written to {out_path.relative_to(REPO_ROOT)}")
    print("NOTE: findings are review items -- verify each against disassembly "
          "(a narrow field read wide, or vice versa) before any fix.")
    return 0


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------

def build_parser():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    sub = ap.add_subparsers(dest="command")

    p_update = sub.add_parser("update", help="Update baseline for source file(s)")
    p_update.add_argument("--source", "-s", nargs="+",
                          help="Source .c file(s) to run vc71_verify on")
    p_update.add_argument("--force", action="store_true",
                          help="Allow lowering stored floors (normally disallowed)")

    p_check = sub.add_parser("check", help="Check for regressions against baseline")
    p_check.add_argument("--source", "-s", nargs="+",
                         help="Limit check to these source files")
    p_check.add_argument("--threshold", "-t", type=float, default=2.0,
                         help="Acceptable drop in pp before flagging (default: 2.0)")
    p_check.add_argument("--quiet", "-q", action="store_true",
                         help="Suppress improvement messages")
    p_check.add_argument("--strict", action="store_true",
                         help="Fail closed on any missing or invalid verification evidence")

    sub.add_parser("show", help="Display current baseline")

    p_pop = sub.add_parser("populate",
                           help="Populate baseline for all objdiff.json source files")
    p_pop.add_argument("--force", action="store_true",
                       help="Allow lowering floored baseline to honest scores "
                            "(one-time rebaseline after reference/measurement fixes)")
    p_pop.add_argument("--incremental", action="store_true",
                       help="Only re-verify TUs whose source or reference changed "
                            "since the last populate (state in "
                            "artifacts/audit/populate_state.json). A tooling, "
                            "kb.json, or delinked-chunk change forces a full pass.")

    p_loadw = sub.add_parser("loadw",
                             help="Sweep for load-width (int vs int16/int8) differences")
    p_loadw.add_argument("--source", "-s", nargs="+",
                         help="Limit sweep to these source files (default: all with a ref)")

    return ap


def main():
    parser = build_parser()
    args = parser.parse_args()

    dispatch = {
        "update": cmd_update,
        "check": cmd_check,
        "show": cmd_show,
        "populate": cmd_populate,
        "loadw": cmd_loadw,
    }
    fn = dispatch.get(args.command)
    if fn is None:
        parser.print_help()
        sys.exit(1)
    try:
        rc = fn(args)
    finally:
        # Persist whatever we measured even on a non-zero exit: a `check` that
        # found a regression measured every staged TU to get there, and the
        # operator's very next action is to re-run it.
        flush_measure_memo()
    sys.exit(rc)


if __name__ == "__main__":
    main()
