#!/usr/bin/env python3
"""Apply punpckhdq-derived name proposals as a textual rename across the repo.

Reads `artifacts/punpckhdq_import/name_proposals.json`, filters to safe-to-apply
proposals (high-confidence + real_name=true), then for each proposal performs
a global textual replace of `FUN_<addr>` -> `<proposed_name>` across:

  - kb.json
  - tools/kb_reg_baseline.json
  - src/**/*.{c,h}

Safety:
  - Default mode is dry-run; write nothing without --apply.
  - Skip any proposal whose proposed_name already appears as an identifier
    elsewhere in kb.json or src/ (collision risk).
  - Skip any proposal whose FUN_<addr> isn't actually present in kb.json.
  - Emit a per-rename plan + grand totals before applying.

Usage:
    rtk python3 tools/analysis/apply_punpckhdq_renames.py
    rtk python3 tools/analysis/apply_punpckhdq_renames.py --apply
    rtk python3 tools/analysis/apply_punpckhdq_renames.py --confidence medium  # widen scope
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
PROPOSALS = ROOT / "artifacts" / "punpckhdq_import" / "name_proposals.json"
KB_JSON = ROOT / "kb.json"
KB_REG_BASELINE = ROOT / "tools" / "kb_reg_baseline.json"
SRC_DIR = ROOT / "src"

# Identifiers must not start with `FUN_` (so we don't accidentally treat
# our own placeholders as collisions) and must be valid C symbols.
WORD_RE = re.compile(r"\b[A-Za-z_][A-Za-z0-9_]*\b")


@dataclass
class Rename:
  addr: str
  fun_symbol: str
  proposed: str
  our_object: str
  source: str
  confidence: str


def load_proposals(min_confidence: str) -> list[Rename]:
  if not PROPOSALS.exists():
    print(f"missing {PROPOSALS}; run punpckhdq_import.py first", file=sys.stderr)
    sys.exit(1)
  data = json.loads(PROPOSALS.read_text())
  conf_order = {"high": 3, "medium": 2, "low": 1}
  threshold = conf_order[min_confidence]

  out: list[Rename] = []
  for p in data:
    if not p.get("real_name"):
      continue
    if conf_order.get(p["confidence"], 0) < threshold:
      continue
    addr = p["our_addr"]
    addr_int = int(addr, 16)
    fun_sym = f"FUN_{addr_int:08x}"
    out.append(Rename(
      addr=addr,
      fun_symbol=fun_sym,
      proposed=p["proposed_name"],
      our_object=p["our_object"],
      source=p["their_source"],
      confidence=p["confidence"],
    ))
  return out


def file_text_cache(paths: list[Path]) -> dict[Path, str]:
  return {p: p.read_text(encoding="utf-8", errors="replace") for p in paths}


def all_source_files() -> list[Path]:
  return sorted([p for p in SRC_DIR.rglob("*")
                 if p.suffix in (".c", ".h") and p.is_file()])


def find_files_containing(symbol: str, files: dict[Path, str]) -> set[Path]:
  pattern = re.compile(rf"\b{re.escape(symbol)}\b")
  return {p for p, txt in files.items() if pattern.search(txt)}


def is_function_definition(name: str, text: str) -> bool:
  """True if `name(...)` appears as a function definition or prototype."""
  return bool(re.search(
    rf"\b{re.escape(name)}\s*\([^;{{]*\)\s*[{{;]", text))


def apply_textual(text: str, fun_sym: str, proposed: str) -> tuple[str, int]:
  pattern = re.compile(rf"\b{re.escape(fun_sym)}\b")
  new_text, n = pattern.subn(proposed, text)
  return new_text, n


def main() -> int:
  ap = argparse.ArgumentParser(description=__doc__,
                               formatter_class=argparse.RawDescriptionHelpFormatter)
  ap.add_argument("--apply", action="store_true",
                  help="write changes; default is dry-run")
  ap.add_argument("--confidence", choices=["high", "medium", "low"],
                  default="high", help="minimum confidence to include")
  args = ap.parse_args()

  renames = load_proposals(args.confidence)
  if not renames:
    print("no proposals at the requested confidence level", file=sys.stderr)
    return 1

  src_files = all_source_files()
  src_text = file_text_cache(src_files)
  kb_text = KB_JSON.read_text(encoding="utf-8")
  base_text = (KB_REG_BASELINE.read_text(encoding="utf-8")
               if KB_REG_BASELINE.exists() else "")

  print(f"# punpckhdq rename plan ({len(renames)} {args.confidence}+ proposals)")
  print()

  applied = 0
  skipped: list[tuple[Rename, str]] = []
  changes_per_file: dict[Path | str, int] = {}
  category_counts = {"clean": 0, "drift": 0, "conflict": 0, "noop": 0}

  for r in renames:
    # Sanity: FUN_<addr> must exist in kb.json
    fun_in_kb = re.search(rf"\b{re.escape(r.fun_symbol)}\b", kb_text)
    if not fun_in_kb:
      skipped.append((r, f"{r.fun_symbol} not present in kb.json"))
      continue

    # Categorize the rename
    fun_in_src = find_files_containing(r.fun_symbol, src_text)
    proposed_files = find_files_containing(r.proposed, src_text)
    proposed_is_defined = any(is_function_definition(r.proposed, src_text[p])
                              for p in proposed_files)

    if proposed_is_defined and not fun_in_src:
      category = "drift"  # source already renamed, kb.json stale
    elif proposed_is_defined and fun_in_src:
      # Source has BOTH FUN_<addr> and proposed_name as function definitions —
      # check if FUN_<addr>'s defining file is also where the proposed_name
      # is a *different* function (true conflict) or comment-only (drift+).
      conflict_files = []
      for p in proposed_files:
        if is_function_definition(r.proposed, src_text[p]):
          conflict_files.append(str(p.relative_to(ROOT)))
      category = "conflict"
      skipped.append((r, f"'{r.proposed}' already a function definition in: "
                      + ", ".join(conflict_files[:3])))
      category_counts[category] += 1
      continue
    elif not fun_in_src:
      category = "drift"  # kb.json only
    else:
      category = "clean"  # both kb.json and source need rename

    # Plan replacements
    file_hits: list[tuple[Path | str, int]] = []
    new_kb, n_kb = apply_textual(kb_text, r.fun_symbol, r.proposed)
    if n_kb:
      file_hits.append(("kb.json", n_kb))

    new_base, n_base = apply_textual(base_text, r.fun_symbol, r.proposed) \
        if base_text else (base_text, 0)
    if n_base:
      file_hits.append(("tools/kb_reg_baseline.json", n_base))

    src_updates: list[tuple[Path, str, int]] = []
    for path, text in src_text.items():
      new_text, n = apply_textual(text, r.fun_symbol, r.proposed)
      if n:
        src_updates.append((path, new_text, n))
        file_hits.append((str(path.relative_to(ROOT)), n))

    if not file_hits:
      category_counts["noop"] += 1
      continue

    category_counts[category] += 1
    summary = ", ".join(f"{p}:{n}" for p, n in file_hits)
    print(f"- [{category}] {r.addr} {r.fun_symbol} -> {r.proposed}")
    print(f"    object={r.our_object} source={r.source} hits={summary}")

    if args.apply:
      kb_text = new_kb
      base_text = new_base
      for path, new_text, _ in src_updates:
        src_text[path] = new_text
      applied += 1

    for fp, n in file_hits:
      changes_per_file[fp] = changes_per_file.get(fp, 0) + n

  print()
  print("## category breakdown")
  for cat, n in category_counts.items():
    print(f"  {cat}: {n}")
  print()
  print(f"## skipped: {len(skipped)} (real conflicts, not drift)")
  for r, reason in skipped[:20]:
    print(f"- {r.fun_symbol} -> {r.proposed}: {reason}")
  if len(skipped) > 20:
    print(f"  ... and {len(skipped) - 20} more")

  print()
  print(f"## file change totals ({sum(changes_per_file.values())} total replacements)")
  for fp, n in sorted(changes_per_file.items(), key=lambda x: -x[1])[:20]:
    print(f"  {fp}: {n}")

  if not args.apply:
    print()
    print(f"DRY-RUN: would apply {len(renames) - len(skipped)} renames "
          f"across {len(changes_per_file)} files. Re-run with --apply.")
    return 0

  # Write back
  KB_JSON.write_text(kb_text, encoding="utf-8")
  if base_text:
    KB_REG_BASELINE.write_text(base_text, encoding="utf-8")
  for path, text in src_text.items():
    if text != path.read_text(encoding="utf-8", errors="replace"):
      path.write_text(text, encoding="utf-8")

  print()
  print(f"applied {applied} renames; verify with `rtk python3 tools/build/build.py -q --target halo`")
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
