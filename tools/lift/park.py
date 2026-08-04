#!/usr/bin/env python3
"""Parked-lift ledger — preserve sub-bar lift work for a later improve pass.

A lift that builds and is behaviorally plausible but falls below the commit bar
(e.g. 75% VC71 that the current model can't push higher) must NOT be discarded.
Experience shows such work is often recoverable later with a different model or a
fresh perspective. This tool is the durable, workflow-agnostic home for that work:
the goal-lift workflow, a manual `/lift` session, and the improve-pass all read
and write the SAME ledger through this CLI.

Storage (host-local, under gitignored artifacts/ — same convention as .halorec /
snapshots / index.duckdb):
  artifacts/parked/<slug>.json          one record per function (attempt history)
  artifacts/parked/patches/<slug>-<ts>.patch   the saved `git diff HEAD` of each attempt

Record schema:
  {
    "name", "addr", "obj", "source_path",
    "best_score": float,             # highest VC71 seen across attempts
    "best_patch": "<path>",          # patch of the best-scoring attempt
    "status": "parked" | "promoted" | "capped_confirmed" | "superseded",
    "first_parked": "<iso>",
    "last_updated": "<iso>",
    "promoted_commit": "<hash>"|null,
    "context": {...}|null,           # latest --context research brief, capped at 16KB
    "attempts": [
      {"ts", "model", "effort", "score", "cap_hypothesis", "reason", "notes", "patch"}
    ]
  }

Design notes:
- One JSON file PER FUNCTION (not a shared jsonl) so parallel workflow agents that
  park different functions never race on the same file.
- All git operations run via `git -C <root>` against the CURRENT worktree's repo
  (root resolved from CWD), so the tool is safe to run from any worktree and never
  touches another checkout.
- Ledger logic is separated from git so `--self-test` can exercise it without a repo.

Commands:
  park          save the current lift's diff + record an attempt (optionally revert tree)
  list          list parked records (filter/sort/json)
  next          pick the best candidate for an improve pass
  apply         restore a parked record's best patch into the working tree
  promote       mark a record promoted (committed elsewhere)
  confirm-cap   mark a record as a confirmed structural cap (stop retrying)
  reconcile     retire parked records whose function is already ported in kb.json
  migrate       merge a worktree-local ledger into the shared ledger
  stats         summary counts
  --self-test   run built-in tests (no git needed)

Shared ledger location:
  All worktrees of this repo share one ledger by default, so knowledge gained
  while lifting in a linked worktree is visible everywhere. Resolution order:
    1. $HALO_PARKED_DIR (absolute path to the parked dir itself), if set.
    2. parent-of-`git rev-parse --git-common-dir` + artifacts/parked -- this
       lands on the MAIN checkout regardless of which worktree is current.
    3. repo_root()/artifacts/parked, if the common-dir probe fails.
  Passing an explicit --parked-dir opts out of sharing and resolves against
  the current worktree's repo_root(), as before.

Examples:
  python3 tools/lift/park.py park --name FUN_0001b8a0 --addr 0x1b8a0 \
      --obj actions.obj --source src/halo/ai/actions.c --score 75.1 \
      --model opus --effort high --cap-hypothesis "reg-arg caller ceiling" --revert-tree
  python3 tools/lift/park.py list --status parked --sort score
  python3 tools/lift/park.py next --exclude-model opus --obj actions.obj
  python3 tools/lift/park.py apply --name FUN_0001b8a0
  python3 tools/lift/park.py promote --name FUN_0001b8a0 --commit a5726bbe
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Optional

# Default set of paths a lift touches; the diff of these is what we preserve.
DEFAULT_PATHS = ["src/", "kb.json", "tools/kb_reg_baseline.json"]

# The --parked-dir default. When the caller has NOT overridden it, storage
# routes through the shared ledger_root() instead of this literal path — see
# store_base(). An explicit --parked-dir always resolves against repo_root().
DEFAULT_PARKED_DIR = "artifacts/parked"

# Cap on the JSON-encoded size of a record's stored --context. A research
# brief (disasm_notes, hazards, callees, neighbors, review) can be arbitrarily
# large; an unbounded copy on every attempt would bloat the ledger for no
# benefit -- only the latest context is ever read back.
MAX_CONTEXT_BYTES = 16 * 1024

# The commit bar. Anything below this is a candidate for parking; the improve
# pass targets the band closest to it first.
COMMIT_BAR = 90.0


# ── git helpers (thin; all target the current worktree) ─────────────────────────

def _run(cmd: list[str], cwd: Optional[Path] = None, check: bool = False) -> subprocess.CompletedProcess:
    return subprocess.run(cmd, cwd=str(cwd) if cwd else None,
                          capture_output=True, text=True, check=check)


def repo_root() -> Path:
    """Root of the git repo containing CWD (the current worktree)."""
    r = _run(["git", "rev-parse", "--show-toplevel"])
    if r.returncode != 0:
        print("error: not inside a git repository", file=sys.stderr)
        sys.exit(2)
    return Path(r.stdout.strip())


def ledger_root() -> Path:
    """Shared parked-ledger directory: the same for every worktree of this repo.

    All worktrees of a repo share one `.git` (the "common dir"); its parent is
    the main checkout root regardless of which worktree is CWD. Without this,
    a worktree-local artifacts/parked/ is invisible to every other worktree
    and to the main checkout's improve pass -- knowledge gained in one lane
    never reaches another.

    Precedence: $HALO_PARKED_DIR (absolute path to the parked dir itself) wins;
    else parent-of-common-dir + artifacts/parked; else (common-dir probe
    failed, e.g. CWD is not inside a repo) repo_root()/artifacts/parked.
    """
    env = os.environ.get("HALO_PARKED_DIR")
    if env:
        return Path(env)
    r = _run(["git", "rev-parse", "--git-common-dir"])
    if r.returncode == 0 and r.stdout.strip():
        # --git-common-dir may print a relative path (e.g. ".git") -- resolve
        # against CWD before taking the parent, or a non-toplevel CWD yields
        # the wrong directory.
        common_dir = Path(r.stdout.strip())
        if not common_dir.is_absolute():
            common_dir = Path.cwd() / common_dir
        return common_dir.resolve().parent / "artifacts" / "parked"
    return repo_root() / "artifacts" / "parked"


def store_base(root: Path, parked_dir: str) -> Path:
    """Resolve where a Store should read/write.

    The default --parked-dir routes through the shared ledger_root(); an
    explicitly-passed --parked-dir opts out of sharing and stays relative to
    the current worktree's repo_root(), matching pre-sharing behavior.
    """
    if parked_dir == DEFAULT_PARKED_DIR:
        return ledger_root()
    return root / parked_dir


def git_diff_head(root: Path, paths: list[str]) -> str:
    """Full delta of `paths` versus HEAD (captures staged + unstaged)."""
    return _run(["git", "-C", str(root), "diff", "HEAD", "--"] + paths).stdout


def git_checkout(root: Path, paths: list[str]) -> None:
    _run(["git", "-C", str(root), "checkout", "--"] + paths)


def git_apply_check(root: Path, patch: Path) -> bool:
    return _run(["git", "-C", str(root), "apply", "--check", str(patch)]).returncode == 0


def git_apply(root: Path, patch: Path) -> subprocess.CompletedProcess:
    return _run(["git", "-C", str(root), "apply", str(patch)])


# ── ledger store (pure I/O over JSON files; no git) ─────────────────────────────

def _now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds")


def slugify(name: str) -> str:
    return re.sub(r"[^A-Za-z0-9_.-]", "_", name.strip()) or "unnamed"


@dataclass
class Store:
    """Reads/writes per-function parked records under a directory."""
    base: Path
    patches: Path = field(init=False)

    def __post_init__(self) -> None:
        self.patches = self.base / "patches"

    def _record_path(self, name: str) -> Path:
        return self.base / f"{slugify(name)}.json"

    def load(self, name: str) -> Optional[dict]:
        p = self._record_path(name)
        if not p.exists():
            return None
        try:
            return json.loads(p.read_text())
        except (OSError, json.JSONDecodeError):
            return None

    def save(self, rec: dict) -> Path:
        self.base.mkdir(parents=True, exist_ok=True)
        p = self._record_path(rec["name"])
        p.write_text(json.dumps(rec, indent=2) + "\n")
        return p

    def all(self) -> list[dict]:
        if not self.base.exists():
            return []
        out = []
        for p in sorted(self.base.glob("*.json")):
            try:
                out.append(json.loads(p.read_text()))
            except (OSError, json.JSONDecodeError):
                continue
        return out


def record_attempt(rec: Optional[dict], *, name: str, addr: str, obj: str,
                   source_path: str, score: float, model: str, effort: str,
                   reason: str, cap_hypothesis: str, patch_rel: str,
                   notes: str = "") -> dict:
    """Merge a new attempt into a record (creating it if absent). Pure function."""
    now = _now()
    attempt = {
        "ts": now, "model": model, "effort": effort, "score": score,
        "cap_hypothesis": cap_hypothesis or "", "reason": reason or "",
        "notes": notes or "", "patch": patch_rel,
    }
    if rec is None:
        rec = {
            "name": name, "addr": addr, "obj": obj, "source_path": source_path,
            "best_score": score, "best_patch": patch_rel, "status": "parked",
            "first_parked": now, "last_updated": now, "promoted_commit": None,
            "attempts": [attempt],
        }
        return rec

    rec.setdefault("attempts", []).append(attempt)
    # Keep the best-scoring attempt's patch as the resume point.
    if score >= rec.get("best_score", -1):
        rec["best_score"] = score
        rec["best_patch"] = patch_rel
    # Backfill any missing identity fields without clobbering existing ones.
    for k, v in (("addr", addr), ("obj", obj), ("source_path", source_path)):
        if v and not rec.get(k):
            rec[k] = v
    # A new attempt un-confirms a previously "capped_confirmed" record only if it
    # improved on the best score (a genuinely new result); otherwise leave status.
    if rec.get("status") == "capped_confirmed" and score > rec.get("best_score", -1):
        rec["status"] = "parked"
    elif rec.get("status") != "promoted":
        rec["status"] = "parked"
    rec["last_updated"] = now
    return rec


def tried_models(rec: dict) -> set[str]:
    return {a.get("model", "") for a in rec.get("attempts", [])}


def cap_context(ctx: dict, max_bytes: int = MAX_CONTEXT_BYTES) -> dict:
    """Shrink `ctx` to fit within max_bytes when JSON-encoded. Pure function.

    Drops the largest top-level values first (by their own JSON size) until
    the remainder fits, rather than truncating a value mid-string -- every
    key that survives stays complete and valid. Marks `context_truncated`
    when anything was dropped, so a reader knows the record is incomplete.
    """
    if not isinstance(ctx, dict):
        return ctx
    if len(json.dumps(ctx).encode("utf-8")) <= max_bytes:
        return ctx
    out = dict(ctx)
    biggest_first = sorted(out.keys(), key=lambda k: -len(json.dumps(out[k])))
    for k in biggest_first:
        if len(json.dumps(out).encode("utf-8")) <= max_bytes:
            break
        del out[k]
    out["context_truncated"] = True
    return out


def _last_notes(rec: dict) -> str:
    """Most recent attempt with non-empty notes, latest first."""
    for a in reversed(rec.get("attempts", [])):
        if a.get("notes"):
            return a["notes"]
    return ""


def _attempt_history(rec: dict) -> list[dict]:
    """Compact per-attempt summary for the improve pass -- what was already
    tried and how it scored, without the full patch/context payloads."""
    return [{"ts": a.get("ts"), "model": a.get("model"), "effort": a.get("effort"),
             "score": a.get("score"), "reason": a.get("reason")}
            for a in rec.get("attempts", [])]


# ── commands ────────────────────────────────────────────────────────────────────

def cmd_park(args: argparse.Namespace) -> int:
    root = repo_root()
    store = Store(store_base(root, args.parked_dir))
    paths = args.paths or DEFAULT_PATHS

    patch_text = git_diff_head(root, paths)
    if not patch_text.strip() and not args.allow_empty:
        print("error: no diff vs HEAD for the given paths — nothing to park "
              "(use --allow-empty to record an attempt anyway)", file=sys.stderr)
        return 1

    store.patches.mkdir(parents=True, exist_ok=True)
    ts = datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")
    patch_path = store.patches / f"{slugify(args.name)}-{ts}.patch"
    patch_path.write_text(patch_text)
    # Store repo-relative when the ledger lives inside the repo (the normal
    # artifacts/parked case); fall back to the absolute path for an out-of-repo
    # ledger. cmd_apply resolves both correctly (root / abs == abs).
    try:
        patch_rel = str(patch_path.relative_to(root))
    except ValueError:
        patch_rel = str(patch_path)

    rec = store.load(args.name)
    rec = record_attempt(
        rec, name=args.name, addr=args.addr or (rec or {}).get("addr", ""),
        obj=args.obj or (rec or {}).get("obj", ""),
        source_path=args.source or (rec or {}).get("source_path", ""),
        score=args.score, model=args.model, effort=args.effort,
        reason=args.reason, cap_hypothesis=args.cap_hypothesis,
        notes=args.notes, patch_rel=patch_rel,
    )
    if args.context:
        try:
            ctx = json.loads(Path(args.context).read_text())
        except (OSError, json.JSONDecodeError) as e:
            print(f"warning: could not read --context {args.context}: {e}", file=sys.stderr)
            ctx = None
        if isinstance(ctx, dict):
            # Latest write wins -- the newest research brief is the one an
            # improve pass should see, not a merge of every attempt's context.
            rec["context"] = cap_context(ctx)
    store.save(rec)

    if args.revert_tree:
        git_checkout(root, paths)

    print(f"parked {args.name} @ {args.score}% (attempts={len(rec['attempts'])}, "
          f"best={rec['best_score']}%, status={rec['status']})")
    print(f"  patch: {patch_rel}")
    if args.revert_tree:
        print("  tree reverted to HEAD")
    return 0


def _match(rec: dict, args: argparse.Namespace) -> bool:
    if getattr(args, "status", None) and rec.get("status") != args.status:
        return False
    if getattr(args, "obj", None) and rec.get("obj") != args.obj:
        return False
    if getattr(args, "min_score", None) is not None and rec.get("best_score", 0) < args.min_score:
        return False
    return True


def _sort_key(rec: dict, how: str):
    if how == "attempts":
        return len(rec.get("attempts", []))
    if how == "age":
        return rec.get("first_parked", "")
    # default "score": closest-to-bar first (highest best_score)
    return -rec.get("best_score", 0)


def _list_view(rec: dict) -> dict:
    """Trimmed record for `list --json`. A record's --context can be up to
    16KB and every attempt can carry its own free-text notes; dumping all of
    that for every matched record would make `list` as expensive as `next`
    for no benefit -- list is for scanning, next is for reading one in full.
    Presence is still reported (has_context / has_notes) so a caller knows
    there is more to fetch via `next`.
    """
    view = {k: v for k, v in rec.items() if k != "context"}
    view["has_context"] = "context" in rec
    view["attempts"] = [
        {**{k: v for k, v in a.items() if k != "notes"}, "has_notes": bool(a.get("notes"))}
        for a in rec.get("attempts", [])
    ]
    return view


def cmd_list(args: argparse.Namespace) -> int:
    root = repo_root()
    store = Store(store_base(root, args.parked_dir))
    recs = [r for r in store.all() if _match(r, args)]
    recs.sort(key=lambda r: _sort_key(r, args.sort))

    if args.json:
        print(json.dumps([_list_view(r) for r in recs], indent=2))
        return 0
    if not recs:
        print("(no parked records match)")
        return 0
    print(f"{'name':32s} {'addr':10s} {'obj':16s} {'best':>6s} {'#':>2s} {'status':16s} cap_hypothesis")
    for r in recs:
        cap = ""
        for a in reversed(r.get("attempts", [])):
            if a.get("cap_hypothesis"):
                cap = a["cap_hypothesis"]
                break
        print(f"{r['name'][:32]:32s} {r.get('addr','')[:10]:10s} "
              f"{(r.get('obj') or '-')[:16]:16s} {r.get('best_score',0):6.1f} "
              f"{len(r.get('attempts',[])):2d} {r.get('status','')[:16]:16s} {cap[:40]}")
    return 0


def cmd_next(args: argparse.Namespace) -> int:
    root = repo_root()
    store = Store(store_base(root, args.parked_dir))
    cands = [r for r in store.all() if r.get("status") == "parked"]
    if args.obj:
        cands = [r for r in cands if r.get("obj") == args.obj]
    if args.exclude_model:
        cands = [r for r in cands if args.exclude_model not in tried_models(r)]
    if args.max_score is not None:
        cands = [r for r in cands if r.get("best_score", 0) <= args.max_score]
    # Closest to the bar first, then fewest attempts (cheapest to push over).
    cands.sort(key=lambda r: (-r.get("best_score", 0), len(r.get("attempts", []))))
    if not cands:
        print(json.dumps({"found": False}))
        return 0
    chosen = cands[0]
    out = {
        "found": True,
        "record": chosen,
        # Convenience copies so a caller doesn't need to re-derive them from
        # `record` -- the improve pass reads these directly to avoid
        # rediscovering what earlier attempts already learned.
        "last_notes": _last_notes(chosen),
        "context": chosen.get("context"),
        "attempt_history": _attempt_history(chosen),
    }
    print(json.dumps(out, indent=2))
    return 0


def cmd_apply(args: argparse.Namespace) -> int:
    root = repo_root()
    store = Store(store_base(root, args.parked_dir))
    rec = store.load(args.name)
    if not rec:
        print(f"error: no parked record for {args.name}", file=sys.stderr)
        return 1
    patch = root / rec["best_patch"]
    if not patch.exists():
        print(f"error: best_patch missing on disk: {rec['best_patch']}", file=sys.stderr)
        return 1
    if not git_apply_check(root, patch):
        print(f"error: patch does not apply cleanly to the current tree "
              f"(HEAD moved?). Patch: {rec['best_patch']}", file=sys.stderr)
        return 3
    res = git_apply(root, patch)
    if res.returncode != 0:
        print(f"error: git apply failed: {res.stderr.strip()}", file=sys.stderr)
        return 3
    print(f"applied {rec['best_patch']} ({rec['best_score']}% baseline for {args.name})")
    return 0


def cmd_promote(args: argparse.Namespace) -> int:
    root = repo_root()
    store = Store(store_base(root, args.parked_dir))
    rec = store.load(args.name)
    if not rec:
        print(f"error: no parked record for {args.name}", file=sys.stderr)
        return 1
    rec["status"] = "promoted"
    rec["promoted_commit"] = args.commit or rec.get("promoted_commit")
    rec["last_updated"] = _now()
    store.save(rec)
    print(f"promoted {args.name} (commit={rec.get('promoted_commit') or '?'})")
    return 0


def cmd_confirm_cap(args: argparse.Namespace) -> int:
    root = repo_root()
    store = Store(store_base(root, args.parked_dir))
    rec = store.load(args.name)
    if not rec:
        print(f"error: no parked record for {args.name}", file=sys.stderr)
        return 1
    rec["status"] = "capped_confirmed"
    rec.setdefault("cap_reasons", []).append({"ts": _now(), "reason": args.reason})
    rec["last_updated"] = _now()
    store.save(rec)
    print(f"confirmed cap for {args.name}: {args.reason}")
    return 0


def _norm_addr(addr: str) -> str:
    """Canonical '0x<lowerhex>' with no leading zeros, or '' when unparseable."""
    try:
        return "0x" + format(int(str(addr).strip().lower(), 16), "x")
    except (TypeError, ValueError):
        return ""


def kb_ported_index(kb_path: Path) -> tuple[dict, dict]:
    """Return (ported_by_name, ported_by_addr) from kb.json.

    Walks the whole document rather than assuming a fixed nesting depth -- the
    object/function layout differs between sections and a hardcoded path silently
    yields an empty index, which would make reconcile report "nothing stale" and
    look like a pass.
    """
    by_name: dict[str, bool] = {}
    by_addr: dict[str, bool] = {}

    def walk(node) -> None:
        if isinstance(node, dict):
            if "ported" in node and ("addr" in node or "name" in node):
                ported = node.get("ported") is True
                nm = node.get("name")
                if isinstance(nm, str) and nm:
                    by_name[nm] = ported
                ad = _norm_addr(node.get("addr") or "")
                if ad:
                    by_addr[ad] = ported
            for v in node.values():
                walk(v)
        elif isinstance(node, list):
            for v in node:
                walk(v)

    try:
        walk(json.loads(kb_path.read_text()))
    except (OSError, json.JSONDecodeError):
        return {}, {}
    return by_name, by_addr


def reconcile_targets(recs: list[dict], by_name: dict, by_addr: dict) -> list[dict]:
    """Records still marked `parked` whose function is already ported in kb.json.

    Pure so the decision is testable. Name is checked before address because a
    record's `name` field is authoritative when present; several records carry
    annotated names ("lruv_cache_dispose (FUN_0011cab0)", "... (guess; ...)")
    that cannot match by name at all, and those are caught by address.

    A missing/unknown function is NEVER reported: absence of evidence must not
    retire a real parked target.
    """
    out = []
    for rec in recs:
        if rec.get("status") != "parked":
            continue
        ported = by_name.get(rec.get("name") or "")
        if ported is None:
            ported = by_addr.get(_norm_addr(rec.get("addr") or ""))
        if ported is True:
            out.append(rec)
    return out


def cmd_reconcile(args: argparse.Namespace) -> int:
    """Retire parked records whose function has since landed.

    The ledger is append-only and never learns that a function was later ported
    by a different route, so `list`/`next` keep serving finished work as if it
    were an open target. Measured 2026-08-04: 30 of 45 `parked` records were
    already ported=true, and four of them were handed to an auto-session run as
    "score recovery" targets -- the whole batch spent its research budget
    confirming work that was already committed.

    Retired records keep their full attempt history and best patch; only
    `status` changes, so nothing is lost and `next` stops offering them (it
    filters on status == "parked").
    """
    root = repo_root()
    store = Store(store_base(root, args.parked_dir))
    recs = store.all()
    by_name, by_addr = kb_ported_index(root / "kb.json")
    if not by_name and not by_addr:
        print("error: could not read kb.json -- refusing to reconcile blind",
              file=sys.stderr)
        return 2

    stale = reconcile_targets(recs, by_name, by_addr)
    parked_total = sum(1 for r in recs if r.get("status") == "parked")
    if not stale:
        print(f"reconcile: {parked_total} parked record(s), none stale")
        return 0

    print(f"reconcile: {len(stale)} of {parked_total} parked record(s) already "
          f"ported=true in kb.json")
    for rec in sorted(stale, key=lambda r: -(r.get("best_score") or 0)):
        print(f"  {(rec.get('name') or '?')[:52]:52s} "
              f"{rec.get('addr') or '-':10s} best={rec.get('best_score')}")
    if not args.apply:
        print("\n(dry run -- pass --apply to mark these superseded)")
        return 0

    for rec in stale:
        rec["status"] = "superseded"
        rec["superseded_reason"] = ("kb.json ported=true: function landed; this "
                                   "parked patch is no longer the path forward")
        rec["last_updated"] = _now()
        store.save(rec)
    print(f"\nmarked {len(stale)} record(s) superseded")
    return 0


def merge_parked_records(dst: Optional[dict], src: dict) -> dict:
    """Merge worktree-local record `src` into shared record `dst`. Pure function
    (no git, no disk) so migrate is testable the same way reconcile_targets is.

    Rules:
    - attempts: union, deduped by ts (an attempt is never re-recorded with the
      same timestamp, so ts is a safe identity key).
    - best_score/best_patch: recomputed from the union, not just copied from
      whichever side happened to have the higher value going in.
    - status: a terminal state (promoted/superseded/capped_confirmed) on
      EITHER side wins over "parked" -- migrate must never resurrect a
      finished record back to an open target. Between two terminal states,
      dst's is kept (it is the shared ledger's record of what actually shipped).
    - first_parked: earliest of the two; last_updated: latest of the two.
    - identity fields (addr/obj/source_path/promoted_commit): keep dst's if
      set, else backfill from src.
    """
    if dst is None:
        dst = {
            "name": src.get("name"), "addr": src.get("addr", ""),
            "obj": src.get("obj", ""), "source_path": src.get("source_path", ""),
            "best_score": src.get("best_score", 0), "best_patch": src.get("best_patch", ""),
            "status": src.get("status", "parked"),
            "first_parked": src.get("first_parked", _now()),
            "last_updated": src.get("last_updated", _now()),
            "promoted_commit": src.get("promoted_commit"),
            "attempts": [],
        }

    by_ts = {a.get("ts"): a for a in dst.get("attempts", [])}
    for a in src.get("attempts", []):
        by_ts.setdefault(a.get("ts"), a)
    attempts = sorted(by_ts.values(), key=lambda a: a.get("ts") or "")
    dst["attempts"] = attempts

    if attempts:
        def _score(a: dict) -> float:
            s = a.get("score")
            return s if isinstance(s, (int, float)) else -1
        best = max(attempts, key=_score)
        dst["best_score"] = best.get("score", dst.get("best_score", 0))
        dst["best_patch"] = best.get("patch", dst.get("best_patch", ""))

    TERMINAL = {"promoted", "superseded", "capped_confirmed"}
    dst_status = dst.get("status", "parked")
    src_status = src.get("status", "parked")
    if dst_status in TERMINAL:
        pass  # dst's terminal state is authoritative; do not downgrade.
    elif src_status in TERMINAL:
        dst["status"] = src_status
        if src_status == "promoted":
            dst["promoted_commit"] = src.get("promoted_commit") or dst.get("promoted_commit")
    else:
        dst["status"] = dst_status or src_status

    for k in ("addr", "obj", "source_path", "promoted_commit"):
        if not dst.get(k) and src.get(k):
            dst[k] = src[k]

    fp = [x for x in (dst.get("first_parked"), src.get("first_parked")) if x]
    if fp:
        dst["first_parked"] = min(fp)
    lu = [x for x in (dst.get("last_updated"), src.get("last_updated")) if x]
    if lu:
        dst["last_updated"] = max(lu)

    return dst


def cmd_migrate(args: argparse.Namespace) -> int:
    """Merge a worktree-local ledger into the shared ledger.

    Before ledger_root() existed, every worktree accumulated its own
    artifacts/parked/. Knowledge captured there is real work and must not be
    silently orphaned once storage moves to the shared location -- this folds
    it in. Idempotent: re-running with nothing new to merge is a no-op (the
    ts-dedupe in merge_parked_records makes re-merging the same source safe).
    Dry-run by default, --apply to write, matching cmd_reconcile.
    """
    root = repo_root()
    src_base = (Path(args.from_dir) if args.from_dir else root / DEFAULT_PARKED_DIR).resolve()
    dst_base = ledger_root().resolve()

    if src_base == dst_base:
        print(f"migrate: source and destination are the same ledger ({dst_base}) -- nothing to migrate")
        return 0

    src_store = Store(src_base)
    dst_store = Store(dst_base)
    src_recs = src_store.all()
    if not src_recs:
        print(f"migrate: no records under {src_base}")
        return 0

    created = merged = copied = 0
    for src_rec in src_recs:
        name = src_rec.get("name")
        if not name:
            continue
        # Patches physically live under <ledger>/patches/ regardless of what
        # the stored patch_rel string says (it may be worktree-relative from
        # before sharing existed) -- relocate by filename into the shared
        # patches dir so cmd_apply finds them from any worktree afterward.
        rec_copy = json.loads(json.dumps(src_rec))
        for a in rec_copy.get("attempts", []):
            rel = a.get("patch") or ""
            if not rel:
                continue
            fname = Path(rel).name
            sp = src_store.patches / fname
            dp = dst_store.patches / fname
            if args.apply and sp.exists() and not dp.exists():
                dst_store.patches.mkdir(parents=True, exist_ok=True)
                dp.write_bytes(sp.read_bytes())
                copied += 1
            a["patch"] = str(dp)
        if rec_copy.get("best_patch"):
            rec_copy["best_patch"] = str(dst_store.patches / Path(rec_copy["best_patch"]).name)

        dst_rec = dst_store.load(name)
        was_new = dst_rec is None
        result = merge_parked_records(dst_rec, rec_copy)
        if args.apply:
            dst_store.save(result)
        if was_new:
            created += 1
        else:
            merged += 1

    print(f"migrate: {len(src_recs)} record(s) from {src_base} -> {dst_base} "
          f"({created} new, {merged} merged, {copied} patch file(s) copied)")
    if not args.apply:
        print("\n(dry run -- pass --apply to write)")
    return 0


def cmd_stats(args: argparse.Namespace) -> int:
    root = repo_root()
    store = Store(store_base(root, args.parked_dir))
    recs = store.all()
    by_status: dict[str, int] = {}
    by_obj: dict[str, int] = {}
    near_bar = 0
    for r in recs:
        by_status[r.get("status", "?")] = by_status.get(r.get("status", "?"), 0) + 1
        by_obj[r.get("obj") or "-"] = by_obj.get(r.get("obj") or "-", 0) + 1
        if r.get("status") == "parked" and r.get("best_score", 0) >= 85:
            near_bar += 1
    if args.json:
        print(json.dumps({"total": len(recs), "by_status": by_status,
                          "by_obj": by_obj, "parked_near_bar_85plus": near_bar}, indent=2))
        return 0
    print(f"parked ledger: {len(recs)} record(s)")
    for s, n in sorted(by_status.items()):
        print(f"  {s:18s} {n}")
    print(f"  parked >=85% (next-pass targets): {near_bar}")
    if by_obj:
        print("  by object:")
        for o, n in sorted(by_obj.items(), key=lambda kv: -kv[1]):
            print(f"    {o:24s} {n}")
    return 0


# ── self-test (no git) ──────────────────────────────────────────────────────────

def _self_test() -> int:
    import tempfile
    ok = True

    def check(cond: bool, msg: str) -> None:
        nonlocal ok
        print(("PASS" if cond else "FAIL") + f": {msg}")
        ok = ok and cond

    with tempfile.TemporaryDirectory() as td:
        store = Store(Path(td) / "parked")

        # First attempt creates the record.
        rec = record_attempt(None, name="FUN_0001b8a0", addr="0x1b8a0",
                              obj="actions.obj", source_path="src/a.c", score=75.1,
                              model="opus", effort="high", reason="ceiling",
                              cap_hypothesis="reg-arg caller", patch_rel="p1.patch")
        store.save(rec)
        check(rec["best_score"] == 75.1 and rec["status"] == "parked", "create sets best=75.1 parked")
        check(len(rec["attempts"]) == 1, "one attempt recorded")

        # Second, better attempt updates best + patch, keeps history.
        rec = store.load("FUN_0001b8a0")
        rec = record_attempt(rec, name="FUN_0001b8a0", addr="0x1b8a0", obj="actions.obj",
                             source_path="src/a.c", score=88.0, model="fable",
                             effort="high", reason="better", cap_hypothesis="",
                             patch_rel="p2.patch")
        store.save(rec)
        check(rec["best_score"] == 88.0 and rec["best_patch"] == "p2.patch", "improve updates best+patch")
        check(len(rec["attempts"]) == 2, "history preserved (2 attempts)")
        check(tried_models(rec) == {"opus", "fable"}, "tried_models tracks both")

        # A worse attempt keeps the better best_patch.
        rec = record_attempt(rec, name="FUN_0001b8a0", addr="0x1b8a0", obj="actions.obj",
                             source_path="src/a.c", score=70.0, model="haiku",
                             effort="low", reason="regressed", cap_hypothesis="",
                             patch_rel="p3.patch")
        store.save(rec)
        check(rec["best_score"] == 88.0 and rec["best_patch"] == "p2.patch", "worse attempt keeps best")

        # A second, distinct function.
        r2 = record_attempt(None, name="FUN_0001beb0", addr="0x1beb0", obj="actions.obj",
                            source_path="src/a.c", score=68.3, model="opus", effort="high",
                            reason="", cap_hypothesis="loop-unroll", patch_rel="q1.patch")
        store.save(r2)

        allrecs = store.all()
        check(len(allrecs) == 2, "store.all() returns both functions")

        # next: closest-to-bar first (88 > 68.3) → should pick b8a0.
        cands = [r for r in allrecs if r.get("status") == "parked"]
        cands.sort(key=lambda r: (-r.get("best_score", 0), len(r.get("attempts", []))))
        check(cands[0]["name"] == "FUN_0001b8a0", "next picks closest-to-bar (b8a0 @88)")

        # exclude-model opus → both have opus, so none.
        excl = [r for r in cands if "opus" not in tried_models(r)]
        check(len(excl) == 0, "exclude-model opus removes both")
        # exclude-model fable → only b8a0 tried fable, so beb0 remains.
        excl_f = [r for r in cands if "fable" not in tried_models(r)]
        check(len(excl_f) == 1 and excl_f[0]["name"] == "FUN_0001beb0", "exclude-model fable leaves beb0")

        # promote transition.
        r2["status"] = "promoted"
        store.save(r2)
        check(store.load("FUN_0001beb0")["status"] == "promoted", "promote persists")

    # ── reconcile ──────────────────────────────────────────────────────────
    # Both directions matter. Too lax and the ledger keeps serving finished
    # work as an open target (the bug this exists to fix); too eager and it
    # silently retires a real parked lift, losing the only pointer to that
    # patch. So the unported and unknown cases are pinned as hard as the
    # stale one.
    recs = [
        {"name": "landed_by_name", "addr": "0xaaa", "status": "parked"},
        {"name": "still_open", "addr": "0xbbb", "status": "parked"},
        {"name": "annotated (guess; FUN_00000ccc)", "addr": "0x000ccc",
         "status": "parked"},
        {"name": "not_in_kb", "addr": "0xddd", "status": "parked"},
        {"name": "already_promoted", "addr": "0xaaa", "status": "promoted"},
    ]
    by_name = {"landed_by_name": True, "still_open": False}
    by_addr = {"0xaaa": True, "0xbbb": False, "0xccc": True}
    got = {r["name"] for r in reconcile_targets(recs, by_name, by_addr)}

    check("landed_by_name" in got, "reconcile: ported=true by name is stale")
    check("annotated (guess; FUN_00000ccc)" in got,
          "reconcile: annotated name still matches on addr (leading zeros)")
    check("still_open" not in got, "reconcile: ported=false is NOT retired")
    check("not_in_kb" not in got,
          "reconcile: function absent from kb.json is NOT retired")
    check("already_promoted" not in got,
          "reconcile: only status=parked records are considered")
    check(len(got) == 2, "reconcile: exactly the two stale records")

    # A name present in kb.json must win over a stale/reused address, or a
    # record could be retired on someone else's addr collision.
    shadow = [{"name": "open_at_reused_addr", "addr": "0xaaa", "status": "parked"}]
    check(reconcile_targets(shadow, {"open_at_reused_addr": False},
                            {"0xaaa": True}) == [],
          "reconcile: name lookup takes precedence over address")

    check(_norm_addr("0x0001B8A0") == "0x1b8a0", "reconcile: addr normalized")
    check(_norm_addr("") == "" and _norm_addr("zz") == "",
          "reconcile: unparseable addr yields no match key")

    # An unreadable kb.json must not look like "nothing is stale".
    with tempfile.TemporaryDirectory() as td:
        check(kb_ported_index(Path(td) / "missing.json") == ({}, {}),
              "reconcile: missing kb.json returns empty index (caller refuses)")

    # ── migrate (merge_parked_records) ────────────────────────────────────
    src = {
        "name": "FUN_1", "addr": "0x1", "obj": "o.obj", "source_path": "src/o.c",
        "best_score": 70.0, "best_patch": "p1.patch", "status": "parked",
        "first_parked": "2026-01-01T00:00:00+00:00",
        "last_updated": "2026-01-01T00:00:00+00:00",
        "promoted_commit": None,
        "attempts": [{"ts": "2026-01-01T00:00:00+00:00", "model": "opus",
                      "effort": "high", "score": 70.0, "cap_hypothesis": "",
                      "reason": "", "patch": "p1.patch"}],
    }
    merged = merge_parked_records(None, src)
    check(merged["best_score"] == 70.0 and len(merged["attempts"]) == 1,
          "migrate: create from src when dst absent")

    dst2 = {
        "name": "FUN_1", "addr": "0x1", "obj": "o.obj", "source_path": "src/o.c",
        "best_score": 82.0, "best_patch": "p2.patch", "status": "parked",
        "first_parked": "2026-01-02T00:00:00+00:00",
        "last_updated": "2026-01-02T00:00:00+00:00",
        "promoted_commit": None,
        "attempts": [{"ts": "2026-01-02T00:00:00+00:00", "model": "fable",
                      "effort": "high", "score": 82.0, "cap_hypothesis": "",
                      "reason": "", "patch": "p2.patch"}],
    }
    merged2 = merge_parked_records(dict(dst2), src)
    check(len(merged2["attempts"]) == 2, "migrate: union of attempts (2)")
    check(merged2["best_score"] == 82.0 and merged2["best_patch"] == "p2.patch",
          "migrate: best recomputed from union")
    check(merged2["first_parked"] == "2026-01-01T00:00:00+00:00",
          "migrate: first_parked takes earliest")
    check(merged2["last_updated"] == "2026-01-02T00:00:00+00:00",
          "migrate: last_updated takes latest")

    # Re-merging the SAME src into the already-merged result must not
    # duplicate the attempt (idempotence -- migrate is safe to re-run).
    merged3 = merge_parked_records(merged2, src)
    check(len(merged3["attempts"]) == 2, "migrate: idempotent (dedupes by ts)")

    # A terminal dst status must not be downgraded back to "parked".
    dst_promoted = dict(dst2, status="promoted", promoted_commit="abc123")
    merged4 = merge_parked_records(dict(dst_promoted), src)
    check(merged4["status"] == "promoted" and merged4["promoted_commit"] == "abc123",
          "migrate: terminal dst status is kept")

    # A terminal src status promotes a still-"parked" dst.
    src_promoted = dict(src, status="promoted", promoted_commit="def456")
    merged5 = merge_parked_records(dict(dst2), src_promoted)
    check(merged5["status"] == "promoted" and merged5["promoted_commit"] == "def456",
          "migrate: terminal src status wins over parked dst")

    # A real-world record can carry an attempt with score=None (e.g. an
    # --allow-empty park). best-of-union must not crash comparing None to float.
    src_none_score = dict(src, attempts=[dict(src["attempts"][0], score=None,
                                               ts="2026-01-03T00:00:00+00:00")])
    merged6 = merge_parked_records(dict(dst2), src_none_score)
    check(merged6["best_score"] == 82.0, "migrate: None-score attempt does not crash best-of-union")

    # ── notes + context (park knowledge capture) ────────────────────────────
    rec_n = record_attempt(None, name="FUN_2", addr="0x2", obj="o.obj",
                           source_path="src/o.c", score=80.0, model="opus",
                           effort="high", reason="try", cap_hypothesis="",
                           notes="check the callee's buffer size before recursing",
                           patch_rel="pn.patch")
    check(rec_n["attempts"][0]["notes"] == "check the callee's buffer size before recursing",
          "notes: stored on the attempt")
    check(_last_notes(rec_n) == "check the callee's buffer size before recursing",
          "last_notes: returns the most recent non-empty notes")

    rec_n2 = record_attempt(rec_n, name="FUN_2", addr="0x2", obj="o.obj",
                            source_path="src/o.c", score=85.0, model="fable",
                            effort="high", reason="better", cap_hypothesis="",
                            notes="", patch_rel="pn2.patch")
    check(_last_notes(rec_n2) == "check the callee's buffer size before recursing",
          "last_notes: skips an empty-notes attempt, returns the prior one")

    hist = _attempt_history(rec_n2)
    check(len(hist) == 2 and hist[0]["model"] == "opus" and hist[1]["score"] == 85.0,
          "attempt_history: compact ts/model/effort/score/reason per attempt")

    small_ctx = {"hazards": "none"}
    check(cap_context(small_ctx) == small_ctx,
          "cap_context: small context passes through unchanged")

    big_ctx = {"disasm_notes": "x" * 20000, "hazards": "ok"}
    capped = cap_context(big_ctx, max_bytes=1024)
    check(len(json.dumps(capped).encode("utf-8")) <= 1024 + 64,
          "cap_context: shrinks a too-large context to roughly fit the cap")
    check(capped.get("context_truncated") is True, "cap_context: marks truncated context")
    check("hazards" in capped and "disasm_notes" not in capped,
          "cap_context: drops the biggest key first, keeps the small one")

    # list --json must not leak full context/notes text, only presence.
    rec_ctx = dict(rec_n2, context={"hazards": "some notes here"})
    view = _list_view(rec_ctx)
    check("context" not in view and view["has_context"] is True,
          "list view: context replaced with a presence flag")
    check(all("notes" not in a for a in view["attempts"]),
          "list view: attempt notes text is not included")
    check(view["attempts"][0]["has_notes"] is True and view["attempts"][1]["has_notes"] is False,
          "list view: has_notes reflects which attempts actually had notes")

    return 0 if ok else 1


# ── arg parsing ───────────────────────────────────────────────────────────────

def build_parser() -> argparse.ArgumentParser:
    ap = argparse.ArgumentParser(description="Parked-lift ledger for sub-bar lift work.")
    ap.add_argument("--parked-dir", default=DEFAULT_PARKED_DIR,
                    help="Ledger directory (default: the shared ledger_root(); "
                         "an explicit override resolves relative to repo root)")
    ap.add_argument("--self-test", action="store_true", help="Run built-in tests and exit")
    sub = ap.add_subparsers(dest="cmd")

    p = sub.add_parser("park", help="Save the current lift diff + record an attempt")
    p.add_argument("--name", required=True)
    p.add_argument("--addr", default="")
    p.add_argument("--obj", default="")
    p.add_argument("--source", default="")
    p.add_argument("--score", type=float, required=True)
    p.add_argument("--model", default="")
    p.add_argument("--effort", default="")
    p.add_argument("--reason", default="")
    p.add_argument("--cap-hypothesis", dest="cap_hypothesis", default="")
    p.add_argument("--notes", default="",
                   help="Freeform rationale / next-step hint for this attempt "
                        "(surfaced back via `next` as last_notes)")
    p.add_argument("--context", default="",
                   help="Path to a JSON file with research-brief fields "
                        "(disasm_notes, hazards, callees, neighbors, review, ...) "
                        "stored on the record, capped at 16KB (latest write wins)")
    p.add_argument("--paths", nargs="*", default=None,
                   help=f"Pathspecs to diff (default: {' '.join(DEFAULT_PATHS)})")
    p.add_argument("--revert-tree", action="store_true",
                   help="git checkout the paths after saving the patch (clean the tree)")
    p.add_argument("--allow-empty", action="store_true",
                   help="Record an attempt even if the diff is empty")
    p.set_defaults(func=cmd_park)

    p = sub.add_parser("list", help="List parked records")
    p.add_argument("--status", choices=["parked", "promoted", "capped_confirmed",
                                        "superseded"])
    p.add_argument("--obj")
    p.add_argument("--min-score", type=float)
    p.add_argument("--sort", choices=["score", "attempts", "age"], default="score")
    p.add_argument("--json", action="store_true")
    p.set_defaults(func=cmd_list)

    p = sub.add_parser("next", help="Pick the best candidate for an improve pass")
    p.add_argument("--obj")
    p.add_argument("--exclude-model", help="Skip records already attempted with this model")
    p.add_argument("--max-score", type=float, help="Only consider records at/below this score")
    p.set_defaults(func=cmd_next)

    p = sub.add_parser("apply", help="Restore a record's best patch into the tree")
    p.add_argument("--name", required=True)
    p.set_defaults(func=cmd_apply)

    p = sub.add_parser("promote", help="Mark a record promoted (committed)")
    p.add_argument("--name", required=True)
    p.add_argument("--commit", default="")
    p.set_defaults(func=cmd_promote)

    p = sub.add_parser("confirm-cap", help="Mark a record a confirmed structural cap")
    p.add_argument("--name", required=True)
    p.add_argument("--reason", required=True)
    p.set_defaults(func=cmd_confirm_cap)

    p = sub.add_parser("reconcile",
                       help="Retire parked records whose function is now ported")
    p.add_argument("--apply", action="store_true",
                   help="Write the changes (default: dry run)")
    p.set_defaults(func=cmd_reconcile)

    p = sub.add_parser("migrate",
                       help="Merge a worktree-local ledger into the shared ledger")
    p.add_argument("--from", dest="from_dir", default=None,
                   help="Source ledger dir (default: repo_root()/artifacts/parked)")
    p.add_argument("--apply", action="store_true",
                   help="Write the merge (default: dry run)")
    p.set_defaults(func=cmd_migrate)

    p = sub.add_parser("stats", help="Summary counts")
    p.add_argument("--json", action="store_true")
    p.set_defaults(func=cmd_stats)
    return ap


def main(argv: Optional[list[str]] = None) -> int:
    ap = build_parser()
    args = ap.parse_args(argv)
    if args.self_test:
        return _self_test()
    if not getattr(args, "func", None):
        ap.print_help()
        return 1
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
