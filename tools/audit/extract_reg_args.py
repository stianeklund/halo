#!/usr/bin/env python3
"""Extract register-argument annotations from kb.json and manage kb_reg_baseline.json.

Modes:
  Single function (by name or address):
      extract_reg_args.py --target <name|addr>
      Prints the kb_reg_baseline.json fragment for that function.

  Batch (all reg-arg functions in a kb.json object):
      extract_reg_args.py --batch <object_name.obj>
      Emits a complete fragment for all reg-arg functions in that object.

  Check (drift detection):
      extract_reg_args.py --check [--target <name|addr>] [--batch <obj>]
      Compares kb.json declarations against kb_reg_baseline.json entries and
      reports any drift (signature differs, entry missing, or entry present
      but function no longer has @<reg> args).

  Apply (merge new entries):
      extract_reg_args.py --apply [--target <name|addr>] [--batch <obj>]
      Merges newly extracted entries into kb_reg_baseline.json. Never
      overwrites existing entries (they are immutable).

  Self-test:
      extract_reg_args.py --self-test
      Picks 5 functions with @<reg> annotations from both kb.json and
      kb_reg_baseline.json, extracts them, and prints a comparison table.

Ghidra is used optionally to cross-check register parameter storage when
the --ghidra flag is passed. Without it, kb.json is the sole source of truth.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from pathlib import Path
from typing import Optional

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
_this_dir = Path(__file__).resolve().parent
_tools_dir = _this_dir.parent
_root = _tools_dir.parent

KB_PATH = _root / "kb.json"
BASELINE_PATH = _tools_dir / "kb_reg_baseline.json"
GHIDRA_PREFLIGHT = _tools_dir / "audit" / "check_ghidra_mcp.py"

# ---------------------------------------------------------------------------
# Regex helpers (shared with knowledge.py / audit_reg_abi.py)
# ---------------------------------------------------------------------------
_REG_ANNOTATION_RE = re.compile(r"@<(\w+)>")
_COMMENTED_REG_RE = re.compile(r"/\*\s*@<(\w+)>\s*\*/")

REG_PARENT: dict[str, str] = {
    "eax": "eax", "ax": "eax", "al": "eax", "ah": "eax",
    "ecx": "ecx", "cx": "ecx", "cl": "ecx", "ch": "ecx",
    "edx": "edx", "dx": "edx", "dl": "edx", "dh": "edx",
    "ebx": "ebx", "bx": "ebx", "bl": "ebx", "bh": "ebx",
    "esi": "esi", "si": "esi",
    "edi": "edi", "di": "edi",
    "ebp": "ebp", "bp": "ebp",
}


def canon32(reg: str) -> str:
    return REG_PARENT.get(reg.lower(), reg.lower())


# ---------------------------------------------------------------------------
# kb.json loading (jq-free pure-Python parse — we already ban Read on kb.json
# but the script itself needs data; we load once via a single json.load call)
# ---------------------------------------------------------------------------
_kb_cache: Optional[dict] = None


def _load_kb() -> dict:
    global _kb_cache
    if _kb_cache is None:
        with open(KB_PATH, "r", encoding="utf-8") as f:
            _kb_cache = json.load(f)
    return _kb_cache


def _load_baseline() -> dict:
    if not BASELINE_PATH.exists():
        return {"functions": {}}
    with open(BASELINE_PATH, "r", encoding="utf-8") as f:
        return json.load(f)


def _save_baseline(data: dict) -> None:
    with open(BASELINE_PATH, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)
        f.write("\n")


# ---------------------------------------------------------------------------
# Annotation parsing
# ---------------------------------------------------------------------------

def _parse_reg_annotations(decl: str) -> list[tuple[int, str]]:
    """Return list of (param_index, reg_name) for active @<reg> annotations.

    Commented-out annotations like /* @<eax> */ are skipped — they are
    documented-but-inactive hints and must not generate baseline entries.
    """
    # Strip comments first so /* @<eax> */ is invisible to the live scanner
    stripped = re.sub(r"/\*.*?\*/", "", decl)

    open_paren = stripped.find("(")
    close_paren = stripped.rfind(")")
    if open_paren < 0 or close_paren < 0:
        return []

    params_src = stripped[open_paren + 1:close_paren]
    depth = 0
    buf: list[str] = []
    params: list[str] = []
    for ch in params_src:
        if ch in ("(", "<"):
            depth += 1
            buf.append(ch)
        elif ch in (")", ">"):
            depth -= 1
            buf.append(ch)
        elif ch == "," and depth == 0:
            params.append("".join(buf))
            buf = []
        else:
            buf.append(ch)
    if buf:
        params.append("".join(buf))

    result: list[tuple[int, str]] = []
    for i, p in enumerate(params):
        m = _REG_ANNOTATION_RE.search(p)
        if m is not None:
            result.append((i, m.group(1).lower()))
    return result


def _has_active_reg_args(decl: str) -> bool:
    return bool(_parse_reg_annotations(decl))


# ---------------------------------------------------------------------------
# Normalize address to canonical 0x<lower-hex> form (no leading zeros beyond "0x")
# ---------------------------------------------------------------------------

def _norm_addr(raw: str | int) -> str:
    if isinstance(raw, str):
        try:
            val = int(raw, 16)
        except ValueError:
            val = int(raw, 0)
    else:
        val = int(raw)
    return hex(val)


# ---------------------------------------------------------------------------
# KB function lookup
# ---------------------------------------------------------------------------

class FuncEntry:
    __slots__ = ("addr", "decl", "obj_name", "reg_args")

    def __init__(self, addr: str, decl: str, obj_name: str) -> None:
        self.addr = _norm_addr(addr)
        self.decl = decl
        self.obj_name = obj_name
        self.reg_args: list[tuple[int, str]] = _parse_reg_annotations(decl)

    @property
    def name(self) -> Optional[str]:
        # Extract function name: last identifier before '('
        cleaned = re.sub(r"@<\w+>", "", self.decl)
        cleaned = re.sub(r"/\*.*?\*/", "", cleaned)
        m = re.search(r"(\w+)\s*\(", cleaned)
        return m.group(1) if m else None

    def baseline_decl(self) -> str:
        """Return the declaration as it should appear in kb_reg_baseline.json."""
        return self.decl.strip().rstrip(";") + ";"


def _iter_objects(kb: dict):
    """Yield object dicts from kb.json (handles both list and dict layouts)."""
    objs = kb.get("objects", [])
    if isinstance(objs, dict):
        yield from objs.values()
    else:
        yield from objs


def _all_functions(kb: dict) -> list[FuncEntry]:
    """Flatten all function entries from kb.json into FuncEntry list."""
    result: list[FuncEntry] = []
    for obj in _iter_objects(kb):
        if not isinstance(obj, dict):
            continue
        obj_name = obj.get("name", "")
        for fn in obj.get("functions", []) or []:
            if not isinstance(fn, dict):
                continue
            decl = fn.get("decl", "")
            addr = fn.get("addr", "")
            if not decl or not addr:
                continue
            result.append(FuncEntry(addr, decl, obj_name))
    return result


def _lookup_by_target(kb: dict, target: str) -> FuncEntry:
    """Find a function by name or address (flexible matching)."""
    target_l = target.lower().strip()
    try:
        target_addr = _norm_addr(target_l)
    except (ValueError, OverflowError):
        target_addr = ""

    for fn in _all_functions(kb):
        if target_addr and fn.addr == target_addr:
            return fn
        if fn.name and fn.name.lower() == target_l:
            return fn

    raise KeyError(f"Function not found in kb.json: {target!r}")


def _lookup_by_object(kb: dict, obj_name: str) -> list[FuncEntry]:
    """Return all functions in an object, filtered to those with active @<reg> args."""
    obj_name_l = obj_name.lower()
    result: list[FuncEntry] = []
    for obj in _iter_objects(kb):
        if not isinstance(obj, dict):
            continue
        name = obj.get("name") or ""
        if (name.lower() != obj_name_l
                and name.lower().removesuffix(".obj") != obj_name_l.removesuffix(".obj")):
            continue
        for fn in obj.get("functions", []) or []:
            if not isinstance(fn, dict):
                continue
            decl = fn.get("decl", "")
            addr = fn.get("addr", "")
            if not decl or not addr:
                continue
            entry = FuncEntry(addr, decl, name)
            if entry.reg_args:
                result.append(entry)
    return result


# ---------------------------------------------------------------------------
# Ghidra cross-check (optional)
# ---------------------------------------------------------------------------

def _ghidra_preflight() -> bool:
    """Run check_ghidra_mcp.py preflight. Return True if Ghidra is available."""
    result = subprocess.run(
        [sys.executable, str(GHIDRA_PREFLIGHT)],
        cwd=str(_root),
        capture_output=True,
        text=True,
    )
    return result.returncode == 0


def _ghidra_get_variables(addr: str) -> Optional[dict]:
    """Call mcp__ghidra__get_function_variables via the Ghidra MCP bridge.

    Since we are a subprocess (not an MCP-aware agent), we use the JSON-RPC
    bridge script if available, otherwise skip Ghidra cross-check gracefully.
    """
    # The MCP tools are only available inside the agent. From a plain Python
    # subprocess we can't call them directly. Instead we use check_ghidra_mcp.py
    # which validates connectivity, and then delegate the actual call to the
    # ghidra_bridge CLI wrapper if present.
    bridge = _root / "tools" / "shell" / "ghidra_bridge.py"
    if not bridge.exists():
        return None
    result = subprocess.run(
        [sys.executable, str(bridge), "get_function_variables", "--address", addr],
        cwd=str(_root),
        capture_output=True,
        text=True,
        timeout=30,
    )
    if result.returncode != 0:
        return None
    try:
        return json.loads(result.stdout)
    except json.JSONDecodeError:
        return None


def _ghidra_reg_params(addr: str) -> list[tuple[str, str]]:
    """Return list of (param_name, reg) as reported by Ghidra, or empty list."""
    data = _ghidra_get_variables(addr)
    if not data:
        return []
    params = data.get("parameters", [])
    result: list[tuple[str, str]] = []
    for p in params:
        storage = p.get("storage", "")
        # Ghidra storage format: "EAX:4" or "Register[EAX]:4"
        m = re.search(r"\b([A-Za-z]{2,3}):(\d+)", storage)
        if not m:
            # Try "Register[EAX]:4" form
            m = re.search(r"Register\[([A-Za-z]{2,3})\]", storage, re.I)
        if m:
            reg = m.group(1).lower()
            if reg in REG_PARENT or reg in {"si", "di", "bx", "cx"}:
                result.append((p.get("name", "?"), reg))
    return result


# ---------------------------------------------------------------------------
# Fragment generation
# ---------------------------------------------------------------------------

def _make_fragment(entries: list[FuncEntry]) -> dict[str, str]:
    """Build a kb_reg_baseline.json fragment (addr -> decl string)."""
    return {fn.addr: fn.baseline_decl() for fn in entries if fn.reg_args}


# ---------------------------------------------------------------------------
# Modes
# ---------------------------------------------------------------------------

def mode_single(kb: dict, target: str, use_ghidra: bool, json_out: bool) -> int:
    try:
        fn = _lookup_by_target(kb, target)
    except KeyError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1

    if not fn.reg_args:
        if not json_out:
            print(f"{fn.name} ({fn.addr}): no active @<reg> parameters — no baseline entry needed")
        else:
            print(json.dumps({}))
        return 0

    fragment = _make_fragment([fn])

    ghidra_regs: list[tuple[str, str]] = []
    if use_ghidra:
        ghidra_regs = _ghidra_reg_params(fn.addr)

    if json_out:
        out: dict = {"fragment": fragment}
        if use_ghidra:
            out["ghidra_params"] = [{"name": n, "reg": r} for n, r in ghidra_regs]
        print(json.dumps(out, indent=2))
    else:
        print(f"Function : {fn.name}")
        print(f"Address  : {fn.addr}")
        print(f"Object   : {fn.obj_name}")
        print(f"Reg args : {', '.join(f'arg{i}@<{r}>' for i, r in fn.reg_args)}")
        print()
        print("kb_reg_baseline.json fragment:")
        print(json.dumps(fragment, indent=2))
        if use_ghidra:
            if ghidra_regs:
                print(f"\nGhidra register params: {ghidra_regs}")
            else:
                print("\nGhidra: no register params found (may lack custom CC metadata)")

    return 0


def mode_batch(kb: dict, obj_name: str, use_ghidra: bool, json_out: bool) -> int:
    entries = _lookup_by_object(kb, obj_name)
    if not entries:
        if not json_out:
            print(f"No functions with active @<reg> args found in object: {obj_name}")
        else:
            print(json.dumps({}))
        return 0

    fragment = _make_fragment(entries)

    if json_out:
        print(json.dumps({"fragment": fragment}, indent=2))
    else:
        print(f"Object   : {obj_name}")
        print(f"Functions: {len(entries)} with @<reg> args")
        print()
        print("kb_reg_baseline.json fragment:")
        print(json.dumps(fragment, indent=2))

    return 0


def mode_check(kb: dict, target: Optional[str], obj_name: Optional[str], json_out: bool) -> int:
    """Compare kb.json declarations against kb_reg_baseline.json and report drift."""
    baseline = _load_baseline()
    baseline_fns = baseline.get("functions", {})

    # Build candidate set from kb.json
    if target:
        try:
            candidates = [_lookup_by_target(kb, target)]
        except KeyError as exc:
            print(f"ERROR: {exc}", file=sys.stderr)
            return 1
    elif obj_name:
        candidates = _lookup_by_object(kb, obj_name)
        # Also include all kb.json reg-arg functions if no filter
    else:
        candidates = [fn for fn in _all_functions(kb) if fn.reg_args]

    drifts: list[dict] = []
    missing: list[dict] = []
    ok_count = 0

    for fn in candidates:
        addr = fn.addr
        expected_decl = fn.baseline_decl()

        if addr not in baseline_fns:
            if fn.reg_args:
                missing.append({"addr": addr, "name": fn.name, "decl": expected_decl})
            continue

        actual_decl = baseline_fns[addr]
        if actual_decl.strip() != expected_decl.strip():
            drifts.append({
                "addr": addr,
                "name": fn.name,
                "kb_decl": expected_decl,
                "baseline_decl": actual_decl,
            })
        else:
            ok_count += 1

    # Check for entries in baseline that no longer exist in kb.json with @<reg>
    kb_reg_addrs = {fn.addr for fn in _all_functions(kb) if fn.reg_args}
    stale: list[dict] = []
    if not target and not obj_name:
        for addr, decl in baseline_fns.items():
            if addr not in kb_reg_addrs:
                stale.append({"addr": addr, "decl": decl})

    rc = 0
    if drifts or missing or stale:
        rc = 1

    if json_out:
        print(json.dumps({
            "ok": ok_count,
            "drift": drifts,
            "missing_from_baseline": missing,
            "stale_in_baseline": stale,
        }, indent=2))
    else:
        print(f"Check results: {ok_count} OK, {len(drifts)} drift, "
              f"{len(missing)} missing, {len(stale)} stale")
        if drifts:
            print("\n--- DRIFT (kb.json vs baseline) ---")
            for d in drifts:
                print(f"  {d['addr']} ({d['name']}):")
                print(f"    kb.json  : {d['kb_decl']}")
                print(f"    baseline : {d['baseline_decl']}")
        if missing:
            print("\n--- MISSING FROM BASELINE ---")
            for m in missing:
                print(f"  {m['addr']} ({m['name']}): {m['decl']}")
        if stale:
            print("\n--- STALE IN BASELINE (no longer in kb.json with @<reg>) ---")
            for s in stale:
                print(f"  {s['addr']}: {s['decl']}")
        if rc == 0:
            print("No drift detected.")

    return rc


def mode_apply(kb: dict, target: Optional[str], obj_name: Optional[str], dry_run: bool) -> int:
    """Merge new entries into kb_reg_baseline.json without overwriting existing ones."""
    baseline = _load_baseline()
    existing = baseline.setdefault("functions", {})

    if target:
        try:
            candidates = [_lookup_by_target(kb, target)]
        except KeyError as exc:
            print(f"ERROR: {exc}", file=sys.stderr)
            return 1
    elif obj_name:
        candidates = _lookup_by_object(kb, obj_name)
    else:
        candidates = [fn for fn in _all_functions(kb) if fn.reg_args]

    added: list[str] = []
    skipped: list[str] = []

    for fn in candidates:
        if not fn.reg_args:
            continue
        addr = fn.addr
        decl = fn.baseline_decl()
        if addr in existing:
            skipped.append(addr)
        else:
            if not dry_run:
                existing[addr] = decl
            added.append(addr)

    if not dry_run and added:
        # Sort the functions dict by address for deterministic output
        baseline["functions"] = dict(
            sorted(existing.items(), key=lambda kv: int(kv[0], 16))
        )
        _save_baseline(baseline)

    print(f"Apply {'(dry-run) ' if dry_run else ''}results: "
          f"{len(added)} added, {len(skipped)} skipped (immutable)")
    if added:
        print("Added:")
        for addr in added:
            print(f"  {addr}")
    if skipped and dry_run:
        print("Would skip (already present):")
        for addr in skipped[:10]:
            print(f"  {addr}")
        if len(skipped) > 10:
            print(f"  ... and {len(skipped) - 10} more")
    return 0


# ---------------------------------------------------------------------------
# Self-test: verify 5 known @<reg> functions
# ---------------------------------------------------------------------------

_SELF_TEST_ADDRS = [
    "0x1190c0",   # crc_table_init — uint32_t *table@<edx>
    "0x13ded0",   # object_header_new — int type_hint@<eax>
    "0x1a9560",   # unit_impulse_to_animation_kind — int16_t impulse_index@<ax>, int16_t *out_update_kind@<ebx>
    "0x1af180",   # unit_apply_alignment_vector — int unit_handle@<eax>, float *alignment_vector@<ecx>
    "0x1b9bf0",   # tag_instance_resolve — int tag_index@<edi>
]


def mode_self_test(kb: dict) -> int:
    baseline = _load_baseline()
    baseline_fns = baseline.get("functions", {})

    col_w = [12, 30, 55, 55, 10]
    header = (
        f"{'Address':<{col_w[0]}} "
        f"{'Name':<{col_w[1]}} "
        f"{'Extracted decl':<{col_w[2]}} "
        f"{'Baseline decl':<{col_w[3]}} "
        f"{'Match':<{col_w[4]}}"
    )
    sep = "-" * (sum(col_w) + len(col_w))
    print(header)
    print(sep)

    all_pass = True
    for addr in _SELF_TEST_ADDRS:
        try:
            fn = _lookup_by_target(kb, addr)
        except KeyError:
            print(f"{addr:<{col_w[0]}} {'(not found in kb.json)'}")
            all_pass = False
            continue

        extracted = fn.baseline_decl()
        baseline_val = baseline_fns.get(fn.addr, "(not in baseline)")
        match = "OK" if extracted.strip() == baseline_val.strip() else "DRIFT"
        if match == "DRIFT":
            all_pass = False

        # Truncate for display
        def trunc(s: str, n: int) -> str:
            return s if len(s) <= n else s[:n - 3] + "..."

        print(
            f"{fn.addr:<{col_w[0]}} "
            f"{(fn.name or '?'):<{col_w[1]}} "
            f"{trunc(extracted, col_w[2]):<{col_w[2]}} "
            f"{trunc(baseline_val, col_w[3]):<{col_w[3]}} "
            f"{match:<{col_w[4]}}"
        )

    print()
    if all_pass:
        print("Self-test PASSED: all 5 functions match baseline.")
        return 0
    else:
        print("Self-test FAILED: one or more functions have drift or were not found.")
        return 1


# ---------------------------------------------------------------------------
# Entrypoint
# ---------------------------------------------------------------------------

def main() -> int:
    parser = argparse.ArgumentParser(
        description="Extract @<reg> annotations and manage kb_reg_baseline.json",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )

    mode_group = parser.add_mutually_exclusive_group()
    mode_group.add_argument(
        "--check",
        action="store_true",
        help="Compare kb.json vs baseline and report drift",
    )
    mode_group.add_argument(
        "--apply",
        action="store_true",
        help="Merge new entries into baseline (never overwrites existing)",
    )
    mode_group.add_argument(
        "--self-test",
        action="store_true",
        dest="self_test",
        help="Run built-in self-test against 5 known @<reg> functions",
    )

    parser.add_argument(
        "--target",
        metavar="NAME_OR_ADDR",
        help="Function name or hex address (single-function mode)",
    )
    parser.add_argument(
        "--batch",
        metavar="OBJECT_NAME",
        help="Object name (e.g. sound_manager.obj) for batch mode",
    )
    parser.add_argument(
        "--ghidra",
        action="store_true",
        help="Cross-check against Ghidra register parameter storage (requires MCP)",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        dest="dry_run",
        help="With --apply: show what would be added without writing",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        dest="json_out",
        help="Machine-readable JSON output",
    )

    args = parser.parse_args()

    # Ghidra preflight if requested
    if args.ghidra:
        print("Running Ghidra MCP preflight...", file=sys.stderr)
        if not _ghidra_preflight():
            print(
                "ERROR: Ghidra MCP preflight failed.\n"
                "You might have forgotten to start tools/shell/mcp-servers.sh "
                "or ghidra may not be running?",
                file=sys.stderr,
            )
            return 1
        print("Ghidra MCP preflight OK.", file=sys.stderr)

    kb = _load_kb()

    # Mandatory preflight for modes that need Ghidra (even without --ghidra flag,
    # MCP is not called from subprocess; this is informational only).

    if args.self_test:
        return mode_self_test(kb)

    if args.check:
        return mode_check(kb, args.target, args.batch, args.json_out)

    if args.apply:
        return mode_apply(kb, args.target, args.batch, args.dry_run)

    # Default: extract / display mode
    if args.target:
        return mode_single(kb, args.target, args.ghidra, args.json_out)
    elif args.batch:
        return mode_batch(kb, args.batch, args.ghidra, args.json_out)
    else:
        parser.print_help()
        return 0


if __name__ == "__main__":
    raise SystemExit(main())
