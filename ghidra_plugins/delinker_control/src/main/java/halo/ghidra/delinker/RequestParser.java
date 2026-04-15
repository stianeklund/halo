package halo.ghidra.delinker;

import java.io.UnsupportedEncodingException;
import java.net.URLDecoder;
import java.nio.charset.StandardCharsets;
import java.util.LinkedHashMap;
import java.util.Map;

public final class RequestParser {
  private RequestParser() {}

  public static Map<String, String> parseFormBody(String body) {
    Map<String, String> params = new LinkedHashMap<>();
    if (body == null || body.isBlank()) {
      return params;
    }

    String[] pairs = body.split("&");
    for (String pair : pairs) {
      String[] parts = pair.split("=", 2);
      String key = decode(parts[0]);
      String value = parts.length > 1 ? decode(parts[1]) : "";
      params.put(key, value);
    }
    return params;
  }

  private static String decode(String value) {
    try {
      return URLDecoder.decode(value, StandardCharsets.UTF_8.name());
    }
    catch (UnsupportedEncodingException exc) {
      throw new RuntimeException(exc);
    }
  }
}
