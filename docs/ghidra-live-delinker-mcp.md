# Live Ghidra Delinker MCP

This repo now contains a starter implementation for a live-session delinker
workflow that does not use `analyzeHeadless`.

## Architecture

```text
MCP client
  -> Python adapter in tools/ghidra_live_mcp/
  -> localhost JSON-RPC
  -> Ghidra GUI plugin in ghidra_plugins/delinker_control/
  -> installed ghidra-delinker-extension analyzer/exporters
```

## Why this design

- The installed delinker extension already exposes the needed functionality as a
  normal analyzer and normal exporters.
- The headless helper script in the extension uses standard Ghidra APIs.
- A small in-process plugin is the narrowest reliable control layer for a live
  CodeBrowser session.
- The repo-side starter now avoids external JSON dependencies in the plugin to
  make Ghidra extension packaging simpler.

## Confirmed extension entrypoints

From the installed extension source:

- Analyzer name: `Relocation table synthesizer`
- Exporter name: `COFF relocatable object`
- Exporter name: `ELF relocatable object`

The workflow is:

1. Resolve an `AddressSet`
2. Run one-shot analysis through `AutoAnalysisManager`
3. Find exporter by `Exporter.getName()`
4. Call `export(file, program, addressSet, monitor)`

## Checked-in pieces

### External adapter

- `tools/ghidra_live_mcp/rpc_client.py`
- `tools/ghidra_live_mcp/mcp_server.py`
- `tools/ghidra_live_mcp/server.py`

### Ghidra-side plugin skeleton

- `ghidra_plugins/delinker_control/...`

## Local path evaluation

Use these roles:

- Ghidra install root:
  `/mnt/g/ghidra_12.0.3_PUBLIC_20260210/ghidra_12.0.3_PUBLIC`
- User extension install directory:
  `/mnt/c/Users/stian/AppData/Roaming/ghidra/ghidra_12.0.3_PUBLIC/Extensions`

Only the install root should be used for `GHIDRA_INSTALL_DIR` because it
contains `support/buildExtension.gradle`.

## Remaining work

1. Package the Java plugin as a loadable Ghidra extension/module.
2. Verify any thread constraints for analyzer/exporter calls in live GUI mode.
3. Install an MCP Python SDK in the environment used to launch
   `tools/ghidra_live_mcp/server.py`.
4. Add an optional auth token if localhost-only is not enough for your machine.

## Suggested first validation

1. Open `cachebeta.xbe` in Ghidra GUI.
2. Enable both:
   - `RelocationTableSynthesizedPlugin`
   - `DelinkerControlPlugin`
3. Select a small function range.
4. Call:
   - `get_current_program`
   - `get_current_selection`
   - `run_relocation_synthesizer`
   - `export_delinked_object`
5. Compare the resulting object with the existing objdiff flow.
