#!/usr/bin/env python3

import sys, os
_tools_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)

from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parent.parent.parent
FILES = (ROOT / "AGENTS.md", ROOT / "CLAUDE.md")
MAX_LINES = 250


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def line_count(text: str) -> int:
    return len(text.splitlines())


def main() -> int:
    missing = [str(path) for path in FILES if not path.exists()]
    if missing:
        print("Missing required files:")
        for path in missing:
            print(f"- {path}")
        return 1

    agents_text = read_text(FILES[0])
    claude_text = read_text(FILES[1])

    failed = False

    for path, text in ((FILES[0], agents_text), (FILES[1], claude_text)):
        lines = line_count(text)
        if lines > MAX_LINES:
            print(f"{path.name} has {lines} lines (limit: {MAX_LINES}).")
            failed = True

    if agents_text != claude_text:
        print("AGENTS.md and CLAUDE.md are out of sync.")
        failed = True

    if failed:
        return 1

    print(
        f"OK: AGENTS.md and CLAUDE.md are identical and <= {MAX_LINES} lines."
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
