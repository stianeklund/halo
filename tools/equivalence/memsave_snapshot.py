#!/usr/bin/env python3
"""Capture a VIRTUAL state snapshot for an equivalence target via xemu `memsave`.

Why this exists
---------------
`dump_xemu_memory.py` captures via QMP `pmemsave` (PHYSICAL memory).  The Xbox
does not identity-map physical RAM: the XBE image lives at a non-uniform
physical offset (empirically +0x72000 for parts of .rdata, but it is NOT
uniform — read-only constants a few KB apart need different shifts), and heap
pointers land in yet another regime.  So a physical dump cannot be turned back
into the guest's virtual view without page-table-aware translation.  This was
verified empirically (2026-05-30): the read-only constant at 0x254cb8 reads
garbage at +0x72000 while 0x2533c8 reads correctly, and global pointer storage
reads as impossible addresses.

`memsave` instead dumps the guest's VIRTUAL address space (the running CPU's
MMU view): file_offset == virtual_address, ZERO translation.  Validated
byte-exact against known_globals.json — and it even recovers bytes the static
extractor missed (e.g. the high dword of the 8-byte double at 0x2533d0, which
known_globals.json lacks because no DIR32 reloc references 0x2533d4 directly).

This tool is RELOC-DRIVEN: it parses the target function's DIR32 relocations
from its delinked .obj to learn exactly which global addresses it reads, then
memsave's only those small windows (optionally following pointer chains).  That
keeps snapshots tiny (KB, not the 244 MB that `--full` physical dumps produced)
and supplies the real data that stubbable functions early-exit without.

Workflow
--------
    # 1. Discover regions + emit a capture plan (no VM needed):
    python3 tools/equivalence/memsave_snapshot.py plan --target FUNC \
        --out artifacts/equivalence/plan_FUNC.json

    # 2. Capture (drives QMP memsave; needs a running xemu in the target state):
    python3 tools/equivalence/memsave_snapshot.py capture \
        --plan artifacts/equivalence/plan_FUNC.json \
        --out  artifacts/equivalence/snap_FUNC.json
    #    In a session where the xemu MCP already owns the QMP socket, run the
    #    memsave commands printed by `plan` through the MCP, then `assemble`.

    # 3. Assemble from pre-captured region .bin files (MCP-driven capture):
    python3 tools/equivalence/memsave_snapshot.py assemble \
        --plan artifacts/equivalence/plan_FUNC.json \
        --out  artifacts/equivalence/snap_FUNC.json

    # 4. Use it:
    python3 tools/equivalence/unicorn_diff.py FUNC --allow-stubs \
        --state-snapshot artifacts/equivalence/snap_FUNC.json --mem-trace

The snapshot is a {"regions": {hexaddr: hexbytes}} JSON, the exact format
state_snapshot.load_snapshot() / unicorn_diff --state-snapshot consume.
"""

import argparse
import json
import os
import re
import struct
import sys
import tempfile
from pathlib import Path

_SCRIPT_DIR = Path(__file__).resolve().parent
ROOT = _SCRIPT_DIR.parent.parent
sys.path.insert(0, str(_SCRIPT_DIR))

# Xbox virtual address regimes the guest actually maps.  memsave reads VAs
# directly, so these only bound which addresses are worth requesting.
_VA_RANGES = [
    (0x00010000, 0x04000000),   # XBE image, globals, low heap
    (0x80000000, 0x84000000),   # kernel-mirror heap
]

# Default window captured around each referenced global.  Slightly generous so
# a struct/array referenced by its base is captured whole.
_DEFAULT_WINDOW = 0x100


def _va_is_mappable(va: int) -> bool:
    return any(lo <= va < hi for lo, hi in _VA_RANGES)


# ---------------------------------------------------------------------------
# Reloc-driven region discovery
# ---------------------------------------------------------------------------

def discover_regions(target: str, window: int = _DEFAULT_WINDOW,
                     verbose: bool = False) -> list:
    """Return [(addr, size, label)] of global windows the target reads.

    Parses the target's DIR32 relocations from its delinked oracle .obj — the
    DAT_/PTR_-prefixed symbols encode the original Xbox VA the function loads
    from.  Reuses unicorn_diff's kb.json/objdiff resolution and coff_loader.
    """
    import unicorn_diff as U
    from coff_loader import extract_function, CoffParseError

    kb = U._load_kb()
    entry = U._find_kb_entry(kb, target)
    if entry is None:
        raise SystemExit(f"target {target!r} not found in kb.json")
    addr = entry.get("addr", "")
    delinked_path, _build_path = U._find_obj_paths(entry)
    if delinked_path is None:
        raise SystemExit(f"no delinked .obj for {target!r} (need a delinked reference)")

    # The function's symbol in the delinked obj is FUN_<addr> (zero-padded).
    addr_int = int(addr, 16)
    sym_candidates = [f"FUN_{addr_int:08x}", f"FUN_{addr_int:08X}", target]
    sl = None
    last_err = None
    for sym in sym_candidates:
        try:
            sl = extract_function(str(delinked_path), sym)
            break
        except (CoffParseError, KeyError, ValueError) as exc:
            last_err = exc
    if sl is None:
        raise SystemExit(f"could not extract {target} from {delinked_path}: {last_err}")

    seen = {}
    for r in sl.relocs:
        sym = r.symbol_name or ""
        m = re.match(r'(?:DAT|PTR|PTR_FUN|PTR_DAT|s)_([0-9a-fA-F]{4,})$', sym)
        if not m:
            continue
        va = int(m.group(1), 16)
        if not _va_is_mappable(va):
            continue
        # Align the window down so a base-referenced struct is captured whole.
        base = va & ~0xF
        if base not in seen:
            seen[base] = (base, window, sym)
            if verbose:
                print(f"  reloc {sym} -> VA 0x{va:08x}  window 0x{window:x}")

    regions = sorted(seen.values())
    if verbose:
        print(f"  {len(regions)} region(s) from {delinked_path.name}")
    return regions


def anchor_regions(window: int = 0x200) -> list:
    """Optional always-live global anchors worth following (pointer storage).

    These are image .data globals holding pointers into the heap; capturing the
    storage lets `capture --follow` chase the pointer to the real table.
    """
    return [
        (0x5aa6d4, 4, "player_data_ptr"),
        (0x456b60, 4, "current_game_engine_ptr"),
    ]


# ---------------------------------------------------------------------------
# memsave capture
# ---------------------------------------------------------------------------

def _win_path(p: Path) -> str:
    """Translate a /mnt/<drive>/ path to a Windows path for xemu's memsave."""
    s = str(p)
    m = re.match(r'/mnt/([a-zA-Z])/(.*)', s)
    if m:
        return f"{m.group(1).upper()}:\\" + m.group(2).replace("/", "\\")
    return s


def memsave_cmd(vaddr: int, size: int, out_bin: Path) -> str:
    """The HMP command line to capture [vaddr, vaddr+size) to out_bin."""
    return f"memsave 0x{vaddr:x} {size} {_win_path(out_bin)}"


def capture_via_monitor(plan: list, region_dir: Path, monitor_fn,
                        follow: bool = False, verbose: bool = False) -> dict:
    """Capture each planned region by invoking monitor_fn(command_line).

    monitor_fn must run an HMP command line against the live VM (e.g. QMP
    human-monitor-command, or the xemu MCP).  Returns {addr: bytes}.
    Pointer-following: if `follow`, any 4-byte region named '*_ptr' is
    dereferenced and the pointed-to window is captured too.
    """
    region_dir.mkdir(parents=True, exist_ok=True)
    out = {}
    work = list(plan)
    followed = set()
    while work:
        addr, size, label = work.pop(0)
        binp = region_dir / f"r_{addr:08x}_{size:x}.bin"
        monitor_fn(memsave_cmd(addr, size, binp))
        data = binp.read_bytes()[:size] if binp.exists() else b""
        if len(data) < size:
            data = data + b"\x00" * (size - len(data))
        out[addr] = data
        if verbose:
            print(f"  captured 0x{addr:08x} +0x{size:x} ({label}): {data[:8].hex()}")
        if follow and label.endswith("_ptr") and size >= 4 and addr not in followed:
            followed.add(addr)
            ptr = struct.unpack_from("<I", data, 0)[0]
            if _va_is_mappable(ptr):
                work.append((ptr, 0x400, f"{label}_target"))
    return out


def assemble_from_files(plan: list, region_dir: Path, verbose: bool = False) -> dict:
    """Assemble {addr: bytes} from region .bin files captured externally."""
    out = {}
    for addr, size, label in plan:
        binp = region_dir / f"r_{addr:08x}_{size:x}.bin"
        if not binp.exists():
            if verbose:
                print(f"  MISSING 0x{addr:08x} ({label}) — expected {binp}")
            continue
        data = binp.read_bytes()[:size]
        if len(data) < size:
            data = data + b"\x00" * (size - len(data))
        out[addr] = data
        if verbose:
            print(f"  loaded 0x{addr:08x} +0x{size:x} ({label}): {data[:8].hex()}")
    return out


def write_snapshot(regions: dict, target: str, out_path: Path) -> None:
    snap = {
        "description": f"Virtual (memsave) state snapshot for {target}",
        "source": "xemu:memsave (virtual addressing)",
        "regions": {f"0x{addr:08x}": data.hex() for addr, data in sorted(regions.items())},
    }
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(snap))
    total = sum(len(d) for d in regions.values())
    print(f"  wrote {out_path} ({len(regions)} regions, {total} bytes)")


def write_plan(plan: list, target: str, region_dir: Path, out_path: Path) -> None:
    obj = {
        "target": target,
        "region_dir": str(region_dir),
        "regions": [{"addr": f"0x{a:08x}", "size": s, "label": l} for a, s, l in plan],
        "memsave_commands": [
            memsave_cmd(a, s, region_dir / f"r_{a:08x}_{s:x}.bin") for a, s, l in plan
        ],
    }
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(obj, indent=2))
    print(f"  wrote plan {out_path} ({len(plan)} regions)")
    print("  memsave commands (run via xemu monitor/MCP if QMP is held elsewhere):")
    for c in obj["memsave_commands"]:
        print(f"    {c}")


def load_plan(path: Path) -> tuple:
    obj = json.loads(Path(path).read_text())
    plan = [(int(r["addr"], 16), int(r["size"]), r["label"]) for r in obj["regions"]]
    region_dir = Path(obj.get("region_dir") or (ROOT / "artifacts/equivalence/regions"))
    return plan, region_dir, obj.get("target", "unknown")


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def _qmp_monitor_fn():
    """Return a monitor_fn backed by a direct QMP connection (CI path)."""
    from game_state_snapshot import qmp_connect
    sess = qmp_connect("localhost")

    def run(cmdline: str):
        sess.command("human-monitor-command", {"command-line": cmdline})

    return run, sess


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)

    p_plan = sub.add_parser("plan", help="Discover regions + emit capture plan")
    p_plan.add_argument("--target", "-t", required=True)
    p_plan.add_argument("--window", type=lambda x: int(x, 0), default=_DEFAULT_WINDOW)
    p_plan.add_argument("--anchors", action="store_true",
                        help="Include player_data/game_engine pointer anchors")
    p_plan.add_argument("--region-dir", type=Path,
                        default=ROOT / "artifacts/equivalence/regions")
    p_plan.add_argument("--out", "-o", type=Path, required=True)

    p_cap = sub.add_parser("capture", help="Capture via direct QMP memsave (CI)")
    p_cap.add_argument("--plan", type=Path, required=True)
    p_cap.add_argument("--follow", action="store_true",
                       help="Dereference *_ptr regions and capture their targets")
    p_cap.add_argument("--out", "-o", type=Path, required=True)
    p_cap.add_argument("--verbose", "-v", action="store_true")

    p_asm = sub.add_parser("assemble", help="Assemble snapshot from region .bin files")
    p_asm.add_argument("--plan", type=Path, required=True)
    p_asm.add_argument("--out", "-o", type=Path, required=True)
    p_asm.add_argument("--verbose", "-v", action="store_true")

    args = ap.parse_args()

    if args.cmd == "plan":
        plan = discover_regions(args.target, window=args.window, verbose=True)
        if args.anchors:
            plan = sorted(set(plan) | set(anchor_regions()))
        write_plan(plan, args.target, args.region_dir, args.out)
        return

    if args.cmd == "capture":
        plan, region_dir, target = load_plan(args.plan)
        run, sess = _qmp_monitor_fn()
        try:
            regions = capture_via_monitor(plan, region_dir, run,
                                          follow=args.follow, verbose=args.verbose)
        finally:
            try:
                sess.close()
            except Exception:
                pass
        write_snapshot(regions, target, args.out)
        return

    if args.cmd == "assemble":
        plan, region_dir, target = load_plan(args.plan)
        regions = assemble_from_files(plan, region_dir, verbose=args.verbose)
        write_snapshot(regions, target, args.out)
        return


if __name__ == "__main__":
    main()
