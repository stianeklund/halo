#!/usr/bin/env python3

import os
import sys
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


class _SSEEndpoint:
    """ASGI endpoint wrapper to avoid duplicate response emission on Starlette 1.x."""

    def __init__(self, mcp_server, sse_transport):
        self._mcp_server = mcp_server
        self._sse_transport = sse_transport

    async def __call__(self, scope, receive, send):
        async with self._sse_transport.connect_sse(scope, receive, send) as streams:
            await self._mcp_server.run(
                streams[0],
                streams[1],
                self._mcp_server.create_initialization_options(),
            )


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
    from starlette.routing import Mount, Route
    import uvicorn

    sse = SseServerTransport("/messages/")
    app = Starlette(
        routes=[
            Route("/sse", endpoint=_SSEEndpoint(mcp._mcp_server, sse), methods=["GET"]),
            Mount("/messages/", app=sse.handle_post_message),
        ],
    )
    print(f"ghidra-live MCP listening on http://{host}:{port}/sse", file=sys.stderr)
    config = uvicorn.Config(app, host=host, port=port, log_level="warning")
    server = uvicorn.Server(config)
    server.run()


if __name__ == "__main__":
    main()
