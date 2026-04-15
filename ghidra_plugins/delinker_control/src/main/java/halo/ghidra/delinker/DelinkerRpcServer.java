package halo.ghidra.delinker;

import com.sun.net.httpserver.HttpExchange;
import com.sun.net.httpserver.HttpServer;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.nio.charset.StandardCharsets;
import java.util.Map;

public class DelinkerRpcServer {
  private final DelinkerService service;
  private final int port;
  private HttpServer server;

  public DelinkerRpcServer(DelinkerService service, int port) {
    this.service = service;
    this.port = port;
  }

  public void start() {
    try {
      server = HttpServer.create(new InetSocketAddress("127.0.0.1", port), 0);
      server.createContext("/rpc", this::handleRpc);
      server.start();
    }
    catch (IOException exc) {
      throw new RuntimeException("Failed to start RPC server", exc);
    }
  }

  public void stop() {
    if (server != null) {
      server.stop(0);
    }
  }

  private void handleRpc(HttpExchange exchange) throws IOException {
    if (!"POST".equals(exchange.getRequestMethod())) {
      exchange.sendResponseHeaders(405, -1);
      return;
    }

    byte[] requestBytes;
    try (InputStream in = exchange.getRequestBody()) {
      requestBytes = in.readAllBytes();
    }

    String requestJson = new String(requestBytes, StandardCharsets.UTF_8);
    String responseText = dispatch(requestJson);
    byte[] responseBytes = responseText.getBytes(StandardCharsets.UTF_8);

    exchange.getResponseHeaders().set("Content-Type", "text/plain; charset=utf-8");
    exchange.sendResponseHeaders(200, responseBytes.length);
    try (OutputStream out = exchange.getResponseBody()) {
      out.write(responseBytes);
    }
  }

  private String dispatch(String requestBody) {
    try {
      Map<String, String> params = RequestParser.parseFormBody(requestBody);
      String method = params.get("method");
      if (method == null || method.isBlank()) {
        throw new RpcException("BAD_REQUEST", "Missing method");
      }

      Map<String, Object> result = switch (method) {
        case "get_current_program" -> service.getCurrentProgram();
        case "get_current_selection" -> service.getCurrentSelection();
        case "run_relocation_synthesizer" -> service.runRelocationSynthesizer(
            params.get("selection_mode"), params.get("range"));
        case "export_delinked_object" -> service.exportDelinkedObject(
            params.get("export_path"), params.get("exporter_name"), params.get("selection_mode"),
            params.get("range"), Boolean.parseBoolean(params.getOrDefault(
                "run_relocation_synthesizer", "true")));
        case "list_symbols_in_range" -> service.listSymbolsInRange(params.get("range"));
        case "get_last_export_status" -> service.getLastExportStatus();
        default -> throw new RpcException("UNKNOWN_METHOD", "Unknown method: " + method);
      };

      return ResponseWriter.success(ResponseWriter.writeMap(result));
    }
    catch (RpcException exc) {
      return ResponseWriter.error(exc.getCode(), exc.getMessage());
    }
    catch (Exception exc) {
      return ResponseWriter.error("INTERNAL_ERROR", exc.toString());
    }
  }
}
