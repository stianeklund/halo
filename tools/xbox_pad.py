#!/usr/bin/env python3
"""Virtual Xbox 360 controller for xemu — LLM-controllable via TCP.

Server (Windows, holds the vgamepad):
    python.exe tools/xbox_pad.py serve
    python.exe tools/xbox_pad.py serve --port 27000

Client (WSL or Windows, no vgamepad needed):
    python3 tools/xbox_pad.py status
    python3 tools/xbox_pad.py tap A
    python3 tools/xbox_pad.py tap A -d 0.5
    python3 tools/xbox_pad.py press Start
    python3 tools/xbox_pad.py release
    python3 tools/xbox_pad.py stick 0.5 -1.0
    python3 tools/xbox_pad.py dpad down
    python3 tools/xbox_pad.py trigger left 0.8
    python3 tools/xbox_pad.py reset

Setup:
    1. Install ViGEmBus driver: https://github.com/nefarius/ViGEmBus/releases
    2. Install vgamepad (Windows Python): pip install vgamepad
    3. Start server: python.exe tools/xbox_pad.py serve
    4. In xemu, map the virtual controller in Settings > Controllers
    5. Use client commands from WSL — localhost forwarding handles the rest

Protocol: newline-delimited JSON over TCP.
    Client sends: {"action":"tap","button":"A","duration":0.1}
    Server replies: {"ok":true,"tapped":"A","duration":0.1}
"""

import argparse
import json
import socket
import sys
import threading
import time

DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 27000
BUFFER_SIZE = 4096

BUTTON_NAMES = {
    "A": "XUSB_GAMEPAD_A",
    "B": "XUSB_GAMEPAD_B",
    "X": "XUSB_GAMEPAD_X",
    "Y": "XUSB_GAMEPAD_Y",
    "LB": "XUSB_GAMEPAD_LEFT_SHOULDER",
    "RB": "XUSB_GAMEPAD_RIGHT_SHOULDER",
    "BACK": "XUSB_GAMEPAD_BACK",
    "START": "XUSB_GAMEPAD_START",
    "LS": "XUSB_GAMEPAD_LEFT_THUMB",
    "RS": "XUSB_GAMEPAD_RIGHT_THUMB",
    "UP": "XUSB_GAMEPAD_DPAD_UP",
    "DOWN": "XUSB_GAMEPAD_DPAD_DOWN",
    "LEFT": "XUSB_GAMEPAD_DPAD_LEFT",
    "RIGHT": "XUSB_GAMEPAD_DPAD_RIGHT",
}


def normalize_button(name):
    n = name.upper().replace("-", "_")
    if n in BUTTON_NAMES:
        return n
    aliases = {
        "L1": "LB", "LSHOULDER": "LB", "LEFT_SHOULDER": "LB",
        "R1": "RB", "RSHOULDER": "RB", "RIGHT_SHOULDER": "RB",
        "L3": "LS", "LTHUMB": "LS", "LEFT_THUMB": "LS",
        "R3": "RS", "RTHUMB": "RS", "RIGHT_THUMB": "RS",
        "SELECT": "BACK",
    }
    return aliases.get(n)


def send_command(host, port, command):
    try:
        sock = socket.create_connection((host, port), timeout=5)
    except ConnectionRefusedError:
        print(
            "error: controller server not running. "
            "Start with: python.exe tools/xbox_pad.py serve",
            file=sys.stderr,
        )
        return None
    except OSError as exc:
        print(f"error: cannot connect to {host}:{port}: {exc}", file=sys.stderr)
        return None

    payload = json.dumps(command, separators=(",", ":")) + "\n"
    sock.sendall(payload.encode("utf-8"))

    sock.settimeout(10)
    data = b""
    while True:
        try:
            chunk = sock.recv(BUFFER_SIZE)
            if not chunk:
                break
            data += chunk
            if b"\n" in data:
                break
        except socket.timeout:
            break

    sock.close()
    if not data:
        return None
    try:
        return json.loads(data.decode("utf-8").strip())
    except json.JSONDecodeError:
        return None


# ─── Server ───────────────────────────────────────────────────────────


def run_server(host, port):
    try:
        import vgamepad as vg
    except ImportError:
        print(
            "error: vgamepad not installed. pip install vgamepad",
            file=sys.stderr,
        )
        print(
            "       ViGEmBus driver: https://github.com/nefarius/ViGEmBus/releases",
            file=sys.stderr,
        )
        return 1

    button_map = {}
    for name, attr in BUTTON_NAMES.items():
        if hasattr(vg.XUSB_BUTTON, attr):
            button_map[name] = getattr(vg.XUSB_BUTTON, attr)

    pad = vg.VX360Gamepad()

    state = {
        "buttons": set(),
        "lx": 0.0,
        "ly": 0.0,
        "rx": 0.0,
        "ry": 0.0,
        "lt": 0.0,
        "rt": 0.0,
    }
    lock = threading.Lock()

    def update_pad():
        pad.reset()
        for btn in state["buttons"]:
            if btn in button_map:
                pad.press_button(button=button_map[btn])
        pad.left_joystick_float(
            max(-1.0, min(1.0, state["lx"])),
            max(-1.0, min(1.0, state["ly"])),
        )
        pad.right_joystick_float(
            max(-1.0, min(1.0, state["rx"])),
            max(-1.0, min(1.0, state["ry"])),
        )
        pad.left_trigger_float(max(0.0, min(1.0, state["lt"])))
        pad.right_trigger_float(max(0.0, min(1.0, state["rt"])))
        pad.update()

    def handle_command(cmd):
        action = cmd.get("action", "")

        if action == "tap":
            button = normalize_button(cmd.get("button", ""))
            duration = float(cmd.get("duration", 0.1))
            if not button or button not in button_map:
                return {"ok": False, "error": f"unknown button: {cmd.get('button')}"}
            with lock:
                state["buttons"].add(button)
                update_pad()
            time.sleep(duration)
            with lock:
                state["buttons"].discard(button)
                update_pad()
            return {"ok": True, "tapped": button, "duration": duration}

        if action == "press":
            button = normalize_button(cmd.get("button", ""))
            if not button or button not in button_map:
                return {"ok": False, "error": f"unknown button: {cmd.get('button')}"}
            with lock:
                state["buttons"].add(button)
                update_pad()
            return {"ok": True, "pressed": button}

        if action == "release":
            target = cmd.get("button")
            with lock:
                if target:
                    b = normalize_button(target)
                    if b:
                        state["buttons"].discard(b)
                    else:
                        state["buttons"].clear()
                else:
                    state["buttons"].clear()
                    state["lx"] = state["ly"] = 0.0
                    state["rx"] = state["ry"] = 0.0
                    state["lt"] = state["rt"] = 0.0
                update_pad()
            return {"ok": True, "released": target or "all"}

        if action == "dpad":
            direction = cmd.get("direction", "").lower()
            duration = float(cmd.get("duration", 0.15))
            dpad_map = {"up": "UP", "down": "DOWN", "left": "LEFT", "right": "RIGHT"}
            if direction not in dpad_map:
                return {"ok": False, "error": f"unknown direction: {direction}"}
            btn = dpad_map[direction]
            with lock:
                state["buttons"].add(btn)
                update_pad()
            time.sleep(duration)
            with lock:
                state["buttons"].discard(btn)
                update_pad()
            return {"ok": True, "dpad": direction, "duration": duration}

        if action == "stick":
            which = cmd.get("which", "left").lower()
            x = float(cmd.get("x", 0.0))
            y = float(cmd.get("y", 0.0))
            with lock:
                if which in ("left", "l"):
                    state["lx"], state["ly"] = x, y
                else:
                    state["rx"], state["ry"] = x, y
                update_pad()
            return {"ok": True, "stick": which, "x": x, "y": y}

        if action == "trigger":
            which = cmd.get("which", "left").lower()
            value = float(cmd.get("value", 0.0))
            with lock:
                if which in ("left", "l", "lt"):
                    state["lt"] = value
                else:
                    state["rt"] = value
                update_pad()
            return {"ok": True, "trigger": which, "value": value}

        if action == "reset":
            with lock:
                state["buttons"].clear()
                state["lx"] = state["ly"] = 0.0
                state["rx"] = state["ry"] = 0.0
                state["lt"] = state["rt"] = 0.0
                update_pad()
            return {"ok": True, "reset": True}

        if action == "status":
            return {
                "ok": True,
                "buttons": sorted(state["buttons"]),
                "lx": state["lx"],
                "ly": state["ly"],
                "rx": state["rx"],
                "ry": state["ry"],
                "lt": state["lt"],
                "rt": state["rt"],
            }

        return {"ok": False, "error": f"unknown action: {action}"}

    def handle_client(client_sock, _addr):
        buf = b""
        try:
            client_sock.settimeout(30)
            while True:
                chunk = client_sock.recv(BUFFER_SIZE)
                if not chunk:
                    break
                buf += chunk
                while b"\n" in buf:
                    line, buf = buf.split(b"\n", 1)
                    if not line.strip():
                        continue
                    try:
                        cmd = json.loads(line.decode("utf-8"))
                    except json.JSONDecodeError as exc:
                        resp = {"ok": False, "error": f"invalid JSON: {exc}"}
                        client_sock.sendall(
                            (json.dumps(resp) + "\n").encode("utf-8")
                        )
                        continue
                    resp = handle_command(cmd)
                    client_sock.sendall(
                        (json.dumps(resp) + "\n").encode("utf-8")
                    )
        except (socket.timeout, ConnectionResetError, BrokenPipeError, OSError):
            pass
        finally:
            try:
                client_sock.close()
            except OSError:
                pass

    server_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_sock.bind((host, port))
    server_sock.listen(5)

    print(f"xbox_pad server on {host}:{port}")
    print("Virtual Xbox 360 controller active.")
    print("Map it in xemu Settings > Controllers.")
    print("Ctrl+C to quit.\n")

    print("Self-test: tapping A for 0.3s ...")
    handle_command({"action": "tap", "button": "A", "duration": 0.3})
    print("Self-test done.\n")

    try:
        while True:
            client_sock, addr = server_sock.accept()
            t = threading.Thread(
                target=handle_client, args=(client_sock, addr), daemon=True
            )
            t.start()
    except KeyboardInterrupt:
        print("\nShutting down.")
    finally:
        server_sock.close()
        del pad

    return 0


# ─── Client Commands ──────────────────────────────────────────────────


def cmd_status(args):
    resp = send_command(args.host, args.port, {"action": "status"})
    if resp is None:
        return 1
    print(json.dumps(resp, indent=2))
    return 0 if resp.get("ok") else 1


def cmd_tap(args):
    resp = send_command(
        args.host,
        args.port,
        {"action": "tap", "button": args.button, "duration": args.duration},
    )
    if resp is None:
        return 1
    print(json.dumps(resp, indent=2))
    return 0 if resp.get("ok") else 1


def cmd_press(args):
    resp = send_command(
        args.host, args.port, {"action": "press", "button": args.button}
    )
    if resp is None:
        return 1
    print(json.dumps(resp, indent=2))
    return 0 if resp.get("ok") else 1


def cmd_release(args):
    resp = send_command(
        args.host,
        args.port,
        {"action": "release", "button": args.button},
    )
    if resp is None:
        return 1
    print(json.dumps(resp, indent=2))
    return 0 if resp.get("ok") else 1


def cmd_dpad(args):
    resp = send_command(
        args.host,
        args.port,
        {"action": "dpad", "direction": args.direction, "duration": args.duration},
    )
    if resp is None:
        return 1
    print(json.dumps(resp, indent=2))
    return 0 if resp.get("ok") else 1


def cmd_stick(args):
    resp = send_command(
        args.host,
        args.port,
        {"action": "stick", "which": args.which, "x": args.x, "y": args.y},
    )
    if resp is None:
        return 1
    print(json.dumps(resp, indent=2))
    return 0 if resp.get("ok") else 1


def cmd_trigger(args):
    resp = send_command(
        args.host,
        args.port,
        {"action": "trigger", "which": args.which, "value": args.value},
    )
    if resp is None:
        return 1
    print(json.dumps(resp, indent=2))
    return 0 if resp.get("ok") else 1


def cmd_reset(args):
    resp = send_command(args.host, args.port, {"action": "reset"})
    if resp is None:
        return 1
    print(json.dumps(resp, indent=2))
    return 0 if resp.get("ok") else 1


def build_parser():
    p = argparse.ArgumentParser(
        description="Virtual Xbox 360 controller for xemu (client/server).",
    )
    p.add_argument("--host", default=DEFAULT_HOST)
    p.add_argument("--port", type=int, default=DEFAULT_PORT)

    sub = p.add_subparsers(dest="command", required=True)

    srv = sub.add_parser("serve", help="Start the controller server (Windows + vgamepad)")
    srv.set_defaults(func=lambda a: run_server(a.host, a.port))

    st = sub.add_parser("status", help="Query controller state")
    st.set_defaults(func=cmd_status)

    tap = sub.add_parser("tap", help="Press and release a button")
    tap.add_argument(
        "button",
        help="Button: A B X Y Start Back LB RB LS RS Up Down Left Right",
    )
    tap.add_argument(
        "-d", "--duration", type=float, default=0.1, help="Hold time (default: 0.1s)",
    )
    tap.set_defaults(func=cmd_tap)

    pr = sub.add_parser("press", help="Hold a button until 'release'")
    pr.add_argument("button")
    pr.set_defaults(func=cmd_press)

    rl = sub.add_parser("release", help="Release button(s)")
    rl.add_argument("button", nargs="?", default=None, help="Specific button, or omit for all")
    rl.set_defaults(func=cmd_release)

    dp = sub.add_parser("dpad", help="Tap a D-pad direction")
    dp.add_argument("direction", choices=["up", "down", "left", "right"])
    dp.add_argument("-d", "--duration", type=float, default=0.15)
    dp.set_defaults(func=cmd_dpad)

    sk = sub.add_parser("stick", help="Set stick position")
    sk.add_argument("x", type=float, help="X axis (-1.0 to 1.0)")
    sk.add_argument("y", type=float, help="Y axis (-1.0 to 1.0)")
    sk.add_argument("--which", default="left", choices=["left", "right", "l", "r"])
    sk.set_defaults(func=cmd_stick)

    tr = sub.add_parser("trigger", help="Set trigger value")
    tr.add_argument("which", choices=["left", "right", "l", "r", "lt", "rt"])
    tr.add_argument("value", type=float, help="0.0 to 1.0")
    tr.set_defaults(func=cmd_trigger)

    rst = sub.add_parser("reset", help="Release all inputs, center sticks")
    rst.set_defaults(func=cmd_reset)

    return p


def main():
    parser = build_parser()
    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
