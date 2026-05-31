# Live Ghidra Delinker

Live-session MCP integration for exporting small relocatable reference objects from a running Ghidra GUI for comparison against compiled candidate objects.

## Architecture

```
MCP client
  -> Python adapter in tools/ghidra_live_mcp/
  -> localhost JSON-RPC
  -> Ghidra GUI plugin in ghidra_plugins/delinker_control/
  -> installed ghidra-delinker-extension analyzer/exporters
```

The installed delinker extension exposes needed functionality as analyzers and exporters. A small in-process plugin is the narrowest control layer for a live CodeBrowser session. No external JSON dependencies in the plugin to keep Ghidra packaging simple.

### Extension entrypoints

- Analyzer: `Relocation table synthesizer`
- Exporter: `COFF relocatable object`
- Exporter: `ELF relocatable object`

### Checked-in pieces

**External adapter:** `tools/ghidra_live_mcp/rpc_client.py`, `mcp_server.py`, `server.py`
**Ghidra plugin:** `ghidra_plugins/delinker_control/...`

### Prerequisites

- Ghidra GUI session with `DelinkerControlPlugin` enabled (RPC on `127.0.0.1:18081`)
- MCP Python SDK installed in the server environment

---

## Workflow

Use this as an optional validation lane, not the main decompilation path. Best fit for:
- Frontier helper clusters
- Nearly complete `.obj` groups
- Difficult functions where structural validation helps
- Targeted verification before landing

### Standard flow

```
binary analysis -> implementation -> live delinker export -> objdiff -> fidelity fixes
```

### Procedure

1. Analyze and implement a small function or helper cluster as usual.
2. Select one function or a tight address range in the live Ghidra GUI.
3. Export via MCP:
   - `run_relocation_synthesizer` then `export_delinked_object`
   - Or CLI: `python3 tools/audit/batch_delink.py --object <name>`
4. Build the project normally.
5. Compare the delinked reference object with the compiled candidate via objdiff.
6. Use diff results to guide fidelity fixes.

---

## Targeted Use: Catching Field-Rotation / Offset-Swap Bugs

The live delinker is well-suited to catching bugs where our C has the right memory accesses but writes to the wrong struct fields — correct in review but silently violating an original invariant. Commit `8ea4afa` (ctl2 rotation in `players_update_before_game`) is the canonical example: invisible in the decompiler, passed `-Wall -Werror`, surfaced as a page fault in `stack_walk` several frames downstream.

### Automated procedure (no GUI interaction)

1. Resolve the function's object name from `kb.json`.
2. Run `python3 tools/audit/batch_delink.py --object <name>` to export the delinked reference.
3. Build normally.
4. Run `tools/verify/objdiff_lift.py` with the delinked object as `--reference` and our compiled object as `--candidate`.
5. Scan the diff for systematic `[reg+N]` store-offset mismatches across adjacent writes — the signature of rotated assignments.

### Path gotcha

Ghidra runs on Windows even when the repo is under WSL. `export_path` is opened on the Ghidra side, so use Windows-format paths (e.g. `G:\dev\halo\artifacts\delinker\foo.o`). Pre-create the directory from WSL (`mkdir -p /mnt/g/dev/halo/artifacts/delinker/`).

### Tested example

Verified end-to-end against `cinematic_in_progress` (9 bytes). Produced a valid COFF object with one `dir32` relocation.

### Caveats

- Clang vs MSVC scheduling produces structural noise in objdiff. Look for *systematic* mismatches across adjacent stores, not single unexplained diffs.
- For long functions, prefer the xbox-halo-re-analyst agent's store-offset table first.
- Don't save the Ghidra project after a delinker run unless you intend to keep the synthesized relocations permanently.

### Harm analysis

| Risk | Severity | Notes |
|------|----------|-------|
| Ghidra database mutation | Low | Reloc synthesizer edits relocation table; persists only if project is saved |
| GUI thread lag | None | Analysis runs on Ghidra's internal threads |
| File-lock conflict | None | Live MCP runs inside the GUI process |
| `cachebeta.xbe` corruption | None | Binary is read-only |
| Runtime side effects | None | Pure analysis/export |
| Environment fragility | Low | If misconfigured, calls fail cleanly |
| Incidental rework | Low | Re-running after analyzer is idempotent but wastes seconds |

---

## Remaining Work

1. Package the Java plugin as a loadable Ghidra extension/module.
2. Verify thread constraints for analyzer/exporter calls in live GUI mode.
3. Install MCP Python SDK in the server launch environment.
4. Add optional auth token if localhost-only is insufficient.
