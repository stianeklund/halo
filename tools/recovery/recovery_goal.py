#!/usr/bin/env python3
"""Goal-mode queue and ledger for readability-recovery campaigns.

`/recover-goal` loops the recovery frontier and runs the source-recovery ladder
over one kb.json object at a time. This tool owns the *deterministic* part of
that loop: which object is next, and what happened to the ones already taken.
It never edits source, never commits, and never runs a gate.

  next    highest-scored eligible object not already taken (exit 3 = exhausted)
  start   mark an object in progress
  finish  mark an object recovered (requires commit shas, or --no-commits)
  park    mark an object abandoned with a reason (fail-closed record)
  status  ledger summary

Ledger: recovery/goal_ledger.json (committed).
  {"schema": 1,
   "objects": {"<object.obj>": {"status": "done"|"parked"|"in_progress",
                                "reason": <str|null>,
                                "started": "YYYY-MM-DD",
                                "finished": "YYYY-MM-DD"|null,
                                "commits": ["<sha>", ...],
                                "categories_done": ["<ladder-id>", ...]}}}
"""

from __future__ import annotations

import argparse
import datetime
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

TOOL_VERSION = "recovery-goal/1"
SCHEMA = 1
LEDGER_PATH = ROOT / "recovery" / "goal_ledger.json"
FRONTIER = ROOT / "tools" / "recovery" / "recovery_frontier.py"

# Statuses that take an object out of the queue. A parked object stays out
# until a human clears it (or `start --force` re-attempts it deliberately).
TAKEN = ("done", "parked", "in_progress")
STATUSES = TAKEN
NOISY_PARTS = {"build", "build_debug", "node_modules", ".git", "halo-patched",
               "__pycache__", "dist"}

OBJECT_RE = re.compile(r"^[A-Za-z0-9_.+-]+\.obj$")
SHA_RE = re.compile(r"^[0-9a-fA-F]{7,40}$")
# The source-recovery ladder (tools/recovery/source_recovery.py LADDER). Kept as
# a literal so this tool stays importable without that module; validated by the
# self-test against the real ladder when it is available.
LADDER = (
    "comments",
    "local-renames",
    "symbol-names",
    "const-enum",
    "struct-define",
    "offset-to-field",
    "expr-simplify",
    "control-flow",
)

EXIT_OK = 0
EXIT_ERROR = 2
EXIT_EXHAUSTED = 3


class GoalError(Exception):
    """Expected, fail-closed driver error (never a traceback)."""


# --- small helpers ---------------------------------------------------------

def _today() -> str:
    """ISO date stamp; patched by the tests for determinism."""
    return datetime.date.today().isoformat()


def _repo_path(value: str, suffix: str) -> Path:
    path = Path(value).expanduser()
    if not path.is_absolute():
        path = ROOT / path
    resolved = path.resolve(strict=False)
    if resolved.suffix != suffix:
        raise GoalError("path must be a %s file: %s" % (suffix, value))
    try:
        parts = resolved.relative_to(ROOT).parts
    except ValueError:
        parts = ()
    if any(part in NOISY_PARTS for part in parts):
        raise GoalError("generated/noisy path is not allowed: %s" % value)
    return resolved


def _write_atomic(path: Path, value: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    payload = json.dumps(value, indent=2, sort_keys=True) + "\n"
    fd, temporary = tempfile.mkstemp(prefix=".%s." % path.name, dir=path.parent)
    try:
        with os.fdopen(fd, "w", encoding="ascii") as stream:
            stream.write(payload)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def _object_name(value: str) -> str:
    name = value.strip()
    if not OBJECT_RE.match(name):
        raise GoalError("not a kb.json object name (expected '<name>.obj'): %s" % value)
    return name


# --- ledger ----------------------------------------------------------------

def _empty_ledger() -> dict[str, Any]:
    return {"schema": SCHEMA, "objects": {}}


def _validate_ledger(ledger: Any) -> dict[str, Any]:
    if not isinstance(ledger, dict):
        raise GoalError("ledger is not a JSON object")
    if ledger.get("schema") != SCHEMA:
        raise GoalError("unsupported ledger schema: %r (expected %d)"
                        % (ledger.get("schema"), SCHEMA))
    objects = ledger.get("objects")
    if not isinstance(objects, dict):
        raise GoalError("ledger 'objects' is not a JSON object")
    for name, entry in objects.items():
        if not isinstance(entry, dict):
            raise GoalError("ledger entry for %s is not a JSON object" % name)
        status = entry.get("status")
        if status not in STATUSES:
            raise GoalError("ledger entry for %s has invalid status %r" % (name, status))
        for field in ("commits", "categories_done"):
            if not isinstance(entry.get(field, []), list):
                raise GoalError("ledger entry for %s has invalid %s" % (name, field))
    return ledger


def _load_ledger(path: Path) -> dict[str, Any]:
    """Load the ledger; a missing file is an empty ledger, not an error."""
    if not path.exists():
        return _empty_ledger()
    try:
        with path.open(encoding="utf-8") as stream:
            ledger = json.load(stream)
    except (OSError, json.JSONDecodeError) as exc:
        raise GoalError("unable to read ledger %s: %s" % (path, exc))
    return _validate_ledger(ledger)


def _entry(ledger: dict[str, Any], name: str) -> dict[str, Any] | None:
    return ledger["objects"].get(name)


def _status_of(ledger: dict[str, Any], name: str) -> str:
    entry = _entry(ledger, name)
    return entry["status"] if entry else "none"


def _new_entry() -> dict[str, Any]:
    return {"status": "in_progress", "reason": None, "started": _today(),
            "finished": None, "commits": [], "categories_done": []}


def _extend_unique(existing: list[str], added: list[str]) -> list[str]:
    out = list(existing)
    for value in added:
        if value not in out:
            out.append(value)
    return out


# --- transitions -----------------------------------------------------------

def transition_start(ledger: dict[str, Any], name: str,
                     force: bool = False) -> dict[str, Any]:
    """none/parked -> in_progress. `done`/`in_progress` need --force."""
    status = _status_of(ledger, name)
    if status in ("done", "in_progress") and not force:
        raise GoalError("cannot start %s: already %s (use --force to re-attempt)"
                        % (name, status))
    entry = _entry(ledger, name)
    if entry is None:
        entry = _new_entry()
        ledger["objects"][name] = entry
    else:
        # Re-attempt: keep the audit trail (commits, categories) and re-open.
        entry["status"] = "in_progress"
        entry["reason"] = None
        entry["finished"] = None
        entry.setdefault("commits", [])
        entry.setdefault("categories_done", [])
        entry["started"] = _today()
    return entry


def transition_finish(ledger: dict[str, Any], name: str, commits: list[str],
                      categories: list[str] | None = None,
                      no_commits: bool = False) -> dict[str, Any]:
    """in_progress -> done. A `done` with no commits must be explicit."""
    status = _status_of(ledger, name)
    if status != "in_progress":
        raise GoalError("cannot finish %s: not in progress (status: %s)" % (name, status))
    if not commits and not no_commits:
        raise GoalError("cannot finish %s: no --commit <sha> given "
                        "(pass --no-commits to record a recovery that landed nothing)"
                        % name)
    for sha in commits:
        if not SHA_RE.match(sha):
            raise GoalError("not a commit sha: %s" % sha)
    entry = ledger["objects"][name]
    entry["status"] = "done"
    entry["reason"] = None
    entry["finished"] = _today()
    entry["commits"] = _extend_unique(entry.get("commits", []), commits)
    entry["categories_done"] = _extend_unique(entry.get("categories_done", []),
                                              categories or [])
    return entry


def transition_park(ledger: dict[str, Any], name: str, reason: str,
                    force: bool = False) -> dict[str, Any]:
    """any -> parked. Overwrites in_progress; re-parking updates the reason.

    Parking a `done` object needs --force: it would erase a landed result.
    """
    reason = (reason or "").strip()
    if not reason:
        raise GoalError("cannot park %s: --reason is required and must be non-empty" % name)
    status = _status_of(ledger, name)
    if status == "done" and not force:
        raise GoalError("cannot park %s: already done (use --force to overrule)" % name)
    entry = _entry(ledger, name)
    if entry is None:
        entry = _new_entry()
        ledger["objects"][name] = entry
    entry["status"] = "parked"
    entry["reason"] = reason
    entry["finished"] = _today()
    entry.setdefault("commits", [])
    entry.setdefault("categories_done", [])
    return entry


# --- frontier --------------------------------------------------------------

def _frontier_payload(min_funcs: int) -> dict[str, Any]:
    """Run recovery_frontier.py --json and return the parsed payload."""
    if not FRONTIER.exists():
        raise GoalError("recovery frontier tool not found: %s "
                        "(pass --frontier-json <path> to reuse a ranking)" % FRONTIER)
    handle, temporary = tempfile.mkstemp(prefix="recovery_frontier.", suffix=".json")
    os.close(handle)
    try:
        command = [sys.executable, str(FRONTIER), "--json", temporary]
        if min_funcs > 0:
            command += ["--min-funcs", str(min_funcs)]
        try:
            result = subprocess.run(command, cwd=str(ROOT), capture_output=True,
                                    text=True)
        except OSError as exc:
            raise GoalError("unable to run the recovery frontier: %s" % exc)
        if result.returncode != 0:
            tail = (result.stderr or result.stdout or "").strip().splitlines()[-5:]
            raise GoalError("recovery frontier failed (exit %d): %s"
                            % (result.returncode, " | ".join(tail) or "no output"))
        return _read_frontier_json(Path(temporary))
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def _read_frontier_json(path: Path) -> dict[str, Any]:
    try:
        with path.open(encoding="utf-8") as stream:
            payload = json.load(stream)
    except (OSError, json.JSONDecodeError) as exc:
        raise GoalError("unable to read frontier JSON %s: %s" % (path, exc))
    if not isinstance(payload, dict) or not isinstance(payload.get("eligible"), list):
        raise GoalError("frontier JSON %s has no 'eligible' list "
                        "(regenerate with recovery_frontier.py --json)" % path)
    return payload


def frontier_rows(frontier_json: str | None = None, min_funcs: int = 0) -> list[dict[str, Any]]:
    """Eligible frontier rows, filtered by --min-funcs, best score first.

    `--min-funcs` is re-applied locally so a reused ranking honours it too (the
    frontier's own flag is a display filter that also prunes its JSON output).
    """
    if frontier_json:
        payload = _read_frontier_json(_repo_path(frontier_json, ".json"))
    else:
        payload = _frontier_payload(min_funcs)
    rows = [row for row in payload["eligible"] if isinstance(row, dict)]
    if min_funcs > 0:
        rows = [row for row in rows if int(row.get("funcs", 0)) >= min_funcs]
    rows.sort(key=lambda row: (-float(row.get("score", 0.0)), str(row.get("object", ""))))
    return rows


def select_next(rows: list[dict[str, Any]], ledger: dict[str, Any],
                min_score: float = 0.0) -> tuple[dict[str, Any] | None, dict[str, int]]:
    """First row not already taken by the ledger and at/above `min_score`."""
    stats = {"eligible": len(rows), "taken": 0, "below_min_score": 0}
    chosen = None
    for row in rows:
        name = str(row.get("object", ""))
        if _status_of(ledger, name) in TAKEN:
            stats["taken"] += 1
            continue
        if float(row.get("score", 0.0)) < min_score:
            stats["below_min_score"] += 1
            continue
        if chosen is None:
            chosen = row
    return chosen, stats


# --- rendering -------------------------------------------------------------

def _format_row(row: dict[str, Any]) -> str:
    files = row.get("files") or []
    lines = [
        "next recovery target: %s" % row.get("object", "?"),
        "  frontier rank   %s" % row.get("rank", "?"),
        "  score           %.1f" % float(row.get("score", 0.0)),
        "  functions       %s (all ported, delinked ref present)" % row.get("funcs", "?"),
        "  debt            %s items (%.1f / func)"
        % (row.get("debt_total", "?"), float(row.get("debt_per_func", 0.0))),
        "  vc71 >= 99%%     %.0f%% of measured funcs (%s of %s)"
        % (float(row.get("high_match_pct", 0.0)), row.get("high_match_funcs", "?"),
           row.get("measured_funcs", "?")),
        "  source files    %s" % (", ".join(files) if files else "-"),
    ]
    debt = row.get("debt")
    if isinstance(debt, dict) and debt:
        worst = sorted(debt.items(), key=lambda kv: (-int(kv[1]), kv[0]))[:4]
        lines.append("  top debt        %s"
                     % ", ".join("%s=%s" % (key, value) for key, value in worst))
    if row.get("multi_object_files"):
        lines.append("  shared files    %s (object majority-owns a shared file)"
                     % ", ".join(row["multi_object_files"]))
    return "\n".join(lines)


def _format_status(ledger: dict[str, Any]) -> str:
    objects = ledger["objects"]
    header = ("%-32s %-12s %-10s %-10s %5s  %s"
              % ("object", "status", "started", "finished", "cmts", "categories / reason"))
    lines = ["Recovery Goal Ledger", "-" * len("Recovery Goal Ledger"), header,
             "-" * len(header)]
    counts = {status: 0 for status in STATUSES}
    for name in sorted(objects):
        entry = objects[name]
        counts[entry["status"]] = counts.get(entry["status"], 0) + 1
        detail = ", ".join(entry.get("categories_done") or []) or "-"
        if entry.get("reason"):
            detail = "%s [%s]" % (detail, entry["reason"])
        lines.append("%-32s %-12s %-10s %-10s %5d  %s"
                     % (name, entry["status"], entry.get("started") or "-",
                        entry.get("finished") or "-",
                        len(entry.get("commits") or []), detail))
    if not objects:
        lines.append("(empty — no object taken yet)")
    lines.append("")
    lines.append("%d object(s): %s" % (len(objects),
                 ", ".join("%s %d" % (status, counts.get(status, 0))
                           for status in STATUSES)))
    return "\n".join(lines)


# --- commands --------------------------------------------------------------

def _cmd_next(args: argparse.Namespace, path: Path, ledger: dict[str, Any]) -> int:
    rows = frontier_rows(args.frontier_json, args.min_funcs)
    row, stats = select_next(rows, ledger, args.min_score)
    if row is None:
        message = ("recovery queue exhausted: %d eligible object(s), %d already "
                   "taken, %d below --min-score %g"
                   % (stats["eligible"], stats["taken"], stats["below_min_score"],
                      args.min_score))
        if args.json:
            print(json.dumps({"object": None, "row": None, "stats": stats,
                              "message": message}, indent=2, sort_keys=True))
        else:
            print(message)
        return EXIT_EXHAUSTED
    if args.json:
        print(json.dumps({"object": row["object"], "row": row, "stats": stats},
                         indent=2, sort_keys=True))
        return EXIT_OK
    print(_format_row(row))
    print()
    print("start it with: rtk python3 tools/recovery/recovery_goal.py start %s"
          % row["object"])
    return EXIT_OK


def _cmd_start(args: argparse.Namespace, path: Path, ledger: dict[str, Any]) -> int:
    name = _object_name(args.object)
    entry = transition_start(ledger, name, args.force)
    _write_atomic(path, ledger)
    print("%s: in_progress (started %s)" % (name, entry["started"]))
    return EXIT_OK


def _cmd_finish(args: argparse.Namespace, path: Path, ledger: dict[str, Any]) -> int:
    name = _object_name(args.object)
    entry = transition_finish(ledger, name, args.commit, args.category, args.no_commits)
    _write_atomic(path, ledger)
    print("%s: done (%d commit(s), categories: %s)"
          % (name, len(entry["commits"]),
             ", ".join(entry["categories_done"]) or "-"))
    return EXIT_OK


def _cmd_park(args: argparse.Namespace, path: Path, ledger: dict[str, Any]) -> int:
    name = _object_name(args.object)
    entry = transition_park(ledger, name, args.reason, args.force)
    _write_atomic(path, ledger)
    print("%s: parked (%s)" % (name, entry["reason"]))
    return EXIT_OK


def _cmd_status(args: argparse.Namespace, path: Path, ledger: dict[str, Any]) -> int:
    if args.json:
        print(json.dumps(ledger, indent=2, sort_keys=True))
    else:
        print(_format_status(ledger))
    return EXIT_OK


# --- self-test -------------------------------------------------------------

def _self_test() -> int:
    global _today
    checks: list[tuple[bool, str]] = []
    original_today = _today

    def _check(name: str, function) -> None:
        try:
            checks.append((bool(function()), name))
        except Exception as exc:  # noqa: BLE001 - self-test reports, never raises
            checks.append((False, "%s (raised %s: %s)" % (name, type(exc).__name__, exc)))

    def _raises(function) -> bool:
        try:
            function()
        except GoalError:
            return True
        return False

    _today = lambda: "2026-08-01"  # noqa: E731 - deterministic stamps
    try:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            path = root / "goal_ledger.json"

            _check("missing ledger loads as empty schema",
                   lambda: _load_ledger(path) == _empty_ledger())

            ledger = _empty_ledger()

            # --- valid transition matrix ---
            _check("start on unknown object opens in_progress",
                   lambda: transition_start(ledger, "a.obj")["status"] == "in_progress"
                   and ledger["objects"]["a.obj"]["started"] == "2026-08-01")
            _check("finish records commits, categories and finished date",
                   lambda: transition_finish(ledger, "a.obj", ["abc1234"], ["comments"])
                   == {"status": "done", "reason": None, "started": "2026-08-01",
                       "finished": "2026-08-01", "commits": ["abc1234"],
                       "categories_done": ["comments"]})
            _check("park on unknown object records reason",
                   lambda: transition_park(ledger, "b.obj", "no COFF baseline")["reason"]
                   == "no COFF baseline")
            _check("park overwrites in_progress",
                   lambda: (transition_start(ledger, "c.obj"),
                            transition_park(ledger, "c.obj", "build broken"))
                   and ledger["objects"]["c.obj"]["status"] == "parked")
            _check("re-park updates the reason",
                   lambda: transition_park(ledger, "c.obj", "manifest errors")["reason"]
                   == "manifest errors")
            _check("start re-attempts a parked object and keeps history",
                   lambda: transition_start(ledger, "b.obj")["status"] == "in_progress"
                   and ledger["objects"]["b.obj"]["reason"] is None)
            _check("finish --no-commits is allowed explicitly",
                   lambda: transition_finish(ledger, "b.obj", [], None, True)["commits"] == [])
            _check("finish is additive and de-duplicates commits",
                   lambda: (transition_start(ledger, "d.obj", True),
                            transition_finish(ledger, "d.obj", ["aaaaaaa", "aaaaaaa", "bbbbbbb"]))
                   and ledger["objects"]["d.obj"]["commits"] == ["aaaaaaa", "bbbbbbb"])

            # --- invalid transitions ---
            _check("cannot finish an object that was never started",
                   lambda: _raises(lambda: transition_finish(ledger, "zz.obj", ["abc1234"])))
            _check("cannot finish a done object",
                   lambda: _raises(lambda: transition_finish(ledger, "a.obj", ["abc1234"])))
            _check("cannot finish a parked object",
                   lambda: _raises(lambda: transition_finish(ledger, "c.obj", ["abc1234"])))
            _check("cannot finish without commits or --no-commits",
                   lambda: (transition_start(ledger, "e.obj"),
                            _raises(lambda: transition_finish(ledger, "e.obj", [])))[1])
            _check("finish rejects a non-sha commit",
                   lambda: _raises(lambda: transition_finish(ledger, "e.obj", ["hud_weapon.obj"])))
            _check("cannot start a done object without --force",
                   lambda: _raises(lambda: transition_start(ledger, "a.obj")))
            _check("cannot start an in_progress object without --force",
                   lambda: _raises(lambda: transition_start(ledger, "e.obj")))
            _check("cannot park a done object without --force",
                   lambda: _raises(lambda: transition_park(ledger, "a.obj", "oops")))
            _check("park requires a non-empty reason",
                   lambda: _raises(lambda: transition_park(ledger, "e.obj", "   ")))
            _check("object names must look like <name>.obj",
                   lambda: _raises(lambda: _object_name("src/halo/hud.c")))

            # --- persistence round-trip ---
            _write_atomic(path, ledger)
            _check("ledger round-trips through the atomic writer",
                   lambda: _load_ledger(path) == ledger)
            _check("corrupt ledger is a clean error, not a traceback",
                   lambda: (path.write_text("{not json", encoding="utf-8"),
                            _raises(lambda: _load_ledger(path)))[1])
            _check("wrong schema is rejected",
                   lambda: _raises(lambda: _validate_ledger({"schema": 99, "objects": {}})))
            _check("invalid status is rejected",
                   lambda: _raises(lambda: _validate_ledger(
                       {"schema": 1, "objects": {"x.obj": {"status": "wat"}}})))

            # --- queue selection over a synthetic frontier ---
            frontier = root / "frontier.json"
            frontier.write_text(json.dumps({"eligible": [
                {"object": "small.obj", "funcs": 4, "score": 400.0, "rank": 1},
                {"object": "a.obj", "funcs": 20, "score": 300.0, "rank": 2},
                {"object": "big.obj", "funcs": 40, "score": 200.0, "rank": 3},
                {"object": "tiny.obj", "funcs": 30, "score": 1.0, "rank": 4},
            ]}), encoding="utf-8")
            all_rows = frontier_rows(str(frontier))
            _check("frontier rows are score-ordered",
                   lambda: [row["object"] for row in all_rows]
                   == ["small.obj", "a.obj", "big.obj", "tiny.obj"])
            _check("--min-funcs filters a reused ranking",
                   lambda: [row["object"] for row in frontier_rows(str(frontier), 10)]
                   == ["a.obj", "big.obj", "tiny.obj"])
            _check("next skips objects already taken by the ledger",
                   lambda: select_next(frontier_rows(str(frontier), 10), ledger)[0]["object"]
                   == "big.obj")
            _check("--min-score excludes low-value objects",
                   lambda: select_next(all_rows, ledger, 100.0)[0]["object"] == "small.obj")
            _check("queue exhaustion returns no row with counted reasons",
                   lambda: select_next(frontier_rows(str(frontier), 10), ledger, 500.0)
                   == (None, {"eligible": 3, "taken": 1, "below_min_score": 2}))
            _check("empty frontier is exhaustion, not an error",
                   lambda: select_next([], ledger) == (None, {"eligible": 0, "taken": 0,
                                                              "below_min_score": 0}))
            _check("frontier JSON without 'eligible' is a clean error",
                   lambda: (frontier.write_text("{}", encoding="utf-8"),
                            _raises(lambda: frontier_rows(str(frontier))))[1])
            _check("missing frontier JSON is a clean error",
                   lambda: _raises(lambda: frontier_rows(str(root / "absent.json"))))

            # --- rendering never raises on sparse rows ---
            _check("row rendering tolerates a sparse frontier row",
                   lambda: "next recovery target: x.obj" in _format_row({"object": "x.obj"}))
            _check("status table renders an empty ledger",
                   lambda: "(empty" in _format_status(_empty_ledger()))
            _check("status table renders every entry",
                   lambda: all(name in _format_status(ledger) for name in ledger["objects"]))

            # --- ladder vocabulary stays in sync with source_recovery ---
            def _ladder_matches() -> bool:
                try:
                    from tools.recovery import source_recovery
                except ImportError:
                    return True  # optional dependency; nothing to compare against
                return tuple(source_recovery.LADDER) == LADDER

            _check("ladder vocabulary matches source_recovery.LADDER", _ladder_matches)
    finally:
        _today = original_today

    for passed, name in checks:
        print("  %s %s" % ("ok  " if passed else "FAIL", name))
    failures = sum(1 for passed, _name in checks if not passed)
    print("%d check(s), %d failure(s)" % (len(checks), failures))
    return EXIT_OK if failures == 0 else 1


# --- entry point -----------------------------------------------------------

def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Queue and ledger for /recover-goal recovery campaigns.")
    parser.add_argument("--self-test", action="store_true",
                        help="run internal checks and exit non-zero on failure")
    parser.add_argument("--ledger", default=str(LEDGER_PATH),
                        help="ledger path (default recovery/goal_ledger.json)")
    sub = parser.add_subparsers(dest="command")

    nxt = sub.add_parser("next", help="highest-scored eligible object not yet taken")
    nxt.add_argument("--frontier-json",
                     help="reuse a recovery_frontier.py --json ranking instead of "
                          "recomputing it")
    nxt.add_argument("--min-funcs", type=int, default=0,
                     help="skip objects with fewer than N functions")
    nxt.add_argument("--min-score", type=float, default=0.0,
                     help="skip objects scored below X")
    nxt.add_argument("--json", action="store_true", help="machine-readable output")

    start = sub.add_parser("start", help="mark an object in progress")
    start.add_argument("object")
    start.add_argument("--force", action="store_true",
                       help="re-open an object that is done or already in progress")

    finish = sub.add_parser("finish", help="mark an object recovered")
    finish.add_argument("object")
    finish.add_argument("--commit", action="append", default=[], metavar="SHA",
                        help="commit sha landed for this object (repeatable)")
    finish.add_argument("--category", action="append", default=[], metavar="LADDER_ID",
                        help="ladder category completed (repeatable)")
    finish.add_argument("--no-commits", action="store_true",
                        help="record a recovery that landed no commits (suspicious; be explicit)")

    park = sub.add_parser("park", help="abandon an object with a reason")
    park.add_argument("object")
    park.add_argument("--reason", required=True)
    park.add_argument("--force", action="store_true",
                      help="park an object already recorded as done")

    status = sub.add_parser("status", help="ledger summary")
    status.add_argument("--json", action="store_true", help="machine-readable output")

    args = parser.parse_args(argv)
    handlers = {"next": _cmd_next, "start": _cmd_start, "finish": _cmd_finish,
                "park": _cmd_park, "status": _cmd_status}
    try:
        if args.self_test:
            return _self_test()
        if args.command not in handlers:
            parser.error("a subcommand is required")
        path = _repo_path(args.ledger, ".json")
        return handlers[args.command](args, path, _load_ledger(path))
    except GoalError as exc:
        print("error: %s" % exc, file=sys.stderr)
        return EXIT_ERROR
    except OSError as exc:
        print("error: %s" % exc, file=sys.stderr)
        return EXIT_ERROR


if __name__ == "__main__":
    raise SystemExit(main())
