#!/usr/bin/env python3
"""Hunk-scoped recovery of a parked lift.

`park.py apply` restores the whole saved `git diff HEAD`, which also carries
unrelated edits from whatever else was in the tree when the lift was parked
(formatting churn, include reorders, neighbouring functions). This tool keeps
only the hunks that belong to the target function:

  - kb.json and tools/kb_reg_baseline.json hunks (small; they carry the target's
    signature/ported flag and any callee signatures recorded with it);
  - source hunks that mention the target name (a new definition or an edit to it).

Everything else is dropped. The filtered patch is applied with reduced context
(-C1) so neighbouring lifts landed since the park do not block it.

Usage:
  park_recover.py --name FN            # plan only: print kept/dropped hunks
  park_recover.py --name FN --apply    # apply the filtered patch to the tree
  park_recover.py --all [--min-score N] [--apply]

Exit 0 = applied (or plan ok), 3 = filtered patch does not apply, 1 = no patch.
It does not build, gate or commit; the caller does that.
"""
import argparse
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ALWAYS_KEEP = ("kb.json", "tools/kb_reg_baseline.json")
MAX_SRC_HUNK_LINES = 250


def run(cmd, **kw):
    return subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, **kw)


def parked_records(status="parked"):
    out = run([sys.executable, "tools/lift/park.py", "list", "--status", status, "--json"])
    if out.returncode != 0:
        sys.exit(f"park.py list failed: {out.stderr.strip()}")
    return json.loads(out.stdout)


def split_patch(text):
    """Return [(file_header_lines, file_path, [hunk_lines...])]."""
    files = []
    cur = None
    hunk = None
    for line in text.split("\n"):
        if line.startswith("diff --git "):
            m = re.search(r" w/(\S+)$", line) or re.search(r" b/(\S+)$", line)
            cur = {"hdr": [line], "path": m.group(1) if m else "", "hunks": []}
            files.append(cur)
            hunk = None
        elif cur is None:
            continue
        elif line.startswith("@@"):
            hunk = [line]
            cur["hunks"].append(hunk)
        elif hunk is None:
            cur["hdr"].append(line)
        else:
            hunk.append(line)
    return files


def keep_hunk(path, hunk, name):
    if path in ALWAYS_KEEP:
        return True
    body = hunk[1:]
    changed = [l for l in body if l[:1] in "+-"]
    return any(re.search(r"\b" + re.escape(name) + r"\b", l) for l in changed)


def filtered_patch(text, name):
    kept, dropped = [], []
    out = []
    for f in split_patch(text):
        hunks = []
        for h in f["hunks"]:
            (kept if keep_hunk(f["path"], h, name) else dropped).append((f["path"], h[0]))
            if keep_hunk(f["path"], h, name):
                hunks.append(h)
        if hunks:
            out.extend(f["hdr"])
            for h in hunks:
                out.extend(h)
    return "\n".join(out).rstrip("\n") + "\n", kept, dropped


def recover(rec, apply):
    name = rec["name"]
    patch = Path(rec["best_patch"])
    if not patch.is_absolute():
        patch = ROOT / patch
    if not patch.exists():
        return {"name": name, "status": "no_patch"}
    text, kept, dropped = filtered_patch(patch.read_text(encoding="utf-8"), name)
    src_kept = [k for k in kept if k[0] not in ALWAYS_KEEP]
    if not src_kept:
        return {"name": name, "status": "no_source_hunk", "dropped": len(dropped)}
    # A kept source hunk that rewrites most of a file is reformat churn that
    # happens to mention the name; extracting it needs a human.
    big = max((len([l for l in h[1:] if l[:1] in "+-"])
               for f in split_patch(text) if f["path"] not in ALWAYS_KEEP for h in f["hunks"]),
              default=0)
    if big > MAX_SRC_HUNK_LINES:
        return {"name": name, "status": "needs_manual", "reason": f"source hunk of {big} changed lines"}
    with tempfile.NamedTemporaryFile("w", suffix=".patch", delete=False, encoding="utf-8") as fh:
        fh.write(text)
        tmp = fh.name
    chk = run(["git", "apply", "--check", "-C1", tmp])
    res = {"name": name, "kept": len(kept), "dropped": len(dropped),
           "files": sorted({k[0] for k in kept}), "patch": tmp}
    if chk.returncode != 0:
        res.update(status="conflict", error=chk.stderr.strip()[:300])
        return res
    if apply:
        ap = run(["git", "apply", "-C1", tmp])
        if ap.returncode != 0:
            res.update(status="apply_failed", error=ap.stderr.strip()[:300])
            return res
        res["status"] = "applied"
    else:
        res["status"] = "ok"
    return res


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--name")
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--min-score", type=float, default=0.0)
    ap.add_argument("--apply", action="store_true")
    args = ap.parse_args()
    if not args.name and not args.all:
        ap.error("give --name or --all")
    recs = parked_records()
    if args.name:
        recs = [r for r in recs if r["name"] == args.name]
        if not recs:
            print(json.dumps({"name": args.name, "status": "no_record"}))
            return 1
    else:
        recs = [r for r in recs if (r.get("best_score") or 0) >= args.min_score]
        recs.sort(key=lambda r: -(r.get("best_score") or 0))
    code = 0
    for r in recs:
        res = recover(r, args.apply)
        res["score"] = r.get("best_score")
        print(json.dumps(res))
        if res["status"] in ("conflict", "apply_failed"):
            code = 3
        if args.apply and res["status"] == "applied" and args.all:
            break  # one at a time: the caller builds/gates/commits before the next
    return code


if __name__ == "__main__":
    sys.exit(main())
