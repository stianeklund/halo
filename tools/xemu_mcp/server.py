#!/usr/bin/env python3
"""xemu MCP server (Python / FastMCP, Streamable HTTP transport).

One long-lived daemon owns a single QMP connection to one xemu VM and serves many
concurrent MCP clients over Streamable HTTP. This replaces the original per-session
Node/stdio server, whose per-process QMP sockets collided (xemu's QMP accepts one
client at a time).

Module-level singletons (cfg / qmp / proc) ARE the shared VM: every session's tool
call routes through the same QMP client, which serializes commands over the one
socket. Tools hold no per-connection state.

Tool names and QMP commands are ported verbatim from the Node project at
/mnt/g/dev/xemu_mcp (src/server.ts) so the existing mcp__xemu__* contract is
unchanged.

Launched by tools/shell/mcp-servers.sh via the repo .venv. Sibling modules
(config/qmp/process) are imported by plain name, matching how the other Python MCP
servers in this repo are launched (direct script path puts this dir on sys.path).
"""

import base64
import datetime
import json
import logging
import os

from starlette.requests import Request
from starlette.responses import JSONResponse

from mcp.server.fastmcp import FastMCP

from config import load_config
from qmp import QmpClient, QmpError
from process import XemuProcessManager
from memcap import read_regions

cfg = load_config()
logging.basicConfig(level=getattr(logging, cfg.log_level, logging.INFO))
logger = logging.getLogger("xemu-mcp")

# Shared, process-wide state == one shared VM for all sessions.
qmp = QmpClient(cfg.qmp_host, cfg.qmp_port, idle_disconnect_s=cfg.idle_disconnect_s)
proc = XemuProcessManager(cfg)

mcp = FastMCP(
    name="xemu",
    host="127.0.0.1",
    port=cfg.http_port,
    # Stateful streamable-HTTP (the canonical, most client-compatible mode): the
    # server issues an mcp-session-id and keeps the standalone GET /mcp SSE stream
    # open. Stateless mode closed that stream immediately, which OpenCode reports as
    # "-32000 Connection closed". Sessions are MCP-protocol-level only; the VM stays
    # shared across all sessions via the module-level qmp/proc singletons below.
)


@mcp.custom_route("/health", methods=["GET"])
async def health(_request: Request) -> JSONResponse:
    return JSONResponse(
        {
            "ok": True,
            "qmp": f"{cfg.qmp_host}:{cfg.qmp_port}",
            "connected": qmp.is_connected(),
            "xemu_running": proc.is_running(),
        }
    )


@mcp.tool()
async def xemu_status() -> str:
    """Get current status of xemu and QMP connection."""
    reachable = False
    vm_status = "unknown"

    # connect-on-demand: query-status establishes (or reuses) the connection and
    # tolerates a stale socket via the client's built-in retry. No throwaway
    # probe connection — that grabbed the single :4444 slot and fought the Rust
    # viewer / raw-socket tools.
    try:
        res = await qmp.execute("query-status")
        vm_status = res.get("status", vm_status)
        reachable = True
    except Exception:
        reachable = False

    status = {
        "connected": qmp.is_connected(),
        "reachable": reachable,
        "status": vm_status,
        "instanceAttached": qmp.is_connected(),
        "launchedByMcp": proc.is_launched_by_us(),
    }
    return json.dumps(status, indent=2)


@mcp.tool()
async def xemu_connect() -> str:
    """Connect to an existing xemu QMP endpoint."""
    await qmp.connect()
    await qmp.execute("query-status")
    return "Successfully connected to xemu QMP."


@mcp.tool()
async def xemu_launch(extraArgs: list[str] | None = None) -> str:
    """Launch xemu with QMP enabled."""
    if proc.is_process_alive():
        return "xemu is already running."

    proc.launch(extraArgs or [])

    import asyncio

    retries = 10
    while retries > 0:
        try:
            await qmp.connect()
            break
        except Exception:
            retries -= 1
            await asyncio.sleep(1.0)

    if not qmp.is_connected():
        return "xemu launched but QMP failed to connect."

    await qmp.execute("query-status")
    return "xemu launched and QMP connected successfully."


@mcp.tool()
async def xemu_pause() -> str:
    """Pause the VM."""
    await qmp.execute("stop")
    return "VM paused."


@mcp.tool()
async def xemu_resume() -> str:
    """Resume the VM."""
    await qmp.execute("cont")
    return "VM resumed."


@mcp.tool()
async def xemu_reset() -> str:
    """Reset the VM."""
    await qmp.execute("system_reset")
    return "VM reset initiated."


@mcp.tool()
async def xemu_stop() -> str:
    """Stop/quit xemu."""
    await qmp.execute("quit")
    qmp.disconnect()
    proc.stop()
    return "xemu stopped."


@mcp.tool()
async def xemu_load_iso(path: str) -> str:
    """Load an ISO file into the CD-ROM drive."""
    block_devices = await qmp.execute("query-block")
    cdrom = None
    for d in block_devices or []:
        device = d.get("device", "")
        qdev = d.get("qdev", "")
        if "cd" in device or (qdev and "cd" in qdev):
            cdrom = d
            break

    if cdrom is None:
        raise RuntimeError(
            "CD-ROM device not found. Ensure xemu is launched with a CD-ROM drive."
        )

    dev_id = cdrom.get("qdev") or cdrom.get("device")
    await qmp.execute(
        "blockdev-change-medium",
        {"id": dev_id, "filename": path, "format": "raw"},
    )
    return f"ISO loaded into {dev_id}: {path}"


@mcp.tool()
async def xemu_screenshot(filename: str | None = None) -> str:
    """Take a PNG screenshot. If filename is omitted, a timestamped path is
    auto-generated in XEMU_SCREENSHOT_DIR (or the system temp dir if unset)."""
    if filename is None:
        ts = datetime.datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
        filename = os.path.join(cfg.screenshot_target_dir(), f"xemu-{ts}.png")

    try:
        await qmp.execute("screendump", {"filename": filename, "format": "png"})
    except QmpError as e:
        if "CommandNotFound" in str(e):
            response = await qmp.execute(
                "human-monitor-command",
                {"command-line": f"screendump {filename}"},
            )
            if isinstance(response, str) and response.strip():
                raise RuntimeError(f"HMP screendump failed: {response.strip()}")
        else:
            raise
    return f"Screenshot saved to {filename}"


@mcp.tool()
async def xemu_send_monitor_command(command: str) -> str:
    """Execute a human monitor command."""
    blocked = {"quit", "exit"}
    if command.strip().lower() in blocked:
        raise RuntimeError(
            f"Command '{command}' is blocked for safety. Use xemu_stop instead."
        )
    res = await qmp.execute("human-monitor-command", {"command-line": command})
    return res if isinstance(res, str) else json.dumps(res)


# Cap total bytes per call so the base64 response stays reasonable for the model.
_READ_MEMORY_MAX_TOTAL = 256 * 1024


@mcp.tool()
async def xemu_read_memory(regions: list) -> str:
    """Atomically read guest VIRTUAL memory regions in one paused window.

    Pauses the VM, memsaves every region, resumes, then returns the bytes — so
    all regions are coherent with a single frozen frame (the right primitive for
    reading object pools / linked structures). Uses virtual `memsave` only
    (physical `pmemsave` reads wrong bytes on this box).

    Args:
      regions: list of [address, size] pairs. address may be an int or a hex/dec
        string (e.g. "0x80146d88" or 2149559176); size is a byte count.

    Returns JSON: {"0x<addr>": "<base64 bytes>", ...} plus a "_meta" entry with
    the per-region byte counts.
    """
    parsed: list[tuple[int, int]] = []
    total = 0
    for r in regions:
        if not isinstance(r, (list, tuple)) or len(r) != 2:
            raise RuntimeError(f"each region must be [address, size]; got {r!r}")
        addr_raw, size = r
        addr = int(addr_raw, 0) if isinstance(addr_raw, str) else int(addr_raw)
        size = int(size)
        if size <= 0:
            raise RuntimeError(f"region size must be positive; got {size}")
        total += size
        parsed.append((addr, size))
    if total > _READ_MEMORY_MAX_TOTAL:
        raise RuntimeError(
            f"total requested {total} bytes exceeds cap {_READ_MEMORY_MAX_TOTAL}; "
            f"split into fewer/smaller regions"
        )

    data = await read_regions(qmp, parsed)
    out = {f"0x{addr:08x}": base64.b64encode(data[addr]).decode("ascii") for addr, _ in parsed}
    out["_meta"] = {f"0x{addr:08x}": len(data[addr]) for addr, _ in parsed}
    return json.dumps(out)


@mcp.tool()
async def xemu_query_state() -> str:
    """Query comprehensive VM state (status, version, commands)."""
    status = await qmp.execute("query-status")
    version = await qmp.execute("query-version")
    commands = await qmp.execute("query-commands")
    return json.dumps(
        {
            "status": status,
            "version": version,
            "commands": [c.get("name") for c in (commands or [])],
        },
        indent=2,
    )


@mcp.tool()
async def xemu_attach_gdb(port: int = 1234) -> str:
    """Start a GDB server for remote debugging."""
    result = await qmp.execute(
        "human-monitor-command", {"command-line": f"gdbserver tcp::{port}"}
    )
    if result and result.strip():
        raise RuntimeError(f"gdbserver failed: {result.strip()}")
    return f"GDB server listening on tcp::{port}"


@mcp.tool()
async def xemu_read_serial(chardev: str = "serial0") -> str:
    """Read from a serial chardev (requires ringbuf)."""
    try:
        res = await qmp.execute("ringbuf-read", {"device": chardev, "size": 1024})
        return res if isinstance(res, str) else json.dumps(res)
    except Exception as e:
        if "not found" in str(e):
            raise RuntimeError(
                f"Serial chardev '{chardev}' not found or not configured as a "
                f"ringbuf. Use -serial ringbuf,id={chardev} when launching."
            )
        raise


if __name__ == "__main__":
    logger.info(
        "xemu MCP server listening on http://127.0.0.1:%d/mcp (QMP %s:%d)",
        cfg.http_port,
        cfg.qmp_host,
        cfg.qmp_port,
    )
    mcp.run(transport="streamable-http")
