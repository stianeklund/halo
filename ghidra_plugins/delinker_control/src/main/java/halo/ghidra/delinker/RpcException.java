package halo.ghidra.delinker;

public class RpcException extends RuntimeException {
  private final String code;

  public RpcException(String code, String message) {
    super(message);
    this.code = code;
  }

  public String getCode() {
    return code;
  }
}
