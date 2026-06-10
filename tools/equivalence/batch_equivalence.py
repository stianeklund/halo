#!/usr/bin/env python3
"""Retroactive equivalence campaign driver (Task 12).

Builds a priority queue of ported functions most in need of behavioral
verification, then runs unicorn_diff.py on each, appending one-line verdicts
to artifacts/equivalence/campaign_ledger.md.

Priority order (highest first):
  1. VC71 < 65%     — strong mismatch; equivalence tells us if the lift is wrong
  2. VC71 65–85%    — borderline; instruction-faithful ≠ behavior-faithful
  3. FPU-heavy      — float precision issues are invisible to instruction diff
  4. VC71 85–98%    — permuter band; behavioral check confirms correctness
  (Functions at VC71 >= 99% are skipped; byte-match is sufficient evidence)

Usage:
  rtk python3 tools/equivalence/batch_equivalence.py --limit 50
  rtk python3 tools/equivalence/batch_equivalence.py --resume  # skip already-ledgered
  rtk python3 tools/equivalence/batch_equivalence.py --dry-run # print queue, don't run
  rtk python3 tools/equivalence/batch_equivalence.py --seeds 50 --float-tolerance 16
"""

import json
import re
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
KB_JSON        = ROOT / "kb.json"
VC71_SCORES    = ROOT / "tools" / "verify" / "vc71_scores.json"
LEAF_CACHE     = ROOT / "tools" / "equivalence" / "leaf_cache.json"
LEDGER_DIR     = ROOT / "artifacts" / "equivalence"
LEDGER_PATH    = LEDGER_DIR / "campaign_ledger.md"


def _load_kb_ported():
    """Return list of {addr, name, obj, decl} for all ported functions."""
    kb = json.loads(KB_JSON.read_text(encoding="utf-8"))
    out = []

    def _walk(node):
        if isinstance(node, dict):
            if node.get("ported") and node.get("addr") and node.get("decl"):
                decl = node["decl"]
                m = re.search(r'\b(\w+)\s*\(', decl)
                name = m.group(1) if m else node["addr"]
                out.append({"addr": node["addr"], "name": name, "decl": decl, "obj": ""})
            for v in node.values():
                _walk(v)
        elif isinstance(node, list):
            for item in node:
                _walk(item)

    _walk(kb)
    return out


def _load_scores():
    if not VC71_SCORES.exists():
        return {}
    raw = json.loads(VC71_SCORES.read_text())
    # vc71_scores.json top-level has {"scores": {...}, "version": N}
    if isinstance(raw, dict) and "scores" in raw:
        raw = raw["scores"]
    out = {}
    for k, v in raw.items():
        if isinstance(v, dict):
            out[k] = v.get("score", 100.0)
        else:
            out[k] = float(v)
    return out


def _is_fpu_heavy(decl: str) -> bool:
    return bool(re.search(r'\bfloat\b|\bdouble\b', decl))


def _already_ledgered(name: str) -> bool:
    if not LEDGER_PATH.exists():
        return False
    txt = LEDGER_PATH.read_text(encoding="utf-8")
    return bool(re.search(r'\b' + re.escape(name) + r'\b', txt))


def _priority(name: str, scores: dict, fpu: bool) -> int:
    """Lower number = higher priority."""
    s = scores.get(name, 100.0)
    if s < 65:
        return 0
    if s < 85:
        return 1
    if fpu and s < 99:
        return 2
    if s < 99:
        return 3
    return 99  # skip


def build_queue(limit: int, resume: bool) -> list[dict]:
    fns = _load_kb_ported()
    scores = _load_scores()

    queue = []
    for fn in fns:
        name = fn["name"]
        fpu = _is_fpu_heavy(fn["decl"])
        pri = _priority(name, scores, fpu)
        if pri >= 99:
            continue
        if resume and _already_ledgered(name):
            continue
        fn["priority"] = pri
        fn["vc71"] = scores.get(name, None)
        queue.append(fn)

    queue.sort(key=lambda x: (x["priority"], -(x["vc71"] or 0)))
    return queue[:limit]


def run_unicorn(target_name: str, seeds: int, float_tol: int) -> str:
    """Run unicorn_diff.py and return a short verdict string."""
    cmd = [
        sys.executable,
        str(ROOT / "tools" / "equivalence" / "unicorn_diff.py"),
        target_name,
        "--seeds", str(seeds),
        "--allow-stubs",
        "--mem-trace",
        "--float-tolerance", str(float_tol),
    ]
    try:
        result = subprocess.run(
            cmd,
            capture_output=True, text=True, timeout=120, cwd=ROOT,
        )
        output = result.stdout + result.stderr
        # Look for the summary line
        for line in reversed(output.splitlines()):
            if any(kw in line.lower() for kw in ("pass", "fail", "error", "diverge",
                                                   "coverage", "equivalen")):
                return line.strip()[:120]
        return ("PASS" if result.returncode == 0 else "FAIL") + f" (rc={result.returncode})"
    except subprocess.TimeoutExpired:
        return "TIMEOUT (>120s)"
    except Exception as exc:
        return f"ERROR: {exc}"


def append_ledger(name: str, addr: str, vc71, priority: int, verdict: str):
    LEDGER_DIR.mkdir(parents=True, exist_ok=True)
    ts = datetime.now(timezone.utc).strftime("%Y-%m-%d")
    pri_label = ["<65%", "65-85%", "FPU", "85-98%"][min(priority, 3)]
    vc71_str = f"{vc71:.1f}%" if vc71 is not None else "n/a"
    line = f"| {ts} | {addr} | {name} | {vc71_str} | {pri_label} | {verdict} |\n"
    if not LEDGER_PATH.exists():
        LEDGER_PATH.write_text(
            "# Equivalence Campaign Ledger\n\n"
            "| Date | Address | Function | VC71 | Priority | Verdict |\n"
            "|------|---------|----------|------|----------|--------|\n",
            encoding="utf-8",
        )
    with LEDGER_PATH.open("a", encoding="utf-8") as f:
        f.write(line)


def main():
    import argparse
    parser = argparse.ArgumentParser(description=__doc__[:80])
    parser.add_argument("--limit", type=int, default=50,
                        help="Max functions to process this run (default: 50)")
    parser.add_argument("--resume", action="store_true",
                        help="Skip functions already in campaign_ledger.md")
    parser.add_argument("--dry-run", action="store_true",
                        help="Print priority queue without running unicorn_diff")
    parser.add_argument("--seeds", type=int, default=100,
                        help="Unicorn seeds per function (default: 100)")
    parser.add_argument("--float-tolerance", type=int, default=32,
                        help="ULP float tolerance (default: 32)")
    args = parser.parse_args()

    queue = build_queue(limit=args.limit, resume=args.resume)
    print(f"[batch_equivalence] {len(queue)} functions queued")
    if not queue:
        print("Nothing to do (all already ledgered or above VC71 99%).")
        return 0

    if args.dry_run:
        print(f"\n{'Priority':<10}  {'VC71':<8}  {'Address':<12}  {'Name'}")
        for fn in queue:
            vc = f"{fn['vc71']:.1f}%" if fn['vc71'] else "n/a"
            print(f"  P{fn['priority']}        {vc:<8}  {fn['addr']:<12}  {fn['name']}")
        return 0

    passed = failed = errors = 0
    for fn in queue:
        name = fn["name"]
        addr = fn["addr"]
        vc71 = fn["vc71"]
        print(f"  [{fn['priority']}] {name} ({addr}, vc71={vc71})...", flush=True)
        verdict = run_unicorn(name, args.seeds, args.float_tolerance)
        print(f"       → {verdict}")
        append_ledger(name, addr, vc71, fn["priority"], verdict)
        if "PASS" in verdict.upper() or "equiv" in verdict.lower():
            passed += 1
        elif "TIMEOUT" in verdict or "ERROR" in verdict:
            errors += 1
        else:
            failed += 1

    print(f"\nDone: {passed} pass / {failed} fail / {errors} error  → {LEDGER_PATH}")
    return 0 if failed == 0 and errors == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
