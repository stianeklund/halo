"""State snapshot capture and replay for Unicorn differential testing.

Captures memory regions from a running xemu instance (via QMP virtual memsave)
or XBDM getmem and saves them as JSON. Snapshots can be loaded into Unicorn as initial
memory state, replacing the default zero-fill for game-state-dependent
functions.

Format:
    {
      "description": "multiplayer lobby, 4 teams",
      "captured_at": "2026-05-15T12:00:00",
      "build_label": "patched" | "original" | "commit abc123",
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

OBJECT_TABLE_PTR = 0x5A8D50
DATA_T_MAGIC = 0x64407440
DATA_T_MAGIC_OFFSET = 0x28
_REPO_ROOT = Path(__file__).resolve().parent.parent.parent
_SCRATCH_DIR = _REPO_ROOT / "tmp" / "state_snapshot"


def load_snapshot(path: str) -> tuple[dict, dict, dict]:
    """Load a snapshot JSON file.

    Returns (memory_overrides, arg_overrides, stub_returns) where:
      memory_overrides: {int_address: bytes} for unicorn memory setup
      arg_overrides: {param_name: value_or_range} for seed constraining
        - Fixed: {"eax": 0x500000} — every seed uses this value
        - Range: {"param_1": [1, 10]} — seeder picks from [lo, hi]
      stub_returns: {callee_name: int_or_float} — deterministic return for a
        stubbed callee (applied identically to oracle and candidate); lets a
        run open paths gated on a stub's pointer/handle result, e.g.
        {"FUN_0017dc70": 0x780000} with a synthetic block at 0x780000. A JSON
        float value is preserved as float and returned in ST0 by
        float-returning stubs (PUSH imm32 + FLD trampoline in stubs.py).
        A JSON list value gives SEQUENCED returns: the callee returns
        list[0] on its first call, list[1] on the second, ..., repeating
        the last element past the end (natural for -1 loop terminators).
        Sequences are served dynamically and reset per run, so oracle and
        candidate replay the identical sequence.
    """
    with open(path, encoding="utf-8") as f:
        data = json.load(f)

    build_label = data.get("build_label", "unknown")
    captured = data.get("captured_at", "unknown")
    desc = data.get("description", "")
    print(f"  [snapshot] {desc}  build={build_label}  captured={captured}")

    regions = data.get("regions", {})
    mem = {}
    for addr_hex, val_hex in regions.items():
        addr = int(addr_hex, 16)
        mem[addr] = bytes.fromhex(val_hex)

    arg_overrides = data.get("arg_overrides", {})

    def _coerce(v):
        if isinstance(v, list):
            # Sequenced returns: one value per call, last value repeats.
            return [x if isinstance(x, float) else int(x) for x in v]
        return v if isinstance(v, float) else int(v)

    stub_returns = {str(k).lstrip("_").lower(): _coerce(v)
                    for k, v in data.get("stub_returns", {}).items()}
    return mem, arg_overrides, stub_returns


def save_snapshot(regions: dict, path: str, description: str = "",
                  build_label: str = "", verified: bool = False):
    """Save memory regions to a snapshot JSON file.

    regions: {int_address: bytes}
    """
    if not verified:
        raise RuntimeError(
            "refusing to write unverified state snapshot; capture must pass "
            "the active-gameplay datum magic check"
        )
    out = {
        "description": description,
        "captured_at": datetime.now(timezone.utc).isoformat(),
        "verified": True,
        "regions": {},
    }
    if build_label:
        out["build_label"] = build_label
    for addr in sorted(regions):
        out["regions"][f"0x{addr:08x}"] = regions[addr].hex()

    Path(path).parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(out, f, indent=2)
        f.write("\n")


def _win_path(path: str) -> str:
    p = Path(path).resolve()
    parts = p.parts
    if len(parts) >= 3 and parts[0] == "/" and parts[1] == "mnt" and len(parts[2]) == 1:
        drive = parts[2].upper()
        rest = "\\".join(parts[3:])
        return f"{drive}:\\{rest}"
    return str(p)


def _qmp_memsave(session, base_addr: int, size: int) -> bytes:
    _SCRATCH_DIR.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(suffix=".bin", dir=str(_SCRATCH_DIR), delete=False) as tmp:
        tmp_path = tmp.name
    try:
        cmd = f"memsave {base_addr:#x} {size} {_win_path(tmp_path)}"
        session.command("human-monitor-command", {"command-line": cmd})
        with open(tmp_path, "rb") as f:
            return f.read()
    finally:
        try:
            os.unlink(tmp_path)
        except OSError:
            pass


def _xbdm_getmem(rdcp_script: Path, root: Path, host: str, port: int,
                 timeout: float, base_addr: int, size: int) -> bytes:
    proc = subprocess.run(
        [
            "python3",
            str(rdcp_script),
            "--host",
            host,
            "--port",
            str(port),
            "--timeout",
            str(timeout),
            "--json",
            f"getmem addr={base_addr:#x} length={size:#x}",
        ],
        cwd=str(root),
        capture_output=True,
        text=True,
    )
    if proc.returncode != 0:
        raise RuntimeError(proc.stderr.strip() or proc.stdout.strip())
    response = json.loads(proc.stdout)
    return bytes.fromhex("".join(response.get("lines", [])))


def _verify_object_table_magic(read_mem) -> None:
    ptr_bytes = read_mem(OBJECT_TABLE_PTR, 4)
    if len(ptr_bytes) < 4:
        raise RuntimeError("active-gameplay magic check failed: object table pointer unreadable")
    object_table = struct.unpack_from("<I", ptr_bytes, 0)[0]
    if object_table == 0:
        raise RuntimeError("active-gameplay magic check failed: object table pointer is zero")
    magic_bytes = read_mem(object_table + DATA_T_MAGIC_OFFSET, 4)
    if len(magic_bytes) < 4:
        raise RuntimeError("active-gameplay magic check failed: object table header unreadable")
    magic = struct.unpack_from("<I", magic_bytes, 0)[0]
    if magic != DATA_T_MAGIC:
        raise RuntimeError(
            f"active-gameplay magic check failed: object table magic "
            f"0x{magic:08x} != 0x{DATA_T_MAGIC:08x}"
        )


def capture_from_xemu(addresses: list,
                      output_path: Optional[str] = None,
                      description: str = "") -> dict:
    """Capture memory from a running xemu instance via QMP virtual memsave.

    addresses: list of (virtual_addr, size) tuples to capture.
    output_path: if set, also save to this JSON file.

    Returns {int_address: bytes}.

    Requires xemu to be running with QMP enabled. Uses HMP `memsave`, which
    dumps the guest virtual address space. Physical `pmemsave` is intentionally
    not used here because it reads the wrong bytes on this setup.
    """
    qmp_mod = _REPO_ROOT / "tools" / "xbox" / "xemu_qmp.py"

    if not qmp_mod.exists():
        raise RuntimeError(f"xemu QMP module not found: {qmp_mod}")

    import importlib.util
    spec = importlib.util.spec_from_file_location("xemu_qmp", str(qmp_mod))
    xemu_qmp = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(xemu_qmp)

    # Compatibility with both discover_session() signatures:
    # legacy: discover_session()
    # current: discover_session(host, port=None)
    discover = xemu_qmp.discover_session
    try:
        session = discover()
    except TypeError:
        host = getattr(xemu_qmp, "DEFAULT_HOST", "localhost")
        port = getattr(xemu_qmp, "DEFAULT_PORT", None)
        session = discover(host, port)
    if session is None:
        raise RuntimeError("No running xemu QMP session found")

    _verify_object_table_magic(lambda addr, size: _qmp_memsave(session, addr, size))

    regions = {}
    for base_addr, size in addresses:
        data = _qmp_memsave(session, base_addr, size)
        if len(data) >= size:
            regions[base_addr] = data[:size]

    if output_path:
        save_snapshot(regions, output_path, description=description, verified=True)

    return regions


def capture_from_xbdm(addresses: list,
                      output_path: Optional[str] = None,
                      description: str = "",
                      host: str = "localhost",
                      port: int = 731,
                      timeout: float = 5.0) -> dict:
    """Capture memory from a running Xbox/xemu instance via XBDM getmem.

    This is a transport fallback for environments where xemu is reachable via
    XBDM but was not launched with QMP. Prefer capture_from_xemu for xemu
    virtual memsave captures when QMP is available.
    """
    tools_dir = _REPO_ROOT / "tools"

    rdcp_script = tools_dir / "xbox" / "xbdm_rdcp.py"

    _verify_object_table_magic(
        lambda addr, size: _xbdm_getmem(rdcp_script, _REPO_ROOT, host, port, timeout, addr, size)
    )

    regions = {}
    for base_addr, size in addresses:
        data = _xbdm_getmem(rdcp_script, _REPO_ROOT, host, port, timeout, base_addr, size)
        if len(data) >= size:
            regions[base_addr] = data[:size]

    if output_path:
        save_snapshot(regions, output_path, description=description, verified=True)

    return regions
