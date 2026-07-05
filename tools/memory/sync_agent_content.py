#!/usr/bin/env python3
"""Sync OpenCode agent content to Antigravity/Gemini skill directories.

The repo keeps OpenCode commands and skills under .opencode/. Antigravity and
Gemini discover skills from SKILL.md files, so this tool materializes:

  - .opencode/skills/*/SKILL.md as normal skills
  - .opencode/commands/*.md as opencode-command-* wrapper skills
  - .opencode/agents/*.md as opencode-agent-* persona wrapper skills

Workspace sync is the default. Global/shared destinations are opt-in so local
repo maintenance does not unexpectedly write to a user's home directory.
"""

from __future__ import annotations

import argparse
import re
import shutil
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent.parent.parent
OPENCODE_SKILLS = REPO_ROOT / ".opencode" / "skills"
OPENCODE_COMMANDS = REPO_ROOT / ".opencode" / "commands"
OPENCODE_AGENTS = REPO_ROOT / ".opencode" / "agents"

DESTINATIONS = {
    "workspace": REPO_ROOT / ".agents" / "skills",
    "global": Path.home() / ".gemini" / "antigravity-cli" / "skills",
    "shared": Path.home() / ".gemini" / "skills",
}


def split_frontmatter(text: str) -> tuple[dict[str, str], str]:
    if not text.startswith("---\n"):
        return {}, text
    end = text.find("\n---", 4)
    if end == -1:
        return {}, text

    raw = text[4:end]
    body = text[end + len("\n---") :].lstrip("\n")
    meta: dict[str, str] = {}
    lines = raw.splitlines()
    i = 0
    while i < len(lines):
        line = lines[i]
        match = re.match(r"^([A-Za-z0-9_-]+):\s*(.*)$", line)
        if not match:
            i += 1
            continue
        key, value = match.groups()
        value = value.strip()
        if value in {">", ">-", "|", "|-"}:
            block: list[str] = []
            i += 1
            while i < len(lines) and not re.match(r"^[A-Za-z0-9_-]+:\s*", lines[i]):
                block.append(lines[i].strip())
                i += 1
            meta[key] = " ".join(part for part in block if part)
            continue
        meta[key] = value.strip('"')
        i += 1
    return meta, body


def skill_name_from_file(path: Path) -> str:
    meta, _ = split_frontmatter(path.read_text(encoding="utf-8"))
    return meta.get("name") or path.parent.name


def command_skill_name(command_name: str) -> str:
    return "opencode-command-" + command_name.replace("_", "-").lower()


def agent_skill_name(agent_name: str) -> str:
    return "opencode-agent-" + agent_name.replace("_", "-").lower()


def yaml_quote(value: str) -> str:
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


def ensure_dir(path: Path) -> None:
    if path.exists() and not path.is_dir():
        path.unlink()
    path.mkdir(parents=True, exist_ok=True)


def sync_skill_dir(src_dir: Path, dest_root: Path) -> Path:
    skill_file = src_dir / "SKILL.md"
    name = skill_name_from_file(skill_file)
    dest_dir = dest_root / name
    ensure_dir(dest_dir)
    shutil.copytree(src_dir, dest_dir, dirs_exist_ok=True)
    return dest_dir


def render_command_skill(command_path: Path) -> str:
    text = command_path.read_text(encoding="utf-8")
    meta, body = split_frontmatter(text)
    command = command_path.stem
    skill_name = command_skill_name(command)
    description = meta.get("description") or f"OpenCode /{command} command workflow"
    skill_description = (
        f"Antigravity/Gemini wrapper for OpenCode /{command}. Use when the user "
        f"asks for /{command}, the {command} command, or says: {description}"
    )
    return f"""---
name: {skill_name}
description: {yaml_quote(skill_description)}
---

# OpenCode Command: /{command}

This skill ports the existing OpenCode command `.opencode/commands/{command}.md`
to Antigravity/Gemini.

When invoked:

- Treat the user's text after `/{command}` or after the skill request as `$ARGUMENTS`.
- Follow the command prompt below as the task instructions.
- Keep using the repo's normal `AGENTS.md` and skill doctrine.

## Command Prompt

{body.rstrip()}
"""


def sync_command(command_path: Path, dest_root: Path) -> Path:
    dest_dir = dest_root / command_skill_name(command_path.stem)
    ensure_dir(dest_dir)
    (dest_dir / "SKILL.md").write_text(render_command_skill(command_path), encoding="utf-8")
    return dest_dir


def render_agent_skill(agent_path: Path) -> str:
    text = agent_path.read_text(encoding="utf-8")
    meta, body = split_frontmatter(text)
    agent = meta.get("name") or agent_path.stem
    skill_name = agent_skill_name(agent)
    description = meta.get("description") or f"OpenCode {agent} agent persona"
    skill_description = (
        f"Antigravity/Gemini wrapper for OpenCode agent {agent}. Use when the user "
        f"asks for the {agent} agent/persona or needs: {description}"
    )
    return f"""---
name: {skill_name}
description: {yaml_quote(skill_description)}
---

# OpenCode Agent: {agent}

This skill ports the existing OpenCode agent `.opencode/agents/{agent_path.name}`
to Antigravity/Gemini.

When invoked, adopt the persona and task instructions below for the current task.
Keep using the repo's normal `AGENTS.md` and skill doctrine.

## Agent Prompt

{body.rstrip()}
"""


def sync_agent(agent_path: Path, dest_root: Path) -> Path:
    meta, _ = split_frontmatter(agent_path.read_text(encoding="utf-8"))
    agent = meta.get("name") or agent_path.stem
    dest_dir = dest_root / agent_skill_name(agent)
    ensure_dir(dest_dir)
    (dest_dir / "SKILL.md").write_text(render_agent_skill(agent_path), encoding="utf-8")
    return dest_dir


def sync_to(dest_root: Path) -> tuple[int, int, int]:
    ensure_dir(dest_root)

    skill_count = 0
    for skill_file in sorted(OPENCODE_SKILLS.glob("*/SKILL.md")):
        sync_skill_dir(skill_file.parent, dest_root)
        skill_count += 1

    command_count = 0
    for command_path in sorted(OPENCODE_COMMANDS.glob("*.md")):
        sync_command(command_path, dest_root)
        command_count += 1

    agent_count = 0
    for agent_path in sorted(OPENCODE_AGENTS.glob("*.md")):
        sync_agent(agent_path, dest_root)
        agent_count += 1

    return skill_count, command_count, agent_count


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--target",
        choices=sorted(DESTINATIONS),
        action="append",
        default=None,
        help="Destination to sync. May be repeated. Defaults to workspace.",
    )
    parser.add_argument(
        "--all",
        action="store_true",
        help="Sync workspace, global Antigravity, and shared Gemini skill directories.",
    )
    args = parser.parse_args()

    targets = sorted(DESTINATIONS) if args.all else (args.target or ["workspace"])
    for target in targets:
        skills, commands, agents = sync_to(DESTINATIONS[target])
        print(
            f"{target}: synced {skills} skills, {commands} command wrappers, "
            f"and {agents} agent wrappers to {DESTINATIONS[target]}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
