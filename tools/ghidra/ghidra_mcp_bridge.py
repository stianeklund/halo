#!/usr/bin/env python3
"""Bridge: exposes GhidraMCP HTTP API as MCP SSE/stdio server for local tools.

Hardened with:
- Schema refresh (TTL + auto-refresh on Ghidra recovery)
- Session lifecycle tracking and stale-writer cleanup
- Concurrency limiting on Ghidra HTTP calls
- Structured logging and /health endpoint
"""

import asyncio
import json
import sys
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
from typing import Any

from mcp import types
from mcp.server import Server

GHIDRA_BASE = "http://localhost:8089"

_SCHEMA_TTL_S = 300
_HEALTH_CHECK_S = 30
_MAX_CONCURRENT = 4


def _log(msg: str) -> None:
    print(f"[ghidra-mcp] {msg}", file=sys.stderr, flush=True)


# ---------------------------------------------------------------------------
# HTTP layer
# ---------------------------------------------------------------------------

def _http_request(method: str, path: str, query: dict, body: dict) -> Any:
    url = GHIDRA_BASE + path
    if query:
        url += "?" + urllib.parse.urlencode(
            {k: v for k, v in query.items() if v is not None}
        )

    if method.upper() == "GET":
        req = urllib.request.Request(url, method="GET")
    else:
        data = json.dumps(body).encode()
        req = urllib.request.Request(
            url,
            data=data,
            headers={"Content-Type": "application/json"},
            method=method.upper(),
        )

    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            raw = resp.read().decode()
            try:
                return json.loads(raw)
            except json.JSONDecodeError:
                return {"result": raw}
    except urllib.error.HTTPError as exc:
        body_bytes = exc.read()
        try:
            return json.loads(body_bytes.decode())
        except Exception:
            return {"error": f"HTTP {exc.code}: {body_bytes.decode()}"}


def _ghidra_ping() -> bool:
    try:
        with urllib.request.urlopen(
            f"{GHIDRA_BASE}/get_function_count?program=", timeout=5
        ) as resp:
            resp.read()
        return True
    except Exception:
        return False


# ---------------------------------------------------------------------------
# Schema management
# ---------------------------------------------------------------------------

def _fetch_schema_sync() -> list[dict]:
    with urllib.request.urlopen(f"{GHIDRA_BASE}/mcp/schema", timeout=10) as resp:
        return json.loads(resp.read().decode()).get("tools", [])


def _build_tools(raw_tools: list[dict]) -> tuple[list[types.Tool], dict]:
    mcp_tools: list[types.Tool] = []
    endpoint_map: dict[str, dict] = {}

    for tool in raw_tools:
        path: str = tool.get("path", "")
        method: str = tool.get("method", "POST")
        description: str = tool.get("description", "")
        category_desc: str = tool.get("category_description", "")
        params: list[dict] = tool.get("params", [])

        name = path.lstrip("/")
        if not name:
            continue

        full_description = description
        if category_desc:
            full_description += f"\n\nCategory: {category_desc}"

        properties: dict[str, Any] = {}
        required_fields: list[str] = []
        for param in params:
            pname = param.get("name", "")
            ptype = param.get("type", "string")
            pdesc = param.get("description", "")
            prequired = param.get("required", False)
            pdefault = param.get("default")

            json_type = {
                "string": "string",
                "integer": "integer",
                "int": "integer",
                "boolean": "boolean",
                "bool": "boolean",
                "number": "number",
            }.get(ptype.lower(), "string")

            prop: dict[str, Any] = {"type": json_type}
            if pdesc:
                prop["description"] = pdesc
            if pdefault is not None and pdefault != "":
                prop["default"] = pdefault

            properties[pname] = prop
            if prequired:
                required_fields.append(pname)

        input_schema: dict[str, Any] = {
            "type": "object",
            "properties": properties,
        }
        if required_fields:
            input_schema["required"] = required_fields

        mcp_tools.append(
            types.Tool(
                name=name,
                description=full_description,
                inputSchema=input_schema,
            )
        )
        endpoint_map[name] = {"path": path, "method": method, "params": params}

    return mcp_tools, endpoint_map


# ---------------------------------------------------------------------------
# MCP server
# ---------------------------------------------------------------------------

server = Server("ghidra-mcp")
_tools: list[types.Tool] = []
_endpoint_map: dict[str, dict] = {}
_schema_loaded_at: float = 0.0
_schema_lock = threading.Lock()
_ghidra_up: bool = False
_active_sessions: int = 0
_ghidra_sem: asyncio.Semaphore | None = None


def _ensure_loaded() -> None:
    global _tools, _endpoint_map, _schema_loaded_at
    now = time.monotonic()
    if _schema_loaded_at and (now - _schema_loaded_at) < _SCHEMA_TTL_S:
        return
    if _schema_loaded_at and not _ghidra_up:
        return
    with _schema_lock:
        if _schema_loaded_at and (time.monotonic() - _schema_loaded_at) < _SCHEMA_TTL_S:
            return
        try:
            raw = _fetch_schema_sync()
            new_tools, new_map = _build_tools(raw)
            prev = _schema_loaded_at
            _tools = new_tools
            _endpoint_map = new_map
            _schema_loaded_at = time.monotonic()
            if prev:
                _log(f"Schema refreshed: {len(_tools)} tools")
            else:
                _log(f"Schema loaded: {len(_tools)} tools")
        except Exception as exc:
            if _schema_loaded_at:
                _log(f"Schema refresh failed (serving cached {len(_tools)} tools): {exc}")
                return
            raise


@server.list_tools()
async def list_tools() -> list[types.Tool]:
    loop = asyncio.get_running_loop()
    await loop.run_in_executor(None, _ensure_loaded)
    return _tools


@server.call_tool()
async def call_tool(name: str, arguments: dict[str, Any]) -> list[types.TextContent]:
    loop = asyncio.get_running_loop()
    await loop.run_in_executor(None, _ensure_loaded)

    endpoint = _endpoint_map.get(name)
    if not endpoint:
        return [types.TextContent(type="text", text=f"Unknown tool: {name}")]

    if not _ghidra_up:
        return [
            types.TextContent(
                type="text",
                text=json.dumps(
                    {
                        "error": f"Ghidra is not reachable at {GHIDRA_BASE}",
                        "hint": "Check that Ghidra is running with the GhidraMCP plugin.",
                    }
                ),
            )
        ]

    path = endpoint["path"]
    method = endpoint["method"]
    param_defs = endpoint["params"]

    query_params: dict[str, Any] = {}
    body_params: dict[str, Any] = {}
    for param in param_defs:
        pname = param["name"]
        source = param.get("source", "body")
        value = arguments.get(pname)
        if value is None:
            continue
        if source == "query":
            query_params[pname] = value
        else:
            body_params[pname] = value

    def _call() -> Any:
        return _http_request(method, path, query_params, body_params)

    sem = _ghidra_sem
    if sem:
        async with sem:
            result = await loop.run_in_executor(None, _call)
    else:
        result = await loop.run_in_executor(None, _call)

    return [types.TextContent(type="text", text=json.dumps(result, indent=2))]


# ---------------------------------------------------------------------------
# Session tracking
# ---------------------------------------------------------------------------

def _cleanup_stale_writers(transport) -> None:
    writers = getattr(transport, "_read_stream_writers", {})
    stale = [sid for sid, w in writers.items() if getattr(w, "_closed", False)]
    for sid in stale:
        del writers[sid]
    if stale:
        _log(f"Cleaned {len(stale)} stale session entries ({len(writers)} remaining)")


class _SSEEndpoint:
    """ASGI endpoint with session lifecycle tracking and stale-writer cleanup."""

    def __init__(self, mcp_server: Server, sse_transport) -> None:
        self._mcp_server = mcp_server
        self._sse_transport = sse_transport

    async def __call__(self, scope, receive, send) -> None:
        global _active_sessions
        _active_sessions += 1
        _log(f"Session connected ({_active_sessions} active)")
        try:
            async with self._sse_transport.connect_sse(scope, receive, send) as streams:
                await self._mcp_server.run(
                    streams[0],
                    streams[1],
                    self._mcp_server.create_initialization_options(),
                )
        finally:
            _active_sessions -= 1
            _log(f"Session disconnected ({_active_sessions} active)")
            _cleanup_stale_writers(self._sse_transport)


# ---------------------------------------------------------------------------
# Background tasks
# ---------------------------------------------------------------------------

async def _health_loop() -> None:
    global _ghidra_up, _schema_loaded_at
    loop = asyncio.get_running_loop()

    while True:
        await asyncio.sleep(_HEALTH_CHECK_S)
        was_up = _ghidra_up
        _ghidra_up = await loop.run_in_executor(None, _ghidra_ping)

        if _ghidra_up and not was_up:
            _log("Ghidra recovered — refreshing schema")
            _schema_loaded_at = 0.0
            try:
                await loop.run_in_executor(None, _ensure_loaded)
            except Exception as exc:
                _log(f"Schema refresh after recovery failed: {exc}")
        elif not _ghidra_up and was_up:
            _log("Ghidra became unavailable")


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def _parse_args() -> tuple[str, str, int]:
    transport = "sse"
    host = "127.0.0.1"
    port = 8090
    for arg in sys.argv[1:]:
        if arg.startswith("--port="):
            port = int(arg.split("=", 1)[1])
        elif arg.startswith("--host="):
            host = arg.split("=", 1)[1]
        elif arg == "--stdio":
            transport = "stdio"
    return transport, host, port


async def main() -> None:
    global _ghidra_sem, _ghidra_up
    _ghidra_sem = asyncio.Semaphore(_MAX_CONCURRENT)

    transport, host, port = _parse_args()
    if transport == "stdio":
        from mcp.server.stdio import stdio_server

        async with stdio_server() as (read_stream, write_stream):
            await server.run(
                read_stream, write_stream, server.create_initialization_options()
            )
        return

    from mcp.server.sse import SseServerTransport
    from starlette.applications import Starlette
    from starlette.responses import JSONResponse
    from starlette.routing import Mount, Route
    import uvicorn

    sse = SseServerTransport("/messages/")

    async def health(request):
        age = round(time.monotonic() - _schema_loaded_at) if _schema_loaded_at else None
        writers = len(getattr(sse, "_read_stream_writers", {}))
        return JSONResponse(
            {
                "ghidra": "up" if _ghidra_up else "down",
                "active_sessions": _active_sessions,
                "sdk_writers": writers,
                "tools": len(_tools),
                "schema_age_s": age,
            }
        )

    app = Starlette(
        routes=[
            Route("/sse", endpoint=_SSEEndpoint(server, sse), methods=["GET"]),
            Mount("/messages/", app=sse.handle_post_message),
            Route("/health", endpoint=health, methods=["GET"]),
        ],
    )

    _ghidra_up = _ghidra_ping()
    _log(f"Ghidra initial state: {'up' if _ghidra_up else 'down'}")

    asyncio.ensure_future(_health_loop())

    _log(f"SSE server listening on http://{host}:{port}/sse")
    _log(f"Health endpoint: http://{host}:{port}/health")
    config = uvicorn.Config(app, host=host, port=port, log_level="warning")
    srv = uvicorn.Server(config)
    await srv.serve()


if __name__ == "__main__":
    asyncio.run(main())
