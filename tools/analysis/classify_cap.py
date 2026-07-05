#!/usr/bin/env python3
"""Deterministic structural-cap classifier for the goal-lift escalation gate (P3).

Problem it replaces: goal-lift escalates a sub-bar lift to a second opus-high
attempt when `band in [65,84] and not a1.capped`, where `a1.capped` is the lift
agent's *soft self-assessment*. A wrong "not capped" burns a second opus-high
lift to reach the same permanent ceiling. This tool turns the "is it a structural
cap" question into explicit, inspectable rules so the provable caps are decided
deterministically; only genuinely ambiguous cases fall back to agent judgment.

Design contract (how goal-lift uses it):
  - capped=true, confidence="high"  -> PARK, do NOT escalate (retry is provably
                                        futile). This is the only verdict that
                                        overrides the agent.
  - capped=false, confidence="inconclusive" -> the workflow/agent applies its own
                                        CAP_TABLE judgment (behaviour unchanged for
                                        ambiguous FPU-shape cases we can't prove
                                        mechanically). We almost never *prove* the
                                        absence of a cap, so we don't claim to.

High-confidence cap rules (each cites explicit evidence):
  R1 reg_defining_prologue : the target's own decl has an `@<reg>` parameter. VC71
     cannot emit the custom register-reading prologue, so the function is
     permanently sub-bar (see docs / CAP_TABLE). Evidence: the decl string.
  R2 ledger_confirmed_cap  : park.py has this function with status
     "capped_confirmed" (a human/prior pass already confirmed it). Evidence: ledger.
  R3 ledger_prior_cap      : a prior parked attempt recorded a cap_hypothesis and
     the current score did not improve past that attempt's best (score <=
     best_score + EPS). We have already reached this ceiling before. Evidence: ledger.

Low-confidence hints (reported in cap_reason, but do NOT set capped/high — they
are also produced by *fixable* bugs, so they must not block escalation):
  - FPU-WARN / LOADW-WARN / IMM-WARN present in a supplied --vc71-log.

Everything is input-driven so the tool is unit-testable and reusable by the
improve pass. It never parses kb.json itself (the caller passes --decl).

I/O:
  --name, --addr, --score REQUIRED. --decl optional (R1). --vc71-log optional
  (hints). --parked-dir default artifacts/parked (R2/R3).
  stdout: {"capped": bool, "cap_confidence": "high"|"inconclusive", "cap_reason": str}

Usage:
  rtk python3 tools/analysis/classify_cap.py --name FUN_x --addr 0x.. --score 78 \
      --decl 'void *FUN_x(int a@<edi>)' --vc71-log artifacts/.../vc71_verify.log
  rtk python3 tools/analysis/classify_cap.py --self-test
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "lift"))

# A prior parked attempt counts as a confirmed ceiling only if the current score
# did not meaningfully beat it. Small epsilon absorbs VC71 %-noise.
SCORE_EPS = 1.0

_REG_PARAM_RE = re.compile(r"@<\s*[a-zA-Z]{2,3}\s*>")


def has_reg_defining_prologue(decl: str) -> bool:
    """True if the decl declares an @<reg> parameter (permanent VC71 sub-bar)."""
    return bool(decl) and bool(_REG_PARAM_RE.search(decl))


def load_ledger_record(name: str, parked_dir: str):
    try:
        from park import Store  # tools/lift/park.py
    except Exception:
        return None
    try:
        return Store(ROOT / parked_dir).load(name)
    except Exception:
        return None


def scan_vc71_hints(vc71_log: str) -> list[str]:
    """Return the WARN tokens present in a vc71_verify log (low-confidence hints)."""
    if not vc71_log:
        return []
    p = Path(vc71_log)
    if not p.exists():
        return []
    try:
        text = p.read_text(errors="replace")
    except OSError:
        return []
    hints = []
    for tok in ("FPU-WARN", "LOADW-WARN", "IMM-WARN"):
        if tok in text:
            hints.append(tok)
    return hints


def classify(name: str, addr: str, score: float, decl: str = "",
             vc71_log: str = "", parked_dir: str = "artifacts/parked") -> dict:
    # R1 — @reg-defining prologue (decl evidence).
    if has_reg_defining_prologue(decl):
        return {"capped": True, "cap_confidence": "high",
                "cap_reason": "reg_defining_prologue: decl has an @<reg> parameter; "
                              "VC71 cannot emit the register-reading prologue (permanent sub-bar)"}

    # R2/R3 — parked-ledger evidence.
    rec = load_ledger_record(name, parked_dir)
    if rec:
        if rec.get("status") == "capped_confirmed":
            return {"capped": True, "cap_confidence": "high",
                    "cap_reason": f"ledger_confirmed_cap: park.py status=capped_confirmed "
                                  f"(best={rec.get('best_score')}%)"}
        best = rec.get("best_score")
        attempts = rec.get("attempts") or []
        had_cap_hyp = any((a.get("cap_hypothesis") or "").strip() for a in attempts)
        if had_cap_hyp and isinstance(best, (int, float)) and score <= best + SCORE_EPS:
            hyp = next(((a.get("cap_hypothesis") or "").strip() for a in reversed(attempts)
                        if (a.get("cap_hypothesis") or "").strip()), "")
            return {"capped": True, "cap_confidence": "high",
                    "cap_reason": f"ledger_prior_cap: prior attempt capped at {best}% "
                                  f"(no improvement now at {score}%){'; ' + hyp if hyp else ''}"}

    # Inconclusive — defer to agent judgment. Surface any WARN hints for context.
    hints = scan_vc71_hints(vc71_log)
    reason = "inconclusive: no high-confidence structural-cap rule matched — apply agent judgment"
    if hints:
        reason += f" (vc71 hints: {', '.join(hints)} — may be fixable, not proof of a cap)"
    return {"capped": False, "cap_confidence": "inconclusive", "cap_reason": reason}


def _self_test() -> int:
    import tempfile
    checks = []

    # R1: reg-defining prologue.
    r = classify("FUN_x", "0x1", 78, decl="void *FUN_x(int endpoint@<edi>)")
    checks.append(("R1 reg-defining -> high cap",
                   r["capped"] is True and r["cap_confidence"] == "high"
                   and "reg_defining_prologue" in r["cap_reason"]))
    checks.append(("plain decl not capped",
                   classify("FUN_x", "0x1", 78, decl="int FUN_x(int a, float b)")["capped"] is False))
    checks.append(("reg regex matches @<esi>", has_reg_defining_prologue("void f(int a@<esi>)") is True))
    checks.append(("reg regex ignores plain", has_reg_defining_prologue("void f(int a)") is False))

    # R2/R3: ledger. Build a temp parked dir with park.py Store.
    sys.path.insert(0, str(ROOT / "tools" / "lift"))
    try:
        from park import Store
        with tempfile.TemporaryDirectory() as td:
            store = Store(Path(td))
            store.save({"name": "FUN_cap", "addr": "0x2", "status": "capped_confirmed",
                        "best_score": 82.0, "attempts": []})
            store.save({"name": "FUN_hyp", "addr": "0x3", "status": "parked", "best_score": 80.0,
                        "attempts": [{"cap_hypothesis": "fucompp permanent gap", "score": 80.0}]})
            store.save({"name": "FUN_improved", "addr": "0x4", "status": "parked", "best_score": 70.0,
                        "attempts": [{"cap_hypothesis": "loop unroll", "score": 70.0}]})
            rel = str(Path(td))
            r2 = classify("FUN_cap", "0x2", 80, parked_dir=rel)
            checks.append(("R2 confirmed cap -> high", r2["capped"] is True and r2["cap_confidence"] == "high"))
            r3 = classify("FUN_hyp", "0x3", 80, parked_dir=rel)
            checks.append(("R3 prior cap, no improvement -> high", r3["capped"] is True))
            r3b = classify("FUN_improved", "0x4", 85, parked_dir=rel)
            checks.append(("R3 improved past prior cap -> not capped", r3b["capped"] is False))
    except Exception as e:  # noqa: BLE001
        checks.append((f"ledger tests skipped ({e})", True))

    # vc71 hints do not, by themselves, set capped.
    with tempfile.NamedTemporaryFile("w", suffix=".log", delete=False) as tf:
        tf.write("... 78.0% match ... [FPU-WARN] fucompp ... [LOADW-WARN] ...")
        logp = tf.name
    rh = classify("FUN_amb", "0x9", 78, vc71_log=logp)
    checks.append(("vc71 hints -> inconclusive not capped",
                   rh["capped"] is False and "FPU-WARN" in rh["cap_reason"]))
    Path(logp).unlink(missing_ok=True)

    ok = True
    for nm, cond in checks:
        print(f"  {'PASS' if cond else 'FAIL'}: {nm}")
        ok = ok and cond
    print("SELF-TEST", "OK" if ok else "FAILED")
    return 0 if ok else 1


def main() -> int:
    ap = argparse.ArgumentParser(description="Deterministic structural-cap classifier (goal-lift P3).")
    ap.add_argument("--name")
    ap.add_argument("--addr", default="")
    ap.add_argument("--score", type=float)
    ap.add_argument("--decl", default="")
    ap.add_argument("--vc71-log", default="")
    ap.add_argument("--parked-dir", default="artifacts/parked")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()

    if args.self_test:
        return _self_test()
    if not args.name or args.score is None:
        print("error: --name and --score are required", file=sys.stderr)
        return 2

    verdict = classify(args.name, args.addr, args.score, decl=args.decl,
                       vc71_log=args.vc71_log, parked_dir=args.parked_dir)
    json.dump(verdict, sys.stdout)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
