"""Environment-variable configuration for the xemu MCP server.

Ported faithfully from the original Node project's src/config.ts (loadConfig).
Defaults match the TS server exactly; XEMU_MCP_HTTP_PORT is new (the MCP HTTP
listen port, distinct from the QMP port).
"""

import os
import tempfile
from dataclasses import dataclass, field


@dataclass
class XemuConfig:
    path: str
    qmp_host: str
    qmp_port: int
    default_args: list = field(default_factory=list)
    screenshot_dir: str = ""
    log_level: str = "INFO"
    http_port: int = 4445

    def screenshot_target_dir(self) -> str:
        """Resolved screenshot directory (falls back to the system temp dir)."""
        return self.screenshot_dir or tempfile.gettempdir()


def load_config() -> XemuConfig:
    qmp_port_str = os.environ.get("XEMU_QMP_PORT", "4444")
    try:
        qmp_port = int(qmp_port_str)
    except ValueError:
        raise ValueError(f"Invalid XEMU_QMP_PORT: {qmp_port_str}")

    default_args = [
        arg for arg in os.environ.get("XEMU_DEFAULT_ARGS", "-s").split(" ") if arg
    ]

    return XemuConfig(
        path=os.environ.get("XEMU_PATH", "xemu.exe"),
        qmp_host=os.environ.get("XEMU_QMP_HOST", "127.0.0.1"),
        qmp_port=qmp_port,
        default_args=default_args,
        screenshot_dir=os.environ.get("XEMU_SCREENSHOT_DIR", ""),
        log_level=os.environ.get("LOG_LEVEL", "INFO").upper(),
        http_port=int(os.environ.get("XEMU_MCP_HTTP_PORT", "4445")),
    )
