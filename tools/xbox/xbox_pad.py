#!/usr/bin/env python3
import sys, os
_tools_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)

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
import subprocess
import sys
import tempfile
import threading
import time

from internal.local_env import build_windows_python_command, is_wsl

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


def send_command(host, port, command, quiet=False):
    try:
        sock = socket.create_connection((host, port), timeout=5)
    except ConnectionRefusedError:
        if not quiet:
            print(
                "error: controller server not running. "
                "Start with: python.exe tools/xbox/xbox_pad.py serve",
                file=sys.stderr,
            )
        return None
    except OSError as exc:
        if not quiet:
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


def server_status(host, port):
    return send_command(host, port, {"action": "status"}, quiet=True)


def server_command(host, port):
    script_path = os.path.abspath(__file__)
    script_args = ["--host", host, "--port", str(port), "serve"]
    if is_wsl():
        return build_windows_python_command(script_path, script_args)
    return [sys.executable, script_path] + script_args


def start_server_process(host, port):
    command = server_command(host, port)
    if command is None:
        return None, "could not locate Windows Python for controller server startup"

    log_path = os.path.join(tempfile.gettempdir(), f"xbox_pad_{port}.log")
    log = open(log_path, "ab", buffering=0)
    try:
        subprocess.Popen(
            command,
            stdout=log,
            stderr=subprocess.STDOUT,
            stdin=subprocess.DEVNULL,
            close_fds=True,
            start_new_session=(os.name != "nt"),
        )
    except OSError as exc:
        log.close()
        return None, f"failed to start controller server: {exc}"
    log.close()
    return log_path, None


def ensure_server(host, port, start=True, timeout=8.0):
    status = server_status(host, port)
    if status and status.get("ok"):
        status["running"] = True
        status["started"] = False
        return status

    if not start:
        return {"ok": False, "running": False, "started": False}

    log_path, error = start_server_process(host, port)
    if error:
        return {"ok": False, "running": False, "started": False, "error": error}

    deadline = time.time() + timeout
    while time.time() < deadline:
        time.sleep(0.25)
        status = server_status(host, port)
        if status and status.get("ok"):
            status["running"] = True
            status["started"] = True
            status["log"] = log_path
            return status

    return {
        "ok": False,
        "running": False,
        "started": True,
        "error": "controller server did not become ready before timeout",
        "log": log_path,
    }


def maybe_auto_start(args):
    if not getattr(args, "auto_start", False):
        return True
    result = ensure_server(args.host, args.port, start=True)
    if result.get("ok"):
        return True
    print(json.dumps(result, indent=2), file=sys.stderr)
    return False


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


def cmd_ensure(args):
    resp = ensure_server(args.host, args.port, start=not args.no_start,
                         timeout=args.timeout)
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


def load_sequence(path):
    with open(path, "r", encoding="utf-8") as handle:
        data = json.load(handle)
    if isinstance(data, dict):
        data = data.get("steps", [])
    if not isinstance(data, list):
        raise ValueError("sequence must be a JSON list or an object with a steps list")
    return data


def normalize_step(step):
    if not isinstance(step, dict):
        raise ValueError(f"sequence step must be an object: {step!r}")
    if "wait" in step:
        return {"action": "wait", "duration": float(step["wait"])}
    if "sleep" in step:
        return {"action": "wait", "duration": float(step["sleep"])}
    if "tap" in step:
        return {
            "action": "tap",
            "button": step["tap"],
            "duration": float(step.get("duration", 0.1)),
        }
    if "dpad" in step:
        return {
            "action": "dpad",
            "direction": step["dpad"],
            "duration": float(step.get("duration", 0.15)),
        }
    return step


def cmd_sequence(args):
    try:
        steps = load_sequence(args.path)
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"error: failed to load sequence: {exc}", file=sys.stderr)
        return 1

    responses = []
    for index, raw_step in enumerate(steps):
        try:
            step = normalize_step(raw_step)
        except ValueError as exc:
            print(f"error: step {index}: {exc}", file=sys.stderr)
            return 1
        action = step.get("action", "")
        if action in ("wait", "sleep"):
            duration = float(step.get("duration", 0.0))
            time.sleep(max(0.0, duration))
            responses.append({"ok": True, "waited": duration})
            continue
        resp = send_command(args.host, args.port, step)
        if resp is None:
            return 1
        responses.append(resp)
        if not resp.get("ok") and not args.keep_going:
            print(json.dumps({"ok": False, "failed_step": index, "responses": responses}, indent=2))
            return 1
        if args.step_delay > 0:
            time.sleep(args.step_delay)

    print(json.dumps({"ok": True, "steps": len(steps), "responses": responses}, indent=2))
    return 0


def build_parser():
    p = argparse.ArgumentParser(
        description="Virtual Xbox 360 controller for xemu (client/server).",
    )
    p.add_argument("--host", default=DEFAULT_HOST)
    p.add_argument("--port", type=int, default=DEFAULT_PORT)
    p.add_argument(
        "--auto-start",
        action="store_true",
        help="Start the controller server if a client command cannot reach it",
    )

    sub = p.add_subparsers(dest="command", required=True)

    srv = sub.add_parser("serve", help="Start the controller server (Windows + vgamepad)")
    srv.set_defaults(func=lambda a: run_server(a.host, a.port))

    st = sub.add_parser("status", help="Query controller state")
    st.set_defaults(func=cmd_status)

    ens = sub.add_parser("ensure", help="Ensure the controller server is running")
    ens.add_argument("--no-start", action="store_true", help="Only check; do not start")
    ens.add_argument("--timeout", type=float, default=8.0, help="Seconds to wait after start")
    ens.set_defaults(func=cmd_ensure)

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

    seq = sub.add_parser("sequence", help="Play an agent-neutral JSON input sequence")
    seq.add_argument("path", help="JSON list or object with a steps list")
    seq.add_argument("--step-delay", type=float, default=0.0,
                     help="Extra delay after each non-wait step")
    seq.add_argument("--keep-going", action="store_true",
                     help="Continue after a failed controller command")
    seq.set_defaults(func=cmd_sequence)

    return p


def main():
    parser = build_parser()
    args = parser.parse_args()
    if args.command not in ("serve", "ensure") and not maybe_auto_start(args):
        return 1
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
