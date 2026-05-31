"""Async QMP client for the xemu MCP server.

A single shared instance owns the one TCP connection to xemu's QMP socket. All
command execution is serialized by an asyncio.Lock, so concurrent callers (from
multiple MCP sessions) queue rather than interleave on the wire. Unsolicited QMP
events are drained while awaiting the id-matched response.

Ported from the Node project's src/qmp/client.ts. Behavior preserved:
  - connect(): open socket, consume greeting, negotiate qmp_capabilities.
  - execute(): {"execute":cmd,"arguments":args,"id":n}, 10s timeout, match by id.
  - disconnect(): tear down socket, mark disconnected.
"""

import asyncio
import json
from typing import Any, Optional

GREETING_TIMEOUT = 5.0
COMMAND_TIMEOUT = 10.0


class QmpError(RuntimeError):
    """Raised when QMP returns an {"error": {...}} response."""


class QmpClient:
    def __init__(self, host: str, port: int):
        self.host = host
        self.port = port
        self._reader: Optional[asyncio.StreamReader] = None
        self._writer: Optional[asyncio.StreamWriter] = None
        self._next_id = 0
        self._connected = False
        self._exec_lock = asyncio.Lock()
        self._connect_lock = asyncio.Lock()

    def is_connected(self) -> bool:
        return self._connected

    async def connect(self) -> None:
        """Idempotent connect: no-op if already connected."""
        async with self._connect_lock:
            if self._writer is not None:
                return
            reader, writer = await asyncio.wait_for(
                asyncio.open_connection(self.host, self.port), timeout=GREETING_TIMEOUT
            )
            self._reader = reader
            self._writer = writer
            try:
                # First line is the QMP greeting ({"QMP": {...}}).
                greeting = await asyncio.wait_for(
                    self._read_message(), timeout=GREETING_TIMEOUT
                )
                if "QMP" not in greeting:
                    raise QmpError(f"Unexpected QMP greeting: {greeting!r}")
                await self.execute("qmp_capabilities")
                self._connected = True
            except Exception:
                self.disconnect()
                raise

    def disconnect(self) -> None:
        self._connected = False
        if self._writer is not None:
            try:
                self._writer.close()
            except Exception:
                pass
        self._writer = None
        self._reader = None

    async def _read_message(self) -> dict:
        assert self._reader is not None
        line = await self._reader.readline()
        if not line:
            raise ConnectionError("QMP socket closed")
        return json.loads(line.decode("utf-8"))

    async def execute(self, command: str, args: Optional[dict] = None) -> Any:
        if self._writer is None and command != "qmp_capabilities":
            raise RuntimeError("Not connected to QMP")

        async with self._exec_lock:
            if self._writer is None:
                raise RuntimeError("Not connected to QMP")
            self._next_id += 1
            msg_id = self._next_id
            request: dict = {"execute": command, "id": msg_id}
            if args:
                request["arguments"] = args
            self._writer.write((json.dumps(request) + "\n").encode("utf-8"))
            await self._writer.drain()

            # Read until the matching id, skipping greetings and unsolicited events.
            while True:
                msg = await asyncio.wait_for(
                    self._read_message(), timeout=COMMAND_TIMEOUT
                )
                if "QMP" in msg or "event" in msg:
                    continue
                if msg.get("id") == msg_id:
                    if "error" in msg:
                        err = msg["error"]
                        raise QmpError(
                            f"QMP error: {err.get('desc')} ({err.get('class')})"
                        )
                    return msg.get("return")
                # A response for a different id should not occur under the lock;
                # ignore and keep reading.
