#!/usr/bin/env python3

import sys, os
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

    import sys
    import time
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
    if transport == "sse":
        print(f"ghidra-live MCP listening on http://{host}:{port}/sse", file=sys.stderr)

    # Work around intermittent Starlette/Uvicorn response-state errors seen on
    # SSE disconnect/exception paths; keep the server alive instead of exiting.
    while True:
        try:
            mcp.run(transport=transport)
            break
        except RuntimeError as exc:
            if "Expected ASGI message 'http.response.body'" not in str(exc):
                raise
            print(
                "ghidra-live MCP recovered from ASGI response-order error; continuing",
                file=sys.stderr,
            )
            if transport != "sse":
                raise
            time.sleep(0.25)


if __name__ == "__main__":
    main()
