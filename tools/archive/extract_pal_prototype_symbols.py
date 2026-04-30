#!/usr/bin/env python3
"""Extract a reusable symbol/name corpus from a Halo prototype build.

This script is intentionally robust to very old PDB formats (e.g., MSF/PDB v2)
that modern llvm-pdbutil cannot parse. In that case, it falls back to a
strings-based symbol corpus so we can still cross-check candidate names.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

_tools_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)


IDENT_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
DECORATED_RE = re.compile(r"^[@_?A-Za-z][A-Za-z0-9_?$@]*$")


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        while True:
            b = f.read(1024 * 1024)
            if not b:
                break
            h.update(b)
    return h.hexdigest()


def run(cmd: list[str]) -> tuple[int, str, str]:
    p = subprocess.run(cmd, capture_output=True, text=True)
    return p.returncode, p.stdout, p.stderr


def extract_with_llvm_pdbutil(pdb: Path) -> list[str]:
    rc, out, err = run(["llvm-pdbutil", "dump", "-publics", "-symbols", str(pdb)])
    if rc != 0:
        raise RuntimeError((err or out).strip() or "llvm-pdbutil failed")
    names: set[str] = set()
    for line in out.splitlines():
        line = line.strip()
        if "`" in line and line.endswith("`"):
            i = line.find("`")
            j = line.rfind("`")
            if i >= 0 and j > i:
                candidate = line[i + 1 : j].strip()
                if candidate:
                    names.add(candidate)
    return sorted(names)


def extract_strings_candidates(path: Path) -> list[str]:
    rc, out, _ = run(["strings", "-n", "4", str(path)])
    if rc != 0:
        return []
    names: set[str] = set()
    for raw in out.splitlines():
        s = raw.strip()
        if len(s) < 3 or len(s) > 220:
            continue
        if " " in s or "\\" in s or "/" in s:
            continue
        if ":" in s and not s.startswith("?"):
            continue
        if not DECORATED_RE.match(s):
            continue
        if s.count("@") > 8:
            continue
        names.add(s)
    return sorted(names)


def classify_name(name: str) -> str:
    if name.startswith("?"):
        return "msvc_mangled"
    if name.startswith("@") and name.count("@") >= 2:
        return "stdcall_decorated"
    if name.startswith("_") and name.rsplit("@", 1)[-1].isdigit() and "@" in name:
        return "stdcall_underscore"
    if IDENT_RE.match(name):
        return "identifier"
    return "other"


def main() -> int:
    ap = argparse.ArgumentParser(description="Extract PAL prototype symbol corpus")
    ap.add_argument("--build-dir", required=True, help="Prototype build directory")
    ap.add_argument("--out-dir", required=True, help="Output directory for extracted corpus")
    args = ap.parse_args()

    build_dir = Path(args.build_dir)
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    pdb = build_dir / "cachebeta.pdb"
    exe = build_dir / "cachebeta.exe"
    if not pdb.exists():
        raise FileNotFoundError(f"Missing PDB: {pdb}")
    if not exe.exists():
        raise FileNotFoundError(f"Missing EXE: {exe}")

    manifest: dict[str, object] = {
        "build_dir": str(build_dir),
        "files": {
            "cachebeta.pdb": {"sha256": sha256(pdb), "size": pdb.stat().st_size},
            "cachebeta.exe": {"sha256": sha256(exe), "size": exe.stat().st_size},
        },
        "tools": {
            "llvm-pdbutil": shutil.which("llvm-pdbutil"),
            "llvm-readobj": shutil.which("llvm-readobj"),
            "strings": shutil.which("strings"),
        },
    }

    debug_dir_text = ""
    if shutil.which("llvm-readobj"):
        _, out, err = run(["llvm-readobj", "--coff-debug-directory", str(exe)])
        debug_dir_text = out or err
    (out_dir / "exe_debug_directory.txt").write_text(debug_dir_text, encoding="utf-8")

    extracted: list[dict[str, str]] = []
    mode = "unknown"
    failure = None
    if shutil.which("llvm-pdbutil"):
        try:
            llvm_names = extract_with_llvm_pdbutil(pdb)
            mode = "llvm-pdbutil"
            for n in llvm_names:
                extracted.append({"name": n, "source": "pdb", "confidence": "high", "kind": classify_name(n)})
        except Exception as exc:  # noqa: BLE001
            failure = str(exc)

    if not extracted:
        mode = "strings-fallback"
        pdb_names = extract_strings_candidates(pdb)
        exe_names = extract_strings_candidates(exe)
        for n in pdb_names:
            extracted.append({"name": n, "source": "pdb_strings", "confidence": "medium", "kind": classify_name(n)})
        for n in exe_names:
            extracted.append({"name": n, "source": "exe_strings", "confidence": "low", "kind": classify_name(n)})

    dedup: dict[str, dict[str, str]] = {}
    rank = {"high": 3, "medium": 2, "low": 1}
    for row in extracted:
        key = row["name"]
        prev = dedup.get(key)
        if prev is None or rank[row["confidence"]] > rank[prev["confidence"]]:
            dedup[key] = row

    symbol_rows = sorted(dedup.values(), key=lambda r: (r["kind"], r["name"]))
    summary: dict[str, int] = {}
    for r in symbol_rows:
        summary[r["kind"]] = summary.get(r["kind"], 0) + 1

    manifest["extraction_mode"] = mode
    manifest["llvm_failure"] = failure
    manifest["symbol_count"] = len(symbol_rows)
    manifest["kind_summary"] = summary

    (out_dir / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    (out_dir / "symbol_names.json").write_text(json.dumps(symbol_rows, indent=2), encoding="utf-8")
    (out_dir / "symbol_names.txt").write_text("\n".join(r["name"] for r in symbol_rows) + "\n", encoding="utf-8")

    print(f"[+] mode={mode}")
    print(f"[+] symbols={len(symbol_rows)}")
    if failure:
        print(f"[i] llvm-pdbutil failure: {failure}")
    print(f"[+] wrote {out_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
