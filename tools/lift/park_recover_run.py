#!/usr/bin/env python3
"""Unattended recovery of parked lifts that were blocked only by a commit hook.

For each candidate: hunk-scoped apply (park_recover.py), build, commit, and
mark the parked record promoted. Any failure reverts exactly what was applied
and moves on; the tree is never left dirty by a failed candidate.

Candidates are parked records at or above --min-score whose LATEST row in
artifacts/auto_lift/goal_progress.md is a commit-hook block
(`commit_failed` / `Commit blocked`). Lifts a reviewer REJECTed or marked
NEEDS_RUNTIME are never taken: their park reason is a lift-quality verdict.

The deactivation gate fails on main HEAD (see memory
reference_deactivation_gate_fails_on_main_head), so this sets
HALO_ALLOW_DEACTIVATIONS=1 only after proving the staged kb.json diff removes no
`"ported": true`. It never uses --no-verify.

Usage: park_recover_run.py [--min-score 90] [--limit N] [--dry-run]
Writes one JSON row per candidate to artifacts/campaigns/park_recover.jsonl.
"""
import argparse
import json
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PROGRESS = ROOT / "artifacts" / "auto_lift" / "goal_progress.md"
LEDGER = ROOT / "artifacts" / "campaigns" / "park_recover.jsonl"
HOOK_MARKERS = ("commit_failed", "Commit blocked", "pre-commit hook")
QUALITY_MARKERS = ("REJECT:", "NEEDS_RUNTIME:")


def run(cmd, env=None, timeout=900):
    return subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, env=env, timeout=timeout)


def hook_blocked_names():
    latest = {}
    for line in PROGRESS.read_text(encoding="utf-8").splitlines():
        if not line.startswith("| ") or line.startswith("| ---"):
            continue
        c = [x.strip() for x in line.split("|")]
        if len(c) < 7:
            continue
        latest[c[1]] = (c[5].lower(), c[6])
    out = set()
    for name, (status, reason) in latest.items():
        if status != "parked":
            continue
        if any(m in reason[:200] for m in QUALITY_MARKERS):
            continue
        if any(m in reason for m in HOOK_MARKERS):
            out.add(name)
    return out


def staged_removes_ported():
    d = run(["git", "diff", "--cached", "-U0", "--", "kb.json"]).stdout
    return any(re.match(r"^-.*\"ported\"\s*:\s*true", l) for l in d.splitlines())


def candidate_result(name, score, apply_json, apply):
    row = {"name": name, "score": score, "status": apply_json["status"]}
    if apply_json["status"] != "ok":
        row["detail"] = apply_json.get("error") or apply_json.get("reason", "")
        return row
    if not apply:
        return row
    patch = apply_json["patch"]
    files = apply_json["files"]
    # Never layer onto uncommitted work in the files we are about to touch.
    if run(["git", "diff", "--quiet", "--"] + files).returncode != 0 or \
       run(["git", "diff", "--cached", "--quiet", "--"] + files).returncode != 0:
        row["status"] = "skipped_dirty_target"
        return row
    ap = run(["git", "apply", "-C1", patch])
    if ap.returncode != 0:
        row.update(status="apply_failed", detail=ap.stderr.strip()[:300])
        return row

    def revert(why, detail=""):
        run(["git", "reset", "-q", "--"] + files)
        run(["git", "apply", "-R", "-C1", patch])
        row.update(status=why, detail=detail[:400])
        return row

    b = run([sys.executable, "tools/build/build.py", "-q", "--target", "halo"], timeout=1800)
    if b.returncode != 0:
        tail = "\n".join((b.stdout + b.stderr).splitlines()[-6:])
        return revert("build_failed", tail)
    run(["git", "add", "--"] + files)
    if staged_removes_ported():
        return revert("would_deactivate")
    msg = f"Port {name}\n\n- {name}: {score}% VC71 (recovered from parked ledger)\n"
    with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False) as fh:
        fh.write(msg)
        mf = fh.name
    env = dict(os.environ, HALO_ALLOW_DEACTIVATIONS="1")
    c = run(["git", "commit", "-F", mf], env=env, timeout=1800)
    if c.returncode != 0 and "kb_reg_baseline drift" in (c.stdout + c.stderr):
        # Known infra park: a missing baseline entry. extract_reg_args --apply is
        # behavior-neutral for MISSING entries but ignores --target (it fills every
        # missing one), so accept it only when it purely adds a few lines.
        run([sys.executable, "tools/audit/extract_reg_args.py", "--apply"])
        diff = run(["git", "diff", "-U0", "--", "tools/kb_reg_baseline.json"]).stdout
        added = [l for l in diff.splitlines() if l.startswith("+") and not l.startswith("+++")]
        removed = [l for l in diff.splitlines() if l.startswith("-") and not l.startswith("---")]
        if added and len(added) <= 6 and not removed:
            run(["git", "add", "--", "tools/kb_reg_baseline.json"])
            files = files + ["tools/kb_reg_baseline.json"]
            c = run(["git", "commit", "-F", mf], env=env, timeout=1800)
        else:
            fix =tempfile.NamedTemporaryFile("w", suffix=".patch", delete=False)
            fix.write(diff)
            fix.close()
            run(["git", "apply", "-R", "--unidiff-zero", fix.name])
    if c.returncode != 0:
        lines = (c.stdout + c.stderr).splitlines()
        sig = [l for l in lines if re.search(r"FAIL|ERROR|blocked|drift|reject|regress|invalid", l, re.I)
               and "0 failed" not in l and "0 ERROR" not in l]
        return revert("commit_gate_failed", "\n".join((sig[:6] or lines[-6:])))
    sha = run(["git", "rev-parse", "--short", "HEAD"]).stdout.strip()
    run([sys.executable, "tools/lift/park.py", "promote", "--name", name, "--commit", sha])
    row.update(status="committed", commit=sha)
    return row


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--min-score", type=float, default=90.0)
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    blocked = hook_blocked_names()
    recs = json.loads(run([sys.executable, "tools/lift/park.py", "list", "--status", "parked", "--json"]).stdout)
    recs = [r for r in recs if r["name"] in blocked and (r.get("best_score") or 0) >= args.min_score]
    recs.sort(key=lambda r: -(r.get("best_score") or 0))
    if args.limit:
        recs = recs[:args.limit]

    LEDGER.parent.mkdir(parents=True, exist_ok=True)
    counts = {}
    for r in recs:
        plan = run([sys.executable, "tools/lift/park_recover.py", "--name", r["name"]])
        try:
            pj = json.loads(plan.stdout.strip().splitlines()[-1])
        except (ValueError, IndexError):
            pj = {"status": "plan_failed", "error": plan.stderr[:200]}
        row = candidate_result(r["name"], r.get("best_score"), pj, not args.dry_run)
        counts[row["status"]] = counts.get(row["status"], 0) + 1
        print(json.dumps(row), flush=True)
        if not args.dry_run:
            with LEDGER.open("a", encoding="utf-8") as fh:
                fh.write(json.dumps(row) + "\n")
    print(json.dumps({"summary": counts}))
    return 0


if __name__ == "__main__":
    sys.exit(main())
