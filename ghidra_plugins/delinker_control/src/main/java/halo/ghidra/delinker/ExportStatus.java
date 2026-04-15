package halo.ghidra.delinker;

import java.time.Instant;
import java.util.LinkedHashMap;
import java.util.Map;

public class ExportStatus {
  private String timestamp = "";
  private String status = "idle";
  private String message = "No export has run yet";
  private String exportPath = "";

  public synchronized void markOk(String message, String exportPath) {
    this.timestamp = Instant.now().toString();
    this.status = "ok";
    this.message = message;
    this.exportPath = exportPath;
  }

  public synchronized void markError(String message, String exportPath) {
    this.timestamp = Instant.now().toString();
    this.status = "error";
    this.message = message;
    this.exportPath = exportPath;
  }

  public synchronized Map<String, Object> toMap() {
    Map<String, Object> out = new LinkedHashMap<>();
    out.put("timestamp", timestamp);
    out.put("status", status);
    out.put("message", message);
    out.put("export_path", exportPath);
    return out;
  }
}
