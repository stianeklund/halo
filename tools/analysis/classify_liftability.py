#!/usr/bin/env python3
"""Stamp goal-lift candidates with a liftable_class so the workflow can drop
mechanically-non-liftable targets BEFORE spawning expensive research/lift agents.

Motivation (P2 of the goal-lift token-reduction plan): a goal-lift run spawned 66
agents to commit ~14 functions; the rest included switch-case fragments and
already-known structural caps that a cheap classifier can reject up front. Each
such candidate otherwise paid a full opus research + lift round.

Classes (drop everything that is not `liftable`):
  - "fragment"  : a Ghidra switch-case body carved into a fake FUN_ entry. NOT a
                  callable function — porting one installs a redirect that runs a
                  normally-framed C function on the parent dispatcher's mid-flight
                  stack -> RET pops a parent arg as a return address -> crash.
                  Rule (grounded against cachebeta.xbe get_xrefs_to output):
                  ZERO call xrefs AND >=1 COMPUTED_JUMP xref. Requiring
                  COMPUTED_JUMP (not merely "no CALL") avoids misclassifying a
                  legitimately pointer-called function that has only DATA xrefs.
  - "known_cap" : present in the parked ledger with status == "capped_confirmed"
                  (set by `park.py confirm-cap`). A *fixable* parked record
                  (status "parked") is NOT gated out — it is recoverable work.
  - "liftable"  : everything else (default; conservative — never drop on absence
                  of evidence).

Data sources (established by the P2 investigation):
  - kb.json / the selector's score_details / the context_cache have NO xref or
    function-extent data, so fragment detection needs live Ghidra xref shape,
    supplied via --xrefs (one cheap `get_bulk_xrefs` batch agent produces it).
  - known_cap is fully standalone from the parked ledger (no MCP).

I/O contract:
  stdin (or --candidates FILE): JSON array of candidates, each >= {addr, name}.
  --xrefs FILE: JSON object mapping addr -> xref text (verbatim get_xrefs_to
                "result" string) or addr -> list[str] of xref lines. Optional;
                without it, fragment detection is skipped (all stay liftable).
  stdout: the same array, each element gains "liftable_class" and, when dropped,
          "class_reason".

Usage:
  rtk python3 tools/analysis/classify_liftability.py --candidates cand.json --xrefs xr.json
  echo '[{"addr":"0x1280ed","name":"FUN_00128..."}]' | rtk python3 tools/analysis/classify_liftability.py --xrefs xr.json
  rtk python3 tools/analysis/classify_liftability.py --self-test
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

# Repo root = two levels up from tools/analysis/.
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "lift"))


def _norm_addr(addr: str) -> str:
    """Canonicalize an address to lowercase 0x form for cross-source matching."""
    if addr is None:
        return ""
    s = str(addr).strip().lower()
    if not s:
        return ""
    if s.startswith("0x"):
        s = s[2:]
    try:
        return "0x%x" % int(s, 16)
    except ValueError:
        return str(addr).strip().lower()


def _xref_lines(entry) -> list[str]:
    """Normalize an --xrefs entry into a list of xref-descriptor strings.

    Accepts both xref shapes the workflow can supply:
      - get_xrefs_to: a raw string, one xref per line ("From ... [TYPE]").
      - get_bulk_xrefs: a list of {"from","type"} dicts. The `type` value carries
        the DATA/COMPUTED_JUMP/*_CALL signal, so surface it explicitly rather than
        relying on dict repr.
    """
    if entry is None:
        return []
    if isinstance(entry, list):
        out = []
        for x in entry:
            if isinstance(x, dict):
                out.append(str(x.get("type", x)))
            else:
                out.append(str(x))
        return out
    if isinstance(entry, dict):
        # Tolerate the MCP tool's {"result": "..."} shape passed through verbatim.
        entry = entry.get("result", "")
    return [ln for ln in str(entry).splitlines() if ln.strip()]


def is_fragment(xref_lines: list[str]) -> bool:
    """True when the xref shape is a switch-case body, not a callable function.

    ZERO call xrefs AND at least one COMPUTED_JUMP. Empty xref data -> False
    (cannot prove fragment; stay conservative).
    """
    if not xref_lines:
        return False
    has_call = any("CALL" in ln.upper() for ln in xref_lines)
    has_computed_jump = any("COMPUTED_JUMP" in ln.upper() for ln in xref_lines)
    return (not has_call) and has_computed_jump


def load_confirmed_caps(parked_dir: str) -> set[str]:
    """Set of normalized addrs whose parked record is a confirmed structural cap."""
    try:
        from park import Store  # tools/lift/park.py
    except Exception:
        return set()
    caps: set[str] = set()
    try:
        for rec in Store(ROOT / parked_dir).all():
            if rec.get("status") == "capped_confirmed":
                a = _norm_addr(rec.get("addr", ""))
                if a:
                    caps.add(a)
    except Exception:
        return set()
    return caps


def classify(candidates: list[dict], xrefs: dict, confirmed_caps: set[str]) -> list[dict]:
    """Stamp each candidate with liftable_class (+ class_reason when dropped)."""
    out = []
    for c in candidates:
        c = dict(c)
        a = _norm_addr(c.get("addr", ""))
        cls, reason = "liftable", ""
        # known_cap takes precedence: it is the strongest "do not spend agents" signal.
        if a and a in confirmed_caps:
            cls, reason = "known_cap", "parked ledger: status=capped_confirmed"
        elif is_fragment(_xref_lines(xrefs.get(a) or xrefs.get(c.get("addr", "")))):
            cls, reason = "fragment", "xref shape: 0 CALL + >=1 COMPUTED_JUMP (switch-case body, not callable)"
        c["liftable_class"] = cls
        if reason:
            c["class_reason"] = reason
        out.append(c)
    return out


def _self_test() -> int:
    frag = ["From 00128298 [DATA]", "From 00127f62 in FUN_00127ea0 [COMPUTED_JUMP]"]
    callable_ = ["From 0012702e in FUN_00126fe0 [UNCONDITIONAL_CALL]",
                 "From 0012ef3a in FUN_0012eef0 [UNCONDITIONAL_CALL]"]
    ptr_only = ["From 001a0000 [DATA]"]  # pointer-called: DATA only, no COMPUTED_JUMP
    checks = [
        ("fragment shape", is_fragment(frag) is True),
        ("callable shape", is_fragment(callable_) is False),
        ("pointer-called (DATA only) not fragment", is_fragment(ptr_only) is False),
        ("empty xrefs not fragment", is_fragment([]) is False),
        ("computed_jump WITH a call is not fragment",
         is_fragment(frag + callable_) is False),
        # get_bulk_xrefs structured shape (list of {from,type} dicts):
        ("structured dict fragment",
         is_fragment(_xref_lines([{"from": "1", "type": "DATA"}, {"from": "2", "type": "COMPUTED_JUMP"}])) is True),
        ("structured dict callable",
         is_fragment(_xref_lines([{"from": "1", "type": "UNCONDITIONAL_CALL"}])) is False),
        ("addr norm", _norm_addr("0X1280ED") == "0x1280ed" and _norm_addr("1280ed") == "0x1280ed"),
    ]
    cands = [
        {"addr": "0x1280ed", "name": "FUN_00128"},
        {"addr": "0x1296b0", "name": "network_connection_new"},
        {"addr": "0x2222", "name": "capped_fn"},
        {"addr": "0x3333", "name": "plain"},
    ]
    xr = {"0x1280ed": frag, "0x1296b0": callable_}
    res = classify(cands, xr, confirmed_caps={"0x2222"})
    by = {r["addr"]: r["liftable_class"] for r in res}
    checks += [
        ("classify fragment", by["0x1280ed"] == "fragment"),
        ("classify callable liftable", by["0x1296b0"] == "liftable"),
        ("classify known_cap", by["0x2222"] == "known_cap"),
        ("classify default liftable", by["0x3333"] == "liftable"),
    ]
    ok = True
    for name, cond in checks:
        print(f"  {'PASS' if cond else 'FAIL'}: {name}")
        ok = ok and cond
    print("SELF-TEST", "OK" if ok else "FAILED")
    return 0 if ok else 1


def main() -> int:
    ap = argparse.ArgumentParser(description="Stamp goal-lift candidates with liftable_class.")
    ap.add_argument("--candidates", help="JSON array file; default stdin")
    ap.add_argument("--xrefs", help="JSON map addr->xref text/lines (from get_bulk_xrefs)")
    ap.add_argument("--parked-dir", default="artifacts/parked")
    ap.add_argument("--summary", action="store_true", help="also print a class tally to stderr")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()

    if args.self_test:
        return _self_test()

    raw = Path(args.candidates).read_text() if args.candidates else sys.stdin.read()
    candidates = json.loads(raw) if raw.strip() else []
    if not isinstance(candidates, list):
        print("error: candidates must be a JSON array", file=sys.stderr)
        return 2

    xrefs = {}
    if args.xrefs:
        try:
            xrefs = json.loads(Path(args.xrefs).read_text() or "{}")
        except (OSError, json.JSONDecodeError) as e:
            print(f"warning: could not read --xrefs ({e}); fragment detection skipped", file=sys.stderr)
    # Normalize xref map keys so lookups by canonical addr succeed.
    xrefs = {_norm_addr(k): v for k, v in xrefs.items()}

    result = classify(candidates, xrefs, load_confirmed_caps(args.parked_dir))

    if args.summary:
        tally = {}
        for r in result:
            tally[r["liftable_class"]] = tally.get(r["liftable_class"], 0) + 1
        print("liftable_class tally: " + json.dumps(tally), file=sys.stderr)

    json.dump(result, sys.stdout)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
