#!/usr/bin/env python3

from __future__ import annotations

import sys, os
_tools_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)

import argparse
from dataclasses import dataclass
from pathlib import Path


ROOT_DIR = Path(__file__).resolve().parent.parent.parent
SHARED_DIR = ROOT_DIR / "agent-content"

CLAUDE_DIR = ROOT_DIR / ".claude"
OPENCODE_DIR = ROOT_DIR / ".opencode"


@dataclass(frozen=True)
class OutputSpec:
    relative_path: str


COMMAND_OUTPUTS = {
    "analyze_screenshot.md": {
        "claude": OutputSpec("commands/analyze_screenshot.md"),
        "opencode": OutputSpec("commands/analyze_screenshot.md"),
    },
    "auto-lift.md": {
        "claude": OutputSpec("commands/auto-lift.md"),
        "opencode": OutputSpec("commands/auto-lift.md"),
    },
    "build.md": {
        "claude": OutputSpec("commands/build.md"),
        "opencode": OutputSpec("commands/build.md"),
    },
    "debug-regression.md": {
        "claude": OutputSpec("commands/debug-regression.md"),
        "opencode": OutputSpec("commands/debug-regression.md"),
    },
    "deploy.md": {
        "claude": OutputSpec("commands/deploy.md"),
        "opencode": OutputSpec("commands/deploy.md"),
    },
    "frontier.md": {
        "claude": OutputSpec("commands/frontier.md"),
        "opencode": OutputSpec("commands/frontier.md"),
    },
    "lift.md": {
        "claude": OutputSpec("commands/lift.md"),
        "opencode": OutputSpec("commands/lift.md"),
    },
    "maintain.md": {
        "claude": OutputSpec("commands/maintain.md"),
        "opencode": OutputSpec("commands/maintain.md"),
    },
    "review-deep.md": {
        "claude": OutputSpec("commands/review-deep.md"),
        "opencode": OutputSpec("commands/review-deep.md"),
    },
    "verify.md": {
        "claude": OutputSpec("commands/verify.md"),
        "opencode": OutputSpec("commands/verify.md"),
    },
    "xbdm.md": {
        "claude": OutputSpec("commands/xbdm.md"),
        "opencode": OutputSpec("commands/xbdm.md"),
    },
    "xemu.md": {
        "claude": OutputSpec("commands/xemu.md"),
        "opencode": OutputSpec("commands/xemu.md"),
    },
}

SKILL_OUTPUTS = {
    "halo-deploy-xbdm": {
        "claude": OutputSpec("skills/halo-deploy-xbdm/SKILL.md"),
        "opencode": OutputSpec("skills/halo-deploy-xbdm/SKILL.md"),
    },
    "halo-build-xemu": {
        "claude": OutputSpec("skills/halo-build-xemu/SKILL.md"),
        "opencode": OutputSpec("skills/halo-build-xemu/SKILL.md"),
    },
    "halo-page-fault": {
        "claude": OutputSpec("skills/halo-page-fault/SKILL.md"),
        "opencode": OutputSpec("skills/halo-page-fault/SKILL.md"),
    },
    "halo-re-lift": {
        "claude": OutputSpec("skills/halo-re-lift/SKILL.md"),
        "opencode": OutputSpec("skills/halo-re-lift/SKILL.md"),
    },
    "halo-verify-debug": {
        "claude": OutputSpec("skills/halo-verify-debug/SKILL.md"),
        "opencode": OutputSpec("skills/halo-verify-debug/SKILL.md"),
    },
    "halo-xbdm": {
        "claude": OutputSpec("skills/halo-xbdm/SKILL.md"),
        "opencode": OutputSpec("skills/halo-xbdm/SKILL.md"),
    },
    "halo-xbox-re": {
        "claude": OutputSpec("skills/halo-xbox-re/SKILL.md"),
        "opencode": OutputSpec("skills/halo-xbox-re/SKILL.md"),
    },
}

TARGET_ROOTS = {
    "claude": CLAUDE_DIR,
    "opencode": OPENCODE_DIR,
}


def normalize_text(text: str) -> str:
    return text.replace("\r\n", "\n")


def read_text(path: Path) -> str:
    return normalize_text(path.read_text(encoding="utf-8"))


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def bootstrap_file(shared_path: Path, outputs: dict[str, OutputSpec], prefer: str, force: bool) -> None:
    if shared_path.exists() and not force:
        return

    ordered_targets = [prefer] + [name for name in TARGET_ROOTS if name != prefer]
    source_path: Path | None = None
    for target_name in ordered_targets:
        candidate = TARGET_ROOTS[target_name] / outputs[target_name].relative_path
        if candidate.exists():
            source_path = candidate
            break

    if source_path is None:
        raise FileNotFoundError(f"no source file found for {shared_path}")

    write_text(shared_path, read_text(source_path))


def sync_file(shared_path: Path, outputs: dict[str, OutputSpec], check: bool) -> int:
    shared_text = read_text(shared_path)
    differences = 0

    for target_name, spec in outputs.items():
        output_path = TARGET_ROOTS[target_name] / spec.relative_path
        if check:
            if not output_path.exists() or read_text(output_path) != shared_text:
                print(f"DRIFT: {output_path.relative_to(ROOT_DIR).as_posix()}")
                differences += 1
        else:
            write_text(output_path, shared_text)

    return differences


def bootstrap(prefer: str, force: bool) -> None:
    for name, outputs in COMMAND_OUTPUTS.items():
        bootstrap_file(SHARED_DIR / "commands" / name, outputs, prefer, force)
    for name, outputs in SKILL_OUTPUTS.items():
        bootstrap_file(SHARED_DIR / "skills" / name / "SKILL.md", outputs, prefer, force)


def sync(check: bool) -> int:
    differences = 0
    for name, outputs in COMMAND_OUTPUTS.items():
        differences += sync_file(SHARED_DIR / "commands" / name, outputs, check)
    for name, outputs in SKILL_OUTPUTS.items():
        differences += sync_file(SHARED_DIR / "skills" / name / "SKILL.md", outputs, check)
    return differences


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Sync shared agent content into separate .claude and .opencode trees.",
    )
    parser.add_argument(
        "--bootstrap",
        action="store_true",
        help="seed agent-content/ from current .claude/.opencode outputs",
    )
    parser.add_argument(
        "--prefer",
        choices=sorted(TARGET_ROOTS),
        default="opencode",
        help="preferred source tree for bootstrap (default: opencode)",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="overwrite existing agent-content/ files during bootstrap",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="check whether .claude/.opencode outputs match agent-content/",
    )
    return parser


def main() -> int:
    args = build_parser().parse_args()

    if args.bootstrap:
        bootstrap(args.prefer, args.force)

    differences = sync(check=args.check)
    if args.check and differences:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
