"""Async QMP client for the xemu MCP server.

A single shared instance owns the one TCP connection to xemu's QMP socket. All
command execution is serialized by an asyncio.Lock, so concurrent callers (from
multiple MCP sessions) queue rather than interleave on the wire. Unsolicited QMP
events are drained while awaiting the id-matched response.

Resilience contract (added 2026-07-02, fixes the "connection reset by peer"
brick + raw-socket-client poisoning):
  - connect-on-demand: execute() calls ensure_connected() first, so tools no
    longer need an explicit xemu_connect prelude.
  - retry-on-failure + teardown: a transport error (dropped/half-open socket,
    read timeout) tears the socket down (releasing the single :4444 client
    slot), reconnects with backoff, and retries the command ONCE. This is what
    keeps the daemon alive across xemu redeploys and stops a failed call from
    leaving a stale FD that poisons the next raw-socket client.
  - idle auto-disconnect: a background watcher releases the QMP slot after
    idle_disconnect_s of inactivity, so raw-socket tools (qmp_capture.py,
    dump_xemu_memory.py) and the Rust viewer can grab :4444 between MCP calls.
    The next tool call transparently reconnects.

Protocol behavior is otherwise preserved from the original port:
  - connect(): open socket, consume greeting, negotiate qmp_capabilities.
  - execute(): {"execute":cmd,"arguments":args,"id":n}, 10s timeout, match by id.
  - disconnect(): tear down socket, mark disconnected.

QmpError (a real {"error": {...}} protocol response) is NOT a transport failure
and is never retried — it propagates straight to the caller.
"""

import asyncio
import json
from typing import Any, Optional

GREETING_TIMEOUT = 5.0
COMMAND_TIMEOUT = 10.0
RECONNECT_ATTEMPTS = 3
RECONNECT_BASE_DELAY = 0.2
IDLE_CHECK_INTERVAL = 2.0

# Transport-level failures that justify a teardown + reconnect + one retry.
_TRANSPORT_ERRORS = (ConnectionError, asyncio.TimeoutError, OSError)


class QmpError(RuntimeError):
    """Raised when QMP returns an {"error": {...}} response (NOT a transport fault)."""


class QmpClient:
    def __init__(self, host: str, port: int, idle_disconnect_s: float = 20.0):
        self.host = host
        self.port = port
        self.idle_disconnect_s = idle_disconnect_s
        self._reader: Optional[asyncio.StreamReader] = None
        self._writer: Optional[asyncio.StreamWriter] = None
        self._next_id = 0
        self._connected = False
        self._exec_lock = asyncio.Lock()
        self._connect_lock = asyncio.Lock()
        self._last_activity = 0.0
        self._idle_task: Optional[asyncio.Task] = None

    def is_connected(self) -> bool:
        return self._connected

    async def ensure_connected(self) -> None:
        """Connect if the socket is not currently up (connect-on-demand)."""
        if self._writer is None:
            await self.connect()

    async def connect(self) -> None:
        """Idempotent connect: no-op if a live socket already exists."""
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
                # Handshake: single attempt on the fresh socket, no ensure/retry.
                await self._exec_once("qmp_capabilities", None)
                self._connected = True
                self._touch_idle()
            except Exception:
                self.disconnect()
                raise

    def disconnect(self) -> None:
        """Tear down the socket and release the single :4444 client slot."""
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

    async def _exec_once(self, command: str, args: Optional[dict]) -> Any:
        """One command attempt over the current socket. Assumes _writer is set.

        Raises a transport error (ConnectionError/TimeoutError/OSError) on a
        dropped or hung socket, or QmpError on a protocol-level error response.
        """
        async with self._exec_lock:
            if self._writer is None:
                raise ConnectionError("QMP socket not open")
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

    async def execute(self, command: str, args: Optional[dict] = None) -> Any:
        """Run a QMP command with connect-on-demand + one transport retry.

        Transport failures (dropped/half-open socket, read timeout) trigger a
        teardown, a backoff reconnect, and a single retry. QmpError (a real
        protocol error response) propagates immediately without retry.
        """
        # Handshake reentrancy guard: connect() already holds _connect_lock and
        # calls _exec_once directly, so qmp_capabilities never reaches here.
        await self.ensure_connected()
        try:
            result = await self._exec_once(command, args)
        except _TRANSPORT_ERRORS as first_err:
            # Stale/half-open socket: release the slot, rebuild, retry once.
            self.disconnect()
            try:
                await self._reconnect_with_backoff()
                result = await self._exec_once(command, args)
            except _TRANSPORT_ERRORS as second_err:
                self.disconnect()
                raise ConnectionError(
                    f"QMP command {command!r} failed after retry: {second_err}"
                ) from first_err
        self._touch_idle()
        return result

    async def _reconnect_with_backoff(self) -> None:
        last: Optional[Exception] = None
        for i in range(RECONNECT_ATTEMPTS):
            try:
                await self.connect()
                return
            except Exception as e:  # noqa: BLE001 - surface the last cause below
                last = e
                await asyncio.sleep(RECONNECT_BASE_DELAY * (2 ** i))
        raise ConnectionError(
            f"QMP reconnect failed after {RECONNECT_ATTEMPTS} attempts: {last}"
        )

    # -- idle slot release --------------------------------------------------
    def _touch_idle(self) -> None:
        try:
            self._last_activity = asyncio.get_running_loop().time()
        except RuntimeError:
            return
        if self.idle_disconnect_s and (self._idle_task is None or self._idle_task.done()):
            self._idle_task = asyncio.ensure_future(self._idle_watcher())

    async def _idle_watcher(self) -> None:
        """Release the QMP slot after idle_disconnect_s of inactivity."""
        while self._writer is not None:
            await asyncio.sleep(IDLE_CHECK_INTERVAL)
            if self._writer is None:
                return
            # Never yank the socket out from under an in-flight command.
            if self._exec_lock.locked():
                continue
            idle = asyncio.get_running_loop().time() - self._last_activity
            if idle >= self.idle_disconnect_s:
                async with self._connect_lock:
                    still_idle = (
                        asyncio.get_running_loop().time() - self._last_activity
                        >= self.idle_disconnect_s
                    )
                    if self._writer is not None and not self._exec_lock.locked() and still_idle:
                        self.disconnect()
                return
