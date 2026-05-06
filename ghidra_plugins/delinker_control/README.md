# Delinker Control Plugin

This is a small Ghidra GUI plugin that exposes a localhost RPC surface for the
installed `ghidra-delinker-extension` workflow.

## Purpose

It avoids `analyzeHeadless` entirely.
The plugin runs inside an open CodeBrowser session and directly invokes:

- the `Relocation table synthesizer` analyzer
- the installed exporters such as `COFF relocatable object`

## RPC methods

- `get_current_program`
- `get_current_selection`
- `run_relocation_synthesizer`
- `export_delinked_object` (RPC bridge — prefer `batch_delink.py --object <name>` for CLI use)
- `list_symbols_in_range`
- `get_last_export_status`

## Build

Set `GHIDRA_INSTALL_DIR` to the main Ghidra install root, then run:

```bash
cd ghidra_plugins/delinker_control
gradle buildExtension
```

This uses Ghidra's `support/buildExtension.gradle` script.

For this machine, the correct install root appears to be:

```text
/mnt/g/ghidra_12.0.3_PUBLIC_20260210/ghidra_12.0.3_PUBLIC
```

The user extension directory is different and should not be used as
`GHIDRA_INSTALL_DIR`:

```text
/mnt/c/Users/stian/AppData/Roaming/ghidra/ghidra_12.0.3_PUBLIC/Extensions
```

## Current status

This is a starter skeleton that should now be close to a buildable extension,
but it still needs local validation for:

- exact plugin packaging behavior on your Ghidra 12.0.3 install
- live execution thread constraints for analyzer/exporter calls

The RPC layer intentionally avoids external JSON dependencies. It uses a small
form-encoded request format and a plain-text key/value response format to keep
packaging simple.

## Intended usage

1. Install `ghidra-delinker-extension`.
2. Build and install this plugin.
3. Open CodeBrowser on the target program.
4. Enable the relevant plugins.
5. Start the external MCP adapter in `tools/ghidra_live_mcp/`.
