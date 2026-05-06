# Live Ghidra Delinker MCP Adapter

This directory contains the external side of a live-session delinker workflow.

It does not run Ghidra internals itself.
It forwards a narrow RPC surface to a Ghidra GUI plugin running inside CodeBrowser.

## Commands

- `get_current_program`
- `get_current_selection`
- `run_relocation_synthesizer`
- `export_delinked_object` (RPC bridge — prefer `batch_delink.py --object <name>` for CLI use)
- `list_symbols_in_range`
- `get_last_export_status`

## Files

- `rpc_client.py`: localhost client for the Ghidra GUI plugin
- `mcp_server.py`: plain Python adapter functions
- `server.py`: real MCP server entrypoint

## RPC endpoint

Default: `http://127.0.0.1:18080/rpc`

The wire format is intentionally minimal: `application/x-www-form-urlencoded`
requests and a plain-text key/value response.

## Notes

- The Ghidra GUI must already be open.
- The `ghidra-delinker-extension` must already be installed.
- The companion plugin under `ghidra_plugins/delinker_control/` must be installed and active.

## Running as MCP

`server.py` expects an MCP Python SDK that provides:

```python
from mcp.server.fastmcp import FastMCP
```

Example OpenCode config:

```jsonc
{
  "$schema": "https://opencode.ai/config.json",
  "mcp": {
    "ghidra-live": {
      "type": "local",
      "command": [
        "python3",
        "/mnt/g/dev/halo/tools/ghidra_live_mcp/server.py"
      ]
    }
  }
}
```
