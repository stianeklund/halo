package halo.ghidra.delinker;

import java.util.List;
import java.util.Map;

public final class ResponseWriter {
  private ResponseWriter() {}

  public static String success(String body) {
    return "ok\n" + body;
  }

  public static String error(String code, String message) {
    return "error\ncode=" + escape(code) + "\nmessage=" + escape(message) + "\n";
  }

  public static String writeMap(Map<String, Object> map) {
    StringBuilder sb = new StringBuilder();
    writeMapInto(sb, "", map);
    return sb.toString();
  }

  @SuppressWarnings("unchecked")
  private static void writeValue(StringBuilder sb, String key, Object value) {
    if (value == null) {
      sb.append(key).append("=\n");
      return;
    }

    if (value instanceof Map<?, ?> nestedMap) {
      writeMapInto(sb, key + ".", (Map<String, Object>) nestedMap);
      return;
    }

    if (value instanceof List<?> list) {
      for (int i = 0; i < list.size(); i++) {
        Object item = list.get(i);
        if (item instanceof Map<?, ?> nestedMap) {
          writeMapInto(sb, key + "[" + i + "].", (Map<String, Object>) nestedMap);
        }
        else {
          sb.append(key).append("[").append(i).append("]=").append(escape(String.valueOf(item)))
              .append("\n");
        }
      }
      return;
    }

    sb.append(key).append("=").append(escape(String.valueOf(value))).append("\n");
  }

  private static void writeMapInto(StringBuilder sb, String prefix, Map<String, Object> map) {
    for (Map.Entry<String, Object> entry : map.entrySet()) {
      writeValue(sb, prefix + entry.getKey(), entry.getValue());
    }
  }

  private static String escape(String value) {
    return value.replace("\\", "\\\\").replace("\n", "\\n");
  }
}
