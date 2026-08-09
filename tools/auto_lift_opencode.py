#!/usr/bin/env python3
"""Guarded, one-target-at-a-time OpenCode lift driver.

The driver owns queue selection and repository safety checks.  OpenCode owns
the lift itself, including its normal /lift workflow and commit procedure.
It never attempts destructive cleanup: a dirty protected path is a stop.
"""

import argparse
import json
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable, Iterable, Optional, Sequence


ROOT = Path(__file__).resolve().parents[1]
AUTOMATION_LANES = frozenset(("auto-lift", "cache-context"))
PROTECTED_PREFIXES = ("src/", "tools/", ".opencode/", ".claude/", ".agents/", "recovery/")
PROTECTED_FILES = frozenset(("kb.json",))


class DriverError(RuntimeError):
    """A command or repository guard prevented safe automation."""


@dataclass(frozen=True)
class Candidate:
    """One selector row eligible for an OpenCode lift attempt."""

    name: str
    object_name: str
    addr: str
    lane: str
    row: dict[str, Any]


@dataclass(frozen=True)
class StatusEntry:
    """One porcelain-v1 status entry, including both sides of a rename."""

    code: str
    paths: tuple[str, ...]


@dataclass(frozen=True)
class RepositoryState:
    """Branch, commit, and tracked changes relevant to the driver guard."""

    branch: str
    head: str
    protected_changes: tuple[StatusEntry, ...]


@dataclass(frozen=True)
class AttemptResult:
    """Post-agent result; retryable failures left protected paths clean."""

    committed: bool
    retryable: bool
    reason: str


@dataclass(frozen=True)
class RunSummary:
    """Compact result returned by the driver and rendered for operators."""

    attempted: int
    committed: int
    failed: int
    skipped: int
    reason: str


def parse_objects(value: str) -> tuple[str, ...]:
    """Parse a comma-separated object allowlist without changing its order."""

    return tuple(item.strip() for item in value.split(",") if item.strip())


def build_parser() -> argparse.ArgumentParser:
    """Build the small command-line interface without performing I/O."""

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--goal", "--batch", dest="goal", type=int, default=1,
                        help="Number of verified commits to create (default: 1; --batch is an alias)")
    parser.add_argument("--stop-on-fail", type=int, default=3,
                        help="Stop after this many clean consecutive failures (default: 3)")
    parser.add_argument("--objects", default="",
                        help="Comma-separated selector object allowlist")
    parser.add_argument("--criteria", default="",
                        help="Extra target guidance included in the OpenCode prompt")
    parser.add_argument("--dry-run", action="store_true",
                        help="Print eligible ordered candidates without invoking OpenCode")
    parser.add_argument("--allow-main", action="store_true",
                        help="Permit execution on branch main")
    parser.add_argument("--model", default="",
                        help="Optional OpenCode model passed with -m")
    parser.add_argument("--agent", default="build",
                        help="OpenCode agent name (default: build)")
    return parser


def parse_args(argv: Optional[Sequence[str]] = None) -> argparse.Namespace:
    """Parse and validate command-line arguments."""

    parser = build_parser()
    args = parser.parse_args(argv)
    if args.goal < 1:
        parser.error("--goal must be at least 1")
    if args.stop_on_fail < 1:
        parser.error("--stop-on-fail must be at least 1")
    args.objects = parse_objects(args.objects)
    args.criteria = args.criteria.strip()
    args.model = args.model.strip()
    args.agent = args.agent.strip()
    if not args.agent:
        parser.error("--agent must not be empty")
    return args


def selector_command() -> list[str]:
    """Return the fixed selector invocation required by this driver."""

    return ["python3", "tools/llm_auto_lift.py", "select", "--limit", "60", "--json"]


def is_protected_path(path: str) -> bool:
    """Return whether a repository-relative path is part of tracked lift state."""

    normalized = path[2:] if path.startswith("./") else path
    return normalized in PROTECTED_FILES or normalized.startswith(PROTECTED_PREFIXES)


def parse_porcelain_status(raw_status: str) -> tuple[StatusEntry, ...]:
    """Parse porcelain-v1 status, accepting both NUL and newline test fixtures."""

    records = raw_status.split("\0") if "\0" in raw_status else raw_status.splitlines()
    entries = []
    index = 0
    while index < len(records):
        record = records[index]
        index += 1
        if not record:
            continue
        if len(record) < 4 or record[2] != " ":
            raise DriverError("unrecognized git status record")

        code = record[:2]
        path = record[3:]
        paths = [path]
        if "R" in code or "C" in code:
            if "\0" in raw_status:
                if index >= len(records) or not records[index]:
                    raise DriverError("rename status record lacks source path")
                paths.append(records[index])
                index += 1
            elif " -> " in path:
                old_path, new_path = path.split(" -> ", 1)
                paths = [old_path, new_path]
        entries.append(StatusEntry(code=code, paths=tuple(paths)))
    return tuple(entries)


def protected_tracked_changes(raw_status: str) -> tuple[StatusEntry, ...]:
    """Return tracked changes in protected paths; ignore untracked entries."""

    return tuple(
        entry for entry in parse_porcelain_status(raw_status)
        if entry.code not in ("??", "!!")
        and any(is_protected_path(path) for path in entry.paths)
    )


def is_confirmed_cap_status(status: Any) -> bool:
    """Recognize terminal cap spelling from the parked ledger."""

    normalized = str(status or "").strip().lower().replace("-", "_").replace(" ", "_")
    return normalized in ("capped_confirmed", "confirmed_cap")


def filter_candidates(rows: Iterable[Any], objects: Iterable[str] = ()) -> list[Candidate]:
    """Keep selector ordering while applying only the driver eligibility rules."""

    allowed_objects = frozenset(objects)
    candidates = []
    for row in rows:
        if not isinstance(row, dict):
            continue
        target = row.get("target")
        if not isinstance(target, dict):
            continue
        name = target.get("name")
        object_name = target.get("object_name", row.get("object_name", ""))
        lane = row.get("lane")
        if not isinstance(name, str) or not name:
            continue
        if not isinstance(object_name, str) or not object_name:
            continue
        if lane not in AUTOMATION_LANES:
            continue
        if allowed_objects and object_name not in allowed_objects:
            continue
        if row.get("prior_fail"):
            continue
        if is_confirmed_cap_status(row.get("parked_status")):
            continue
        candidates.append(Candidate(
            name=name,
            object_name=object_name,
            addr=str(target.get("addr", "")),
            lane=lane,
            row=row,
        ))
    return candidates


def build_lift_prompt(candidate: Candidate, criteria: str = "") -> str:
    """Build one constrained prompt; it is passed as one OpenCode argument."""

    criteria_block = ""
    if criteria:
        criteria_block = "\nAdditional selection guidance: " + criteria + "\n"
    return f"""Lift exactly one target: {candidate.name} at {candidate.addr} from {candidate.object_name}.

Use existing /lift rules, AGENTS.md, and applicable lift skills as mandatory workflow. Implement only this target as faithful C89; preserve ABI, binary behavior, and existing work outside this target.{criteria_block}
Before editing, classify this target with `tools/analysis/classify_liftability.py` using Ghidra xrefs: fetch xrefs for {candidate.addr}, write a one-entry candidates JSON and address-to-xrefs JSON, then run the classifier. If it is a fragment or confirmed cap, make no edits and exit cleanly.
Run exactly:
rtk python3 tools/lift_pipeline.py --target {candidate.name} --no-metadata-update --verify-policy goal90

Commit only when that pipeline succeeds and its verified match is at least 90%. Stage only files required by this target, then use the repository mktemp commit procedure:
MSG=$(mktemp /tmp/halo-commit-msg.XXXXXX)
rtk python3 tools/audit/generate_lift_commit.py --batch-name "{candidate.name}" > "$MSG"
rtk git commit -F "$MSG" && rm -f "$MSG"

If implementation, build, or verification fails, do not commit. Leave tracked protected paths clean before returning. Never invoke git checkout, git reset, git restore, or equivalent destructive cleanup. Do not modify, stage, discard, or otherwise touch unrelated work, including pre-existing artifacts and untracked ci/ files. Work on this target only; do not begin another target."""


def opencode_command(candidate: Candidate, agent: str, model: str, criteria: str) -> list[str]:
    """Compose the native OpenCode run command for exactly one target."""

    command = ["opencode", "run", "--agent", agent]
    if model:
        command.extend(("-m", model))
    command.append(build_lift_prompt(candidate, criteria))
    return command


def _text(value: Any) -> str:
    if isinstance(value, bytes):
        return value.decode("utf-8", errors="replace")
    return str(value or "")


def run_command(command: Sequence[str], root: Path,
                runner: Optional[Callable[..., Any]] = None,
                capture_output: bool = True) -> Any:
    """Run one command without a shell; convert unavailable executables to failure."""

    if runner is None:
        runner = subprocess.run
    try:
        return runner(
            list(command),
            cwd=str(root),
            text=True,
            capture_output=capture_output,
            check=False,
        )
    except OSError as exc:
        return subprocess.CompletedProcess(list(command), 127, "", str(exc))


def checked_output(command: Sequence[str], root: Path,
                   runner: Optional[Callable[..., Any]] = None) -> str:
    """Run a read-only command and raise a compact driver error on failure."""

    result = run_command(command, root, runner=runner)
    if result.returncode:
        detail = _text(getattr(result, "stderr", "")).strip()
        if not detail:
            detail = _text(getattr(result, "stdout", "")).strip()
        raise DriverError("command failed: " + " ".join(command) + (": " + detail if detail else ""))
    return _text(getattr(result, "stdout", ""))


def inspect_repository(root: Path,
                       runner: Optional[Callable[..., Any]] = None) -> RepositoryState:
    """Read branch, HEAD, and tracked protected-path state."""

    branch = checked_output(("git", "branch", "--show-current"), root, runner).strip()
    head = checked_output(("git", "rev-parse", "HEAD"), root, runner).strip()
    status = checked_output(
        ("git", "status", "--porcelain=v1", "-z", "--untracked-files=all"),
        root,
        runner,
    )
    return RepositoryState(
        branch=branch,
        head=head,
        protected_changes=protected_tracked_changes(status),
    )


def format_changes(changes: Iterable[StatusEntry]) -> str:
    """Render protected status entries compactly for a refusal message."""

    return ", ".join(entry.code + " " + "/".join(entry.paths) for entry in changes)


def validate_start_state(state: RepositoryState, allow_main: bool) -> None:
    """Fail before selection when branch or protected tracked state is unsafe."""

    if state.branch == "main" and not allow_main:
        raise DriverError("refusing branch main; pass --allow-main to override")
    if state.protected_changes:
        raise DriverError("protected tracked changes: " + format_changes(state.protected_changes))


def load_candidates(root: Path, objects: Iterable[str],
                    runner: Optional[Callable[..., Any]] = None) -> tuple[list[Candidate], int]:
    """Run the fixed selector and return eligible candidates in selector order."""

    payload_text = checked_output(selector_command(), root, runner)
    try:
        rows = json.loads(payload_text)
    except json.JSONDecodeError as exc:
        raise DriverError("selector returned invalid JSON") from exc
    if not isinstance(rows, list):
        raise DriverError("selector JSON must be a list")
    return filter_candidates(rows, objects), len(rows)


def assess_attempt(before: RepositoryState, after: RepositoryState,
                   returncode: int) -> AttemptResult:
    """Apply post-agent invariants without ever modifying repository state."""

    if after.branch != before.branch:
        return AttemptResult(False, False, "agent changed branch")
    if after.protected_changes:
        return AttemptResult(
            False,
            False,
            "protected tracked changes remain: " + format_changes(after.protected_changes),
        )
    if returncode:
        if after.head != before.head:
            return AttemptResult(False, False, "agent failed after changing HEAD")
        return AttemptResult(False, True, "agent exited " + str(returncode) + " with clean state")
    if after.head == before.head:
        return AttemptResult(False, False, "agent exited without creating a commit")
    return AttemptResult(True, False, "committed")


def render_summary(summary: RunSummary) -> str:
    """Return the intentionally compact terminal summary."""

    return (
        "summary attempted={0.attempted} committed={0.committed} "
        "failed={0.failed} skipped={0.skipped} reason={0.reason}"
    ).format(summary)


def _emit_candidates(candidates: Iterable[Candidate], emit: Callable[[str], None]) -> None:
    for index, candidate in enumerate(candidates, 1):
        emit("{0:02d} {1.name} {1.object_name} {1.lane}".format(index, candidate))


def run_driver(args: argparse.Namespace, root: Path = ROOT,
               runner: Optional[Callable[..., Any]] = None,
               emit: Callable[[str], None] = print) -> RunSummary:
    """Run guarded selection and one OpenCode agent per target attempt."""

    root = Path(root)
    try:
        initial_state = inspect_repository(root, runner)
        validate_start_state(initial_state, args.allow_main)
    except DriverError as exc:
        summary = RunSummary(0, 0, 0, 0, "guard:" + str(exc))
        emit(render_summary(summary))
        return summary

    if args.dry_run:
        try:
            candidates, total_rows = load_candidates(root, args.objects, runner)
        except DriverError as exc:
            summary = RunSummary(0, 0, 0, 0, "selector:" + str(exc))
            emit(render_summary(summary))
            return summary
        emit("dry-run candidates=" + str(len(candidates)))
        _emit_candidates(candidates, emit)
        summary = RunSummary(0, 0, 0, total_rows - len(candidates), "dry-run")
        emit(render_summary(summary))
        return summary

    attempted_names = set()
    attempted = 0
    committed = 0
    failed = 0
    skipped = 0
    consecutive_failures = 0
    reason = "goal-reached"

    while committed < args.goal:
        try:
            candidates, total_rows = load_candidates(root, args.objects, runner)
        except DriverError as exc:
            reason = "selector:" + str(exc)
            break
        skipped = max(skipped, total_rows - len(candidates))
        candidate = next((item for item in candidates if item.name not in attempted_names), None)
        if candidate is None:
            reason = "no-eligible-candidate"
            break

        try:
            before = inspect_repository(root, runner)
            validate_start_state(before, args.allow_main)
        except DriverError as exc:
            reason = "guard:" + str(exc)
            break

        command = opencode_command(candidate, args.agent, args.model, args.criteria)
        emit("run " + candidate.name)
        result = run_command(command, root, runner=runner, capture_output=False)
        try:
            after = inspect_repository(root, runner)
        except DriverError as exc:
            attempted += 1
            failed += 1
            reason = "post-agent guard:" + str(exc)
            break

        attempted += 1
        attempted_names.add(candidate.name)
        assessment = assess_attempt(before, after, result.returncode)
        if assessment.committed:
            committed += 1
            consecutive_failures = 0
            emit("ok " + candidate.name)
            continue

        failed += 1
        emit("fail " + candidate.name + " " + assessment.reason)
        if not assessment.retryable:
            reason = "safety-stop"
            break
        consecutive_failures += 1
        if consecutive_failures >= args.stop_on_fail:
            reason = "failure-limit"
            break

    summary = RunSummary(attempted, committed, failed, skipped, reason)
    emit(render_summary(summary))
    return summary


def main(argv: Optional[Sequence[str]] = None) -> int:
    """CLI entry point."""

    summary = run_driver(parse_args(argv))
    if summary.reason in ("goal-reached", "dry-run", "no-eligible-candidate"):
        return 0
    return 1


if __name__ == "__main__":
    sys.exit(main())
