#!/usr/bin/env python3
"""Analyze <common> functions in kb.json and propose reclassification targets.

Scores each <common> function by address proximity, name-prefix matching,
and address-gap analysis to determine which object file it likely belongs to.

Usage:
    python3 tools/classify_common.py                  # Full report
    python3 tools/classify_common.py --high-only      # Only high-confidence
    python3 tools/classify_common.py --json            # Machine-readable output
    python3 tools/classify_common.py --by-target       # Group by target object
    python3 tools/classify_common.py --summary         # Counts only
"""

from __future__ import annotations

import argparse
import bisect
import json
import os
import re
import subprocess
import sys
from collections import defaultdict
from dataclasses import dataclass, field
from pathlib import Path

_tools_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
KB_JSON = REPO_ROOT / "kb.json"

# Name prefixes that strongly associate with known object files.
# Built from the actual object/source naming conventions in the project.
PREFIX_TO_OBJECT: dict[str, str] = {
    "object_": "objects.obj",
    "unit_": "units.obj",
    "weapon_": "weapons.obj",
    "item_": "items.obj",
    "vehicle_": "vehicles.obj",
    "actor_": "actors.obj",
    "ai_": "ai.obj",
    "player_": "players.obj",
    "game_engine_": "game_engine.obj",
    "game_time_": "game_time.obj",
    "game_": "game.obj",
    "sound_": "sound_manager.obj",
    "random_": "random_math.obj",
    "matrix_": "real_math.obj",
    "real_math_": "real_math.obj",
    "cross_product": "vector_math.obj",
    "valid_real_normal": "vector_math.obj",
    "rotate_vector": "real_math.obj",
    "bsp3d_": "collision_bsp.obj",
    "cluster_partition_": "structures.obj",
    "decal_": "decals.obj",
    "structure_": "structures.obj",
    "scenario_": "scenario.obj",
    "model_animation_": "model_animations.obj",
    "device_group_": "devices.obj",
    "edit_text_": "edit_text.obj",
    "network_": "network.obj",
    "projectile_": "projectiles.obj",
    "effect_": "effects.obj",
    "damage_": "damage.obj",
    "hs_": "hs.obj",
    "tag_": "tags.obj",
    "collision_": "collision.obj",
    "rasterizer_": "rasterizer.obj",
}

# XDK / D3D / DirectSound patterns — these are SDK stubs, not game code.
XDK_PATTERNS = [
    re.compile(r"^D3D"),
    re.compile(r"^DirectSound"),
    re.compile(r"^IDirectSound"),
    re.compile(r"^SwitchToThread$"),
    re.compile(r"^Nt\w+"),
    re.compile(r"^Ke\w+"),
    re.compile(r"^Rtl\w+"),
    re.compile(r"^Xapi\w+"),
]


@dataclass
class CommonFunction:
    addr: int
    name: str
    decl: str
    is_unnamed: bool
    is_xdk: bool = False

    # Analysis results
    before_obj: str | None = None
    after_obj: str | None = None
    before_addr: int | None = None
    after_addr: int | None = None
    gap_before: int | None = None
    gap_after: int | None = None
    proposed_target: str | None = None
    confidence: str = "unknown"  # high, medium, low, xdk
    reasons: list[str] = field(default_factory=list)


def load_kb() -> dict:
    """Load kb.json via jq for the specific fields we need."""
    result = subprocess.run(
        [
            "jq",
            "-c",
            ".objects"
            ' | [.[] | {name, source, functions: [.functions[]? | {decl, addr}],'
            " data: [.data[]? | {decl, addr}]}]",
            str(KB_JSON),
        ],
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        print(f"error: jq failed: {result.stderr.strip()}", file=sys.stderr)
        raise SystemExit(1)
    return json.loads(result.stdout)


def extract_name(decl: str) -> str:
    """Extract function name from declaration string."""
    # Remove @<reg> annotations
    cleaned = re.sub(r"@<\w+>", "", decl)
    m = re.search(r"(\*?\w+)\s*\(", cleaned)
    return m.group(1) if m else "???"


def is_xdk_function(name: str) -> bool:
    return any(p.match(name.lstrip("*")) for p in XDK_PATTERNS)


def prefix_match(name: str) -> str | None:
    """Check if a function name prefix matches a known object."""
    clean = name.lstrip("*")
    for prefix, obj in PREFIX_TO_OBJECT.items():
        if clean.startswith(prefix):
            return obj
    return None


def analyze(objects: list[dict]) -> list[CommonFunction]:
    # Build the address->object index from all non-<common> objects
    addr_to_obj: dict[int, tuple[str, str]] = {}
    obj_addr_ranges: dict[str, tuple[int, int]] = {}

    for obj in objects:
        if obj["name"] == "<common>" or obj["name"] is None:
            continue
        obj_addrs = []
        for fn in obj.get("functions") or []:
            try:
                a = int(fn["addr"], 16)
            except (KeyError, ValueError):
                continue
            name = extract_name(fn["decl"])
            addr_to_obj[a] = (obj["name"], name)
            obj_addrs.append(a)
        if obj_addrs:
            obj_addr_ranges[obj["name"]] = (min(obj_addrs), max(obj_addrs))

    sorted_addrs = sorted(addr_to_obj.keys())

    # Extract <common> functions
    common_obj = next((o for o in objects if o.get("name") == "<common>"), None)
    if not common_obj:
        print("No <common> object found in kb.json", file=sys.stderr)
        return []

    results: list[CommonFunction] = []

    for fn in common_obj.get("functions") or []:
        try:
            addr = int(fn["addr"], 16)
        except (KeyError, ValueError):
            continue

        name = extract_name(fn["decl"])
        is_unnamed = name.startswith("FUN_") or name.startswith("*FUN_")
        cf = CommonFunction(
            addr=addr,
            name=name,
            decl=fn["decl"],
            is_unnamed=is_unnamed,
            is_xdk=is_xdk_function(name),
        )

        # Find neighbors
        idx = bisect.bisect_left(sorted_addrs, addr)

        if idx > 0:
            before_a = sorted_addrs[idx - 1]
            cf.before_obj = addr_to_obj[before_a][0]
            cf.before_addr = before_a
            cf.gap_before = addr - before_a

        if idx < len(sorted_addrs):
            after_a = sorted_addrs[idx]
            cf.after_obj = addr_to_obj[after_a][0]
            cf.after_addr = after_a
            cf.gap_after = after_a - addr

        # Score and classify
        _classify(cf, obj_addr_ranges)
        results.append(cf)

    results.sort(key=lambda c: _confidence_rank(c.confidence))
    return results


def _classify(cf: CommonFunction, obj_ranges: dict[str, tuple[int, int]]) -> None:
    """Assign confidence and proposed_target to a CommonFunction."""
    reasons = cf.reasons

    # XDK stubs get their own category
    if cf.is_xdk:
        cf.confidence = "xdk"
        cf.proposed_target = "<xdk_stubs>"
        reasons.append("XDK/D3D/DirectSound API stub")
        return

    sandwiched = cf.before_obj == cf.after_obj and cf.before_obj is not None
    prefix_obj = prefix_match(cf.name)

    # Check if the function falls within a known object's address span
    inside_obj = None
    for oname, (lo, hi) in obj_ranges.items():
        if lo <= cf.addr <= hi:
            inside_obj = oname
            break

    if sandwiched:
        cf.proposed_target = cf.before_obj
        # Check gap sizes — tighter gaps = higher confidence
        tight = (cf.gap_before is not None and cf.gap_before < 0x2000
                 and cf.gap_after is not None and cf.gap_after < 0x2000)

        if tight:
            cf.confidence = "high"
            reasons.append(f"sandwiched between {cf.before_obj} functions (tight gap)")
        else:
            cf.confidence = "high"
            reasons.append(f"sandwiched between {cf.before_obj} functions")

        if prefix_obj and prefix_obj == cf.before_obj:
            reasons.append(f"name prefix '{cf.name.split('_')[0]}_' matches target")
        elif prefix_obj and prefix_obj != cf.before_obj:
            reasons.append(f"NOTE: name prefix suggests {prefix_obj} instead")
            cf.confidence = "medium"

    elif inside_obj and (inside_obj == cf.before_obj or inside_obj == cf.after_obj):
        cf.proposed_target = inside_obj
        cf.confidence = "high"
        reasons.append(f"within address span of {inside_obj}")
        if prefix_obj and prefix_obj == inside_obj:
            reasons.append("name prefix confirms")

    elif cf.before_obj and cf.after_obj:
        # Boundary case — use heuristics to pick a side
        prefix_match_before = prefix_obj == cf.before_obj if prefix_obj else False
        prefix_match_after = prefix_obj == cf.after_obj if prefix_obj else False

        gap_b = cf.gap_before or 0xFFFFFF
        gap_a = cf.gap_after or 0xFFFFFF

        if prefix_match_before and not prefix_match_after:
            cf.proposed_target = cf.before_obj
            cf.confidence = "medium"
            reasons.append(f"name prefix matches {cf.before_obj}")
            reasons.append(f"on boundary {cf.before_obj} / {cf.after_obj}")
        elif prefix_match_after and not prefix_match_before:
            cf.proposed_target = cf.after_obj
            cf.confidence = "medium"
            reasons.append(f"name prefix matches {cf.after_obj}")
            reasons.append(f"on boundary {cf.before_obj} / {cf.after_obj}")
        elif gap_b < gap_a * 0.3:
            cf.proposed_target = cf.before_obj
            cf.confidence = "medium"
            reasons.append(f"much closer to {cf.before_obj} (0x{gap_b:x} vs 0x{gap_a:x})")
        elif gap_a < gap_b * 0.3:
            cf.proposed_target = cf.after_obj
            cf.confidence = "medium"
            reasons.append(f"much closer to {cf.after_obj} (0x{gap_a:x} vs 0x{gap_b:x})")
        else:
            # Could be either — or a missing object between them
            cf.proposed_target = f"{cf.before_obj} or {cf.after_obj}"
            cf.confidence = "low"
            reasons.append(f"on boundary {cf.before_obj} / {cf.after_obj}")
            reasons.append(f"gaps: before=0x{gap_b:x} after=0x{gap_a:x}")

        if prefix_obj and cf.proposed_target and prefix_obj not in cf.proposed_target:
            reasons.append(f"name prefix suggests {prefix_obj} (neither neighbor)")

    elif cf.before_obj or cf.after_obj:
        neighbor = cf.before_obj or cf.after_obj
        cf.proposed_target = neighbor
        cf.confidence = "low"
        reasons.append(f"only one neighbor: {neighbor}")
    else:
        cf.confidence = "low"
        cf.proposed_target = None
        reasons.append("no known neighbors")

    # Unnamed functions get a confidence downgrade for anything above low
    if cf.is_unnamed and cf.confidence == "high":
        cf.confidence = "medium"
        reasons.append("unnamed function — verify via delinker before moving")


def _confidence_rank(c: str) -> int:
    return {"high": 0, "medium": 1, "low": 2, "xdk": 3, "unknown": 4}[c]


def _confidence_color(c: str) -> str:
    return {"high": "\033[92m", "medium": "\033[93m", "low": "\033[91m",
            "xdk": "\033[96m", "unknown": "\033[90m"}[c]


RESET = "\033[0m"


def print_report(results: list[CommonFunction], high_only: bool = False) -> None:
    counts = defaultdict(int)
    for r in results:
        counts[r.confidence] += 1

    print("=" * 78)
    print(f"  <common> Reclassification Analysis — {len(results)} functions")
    print(f"  high: {counts['high']}  medium: {counts['medium']}"
          f"  low: {counts['low']}  xdk: {counts['xdk']}")
    print("=" * 78)

    for r in results:
        if high_only and r.confidence not in ("high",):
            continue

        color = _confidence_color(r.confidence)
        print(f"\n  {color}[{r.confidence:6s}]{RESET}  0x{r.addr:06x}  {r.name}")
        if r.proposed_target:
            print(f"           -> {r.proposed_target}")
        for reason in r.reasons:
            print(f"           {reason}")


def print_by_target(results: list[CommonFunction]) -> None:
    groups: dict[str, list[CommonFunction]] = defaultdict(list)
    for r in results:
        key = r.proposed_target or "<unresolved>"
        groups[key].append(r)

    # Sort groups by count descending
    for target, fns in sorted(groups.items(), key=lambda kv: -len(kv[1])):
        confs = defaultdict(int)
        for f in fns:
            confs[f.confidence] += 1

        conf_str = ", ".join(f"{c}: {n}" for c, n in sorted(confs.items(), key=lambda x: _confidence_rank(x[0])))
        print(f"\n{'─' * 70}")
        print(f"  {target}  ({len(fns)} functions — {conf_str})")
        print(f"{'─' * 70}")
        for f in sorted(fns, key=lambda x: x.addr):
            color = _confidence_color(f.confidence)
            tag = "  " if not f.is_unnamed else "? "
            print(f"  {color}{tag}0x{f.addr:06x}{RESET}  {f.name}")


def find_gap_clusters(results: list[CommonFunction], objects: list[dict]) -> list[dict]:
    """Find large address gaps between known objects that contain <common> functions.

    These are likely entire missing object files that haven't been catalogued yet.
    Returns a list of cluster dicts sorted by function count descending.
    """
    # Build sorted list of (addr, object_name) for all non-common functions
    known_endpoints: list[tuple[int, str]] = []
    for obj in objects:
        if obj["name"] in ("<common>", None):
            continue
        for fn in obj.get("functions") or []:
            try:
                a = int(fn["addr"], 16)
            except (KeyError, ValueError):
                continue
            known_endpoints.append((a, obj["name"]))
    known_endpoints.sort()

    if len(known_endpoints) < 2:
        return []

    # Find gaps between consecutive known objects where the object changes
    non_xdk = [r for r in results if r.confidence != "xdk"]
    addr_to_cf = {r.addr: r for r in non_xdk}

    clusters: list[dict] = []
    for i in range(len(known_endpoints) - 1):
        a1, obj1 = known_endpoints[i]
        a2, obj2 = known_endpoints[i + 1]

        if obj1 == obj2:
            continue

        gap_size = a2 - a1
        if gap_size < 0x400:
            continue

        # Find <common> functions in this gap
        gap_fns = [cf for cf in non_xdk if a1 < cf.addr < a2]
        if len(gap_fns) < 2:
            continue

        gap_fns.sort(key=lambda f: f.addr)
        named = [f for f in gap_fns if not f.is_unnamed]

        clusters.append({
            "before_obj": obj1,
            "after_obj": obj2,
            "gap_start": a1,
            "gap_end": a2,
            "gap_size": gap_size,
            "functions": gap_fns,
            "named_functions": named,
            "delinker_range": f"0x{gap_fns[0].addr:08x}-0x{gap_fns[-1].addr + 0x200:08x}",
        })

    clusters.sort(key=lambda c: -len(c["functions"]))
    return clusters


def print_contiguous_blocks(results: list[CommonFunction], objects: list[dict]) -> None:
    """Identify gap clusters that likely represent missing object files."""
    clusters = find_gap_clusters(results, objects)

    print(f"\n{'=' * 78}")
    print("  Gap Clusters (probable missing objects)")
    print(f"{'=' * 78}")

    if not clusters:
        print("\n  No significant clusters found.")
        return

    for cluster in clusters:
        fns = cluster["functions"]
        named = cluster["named_functions"]
        lo = fns[0].addr
        hi = fns[-1].addr

        print(f"\n  {'─' * 68}")
        print(f"  {len(fns)} functions in gap between"
              f" {cluster['before_obj']} and {cluster['after_obj']}")
        print(f"  address range: 0x{lo:06x} - 0x{hi:06x}"
              f"  (gap 0x{cluster['gap_size']:x})")
        print(f"  named: {len(named)}/{len(fns)}")

        if named:
            print(f"  known names: {', '.join(f.name for f in named[:10])}")

        # Show address sub-clusters (gaps within the gap)
        sub_groups: list[list[CommonFunction]] = []
        current: list[CommonFunction] = [fns[0]]
        for f in fns[1:]:
            if f.addr - current[-1].addr > 0x2000:
                sub_groups.append(current)
                current = [f]
            else:
                current.append(f)
        sub_groups.append(current)

        if len(sub_groups) > 1:
            print(f"  sub-clusters: {len(sub_groups)}")
            for i, sg in enumerate(sub_groups):
                sg_named = [f for f in sg if not f.is_unnamed]
                names_str = f"  ({', '.join(f.name for f in sg_named)})" if sg_named else ""
                print(f"    [{i+1}] 0x{sg[0].addr:06x}-0x{sg[-1].addr:06x}"
                      f"  ({len(sg)} fns){names_str}")

        print(f"  delinker range: {cluster['delinker_range']}")


def print_summary(results: list[CommonFunction]) -> None:
    counts = defaultdict(int)
    targets = defaultdict(int)
    for r in results:
        counts[r.confidence] += 1
        if r.proposed_target:
            targets[r.proposed_target] += 1

    print(f"Total <common> functions: {len(results)}")
    print(f"\nBy confidence:")
    for c in ["high", "medium", "low", "xdk"]:
        if counts[c]:
            print(f"  {c:8s}: {counts[c]}")

    print(f"\nTop proposed targets:")
    for target, count in sorted(targets.items(), key=lambda kv: -kv[1])[:15]:
        print(f"  {count:3d}  {target}")


def _split_into_contiguous(fns: list[CommonFunction],
                           max_gap: int = 0x2000) -> list[list[CommonFunction]]:
    """Split a list of functions into contiguous sub-groups."""
    if not fns:
        return []
    fns_sorted = sorted(fns, key=lambda f: f.addr)
    groups: list[list[CommonFunction]] = [[fns_sorted[0]]]
    for f in fns_sorted[1:]:
        if f.addr - groups[-1][-1].addr > max_gap:
            groups.append([f])
        else:
            groups[-1].append(f)
    return groups


def print_delinker_plan(results: list[CommonFunction], objects: list[dict]) -> None:
    """Output delinker commands to validate high/medium confidence reclassifications."""
    ARTIFACTS = "G:\\\\dev\\\\halo\\\\artifacts\\\\delinker"

    print(f"{'=' * 78}")
    print("  Delinker Validation Plan")
    print(f"  Export path root: {ARTIFACTS}")
    print(f"{'=' * 78}")

    # 1. High-confidence targets grouped by object, split into contiguous ranges
    high_medium = [r for r in results if r.confidence in ("high", "medium") and r.proposed_target]
    groups: dict[str, list[CommonFunction]] = defaultdict(list)
    for r in high_medium:
        if " or " not in (r.proposed_target or ""):
            groups[r.proposed_target].append(r)

    plan_items: list[tuple[str, str, int, int, int]] = []  # (label, path, lo, hi, count)

    print(f"\n  --- Per-object validation ---\n")
    for target, fns in sorted(groups.items(), key=lambda kv: -len(kv[1])):
        if target == "<xdk_stubs>":
            continue
        safe_name = target.replace(".obj", "")

        sub_groups = _split_into_contiguous(fns)
        for i, sg in enumerate(sub_groups):
            lo = sg[0].addr
            hi = sg[-1].addr + 0x200
            suffix = f"_{i+1}" if len(sub_groups) > 1 else ""
            label = f"{safe_name}_common{suffix}"
            named = [f.name for f in sg if not f.is_unnamed]
            names_hint = f"  ({', '.join(named[:4])})" if named else ""

            print(f"  # {target}: {len(sg)} candidates{names_hint}")
            print(f"  run_relocation_synthesizer(selection_mode=\"range\","
                  f" range=\"0x{lo:08x}-0x{hi:08x}\")")
            print(f"  export_delinked_object(")
            print(f"    export_path=\"{ARTIFACTS}\\\\{label}.o\",")
            print(f"    selection_mode=\"range\",")
            print(f"    range=\"0x{lo:08x}-0x{hi:08x}\",")
            print(f"    run_relocation_synthesizer=false)")
            print()

    # 2. Gap clusters
    clusters = find_gap_clusters(results, objects)
    if clusters:
        print(f"\n  --- Gap cluster validation ({len(clusters)} clusters) ---\n")
        for cluster in clusters:
            fns = cluster["functions"]
            before = cluster["before_obj"].replace(".obj", "")
            after = cluster["after_obj"].replace(".obj", "")
            lo = fns[0].addr
            hi = fns[-1].addr + 0x200
            label = f"gap_{before}_{after}"

            print(f"  # {len(fns)} functions between"
                  f" {cluster['before_obj']} / {cluster['after_obj']}")
            print(f"  run_relocation_synthesizer(selection_mode=\"range\","
                  f" range=\"0x{lo:08x}-0x{hi:08x}\")")
            print(f"  export_delinked_object(")
            print(f"    export_path=\"{ARTIFACTS}\\\\{label}.o\",")
            print(f"    selection_mode=\"range\",")
            print(f"    range=\"0x{lo:08x}-0x{hi:08x}\",")
            print(f"    run_relocation_synthesizer=false)")
            print()


# ---------------------------------------------------------------------------
# Live delinker analysis
# ---------------------------------------------------------------------------

GHIDRA_RPC_URL = "http://127.0.0.1:18080/rpc"
DELINKER_ARTIFACTS = Path("/mnt/g/dev/halo/artifacts/delinker")
DELINKER_ARTIFACTS_WIN = "G:\\dev\\halo\\artifacts\\delinker"

SOURCE_PATH_RE = re.compile(
    r"s_c:\\halo\\(?:SOURCE|source)\\(.+?)_([0-9a-f]{8})$"
)

XBE_PATH = REPO_ROOT / "halo-patched" / "cachebeta.xbe"

_xbe_sections: list[tuple[int, int, int, int]] | None = None


def _load_xbe_sections() -> list[tuple[int, int, int, int]]:
    """Load XBE section table for VA-to-file-offset translation."""
    global _xbe_sections
    if _xbe_sections is not None:
        return _xbe_sections

    import struct
    sections = []
    with open(XBE_PATH, "rb") as f:
        f.seek(0x104)
        base_addr = struct.unpack("<I", f.read(4))[0]
        f.seek(0x11C)
        section_count = struct.unpack("<I", f.read(4))[0]
        section_header_addr = struct.unpack("<I", f.read(4))[0]
        f.seek(section_header_addr - base_addr)
        for _ in range(section_count):
            sh = f.read(56)
            vaddr = struct.unpack_from("<I", sh, 4)[0]
            vsize = struct.unpack_from("<I", sh, 8)[0]
            raw_addr = struct.unpack_from("<I", sh, 12)[0]
            raw_size = struct.unpack_from("<I", sh, 16)[0]
            sections.append((vaddr, vsize, raw_addr, raw_size))
    _xbe_sections = sections
    return sections


def _read_xbe_string(va: int) -> str | None:
    """Read a null-terminated ASCII string from the XBE at virtual address va."""
    import struct
    sections = _load_xbe_sections()
    for vaddr, vsize, raw_addr, raw_size in sections:
        if vaddr <= va < vaddr + vsize:
            file_off = raw_addr + (va - vaddr)
            with open(XBE_PATH, "rb") as f:
                f.seek(file_off)
                data = f.read(120)
            null_idx = data.find(b"\x00")
            if null_idx >= 0:
                return data[:null_idx].decode("ascii", errors="replace")
            return None
    return None


def _resolve_source_path(truncated: str, addr_hex: str) -> str:
    """Resolve a possibly-truncated source path by reading the full string from the XBE."""
    if not XBE_PATH.exists():
        return truncated
    try:
        va = int(addr_hex, 16)
        full = _read_xbe_string(va)
        if full and "\\halo\\" in full.lower():
            # Extract relative path after SOURCE\ or source\
            idx = full.lower().find("\\source\\")
            if idx >= 0:
                return full[idx + len("\\source\\"):]
            idx = full.lower().find("\\halo\\")
            if idx >= 0:
                return full[idx + len("\\halo\\"):]
        return full or truncated
    except Exception:
        return truncated


def _ghidra_rpc(method: str, params: dict | None = None) -> dict:
    """Call the Ghidra delinker RPC directly (bypasses MCP SSE transport)."""
    import urllib.error
    import urllib.parse
    import urllib.request

    form: dict[str, str] = {"method": method}
    if params:
        for k, v in params.items():
            if v is not None:
                form[k] = str(v)
    payload = urllib.parse.urlencode(form).encode("utf-8")
    req = urllib.request.Request(
        GHIDRA_RPC_URL,
        data=payload,
        headers={"Content-Type": "application/x-www-form-urlencoded"},
        method="POST",
    )
    with urllib.request.urlopen(req, timeout=120) as resp:
        raw = resp.read().decode("utf-8")
    lines = raw.splitlines()
    if not lines or lines[0].strip() != "ok":
        raise RuntimeError(f"RPC error: {raw[:200]}")
    kv: dict[str, str] = {}
    for line in lines[1:]:
        if "=" in line:
            k, v = line.split("=", 1)
            kv[k] = v
    return kv


def _is_ghidra_available() -> bool:
    try:
        _ghidra_rpc("get_current_program")
        return True
    except Exception:
        return False


def _delink_range(label: str, lo: int, hi: int) -> Path | None:
    """Run relocation synthesizer + export for a range. Returns the .o path."""
    out_path_win = f"{DELINKER_ARTIFACTS_WIN}\\{label}.o"
    range_str = f"{lo:08x}-{hi:08x}"

    try:
        _ghidra_rpc("run_relocation_synthesizer", {
            "selection_mode": "range",
            "range": range_str,
        })
        _ghidra_rpc("export_delinked_object", {
            "export_path": out_path_win,
            "exporter_name": "COFF relocatable object",
            "selection_mode": "range",
            "range": range_str,
            "run_relocation_synthesizer": "false",
        })
    except Exception as e:
        print(f"  FAIL  {label}: {e}", file=sys.stderr)
        return None

    local_path = DELINKER_ARTIFACTS / f"{label}.o"
    if local_path.exists():
        return local_path
    return None


def _parse_source_paths(obj_path: Path) -> list[tuple[int, str]]:
    """Extract (offset, source_file) pairs from COFF relocation symbols.

    Resolves truncated Ghidra symbol names to full paths via XBE string lookup.
    """
    result = subprocess.run(
        ["objdump", "-r", str(obj_path)],
        capture_output=True, text=True,
    )
    pairs: list[tuple[int, str]] = []
    for line in result.stdout.splitlines():
        parts = line.split()
        if len(parts) < 3:
            continue
        sym = parts[-1]
        m = SOURCE_PATH_RE.match(sym)
        if m:
            try:
                offset = int(parts[0], 16)
            except ValueError:
                continue
            truncated_path = m.group(1)
            addr_hex = m.group(2)
            full_path = _resolve_source_path(truncated_path, addr_hex)
            pairs.append((offset, full_path))
    return pairs


def _derive_obj_name(source_path: str) -> str:
    """Derive an object name from a source path like 'ai\\ai_communication.c'."""
    normalized = source_path.replace("\\", "/")
    basename = normalized.rsplit("/", 1)[-1]
    stem = basename.rsplit(".", 1)[0] if "." in basename else basename
    return f"{stem}.obj"


def _derive_source_field(source_path: str) -> str:
    """Derive the kb.json 'source' field from a Windows source path."""
    normalized = source_path.replace("\\", "/")
    # Strip .h -> .c for header files (the .obj comes from the .c)
    if normalized.endswith(".h"):
        normalized = normalized[:-2] + ".c"
    return normalized


def run_delinker_analysis(results: list[CommonFunction],
                          objects: list[dict]) -> None:
    """Run the live delinker on interesting ranges and report source file discoveries."""
    DELINKER_ARTIFACTS.mkdir(parents=True, exist_ok=True)

    if not _is_ghidra_available():
        print("ERROR: Ghidra RPC not available at", GHIDRA_RPC_URL, file=sys.stderr)
        print("Start Ghidra with the Delinker Control plugin enabled.", file=sys.stderr)
        raise SystemExit(1)

    print(f"{'=' * 78}")
    print("  Live Delinker Analysis")
    print(f"  Ghidra RPC: {GHIDRA_RPC_URL}")
    print(f"{'=' * 78}")

    # Collect ranges to analyze:
    # 1. Gap clusters (most interesting — potentially missing objects)
    clusters = find_gap_clusters(results, objects)
    # 2. High/medium confidence groups
    high_medium = [r for r in results
                   if r.confidence in ("high", "medium")
                   and r.proposed_target
                   and " or " not in (r.proposed_target or "")]
    target_groups: dict[str, list[CommonFunction]] = defaultdict(list)
    for r in high_medium:
        if r.proposed_target != "<xdk_stubs>":
            target_groups[r.proposed_target].append(r)

    all_source_discoveries: list[dict] = []

    # --- Gap clusters ---
    if clusters:
        print(f"\n  --- Gap Clusters ({len(clusters)}) ---")
        for cluster in clusters:
            fns = cluster["functions"]
            before = cluster["before_obj"].replace(".obj", "")
            after = cluster["after_obj"].replace(".obj", "")
            lo = fns[0].addr
            hi = fns[-1].addr + 0x200
            label = f"gap_{before}_{after}"

            print(f"\n  Exporting {label} (0x{lo:08x}-0x{hi:08x},"
                  f" {len(fns)} functions)...")
            obj_path = _delink_range(label, lo, hi)
            if not obj_path:
                continue

            sources = _parse_source_paths(obj_path)
            if sources:
                # Group by source file and find offset boundaries
                source_files: dict[str, list[int]] = defaultdict(list)
                for offset, src in sources:
                    source_files[src].append(offset)

                print(f"  Found {len(source_files)} source files:")
                for src, offsets in sorted(source_files.items(),
                                          key=lambda kv: min(kv[1])):
                    obj_name = _derive_obj_name(src)
                    lo_off = min(offsets)
                    hi_off = max(offsets)
                    print(f"    {obj_name:35s}  offsets 0x{lo_off:05x}-0x{hi_off:05x}"
                          f"  ({len(offsets)} refs)  <- {src}")
                    all_source_discoveries.append({
                        "source": src,
                        "obj_name": obj_name,
                        "cluster": label,
                        "ref_count": len(offsets),
                        "offset_lo": lo_off,
                        "offset_hi": hi_off,
                    })
            else:
                print(f"  No source path strings found in relocations")

    # --- Per-target validation ---
    print(f"\n  --- Per-Target Validation ({len(target_groups)} targets) ---")
    for target, fns in sorted(target_groups.items(), key=lambda kv: -len(kv[1])):
        sub_groups = _split_into_contiguous(fns)
        for i, sg in enumerate(sub_groups):
            lo = sg[0].addr
            hi = sg[-1].addr + 0x200
            safe_name = target.replace(".obj", "")
            suffix = f"_{i+1}" if len(sub_groups) > 1 else ""
            label = f"{safe_name}_common{suffix}"

            named = [f.name for f in sg if not f.is_unnamed]
            names_hint = f" ({', '.join(named[:3])})" if named else ""
            print(f"\n  Exporting {label} (0x{lo:08x}-0x{hi:08x},"
                  f" {len(sg)} fns){names_hint}...")
            obj_path = _delink_range(label, lo, hi)
            if not obj_path:
                continue

            sources = _parse_source_paths(obj_path)
            if sources:
                source_files: dict[str, int] = defaultdict(int)
                for _, src in sources:
                    source_files[src] += 1

                for src, count in sorted(source_files.items(),
                                        key=lambda kv: -kv[1]):
                    obj_name = _derive_obj_name(src)
                    match = "MATCH" if obj_name == target else "MISMATCH"
                    color = "\033[92m" if match == "MATCH" else "\033[91m"
                    print(f"    {color}[{match}]{RESET}  {obj_name:30s}"
                          f"  ({count} refs)  <- {src}")
                    all_source_discoveries.append({
                        "source": src,
                        "obj_name": obj_name,
                        "expected_target": target,
                        "match": obj_name == target,
                        "ref_count": count,
                    })
            else:
                print(f"    (no source path strings in relocations)")

    # --- Summary ---
    if all_source_discoveries:
        print(f"\n{'=' * 78}")
        print("  Discovery Summary")
        print(f"{'=' * 78}")

        known_objs = {o["name"] for o in objects if o.get("name")}
        new_objs = set()
        confirmed = set()
        mismatched = set()

        for d in all_source_discoveries:
            obj = d["obj_name"]
            if obj not in known_objs:
                new_objs.add(f"{obj} <- {d['source']}")
            if d.get("match") is True:
                confirmed.add(d.get("expected_target", ""))
            elif d.get("match") is False:
                mismatched.add(f"{d.get('expected_target','')} -> actually {obj}")

        if confirmed:
            print(f"\n  Confirmed targets ({len(confirmed)}):")
            for c in sorted(confirmed):
                print(f"    \033[92m✓\033[0m  {c}")

        if new_objs:
            print(f"\n  NEW object files discovered ({len(new_objs)}):")
            for n in sorted(new_objs):
                print(f"    \033[93m+\033[0m  {n}")

        if mismatched:
            print(f"\n  Mismatched predictions ({len(mismatched)}):")
            for m in sorted(mismatched):
                print(f"    \033[91m✗\033[0m  {m}")


def to_json(results: list[CommonFunction]) -> str:
    return json.dumps(
        [
            {
                "addr": f"0x{r.addr:06x}",
                "name": r.name,
                "decl": r.decl,
                "is_unnamed": r.is_unnamed,
                "confidence": r.confidence,
                "proposed_target": r.proposed_target,
                "before_obj": r.before_obj,
                "after_obj": r.after_obj,
                "gap_before": r.gap_before,
                "gap_after": r.gap_after,
                "reasons": r.reasons,
            }
            for r in results
        ],
        indent=2,
    )


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--high-only", action="store_true",
                    help="Show only high-confidence reclassifications")
    ap.add_argument("--json", action="store_true",
                    help="Output machine-readable JSON")
    ap.add_argument("--by-target", action="store_true",
                    help="Group results by proposed target object")
    ap.add_argument("--summary", action="store_true",
                    help="Print summary counts only")
    ap.add_argument("--blocks", action="store_true",
                    help="Show gap clusters (probable missing objects)")
    ap.add_argument("--delinker-plan", action="store_true",
                    help="Output delinker commands for validation")
    ap.add_argument("--delinker-analyze", action="store_true",
                    help="Run live delinker, export objects, discover source files")
    args = ap.parse_args()

    objects = load_kb()
    results = analyze(objects)

    if args.delinker_analyze:
        run_delinker_analysis(results, objects)
    elif args.json:
        print(to_json(results))
    elif args.summary:
        print_summary(results)
    elif args.by_target:
        print_by_target(results)
    elif args.blocks:
        print_report(results, high_only=args.high_only)
        print_contiguous_blocks(results, objects)
    elif args.delinker_plan:
        print_delinker_plan(results, objects)
    else:
        print_report(results, high_only=args.high_only)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
