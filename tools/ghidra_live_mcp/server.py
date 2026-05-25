#!/usr/bin/env python3

import asyncio
import os
import sys
import time

_tools_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)

from mcp_server import (
    export_delinked_object as export_delinked_object_rpc,
    get_current_program as get_current_program_rpc,
    get_current_selection as get_current_selection_rpc,
    get_last_export_status as get_last_export_status_rpc,
    list_symbols_in_range as list_symbols_in_range_rpc,
    run_relocation_synthesizer as run_relocation_synthesizer_rpc,
)

_SESSION_GC_S = 60

_active_sessions = 0
_next_session_id = 0
_sessions_pruned_total = 0


def _log(msg: str) -> None:
    print(f"[ghidra-live] {msg}", file=sys.stderr, flush=True)


def _cleanup_stale_writers(transport) -> None:
    writers = getattr(transport, "_read_stream_writers", {})
    stale = [sid for sid, w in writers.items() if getattr(w, "_closed", False)]
    for sid in stale:
        del writers[sid]
    if stale:
        _log(f"Cleaned {len(stale)} stale session entries ({len(writers)} remaining)")


async def _session_gc_loop(sse_transport) -> None:
    global _sessions_pruned_total
    while True:
        await asyncio.sleep(_SESSION_GC_S)
        writers = getattr(sse_transport, "_read_stream_writers", {})
        before = len(writers)
        stale = [
            sid for sid, w in writers.items()
            if getattr(w, "_closed", False)
        ]
        for sid in stale:
            del writers[sid]
        after = len(writers)
        if stale:
            _sessions_pruned_total += len(stale)
            _log(
                f"Session GC: pruned {len(stale)} stale sessions "
                f"({before}->{after}, total={_sessions_pruned_total})"
            )


class _SSEEndpoint:
    """ASGI endpoint with session lifecycle tracking and stale-writer cleanup."""

    def __init__(self, mcp_server, sse_transport):
        self._mcp_server = mcp_server
        self._sse_transport = sse_transport

    async def __call__(self, scope, receive, send):
        global _active_sessions, _next_session_id
        _next_session_id += 1
        session_id = _next_session_id
        headers_raw = scope.get("headers", [])
        headers = {
            k.decode("latin-1", errors="replace"): v.decode("latin-1", errors="replace")
            for k, v in headers_raw
        }
        client = scope.get("client") or ("?", "?")
        client_host = client[0]
        user_agent = headers.get("user-agent", "?")
        _active_sessions += 1
        _log(
            f"Session {session_id} connected "
            f"({client_host}, ua={user_agent!r}, {_active_sessions} active)"
        )
        try:
            async with self._sse_transport.connect_sse(scope, receive, send) as streams:
                await self._mcp_server.run(
                    streams[0],
                    streams[1],
                    self._mcp_server.create_initialization_options(),
                )
        finally:
            _active_sessions -= 1
            _log(f"Session {session_id} disconnected ({_active_sessions} active)")
            _cleanup_stale_writers(self._sse_transport)


def main():
    try:
        from mcp.server.fastmcp import FastMCP
    except ImportError as exc:
        raise SystemExit(
            "Missing MCP Python package. Install an MCP SDK that provides "
            "mcp.server.fastmcp.FastMCP before running this server."
        ) from exc

    mcp = FastMCP("ghidra-live")

    @mcp.tool()
    def get_current_program():
        return get_current_program_rpc()

    @mcp.tool()
    def get_current_selection():
        return get_current_selection_rpc()

    @mcp.tool()
    def run_relocation_synthesizer(selection_mode: str, range: str = ""):
        """Run relocation synthesizer on a range or current selection.

        Args:
            selection_mode: "range", "symbol", or "current_selection"
            range: Address range as "START-END" (hex, no 0x prefix), e.g. "0010b240-0010b8e0"
        """
        return run_relocation_synthesizer_rpc(
            selection_mode=selection_mode, range=range or None
        )

    @mcp.tool()
    def export_delinked_object(
        export_path: str,
        exporter_name: str = "COFF relocatable object",
        selection_mode: str = "current_selection",
        range: str = "",
        run_relocation_synthesizer: bool = True,
    ):
        """Export a delinked COFF object from Ghidra.

        Args:
            export_path: Windows path for the output .o file (e.g. G:\\dev\\halo\\artifacts\\delinker\\foo.o)
            exporter_name: Exporter name, usually "COFF relocatable object"
            selection_mode: "range", "symbol", or "current_selection"
            range: Address range as "START-END" (hex, no 0x prefix)
            run_relocation_synthesizer: Run relocation synthesizer before export
        """
        return export_delinked_object_rpc(
            export_path=export_path,
            exporter_name=exporter_name,
            selection_mode=selection_mode,
            range=range or None,
            run_relocation_synthesizer=run_relocation_synthesizer,
        )

    @mcp.tool()
    def list_symbols_in_range(range: str):
        """List all symbols in an address range.

        Args:
            range: Address range as "START-END" (hex, no 0x prefix), e.g. "0010b240-0010b8e0"
        """
        return list_symbols_in_range_rpc(range=range)

    @mcp.tool()
    def get_last_export_status():
        return get_last_export_status_rpc()

    transport = "sse"
    host = "127.0.0.1"
    port = 8091
    for arg in sys.argv[1:]:
        if arg.startswith("--port="):
            port = int(arg.split("=", 1)[1])
        elif arg.startswith("--host="):
            host = arg.split("=", 1)[1]
        elif arg == "--stdio":
            transport = "stdio"
    mcp.settings.host = host
    mcp.settings.port = port
    if transport == "stdio":
        mcp.run(transport=transport)
        return

    from mcp.server.sse import SseServerTransport
    from starlette.applications import Starlette
    from starlette.responses import JSONResponse
    from starlette.routing import Mount, Route
    import uvicorn

    sse = SseServerTransport("/messages/")

    async def health(request):
        writers = len(getattr(sse, "_read_stream_writers", {}))
        return JSONResponse(
            {
                "status": "up",
                "active_sessions": _active_sessions,
                "sdk_writers": writers,
                "tools": 6,
                "sessions_pruned_total": _sessions_pruned_total,
            }
        )

    app = Starlette(
        routes=[
            Route("/sse", endpoint=_SSEEndpoint(mcp._mcp_server, sse), methods=["GET"]),
            Mount("/messages/", app=sse.handle_post_message),
            Route("/health", endpoint=health, methods=["GET"]),
        ],
    )

    _log(f"ghidra-live MCP listening on http://{host}:{port}/sse")
    _log(f"Health endpoint: http://{host}:{port}/health")

    async def run_async():
        asyncio.ensure_future(_session_gc_loop(sse))
        config = uvicorn.Config(app, host=host, port=port, log_level="warning")
        srv = uvicorn.Server(config)
        await srv.serve()

    try:
        asyncio.run(run_async())
    except KeyboardInterrupt:
        _log("Shutting down.")


if __name__ == "__main__":
    main()
