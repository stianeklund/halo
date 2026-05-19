package halo.ghidra.delinker;

import ghidra.app.plugin.core.analysis.AutoAnalysisManager;
import ghidra.app.services.Analyzer;
import ghidra.app.util.Option;
import ghidra.app.util.exporter.Exporter;
import ghidra.app.util.importer.MessageLog;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.address.AddressSetView;
import ghidra.program.model.listing.Program;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;
import ghidra.util.classfinder.ClassSearcher;
import ghidra.util.task.TaskMonitor;

import java.io.File;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

public class DelinkerService {
  private static final String RELOCATION_ANALYZER_NAME = "Relocation table synthesizer";

  private final ProgramPluginAccess access;
  private final ExportStatus lastStatus;

  public DelinkerService(ProgramPluginAccess access, ExportStatus lastStatus) {
    this.access = access;
    this.lastStatus = lastStatus;
  }

  public Map<String, Object> getCurrentProgram() {
    Program program = requireProgram();
    Map<String, Object> out = new LinkedHashMap<>();
    out.put("program_name", program.getName());
    out.put("domain_file_path", program.getDomainFile().getPathname());
    out.put("language_id", program.getLanguageID().getIdAsString());
    out.put("image_base", program.getImageBase().toString());
    return out;
  }

  public Map<String, Object> getCurrentSelection() {
    Map<String, Object> out = new LinkedHashMap<>();
    var selection = access.getCurrentSelection();
    out.put("has_selection", selection != null && !selection.isEmpty());
    out.put("ranges", SelectionUtils.toRangeList(selection));
    return out;
  }

  public Map<String, Object> runRelocationSynthesizer(String selectionMode, String rangeText)
      throws Exception {
    Program program = requireProgram();
    AddressSet set =
        SelectionUtils.resolveSelection(program, access.getCurrentSelection(), selectionMode, rangeText);

    AutoAnalysisManager autoAnalysisManager = AutoAnalysisManager.getAnalysisManager(program);
    Analyzer analyzer = autoAnalysisManager.getAnalyzer(RELOCATION_ANALYZER_NAME);
    if (analyzer == null) {
      throw new RpcException("ANALYZER_NOT_FOUND",
          "Relocation table synthesizer analyzer is not available");
    }

    // Call analyzer.added() directly with a local MessageLog instead of going
    // through scheduleOneTimeAnalysis, which routes messages to the shared
    // AutoAnalysisManager log and triggers popup dialogs on warnings.
    MessageLog localLog = new MessageLog();
    int txId = program.startTransaction("Relocation Synthesizer");
    boolean committed = false;
    try {
      analyzer.added(program, set, TaskMonitor.DUMMY, localLog);
      committed = true;
    } finally {
      program.endTransaction(txId, committed);
    }

    Map<String, Object> out = new LinkedHashMap<>();
    out.put("status", "ok");
    out.put("analyzer", RELOCATION_ANALYZER_NAME);
    out.put("target_ranges", SelectionUtils.toRangeList(set));
    String warnings = localLog.toString().trim();
    if (!warnings.isEmpty()) {
      out.put("warnings", warnings);
    }
    return out;
  }

  public Map<String, Object> exportDelinkedObject(String exportPath, String exporterName,
      String selectionMode, String rangeText, boolean runRelocationSynthesizer) throws Exception {
    Program program = requireProgram();
    AddressSet set =
        SelectionUtils.resolveSelection(program, access.getCurrentSelection(), selectionMode, rangeText);

    if (runRelocationSynthesizer) {
      runRelocationSynthesizer(selectionMode, rangeText);
    }

    Exporter exporter = findExporter(exporterName);
    if (exporter == null) {
      throw new RpcException("EXPORTER_NOT_FOUND", "Exporter not found: " + exporterName);
    }

    File file = new File(exportPath);
    List<Option> options = new ArrayList<>(exporter.getOptions(() -> program));
    exporter.setOptions(options);

    if (!exporter.export(file, program, set, TaskMonitor.DUMMY)) {
      lastStatus.markError("Exporter returned false", exportPath);
      throw new RpcException("EXPORT_FAILED", "Exporter returned false");
    }

    lastStatus.markOk("Export completed", exportPath);

    Map<String, Object> out = new LinkedHashMap<>();
    out.put("status", "ok");
    out.put("export_path", exportPath);
    out.put("exporter_name", exporterName);
    out.put("selection_used", SelectionUtils.toRangeList(set));
    return out;
  }

  public Map<String, Object> listSymbolsInRange(String rangeText) {
    Program program = requireProgram();
    AddressSetView set = SelectionUtils.parseRange(program, rangeText);

    List<Map<String, Object>> symbols = new ArrayList<>();
    SymbolIterator it = program.getSymbolTable().getPrimarySymbolIterator(set, true);
    while (it.hasNext()) {
      Symbol symbol = it.next();
      Map<String, Object> item = new LinkedHashMap<>();
      item.put("name", symbol.getName());
      item.put("address", symbol.getAddress().toString());
      item.put("type", symbol.getSymbolType().toString());
      item.put("global", symbol.isGlobal());
      symbols.add(item);
    }

    Map<String, Object> out = new LinkedHashMap<>();
    out.put("symbols", symbols);
    return out;
  }

  public Map<String, Object> getLastExportStatus() {
    return lastStatus.toMap();
  }

  private Program requireProgram() {
    Program program = access.getCurrentProgram();
    if (program == null) {
      throw new RpcException("NO_ACTIVE_PROGRAM", "No active program in CodeBrowser");
    }
    return program;
  }

  private Exporter findExporter(String exporterName) {
    for (Exporter exporter : ClassSearcher.getInstances(Exporter.class)) {
      if (exporterName.equals(exporter.getName())) {
        return exporter;
      }
    }
    return null;
  }
}
