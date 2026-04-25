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

REPO_ROOT = Path(__file__).resolve().parent.parent
KB_JSON = REPO_ROOT / "kb.json"
DELINKED_DIR = REPO_ROOT / "delinked"
MANIFEST_PATH = DELINKED_DIR / "manifest.json"
TOOLS_DIR = REPO_ROOT / "tools"

# Ghidra-live RPC endpoint
GHIDRA_LIVE_RPC_URL = "http://127.0.0.1:18080/rpc"
GHIDRA_LIVE_SSE_URL = "http://127.0.0.1:8091/sse"

# Headless defaults (mirrors export_delinked_object.py)
DEFAULT_GHIDRA_ROOT = "/mnt/c/Users/stian/AppData/Roaming/ghidra/ghidra_12.0.3_PUBLIC"
DEFAULT_PROJECT_DIR = "/mnt/c/Users/stian/AppData/Roaming/ghidra/ghidra_12.0.3_PUBLIC/projects"
DEFAULT_SCRIPT_DIR = "/mnt/c/Users/stian/ghidra_scripts"
DEFAULT_SCRIPT_NAME = "DelinkProgram.java"
DEFAULT_PROGRAM = "cachebeta.xbe"
DEFAULT_EXPORTER = "Relocatable Object File (COFF)"

# How many bytes past the last known function start to pad the range end.
# This is a heuristic — the real size isn't in kb.json.
RANGE_END_PAD = 0x200


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


def export_via_rpc(export_path: str, addr_range: str) -> None:
    _rpc_call(
        "export_delinked_object",
        {
            "export_path": export_path,
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
    script = str(TOOLS_DIR / "export_delinked_object.py")
    cmd = [
        sys.executable,
        script,
        "--ghidra-root", ghidra_root,
        "--project-dir", project_dir,
        "--script-dir", script_dir,
        "--exporter", DEFAULT_EXPORTER,
        "--output", export_path,
        "--range", addr_range,
    ]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.stdout:
        print(result.stdout, end="")
    if result.stderr:
        print(result.stderr, end="", file=sys.stderr)
    if result.returncode != 0:
        raise RuntimeError(f"headless export failed (rc={result.returncode})")


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
                export_via_rpc(str(export_path), addr_range)
            else:
                export_via_headless(
                    str(export_path),
                    addr_range,
                    args.ghidra_root,
                    args.project_dir,
                    args.script_dir,
                )
            print(f"  ok  {name}  -> {export_path.name}")
            success += 1
        except Exception as exc:
            print(f"  FAIL  {name}: {exc}", file=sys.stderr)
            manifest[name]["error"] = str(exc)
            failed += 1

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
