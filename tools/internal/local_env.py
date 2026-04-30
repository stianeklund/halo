from __future__ import annotations

import sys, os
_tools_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)

import os
import subprocess
import sys
from pathlib import Path


def load_repo_env(filename: str) -> None:
    env_path = Path(__file__).resolve().parent.parent.parent / filename
    if not env_path.is_file():
        return

    for raw_line in env_path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue

        key, value = line.split("=", 1)
        key = key.strip()
        value = value.strip()
        if not key:
            continue

        if len(value) >= 2 and value[0] == value[-1] and value[0] in {'"', "'"}:
            value = value[1:-1]

        os.environ.setdefault(key, value)


def is_wsl() -> bool:
    return sys.platform.startswith("linux") and "WSL_INTEROP" in os.environ


def to_windows_path(path: str) -> str:
    path = os.path.abspath(path).replace("\\", "/")
    if (
        len(path) >= 7
        and path.startswith("/mnt/")
        and path[5].isalpha()
        and path[6] == "/"
    ):
        return f"{path[5].upper()}:{path[6:]}"
    return path


def find_windows_python() -> str | None:
    configured = os.environ.get("WINDOWS_PYTHON", "").strip()
    if configured:
        return configured

    candidates = [
        "/mnt/c/WINDOWS/system32/cmd.exe",
        "/mnt/c/Windows/System32/cmd.exe",
    ]
    for candidate in candidates:
        if os.path.isfile(candidate):
            return candidate
    return None


def build_windows_python_command(script_path: str, script_args: list[str]) -> list[str] | None:
    python_exe = find_windows_python()
    if python_exe is None:
        return None

    command: list[str]
    lower_name = os.path.basename(python_exe).lower()
    if lower_name == "cmd.exe":
        command = [python_exe, "/c", "py", "-3"]
    else:
        command = [python_exe]
        if lower_name == "py.exe":
            command.append("-3")
    command.append(to_windows_path(script_path))
    command.extend(script_args)
    return command


def maybe_reexec_on_windows(script_path: str, script_args: list[str] | None = None) -> None:
    if not is_wsl() or os.environ.get("HALO_WINDOWS_REEXEC") == "1":
        return

    command = build_windows_python_command(script_path, script_args or sys.argv[1:])
    if command is None:
        return

    env = os.environ.copy()
    env["HALO_WINDOWS_REEXEC"] = "1"
    raise SystemExit(subprocess.run(command, env=env, check=False).returncode)
