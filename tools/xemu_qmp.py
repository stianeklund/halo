#!/usr/bin/env python3

import argparse
import json
import os
import socket
import subprocess
import sys
import time

from local_env import to_windows_path


ROOT_DIR = os.path.abspath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
)
DEFAULT_HOST = os.environ.get("XEMU_QMP_HOST", "localhost")
DEFAULT_PORT = int(os.environ.get("XEMU_QMP_PORT", "4444"))
DEFAULT_DEVICE = "ide0-1-0"
DISCOVERY_PORTS = [DEFAULT_PORT] + [port for port in range(4444, 4456)
                                    if port != DEFAULT_PORT]


def discovery_hosts(host: str | None = None) -> list[str]:
    candidates = [host or DEFAULT_HOST, "localhost", "127.0.0.1"]
    ordered = []
    seen: set[str] = set()
    for candidate in candidates:
        if candidate not in seen:
            ordered.append(candidate)
            seen.add(candidate)
    return ordered


def repo_path(path: str) -> str:
    if os.path.isabs(path):
        return path
    return os.path.abspath(os.path.join(ROOT_DIR, path))


def iso_path_candidates(path: str) -> list[str]:
    absolute = repo_path(path)
    candidates = [to_windows_path(absolute), absolute]
    seen: set[str] = set()
    ordered = []
    for candidate in candidates:
        if candidate not in seen:
            ordered.append(candidate)
            seen.add(candidate)
    return ordered


class QmpSession:
    def __init__(self, sock: socket.socket, fileobj, host: str, port: int):
        self.sock = sock
        self.fileobj = fileobj
        self.host = host
        self.port = port
        self.next_id = 1
        self.greeting = None

    def close(self) -> None:
        try:
            self.fileobj.close()
        except OSError:
            pass
        try:
            self.sock.close()
        except OSError:
            pass

    def read_message(self) -> dict | None:
        line = self.fileobj.readline()
        if not line:
            return None
        return json.loads(line)

    def send(self, message: dict) -> None:
        payload = json.dumps(message, separators=(",", ":")) + "\r\n"
        self.fileobj.write(payload.encode("utf-8"))
        self.fileobj.flush()

    def command(self, name: str, arguments: dict | None = None) -> dict:
        qmp_id = self.next_id
        self.next_id += 1
        request = {"execute": name, "id": qmp_id}
        if arguments:
            request["arguments"] = arguments
        self.send(request)

        while True:
            response = self.read_message()
            if response is None:
                raise RuntimeError(f"QMP disconnected while waiting for {name}")
            if response.get("id") == qmp_id:
                if "error" in response:
                    raise RuntimeError(response["error"].get("desc", f"{name} failed"))
                return response.get("return", {})


def connect_qmp(host: str, port: int, timeout: float = 1.0) -> QmpSession | None:
    try:
        sock = socket.create_connection((host, port), timeout=timeout)
    except OSError:
        return None

    try:
        sock.settimeout(timeout)
        fileobj = sock.makefile("rwb")
        session = QmpSession(sock, fileobj, host, port)
        greeting = session.read_message()
        if not greeting or "QMP" not in greeting:
            session.close()
            return None
        session.greeting = greeting
        session.command("qmp_capabilities")
        return session
    except (OSError, RuntimeError, json.JSONDecodeError):
        try:
            sock.close()
        except OSError:
            pass
        return None


def discover_session(host: str, port: int | None = None) -> QmpSession | None:
    ports = [port] if port is not None else DISCOVERY_PORTS
    for candidate_host in discovery_hosts(host):
        for candidate_port in ports:
            session = connect_qmp(candidate_host, candidate_port)
            if session is not None:
                return session
    return None


def launch_xemu(iso_path: str | None) -> bool:
    script = os.path.join(ROOT_DIR, "tools", "xemu.sh")
    command = ["bash", script, "-q"]
    if iso_path is not None:
        command.append(repo_path(iso_path))

    try:
        subprocess.Popen(
            command,
            cwd=ROOT_DIR,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            start_new_session=True,
        )
    except OSError as exc:
        print(f"failed to launch xemu: {exc}", file=sys.stderr)
        return False
    return True


def wait_for_session(host: str, port: int | None, timeout_secs: float) -> QmpSession | None:
    deadline = time.time() + timeout_secs
    while time.time() < deadline:
        session = discover_session(host, port)
        if session is not None:
            return session
        time.sleep(0.5)
    return None


def get_session(args, iso_path: str | None = None) -> QmpSession | None:
    session = discover_session(args.host, args.port)
    if session is not None:
        return session
    if not args.launch_if_missing:
        return None
    if not launch_xemu(iso_path):
        return None
    return wait_for_session(args.host, args.port, args.launch_timeout)


def detect_cdrom_device(session: QmpSession) -> dict | None:
    try:
        devices = session.command("query-block")
    except RuntimeError:
        return None

    for device in devices:
        inserted = device.get("inserted")
        if device.get("removable") and device.get("device"):
            return device
        if inserted and inserted.get("image") and device.get("device"):
            image = inserted["image"]
            if image.get("drv") == "raw" and device.get("tray_open") is not None:
                return device
    return None


def get_block_info_by_device(session: QmpSession, device: str) -> dict | None:
    try:
        devices = session.command("query-block")
    except RuntimeError:
        return None

    for entry in devices:
        if entry.get("device") == device:
            return entry
    return None


def resolve_device(session: QmpSession, requested_device: str | None) -> tuple[str, dict | None]:
    if requested_device:
        return requested_device, None

    detected = detect_cdrom_device(session)
    if detected is not None:
        return detected.get("device", DEFAULT_DEVICE), detected

    return DEFAULT_DEVICE, None


def change_medium(session: QmpSession, device: str, block_info: dict | None,
                  filename: str) -> None:
    attempts = []
    if block_info and block_info.get("qdev"):
        attempts.append({"id": block_info["qdev"]})
    attempts.append({"device": device})
    attempts.append({"id": device})

    errors = []
    for selector in attempts:
        arguments = {
            "filename": filename,
            "format": "raw",
            "force": True,
        }
        arguments.update(selector)
        try:
            session.command("blockdev-change-medium", arguments)
            return
        except RuntimeError as exc:
            errors.append(str(exc))

    raise RuntimeError("; ".join(errors))


def eject_medium(session: QmpSession, device: str, block_info: dict | None) -> None:
    attempts = []
    if block_info and block_info.get("qdev"):
        attempts.append({"id": block_info["qdev"], "force": True})
    attempts.append({"device": device, "force": True})
    attempts.append({"id": device, "force": True})

    errors = []
    for arguments in attempts:
        try:
            session.command("eject", arguments)
            return
        except RuntimeError as exc:
            errors.append(str(exc))

    raise RuntimeError("; ".join(errors))


def command_status(args) -> int:
    session = get_session(args)
    if session is None:
        print("no xemu QMP instance found", file=sys.stderr)
        return 1

    try:
        status = session.command("query-status")
        version = session.greeting.get("QMP", {}).get("version", {})
        print(f"connected: {session.host}:{session.port}")
        if version:
            print(json.dumps(version, indent=2))
        print(json.dumps(status, indent=2))
        return 0
    finally:
        session.close()


def command_load_iso(args) -> int:
    if not os.path.exists(repo_path(args.iso_path)):
        print(f"missing ISO: {repo_path(args.iso_path)}", file=sys.stderr)
        return 1

    session = get_session(args, iso_path=args.iso_path)
    if session is None:
        print("no xemu QMP instance found", file=sys.stderr)
        return 1

    try:
        device, block_info = resolve_device(session, args.device)
        last_error = None
        for candidate in iso_path_candidates(args.iso_path):
            try:
                change_medium(session, device, block_info, candidate)
                print(f"loaded ISO on {session.host}:{session.port} via {device}: {candidate}")
                if args.reset:
                    session.command("system_reset")
                    print("reset VM")
                return 0
            except RuntimeError as exc:
                last_error = exc

        print(f"failed to load ISO: {last_error}", file=sys.stderr)
        return 1
    finally:
        session.close()


def command_reset(args) -> int:
    session = get_session(args)
    if session is None:
        print("no xemu QMP instance found", file=sys.stderr)
        return 1
    try:
        session.command("system_reset")
        print("reset VM")
        return 0
    finally:
        session.close()


def command_stop(args) -> int:
    session = get_session(args)
    if session is None:
        print("no xemu QMP instance found", file=sys.stderr)
        return 1
    try:
        session.command("stop")
        print("stopped VM")
        return 0
    finally:
        session.close()


def command_cont(args) -> int:
    session = get_session(args)
    if session is None:
        print("no xemu QMP instance found", file=sys.stderr)
        return 1
    try:
        session.command("cont")
        print("resumed VM")
        return 0
    finally:
        session.close()


def command_eject(args) -> int:
    session = get_session(args)
    if session is None:
        print("no xemu QMP instance found", file=sys.stderr)
        return 1
    try:
        device, block_info = resolve_device(session, args.device)
        if block_info is None:
            block_info = get_block_info_by_device(session, device)

        if block_info is not None:
            inserted = block_info.get("inserted")
            tray_open = block_info.get("tray_open")
            if not inserted and tray_open:
                print(f"media already ejected from {device}")
                return 0

        eject_medium(session, device, block_info)

        updated = get_block_info_by_device(session, device)
        if updated is not None and updated.get("inserted"):
            print(f"eject command succeeded but media remains inserted in {device}", file=sys.stderr)
            return 1

        print(f"ejected media from {device}")
        return 0
    finally:
        session.close()


def command_hmp(args) -> int:
    session = get_session(args)
    if session is None:
        print("no xemu QMP instance found", file=sys.stderr)
        return 1
    try:
        result = session.command(
            "human-monitor-command",
            {"command-line": args.monitor_command},
        )
        if isinstance(result, str):
            print(result)
        else:
            print(json.dumps(result, indent=2))
        return 0
    finally:
        session.close()


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Interact with xemu via QMP with auto-discovery.",
    )
    parser.add_argument("--host", default=DEFAULT_HOST,
                        help=f"QMP host (default: {DEFAULT_HOST})")
    parser.add_argument("--port", type=int,
                        help="specific QMP port; otherwise scan common xemu ports")
    parser.add_argument("--launch-if-missing", action="store_true",
                        help="launch tools/xemu.sh -q if no QMP instance is found")
    parser.add_argument("--launch-timeout", type=float, default=20.0,
                        help="seconds to wait for xemu after launching")

    subparsers = parser.add_subparsers(dest="command", required=True)

    status_parser = subparsers.add_parser("status", help="show QMP connection and VM status")
    status_parser.set_defaults(func=command_status)

    load_parser = subparsers.add_parser("load-iso", help="swap ISO into the CD-ROM drive")
    load_parser.add_argument("iso_path", help="ISO path, repo-relative or absolute")
    load_parser.add_argument("--device",
                             help="removable device id; auto-detected if omitted")
    load_parser.add_argument("--reset", action="store_true",
                             help="reset the VM after loading the ISO")
    load_parser.set_defaults(func=command_load_iso)

    reset_parser = subparsers.add_parser("reset", help="reset the VM")
    reset_parser.set_defaults(func=command_reset)

    stop_parser = subparsers.add_parser("stop", help="pause the VM")
    stop_parser.set_defaults(func=command_stop)

    cont_parser = subparsers.add_parser("cont", help="resume the VM")
    cont_parser.set_defaults(func=command_cont)

    eject_parser = subparsers.add_parser("eject", help="eject removable media")
    eject_parser.add_argument("--device",
                              help="removable device id; auto-detected if omitted")
    eject_parser.set_defaults(func=command_eject)

    hmp_parser = subparsers.add_parser("hmp", help="run a human monitor command via QMP")
    hmp_parser.add_argument("monitor_command", help="monitor command string")
    hmp_parser.set_defaults(func=command_hmp)

    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
