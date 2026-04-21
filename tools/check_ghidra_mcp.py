#!/usr/bin/env python3
"""Preflight check for local Ghidra MCP SSE endpoints."""

from __future__ import annotations

import argparse
import sys
import time
import urllib.error
import urllib.request


USER_MESSAGE = "You might have forgotten to start tools/mcp-servers.sh or ghidra may not be running?"

ENDPOINTS = (
    ("ghidra", "http://127.0.0.1:8090/sse"),
    ("ghidra-live", "http://127.0.0.1:8091/sse"),
)


def _probe(url: str, timeout_s: float) -> tuple[bool, str]:
    req = urllib.request.Request(url, method="GET")
    try:
        with urllib.request.urlopen(req, timeout=timeout_s) as resp:
            if resp.status != 200:
                return False, f"HTTP {resp.status}"
    except urllib.error.URLError as exc:
        return False, str(exc.reason)
    except Exception as exc:  # defensive: surface any transport/runtime issue
        return False, str(exc)

    return True, "ok"


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
