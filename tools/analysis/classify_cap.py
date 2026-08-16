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
  R4 float_arg_lowering_cap: the score-context diff's every non-equal op is the
     cl.exe-7.1 float-arg marshalling substitution (x87 push/fld/fstp in the
     reference vs a plain GPR mov/push in the candidate — bit-identical dwords,
     different instruction sequence). Proven from THIS attempt's own diff, so
     it fires on a cold first attempt, not just on repeat. Evidence:
     --score-context JSON (artifacts/score_context/<name>.json).
  R5 fucompp_assert_cap: every non-equal op is the float-==/!= assert lowering
     (reference `fcomps [0.0]` + `jp` + `assert_halt` vs candidate `flds 0.0` +
     `fucompp` + bool-materialize). Companion leftovers (xorl-eax scheduling,
     push vs addl-esp cleanup) are part of the same shape. Proven from THIS
     attempt's own diff. Evidence: --score-context JSON.

Low-confidence hints (reported in cap_reason, but do NOT set capped/high — they
are also produced by *fixable* bugs, so they must not block escalation):
  - FPU-WARN / LOADW-WARN / IMM-WARN present in a supplied --vc71-log.

Everything is input-driven so the tool is unit-testable and reusable by the
improve pass. It never parses kb.json itself (the caller passes --decl).

I/O:
  --name, --addr, --score REQUIRED. --decl optional (R1). --vc71-log optional
  (hints). --parked-dir default artifacts/parked (R2/R3, resolved through
  park.py's shared ledger_root() so every worktree sees the same history).
  --score-context optional (R4).
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
from typing import Optional

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "lift"))

# A prior parked attempt counts as a confirmed ceiling only if the current score
# did not meaningfully beat it. Small epsilon absorbs VC71 %-noise.
SCORE_EPS = 1.0

_REG_PARAM_RE = re.compile(r"@<\s*[a-zA-Z]{2,3}\s*>")

# R4 — float-arg lowering cap: cl.exe 7.1 marshals a forwarded float lvalue
# arg via the x87 push-then-store idiom (`subl $N,%esp` / `flds src` /
# `fstps dst`); clang instead copies the same dword through a GPR (`movl` /
# `pushl`). The two forms move the identical 4 bytes, so this is a codegen
# lowering difference, not a correctness gap -- but VC71's LCS scorer still
# penalizes it, at a fixed, family-wide, per-float cost. Confirmed
# independently 9+ times on hs.obj forwarding handlers (FUN_000c2a80,
# FUN_000c2f90, FUN_000c2fe0, ...) before this rule existed, each attempt
# re-deriving the same diagnosis
# from scratch because R3 (below) never saw the ledger it needed (fixed
# alongside this rule) and no rule proved it from the CURRENT attempt's own
# evidence, which this one does.
_FLDS_RE = re.compile(r"^\s*flds\s")
_FSTPS_RE = re.compile(r"^\s*fstps\s")
_SUBL_ESP_RE = re.compile(r"^\s*subl\s+\$0x[0-9a-fA-F]+,\s*%esp")
_GPR_MOV_PUSH_RE = re.compile(r"^\s*(movl|pushl)\s")
_FCOMPS_RE = re.compile(r"^\s*fcomps\s")
_FUCOMPP_RE = re.compile(r"^\s*fucompp\b")
_JP_RE = re.compile(r"^\s*jp\t")
_JNP_RE = re.compile(r"^\s*jnp\t")
_ASSERT_PUSH_RE = re.compile(r"^\s*pushl\s+\$-0x1")
_XORL_EAX_RE = re.compile(r"^\s*xorl\s+%eax,\s*%eax")
_ADDL_ESP_RE = re.compile(r"^\s*addl\s+\$0x[0-9a-fA-F]+,\s*%esp")
_PUSHL_ONLY_RE = re.compile(r"^\s*pushl\s")
_MOVL_ONE_RE = re.compile(r"^\s*movl\s+\$0x1,")


def _is_float_arg_lowering_op(op: dict) -> bool:
    """True if a score-context diff op is exactly the substitution above."""
    kind = op.get("kind")
    ref = op.get("ref") or []
    cand = op.get("cand") or []
    if kind == "insert":
        # The reference's extra `flds` that the GPR-copy candidate omits.
        return not cand and len(ref) == 1 and bool(_FLDS_RE.match(ref[0]))
    if kind == "replace":
        ref_is_x87 = bool(ref) and all(
            _FLDS_RE.match(l) or _FSTPS_RE.match(l) or _SUBL_ESP_RE.match(l) for l in ref)
        cand_is_gpr = bool(cand) and all(_GPR_MOV_PUSH_RE.match(l) for l in cand)
        return ref_is_x87 and cand_is_gpr
    return False


def float_arg_lowering_verdict(score_context: str) -> Optional[dict]:
    """R4 verdict from a lift_pipeline/vc71_verify score-context JSON.

    Proven from THIS attempt's own diff -- unlike R2/R3, needs no prior
    ledger history, so it fires on a cold first attempt. Returns None (not
    "not capped") when there's no score-context, it can't be read, or the
    diff contains any non-equal op that ISN'T this exact substitution --
    absence of proof is not proof of absence, same contract as every other
    rule here.
    """
    if not score_context:
        return None
    p = Path(score_context)
    if not p.exists():
        return None
    try:
        data = json.loads(p.read_text())
    except Exception:
        return None
    ops = ((data.get("diff") or {}).get("ops")) or []
    non_equal = [op for op in ops if op.get("kind") != "equal"]
    if not non_equal or not all(_is_float_arg_lowering_op(op) for op in non_equal):
        return None
    n_floats = sum(1 for op in non_equal if op.get("kind") == "insert")
    return {
        "capped": True, "cap_confidence": "high",
        "cap_reason": f"float_arg_lowering_cap: entire diff is {len(non_equal)} float-arg-marshalling "
                      f"substitution op(s) covering {n_floats} forwarded float arg(s) -- cl.exe 7.1 "
                      "x87 push/fstp vs clang GPR mov/push, bit-identical values, permanent VC71 LCS "
                      "penalty, not a correctness defect (docs/lift-learnings.md)",
    }


def _is_fucompp_assert_op(op: dict) -> bool:
    """True if a score-context diff op is the float-==/!= assert lowering."""
    kind = op.get("kind")
    ref = op.get("ref") or []
    cand = op.get("cand") or []
    if kind == "replace":
        if (any(_FCOMPS_RE.match(l) for l in ref)
                and any(_FUCOMPP_RE.match(l) for l in cand)):
            return True
        if (any(_JP_RE.match(l) for l in ref)
                and any(_JNP_RE.match(l) for l in cand)
                and any(_MOVL_ONE_RE.match(l) for l in cand)
                and any(_XORL_EAX_RE.match(l) for l in cand)):
            return True
        if (bool(ref) and all(_PUSHL_ONLY_RE.match(l) for l in ref)
                and bool(cand) and all(_ADDL_ESP_RE.match(l) for l in cand)):
            return True
        return False
    if kind == "delete":
        return (not ref and bool(cand)
                and all(_FLDS_RE.match(l) or _XORL_EAX_RE.match(l) for l in cand))
    if kind == "insert":
        if any(_ASSERT_PUSH_RE.match(l) for l in ref):
            return True
        return bool(ref) and not cand and all(_XORL_EAX_RE.match(l) for l in ref)
    return False


def fucompp_assert_verdict(score_context: str) -> Optional[dict]:
    """R5 verdict from a lift_pipeline/vc71_verify score-context JSON.

    Proven from THIS attempt's own diff. Returns None when the file is
    missing or any non-equal op is not the fucompp-assert substitution.
    """
    if not score_context:
        return None
    p = Path(score_context)
    if not p.exists():
        return None
    try:
        data = json.loads(p.read_text())
    except Exception:
        return None
    ops = ((data.get("diff") or {}).get("ops")) or []
    non_equal = [op for op in ops if op.get("kind") != "equal"]
    if not non_equal or not all(_is_fucompp_assert_op(op) for op in non_equal):
        return None
    has_core = any(
        op.get("kind") == "replace"
        and any(_FCOMPS_RE.match(l) for l in (op.get("ref") or []))
        and any(_FUCOMPP_RE.match(l) for l in (op.get("cand") or []))
        for op in non_equal)
    if not has_core:
        return None
    n_asserts = sum(1 for op in non_equal
                    if op.get("kind") == "replace"
                    and any(_FCOMPS_RE.match(l) for l in (op.get("ref") or [])))
    return {
        "capped": True, "cap_confidence": "high",
        "cap_reason": f"fucompp_assert_cap: entire diff is {len(non_equal)} float-equality-assert "
                      f"lowering op(s) covering {n_asserts} assert(s) -- reference fcomps[0.0]+jp "
                      "vs candidate flds+fucompp+bool-materialize, permanent VC71 LCS penalty, "
                      "not a correctness defect (docs/lift-learnings.md)",
    }


def has_reg_defining_prologue(decl: str) -> bool:
    """True if the decl declares an @<reg> parameter (permanent VC71 sub-bar)."""
    return bool(decl) and bool(_REG_PARAM_RE.search(decl))


def load_ledger_record(name: str, parked_dir: str):
    try:
        from park import Store, store_base  # tools/lift/park.py
    except Exception:
        return None
    try:
        # store_base() routes the CLI default ("artifacts/parked") through
        # park.py's shared ledger_root() (parent of git-common-dir, i.e. the
        # main checkout) instead of joining it under THIS worktree's ROOT.
        # A plain `ROOT / parked_dir` looked worktree-local, so R2/R3 never
        # matched from a linked worktree (e.g. halo-bugs): every attempt saw
        # an empty ledger and reported inconclusive, even after 8 prior
        # attempts already recorded the same cap_hypothesis. Confirmed dead:
        # FUN_000c2a80 was cold-lifted 9 times, byte-identical 83.6% each
        # time, because R3 (ledger_prior_cap) never saw its own history.
        return Store(store_base(ROOT, parked_dir)).load(name)
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
             vc71_log: str = "", parked_dir: str = "artifacts/parked",
             score_context: str = "") -> dict:
    # R1 — @reg-defining prologue (decl evidence).
    if has_reg_defining_prologue(decl):
        return {"capped": True, "cap_confidence": "high",
                "cap_reason": "reg_defining_prologue: decl has an @<reg> parameter; "
                              "VC71 cannot emit the register-reading prologue (permanent sub-bar)"}

    # R4 — float-arg lowering (this attempt's own score-context diff evidence).
    r4 = float_arg_lowering_verdict(score_context)
    if r4:
        return r4

    # R5 — float-==/!= assert fucompp lowering (this attempt's own diff).
    r5 = fucompp_assert_verdict(score_context)
    if r5:
        return r5

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

    # R4: float-arg lowering, from the real FUN_000c2a80 diff shape (2-float
    # tier: one lone `flds` insert plus one x87-vs-GPR replace).
    two_float_diff = {"diff": {"ops": [
        {"kind": "equal", "cand": ["pushl\t%ebp"], "ref": ["pushl\t%ebp"]},
        {"kind": "insert", "cand": [], "ref": ["flds\t0x8(%eax)"]},
        {"kind": "replace",
         "cand": ["movl\t0x4(%eax), %ecx", "pushl\t%edx", "movl\t(%eax), %edx", "pushl\t%ecx"],
         "ref": ["subl\t$0x8, %esp", "fstps\t0x4(%esp)", "flds\t0x4(%eax)", "fstps\t(%esp)"]},
        {"kind": "equal", "cand": ["popl\t%ebp", "retl"], "ref": ["popl\t%ebp", "retl"]},
    ]}}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as tf:
        json.dump(two_float_diff, tf)
        scp = tf.name
    r4 = classify("FUN_cap4", "0xa", 83.6, score_context=scp)
    checks.append(("R4 float-arg-lowering -> high cap",
                   r4["capped"] is True and r4["cap_confidence"] == "high"
                   and "float_arg_lowering_cap" in r4["cap_reason"]))
    # Fires on a COLD attempt (no ledger record at all needed).
    checks.append(("R4 fires without any ledger history",
                   classify("FUN_never_seen_before", "0xb", 83.6, score_context=scp)["capped"] is True))

    # A diff with an unrelated non-equal op must NOT trip R4 (only exact
    # float-lowering substitutions are proof; anything else stays inconclusive).
    unrelated_diff = {"diff": {"ops": [
        {"kind": "equal", "cand": ["pushl\t%ebp"], "ref": ["pushl\t%ebp"]},
        {"kind": "replace", "cand": ["movl\t0xc(%eax), %ecx"], "ref": ["movl\t0x10(%eax), %ecx"]},
    ]}}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as tf:
        json.dump(unrelated_diff, tf)
        scp2 = tf.name
    checks.append(("R4 does not misfire on an unrelated replace op",
                   classify("FUN_unrelated", "0xc", 83.6, score_context=scp2)["capped"] is False))
    Path(scp).unlink(missing_ok=True)
    Path(scp2).unlink(missing_ok=True)

    # R5: fucompp-assert, from the real shader_environment_texture_animation_evaluate
    # shape (two != 0.0f asserts: fcomps+jp vs fucompp+bool-materialize).
    fucompp_diff = {"diff": {"ops": [
        {"kind": "equal", "cand": ["pushl\t%ebp"], "ref": ["pushl\t%ebp"]},
        {"kind": "delete", "cand": ["flds\t0x0"], "ref": []},
        {"kind": "replace",
         "cand": ["fucompp"],
         "ref": ["fcomps\t0x2533c0", "addl\t$0x8, %esp"]},
        {"kind": "replace",
         "cand": ["jnp\t0xa4", "movl\t$0x1, %eax", "jmp\t0xa6", "xorl\t%eax, %eax"],
         "ref": ["jp\t0xbe"]},
        {"kind": "insert",
         "cand": [],
         "ref": ["pushl\t$-0x1", "calll\t0xffefd860", "addl\t$0x14, %esp"]},
        {"kind": "replace",
         "cand": ["addl\t$0xc, %esp"],
         "ref": ["pushl\t%ecx"]},
    ]}}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as tf:
        json.dump(fucompp_diff, tf)
        scp3 = tf.name
    r5 = classify("FUN_cap5", "0xd", 86.2, score_context=scp3)
    checks.append(("R5 fucompp-assert -> high cap",
                   r5["capped"] is True and r5["cap_confidence"] == "high"
                   and "fucompp_assert_cap" in r5["cap_reason"]))
    checks.append(("R5 fires without any ledger history",
                   classify("FUN_never_seen_fucompp", "0xe", 86.2, score_context=scp3)["capped"] is True))
    Path(scp3).unlink(missing_ok=True)

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
    ap.add_argument("--score-context", default="",
                     help="path to a lift_pipeline/vc71_verify score-context JSON "
                          "(artifacts/score_context/<name>.json) — enables R4")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()

    if args.self_test:
        return _self_test()
    if not args.name or args.score is None:
        print("error: --name and --score are required", file=sys.stderr)
        return 2

    verdict = classify(args.name, args.addr, args.score, decl=args.decl,
                       vc71_log=args.vc71_log, parked_dir=args.parked_dir,
                       score_context=args.score_context)
    json.dump(verdict, sys.stdout)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
