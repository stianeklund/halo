#!/usr/bin/env python3
"""Bridge: exposes GhidraMCP HTTP API as MCP SSE/stdio server for local tools."""

import asyncio
import json
import sys
import urllib.error
import urllib.parse
import urllib.request
from typing import Any

from mcp import types
from mcp.server import Server

GHIDRA_BASE = "http://localhost:8089"


def _http_request(method: str, path: str, query: dict, body: dict) -> Any:
    url = GHIDRA_BASE + path
    if query:
        url += "?" + urllib.parse.urlencode({k: v for k, v in query.items() if v is not None})

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


server = Server("ghidra-mcp")
_tools: list[types.Tool] = []
_endpoint_map: dict[str, dict] = {}
_loaded = False


def _ensure_loaded() -> None:
    global _tools, _endpoint_map, _loaded
    if _loaded:
        return
    raw = _fetch_schema_sync()
    _tools, _endpoint_map = _build_tools(raw)
    _loaded = True
    print(f"[ghidra-mcp] Loaded {len(_tools)} tools from Ghidra", file=sys.stderr)


@server.list_tools()
async def list_tools() -> list[types.Tool]:
    await asyncio.get_event_loop().run_in_executor(None, _ensure_loaded)
    return _tools


@server.call_tool()
async def call_tool(name: str, arguments: dict[str, Any]) -> list[types.TextContent]:
    await asyncio.get_event_loop().run_in_executor(None, _ensure_loaded)

    endpoint = _endpoint_map.get(name)
    if not endpoint:
        return [types.TextContent(type="text", text=f"Unknown tool: {name}")]

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

    result = await asyncio.get_event_loop().run_in_executor(None, _call)
    return [types.TextContent(type="text", text=json.dumps(result, indent=2))]


class _SSEEndpoint:
    """ASGI endpoint wrapper to avoid Starlette's request->Response adapter.

    With Starlette 1.x, wrapping the SSE transport in a request handler can cause
    a second response to be emitted after the EventSourceResponse already started.
    """

    def __init__(self, mcp_server: Server, sse_transport) -> None:
        self._mcp_server = mcp_server
        self._sse_transport = sse_transport

    async def __call__(self, scope, receive, send) -> None:
        async with self._sse_transport.connect_sse(scope, receive, send) as streams:
            await self._mcp_server.run(
                streams[0],
                streams[1],
                self._mcp_server.create_initialization_options(),
            )


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
    transport, host, port = _parse_args()
    if transport == "stdio":
        from mcp.server.stdio import stdio_server

        async with stdio_server() as (read_stream, write_stream):
            await server.run(read_stream, write_stream, server.create_initialization_options())
        return

    from mcp.server.sse import SseServerTransport
    from starlette.applications import Starlette
    from starlette.routing import Mount, Route
    import uvicorn

    sse = SseServerTransport("/messages/")
    app = Starlette(
        routes=[
            Route("/sse", endpoint=_SSEEndpoint(server, sse), methods=["GET"]),
            Mount("/messages/", app=sse.handle_post_message),
        ],
    )
    print(f"[ghidra-mcp] SSE server listening on http://{host}:{port}/sse", file=sys.stderr)
    config = uvicorn.Config(app, host=host, port=port, log_level="warning")
    srv = uvicorn.Server(config)
    await srv.serve()


if __name__ == "__main__":
    asyncio.run(main())
