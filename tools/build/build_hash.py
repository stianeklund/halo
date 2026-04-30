import sys, os
_tools_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)

import hashlib
import os
import subprocess


def sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while True:
            chunk = f.read(1 << 16)
            if not chunk:
                break
            h.update(chunk)
    return h.hexdigest()


def git_rev() -> str:
    try:
        result = subprocess.run(
            ["git", "describe", "--always"],
            capture_output=True,
            text=True,
        )
        if result.returncode == 0 and result.stdout.strip():
            return result.stdout.strip()
    except FileNotFoundError:
        pass
    return "unknown"


def print_build_hash(path: str) -> None:
    if not os.path.isfile(path):
        return
    digest = sha256_file(path)
    size = os.path.getsize(path)
    name = os.path.basename(path)
    rev = git_rev()
    print(f"build: {name}  sha256={digest[:16]}  size={size:,}  rev={rev}")
