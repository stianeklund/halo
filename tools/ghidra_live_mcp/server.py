#!/usr/bin/env python3

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
    def run_relocation_synthesizer(selection_mode: str, range: str | None = None):
        return run_relocation_synthesizer_rpc(selection_mode=selection_mode, range=range)

    @mcp.tool()
    def export_delinked_object(
        export_path: str,
        exporter_name: str = "COFF relocatable object",
        selection_mode: str = "current_selection",
        range: str | None = None,
        run_relocation_synthesizer: bool = True,
    ):
        return export_delinked_object_rpc(
            export_path=export_path,
            exporter_name=exporter_name,
            selection_mode=selection_mode,
            range=range,
            run_relocation_synthesizer=run_relocation_synthesizer,
        )

    @mcp.tool()
    def list_symbols_in_range(range: str):
        return list_symbols_in_range_rpc(range=range)

    @mcp.tool()
    def get_last_export_status():
        return get_last_export_status_rpc()

    import sys
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
    mcp.run(transport=transport)


if __name__ == "__main__":
    main()
