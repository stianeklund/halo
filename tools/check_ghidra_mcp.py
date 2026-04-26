#!/usr/bin/env python3
"""Preflight check for local Ghidra MCP SSE endpoints."""

from __future__ import annotations

import argparse
import json
import sys
import time
import urllib.error
import urllib.parse
import urllib.request


USER_MESSAGE = "You might have forgotten to start tools/mcp-servers.sh or ghidra may not be running?"

ENDPOINTS = (
    ("ghidra", "http://127.0.0.1:8090/sse"),
    ("ghidra-live", "http://127.0.0.1:8091/sse"),
)


def _read_sse_data(resp, deadline: float) -> tuple[bool, str]:
    while True:
        if time.monotonic() >= deadline:
            return False, "timed out waiting for SSE data"
        try:
            raw = resp.readline()
        except Exception as exc:
            return False, str(exc)
        if not raw:
            return False, "SSE stream closed unexpectedly"
        line = raw.decode("utf-8", errors="replace").strip()
        if not line.startswith("data:"):
            continue
        return True, line[5:].strip()


def _post_json(url: str, payload: dict, timeout_s: float) -> tuple[bool, str]:
    req = urllib.request.Request(
        url,
        data=json.dumps(payload).encode("utf-8"),
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    try:
        with urllib.request.urlopen(req, timeout=timeout_s) as resp:
            if not (200 <= resp.status < 300):
                return False, f"HTTP {resp.status}"
    except urllib.error.URLError as exc:
        return False, str(exc.reason)
    except Exception as exc:  # defensive: surface any transport/runtime issue
        return False, str(exc)

    return True, "ok"


def _probe(url: str, timeout_s: float) -> tuple[bool, str]:
    req = urllib.request.Request(url, method="GET")
    deadline = time.monotonic() + timeout_s
    try:
        with urllib.request.urlopen(req, timeout=timeout_s) as resp:
            if resp.status != 200:
                return False, f"HTTP {resp.status}"

            ok, bootstrap = _read_sse_data(resp, deadline)
            if not ok:
                return False, f"sse bootstrap failed: {bootstrap}"
            if not bootstrap.startswith("/messages/?session_id="):
                return False, f"unexpected SSE bootstrap payload: {bootstrap!r}"

            parsed = urllib.parse.urlparse(url)
            post_url = f"{parsed.scheme}://{parsed.netloc}{bootstrap}"

            ok, detail = _post_json(
                post_url,
                {
                    "jsonrpc": "2.0",
                    "id": 1,
                    "method": "initialize",
                    "params": {
                        "protocolVersion": "2024-11-05",
                        "capabilities": {},
                        "clientInfo": {"name": "check_ghidra_mcp.py", "version": "1"},
                    },
                },
                timeout_s,
            )
            if not ok:
                return False, f"initialize POST failed: {detail}"

            ok, detail = _post_json(
                post_url,
                {"jsonrpc": "2.0", "method": "notifications/initialized", "params": {}},
                timeout_s,
            )
            if not ok:
                return False, f"initialized notification failed: {detail}"

            ok, detail = _post_json(
                post_url,
                {"jsonrpc": "2.0", "id": 2, "method": "tools/list", "params": {}},
                timeout_s,
            )
            if not ok:
                return False, f"tools/list POST failed: {detail}"

            saw_initialize = False
            saw_tools_list = False
            while time.monotonic() < deadline:
                ok, payload = _read_sse_data(resp, deadline)
                if not ok:
                    return False, f"SSE response read failed: {payload}"
                if not payload.startswith("{"):
                    continue
                try:
                    message = json.loads(payload)
                except json.JSONDecodeError:
                    continue

                if message.get("id") == 1:
                    if "error" in message:
                        return False, f"initialize error: {message['error']}"
                    saw_initialize = True
                elif message.get("id") == 2:
                    if "error" in message:
                        return False, f"tools/list error: {message['error']}"
                    result = message.get("result")
                    if not isinstance(result, dict):
                        return False, "tools/list result was not an object"
                    if not isinstance(result.get("tools"), list):
                        return False, "tools/list result missing tools array"
                    saw_tools_list = True

                if saw_initialize and saw_tools_list:
                    return True, "ok"

            return False, "timed out waiting for initialize/tools/list responses"
    except urllib.error.URLError as exc:
        return False, str(exc.reason)
    except Exception as exc:  # defensive: surface any transport/runtime issue
        return False, str(exc)


def _probe_with_retry(url: str, timeout_s: float, startup_wait_s: float) -> tuple[bool, str]:
    deadline = time.monotonic() + startup_wait_s
    last_detail = "unknown error"

    while True:
        ok, detail = _probe(url, timeout_s)
        if ok:
            return True, "ok"
        last_detail = detail
        if time.monotonic() >= deadline:
            return False, last_detail
        time.sleep(0.25)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--timeout",
        type=float,
        default=2.0,
        help="Per-endpoint timeout in seconds (default: 2.0)",
    )
    parser.add_argument(
        "--startup-wait",
        type=float,
        default=8.0,
        help="Max seconds to wait for each endpoint to come up (default: 8.0)",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Print endpoint-level failure details to stderr",
    )
    args = parser.parse_args()

    failures: list[str] = []
    for name, url in ENDPOINTS:
        ok, detail = _probe_with_retry(url, args.timeout, args.startup_wait)
        if not ok:
            failures.append(f"{name} ({url}): {detail}")

    if failures:
        print(USER_MESSAGE)
        if args.verbose:
            for failure in failures:
                print(f"[check_ghidra_mcp] {failure}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
