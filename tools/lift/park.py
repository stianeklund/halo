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
    "status": "parked" | "promoted" | "capped_confirmed",
    "first_parked": "<iso>",
    "last_updated": "<iso>",
    "promoted_commit": "<hash>"|null,
    "attempts": [
      {"ts", "model", "effort", "score", "cap_hypothesis", "reason", "patch"}
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
  stats         summary counts
  --self-test   run built-in tests (no git needed)

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
                   reason: str, cap_hypothesis: str, patch_rel: str) -> dict:
    """Merge a new attempt into a record (creating it if absent). Pure function."""
    now = _now()
    attempt = {
        "ts": now, "model": model, "effort": effort, "score": score,
        "cap_hypothesis": cap_hypothesis or "", "reason": reason or "",
        "patch": patch_rel,
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


# ── commands ────────────────────────────────────────────────────────────────────

def cmd_park(args: argparse.Namespace) -> int:
    root = repo_root()
    store = Store(root / args.parked_dir)
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
        reason=args.reason, cap_hypothesis=args.cap_hypothesis, patch_rel=patch_rel,
    )
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


def cmd_list(args: argparse.Namespace) -> int:
    root = repo_root()
    store = Store(root / args.parked_dir)
    recs = [r for r in store.all() if _match(r, args)]
    recs.sort(key=lambda r: _sort_key(r, args.sort))

    if args.json:
        print(json.dumps(recs, indent=2))
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
    store = Store(root / args.parked_dir)
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
    print(json.dumps({"found": True, "record": chosen}, indent=2))
    return 0


def cmd_apply(args: argparse.Namespace) -> int:
    root = repo_root()
    store = Store(root / args.parked_dir)
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
    store = Store(root / args.parked_dir)
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
    store = Store(root / args.parked_dir)
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


def cmd_stats(args: argparse.Namespace) -> int:
    root = repo_root()
    store = Store(root / args.parked_dir)
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

    return 0 if ok else 1


# ── arg parsing ───────────────────────────────────────────────────────────────

def build_parser() -> argparse.ArgumentParser:
    ap = argparse.ArgumentParser(description="Parked-lift ledger for sub-bar lift work.")
    ap.add_argument("--parked-dir", default="artifacts/parked",
                    help="Ledger directory relative to repo root (default: artifacts/parked)")
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
    p.add_argument("--paths", nargs="*", default=None,
                   help=f"Pathspecs to diff (default: {' '.join(DEFAULT_PATHS)})")
    p.add_argument("--revert-tree", action="store_true",
                   help="git checkout the paths after saving the patch (clean the tree)")
    p.add_argument("--allow-empty", action="store_true",
                   help="Record an attempt even if the diff is empty")
    p.set_defaults(func=cmd_park)

    p = sub.add_parser("list", help="List parked records")
    p.add_argument("--status", choices=["parked", "promoted", "capped_confirmed"])
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
