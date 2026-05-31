#!/usr/bin/env python3
"""Batch delink all objects defined in kb.json via Ghidra.

Uses ghidra-live RPC when available, falls back to headless analyzeHeadless.

Usage examples:
    # Dry run — print what would be extracted
    python3 tools/batch_delink.py --dry-run

    # Extract all objects (requires Ghidra running)
    python3 tools/batch_delink.py

    # Extract a single object
    python3 tools/batch_delink.py --object sound_manager.obj

    # Generate objects.csv only (no Ghidra needed)
    python3 tools/batch_delink.py --csv-only

    # Use headless Ghidra instead of ghidra-live RPC
    python3 tools/batch_delink.py --backend headless
"""

from __future__ import annotations

import argparse
import csv
import json
import os
import subprocess
import sys
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

_tools_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
KB_JSON = REPO_ROOT / "kb.json"
DELINKED_DIR = REPO_ROOT / "delinked"
MANIFEST_PATH = DELINKED_DIR / "manifest.json"
TOOLS_DIR = REPO_ROOT / "tools"

# Ghidra-live RPC endpoint
GHIDRA_LIVE_RPC_URL = "http://127.0.0.1:18081/rpc"
GHIDRA_LIVE_SSE_URL = "http://127.0.0.1:8091/sse"

# Headless defaults (inlined from deprecated export_delinked_object.py)
DEFAULT_GHIDRA_ROOT = "/mnt/c/Users/stian/AppData/Roaming/ghidra/ghidra_12.0.3_PUBLIC"
DEFAULT_PROJECT_DIR = "/mnt/c/Users/stian/AppData/Roaming/ghidra/ghidra_12.0.3_PUBLIC/projects"
DEFAULT_SCRIPT_DIR = "/mnt/c/Users/stian/ghidra_scripts"
DEFAULT_SCRIPT_NAME = "DelinkProgram.java"
DEFAULT_PROJECT_NAME = "cachebeta"
DEFAULT_PROGRAM = "cachebeta.xbe"
DEFAULT_EXPORTER = "Relocatable Object File (COFF)"

# How many bytes past the last known function start to pad the range end.
# This is a heuristic — the real size isn't in kb.json.
# Starting pad for the stabilization probe; doubled each round until size stops growing.
RANGE_END_PAD = 0x200
# Stop growing when two successive exports differ by less than this fraction.
_PAD_STABLE_THRESHOLD = 0.05
# Maximum pad doublings before we give up (0x200 → 0x400 → 0x800 → 0x1000 → 0x2000).
_PAD_MAX_DOUBLINGS = 4

# Pad for per-function exports of split TUs — generous enough to cover most functions.
RANGE_PAD_PER_FUNC = 0x2000


# ---------------------------------------------------------------------------
# kb.json loading
# ---------------------------------------------------------------------------

def load_objects() -> list[dict]:
    """Load all non-LIBCMT, non-<common> objects from kb.json via jq."""
    result = subprocess.run(
        [
            "jq",
            "-c",
            '[.objects[] | select(.name | type == "string")'
            ' | select(.name | startswith("LIBCMT") | not)'
            ' | select(.name != "<common>")]',
            str(KB_JSON),
        ],
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        print(f"error: jq failed: {result.stderr.strip()}", file=sys.stderr)
        raise SystemExit(1)
    return json.loads(result.stdout)


def compute_range(obj: dict) -> tuple[int, int] | None:
    """Return (min_addr, max_addr+pad) from the object's function addresses."""
    funcs = obj.get("functions") or []
    addrs = []
    for f in funcs:
        try:
            addrs.append(int(f["addr"], 16))
        except (KeyError, ValueError):
            pass
    if not addrs:
        return None
    return min(addrs), max(addrs) + RANGE_END_PAD


def stabilize_export_rpc(export_path: str, lo: int, max_addr: int) -> tuple[int, int]:
    """Export with progressively larger pads until the obj size stops growing.

    The last function in an object can be arbitrarily large; a fixed RANGE_END_PAD
    after its start address truncates it when it exceeds the pad.  We detect this
    by doubling the pad and re-exporting: if the new obj is more than
    _PAD_STABLE_THRESHOLD larger, the first export was cut short — keep growing.

    Returns (final_hi, final_obj_bytes).
    """
    pad = RANGE_END_PAD
    hi = max_addr + pad
    export_via_rpc(export_path, range_str(lo, hi))
    prev_size = Path(export_path).stat().st_size

    for _ in range(_PAD_MAX_DOUBLINGS):
        pad *= 2
        hi2 = max_addr + pad
        try:
            export_via_rpc(export_path, range_str(lo, hi2))
        except Exception:
            break
        new_size = Path(export_path).stat().st_size
        growth = (new_size - prev_size) / max(prev_size, 1)
        if growth < _PAD_STABLE_THRESHOLD:
            # Size stabilized — the larger export is fine; keep it.
            return hi2, new_size
        # Grew significantly; the previous export was truncated — keep probing.
        hi = hi2
        prev_size = new_size

    return hi, prev_size


def compute_truncated_range(obj: dict) -> tuple[int, int] | None:
    """Return range excluding the last function (workaround for BFT COFF
    relocation bug where the last function's relocations reference data
    past the exported buffer boundary)."""
    funcs = obj.get("functions") or []
    addrs = sorted(set(
        int(f["addr"], 16) for f in funcs
        if "addr" in f
    ))
    if len(addrs) < 2:
        return None
    return addrs[0], addrs[-1]


def find_max_exportable_range_rpc(
    export_path: str,
    obj: dict,
) -> tuple[int, int] | None:
    """Binary-search for the largest contiguous range that exports
    successfully.  Falls back to single-function export if needed."""
    funcs = obj.get("functions") or []
    addrs = sorted(set(
        int(f["addr"], 16) for f in funcs if "addr" in f
    ))
    if not addrs:
        return None

    lo = addrs[0]

    # Try progressively smaller ranges by removing functions from the end
    for end_idx in range(len(addrs) - 1, 0, -1):
        hi = addrs[end_idx]
        try:
            export_via_rpc(export_path, range_str(lo, hi))
            return lo, hi
        except Exception:
            continue

    # Last resort: try just the first function
    hi = addrs[0] + RANGE_END_PAD
    try:
        export_via_rpc(export_path, range_str(lo, hi))
        return lo, hi
    except Exception:
        return None


def range_str(lo: int, hi: int) -> str:
    """Format an address range for Ghidra: 'xxxxxxxx-yyyyyyyy' (no 0x prefix)."""
    return f"{lo:08x}-{hi:08x}"


# ---------------------------------------------------------------------------
# Ghidra-live RPC backend
# ---------------------------------------------------------------------------

def _rpc_call(method: str, params: dict | None = None) -> dict:
    form: dict[str, str] = {"method": method}
    if params:
        for k, v in params.items():
            if v is not None:
                form[k] = str(v)
    payload = urllib.parse.urlencode(form).encode("utf-8")
    req = urllib.request.Request(
        GHIDRA_LIVE_RPC_URL,
        data=payload,
        headers={"Content-Type": "application/x-www-form-urlencoded"},
        method="POST",
    )
    with urllib.request.urlopen(req, timeout=120) as resp:
        raw = resp.read().decode("utf-8")
    lines = raw.splitlines()
    if not lines:
        raise RuntimeError("empty RPC response")
    status = lines[0].strip()
    payload_kv: dict[str, str] = {}
    for line in lines[1:]:
        if not line or "=" not in line:
            continue
        k, v = line.split("=", 1)
        payload_kv[k] = v.replace("\\n", "\n").replace("\\\\", "\\")
    if status != "ok":
        code = payload_kv.get("code", "UNKNOWN")
        msg = payload_kv.get("message", "unknown error")
        raise RuntimeError(f"RPC error {code}: {msg}")
    return payload_kv


def is_ghidra_live_available() -> bool:
    try:
        req = urllib.request.Request(GHIDRA_LIVE_SSE_URL, method="GET")
        with urllib.request.urlopen(req, timeout=2) as resp:
            return resp.status == 200
    except Exception:
        return False


def _wsl_to_windows_path(path: str) -> str:
    """Convert WSL /mnt/<drive>/... to Windows <DRIVE>:\\... for Ghidra."""
    import re
    m = re.match(r"^/mnt/([a-zA-Z])/(.*)", path)
    if m:
        drive, rest = m.group(1).upper(), m.group(2)
        return f"{drive}:\\{rest.replace('/', '\\')}"
    return path


def export_via_rpc(export_path: str, addr_range: str) -> None:
    win_path = _wsl_to_windows_path(export_path)
    _rpc_call(
        "export_delinked_object",
        {
            "export_path": win_path,
            "exporter_name": "COFF relocatable object",
            "selection_mode": "range",
            "range": addr_range,
            "run_relocation_synthesizer": "true",
        },
    )


# ---------------------------------------------------------------------------
# Headless backend
# ---------------------------------------------------------------------------

def export_via_headless(
    export_path: str,
    addr_range: str,
    ghidra_root: str,
    project_dir: str,
    script_dir: str,
) -> None:
    output_path = os.path.abspath(export_path)
    os.makedirs(os.path.dirname(output_path), exist_ok=True)

    analyze_headless = os.path.join(ghidra_root, "support", "analyzeHeadless")
    analyze_headless_bat = os.path.join(ghidra_root, "support", "analyzeHeadless.bat")
    if os.name == "nt" and os.path.exists(analyze_headless_bat):
        launcher = analyze_headless_bat
    elif os.path.exists(analyze_headless):
        launcher = analyze_headless
    elif os.path.exists(analyze_headless_bat):
        launcher = analyze_headless_bat
    else:
        raise FileNotFoundError("analyzeHeadless not found in Ghidra support/")

    script_args = [
        "/exporter", DEFAULT_EXPORTER,
        "/include-range", addr_range,
        "/export", output_path,
    ]
    cmd = [
        launcher,
        project_dir,
        DEFAULT_PROJECT_NAME,
        "-process", DEFAULT_PROGRAM,
        "-scriptPath", script_dir,
        "-postScript", DEFAULT_SCRIPT_NAME,
        *script_args,
        "-noanalysis",
    ]

    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.stdout:
        print(result.stdout, end="")
    if result.stderr:
        print(result.stderr, end="", file=sys.stderr)
    if result.returncode != 0:
        raise RuntimeError(f"headless export failed (rc={result.returncode})")
    if not os.path.exists(output_path):
        raise RuntimeError(f"headless export did not produce output: {output_path}")


# ---------------------------------------------------------------------------
# CSV generation
# ---------------------------------------------------------------------------

def write_objects_csv(objects: list[dict], out_path: Path) -> None:
    """Write objects.csv mapping object names to address ranges."""
    with out_path.open("w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["Object", "Delink?", "addr_range", "func_count"])
        for obj in objects:
            rng = compute_range(obj)
            if rng:
                lo, hi = rng
                range_field = f"0x{lo:08X}-0x{hi:08X}"
            else:
                range_field = ""
            func_count = len(obj.get("functions") or [])
            delink = "true" if rng else "false"
            writer.writerow([obj["name"], delink, range_field, func_count])
    print(f"wrote {out_path}")


# ---------------------------------------------------------------------------
# Per-function export for split TUs
# ---------------------------------------------------------------------------

def export_split_functions(
    objects: list,
    out_dir: Path,
    manifest: dict,
    backend: str,
    verbose: bool,
    args,
) -> tuple:
    """Second pass: export each unported function of split TUs individually.

    Stores each function as delinked/functions/<hex8>.obj so vc71_verify can
    find a reference even when the parent TU is non-contiguous in the binary.
    """
    per_func_dir = out_dir / "functions"
    per_func_dir.mkdir(parents=True, exist_ok=True)
    success = failed = skipped = 0

    # Build addr->obj lookup from the full object list
    obj_by_name = {obj["name"]: obj for obj in objects}

    for name, entry in list(manifest.items()):
        if not entry.get("split"):
            continue
        obj = obj_by_name.get(name)
        if not obj:
            continue
        funcs = [f for f in (obj.get("functions") or []) if not f.get("ported") and f.get("addr")]
        if not funcs:
            continue

        print(f"\n  per-function pass for split TU: {name} ({len(funcs)} unported)")
        for fn in funcs:
            addr_int = int(fn["addr"], 16)
            addr_hex = f"{addr_int:08x}"
            export_path = per_func_dir / f"{addr_hex}.obj"
            manifest_key = f"functions/{addr_hex}"

            if export_path.exists():
                if verbose:
                    print(f"    skip  functions/{addr_hex}.obj  (exists)")
                skipped += 1
                manifest.setdefault(manifest_key, {
                    "obj_path": str(export_path),
                    "source": obj.get("source"),
                    "addr": f"0x{addr_int:08x}",
                    "parent_obj": name,
                })
                continue

            addr_range = range_str(addr_int, addr_int + RANGE_PAD_PER_FUNC)
            try:
                if backend == "rpc":
                    export_via_rpc(str(export_path), addr_range)
                else:
                    export_via_headless(
                        str(export_path), addr_range,
                        args.ghidra_root, args.project_dir, args.script_dir,
                    )
                decl = fn.get("decl", "").split("(")[0].strip() if fn.get("decl") else addr_hex
                print(f"    ok  functions/{addr_hex}.obj  ({decl})")
                manifest[manifest_key] = {
                    "obj_path": str(export_path),
                    "source": obj.get("source"),
                    "addr": f"0x{addr_int:08x}",
                    "parent_obj": name,
                }
                success += 1
            except Exception as exc:
                print(f"    FAIL  functions/{addr_hex}.obj: {exc}", file=sys.stderr)
                failed += 1

    return success, skipped, failed


# ---------------------------------------------------------------------------
# Main logic
# ---------------------------------------------------------------------------

def parse_args() -> argparse.Namespace:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dry-run", action="store_true",
                    help="Print what would be extracted without running Ghidra")
    ap.add_argument("--object", metavar="NAME",
                    help="Extract only this object (e.g. sound_manager.obj)")
    ap.add_argument("--output-dir", default=str(DELINKED_DIR),
                    help=f"Output directory (default: {DELINKED_DIR})")
    ap.add_argument("--backend", choices=["auto", "rpc", "headless"], default="auto",
                    help="Ghidra backend: auto selects rpc if available (default: auto)")
    ap.add_argument("--csv-only", action="store_true",
                    help="Only generate tools/objects.csv, do not run Ghidra")
    ap.add_argument("--no-manifest", action="store_false", dest="write_manifest",
                    help="Skip writing manifest.json")
    ap.add_argument("--ghidra-root", default=DEFAULT_GHIDRA_ROOT)
    ap.add_argument("--project-dir", default=DEFAULT_PROJECT_DIR)
    ap.add_argument("--script-dir", default=DEFAULT_SCRIPT_DIR)
    ap.add_argument("--verbose", action="store_true")
    ap.add_argument("--no-per-function", action="store_true",
                    help="Skip per-function export pass for split TUs")
    ap.add_argument("--per-function-only", action="store_true",
                    help="Only run per-function pass using existing manifest (no main export)")
    return ap.parse_args()


def main() -> int:
    args = parse_args()
    out_dir = Path(args.output_dir)

    # Always generate objects.csv
    objects = load_objects()
    csv_path = TOOLS_DIR / "objects.csv"
    write_objects_csv(objects, csv_path)

    if args.csv_only:
        return 0

    # --per-function-only: skip main export, use existing manifest, run second pass only
    if getattr(args, "per_function_only", False):
        if not MANIFEST_PATH.exists():
            print(f"error: manifest not found at {MANIFEST_PATH}; run without --per-function-only first", file=sys.stderr)
            return 1
        manifest = json.load(MANIFEST_PATH.open())
        backend = "rpc" if is_ghidra_live_available() else "headless"
        out_dir.mkdir(parents=True, exist_ok=True)
        pf_ok, pf_skip, pf_fail = export_split_functions(
            objects, out_dir, manifest, backend, args.verbose, args
        )
        print(f"\nper-function pass: {pf_ok} exported, {pf_skip} skipped, {pf_fail} failed")
        if args.write_manifest:
            with MANIFEST_PATH.open("w") as f:
                json.dump(manifest, f, indent=2)
            print(f"manifest: {MANIFEST_PATH}")
        return 1 if pf_fail else 0

    # Filter to a single object if requested
    if args.object:
        matches = [o for o in objects if o["name"] == args.object]
        if not matches:
            available = [o["name"] for o in objects]
            print(f"error: object '{args.object}' not found in kb.json", file=sys.stderr)
            print(f"available: {available[:10]}...", file=sys.stderr)
            return 1
        objects = matches

    # Preflight: determine backend
    if args.dry_run:
        backend = "dry-run"
    elif args.backend == "rpc" or (args.backend == "auto" and is_ghidra_live_available()):
        backend = "rpc"
    elif args.backend == "headless" or args.backend == "auto":
        backend = "headless"
    else:
        backend = "headless"

    if not args.dry_run:
        if backend == "rpc":
            print(f"backend: ghidra-live RPC ({GHIDRA_LIVE_RPC_URL})")
        else:
            print(f"backend: headless analyzeHeadless ({args.ghidra_root})")
        out_dir.mkdir(parents=True, exist_ok=True)

    # Process objects
    manifest: dict[str, dict] = {}
    success = 0
    skipped = 0
    failed = 0

    for obj in objects:
        name: str = obj["name"]
        rng = compute_range(obj)
        if not rng:
            print(f"  skip  {name}  (no function addresses)")
            skipped += 1
            continue

        lo, hi = rng
        addr_range = range_str(lo, hi)
        # Output path mirrors object name structure, replacing : with /
        safe_name = name.replace(":", "/")
        export_path = out_dir / safe_name
        export_path.parent.mkdir(parents=True, exist_ok=True)

        manifest[name] = {
            "obj_path": str(export_path),
            "source": obj.get("source"),
            "addr_range": f"0x{lo:08x}-0x{hi:08x}",
            "func_count": len(obj.get("functions") or []),
        }

        if args.dry_run:
            print(f"  would export  {name}  range={addr_range}  -> {export_path}")
            continue

        if args.verbose:
            print(f"  exporting {name}  range={addr_range}")

        try:
            if backend == "rpc":
                # Stabilization probe: double the pad until obj size stops growing,
                # catching truncated last functions (e.g. RANGE_END_PAD < function size).
                lo_addr = lo
                max_func_addr = hi - RANGE_END_PAD  # recover max_addr before pad
                final_hi, obj_bytes = stabilize_export_rpc(
                    str(export_path), lo_addr, max_func_addr
                )
                if final_hi != hi:
                    manifest[name]["addr_range"] = f"0x{lo_addr:08x}-0x{final_hi:08x}"
                    pad_used = final_hi - max_func_addr
                    print(f"  ok  {name}  -> {export_path.name}  ({obj_bytes}B)  [pad grown to 0x{pad_used:x}]")
                else:
                    print(f"  ok  {name}  -> {export_path.name}  ({obj_bytes}B)")
            else:
                export_via_headless(
                    str(export_path),
                    addr_range,
                    args.ghidra_root,
                    args.project_dir,
                    args.script_dir,
                )
                obj_bytes = export_path.stat().st_size
                print(f"  ok  {name}  -> {export_path.name}  ({obj_bytes}B)")
            success += 1
        except Exception as exc:
            trunc = compute_truncated_range(obj)
            if trunc and backend == "rpc":
                tlo, thi = trunc
                trunc_range = range_str(tlo, thi)
                print(f"  retry {name}  truncated range={trunc_range}")
                try:
                    export_via_rpc(str(export_path), trunc_range)
                    func_count = len(obj.get("functions") or [])
                    print(f"  ok  {name}  -> {export_path.name}  (truncated: last func excluded, {func_count - 1}/{func_count})")
                    manifest[name]["addr_range"] = f"0x{tlo:08x}-0x{thi:08x}"
                    manifest[name]["truncated"] = True
                    success += 1
                    continue
                except Exception as retry_exc:
                    print(f"  retry also failed: {retry_exc}", file=sys.stderr)
                    # Progressive split: find the largest exportable subset
                    print(f"  split {name}  searching for max exportable range...")
                    result = find_max_exportable_range_rpc(str(export_path), obj)
                    if result:
                        slo, shi = result
                        func_count = len(obj.get("functions") or [])
                        covered = sum(1 for a in [int(f["addr"], 16) for f in (obj.get("functions") or [])] if slo <= a < shi)
                        print(f"  ok  {name}  -> {export_path.name}  (split: {covered}/{func_count} functions)")
                        manifest[name]["addr_range"] = f"0x{slo:08x}-0x{shi:08x}"
                        manifest[name]["truncated"] = True
                        manifest[name]["split"] = True
                        success += 1
                        continue
            print(f"  FAIL  {name}: {exc}", file=sys.stderr)
            manifest[name]["error"] = str(exc)
            failed += 1

    # Second pass: per-function exports for split TUs
    if not args.dry_run and not args.no_per_function and backend == "rpc":
        pf_ok, pf_skip, pf_fail = export_split_functions(
            objects, out_dir, manifest, backend, args.verbose, args
        )
        if pf_ok or pf_fail:
            print(f"\nper-function pass: {pf_ok} exported, {pf_skip} skipped, {pf_fail} failed")

    # Write manifest
    if args.write_manifest and not args.dry_run:
        MANIFEST_PATH.parent.mkdir(parents=True, exist_ok=True)
        with MANIFEST_PATH.open("w") as f:
            json.dump(manifest, f, indent=2)
        print(f"\nmanifest: {MANIFEST_PATH}")
    elif args.dry_run and manifest:
        # Write manifest in dry-run too (no Ghidra needed, just metadata)
        out_dir.mkdir(parents=True, exist_ok=True)
        with MANIFEST_PATH.open("w") as f:
            json.dump(manifest, f, indent=2)
        print(f"\nmanifest (dry-run): {MANIFEST_PATH}")

    if not args.dry_run:
        total = success + skipped + failed
        print(f"\ndone: {success} exported, {skipped} skipped, {failed} failed / {total} total")
        if failed:
            return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
