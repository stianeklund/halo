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
        Limits to specific files when --source is given.  Exits 1 when, for any
        baselined function:
          * the score dropped more than --threshold pp (default 2);
          * the REFERENCE changed (measured ref_sha != the entry's ref_sha) --
            the two scores describe different bytes, so they are not compared;
          * the run produced no score for it at all (deleted, renamed, dropped
            for a missing bounds entry).  Silence is a failure, not a skip.
        The one non-fatal gap is a whole-TU compile failure, which is an
        infrastructure fact (no VC71 toolchain) rather than a lift result; it is
        reported loudly and fails under --strict.

    bounds-gate
        Pre-commit helper for a staged tools/verify/function_bounds.json: prints
        the source files whose baselined functions sit on a CHANGED bounds entry
        (their references moved), or exits 1 with instructions when the staged
        commit does not also carry a re-baselined vc71_scores.json.

    show
        Print the current baseline in a human-readable table.

    populate [--rebaseline] [--source ...] [--workers N]
        Re-derive scores for every ported function in kb.json by scanning
        all relevant source files (slow, but useful for initial population
        or after bulk changes). Writes new floors for any function not yet
        in the baseline.

        --rebaseline switches the merge from raise-only to REPLACE, stamps
        reference provenance (addr/end/kind/n_r/ref_sha) onto every measured
        entry, widens discovery to every kb.json TU with ported functions, and
        journals the pass for `rebaseline-report`. --source shards it.

    rebaseline-report
        Prune stale duplicate keys (same address under two spellings; keep the
        one today's scorer emits) and write artifacts/audit/rebaseline_report.json
        -- per-function old vs new with a reason code, plus aggregates and the
        list of drops that would trip the regression gate.

Baseline schema (version 2), keyed by the symbol the scorer emits:

    "<fn>": {"score": 91.2, "source": "src/halo/main/main.c",
             "addr": "0x100c10", "end": "0x100d54", "kind": "auto",
             "n_r": 28, "ref_sha": "ab12cd34ef567890",
             "opnd_percent": 73.3, "ref": "synth"}

The addr/end/kind/n_r/ref_sha block is reference PROVENANCE: which bytes the
score was measured against. Without it a score change and a reference change
are indistinguishable in a diff.
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
# Schema/semantics version of the memo above.  Bumped whenever what we are
# willing to REMEMBER changes, which the content-derived key cannot express:
# the key hashes the inputs, not the rules for caching a result.
#   1 -> 2: version 1 cached a "structural miss" class -- a TU whose functions
#           had no objdiff.json unit and therefore scored nothing, remembered as
#           a permanent verdict.  Scoring no longer reads objdiff.json at all
#           (references are derived from the XBE + function_bounds.json), so the
#           condition cannot recur, but its cached empty results would keep
#           answering "this TU scores nothing" until the epoch happened to move.
#           Bumping purges them and pins the rule: only a run that produced a
#           measurement (scores or DROPs) is memoized.
MEASURE_MEMO_VERSION = 2
# Cheap stat-keyed cache for the delinked/functions/*.obj content signature; see
# _chunk_dir_signature.  Host-local, gitignored.
CHUNK_SIG_CACHE_PATH = REPO_ROOT / "artifacts" / "audit" / "vc71_chunk_sig.json"
# Cross-shard journal for `populate --rebaseline`; consumed by
# `rebaseline-report` (see record_rebaseline_journal).  Host-local scratch.
REBASELINE_JOURNAL_PATH = REPO_ROOT / "artifacts" / "audit" / "rebaseline_journal.json"
# Snapshot of the floor taken before a re-baseline starts, so the delta report
# can say what each score WAS after the file has already been rewritten.
REBASELINE_PRE_PATH = REPO_ROOT / "artifacts" / "audit" / "rebaseline_pre.json"
# The delta report itself (committed as evidence for the re-baseline).
REBASELINE_REPORT_PATH = REPO_ROOT / "artifacts" / "audit" / "rebaseline_report.json"
# Reference-migration evidence from the bounds-table phase; joined by address to
# explain WHY a score moved (the reference got closer to the truth).
REF_MIGRATION_PATH = REPO_ROOT / "artifacts" / "audit" / "ref_migration_report.json"
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
_ABI_MODEL_RE = re.compile(
    r"\|\s*raw\s+([\d.]+)%\s*\|\s*abi-modeled\s+([\d.]+)%\s*"
    r"\[([a-z_]+):(\d+)\]"
)
# Emitted once per function scored against a reference SYNTHESIZED from the
# pristine XBE rather than delinked by Ghidra (see tools/verify/xbe_reference.py).
# Provenance matters because the two are not identical: measured agreement across
# 1,464 existing chunks is 95.9%.  A score with no delinked reference behind it
# should be identifiable as such in the baseline, not silently equivalent.
_SYNTHREF_RE = re.compile(r"^\s*SYNTHREF\s+(\S+)\s*$")
# Reference provenance, one line per scored function (emitted by vc71_verify even
# under --quiet).  This says WHICH BYTES the score was measured against: the
# derived reference's address range, how its end was determined (`kind`, straight
# from the committed bounds table), its instruction count, and a content hash.
#
#   REFMETA <fn> addr=0x00102e40 end=0x001034aa kind=auto n_r=432 sha=7988df1c...
#
# Recorded into every baseline entry so a score can be audited without re-running
# the scorer: a later reference change (bounds edit, re-derivation) shows up as a
# changed ref_sha, which is the difference between "this lift regressed" and "we
# are no longer measuring the same bytes".  The line shape is pinned by
# test_ref_selection.test_refmeta_line_shape -- change both together.
_REFMETA_RE = re.compile(
    r"^\s*REFMETA (\S+) addr=0x([0-9a-fA-F]+) end=0x([0-9a-fA-F]+) "
    r"kind=(\w+) n_r=(\d+) sha=([0-9a-f]+)\s*$"
)

# Provenance fields carried from REFMETA onto each baseline/current entry.
# Every one of them describes the REFERENCE, never the candidate: a consumer
# diffing two baselines can therefore separate a real score move from a
# reference move without recompiling anything.
PROVENANCE_FIELDS = ("addr", "end", "kind", "n_r", "ref_sha")
# Advisory fields that ride along when the scorer produced them.
_OPTIONAL_FIELDS = (
    "opnd_percent", "raw_mnemonic_pct", "abi_modeled_mnemonic_pct",
    "abi_model", "abi_model_items", "ref",
)
# Fields this module owns and may overwrite when rebuilding an entry.  Anything
# else found on an existing entry is copied forward untouched, so a key some
# other consumer stamped into the baseline is not lost the next time a score is
# refreshed.
_OWNED_FIELDS = ("score", "source") + PROVENANCE_FIELDS + _OPTIONAL_FIELDS

BASELINE_VERSION = 2


def make_score_entry(score: float, source: str, info: dict | None = None,
                     previous: dict | None = None) -> dict:
    """Build one baseline/current entry: {score, source} + provenance + advisory.

    THE single place an entry is constructed.  It exists because there are two
    writers -- this module's floor merge and tools/report/progress_server.py --
    which used to build the dict independently, each keeping only the fields its
    own author knew about.  Whichever ran last silently stripped the other's, so
    a field could not survive a dashboard refresh.

    ``info`` is a parsed vc71_verify result (see run_vc71_verify).  ``previous``
    is the entry being replaced, if any.  Optional fields are written only when
    present, never as null noise.
    """
    entry: dict = {}
    if previous:
        for k, v in previous.items():
            if k not in _OWNED_FIELDS:
                entry[k] = v
    entry["score"] = score
    entry["source"] = source
    info = info or {}
    for k in PROVENANCE_FIELDS + _OPTIONAL_FIELDS:
        if info.get(k) is not None:
            entry[k] = info[k]
    return entry


def backfill_optional_fields(baseline: dict, current: dict) -> int:
    """Stamp fresh advisory/model fields without changing score floors."""
    changed = set()
    for fn_name, info in current.items():
        entry = baseline.get(fn_name)
        if entry is None:
            continue
        for field in _OPTIONAL_FIELDS:
            value = info.get(field)
            if value is not None and entry.get(field) != value:
                entry[field] = value
                changed.add(fn_name)
    return len(changed)


# ---------------------------------------------------------------------------
# Baseline I/O
# ---------------------------------------------------------------------------

def load_baseline() -> dict[str, dict]:
    """Return {fn_name: entry} from the baseline file (see make_score_entry)."""
    if not BASELINE_PATH.exists():
        return {}
    try:
        data = json.loads(BASELINE_PATH.read_text())
        return data.get("scores", {})
    except (json.JSONDecodeError, OSError):
        return {}


def _load_baseline_doc() -> dict:
    """The whole baseline document, for writers that must preserve siblings."""
    if not BASELINE_PATH.exists():
        return {}
    try:
        data = json.loads(BASELINE_PATH.read_text())
        return data if isinstance(data, dict) else {}
    except (json.JSONDecodeError, OSError):
        return {}


def save_baseline(scores: dict[str, dict]) -> None:
    """Write the floor.  Top-level keys other than version/scores are preserved:
    a writer that only knows about `scores` must not delete a sibling key that
    some other tool put in this file."""
    data = {k: v for k, v in _load_baseline_doc().items()
            if k not in ("version", "scores")}
    data["version"] = BASELINE_VERSION
    data["scores"] = scores
    _atomic_write_json(BASELINE_PATH, data, indent=2, sort_keys=True)


def save_current(scores: dict[str, dict]) -> None:
    """Write honest current scores (dashboard source of truth).

    Same entry schema as the floor: generate_decomp_report layers this file over
    the floor per function, so a field present in one and absent from the other
    would render differently depending on which file supplied the entry.
    """
    data = {"version": BASELINE_VERSION,
            "note": "Honest current VC71 scores, gated on reference validity. "
                    "Regenerated by `populate`; not a committed floor.",
            "scores": scores}
    _atomic_write_json(CURRENT_PATH, data, indent=2, sort_keys=True)


def load_current() -> dict[str, dict]:
    """Return {fn_name: entry} from the honest current snapshot, or {} if it has
    not been generated yet.  Counterpart to load_baseline; the dashboard reads
    back through this after a scoped populate."""
    if not CURRENT_PATH.exists():
        return {}
    try:
        return json.loads(CURRENT_PATH.read_text()).get("scores", {}) or {}
    except (json.JSONDecodeError, OSError):
        return {}


def save_validity(flagged: list[dict]) -> None:
    """Write the list of functions skipped because their reference failed
    validation (truncated/absent) — a re-delink work queue."""
    VALIDITY_PATH.parent.mkdir(parents=True, exist_ok=True)
    _atomic_write_json(VALIDITY_PATH, {"version": 1, "flagged": flagged},
                       indent=2, sort_keys=True)


def _merged_current(honest: dict[str, dict]) -> dict[str, dict]:
    """This shard's honest scores layered over whatever the file already held."""
    merged: dict = {}
    if CURRENT_PATH.exists():
        try:
            merged = json.loads(CURRENT_PATH.read_text()).get("scores", {}) or {}
        except (json.JSONDecodeError, OSError):
            merged = {}
    merged.update(honest)
    return merged


def _merged_validity(flagged: list[dict], measured_sources: set) -> list[dict]:
    """This shard's attention-queue entries, replacing only its own TUs' rows.

    Keyed by source so a shard clears the stale entries for the TUs it just
    re-measured (a fixed compile failure must disappear) without touching rows
    belonging to TUs it never looked at.
    """
    prior: list = []
    if VALIDITY_PATH.exists():
        try:
            prior = json.loads(VALIDITY_PATH.read_text()).get("flagged", []) or []
        except (json.JSONDecodeError, OSError):
            prior = []
    kept = [f for f in prior if f.get("source") not in measured_sources]
    return kept + list(flagged)


def record_rebaseline_journal(honest: dict[str, dict], flagged: list[dict],
                              sources: list[str]) -> None:
    """Merge one rebaseline shard's outcome into the cross-shard journal.

    Records the keys the scorer emitted (with their fresh entries), the
    attention-queue rows, and the TUs visited.  `rebaseline-report` reads this
    to tell a measured-and-unchanged entry from a never-measured one — a
    distinction the baseline itself cannot express.
    """
    doc = {"version": 1, "measured": {}, "flagged": [], "sources": []}
    if REBASELINE_JOURNAL_PATH.exists():
        try:
            loaded = json.loads(REBASELINE_JOURNAL_PATH.read_text())
            if isinstance(loaded, dict):
                doc.update({k: loaded.get(k, doc[k]) for k in doc if k != "version"})
        except (json.JSONDecodeError, OSError):
            pass
    doc["measured"].update(honest)
    visited = set(sources)
    doc["flagged"] = [f for f in doc["flagged"]
                      if f.get("source") not in visited] + list(flagged)
    doc["sources"] = sorted(set(doc["sources"]) | visited)
    REBASELINE_JOURNAL_PATH.parent.mkdir(parents=True, exist_ok=True)
    REBASELINE_JOURNAL_PATH.write_text(
        json.dumps(doc, indent=1, sort_keys=True) + "\n")


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
    address layout — which function each symbol is, and therefore which bounds
    entry its reference is cut from — and that changes rarely.
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
            # A memo written under an older rule set is discarded wholesale, not
            # migrated: its entries encode decisions this version would not make.
            if data.get("version") == MEASURE_MEMO_VERSION:
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
        _atomic_write_json(MEASURE_CACHE_PATH,
                           {"version": MEASURE_MEMO_VERSION, "entries": entries})
    except OSError:
        pass


def _atomic_write_json(path: Path, payload: dict, **dump_kw) -> None:
    """Write via temp-file + rename.  Several agents run gates concurrently in
    this checkout; a torn write would leave unparseable JSON that every later
    process silently discards (the loader falls back to an empty cache), turning
    a shared speedup into a permanent cold start.

    ``dump_kw`` is forwarded to ``json.dumps`` so the score files keep their
    committed ``indent=2, sort_keys=True`` formatting; changing it would show up
    as a whole-file diff on the tracked floor."""
    tmp = path.with_suffix(path.suffix + f".{os.getpid()}.tmp")
    try:
        tmp.write_text(json.dumps(payload, **dump_kw) + "\n")
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

    No longer consumed by this module's gates (the instruction-count validity
    heuristics it fed are gone — see the provenance-gate note below).  Kept
    because it is this module's answer to "how big is this function", and its
    test pins that answer to the SAME committed bounds table vc71_verify cuts
    references from: two modules disagreeing about a function's size means they
    disagree about which bytes a score describes.

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


# ---------------------------------------------------------------------------
# Reference provenance gate
# ---------------------------------------------------------------------------
#
# What replaced the instruction-count heuristics.  Until the bounds-table
# rework a reference was SELECTED (whole-object slice / sibling export /
# per-function chunk / synthesized fallback), so it could genuinely be the wrong
# bytes, and the only cheap detector was arithmetic: a reference whose
# instruction count could not fill the function's byte span was truncated, one
# with more instructions than bytes had swallowed a neighbour.  Both tests are
# dead weight now.  A reference is CUT from [start, end) of the pristine XBE
# using the committed bounds table, so it cannot be truncated or bloated
# relative to the span -- the span IS its length.  What can still happen is a
# missing bounds entry, and vc71_verify reports that as a DROP (no score line at
# all), which `check` treats as a hard failure rather than as a skip.
#
# The remaining risk is not an invalid reference but a CHANGED one: edit the
# bounds table or the synthesizer and today's score is measured against
# different bytes than the floor was.  Comparing the two numbers then answers a
# question nobody asked.  ref_sha makes that visible.

REBASELINE_HINT = (
    "The reference itself changed (bounds table or reference synthesizer), so\n"
    "the stored floor was measured against different bytes than this run.  Those\n"
    "two numbers are not comparable -- do not commit through this by lowering a\n"
    "floor.  Review the delta, then re-baseline the affected TUs:\n"
    "  python3 tools/verify/vc71_regression.py populate --rebaseline --source <file>\n"
    "  python3 tools/verify/vc71_regression.py rebaseline-report"
)


def compare_provenance(base_entry: dict, current: dict) -> tuple[str, str]:
    """Classify a fresh measurement against the reference its floor was set on.

    Returns ``(verdict, detail)``:

    - ``"ok"``                  same reference bytes; the scores are comparable.
    - ``"measurement_changed"`` baseline and run name DIFFERENT reference bytes.
    - ``"provenance_missing"``  the baseline knows its reference, the run did not
      report one (the scorer stopped emitting REFMETA).  The score is still a
      measurement, so it is compared, but the pin could not be verified.
    - ``"grandfathered"``       the baseline entry predates provenance stamping
      (47 such entries at the version-2 migration).  Compared as before; the
      next ``populate --rebaseline`` stamps it.
    """
    base_sha = base_entry.get("ref_sha")
    cur_sha = current.get("ref_sha")
    if not base_sha:
        return "grandfathered", ""
    if not cur_sha:
        return "provenance_missing", (f"baseline ref_sha={base_sha}, this run "
                                      f"emitted no REFMETA")
    if base_sha != cur_sha:
        return "measurement_changed", (
            f"ref_sha {base_sha} → {cur_sha}; "
            f"bytes {base_entry.get('addr', '?')}..{base_entry.get('end', '?')} "
            f"→ {current.get('addr', '?')}..{current.get('end', '?')}")
    return "ok", ""


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
    ref_meta: dict[str, dict] = {}
    for line in (result.stdout + result.stderr).splitlines():
        m = _LINE_RE.search(line)
        if m:
            o = _OPND_RE.search(line)
            a = _ABI_MODEL_RE.search(line)
            official = float(m.group(2))
            out[m.group(1)] = {
                "score": official,
                "n_c": int(m.group(3)),
                "n_r": int(m.group(4)),
                "opnd_percent": float(o.group(1)) if o else None,
                "raw_mnemonic_pct": float(a.group(1)) if a else official,
                "abi_modeled_mnemonic_pct": float(a.group(2)) if a else official,
                "abi_model": a.group(3) if a else "raw",
                "abi_model_items": int(a.group(4)) if a else 0,
            }
            continue
        r = _REFMETA_RE.match(line)
        if r:
            ref_meta[r.group(1)] = {
                # Kept as hex STRINGS, matching kb.json's own address form, so a
                # baseline entry can be grepped for an address the way every
                # other artifact in this repo spells it.
                "addr": f"0x{int(r.group(2), 16):x}",
                "end": f"0x{int(r.group(3), 16):x}",
                "kind": r.group(4),
                "n_r": int(r.group(5)),
                "ref_sha": r.group(6),
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

    # Attach reference provenance to each score.  Same two-step join as SYNTHREF
    # above: vc71_verify labels a score line by the compiled symbol, which is the
    # same key REFMETA uses, but a namespace-qualified reference name can differ
    # in its qualifier only.  A score with no REFMETA keeps its parsed n_r and
    # simply carries no provenance -- it must never be dropped, or a scorer that
    # stops printing the line would silently blank the baseline.
    for fn, meta in ref_meta.items():
        target = fn if fn in out else next(
            (k for k in out if k.rsplit("::", 1)[-1] == fn), None)
        if target is None:
            continue
        # The score line and REFMETA are produced from the same reference, so a
        # disagreement means the two were parsed from different runs (or the
        # output format drifted).  Trust the score line's n_r -- it is what the
        # percentage was computed from -- and keep the rest of the provenance.
        out[target].update({k: v for k, v in meta.items() if k != "n_r"})
        if out[target].get("n_r") in (None, meta["n_r"]):
            out[target]["n_r"] = meta["n_r"]

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
        # Version 1 also memoized a third class: a TU with no objdiff.json unit,
        # which scored nothing deterministically and was therefore treated as a
        # permanent fact about the repo.  References are now derived from the XBE
        # + function_bounds.json, so no TU can be in that state and the marker
        # ("No usable objdiff.json unit") is no longer printed by anything.  The
        # class is deleted rather than left dormant: an empty result is now
        # ALWAYS re-measured, so a broken wine/CL environment (which the key
        # deliberately does not cover) can never harden into a cached verdict.
        # MEASURE_MEMO_VERSION was bumped so entries written under the old rule
        # are discarded instead of answering for TUs they no longer describe.
        if out or new_drops:
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

def _result_key(results: dict, fn_name: str, addr=None):
    """The key ``results`` holds ``fn_name`` under, or None.

    vc71_verify records a function under its own name, an address alias, or a
    namespace-qualified name, so a plain ``fn_name in results`` test reports
    perfectly good scores as missing.  Same join order the report generator and
    the dashboard button use, so all three agree on what "scored" means.
    """
    if fn_name in results:
        return fn_name
    addr_int = None
    if isinstance(addr, int):
        addr_int = addr
    elif isinstance(addr, str):
        try:
            addr_int = int(addr, 16)
        except ValueError:
            addr_int = None
    if addr_int is not None:
        for k in (f"FUN_{addr_int:08x}", f"FUN_{addr_int:08X}",
                  f"thunk_FUN_{addr_int:08x}"):
            if k in results:
                return k
    for k in results:
        if k.rsplit("::", 1)[-1] == fn_name:
            return k
    return None


def _measure_source(src: Path, per_function_fallback: bool = False):
    """Measure one source TU with no shared-state mutation and no live printing.

    Returns ``(src_rel, honest_slice, flagged_slice, scored, log)``:
    - ``honest_slice``: ``{fn: {score, source, n_c, n_r}}`` for functions scored
      against a valid reference.
    - ``flagged_slice``: re-delink/attention queue entries for this TU.
    - ``scored``: ``[(fn_name, score)]`` valid scored functions, for the caller's
      serial floor merge.
    - ``log``: status lines to print (drops / invalid-reference / compile-fail).

    ``per_function_fallback`` retries each ported function the whole-TU run left
    unscored on its own.  Off by default: it costs one extra compile per missing
    function, which a whole-tree CI pass cannot afford, and the whole-TU run
    already resolves references per function in the common case.

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

    # Retry, one at a time, each ported function the whole-TU run left unscored.
    # A per-function invocation resolves its reference on its own, so it can
    # score a function the whole-TU pass dropped.  Gated because it costs one
    # extra compile per missing function.
    if per_function_fallback:
        recovered: list = []
        for fn in sorted(_expected_ported_functions(src_rel),
                         key=lambda x: x["name"]):
            if _result_key(results, fn["name"], fn.get("addr")) is not None:
                continue
            extra = run_vc71_verify(src, function=fn["name"])
            if extra:
                results.update(extra)
                recovered.append(fn["name"])
        if recovered:
            # Anything the retry scored is no longer a re-delink candidate.
            drops = [d for d in drops
                     if _result_key(results, d["function"]) is None]
            log.append(f"  ↻ per-function fallback scored {len(recovered)} "
                       f"function(s): {', '.join(recovered)}")

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

    # Every score line here was computed against a reference cut from the
    # committed bounds table, so there is nothing left to validate about the
    # reference's shape (see the provenance-gate note above): a function whose
    # bounds entry is missing produces a DROP, handled directly above, not a
    # suspicious score.  The old instruction-count gate lived here and skipped
    # scores it judged truncated/bloated.
    for fn_name, info in sorted(results.items()):
        new_score = info["score"]

        # Built by the shared entry constructor so the honest snapshot and the
        # floor cannot drift apart in shape.  n_c (candidate instruction count)
        # is measurement detail rather than reference provenance, so it stays a
        # local extra here rather than joining PROVENANCE_FIELDS.
        entry = make_score_entry(new_score, src_rel, info)
        entry["n_c"] = info.get("n_c")
        honest_slice[fn_name] = entry
        scored.append((fn_name, new_score))

    return src_rel, honest_slice, flagged_slice, scored, log


def _apply_floor(baseline: dict, src_rel: str, scored: list, force: bool,
                 info_map: dict = None, rebaseline: bool = False):
    """Merge one TU's scored functions into ``baseline`` (in place).  Serial and
    order-independent for distinct functions.  Returns ``(n_changed, log)``.

    ``info_map`` supplies the parsed measurement per function (``{fn_name:
    entry}``, as built by ``_measure_source``) — the provenance and advisory
    fields ride along on entries this call already rewrites.

    Two mutually exclusive semantics, never mixed by accident:

    - default (``rebaseline=False``): RAISE-ONLY.  The floor is a tripwire, so a
      lower measurement is reported and refused unless ``force``.  Entries the
      call does not rewrite stay byte-identical, provenance included.
    - ``rebaseline=True``: REPLACE.  Every measured entry is rewritten from the
      fresh measurement regardless of direction, because the point of a
      re-baseline is to make the file say what the scorer says today.  ``force``
      is irrelevant here and ignored.
    """
    n_changed = 0
    log: list = []

    def _entry(fn_name, score):
        return make_score_entry(score, src_rel, (info_map or {}).get(fn_name),
                                previous=baseline.get(fn_name))

    for fn_name, new_score in scored:
        old_entry = baseline.get(fn_name)
        old_score = old_entry["score"] if old_entry else None

        if rebaseline:
            # Replace unconditionally: provenance must be stamped even when the
            # score is unchanged, or a re-baseline would leave every stable
            # function without the fields it exists to record.
            baseline[fn_name] = _entry(fn_name, new_score)
            if old_score is None:
                log.append(f"  + {fn_name}: {new_score:.1f}% (new)")
            elif abs(new_score - old_score) >= 0.05:
                arrow = "↑" if new_score > old_score else "↓"
                log.append(f"  {arrow} {fn_name}: {old_score:.1f}% → {new_score:.1f}%")
            n_changed += 1
            continue

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
        baseline, src_rel, scored, force, info_map=honest_slice)
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

    # Hard failures (fatal in every mode) vs strict-only evidence gaps.  The
    # split is deliberate: a hard failure means this run cannot answer the
    # question `check` exists to answer for a specific baselined function, so
    # passing would be a false negative.  Strict adds the "prove there were no
    # gaps at all" gates on top.
    regressions = []          # (fn, base, curr, src)               FATAL
    measurement_changed = []  # (fn, base, curr, detail, src)       FATAL
    vanished = []             # (fn, src, reason)                   FATAL
    improvements = []
    provenance_missing = []   # (fn, src, detail)   compared, unverifiable pin
    grandfathered = []        # (fn, src)           pre-provenance floor
    # TU-level infrastructure failures (compile/parse/subprocess) — see the
    # carve-out note in the loop.
    infra_failed = []         # (src, status, tail, n_functions)
    skipped = 0
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

        status = meta.get("status")
        fn_set = set(fn_names)

        # TU-level infrastructure failure: the run produced no score line AND no
        # DROP, so nothing about this TU was measured.  This is the ONE case that
        # does not fail the gate, and the reason is not that it is harmless: a
        # checkout without the RXDK/wine VC71 toolchain fails EVERY compile, and
        # a gate that blocks every commit there is a gate people disable.  It is
        # reported loudly instead, and --strict (CI, where the toolchain is a
        # precondition) still fails on it.
        #
        # Note the hook does NOT abort on its own for this: `check` returns 0,
        # `update` then runs non-blocking, and the commit proceeds.  So this
        # print is the only signal — keep it unmissable.
        if not results and not drops:
            infra_failed.append((src_rel, status or "no output",
                                 meta.get("stderr_tail") or [], len(fn_names)))
            if strict:
                strict_failures.append(
                    f"{src_rel}: vc71_verify {status or 'produced no output'}")
            continue

        if strict and status not in (None, "ok"):
            strict_failures.append(f"{src_rel}: vc71_verify {status}")
            continue

        # A DROP is vc71_verify saying it compiled the function but could derive
        # no reference for it (missing bounds entry).  For a function this check
        # is responsible for, that is silence with a known cause: fail it here
        # rather than letting the "no score line" branch below report it with a
        # vaguer reason.  Drops for functions with no baseline entry (helpers,
        # statics, new ports) stay a strict-only signal — failing on those would
        # block a commit over a symbol the floor never covered.
        dropped: set[str] = set()
        for drop in sorted(drops, key=lambda d: d["function"]):
            fn = drop["function"]
            if fn in dropped:
                continue  # one verdict per function, or the accounting doubles
            if fn in fn_set:
                dropped.add(fn)
                vanished.append((fn, src_rel,
                                 f"no valid reference — {drop['reason']}"))
            elif strict:
                strict_failures.append(
                    f"{fn}: invalid or missing delinked reference: "
                    f"{drop['reason']}")

        for fn_name in fn_names:
            if fn_name in dropped:
                continue
            current = results.get(fn_name)
            if current is None:
                # Silence is a failure.  The TU compiled (it scored other
                # functions), so a baselined function with no score line was
                # deleted, renamed, or is no longer compiled into this TU — and
                # the old behaviour, a bare `continue`, reported that as a pass.
                # This is the shape of the stale-object bug: FUN_000f56b0 held
                # exactly 81.8% across 13 attempts with no definition in the
                # tree at all.
                vanished.append((
                    fn_name, src_rel,
                    f"no score line (the TU scored {len(results)} other "
                    f"function(s)) — deleted, renamed, or no longer compiled "
                    f"here; re-baseline the TU if this was intended"))
                if strict:
                    strict_failures.append(f"{fn_name}: no parsed vc71_verify result")
                continue

            base_entry = baseline[fn_name]
            try:
                baseline_score = float(base_entry["score"])
                current_score = float(current["score"])
            except (KeyError, TypeError, ValueError):
                vanished.append((fn_name, src_rel,
                                 "invalid baseline or parsed score"))
                if strict:
                    strict_failures.append(f"{fn_name}: invalid baseline or parsed score")
                continue

            # Provenance pin BEFORE the floor comparison: if the reference moved,
            # the two scores are measurements of different bytes and comparing
            # them (in either direction) is meaningless.
            verdict, detail = compare_provenance(base_entry, current)
            if verdict == "measurement_changed":
                measurement_changed.append(
                    (fn_name, baseline_score, current_score, detail, src_rel))
                continue
            if verdict == "provenance_missing":
                provenance_missing.append((fn_name, src_rel, detail))
                if strict:
                    strict_failures.append(
                        f"{fn_name}: no reference provenance in this run ({detail})")
            elif verdict == "grandfathered":
                grandfathered.append((fn_name, src_rel))

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

    # Advisory, non-fatal: the floor was compared, we just could not verify the
    # pin.  One line per function so a recurring one is greppable, never a
    # repeated warning for the same function.
    if grandfathered and not args.quiet:
        print(f"Compared without reference provenance ({len(grandfathered)}) — "
              f"pre-provenance baseline entries, stamped on the next "
              f"`populate --rebaseline`:")
        for fn, src in grandfathered:
            print(f"  ~ {fn} in {src}")
        print()

    if provenance_missing:
        print(f"NO PROVENANCE IN THIS RUN ({len(provenance_missing)}) — the "
              f"baseline pins a reference the scorer did not report:")
        for fn, src, detail in provenance_missing:
            print(f"  ~ {fn}: {detail} in {src}")
        print()

    if infra_failed:
        n_fns = sum(n for _s, _st, _t, n in infra_failed)
        print(f"NOT MEASURED — vc71 toolchain/compile failure "
              f"({len(infra_failed)} TU(s), {n_fns} function(s)):")
        for src, status, tail, n in infra_failed:
            print(f"  ! {src}: {status}; {n} function(s) NOT covered by this gate")
            for line in tail:
                print(f"      {line}")
        print("  (infrastructure, not a lift regression — these functions were "
              "not checked at all)\n")

    if regressions:
        print(f"REGRESSIONS ({len(regressions)}):")
        for fn, base, curr, src in regressions:
            drop = base - curr
            print(f"  ✗ {fn}: {base:.1f}% → {curr:.1f}% (-{drop:.1f}pp) in {src}")
        print(
            "\nHint: investigate the change, fix the regression, then re-run:\n"
            "  python3 tools/verify/vc71_regression.py update --source <file>"
        )

    if measurement_changed:
        print(f"\nMEASUREMENT CHANGED ({len(measurement_changed)}) — scores not "
              f"comparable:")
        for fn, base, curr, detail, src in measurement_changed:
            print(f"  ✗ {fn}: floor {base:.1f}% vs measured {curr:.1f}% in {src}")
            print(f"      {detail}")
        print("\n" + REBASELINE_HINT)

    if vanished:
        print(f"\nUNMEASURED BASELINED FUNCTIONS ({len(vanished)}) — the floor "
              f"covers them, this run did not measure them:")
        for fn, src, reason in vanished:
            print(f"  ✗ {fn} in {src}: {reason}")

    # Accounting.  Every baselined function in a present source file must have
    # landed in exactly one bucket; a non-zero residual means this command lost
    # track of one, which is the same failure mode as silence and is reported the
    # same way.  It used to be a note (and before that, `checked` was overwritten
    # with expected-minus-skips, which counted never-measured functions as
    # passing: 1589 "checked" on the 26-TU maintain commit, 948 measured).
    expected = sum(len(v) for v in by_source.values())
    infra_fns = sum(n for _s, _st, _t, n in infra_failed)
    accounted = (checked + skipped + infra_fns
                 + len(vanished) + len(measurement_changed))
    unaccounted = expected - accounted

    if strict and (strict_failures or checked == 0):
        if checked == 0:
            strict_failures.append("zero functions checked")
        print("STRICT CHECK FAILED:")
        for failure in strict_failures:
            print(f"  ✗ {failure}")
        return 1

    if unaccounted:
        # Negative means a function reached two verdicts, which corrupts the
        # coverage number the same way a missing one does; both are bugs in this
        # command and both fail rather than print a number nobody can trust.
        what = ("reached no verdict" if unaccounted > 0
                else "were counted twice")
        print(f"\nACCOUNTING ERROR: {abs(unaccounted)} of {expected} baselined "
              f"function(s) {what} (checked={checked}, "
              f"skipped={skipped}, compile-failed={infra_fns}, "
              f"unmeasured={len(vanished)}, "
              f"measurement-changed={len(measurement_changed)}).")

    if regressions or measurement_changed or vanished or unaccounted:
        return 1

    notes = []
    if infra_fns:
        notes.append(f"{infra_fns} not measured (compile failure)")
    if skipped:
        notes.append(f"{skipped} source file(s) missing")
    if grandfathered:
        notes.append(f"{len(grandfathered)} without reference provenance")
    note = (", " + ", ".join(notes)) if notes else ""
    print(f"OK — no regressions ({checked} of {expected} functions measured in "
          f"{len(by_source)} source file(s){note}).")
    return 0


# ---------------------------------------------------------------------------
# bounds-gate command
# ---------------------------------------------------------------------------
#
# Editing tools/verify/function_bounds.json IS editing the references: every
# score is the candidate compared against the bytes at [addr, end) of the
# pristine XBE, and that end comes from this table.  A commit that moves a bound
# therefore moves scores without touching a single .c file — and the pre-commit
# gate only fires on staged src/**.c, so it would not even run.  This command is
# the missing trigger: it maps the CHANGED bounds entries back to the baselined
# functions that sit on them, and refuses a bounds edit that arrives without a
# re-baselined floor (where the per-function ref_sha pin then does the real
# verification).

BOUNDS_REL = "tools/verify/function_bounds.json"
SCORES_REL = "tools/verify/vc71_scores.json"


def _norm_addr(value) -> str | None:
    """'0x0015c2d0' / '15c2d0' / 1425104 → '0x15c2d0'.  None when unparseable."""
    if value is None:
        return None
    try:
        if isinstance(value, int):
            return f"0x{value:x}"
        return f"0x{int(str(value), 16):x}"
    except (TypeError, ValueError):
        return None


def _parse_bounds(text: str) -> dict[str, dict]:
    """{normalized addr: entry} from one revision of the bounds table.

    Non-address keys (the `_meta` block) are dropped: a regenerated table stamps
    its own counters there, and a metadata-only change moves no reference.
    """
    try:
        data = json.loads(text or "{}")
    except (json.JSONDecodeError, TypeError):
        return {}
    if not isinstance(data, dict):
        return {}
    out: dict[str, dict] = {}
    for key, entry in data.items():
        addr = _norm_addr(key)
        if addr is None:
            continue
        out[addr] = entry
    return out


def bounds_changed_addrs(old_text: str, new_text: str) -> set[str]:
    """Addresses whose bound differs between two revisions (added/removed/edited)."""
    old = _parse_bounds(old_text)
    new = _parse_bounds(new_text)
    return {a for a in set(old) | set(new) if old.get(a) != new.get(a)}


def sources_for_bounds_addrs(addrs: set[str],
                             baseline: dict[str, dict]) -> dict[str, list[str]]:
    """{source: [baselined fn, ...]} for entries whose reference sits on ``addrs``.

    An entry with no ``addr`` (a pre-provenance floor) cannot be joined and is
    invisible here — one more reason the grandfathered entries want stamping.
    """
    out: dict[str, list[str]] = {}
    for fn, entry in baseline.items():
        addr = _norm_addr(entry.get("addr"))
        if addr is None or addr not in addrs:
            continue
        out.setdefault(entry.get("source", ""), []).append(fn)
    for fns in out.values():
        fns.sort()
    return out


def _git_lines(*args) -> list[str]:
    try:
        r = subprocess.run(["git", *args], capture_output=True, text=True,
                           cwd=REPO_ROOT)
    except OSError:
        return []
    if r.returncode != 0:
        return []
    return [l for l in r.stdout.splitlines() if l.strip()]


def _git_show(spec: str) -> str:
    """Contents of a blob (``:path`` = staged, ``HEAD:path`` = committed)."""
    try:
        r = subprocess.run(["git", "show", spec], capture_output=True, text=True,
                           cwd=REPO_ROOT)
    except OSError:
        return ""
    return r.stdout if r.returncode == 0 else ""


def staged_paths() -> set[str]:
    return set(_git_lines("diff", "--cached", "--name-only", "--diff-filter=ACMR"))


def cmd_bounds_gate(args) -> int:
    """Print the sources a staged bounds edit affects; 1 if it lacks a rebaseline."""
    staged = staged_paths()
    if BOUNDS_REL not in staged:
        return 0

    changed = bounds_changed_addrs(_git_show(f"HEAD:{BOUNDS_REL}"),
                                   _git_show(f":{BOUNDS_REL}"))
    if not changed:
        return 0
    affected = sources_for_bounds_addrs(changed, load_baseline())
    if not affected:
        # Bounds moved, but no floor is measured against them (new functions, or
        # entries the baseline does not cover).  Nothing to verify.
        return 0

    n_fns = sum(len(v) for v in affected.values())
    if SCORES_REL not in staged:
        print(
            f"vc71-bounds-gate: {BOUNDS_REL} is staged and {len(changed)} bounds "
            f"entry(ies) changed, moving the reference for {n_fns} baselined "
            f"function(s) in {len(affected)} source file(s):",
            file=sys.stderr)
        for src in sorted(affected):
            print(f"    {src} ({len(affected[src])} function(s))", file=sys.stderr)
        print("\n" + REBASELINE_HINT, file=sys.stderr)
        print(f"\nThen stage {SCORES_REL} in the same commit, or bypass with "
              f"`git commit --no-verify`.", file=sys.stderr)
        return 1

    for src in sorted(affected):
        if src:
            print(src)
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

def _discover_scoreable_tus(include_kb_only: bool) -> list[tuple]:
    """Return ``[(src_abs, src_rel, ref_abs)]`` for every TU that can be scored.

    Two sources, unioned:

    1. objdiff.json units whose source AND delinked reference both exist.  This
       was the whole set historically, back when a score REQUIRED a Ghidra
       delinked object to compare against.
    2. (``include_kb_only``) source files that kb.json says hold ported
       functions.  Every reference is now DERIVED from the pristine XBE, so a
       missing objdiff unit no longer means unscoreable -- it means invisible.
       Ten such TUs (collision_bsp.c, path_smoothing.c,
       network_client_message_handler.c, ...) held ported functions that had
       never been measured at all, because discovery, not scoring, excluded them.

    ``ref_abs`` for a kb-only TU points at the objdiff base_path that does not
    exist; it feeds only the incremental fingerprint (``_sha_file`` returns ""
    for a missing file), never the measurement.
    """
    tus: list[tuple] = []
    seen: set[str] = set()
    objdiff = REPO_ROOT / "objdiff.json"
    if objdiff.exists():
        data = json.loads(objdiff.read_text())
        for unit in data.get("units", []):
            src = unit.get("metadata", {}).get("source_path", "")
            ref = unit.get("base_path", "")
            if not (src and ref):
                continue
            ref_abs = REPO_ROOT / ref
            full = REPO_ROOT / src
            if full.exists() and ref_abs.exists() and str(full) not in seen:
                seen.add(str(full))
                tus.append((full, str(Path(src)), ref_abs))

    if include_kb_only:
        for kb_src in sorted(_kb_source_funcs()):
            for cand in (REPO_ROOT / "src" / "halo" / kb_src, REPO_ROOT / kb_src):
                if cand.is_file() and str(cand) not in seen:
                    seen.add(str(cand))
                    rel = str(cand.relative_to(REPO_ROOT))
                    tus.append((cand, rel, REPO_ROOT / "delinked" / "__absent__.obj"))
                    break
    return tus


def cmd_populate(args) -> int:
    """Refresh the floored baseline, the honest current scores, and the
    re-delink queue across every scoreable TU (see _discover_scoreable_tus).

    With ``--incremental`` only TUs whose inputs changed since the last populate
    are re-verified; unchanged TUs carry their cached honest/flagged results
    forward (from ``populate_state.json``).  A change to the verify/compare
    tooling, kb.json, or the delinked chunk directory bumps the tool epoch and
    forces a full re-verify.  Without the flag, every TU is re-verified (the
    conservative default used by CI).

    With ``--rebaseline`` the merge is REPLACE rather than raise-only, stale
    duplicate keys are pruned, and a delta report is written.  That mode is the
    deliberate, provenance-backed re-measurement of the whole file — not
    something a routine populate should ever do by accident, which is why it is
    its own flag and not a variation on ``--force``.

    ``--source`` restricts the pass to the given TUs.  It exists so a full
    re-baseline can be SHARDED across processes: this box OOM-kills a single
    25-minute whole-tree pass, and a shard that dies takes only its own slice
    with it.  Shards merge safely because each one rewrites only the functions
    it measured.  It is also how the dashboard's score button refreshes a single
    unit: routing that button through here is what keeps this the ONLY writer of
    the floor, the honest snapshot, and the attention queue.
    """
    # Pin decl.h == kb.json before fingerprinting or compiling any TU.  The
    # per-TU decl digest (in _tu_fingerprint) reads this freshly generated
    # header, so a header that was stale on disk is corrected here and the
    # affected TUs are re-verified rather than reusing a stale/empty slice.
    rebaseline = getattr(args, "rebaseline", False)
    if getattr(args, "skip_decl_regen", False):
        print("Skipping decl.h regeneration (--skip-decl-regen).", flush=True)
    else:
        print("Regenerating build/generated/decl.h from kb.json ...", flush=True)
        regen_decl_header()

    # Discovery breadth is INDEPENDENT of floor semantics.  It used to be
    # `include_kb_only=rebaseline`, which meant the only way to see a kb-only TU
    # was the flag that also REPLACES (and therefore can lower) every floor it
    # touches.  Routine populate — including CI's — was structurally blind to
    # every ported TU without an objdiff unit.
    tus = _discover_scoreable_tus(
        include_kb_only=getattr(args, "include_kb_only", True))

    only = getattr(args, "source", None)
    if only:
        wanted = set()
        for s in only:
            p = Path(s)
            p = p if p.is_absolute() else REPO_ROOT / p
            wanted.add(str(p.resolve()))
        tus = [t for t in tus if str(t[0].resolve()) in wanted]
        missing = wanted - {str(t[0].resolve()) for t in tus}
        for m in sorted(missing):
            print(f"  SKIP {m} (not a scoreable TU)", file=sys.stderr)

    if not tus:
        print("No scoreable source files found.")
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
    if rebaseline:
        mode += ", REBASELINE (replace, not raise-only)"
    print(f"Populating baseline from {len(tus)} source files ({mode})...")

    baseline = load_baseline()
    if rebaseline:
        if getattr(args, "reset_journal", False):
            for p in (REBASELINE_JOURNAL_PATH, REBASELINE_PRE_PATH):
                if p.exists():
                    p.unlink()
            print("Re-baseline journal and pre-snapshot reset.")
        if not REBASELINE_PRE_PATH.exists():
            # Snapshot the floor BEFORE the first shard rewrites it.  The delta
            # report is written after every shard has run, by which time the
            # file no longer remembers what it used to say.
            REBASELINE_PRE_PATH.parent.mkdir(parents=True, exist_ok=True)
            REBASELINE_PRE_PATH.write_text(json.dumps(
                {"version": 1, "scores": baseline}, indent=1, sort_keys=True) + "\n")
            print(f"Pre-rebaseline snapshot ({len(baseline)} entries) → "
                  f"{REBASELINE_PRE_PATH.relative_to(REPO_ROOT)}")
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
        # --workers caps concurrency.  Each worker holds a compiler subprocess
        # and its parsed output; a whole-tree pass at the default width is what
        # OOM-kills this box, and a shard that dies mid-flight leaves the
        # baseline half-written.
        n_workers = min(len(to_verify), max(1, (os.cpu_count() or 4) - 2), 8)
        if getattr(args, "workers", None):
            n_workers = max(1, min(n_workers, args.workers))
        print(f"Re-verifying {len(to_verify)} changed TU(s) "
              f"({n_workers} workers)...", flush=True)
        with ThreadPoolExecutor(max_workers=n_workers) as ex:
            per_fn = getattr(args, "per_function_fallback", False)
            futs = {ex.submit(_measure_source, item[0], per_fn): item[1]
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
            baseline, src_rel, scored, force, info_map=honest_slice,
            rebaseline=rebaseline)
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

    # Backfill advisory/model fields onto unchanged floor entries. `_apply_floor`
    # only stamps entries it rewrites; this adds metadata without touching
    # `score`, so the raise-only guarantee holds.
    n_optional = backfill_optional_fields(baseline, honest)

    save_baseline(baseline)
    if total_changed:
        print(f"\nBaseline updated: {total_changed} function(s) changed → {BASELINE_PATH.name}")
    else:
        print("\nBaseline unchanged.")
    if n_optional:
        print(f"Advisory/model fields stamped on {n_optional} floor entry(ies).")

    if rebaseline:
        # Journal what THIS pass measured, merged across shards.  Two consumers:
        # duplicate pruning needs the set of keys today's scorer actually emits
        # (a key nobody emitted is a stale alias, not a measurement), and the
        # delta report needs the per-TU compile-failure verdicts.  Neither can be
        # recovered from the baseline afterwards, because the baseline cannot
        # distinguish "measured and unchanged" from "never measured".
        record_rebaseline_journal(honest, flagged,
                                  [rel for _a, rel, _r, _f in to_verify])

    # Honest current scores drive the dashboard; the floored baseline stays the
    # CI tripwire.  The validity report is the re-delink work queue.  A --source
    # run measured only part of the tree, so it MERGES into both files: writing
    # its slice wholesale would delete every function it was not asked about,
    # which is exactly how a sharded pass would end up publishing the last
    # shard's TUs as the entire project.
    if only:
        merged_current = _merged_current(honest)
        save_current(merged_current)
        save_validity(_merged_validity(flagged, {rel for _a, rel, _r, _f in to_verify}))
    else:
        merged_current = honest
        save_current(honest)
        save_validity(flagged)
    if incremental:
        save_populate_state(epoch, new_state)
        print(f"Incremental: {n_verified} TU(s) re-verified, {n_cached} cached "
              f"→ {POPULATE_STATE_PATH.relative_to(REPO_ROOT)}")
    # Report BOTH numbers on a scoped run.  Printing only this pass's slice made
    # a one-TU refresh look like it had shrunk the snapshot to 31 functions.
    if only:
        print(f"\nHonest current scores: {len(honest)} function(s) measured this "
              f"pass, {len(merged_current)} total → {CURRENT_PATH.name}")
    else:
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
# rebaseline-report -- duplicate pruning + old-vs-new delta
# ---------------------------------------------------------------------------

_ADDR_KEY_RE = re.compile(r"(?:thunk_)?FUN_0*([0-9a-fA-F]+)$")


def _resolve_addr(key: str, entry: dict | None) -> int | None:
    """Best-effort address for a baseline key.

    Order matters.  A freshly measured entry carries the address the scorer
    ACTUALLY cut its reference from, so it wins over any name lookup; kb.json is
    next; the FUN_<addr> spelling is the last resort, because it is the only
    thing available for a key whose function no longer exists in kb.json (which
    is precisely the stale-alias case duplicate pruning has to recognise).
    """
    if entry:
        a = entry.get("addr")
        if isinstance(a, str):
            try:
                return int(a, 16)
            except ValueError:
                pass
    _addrs, name2addr = _kb_maps()
    for cand in (key, key.rsplit("::", 1)[-1]):
        if cand in name2addr:
            return name2addr[cand]
    m = _ADDR_KEY_RE.match(key)
    if m:
        return int(m.group(1), 16)
    return None


def _load_ref_migration() -> dict[int, dict]:
    """{addr: {reason, ...}} from the bounds-table migration evidence.

    Explains WHY a score moved without re-deriving anything: `changed` means the
    reference's length changed and the new one is provably closer to the truth,
    which is the expected cause of a score move in this re-baseline.
    """
    out: dict[int, dict] = {}
    if not REF_MIGRATION_PATH.exists():
        return out
    try:
        doc = json.loads(REF_MIGRATION_PATH.read_text())
    except (json.JSONDecodeError, OSError):
        return out

    def _add(rows, note, extra=()):
        for r in rows or []:
            try:
                a = int(r.get("addr", ""), 16)
            except (ValueError, TypeError):
                continue
            rec = {"ref_note": note}
            for k in extra:
                if r.get(k) is not None:
                    rec[k] = r[k]
            out.setdefault(a, rec)

    # Most specific first: a function in `changed` must not be relabelled by a
    # later, weaker bucket.
    _add(doc.get("changed"), "ref_length_changed_closer",
         ("old", "new", "truth", "kind", "old_rung"))
    _add(doc.get("same_length_differs_beyond_annotation"),
         "ref_same_length_text_differs", ("n", "old_rung"))
    _add(doc.get("same_length_scored_differs"),
         "ref_same_length_annotation_only", ("n", "old_rung"))
    return out


def cmd_rebaseline_report(args) -> int:
    """Prune stale duplicate keys and report every old-vs-new score change.

    Inputs: the pre-rebaseline snapshot (what the floor said), the cross-shard
    journal (what the scorer emitted this pass), and the rewritten baseline.
    """
    if not REBASELINE_PRE_PATH.exists():
        print(f"No pre-rebaseline snapshot at "
              f"{REBASELINE_PRE_PATH.relative_to(REPO_ROOT)}; run "
              f"`populate --rebaseline` first.", file=sys.stderr)
        return 1
    pre = json.loads(REBASELINE_PRE_PATH.read_text()).get("scores", {}) or {}
    journal = {}
    if REBASELINE_JOURNAL_PATH.exists():
        journal = json.loads(REBASELINE_JOURNAL_PATH.read_text())
    measured = journal.get("measured", {}) or {}
    flagged = journal.get("flagged", []) or []
    baseline = load_baseline()
    migration = _load_ref_migration()

    compile_failed = {f["function"] for f in flagged
                      if f.get("state") == "compile_failed"}
    no_reference = {f["function"] for f in flagged
                    if f.get("state") == "no_reference"}
    # A TU with a compile failure has NO evidence for any of its functions, so
    # every one of them keeps its old floor rather than being read as a drop.
    compile_failed_sources = sorted({f["source"] for f in flagged
                                     if f.get("state") == "compile_failed"})

    # ---- group every key (pre + post) by resolved address --------------------
    keys = set(pre) | set(baseline)
    addr_of: dict[str, int | None] = {
        k: _resolve_addr(k, baseline.get(k) or pre.get(k)) for k in keys}
    groups: dict[int, list[str]] = {}
    for k, a in addr_of.items():
        if a is not None:
            groups.setdefault(a, []).append(k)

    # ---- duplicate pruning ---------------------------------------------------
    # Keep the key today's scorer emits; drop its aliases.  When NOTHING at that
    # address was measured we cannot arbitrate, so every key is kept and flagged
    # stale_key -- dropping one would silently delete a floor on a guess.
    pruned: list[dict] = []
    # Old score inherited by a surviving key from the alias that carried it, so
    # the delta for a merged function compares the floor that actually existed
    # against the score that actually replaced it.
    inherited_old: dict[str, float] = {}
    for addr, ks in sorted(groups.items()):
        if len(ks) < 2:
            continue
        live = sorted(k for k in ks if k in measured)
        if not live:
            continue
        keep = live[0]
        for k in ks:
            if k in live or k not in baseline:
                continue
            old_entry = baseline.get(k) or pre.get(k) or {}
            pruned.append({
                "dropped_key": k,
                "merged_into": keep,
                "addr": f"0x{addr:x}",
                "old_score": old_entry.get("score"),
                "new_score": baseline.get(keep, {}).get("score"),
                "source": old_entry.get("source"),
                "reason": "alias_merged",
            })
            if (pre.get(k) or {}).get("score") is not None:
                inherited_old.setdefault(keep, pre[k]["score"])
            baseline.pop(k, None)

    # ---- per-function reasons ------------------------------------------------
    rows: list[dict] = []
    dropped_keys = {p["dropped_key"] for p in pruned}
    merged_into = {}
    for p in pruned:
        merged_into.setdefault(p["merged_into"], []).append(p["dropped_key"])
    for key in sorted(set(baseline) | set(pre)):
        if key in dropped_keys:
            continue  # reported under `pruned`, not twice
        old = (pre.get(key) or {}).get("score")
        merged_from_alias = old is None and key in inherited_old
        if merged_from_alias:
            old = inherited_old[key]
        new = (baseline.get(key) or {}).get("score")
        entry = baseline.get(key) or {}
        addr = addr_of.get(key)
        mig = migration.get(addr) if addr is not None else None
        row = {
            "function": key,
            "source": entry.get("source") or (pre.get(key) or {}).get("source"),
            "addr": f"0x{addr:x}" if addr is not None else None,
            "old": old, "new": new,
            "delta": (round(new - old, 3) if (old is not None and new is not None)
                      else None),
            "kind": entry.get("kind"),
            "ref_sha": entry.get("ref_sha"),
        }
        if mig:
            row["ref_note"] = mig.get("ref_note")
            for k in ("old", "new", "truth"):
                if mig.get(k) is not None:
                    row[f"ref_insns_{k}"] = mig[k]

        was_measured = key in measured
        alias_had_score = any(
            k in measured for k in groups.get(addr, []) if k != key
        ) if addr is not None else False

        # Order is load-bearing.  A merged alias must be recognised BEFORE the
        # "no old score" test, or a function that has been measured under this
        # repo for months reads as brand-new coverage merely because its floor
        # was filed under the other spelling of its name.
        if key in compile_failed:
            row["reason"] = "compile_failed"
            row["note"] = "TU failed to compile under VC71; old floor kept"
        elif merged_from_alias:
            row["reason"] = "alias_merged"
            row["merged_from"] = merged_into.get(key, [])
        elif not was_measured and old is not None and not alias_had_score:
            row["reason"] = "stale_key"
            row["note"] = ("no_reference" if key in no_reference
                           else "not emitted by the current scorer")
        elif was_measured and old is None:
            row["reason"] = "new_coverage"
        elif old is None or new is None:
            row["reason"] = "stale_key"
        elif abs(new - old) < 0.05:
            row["reason"] = "unchanged"
        elif mig and mig.get("ref_note") == "ref_length_changed_closer":
            row["reason"] = "ref_changed_closer"
        else:
            row["reason"] = "remeasured_moved"
        row["delta"] = (round(new - old, 3)
                        if (old is not None and new is not None) else None)
        row["old"] = old
        rows.append(row)

    # ---- aggregates ----------------------------------------------------------
    # Counted per FUNCTION, not per key: a merged pair contributes one
    # alias_merged row (the surviving key), and `pruned` is the record of which
    # spellings were dropped, not a second population.
    counts: dict[str, int] = {}
    for r in rows:
        counts[r["reason"]] = counts.get(r["reason"], 0) + 1

    moved = [r for r in rows if r["delta"] is not None and abs(r["delta"]) >= 0.05]
    ups = [r for r in moved if r["delta"] > 0]
    downs = [r for r in moved if r["delta"] < 0]
    drops_over_2 = sorted((r for r in downs if r["delta"] <= -2.0),
                          key=lambda r: r["delta"])

    def _mean(vals):
        return round(sum(vals) / len(vals), 3) if vals else 0.0

    aggregates = {
        "entries_before": len(pre),
        "entries_after": len(baseline),
        "measured_this_pass": len(measured),
        "moved": len(moved),
        "up": len(ups), "down": len(downs),
        "mean_delta_up": _mean([r["delta"] for r in ups]),
        "max_delta_up": max([r["delta"] for r in ups], default=0.0),
        "mean_delta_down": _mean([r["delta"] for r in downs]),
        "max_delta_down": min([r["delta"] for r in downs], default=0.0),
        "beats_old_floor": len(ups),
        "drops_over_2pp": len(drops_over_2),
        "without_provenance": sum(1 for e in baseline.values()
                                  if not e.get("ref_sha")),
        "compile_failed_sources": compile_failed_sources,
    }

    report = {
        "version": 1,
        "counts": counts,
        "aggregates": aggregates,
        "top_up": sorted(ups, key=lambda r: -r["delta"])[:25],
        "top_down": sorted(downs, key=lambda r: r["delta"])[:25],
        "drops_over_2pp": drops_over_2,
        "pruned": pruned,
        "functions": rows,
    }
    REBASELINE_REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REBASELINE_REPORT_PATH.write_text(
        json.dumps(report, indent=1, sort_keys=True) + "\n")

    if pruned and not getattr(args, "dry_run", False):
        save_baseline(baseline)

    # ---- stdout summary ------------------------------------------------------
    print(f"Re-baseline delta report → "
          f"{REBASELINE_REPORT_PATH.relative_to(REPO_ROOT)}\n")
    print(f"Entries: {len(pre)} → {len(baseline)} "
          f"({len(pruned)} stale duplicate key(s) pruned)")
    print(f"Measured this pass: {len(measured)} function(s)\n")
    print("Reason codes:")
    for reason, n in sorted(counts.items(), key=lambda kv: -kv[1]):
        print(f"  {reason:<22s} {n:>6d}")
    print(f"\nMoved: {len(moved)} ({len(ups)} up, {len(downs)} down)")
    print(f"  up:   mean +{aggregates['mean_delta_up']:.2f}pp  "
          f"max +{aggregates['max_delta_up']:.2f}pp")
    print(f"  down: mean {aggregates['mean_delta_down']:.2f}pp  "
          f"max {aggregates['max_delta_down']:.2f}pp")
    print(f"  beats the old floor: {len(ups)}")
    print(f"  drops >2pp (would trip the gate): {len(drops_over_2)}")
    print(f"  entries without provenance: {aggregates['without_provenance']}")
    if drops_over_2:
        print("\nDrops over 2pp:")
        for r in drops_over_2[:40]:
            print(f"  ✗ {r['function']:<40s} {r['old']:6.1f}% → {r['new']:6.1f}% "
                  f"({r['delta']:+.1f}pp) [{r['reason']}] {r['source'] or ''}")
        if len(drops_over_2) > 40:
            print(f"  ... and {len(drops_over_2) - 40} more (see report)")
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

    sub.add_parser("bounds-gate",
                   help="Pre-commit helper: print the sources a staged "
                        "function_bounds.json edit moves references for; exit 1 "
                        "when the commit carries no re-baselined floor")

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
    p_pop.add_argument("--rebaseline", action="store_true",
                       help="REPLACE stored scores with freshly measured ones "
                            "(not raise-only) and stamp reference provenance. "
                            "Journals to "
                            "artifacts/audit/rebaseline_journal.json for "
                            "`rebaseline-report`.")
    p_pop.add_argument("--no-kb-only", dest="include_kb_only",
                       action="store_false", default=True,
                       help="Restrict discovery to objdiff.json units. Default "
                            "is to ALSO discover kb.json TUs holding ported "
                            "functions, which are scoreable from an "
                            "XBE-synthesized reference but have no objdiff unit.")
    p_pop.add_argument("--source", "-s", nargs="+",
                       help="Restrict the pass to these TUs (used to shard a "
                            "full re-baseline across processes).")
    p_pop.add_argument("--per-function-fallback", action="store_true",
                       help="For each ported function the whole-TU run did not "
                            "score, retry it on its own. Recovers functions "
                            "whose reference only resolves per-function; costs "
                            "one extra compile each, so it is off by default.")
    p_pop.add_argument("--workers", type=int,
                       help="Cap concurrent vc71_verify subprocesses.")
    p_pop.add_argument("--reset-journal", action="store_true",
                       help="Start a fresh re-baseline campaign: discard the "
                            "journal and the pre-rebaseline snapshot.")
    p_pop.add_argument("--skip-decl-regen", action="store_true",
                       help="Do not regenerate decl.h (a sharded run only needs "
                            "it done once, by the first shard).")

    p_rb = sub.add_parser("rebaseline-report",
                          help="Prune stale duplicate keys and write the "
                               "old-vs-new delta report")
    p_rb.add_argument("--dry-run", action="store_true",
                      help="Write the report but do not prune the baseline")

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
        "bounds-gate": cmd_bounds_gate,
        "populate": cmd_populate,
        "rebaseline-report": cmd_rebaseline_report,
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
