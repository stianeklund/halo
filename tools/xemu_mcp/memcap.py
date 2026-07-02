r"""Atomic batched memory capture over the MCP's own QMP connection.

Ports the proven capture primitive from tools/equivalence/qmp_capture.py, but
issues every command over the shared MCP QmpClient (NOT a second raw socket), so
it never grabs a second :4444 client slot.

Recipe (reference_xemu_qmp_memsave_capture):
  * `memsave` is VIRTUAL; never `pmemsave` (physical reads the wrong bytes on
    this Cerbios / kernel-irqchip=off box — the game heap lives at 0x80xxxxxx),
  * the HMP `memsave` command-line needs DOUBLED backslashes in the Windows path
    (HMP unescapes `\\` -> `\`),
  * `stop` -> memsave the whole region set -> `cont` is atomic by construction:
    nothing advances while paused, and pausing does not perturb tick determinism.
"""

import asyncio
import os
import struct
import tempfile
import time
from pathlib import Path
from typing import Iterable

SETTLE_S = 1.5


def wsl_to_win(path: Path) -> str:
    """/mnt/g/dev/... -> G:\\dev\\... (single-backslash Windows path)."""
    p = path.resolve()
    parts = p.parts
    if len(parts) >= 3 and parts[0] == "/" and parts[1] == "mnt" and len(parts[2]) == 1:
        drive = parts[2].upper()
        rest = "\\".join(parts[3:])
        return f"{drive}:\\{rest}"
    raise ValueError(f"not a /mnt/<drive>/ path, cannot map to Windows: {path}")


def _scratch_dir() -> Path:
    # Prefer the repo tmp/ (a real /mnt/<drive>/ path so wsl_to_win works);
    # fall back to the system temp dir if that isn't under /mnt/<drive>/.
    repo_tmp = Path(__file__).resolve().parent.parent.parent / "tmp" / "xemu_mcp_memcap"
    try:
        wsl_to_win(repo_tmp)
        repo_tmp.mkdir(parents=True, exist_ok=True)
        return repo_tmp
    except ValueError:
        d = Path(tempfile.gettempdir()) / "xemu_mcp_memcap"
        d.mkdir(parents=True, exist_ok=True)
        return d


async def _read_file_settle(wsl: Path, size: int) -> bytes:
    """Await the memsave file appearing with >= size bytes, then read it."""
    deadline = time.monotonic() + SETTLE_S
    while True:
        try:
            data = wsl.read_bytes()
            if len(data) >= size:
                try:
                    wsl.unlink()
                except OSError:
                    pass
                return data[:size]
        except FileNotFoundError:
            pass
        if time.monotonic() > deadline:
            raise RuntimeError(f"memsave produced no/short file: {wsl}")
        await asyncio.sleep(0.01)


async def read_regions(qmp, regions: Iterable[tuple[int, int]]) -> dict[int, bytes]:
    """Atomically capture [(addr, size), ...]; returns {addr: bytes}.

    Issues stop -> memsave(all) -> cont over the shared MCP QMP connection, then
    reads the files back (their content already reflects the frozen frame).
    """
    regions = list(regions)
    scratch = _scratch_dir()
    pending: list[tuple[int, Path, int]] = []

    await qmp.execute("stop")
    try:
        for i, (addr, size) in enumerate(regions):
            wsl = scratch / f"ms_{os.getpid()}_{i}_{addr:x}.bin"
            try:
                wsl.unlink()  # avoid reading stale bytes
            except FileNotFoundError:
                pass
            win = wsl_to_win(wsl).replace("\\", "\\\\")  # HMP unescapes \\ -> \
            await qmp.execute(
                "human-monitor-command",
                {"command-line": f'memsave 0x{addr:x} {size} "{win}"'},
            )
            pending.append((addr, wsl, size))
    finally:
        # Always resume, even if a memsave raised mid-window.
        await qmp.execute("cont")

    out: dict[int, bytes] = {}
    for addr, wsl, size in pending:
        out[addr] = await _read_file_settle(wsl, size)
    return out


def read_u32(regions: dict[int, bytes], addr: int) -> int:
    return struct.unpack("<I", regions[addr][:4])[0]
