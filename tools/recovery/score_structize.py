#!/usr/bin/env python3
"""Automated VC71 score improvement via mechanical source fixes.

Reads score-context packs (artifacts/score_context/*.json) to find fixable
diagnostics, generates candidate source edits, and gates each with a VC71
recompile + score check.  Only improvements are kept; regressions are reverted.

Usage:
    # Read-only census of fixable diagnostics:
    python3 tools/recovery/score_structize.py scan --source src/halo/ai/actor_looking.c

    # Apply all mechanical fixes with per-edit gate:
    python3 tools/recovery/score_structize.py fix --source src/halo/ai/actor_looking.c

    # One rule only:
    python3 tools/recovery/score_structize.py fix --source src/halo/ai/actor_looking.c --rule imm_wrong_literal

    # Dry-run (propose but don't apply):
    python3 tools/recovery/score_structize.py fix --source src/halo/ai/actor_looking.c --dry-run
"""

import argparse
import json
import re
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

SCORE_CONTEXT_DIR = ROOT / "artifacts" / "score_context"

FIXABLE_RULES = {
    "imm_wrong_literal",
    "loadw_field_width",
    "fcom_bound_sense",
    "fpu_operand_order",
}

# ---------------------------------------------------------------------------
# Warning parsers -- extract structured data from warning strings
# ---------------------------------------------------------------------------

_IMM_NEARMISS = re.compile(
    r"near-miss float literal -- reference (0x[0-9a-f]+) .* vs our lift (0x[0-9a-f]+)")
_IMM_REF_ABSENT = re.compile(
    r"reference constant (0x[0-9a-f]+) .*absent from our lift")
_IMM_CAND_EXTRA = re.compile(
    r"our lift has constant (0x[0-9a-f]+) .*not in the reference")

_LOADW_RE = re.compile(
    r"(reference|our lift) reads a narrow field \[(n\d+) disp:(0x-?[0-9a-f]+)\]")

_FCOM_GUARD = re.compile(
    r"testb \$(0x[0-9a-f]+), %ah ; (j\w+)")


def _hex_to_float(hex_str):
    v = int(hex_str, 16) & 0xffffffff
    return struct.unpack("<f", struct.pack("<I", v))[0]


def _float_to_hex(f):
    return "0x%08x" % struct.unpack("<I", struct.pack("<f", f))[0]


def _float_literals(f):
    """Generate plausible C literal forms for a float value (for source grep)."""
    if f != f:
        return []
    forms = []
    if f == int(f) and abs(f) < 1e9:
        n = int(f)
        forms.append("%d.0f" % n)
        forms.append("%d.0" % n)
        forms.append("%d.f" % n)
        forms.append("(float)%d.0" % n)
        forms.append("(float)%d" % n)
        forms.append("%df" % n)
    else:
        seen = set()
        for prec in [9, 8, 7, 6]:
            s = ("%%.%dg" % prec) % f
            if s in seen:
                continue
            seen.add(s)
            forms.append(s + "f")
            forms.append(s)
            if "." not in s:
                forms.append(s + ".0f")
                forms.append(s + ".0")
    return forms


def _exact_float_literal(hex_val):
    """Return the shortest C float literal that round-trips to the given hex."""
    v = int(hex_val, 16) & 0xffffffff
    f = struct.unpack("<f", struct.pack("<I", v))[0]
    if f != f:
        return None
    if f == int(f) and abs(f) < 1e9:
        return "%d.0f" % int(f)
    for prec in range(7, 20):
        s = ("%%.%dg" % prec) % f
        check = struct.unpack("<I", struct.pack("<f", float(s)))[0]
        if check == v:
            return s + "f"
    return "%.9gf" % f


def _parse_imm_warnings(warnings):
    """Parse IMM warnings into actionable fix items."""
    items = []
    for w in warnings:
        m = _IMM_NEARMISS.search(w)
        if m:
            ref_hex, cand_hex = m.group(1), m.group(2)
            items.append({
                "kind": "near_miss",
                "ref_hex": ref_hex,
                "cand_hex": cand_hex,
                "ref_float": _hex_to_float(ref_hex),
                "cand_float": _hex_to_float(cand_hex),
            })
            continue

        m_ref = _IMM_REF_ABSENT.search(w)
        m_cand = _IMM_CAND_EXTRA.search(w)
        if m_ref:
            items.append({
                "kind": "ref_absent",
                "ref_hex": m_ref.group(1),
                "ref_float": _hex_to_float(m_ref.group(1)),
            })
        elif m_cand:
            items.append({
                "kind": "cand_extra",
                "cand_hex": m_cand.group(1),
                "cand_float": _hex_to_float(m_cand.group(1)),
            })
    return items


def _parse_loadw_warnings(warnings):
    """Parse LOADW warnings into fix items."""
    items = []
    for w in warnings:
        m = _LOADW_RE.search(w)
        if m:
            side = m.group(1)
            width_class = m.group(2)
            disp = m.group(3)
            items.append({
                "side": side,
                "width_class": width_class,
                "disp": disp,
                "direction": "ref_narrow" if side == "reference" else "cand_narrow",
            })
    return items


def _parse_fcom_warnings(warnings):
    """Parse FCOM warnings for guard shape info."""
    items = []
    for w in warnings:
        m = _FCOM_GUARD.search(w)
        if m:
            mask_hex = m.group(1)
            jcc = m.group(2)
            items.append({
                "mask": mask_hex,
                "jcc": jcc,
                "raw": w.strip(),
            })
        else:
            items.append({"raw": w.strip()})
    return items


# ---------------------------------------------------------------------------
# Score-context pack loading
# ---------------------------------------------------------------------------

def _load_packs_for_tu(source):
    """Load all score-context packs whose TU matches the given source file."""
    packs = []
    if not SCORE_CONTEXT_DIR.is_dir():
        return packs
    for p in sorted(SCORE_CONTEXT_DIR.glob("*.json")):
        try:
            data = json.loads(p.read_text())
        except (json.JSONDecodeError, OSError):
            continue
        if data.get("tu") == source:
            packs.append(data)
    return packs


# ---------------------------------------------------------------------------
# scan subcommand
# ---------------------------------------------------------------------------

def cmd_scan(source, refresh=False):
    """Read-only census: classify each diagnostic as fixable or needs-llm."""
    if refresh:
        _refresh_packs(source)

    packs = _load_packs_for_tu(source)
    if not packs:
        return {"source": source, "functions_scored": 0,
                "fixable": [], "needs_llm": [],
                "error": "no score-context packs found for this TU"}

    fixable = []
    needs_llm = []

    for pack in packs:
        name = pack["name"]
        warnings = pack.get("warnings", {})
        classification = pack.get("classification", [])

        for cls in classification:
            rule = cls["rule"]
            entry = {
                "function": name,
                "rule": rule,
                "evidence": cls.get("evidence", ""),
                "action": cls.get("action", ""),
            }

            if rule == "imm_wrong_literal":
                parsed = _parse_imm_warnings(warnings.get("imm", []))
                near_misses = [p for p in parsed if p["kind"] == "near_miss"]
                if near_misses:
                    entry["near_misses"] = near_misses
                    entry["confidence"] = "high"
                    fixable.append(entry)
                else:
                    entry["confidence"] = "medium"
                    entry["detail"] = "ref-absent or cand-extra without pairing"
                    needs_llm.append(entry)

            elif rule == "loadw_field_width":
                parsed = _parse_loadw_warnings(warnings.get("loadw", []))
                ref_narrow = [p for p in parsed if p["direction"] == "ref_narrow"]
                if ref_narrow:
                    entry["ref_narrow_fields"] = ref_narrow
                    entry["confidence"] = "medium"
                    fixable.append(entry)
                else:
                    entry["confidence"] = "low"
                    entry["detail"] = "candidate narrower than reference"
                    needs_llm.append(entry)

            elif rule == "fcom_bound_sense":
                parsed = _parse_fcom_warnings(warnings.get("fcom", []))
                entry["guards"] = parsed
                entry["confidence"] = "medium"
                fixable.append(entry)

            elif rule == "fpu_operand_order":
                entry["fpu_warnings"] = warnings.get("fpu", [])
                entry["confidence"] = "low"
                fixable.append(entry)

            else:
                entry["confidence"] = "not-automatable"
                needs_llm.append(entry)

    return {
        "source": source,
        "functions_scored": len(packs),
        "fixable": fixable,
        "needs_llm": needs_llm,
    }


# ---------------------------------------------------------------------------
# fix subcommand -- apply mechanical fixes with per-edit VC71 gate
# ---------------------------------------------------------------------------

def _refresh_packs(source):
    """Re-run vc71_verify to regenerate score-context packs."""
    proc = subprocess.run(
        [sys.executable, str(ROOT / "tools" / "verify" / "vc71_verify.py"), source],
        cwd=str(ROOT), capture_output=True, text=True)
    if proc.returncode != 0:
        raise RuntimeError("vc71_verify failed: %s" % proc.stderr[:500])


def _measure(source):
    """Measure scores via score_improve.measure()."""
    from tools.verify.score_improve import measure
    return measure(source, ROOT)


def _backup_source(source_path):
    """Create a backup of the source file, return backup path."""
    fd, backup = tempfile.mkstemp(suffix=".bak", prefix=source_path.stem + "_")
    import os; os.close(fd)
    shutil.copy2(source_path, backup)
    return Path(backup)


def _restore_source(source_path, backup_path):
    """Restore source from backup."""
    shutil.copy2(backup_path, source_path)


def _read_function_lines(source_path, func_name):
    """Find the approximate line range of a function in a C source file.

    Returns (start_line, end_line) as 0-indexed, or None if not found.
    Uses kb.json addr to find the function in the source.
    """
    text = source_path.read_text()
    lines = text.splitlines()

    func_re = re.compile(r'\b' + re.escape(func_name) + r'\b')
    for i, line in enumerate(lines):
        if func_re.search(line) and ("(" in line or "{" in line):
            start = i
            brace_depth = 0
            found_open = False
            for j in range(i, len(lines)):
                for ch in lines[j]:
                    if ch == "{":
                        brace_depth += 1
                        found_open = True
                    elif ch == "}":
                        brace_depth -= 1
                if found_open and brace_depth == 0:
                    return (start, j)
            return (start, min(i + 200, len(lines) - 1))
    return None


# --- Fix generators ---

_FLOAT_LIT_RE = re.compile(
    r'(?<![0-9a-zA-Z_.])'
    r'(-?\d+\.\d+(?:e[+-]?\d+)?f?'
    r'|-?\d+\.f?'
    r'|-?\d+f)'
    r'(?![0-9a-zA-Z_])')


def _source_float_to_hex(lit_str):
    """Convert a C float literal string to its IEEE754 hex."""
    s = lit_str.rstrip("f").rstrip("F")
    try:
        f = float(s)
        return struct.unpack("<I", struct.pack("<f", f))[0]
    except (ValueError, OverflowError):
        return None


def _gen_imm_fixes(source_path, diag):
    """Generate candidate edits for near-miss float literals."""
    candidates = []
    near_misses = diag.get("near_misses", [])
    if not near_misses:
        return candidates

    text = source_path.read_text()

    for nm in near_misses:
        cand_hex_int = int(nm["cand_hex"], 16) & 0xffffffff
        ref_hex = nm["ref_hex"]
        ref_lit = _exact_float_literal(ref_hex)
        if not ref_lit:
            continue

        for m in _FLOAT_LIT_RE.finditer(text):
            src_lit = m.group(0)
            src_hex = _source_float_to_hex(src_lit)
            if src_hex == cand_hex_int:
                new_text = text[:m.start()] + ref_lit + text[m.end():]
                if new_text != text:
                    candidates.append({
                        "description": "IMM: %s -> %s (ref %s)" % (
                            src_lit, ref_lit, ref_hex),
                        "old_literal": src_lit,
                        "new_literal": ref_lit,
                        "new_text": new_text,
                    })
                break

    return candidates


def _gen_loadw_fixes(source_path, diag):
    """Generate candidate edits for narrow-field width mismatches."""
    candidates = []
    ref_narrow = diag.get("ref_narrow_fields", [])
    if not ref_narrow:
        return candidates

    text = source_path.read_text()

    for item in ref_narrow:
        width_class = item["width_class"]
        disp = item["disp"]

        if width_class == "n8":
            narrow_types = ["int8_t", "uint8_t", "char"]
            wide_patterns = [
                (r'\*\s*\(\s*(int\s*\*)\s*\)', "*(int8_t *)"),
                (r'\*\s*\(\s*(int32_t\s*\*)\s*\)', "*(int8_t *)"),
                (r'\*\s*\(\s*(uint32_t\s*\*)\s*\)', "*(uint8_t *)"),
                (r'\*\s*\(\s*(short\s*\*)\s*\)', "*(int8_t *)"),
                (r'\*\s*\(\s*(int16_t\s*\*)\s*\)', "*(int8_t *)"),
                (r'\*\s*\(\s*(uint16_t\s*\*)\s*\)', "*(uint8_t *)"),
            ]
        elif width_class == "n16":
            narrow_types = ["int16_t", "uint16_t", "short"]
            wide_patterns = [
                (r'\*\s*\(\s*(int\s*\*)\s*\)', "*(int16_t *)"),
                (r'\*\s*\(\s*(int32_t\s*\*)\s*\)', "*(int16_t *)"),
                (r'\*\s*\(\s*(uint32_t\s*\*)\s*\)', "*(uint16_t *)"),
            ]
        else:
            continue

        disp_int = int(disp, 16)
        disp_patterns = []
        if disp_int >= 0:
            disp_patterns.append("0x%x" % disp_int)
            disp_patterns.append("0x%X" % disp_int)
        else:
            disp_patterns.append("-0x%x" % (-disp_int))

        for dp in disp_patterns:
            for wide_re, replacement in wide_patterns:
                pattern = re.compile(
                    wide_re + r'.*' + re.escape(dp),
                    re.MULTILINE)
                if pattern.search(text):
                    candidates.append({
                        "description": "LOADW: widen at disp %s from %s -> %s" % (
                            disp, width_class, replacement),
                        "disp": disp,
                        "width_class": width_class,
                    })
                    break

    return candidates


def _gen_fcom_fixes(source_path, diag):
    """Generate candidate edits for FCOM bound-sense bugs.

    This is harder to automate mechanically because we need to find the right
    float comparison in the source and know what operator to use.  We report
    them as fixable candidates but the actual edit requires reading the guard
    shape mapping.
    """
    candidates = []
    guards = diag.get("guards", [])
    if not guards:
        return candidates

    op_map = {
        ("0x41", "jne"): "<=",
        ("0x1", "jne"): "<",
        ("0x5", "jp"): ">",
        ("0x41", "jp"): ">=",
        ("0x5", "jnp"): "<",
        ("0x1", "jnz"): "<",
        ("0x44", "jp"): "!=",
        ("0x44", "jnp"): "==",
    }

    for guard in guards:
        mask = guard.get("mask")
        jcc = guard.get("jcc")
        if mask and jcc:
            op = op_map.get((mask, jcc))
            if op:
                candidates.append({
                    "description": "FCOM: guard %s/%s suggests operator '%s'" % (
                        mask, jcc, op),
                    "suggested_op": op,
                    "mask": mask,
                    "jcc": jcc,
                })

    return candidates


def _gen_fpu_fixes(source_path, diag):
    """Identify potential FPU operand swaps (report only, no auto-edit)."""
    candidates = []
    fpu_warnings = diag.get("fpu_warnings", [])
    for w in fpu_warnings:
        if "fsub" in w.lower() or "fdiv" in w.lower():
            candidates.append({
                "description": "FPU: potential operand swap -- %s" % w.strip(),
                "kind": "operand_swap",
            })
    return candidates


# --- Gate ---

def _gate_edit(source, func_name, baseline, description):
    """Recompile and check if the edit improved the target score."""
    try:
        current = _measure(source)
    except Exception as e:
        return {"accepted": False, "reason": "measure failed: %s" % str(e)[:200]}

    report = {
        "accepted": False,
        "description": description,
        "function": func_name,
    }

    baseline_scores = baseline.get("scores", {})
    current_scores = current.get("scores", {})

    if func_name not in baseline_scores:
        report["reason"] = "function not in baseline"
        return report
    if func_name not in current_scores:
        report["reason"] = "function not in current measurement"
        return report

    before = baseline_scores[func_name]["score"]
    after = current_scores[func_name]["score"]
    delta = after - before

    report["score_before"] = before
    report["score_after"] = after
    report["delta"] = delta

    if delta < 0:
        report["reason"] = "score regressed by %.2f%%" % abs(delta)
        return report

    for name in baseline_scores:
        if name == func_name:
            continue
        if name not in current_scores:
            report["reason"] = "function %s disappeared" % name
            return report
        other_before = baseline_scores[name]["score"]
        other_after = current_scores[name]["score"]
        if other_after < other_before:
            report["reason"] = "%s regressed (%.2f -> %.2f)" % (
                name, other_before, other_after)
            return report

    before_warnings = 0
    after_warnings = 0
    for cat in ("fpu", "loadw", "imm", "fcom"):
        before_warnings += len(baseline_scores[func_name].get(
            "warnings", {}).get(cat, []))
        after_warnings += len(current_scores[func_name].get(
            "warnings", {}).get(cat, []))
    warnings_improved = after_warnings < before_warnings

    if delta < 0.005 and not warnings_improved:
        report["reason"] = "no measurable improvement (delta=%.4f%%)" % delta
        return report

    report["accepted"] = True
    if warnings_improved:
        report["reason"] = "warnings reduced (%d -> %d), score delta %.2f%%" % (
            before_warnings, after_warnings, delta)
    else:
        report["reason"] = "score improved by %.2f%%" % delta
    return report


def cmd_fix(source, rule_filter=None, func_filter=None, dry_run=False,
            refresh=False):
    """Apply mechanical fixes with per-edit VC71 gate."""
    source_path = (ROOT / source).resolve()
    if not source_path.is_file():
        return {"ok": False, "error": "source not found: %s" % source}

    if refresh:
        _refresh_packs(source)

    scan_result = cmd_scan(source)
    fixable = scan_result.get("fixable", [])

    if rule_filter:
        fixable = [f for f in fixable if f["rule"] == rule_filter]
    if func_filter:
        fixable = [f for f in fixable if f["function"] == func_filter]

    if not fixable:
        return {"ok": True, "source": source, "applied": [],
                "skipped": [], "exit_code": 2,
                "message": "nothing fixable"}

    backup = _backup_source(source_path)
    applied = []
    skipped = []
    baseline = None

    if not dry_run:
        try:
            baseline = _measure(source)
        except Exception as e:
            backup.unlink(missing_ok=True)
            return {"ok": False, "error": "baseline measure failed: %s" % str(e)[:300]}

    generators = {
        "imm_wrong_literal": _gen_imm_fixes,
        "loadw_field_width": _gen_loadw_fixes,
        "fcom_bound_sense": _gen_fcom_fixes,
        "fpu_operand_order": _gen_fpu_fixes,
    }

    for diag in fixable:
        rule = diag["rule"]
        func_name = diag["function"]
        gen = generators.get(rule)
        if not gen:
            skipped.append({
                "function": func_name, "rule": rule,
                "reason": "no generator"})
            continue

        candidates = gen(source_path, diag)
        if not candidates:
            skipped.append({
                "function": func_name, "rule": rule,
                "reason": "no candidate edits generated"})
            continue

        if dry_run:
            for c in candidates:
                applied.append({
                    "function": func_name, "rule": rule,
                    "description": c["description"],
                    "dry_run": True,
                })
            continue

        if rule == "imm_wrong_literal":
            for c in candidates:
                pre_edit = source_path.read_text()
                new_text = c.get("new_text")
                if not new_text or new_text == pre_edit:
                    skipped.append({
                        "function": func_name, "rule": rule,
                        "reason": "no text change for %s" % c["description"]})
                    continue

                source_path.write_text(new_text)
                gate = _gate_edit(source, func_name, baseline, c["description"])

                if gate["accepted"]:
                    applied.append({
                        "function": func_name, "rule": rule,
                        **gate,
                    })
                    baseline = _measure(source)
                else:
                    source_path.write_text(pre_edit)
                    skipped.append({
                        "function": func_name, "rule": rule,
                        "description": c["description"],
                        **gate,
                    })
        else:
            for c in candidates:
                skipped.append({
                    "function": func_name, "rule": rule,
                    "description": c["description"],
                    "reason": "auto-edit not yet implemented for %s" % rule,
                })

    result = {
        "ok": True,
        "source": source,
        "applied": applied,
        "skipped": skipped,
        "exit_code": 0 if applied else 2,
    }

    if not applied and not dry_run:
        _restore_source(source_path, backup)

    backup.unlink(missing_ok=True)
    return result


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def _emit(payload):
    json.dump(payload, sys.stdout, indent=1, sort_keys=True)
    sys.stdout.write("\n")


def main(argv=None):
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command")

    p_scan = sub.add_parser("scan", help="read-only census of fixable diagnostics")
    p_scan.add_argument("--source", required=True,
                        help="C source file relative to repo root")
    p_scan.add_argument("--refresh", action="store_true",
                        help="re-run vc71_verify before scanning")

    p_fix = sub.add_parser("fix", help="apply mechanical fixes with VC71 gate")
    p_fix.add_argument("--source", required=True,
                       help="C source file relative to repo root")
    p_fix.add_argument("--rule",
                       help="restrict to one rule (imm_wrong_literal, etc.)")
    p_fix.add_argument("--function",
                       help="restrict to one function name")
    p_fix.add_argument("--dry-run", action="store_true",
                       help="propose fixes without applying")
    p_fix.add_argument("--refresh", action="store_true",
                       help="re-run vc71_verify before fixing")

    args = parser.parse_args(argv)
    if not args.command:
        parser.print_help()
        return 1

    if args.command == "scan":
        result = cmd_scan(args.source, refresh=args.refresh)
        _emit(result)
        return 0

    if args.command == "fix":
        result = cmd_fix(
            args.source,
            rule_filter=args.rule,
            func_filter=args.function,
            dry_run=args.dry_run,
            refresh=args.refresh,
        )
        _emit(result)
        return result.get("exit_code", 0)

    return 1


if __name__ == "__main__":
    sys.exit(main())
