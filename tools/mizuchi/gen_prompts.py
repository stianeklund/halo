#!/usr/bin/env python3
"""
Convert auto_lift context cache entries into mizuchi prompt folders.

Each cache entry becomes:
  artifacts/mizuchi/prompts/<FuncName>/
    prompt.md       — Ghidra decompile + similar ported functions
    settings.yaml   — functionName, targetObjectPath, asm (required by mizuchi)

Usage:
    python3 tools/mizuchi/gen_prompts.py [--target FUN_000425c0|0x425c0] [--force]

Without --target, processes all cache entries that don't yet have a prompt folder.
"""

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

try:
    import yaml
except ImportError:
    print("ERROR: pyyaml not installed — run: pip install pyyaml", file=sys.stderr)
    sys.exit(1)

ROOT = Path(__file__).resolve().parents[2]
CACHE_DIR = ROOT / "artifacts/auto_lift/context_cache"
PROMPTS_DIR = ROOT / "artifacts/mizuchi/prompts"
DELINKED_DIR = ROOT / "delinked"
KB_PATH = ROOT / "kb.json"


def load_kb_index():
    """Return {addr_hex: {name, obj_name, decl}} from kb.json, addr_hex like '0x425c0'."""
    kb = json.loads(KB_PATH.read_text())
    index = {}
    for obj in kb.get("objects", []):
        obj_name = obj["name"]
        for fn in obj.get("functions", []):
            addr = fn.get("addr")
            decl = fn.get("decl", "")
            name_m = re.search(r'\b(\w+)\s*\(', decl)
            name = name_m.group(1) if name_m else None
            if addr:
                index[addr] = {"name": name, "obj_name": obj_name, "decl": decl}
    return index


def func_name_to_addr(func_name: str) -> str:
    """'FUN_000425c0' -> '0x425c0'"""
    m = re.match(r'FUN_([0-9a-fA-F]+)$', func_name)
    if not m:
        return None
    return hex(int(m.group(1), 16))


def extract_asm(delinked_path: Path, func_name: str) -> str:
    """Extract GAS disassembly for func_name from a delinked COFF object via objdump."""
    result = subprocess.run(
        ["objdump", "-d", "--no-show-raw-insn", str(delinked_path)],
        capture_output=True, text=True, check=True,
    )
    lines = result.stdout.splitlines()
    in_func = False
    asm_lines = []
    header_re = re.compile(r'^[0-9a-f]+ <(.+?)>:')

    for line in lines:
        m = header_re.match(line)
        if m:
            if in_func:
                break
            if m.group(1) == func_name:
                in_func = True
                asm_lines.append(line)
        elif in_func:
            asm_lines.append(line)

    if not asm_lines:
        raise RuntimeError(f"{func_name!r} not found in {delinked_path.name}")

    return "\n".join(asm_lines)


def parse_json_field(value) -> str:
    """Cache fields are sometimes JSON-encoded strings with a 'result' key."""
    if isinstance(value, str):
        try:
            parsed = json.loads(value)
            if isinstance(parsed, dict) and "result" in parsed:
                return parsed["result"]
        except (json.JSONDecodeError, TypeError):
            pass
    return str(value) if value else ""


def build_prompt_md(func_name: str, decl: str, cache: dict) -> str:
    parts = [f"# {func_name}\n"]

    if decl:
        parts.append(f"**Declaration:** `{decl}`\n")

    decompile = parse_json_field(cache.get("decompile_c", "")).strip()
    if decompile:
        parts.append("## Ghidra Decompilation\n")
        parts.append(f"```c\n{decompile}\n```\n")

    callees = cache.get("callees") or []
    if callees:
        parts.append("## Callees\n")
        for c in callees:
            parts.append(f"- `{c}`")
        parts.append("")

    neighbors = cache.get("similar_neighbors") or []
    if neighbors:
        parts.append("## Similar Ported Functions\n")
        for nb in neighbors[:3]:
            sim = nb.get("similarity", 0)
            parts.append(f"### {nb['name']} ({nb['obj_name']}, similarity={sim:.2f})\n")
            c_src = (nb.get("c_source") or "").strip()
            if c_src:
                parts.append(f"```c\n{c_src}\n```\n")

    return "\n".join(parts)


def gen_prompt(cache_path: Path, kb_index: dict, force: bool) -> bool:
    func_name = cache_path.stem  # e.g. FUN_000425c0
    addr = func_name_to_addr(func_name)

    kb_entry = kb_index.get(addr) if addr else None
    decl = kb_entry["decl"] if kb_entry else ""
    obj_name = kb_entry["obj_name"] if kb_entry else None

    if not obj_name:
        print(f"  SKIP {func_name}: not in kb.json")
        return False

    delinked_path = DELINKED_DIR / obj_name
    if not delinked_path.exists():
        print(f"  SKIP {func_name}: delinked/{obj_name} not found")
        return False

    out_dir = PROMPTS_DIR / func_name
    if out_dir.exists() and not force:
        print(f"  SKIP {func_name}: prompt folder exists (use --force to overwrite)")
        return False

    try:
        asm = extract_asm(delinked_path, func_name)
    except RuntimeError as e:
        print(f"  SKIP {func_name}: {e}")
        return False

    cache = json.loads(cache_path.read_text())
    prompt_md = build_prompt_md(func_name, decl, cache)

    settings = {
        "functionName": func_name,
        "targetObjectPath": str(delinked_path),
        "asm": asm,
    }

    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "prompt.md").write_text(prompt_md)
    (out_dir / "settings.yaml").write_text(yaml.dump(settings, default_flow_style=False, allow_unicode=True))

    print(f"  OK  {func_name} -> {out_dir.relative_to(ROOT)}")
    return True


def resolve_target(target: str) -> Path:
    """Accept address (0x425c0) or name (FUN_000425c0), return cache path."""
    # normalise address to padded name
    if re.match(r'^0x[0-9a-fA-F]+$', target):
        padded = f"{int(target, 16):08x}"
        name = f"FUN_{padded}"
    else:
        name = target
    path = CACHE_DIR / f"{name}.json"
    if not path.exists():
        print(f"ERROR: cache entry not found: {path}", file=sys.stderr)
        sys.exit(1)
    return path


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--target", help="Address (0x425c0) or name (FUN_000425c0) to process")
    parser.add_argument("--force", action="store_true", help="Overwrite existing prompt folders")
    args = parser.parse_args()

    kb_index = load_kb_index()
    PROMPTS_DIR.mkdir(parents=True, exist_ok=True)

    if args.target:
        cache_paths = [resolve_target(args.target)]
    else:
        cache_paths = sorted(CACHE_DIR.glob("*.json"))
        if not cache_paths:
            print(f"No cache entries in {CACHE_DIR}")
            sys.exit(0)

    ok = skip = 0
    for p in cache_paths:
        if gen_prompt(p, kb_index, args.force):
            ok += 1
        else:
            skip += 1

    print(f"\nDone: {ok} generated, {skip} skipped.")


if __name__ == "__main__":
    main()
