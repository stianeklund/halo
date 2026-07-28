#!/usr/bin/env python3
"""Re-validate cached `z3_proven` flags under the strict lifter (Phase 0.3).

Why
---
`x86_to_z3` used to skip instructions it could not model, and
`z3_equiv.prove_equivalence` never checked for that. Both sides of a
comparison dropped the same semantics, so non-equivalent code could lift to
identical formulas and the solver returned UNSAT -- a false proof. Worse, a
`z3_proven` verdict short-circuited the Unicorn sweep entirely, so the
affected function received no testing at all.

Both holes are now closed (see tools/equivalence/test_z3_strict_lift.py), but
the flags recorded *before* the fix are still sitting in leaf_cache.json and
are still being reported by batch_verify and the CI dashboard. This script
re-runs the proof for every cached `z3_proven` entry under the strict lifter
and reports which proofs survive.

Usage
-----
    python3 tools/audit/revalidate_z3_proofs.py            # report only
    python3 tools/audit/revalidate_z3_proofs.py --apply    # strip dead flags
    python3 tools/audit/revalidate_z3_proofs.py --check    # exit 1 if any died

Writes artifacts/audit/z3_proof_revalidation.json.
"""

import argparse
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
KB_JSON = ROOT / "kb.json"
LEAF_CACHE = ROOT / "tools" / "equivalence" / "leaf_cache.json"
UNICORN_DIFF = ROOT / "tools" / "equivalence" / "unicorn_diff.py"
REPORT = ROOT / "artifacts" / "audit" / "z3_proof_revalidation.json"

# Just enough seeds to keep the run honest; the proof verdict is what we want.
SEEDS = "5"
TIMEOUT = 180

# Reasons that mean "we could not re-check this proof", NOT "this proof was
# unsound". A missing build object or delinked reference says nothing about
# the formula -- lumping these in with genuinely revoked proofs would both
# overstate the damage and strip flags we have no evidence against.
INFRA_REASONS = {
    "missing_kb_entry", "missing_decl", "missing_delinked_reference",
    "missing_build_object", "oracle_extract_failed", "lifted_extract_failed",
    "empty_oracle_code", "empty_lifted_code", "unicorn_unavailable",
    "external_relocations", "stub_convention_mismatch",
}


_DECL_NAME = re.compile(r'([A-Za-z_]\w*)\s*\(')


def resolve_names() -> dict:
    """Map normalized address -> function name, parsed from kb.json decls.

    unicorn_diff can be queried by address, but it then looks for a symbol
    literally named "0x12f80" (or "FUN_00012f80") in the build object -- which
    fails for every function that has since been given a real name. Resolving
    to the name up front is what makes re-validation actually reach the code.
    """
    try:
        kb = json.loads(KB_JSON.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {}

    out = {}

    def walk(node):
        if isinstance(node, dict):
            addr, decl = node.get("addr"), node.get("decl")
            if isinstance(addr, str) and isinstance(decl, str):
                m = _DECL_NAME.search(decl)
                if m:
                    out[hex(int(addr, 16)).lower()] = m.group(1)
            for v in node.values():
                walk(v)
        elif isinstance(node, list):
            for v in node:
                walk(v)

    walk(kb)
    return out


def load_proven(cache: dict) -> list:
    out = []
    for addr, entry in cache.items():
        if isinstance(entry, dict) and entry.get("z3_proven") is True:
            out.append(addr)
    return sorted(out)


def revalidate(addr: str, name: str = None) -> dict:
    """Re-run the proof for one address. Returns a result record."""
    with tempfile.NamedTemporaryFile(suffix=".json", delete=False) as tf:
        out_path = Path(tf.name)
    query = name or addr
    cmd = [
        sys.executable, str(UNICORN_DIFF), query,
        "--z3-equiv", "--allow-stubs", "--seeds", SEEDS,
        "-q", "--no-leaf-cache", "--output-json", str(out_path),
    ]
    rec = {"addr": addr, "name": name}
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True,
                              timeout=TIMEOUT, cwd=str(ROOT))
        payload = {}
        if out_path.exists():
            try:
                payload = json.loads(out_path.read_text(encoding="utf-8"))
            except json.JSONDecodeError:
                pass
        rec["target"] = payload.get("target")
        rec["status"] = payload.get("status")
        rec["reason"] = payload.get("reason")
        rec["still_proven"] = bool(payload.get("z3_proven"))
        rec["seeds_passed"] = payload.get("passed")
        rec["seeds_failed"] = payload.get("failed")
        rec["contradicted"] = bool(payload.get("z3_proof_contradicted"))
        if not payload:
            rec["still_proven"] = False
            rec["reason"] = f"no payload (rc={proc.returncode})"
        # Surface the Z3 line from the run so a dead proof says WHY.
        for line in (proc.stdout or "").splitlines():
            s = line.strip()
            if s.startswith("Z3 "):
                rec.setdefault("z3_line", s)
    except subprocess.TimeoutExpired:
        rec["still_proven"] = False
        rec["reason"] = f"timeout after {TIMEOUT}s"
        rec["verdict"] = "unverifiable"
        out_path.unlink(missing_ok=True)
        return rec
    finally:
        out_path.unlink(missing_ok=True)

    if rec["still_proven"]:
        rec["verdict"] = "survived"
    elif (rec.get("reason") or "") in INFRA_REASONS:
        rec["verdict"] = "unverifiable"
    else:
        rec["verdict"] = "revoked"
    return rec


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--apply", action="store_true",
                    help="strip z3_proven from entries whose proof no longer holds")
    ap.add_argument("--check", action="store_true",
                    help="exit 1 if any cached proof failed to re-validate")
    ap.add_argument("--limit", type=int, default=0,
                    help="only re-validate the first N entries (smoke run)")
    args = ap.parse_args()

    try:
        cache = json.loads(LEAF_CACHE.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as e:
        print(f"ERROR: cannot read {LEAF_CACHE}: {e}", file=sys.stderr)
        return 2

    proven = load_proven(cache)
    if args.limit:
        proven = proven[:args.limit]
    if not proven:
        print("No cached z3_proven entries.")
        return 0

    names = resolve_names()
    print(f"Re-validating {len(proven)} cached z3_proven entries "
          f"under the strict lifter...\n")

    results = []
    for i, addr in enumerate(proven, 1):
        rec = revalidate(addr, names.get(hex(int(addr, 16)).lower()))
        results.append(rec)
        mark = {"survived": "OK    ", "revoked": "REVOKED",
                "unverifiable": "UNVERIF"}[rec["verdict"]]
        name = rec.get("name") or rec.get("target") or addr
        detail = ("" if rec["verdict"] == "survived"
                  else f"  <- {rec.get('z3_line') or rec.get('reason')}")
        flag = "  [CONTRADICTED BY SEEDS]" if rec.get("contradicted") else ""
        print(f"  [{i:3d}/{len(proven)}] {mark} {name:<44s}{detail}{flag}")

    survived = [r for r in results if r["verdict"] == "survived"]
    died = [r for r in results if r["verdict"] == "revoked"]
    unverif = [r for r in results if r["verdict"] == "unverifiable"]
    contradicted = [r for r in results if r.get("contradicted")]

    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps({
        "total": len(results),
        "survived": len(survived),
        "revoked": len(died),
        "unverifiable": len(unverif),
        "contradicted_by_seeds": len(contradicted),
        "results": results,
    }, indent=2) + "\n", encoding="utf-8")

    print(f"\n  survived     : {len(survived)}")
    print(f"  revoked      : {len(died)}   (proof re-ran and no longer holds)")
    print(f"  unverifiable : {len(unverif)}   (harness could not re-check; "
          f"flag left in place)")
    if contradicted:
        print(f"  CONTRADICTED BY SEEDS: {len(contradicted)}  "
              f"(proof and emulation disagree -- investigate)")
    print(f"\nReport: {REPORT.relative_to(ROOT)}")

    if args.apply and died:
        for r in died:
            entry = cache.get(r["addr"])
            if isinstance(entry, dict):
                entry.pop("z3_proven", None)
        LEAF_CACHE.write_text(
            json.dumps(dict(sorted(cache.items())), indent=2) + "\n",
            encoding="utf-8")
        print(f"Stripped z3_proven from {len(died)} entries in "
              f"{LEAF_CACHE.relative_to(ROOT)}")
    elif died and not args.apply:
        print("Re-run with --apply to strip the revoked flags.")

    if args.check and died:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
