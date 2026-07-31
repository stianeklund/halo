#!/usr/bin/env python3
"""Capture and compare source assertion metadata for recovery work."""

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
_SKIPPED_PARTS = {".dcg", ".git", "build", "build_debug", "dist", "generated",
                  "halo-patched", "node_modules", "__pycache__"}
_MACROS = ("assert_halt_at", "assert_warn_at", "assert_at", "assert_halt_msg",
           "assert_halt", "assert_warn", "assert")
_MACRO_RE = re.compile(r"\b(" + "|".join(_MACROS) + r")\s*\(")


class GuardError(Exception):
    """A source file or baseline cannot be trusted."""


def _source_path(path):
    resolved = Path(path).resolve()
    try:
        relative = resolved.relative_to(ROOT)
    except ValueError:
        raise GuardError("source is outside repository: %s" % path)
    if any(part in _SKIPPED_PARTS for part in relative.parts):
        raise GuardError("generated/build source is not allowed: %s" % path)
    if not resolved.is_file():
        raise GuardError("missing source: %s" % path)
    return resolved, relative.as_posix()


def _without_comments(text):
    result = []
    i = 0
    state = "code"
    while i < len(text):
        char = text[i]
        following = text[i + 1] if i + 1 < len(text) else ""
        if state == "code":
            if char == '"':
                state = "string"
                result.append(char)
            elif char == "'":
                state = "char"
                result.append(char)
            elif char == "/" and following == "/":
                result.append(" ")
                state = "line_comment"
                i += 1
            elif char == "/" and following == "*":
                result.append(" ")
                state = "block_comment"
                i += 1
            else:
                result.append(char)
        elif state in ("string", "char"):
            result.append(char)
            if char == "\\" and following:
                result.append(following)
                i += 1
            elif (state == "string" and char == '"') or (state == "char" and char == "'"):
                state = "code"
        elif state == "line_comment":
            if char == "\n":
                result.append(char)
                state = "code"
        else:
            if char == "\n":
                result.append(char)
            if char == "*" and following == "/":
                result.append(" ")
                state = "code"
                i += 1
        i += 1
    if state == "block_comment":
        raise GuardError("unterminated comment")
    if state in ("string", "char"):
        raise GuardError("unterminated string or character literal")
    return "".join(result)


def _invocation_end(text, open_index):
    depth = 1
    i = open_index + 1
    state = "code"
    while i < len(text):
        char = text[i]
        following = text[i + 1] if i + 1 < len(text) else ""
        if state == "code":
            if char == '"':
                state = "string"
            elif char == "'":
                state = "char"
            elif char == "(" :
                depth += 1
            elif char == ")":
                depth -= 1
                if depth == 0:
                    return i
        elif char == "\\" and following:
            i += 1
        elif (state == "string" and char == '"') or (state == "char" and char == "'"):
            state = "code"
        i += 1
    raise GuardError("unterminated assertion invocation")


def _split_arguments(text):
    args = []
    start = 0
    depth = 0
    state = "code"
    i = 0
    while i < len(text):
        char = text[i]
        following = text[i + 1] if i + 1 < len(text) else ""
        if state == "code":
            if char == '"':
                state = "string"
            elif char == "'":
                state = "char"
            elif char == "(":
                depth += 1
            elif char == ")":
                depth -= 1
            elif char == "," and depth == 0:
                args.append(text[start:i])
                start = i + 1
        elif char == "\\" and following:
            i += 1
        elif (state == "string" and char == '"') or (state == "char" and char == "'"):
            state = "code"
        i += 1
    if state != "code" or depth != 0:
        raise GuardError("malformed assertion arguments")
    args.append(text[start:])
    return args


def _stringified(text):
    return re.sub(r"\s+", " ", text).strip()


def _capture_file(path):
    resolved, relative = _source_path(path)
    original = resolved.read_text(encoding="utf-8")
    stripped = _without_comments(original)
    records = []
    for match in _MACRO_RE.finditer(stripped):
        line_start = stripped.rfind("\n", 0, match.start()) + 1
        if stripped[line_start:match.start()].lstrip().startswith("#"):
            continue
        flavor = match.group(1)
        end = _invocation_end(stripped, match.end() - 1)
        args = _split_arguments(stripped[match.end():end])
        explicit = flavor.endswith("_at")
        expected = 3 if explicit else (2 if flavor.endswith("_msg") else 1)
        if len(args) != expected or not _stringified(args[0]):
            raise GuardError("malformed %s invocation at %s:%d" %
                             (flavor, relative, stripped.count("\n", 0, match.start()) + 1))
        record = {"path": relative,
                  "line": stripped.count("\n", 0, match.start()) + 1,
                  "flavor": flavor,
                  "condition": _stringified(args[2] if explicit else args[0])}
        if explicit:
            record["explicit_file"] = _stringified(args[0])
            record["explicit_line"] = _stringified(args[1])
        records.append(record)
    return records


def capture_sources(paths):
    records = []
    for path in paths:
        records.extend(_capture_file(path))
    return {"schema": 1, "kind": "assert-metadata", "assertions": records}


def _validate(snapshot):
    if (not isinstance(snapshot, dict) or snapshot.get("schema") != 1 or
            snapshot.get("kind") != "assert-metadata" or
            not isinstance(snapshot.get("assertions"), list)):
        raise GuardError("malformed baseline")
    for record in snapshot["assertions"]:
        required = ("path", "line", "flavor", "condition")
        if not isinstance(record, dict) or any(key not in record for key in required):
            raise GuardError("malformed baseline")
        if (not isinstance(record["path"], str) or not isinstance(record["line"], int) or
                record["line"] < 1 or not isinstance(record["flavor"], str) or
                not isinstance(record["condition"], str)):
            raise GuardError("malformed baseline")
        if record["flavor"].endswith("_at") and not all(
                isinstance(record.get(key), str) for key in ("explicit_file", "explicit_line")):
            raise GuardError("malformed baseline")


def compare_snapshots(before, after):
    _validate(before)
    _validate(after)
    errors = []
    if before["assertions"] != after["assertions"]:
        errors.append("assertion metadata changed")
    return {"ok": not errors, "errors": errors}


def _self_test():
    source = ROOT / "tools" / "recovery" / ".assert_metadata_selftest.c"
    source.write_text(
        'assert_halt(foo(1, "x,y") /* c */ && value);\n'
        'assert_warn(outer(a, b) && "quoted )");\n'
        'assert_halt_at("old.c", 42, value /* inline */ > \'\\\')\');\n',
        encoding="utf-8")
    try:
        snapshot = capture_sources([source])
        checks = [
            ("three assertions", len(snapshot["assertions"]) == 3),
            ("line metadata", snapshot["assertions"][1]["line"] ==
             snapshot["assertions"][0]["line"] + 1),
            ("nested condition", snapshot["assertions"][0]["condition"] == 'foo(1, "x,y") && value'),
            ("explicit metadata", snapshot["assertions"][2]["explicit_line"] == "42"),
        ]
        altered = json.loads(json.dumps(snapshot))
        altered["assertions"][0]["condition"] = "changed"
        checks.append(("condition change fails", not compare_snapshots(snapshot, altered)["ok"]))
        try:
            _invocation_end("assert_halt(foo", 11)
        except GuardError:
            checks.append(("unterminated fails", True))
        else:
            checks.append(("unterminated fails", False))
        for name, passed in checks:
            print("  %s %s" % ("ok  " if passed else "FAIL", name))
        return 0 if all(passed for _name, passed in checks) else 1
    finally:
        source.unlink(missing_ok=True)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    sub = parser.add_subparsers(dest="command")
    capture = sub.add_parser("capture")
    capture.add_argument("sources", nargs="+")
    capture.add_argument("-o", "--output", required=True)
    check = sub.add_parser("check")
    check.add_argument("baseline")
    check.add_argument("sources", nargs="+")
    args = parser.parse_args(argv)
    if args.self_test:
        return _self_test()
    try:
        if args.command == "capture":
            result = capture_sources(args.sources)
            Path(args.output).write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="ascii")
            return 0
        if args.command == "check":
            with open(args.baseline, "r", encoding="ascii") as stream:
                baseline = json.load(stream)
            result = compare_snapshots(baseline, capture_sources(args.sources))
            print(json.dumps(result, indent=2, sort_keys=True))
            return 0 if result["ok"] else 1
        parser.error("a subcommand is required")
    except (OSError, UnicodeError, ValueError, GuardError) as exc:
        print(json.dumps({"ok": False, "errors": [str(exc)]}, sort_keys=True))
        return 2


if __name__ == "__main__":
    sys.exit(main())
