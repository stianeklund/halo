#!/usr/bin/env python3
import sys, os
_tools_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)

"""Send keyboard input to xemu through QMP.

This is for Xbox debug-key/keyboard automation. For gamepad input, use
tools/xbox/xbox_pad.py instead.
"""

import argparse
import json
import sys
import time

import xemu_qmp


def parse_key_combo(text):
    keys = []
    for part in text.replace(",", "+").split("+"):
        key = part.strip()
        if not key:
            continue
        keys.append({"type": "qcode", "data": key})
    if not keys:
        raise ValueError("empty key combo")
    return keys


def send_key(session, combo, hold_ms):
    session.command(
        "send-key",
        {
            "keys": parse_key_combo(combo),
            "hold-time": hold_ms,
        },
    )


def load_sequence(path):
    with open(path, "r", encoding="utf-8") as handle:
        data = json.load(handle)
    if isinstance(data, dict):
        data = data.get("steps", [])
    if not isinstance(data, list):
        raise ValueError("sequence must be a JSON list or an object with a steps list")
    return data


def get_session(args):
    session = xemu_qmp.discover_session(args.host, args.port)
    if session is None:
        print("error: no xemu QMP instance found", file=sys.stderr)
    return session


def cmd_tap(args):
    session = get_session(args)
    if session is None:
        return 1
    try:
        send_key(session, args.keys, args.hold_ms)
        print(json.dumps({"ok": True, "keys": args.keys, "hold_ms": args.hold_ms}, indent=2))
        return 0
    except (RuntimeError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    finally:
        session.close()


def cmd_sequence(args):
    try:
        steps = load_sequence(args.path)
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"error: failed to load sequence: {exc}", file=sys.stderr)
        return 1

    session = get_session(args)
    if session is None:
        return 1
    try:
        responses = []
        for index, step in enumerate(steps):
            if not isinstance(step, dict):
                print(f"error: step {index} must be an object", file=sys.stderr)
                return 1
            if "wait" in step or step.get("action") in ("wait", "sleep"):
                duration = float(step.get("wait", step.get("duration", 0.0)))
                time.sleep(max(0.0, duration))
                responses.append({"ok": True, "waited": duration})
                continue
            combo = step.get("keys") or step.get("key") or step.get("tap")
            if not combo:
                print(f"error: step {index} missing keys", file=sys.stderr)
                return 1
            hold_ms = int(step.get("hold_ms", args.hold_ms))
            send_key(session, str(combo), hold_ms)
            responses.append({"ok": True, "keys": combo, "hold_ms": hold_ms})
            if args.step_delay > 0:
                time.sleep(args.step_delay)
        print(json.dumps({"ok": True, "steps": len(steps), "responses": responses}, indent=2))
        return 0
    except (RuntimeError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    finally:
        session.close()


def build_parser():
    parser = argparse.ArgumentParser(description="Send keyboard input to xemu via QMP")
    parser.add_argument("--host", default=xemu_qmp.DEFAULT_HOST)
    parser.add_argument("--port", type=int, default=xemu_qmp.DEFAULT_PORT)
    sub = parser.add_subparsers(dest="command", required=True)

    tap = sub.add_parser("tap", help="Tap a key or key combo, e.g. grave_accent or ctrl+alt+f1")
    tap.add_argument("keys")
    tap.add_argument("--hold-ms", type=int, default=100)
    tap.set_defaults(func=cmd_tap)

    seq = sub.add_parser("sequence", help="Play a JSON keyboard sequence")
    seq.add_argument("path", help="JSON list or object with a steps list")
    seq.add_argument("--hold-ms", type=int, default=100)
    seq.add_argument("--step-delay", type=float, default=0.0)
    seq.set_defaults(func=cmd_sequence)

    return parser


def main():
    parser = build_parser()
    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
