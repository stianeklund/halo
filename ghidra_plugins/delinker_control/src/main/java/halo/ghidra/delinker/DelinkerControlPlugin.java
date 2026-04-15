package halo.ghidra.delinker;

import ghidra.app.plugin.ProgramPlugin;
import ghidra.framework.plugintool.PluginInfo;
import ghidra.framework.plugintool.PluginTool;
import ghidra.framework.plugintool.util.PluginStatus;
import ghidra.program.model.listing.Program;
import ghidra.program.util.ProgramSelection;

@PluginInfo(
    status = PluginStatus.STABLE,
    packageName = "Halo",
    category = "Halo",
    shortDescription = "Live delinker RPC control",
    description = "Exposes localhost RPC for live GUI delinker workflows."
)
public class DelinkerControlPlugin extends ProgramPlugin implements ProgramPluginAccess {
  private DelinkerRpcServer rpcServer;
  private final ExportStatus lastStatus = new ExportStatus();

  public DelinkerControlPlugin(PluginTool tool) {
    super(tool);
  }

  @Override
  protected void init() {
    super.init();
    DelinkerService service = new DelinkerService(this, lastStatus);
    rpcServer = new DelinkerRpcServer(service, 18080);
    rpcServer.start();
  }

  @Override
  public void dispose() {
    if (rpcServer != null) {
      rpcServer.stop();
    }
    super.dispose();
  }

  @Override
  public Program getCurrentProgram() {
    return currentProgram;
  }

  @Override
  public ProgramSelection getCurrentSelection() {
    return currentSelection;
  }
}
