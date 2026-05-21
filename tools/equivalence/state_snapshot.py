"""State snapshot capture and replay for Unicorn differential testing.

Captures memory regions from a running xemu instance (via QMP pmemsave)
and saves them as JSON.  Snapshots can be loaded into Unicorn as initial
memory state, replacing the default zero-fill for game-state-dependent
functions.

Format:
    {
      "description": "multiplayer lobby, 4 teams",
      "captured_at": "2026-05-15T12:00:00",
      "regions": {
        "0x00456600": "aabbccdd...",
        "0x00480000": "00112233..."
      }
    }
"""

import json
import os
import struct
import subprocess
import tempfile
from datetime import datetime, timezone
from pathlib import Path
from typing import Optional


def load_snapshot(path: str) -> tuple[dict, dict]:
    """Load a snapshot JSON file.

    Returns (memory_overrides, arg_overrides) where:
      memory_overrides: {int_address: bytes} for unicorn memory setup
      arg_overrides: {param_name: value_or_range} for seed constraining
        - Fixed: {"eax": 0x500000} — every seed uses this value
        - Range: {"param_1": [1, 10]} — seeder picks from [lo, hi]
    """
    with open(path, encoding="utf-8") as f:
        data = json.load(f)

    regions = data.get("regions", {})
    mem = {}
    for addr_hex, val_hex in regions.items():
        addr = int(addr_hex, 16)
        mem[addr] = bytes.fromhex(val_hex)

    arg_overrides = data.get("arg_overrides", {})
    return mem, arg_overrides


def save_snapshot(regions: dict, path: str, description: str = ""):
    """Save memory regions to a snapshot JSON file.

    regions: {int_address: bytes}
    """
    out = {
        "description": description,
        "captured_at": datetime.now(timezone.utc).isoformat(),
        "regions": {},
    }
    for addr in sorted(regions):
        out["regions"][f"0x{addr:08x}"] = regions[addr].hex()

    Path(path).parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(out, f, indent=2)
        f.write("\n")


def capture_from_xemu(addresses: list,
                      output_path: Optional[str] = None,
                      description: str = "") -> dict:
    """Capture memory from a running xemu instance via QMP pmemsave.

    addresses: list of (virtual_addr, size) tuples to capture.
    output_path: if set, also save to this JSON file.

    Returns {int_address: bytes}.

    Requires xemu to be running with QMP enabled. Uses the QMP
    'human-monitor-command' to issue pmemsave, which dumps physical
    memory to a host file that we then read back.

    Note: pmemsave uses *physical* addresses.  For Xbox, virtual == physical
    in the flat memory model used by the game (identity-mapped first 64MB).
    """
    _REPO_ROOT = Path(__file__).resolve().parent.parent.parent
    qmp_mod = _REPO_ROOT / "tools" / "xbox" / "xemu_qmp.py"

    if not qmp_mod.exists():
        raise RuntimeError(f"xemu QMP module not found: {qmp_mod}")

    import importlib.util
    spec = importlib.util.spec_from_file_location("xemu_qmp", str(qmp_mod))
    xemu_qmp = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(xemu_qmp)

    session = xemu_qmp.discover_session()
    if session is None:
        raise RuntimeError("No running xemu QMP session found")

    regions = {}
    for base_addr, size in addresses:
        with tempfile.NamedTemporaryFile(suffix=".bin", delete=False) as tmp:
            tmp_path = tmp.name

        try:
            cmd = f"pmemsave {base_addr:#x} {size} {tmp_path}"
            session.command("human-monitor-command",
                            {"command-line": cmd})
            with open(tmp_path, "rb") as f:
                data = f.read()
            if len(data) >= size:
                regions[base_addr] = data[:size]
        finally:
            try:
                os.unlink(tmp_path)
            except OSError:
                pass

    if output_path:
        save_snapshot(regions, output_path, description=description)

    return regions
