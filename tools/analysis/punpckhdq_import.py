#!/usr/bin/env python3
"""Import punpckhdq/halo PDB-derived data and propose name/TU mappings.

The punpckhdq/halo decomp targets the August 2001 cachebeta debug build of
Halo CE Xbox. Its config/ holds PDB-derived data:
  - config.json: 11 projects (halobetacache + Xbox libs), each with named TUs
  - contribs.json: section contributions (TU byte ranges)
  - symbols.json: public symbol names with file_offset
  - splits.json: top-level section splits

Our retail build differs (different SHA256, different addresses), but the
function *names* and TU *groupings* are stable across debug/retail. We
project their ordering onto our objects to:
  - Propose real names for our FUN_XXXX placeholders
  - Surface TU split mistakes (their data has 468 game TUs vs our ~199)
  - Surface source-path info (e.g. our actors.obj -> source/ai/actors.c)

Outputs:
  artifacts/punpckhdq_import/report.md
  artifacts/punpckhdq_import/name_proposals.json

Usage:
    rtk python3 tools/analysis/punpckhdq_import.py             # use cached data
    rtk python3 tools/analysis/punpckhdq_import.py --refresh   # re-download
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
import urllib.request
from collections import defaultdict
from dataclasses import asdict, dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
DATA_DIR = ROOT / "artifacts" / "punpckhdq_import"
KB_JSON = ROOT / "kb.json"
REPORT_PATH = DATA_DIR / "report.md"
PROPOSALS_PATH = DATA_DIR / "name_proposals.json"

UPSTREAM = "https://raw.githubusercontent.com/punpckhdq/halo/master/config"
FILES = ("config.json", "contribs.json", "symbols.json", "splits.json")

GAME_PROJECT = "halobetacache"
SIZE_TOLERANCE = 0.30
HIGH_CONF_SIZE_TOL = 0.05


# ---------------------------------------------------------------------------
# Data
# ---------------------------------------------------------------------------

@dataclass
class TheirSymbol:
  name: str
  file_offset: int
  flags: int


@dataclass
class TheirTU:
  module_index: int
  project: str
  source_path: str
  status: str
  contribs: list[dict] = field(default_factory=list)
  symbols: list[TheirSymbol] = field(default_factory=list)

  @property
  def stem(self) -> str:
    base = os.path.basename(self.source_path)
    return os.path.splitext(base)[0].lower()

  @property
  def total_size(self) -> int:
    return sum(c["size"] for c in self.contribs)

  def function_symbols(self) -> list[TheirSymbol]:
    """Symbols that look like functions (in code-flagged contribs)."""
    code_ranges = []
    for c in self.contribs:
      if _is_code_flag(c["flags"]):
        code_ranges.append((c["file_offset"], c["file_offset"] + c["size"]))
    out = []
    for s in self.symbols:
      for lo, hi in code_ranges:
        if lo <= s.file_offset < hi:
          out.append(s)
          break
    out.sort(key=lambda x: x.file_offset)
    return out


@dataclass
class OurFunc:
  addr: int
  name: str
  decl: str
  is_placeholder: bool


@dataclass
class OurObject:
  name: str
  funcs: list[OurFunc] = field(default_factory=list)

  @property
  def stem(self) -> str:
    n = self.name or ""
    if ":" in n:
      n = n.split(":", 1)[1]
    return os.path.splitext(n)[0].lower()


@dataclass
class Proposal:
  our_addr: str
  our_name: str
  proposed_name: str
  our_object: str
  their_source: str
  ordinal: int
  size_their: int
  size_ours: int
  size_match: bool
  confidence: str  # high / medium / low
  real_name: bool = True  # False for their-side `code_XXXX` placeholders


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _is_code_flag(flags: int) -> bool:
  # IMAGE_SCN_CNT_CODE = 0x00000020
  return bool(flags & 0x20)


_THEIR_PLACEHOLDER_RE = re.compile(r"^code_[0-9a-f]+$", re.IGNORECASE)


def _normalize_their_name(name: str) -> str:
  """Strip leading underscore and stdcall @N suffix from MSVC public name."""
  if not name:
    return name
  if name.startswith("?"):
    return name  # C++ mangled, leave alone
  n = name
  if n.startswith("_"):
    n = n[1:]
  m = re.match(r"^(.*?)@\d+$", n)
  if m:
    n = m.group(1)
  return n


def _is_real_name(name: str) -> bool:
  """A real game-function name (not their own placeholder or C++ mangled)."""
  if not name or name.startswith("?"):
    return False
  return not _THEIR_PLACEHOLDER_RE.match(name)


_FUN_RE = re.compile(r"\bFUN_[0-9a-fA-F]{6,}\b")
_DECL_NAME_RE = re.compile(r"([A-Za-z_][A-Za-z0-9_]*)\s*\(")


def _extract_decl_name(decl: str) -> str:
  """Get the function identifier from a C-style decl string."""
  if not decl:
    return ""
  # Strip register annotations like @<ecx>
  cleaned = re.sub(r"@<\w+>", "", decl)
  # Match the rightmost identifier preceding '('
  matches = _DECL_NAME_RE.findall(cleaned)
  if not matches:
    return ""
  # Walk backward to skip type tokens
  for m in reversed(matches):
    if m not in {"void", "int", "char", "short", "long", "float", "double",
                 "unsigned", "signed", "size_t", "bool", "wchar_t",
                 "__cdecl", "__stdcall", "__fastcall", "__thiscall",
                 "const", "volatile", "static", "extern"}:
      return m
  return matches[-1]


def fetch_data(refresh: bool) -> None:
  DATA_DIR.mkdir(parents=True, exist_ok=True)
  for fname in FILES:
    dest = DATA_DIR / fname
    if dest.exists() and not refresh:
      continue
    url = f"{UPSTREAM}/{fname}"
    print(f"fetching {url}", file=sys.stderr)
    with urllib.request.urlopen(url) as r:
      dest.write_bytes(r.read())


def load_their_data() -> dict[int, TheirTU]:
  config = json.loads((DATA_DIR / "config.json").read_text())
  contribs = json.loads((DATA_DIR / "contribs.json").read_text())
  symbols_raw = json.loads((DATA_DIR / "symbols.json").read_text())

  tus: dict[int, TheirTU] = {}
  for proj in config["projects"]:
    for obj in proj["objects"]:
      tus[obj["index"]] = TheirTU(
        module_index=obj["index"],
        project=proj["name"],
        source_path=obj["name"],
        status=obj.get("status", ""),
      )

  for c in contribs:
    mi = c.get("module_index")
    if mi in tus:
      tus[mi].contribs.append(c)

  # Build offset -> TU lookup for symbol assignment
  offset_to_tu: list[tuple[int, int, int]] = []  # (lo, hi, module_index)
  for tu in tus.values():
    for c in tu.contribs:
      offset_to_tu.append((c["file_offset"], c["file_offset"] + c["size"], tu.module_index))
  offset_to_tu.sort()

  for s in symbols_raw:
    fo = s["file_offset"]
    # Binary-search the TU range containing this offset
    lo, hi = 0, len(offset_to_tu)
    while lo < hi:
      mid = (lo + hi) // 2
      a, b, mi = offset_to_tu[mid]
      if fo < a:
        hi = mid
      elif fo >= b:
        lo = mid + 1
      else:
        tus[mi].symbols.append(TheirSymbol(
          name=s["name"], file_offset=fo, flags=s["flags"]))
        break

  return tus


def load_our_kb() -> list[OurObject]:
  kb = json.loads(KB_JSON.read_text())
  out: list[OurObject] = []
  for obj in kb.get("objects", []):
    o = OurObject(name=obj.get("name") or "<unnamed>")
    for f in obj.get("functions", []):
      addr_s = f.get("addr") or ""
      try:
        addr = int(addr_s, 16) if addr_s else 0
      except ValueError:
        addr = 0
      decl = f.get("decl") or ""
      name = _extract_decl_name(decl)
      is_placeholder = bool(_FUN_RE.search(decl))
      o.funcs.append(OurFunc(addr=addr, name=name, decl=decl,
                             is_placeholder=is_placeholder))
    o.funcs.sort(key=lambda x: x.addr)
    out.append(o)
  return out


def estimate_our_func_sizes(obj: OurObject) -> list[int]:
  """Inter-address delta as a noisy size estimate (last entry uses 0)."""
  sizes = []
  for i, f in enumerate(obj.funcs):
    if i + 1 < len(obj.funcs):
      sizes.append(max(0, obj.funcs[i + 1].addr - f.addr))
    else:
      sizes.append(0)
  return sizes


def their_func_sizes(tu: TheirTU) -> list[int]:
  syms = tu.function_symbols()
  out = []
  for i, s in enumerate(syms):
    if i + 1 < len(syms):
      out.append(max(0, syms[i + 1].file_offset - s.file_offset))
    else:
      out.append(0)
  return out


# ---------------------------------------------------------------------------
# Needleman-Wunsch size-based alignment
# ---------------------------------------------------------------------------

def _size_match_score(a: int, b: int) -> float:
  """Match score for two function sizes. +2 / +0.5 / -1 by ratio band."""
  if not a or not b:
    return -0.5  # one side missing — neutral-bad
  ratio = abs(a - b) / max(a, b)
  if ratio <= HIGH_CONF_SIZE_TOL:
    return 2.0
  if ratio <= SIZE_TOLERANCE:
    return 0.5
  return -1.0


def align_by_size(their_sizes: list[int], our_sizes: list[int],
                  gap_penalty: float = -0.3) -> list[tuple[int | None, int | None]]:
  """Needleman-Wunsch align two size lists. Returns list of (i, j) pairs.

  i = index into theirs (or None for our-only insertion);
  j = index into ours (or None for their-only insertion).
  """
  n, m = len(their_sizes), len(our_sizes)
  f = [[0.0] * (m + 1) for _ in range(n + 1)]
  for i in range(n + 1):
    f[i][0] = i * gap_penalty
  for j in range(m + 1):
    f[0][j] = j * gap_penalty
  for i in range(1, n + 1):
    for j in range(1, m + 1):
      match = _size_match_score(their_sizes[i - 1], our_sizes[j - 1])
      f[i][j] = max(
        f[i - 1][j - 1] + match,
        f[i - 1][j] + gap_penalty,
        f[i][j - 1] + gap_penalty,
      )
  # Traceback
  pairs: list[tuple[int | None, int | None]] = []
  i, j = n, m
  while i > 0 or j > 0:
    if i > 0 and j > 0:
      match = _size_match_score(their_sizes[i - 1], our_sizes[j - 1])
      if abs(f[i][j] - (f[i - 1][j - 1] + match)) < 1e-9:
        pairs.append((i - 1, j - 1))
        i -= 1
        j -= 1
        continue
    if i > 0 and (j == 0 or abs(f[i][j] - (f[i - 1][j] + gap_penalty)) < 1e-9):
      pairs.append((i - 1, None))
      i -= 1
    else:
      pairs.append((None, j - 1))
      j -= 1
  pairs.reverse()
  return pairs


# ---------------------------------------------------------------------------
# Matching
# ---------------------------------------------------------------------------

def match_tus(theirs: dict[int, TheirTU],
              ours: list[OurObject]
              ) -> tuple[list[tuple[OurObject, TheirTU]],
                         list[tuple[OurObject, list[TheirTU]]],
                         list[OurObject],
                         list[TheirTU]]:
  """Match TUs by stem (basename without extension).

  Returns:
    one_to_one: deterministic stem -> single TU matches
    one_to_many: stem maps to multiple TUs (likely TU split missed in our kb)
    unmatched_ours: our objects with no stem match
    unmatched_theirs: their TUs with no stem match (informational)
  """
  game_tus = [tu for tu in theirs.values()
              if tu.project == GAME_PROJECT and tu.contribs]

  by_stem: dict[str, list[TheirTU]] = defaultdict(list)
  for tu in game_tus:
    by_stem[tu.stem].append(tu)

  one_to_one: list[tuple[OurObject, TheirTU]] = []
  one_to_many: list[tuple[OurObject, list[TheirTU]]] = []
  unmatched_ours: list[OurObject] = []
  matched_their_indices: set[int] = set()

  for our in ours:
    if not our.name or our.name.startswith("<") or our.name.startswith("LIBCMT"):
      continue
    if not our.funcs:
      continue
    stem = our.stem
    cands = by_stem.get(stem, [])
    if len(cands) == 1:
      one_to_one.append((our, cands[0]))
      matched_their_indices.add(cands[0].module_index)
    elif len(cands) > 1:
      one_to_many.append((our, cands))
      for c in cands:
        matched_their_indices.add(c.module_index)
    else:
      unmatched_ours.append(our)

  unmatched_theirs = [tu for tu in game_tus
                      if tu.module_index not in matched_their_indices]
  return one_to_one, one_to_many, unmatched_ours, unmatched_theirs


def propose_names(our: OurObject,
                  tu: TheirTU,
                  use_nw: bool = True
                  ) -> tuple[list[Proposal], list[str], list[str]]:
  """Align our function list with theirs (Needleman-Wunsch by size, with
  ordinal fallback) and propose names.

  Returns:
    proposals: list of name proposals (FUN_ -> real)
    name_conflicts: where our function already has a different real name
    notes: misc notes (count mismatch, size divergence, etc.)
  """
  proposals: list[Proposal] = []
  conflicts: list[str] = []
  notes: list[str] = []

  their_funcs = tu.function_symbols()
  our_sizes = estimate_our_func_sizes(our)
  their_sizes = their_func_sizes(tu)

  if len(our.funcs) != len(their_funcs):
    notes.append(f"function count differs: ours={len(our.funcs)}, theirs={len(their_funcs)}")

  if use_nw:
    pairs = align_by_size(their_sizes, our_sizes)
  else:
    n_min = min(len(our.funcs), len(their_funcs))
    pairs = [(i, i) for i in range(n_min)]

  for ti, oj in pairs:
    if ti is None or oj is None:
      continue  # gap; one side has no counterpart at this ordinal

    our_fn = our.funcs[oj]
    their_sym = their_funcs[ti]
    proposed = _normalize_their_name(their_sym.name)
    if not proposed or proposed.startswith("?"):
      continue

    s_them = their_sizes[ti]
    s_ours = our_sizes[oj]
    size_match = False
    high_conf = False
    if s_them and s_ours:
      ratio = abs(s_them - s_ours) / max(s_them, s_ours)
      size_match = ratio <= SIZE_TOLERANCE
      high_conf = ratio <= HIGH_CONF_SIZE_TOL

    confidence = "low"
    if size_match:
      confidence = "high" if high_conf else "medium"

    if our_fn.is_placeholder:
      proposals.append(Proposal(
        our_addr=f"0x{our_fn.addr:x}",
        our_name=our_fn.name,
        proposed_name=proposed,
        our_object=our.name,
        their_source=tu.source_path,
        ordinal=oj,
        size_their=s_them,
        size_ours=s_ours,
        size_match=size_match,
        confidence=confidence,
        real_name=_is_real_name(proposed),
      ))
    elif our_fn.name and our_fn.name != proposed:
      conflicts.append(
        f"  ordinal {oj}: ours={our_fn.name} (0x{our_fn.addr:x}) vs theirs={proposed}")

  return proposals, conflicts, notes


# ---------------------------------------------------------------------------
# Report
# ---------------------------------------------------------------------------

def write_report(theirs: dict[int, TheirTU],
                 ours: list[OurObject],
                 one_to_one,
                 one_to_many,
                 unmatched_ours,
                 unmatched_theirs,
                 all_proposals: list[Proposal],
                 conflicts_by_tu: dict[str, list[str]],
                 notes_by_tu: dict[str, list[str]]) -> None:
  game_tus = [tu for tu in theirs.values()
              if tu.project == GAME_PROJECT and tu.contribs]

  total_their_funcs = sum(len(tu.function_symbols()) for tu in game_tus)
  total_our_funcs = sum(len(o.funcs) for o in ours)
  placeholder_count = sum(1 for o in ours for f in o.funcs if f.is_placeholder)

  by_conf = defaultdict(int)
  by_conf_real = defaultdict(int)
  for p in all_proposals:
    by_conf[p.confidence] += 1
    if p.real_name:
      by_conf_real[p.confidence] += 1
  real_proposals = [p for p in all_proposals if p.real_name]
  safe_apply = [p for p in real_proposals if p.confidence == "high"]

  lines = []
  lines.append("# punpckhdq/halo PDB import report")
  lines.append("")
  lines.append(f"- their game-code TUs ({GAME_PROJECT}): **{len(game_tus)}**")
  lines.append(f"- their game-code function symbols: **{total_their_funcs}**")
  lines.append(f"- our objects: **{len(ours)}** (game-code matchable: ~{len(ours) - 3})")
  lines.append(f"- our functions: **{total_our_funcs}** (placeholders: **{placeholder_count}**)")
  lines.append("")
  lines.append("## TU coverage")
  lines.append("")
  lines.append(f"- 1:1 TU stem matches: **{len(one_to_one)}**")
  lines.append(f"- ambiguous (their data has multiple TUs for our stem): **{len(one_to_many)}**")
  lines.append(f"- our objects with no TU match: **{len(unmatched_ours)}**")
  lines.append(f"- their TUs we never use (potential new TU splits): **{len(unmatched_theirs)}**")
  lines.append("")
  lines.append("## Naming proposals")
  lines.append("")
  lines.append(f"- total proposals: **{len(all_proposals)}** "
               f"(real names: **{len(real_proposals)}**, "
               f"their `code_XXXX` placeholders: **{len(all_proposals) - len(real_proposals)}**)")
  lines.append(f"  - high confidence (size within {int(HIGH_CONF_SIZE_TOL*100)}%): "
               f"**{by_conf['high']}** total / **{by_conf_real['high']}** with real names")
  lines.append(f"  - medium (within {int(SIZE_TOLERANCE*100)}%): "
               f"**{by_conf['medium']}** total / **{by_conf_real['medium']}** with real names")
  lines.append(f"  - low (size unknown / divergent): "
               f"**{by_conf['low']}** total / **{by_conf_real['low']}** with real names")
  lines.append("")
  lines.append(f"**Safe-to-apply set: {len(safe_apply)}** proposals (high-conf + real name).")
  lines.append("")

  total_conflicts = sum(len(v) for v in conflicts_by_tu.values())
  lines.append(f"- name conflicts (ours already named, theirs disagrees): **{total_conflicts}**")
  lines.append("  *Conflicts often indicate an ordinal-misalignment, not a wrong name.*")
  lines.append("")

  lines.append("## Top safe-to-apply proposals (high-conf + real name, first 50)")
  lines.append("")
  lines.append("| our addr | our object | -> proposed name | their source | size their/ours |")
  lines.append("|---|---|---|---|---|")
  high_props = sorted(safe_apply, key=lambda p: p.our_addr)[:50]
  for p in high_props:
    lines.append(
      f"| `{p.our_addr}` | `{p.our_object}` | `{p.proposed_name}` | "
      f"`{p.their_source}` | {p.size_their}/{p.size_ours} |"
    )
  lines.append("")

  lines.append("## Their TUs we never use (top 40 by size)")
  lines.append("")
  lines.append("These are TU splits the original PDB recorded but our kb.json")
  lines.append("groups under a different (or no) object — candidates for splitting.")
  lines.append("")
  lines.append("| their source | size | functions |")
  lines.append("|---|---|---|")
  unmatched_sorted = sorted(unmatched_theirs, key=lambda t: -t.total_size)[:40]
  for tu in unmatched_sorted:
    lines.append(
      f"| `{tu.source_path}` | {tu.total_size} | {len(tu.function_symbols())} |")
  lines.append("")

  lines.append("## Our objects with no TU match (top 40 by function count)")
  lines.append("")
  lines.append("| our object | functions |")
  lines.append("|---|---|")
  for o in sorted(unmatched_ours, key=lambda o: -len(o.funcs))[:40]:
    lines.append(f"| `{o.name}` | {len(o.funcs)} |")
  lines.append("")

  lines.append("## Ambiguous TU matches (their data shows split, ours unified)")
  lines.append("")
  for our, cands in sorted(one_to_many, key=lambda x: -len(x[1]))[:25]:
    sources = ", ".join(f"`{c.source_path}`" for c in cands)
    lines.append(f"- `{our.name}` ({len(our.funcs)} fns) → {sources}")
  lines.append("")

  REPORT_PATH.write_text("\n".join(lines))
  print(f"wrote {REPORT_PATH.relative_to(ROOT)}", file=sys.stderr)


def write_proposals(props: list[Proposal]) -> None:
  PROPOSALS_PATH.write_text(json.dumps(
    [asdict(p) for p in props], indent=2, sort_keys=True))
  print(f"wrote {PROPOSALS_PATH.relative_to(ROOT)} ({len(props)} proposals)",
        file=sys.stderr)


# ---------------------------------------------------------------------------

def main() -> int:
  ap = argparse.ArgumentParser(description=__doc__,
                               formatter_class=argparse.RawDescriptionHelpFormatter)
  ap.add_argument("--refresh", action="store_true",
                  help="re-download upstream JSON files")
  args = ap.parse_args()

  fetch_data(refresh=args.refresh)
  theirs = load_their_data()
  ours = load_our_kb()

  one_to_one, one_to_many, unmatched_ours, unmatched_theirs = match_tus(
    theirs, ours)

  all_proposals: list[Proposal] = []
  conflicts_by_tu: dict[str, list[str]] = {}
  notes_by_tu: dict[str, list[str]] = {}

  for our, tu in one_to_one:
    props, confs, notes = propose_names(our, tu)
    all_proposals.extend(props)
    if confs:
      conflicts_by_tu[our.name] = confs
    if notes:
      notes_by_tu[our.name] = notes

  write_report(theirs, ours, one_to_one, one_to_many,
               unmatched_ours, unmatched_theirs,
               all_proposals, conflicts_by_tu, notes_by_tu)
  write_proposals(all_proposals)

  real = [p for p in all_proposals if p.real_name]
  safe = [p for p in real if p.confidence == "high"]
  print(
    f"summary: {len(one_to_one)} TU matches, "
    f"{len(all_proposals)} proposals total "
    f"({len(real)} real names, {len(safe)} safe-to-apply)",
    file=sys.stderr)

  return 0


if __name__ == "__main__":
  raise SystemExit(main())
