#!/usr/bin/env python3

from __future__ import annotations

import sys, os
_tools_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)

import argparse
import hashlib
from dataclasses import dataclass
from pathlib import Path


ROOT_DIR = Path(__file__).resolve().parent.parent.parent
CLAUDE_DIR = ROOT_DIR / ".claude"
OPENCODE_DIR = ROOT_DIR / ".opencode"

COMMAND_ALIASES = {
    "load-iso.md": "load-xemu-with-iso.md",
}


@dataclass(frozen=True)
class MarkdownFile:
    path: Path
    relative_path: str
    frontmatter: str
    body: str
    raw_text: str


def normalize_text(text: str) -> str:
    lines = [line.rstrip() for line in text.replace("\r\n", "\n").split("\n")]
    return "\n".join(lines).strip()


def split_frontmatter(text: str) -> tuple[str, str]:
    normalized = text.replace("\r\n", "\n")
    if not normalized.startswith("---\n"):
        return "", normalized.strip()

    parts = normalized.split("\n---\n", 1)
    if len(parts) != 2:
        return "", normalized.strip()

    frontmatter = parts[0][4:]
    body = parts[1]
    return frontmatter.strip(), body.strip()


def read_markdown_file(path: Path, root: Path) -> MarkdownFile:
    raw_text = path.read_text(encoding="utf-8")
    frontmatter, body = split_frontmatter(raw_text)
    return MarkdownFile(
        path=path,
        relative_path=path.relative_to(root).as_posix(),
        frontmatter=normalize_text(frontmatter),
        body=normalize_text(body),
        raw_text=normalize_text(raw_text),
    )


def collect_commands(base_dir: Path) -> dict[str, MarkdownFile]:
    commands_dir = base_dir / "commands"
    return {
        path.name: read_markdown_file(path, base_dir)
        for path in sorted(commands_dir.glob("*.md"))
    }


def collect_skills(base_dir: Path) -> dict[str, MarkdownFile]:
    skills_dir = base_dir / "skills"
    return {
        path.parent.name: read_markdown_file(path, base_dir)
        for path in sorted(skills_dir.glob("*/SKILL.md"))
    }


def sha256(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()[:12]


def command_partner_name(claude_name: str) -> str:
    return COMMAND_ALIASES.get(claude_name, claude_name)


def format_status(label: str, left: MarkdownFile, right: MarkdownFile | None) -> str:
    if right is None:
        return f"{label}: {left.relative_path} -> missing"

    if left.raw_text == right.raw_text:
        return f"{label}: {left.relative_path} == {right.relative_path}"

    if left.body == right.body:
        return (
            f"{label}: {left.relative_path} ~= {right.relative_path} "
            f"(body matches, wrapper/frontmatter differs)"
        )

    return (
        f"DIFF: {left.relative_path} != {right.relative_path} "
        f"(body {sha256(left.body)} vs {sha256(right.body)})"
    )


def audit_commands() -> tuple[list[str], int, int]:
    claude_commands = collect_commands(CLAUDE_DIR)
    opencode_commands = collect_commands(OPENCODE_DIR)
    lines: list[str] = []
    mismatches = 0
    wrappers = 0

    for claude_name, claude_file in sorted(claude_commands.items()):
        partner_name = command_partner_name(claude_name)
        opencode_file = opencode_commands.get(partner_name)
        line = format_status("command", claude_file, opencode_file)
        lines.append(line)
        if opencode_file is None or claude_file.body != opencode_file.body:
            if opencode_file is None or claude_file.raw_text != opencode_file.raw_text:
                mismatches += 1
        elif claude_file.raw_text != opencode_file.raw_text:
            wrappers += 1

    claude_partner_names = {command_partner_name(name) for name in claude_commands}
    for opencode_name, opencode_file in sorted(opencode_commands.items()):
        if opencode_name not in claude_partner_names:
            lines.append(f"command: missing in .claude -> {opencode_file.relative_path}")
            mismatches += 1

    return lines, mismatches, wrappers


def audit_skills() -> tuple[list[str], int, int]:
    claude_skills = collect_skills(CLAUDE_DIR)
    opencode_skills = collect_skills(OPENCODE_DIR)
    lines: list[str] = []
    mismatches = 0
    wrappers = 0

    for skill_name, claude_file in sorted(claude_skills.items()):
        opencode_file = opencode_skills.get(skill_name)
        line = format_status("skill", claude_file, opencode_file)
        lines.append(line)
        if opencode_file is None or claude_file.body != opencode_file.body:
          if opencode_file is None or claude_file.raw_text != opencode_file.raw_text:
              mismatches += 1
        elif claude_file.raw_text != opencode_file.raw_text:
            wrappers += 1

    for skill_name, opencode_file in sorted(opencode_skills.items()):
        if skill_name not in claude_skills:
            lines.append(f"skill: missing in .claude -> {opencode_file.relative_path}")
            mismatches += 1

    return lines, mismatches, wrappers


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Audit drift between .claude and .opencode command and skill content.",
    )
    parser.add_argument(
        "--strict",
        action="store_true",
        help="return non-zero if any content mismatch or missing file is found",
    )
    return parser


def main() -> int:
    args = build_parser().parse_args()

    command_lines, command_mismatches, command_wrappers = audit_commands()
    skill_lines, skill_mismatches, skill_wrappers = audit_skills()
    mismatches = command_mismatches + skill_mismatches

    print("Agent content audit")
    print(f"  commands: {len(command_lines)} checked")
    print(f"  skills:   {len(skill_lines)} checked")
    print(f"  wrapper-only differences: {command_wrappers + skill_wrappers}")
    print(f"  content mismatches/missing files: {mismatches}")
    print()
    print("Commands")
    for line in command_lines:
        print(f"  {line}")
    print()
    print("Skills")
    for line in skill_lines:
        print(f"  {line}")

    if args.strict and mismatches:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
