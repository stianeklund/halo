"""xemu process manager for the MCP server.

Ported from the Node project's src/xemu/process.ts. Launches xemu with the QMP
TCP server enabled (matching the QMP host/port the client connects to), tracks
whether this server launched the process, and supports teardown.
"""

import subprocess
from typing import List, Optional

from config import XemuConfig


class XemuProcessManager:
    def __init__(self, config: XemuConfig):
        self.config = config
        self._proc: Optional[subprocess.Popen] = None
        self._launched_by_us = False

    def launch(self, extra_args: Optional[List[str]] = None) -> subprocess.Popen:
        if self._proc is not None and self._proc.poll() is None:
            raise RuntimeError("xemu process already managed by this server")

        qmp_arg = (
            f"tcp:{self.config.qmp_host}:{self.config.qmp_port},server=on,wait=off"
        )
        args = [
            self.config.path,
            "-qmp",
            qmp_arg,
            *self.config.default_args,
            *(extra_args or []),
        ]

        self._proc = subprocess.Popen(args)
        self._launched_by_us = True
        return self._proc

    def is_launched_by_us(self) -> bool:
        # Stale if the process has since exited externally.
        if self._proc is not None and self._proc.poll() is not None:
            self._launched_by_us = False
        return self._launched_by_us

    def is_process_alive(self) -> bool:
        return self._proc is not None and self._proc.poll() is None

    # Alias used by the /health route.
    def is_running(self) -> bool:
        return self.is_process_alive()

    def stop(self) -> None:
        if self._proc is not None:
            try:
                self._proc.terminate()
            except Exception:
                pass
            self._proc = None
            self._launched_by_us = False
