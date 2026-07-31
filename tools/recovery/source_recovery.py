#!/usr/bin/env python3
"""Manifest-driven, evidence-gated source recovery workflow."""

from __future__ import annotations

import argparse
import hashlib
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

TOOL_VERSION = "source-recovery/1"
SCHEMA = 1
NOISY_PARTS = {"build", "build_debug", "node_modules", ".git", "halo-patched", "__pycache__", "dist"}

PATTERNS = {
    "raw_function_pointer_address_casts": re.compile(r"\(\(.*\(\*\).*\)0x[0-9a-fA-F]+"),
    "xcall_uses": re.compile(r"\bXCALL\s*\("),
    "fun_calls": re.compile(r"\bFUN_[0-9A-Fa-f]{4,}\s*\("),
    "absolute_address_dereferences": re.compile(
        r"\*\s*\([^\n)]*\*\)\s*(?:\(\s*)?0x[0-9A-Fa-f]+"
    ),
    "raw_base_offset_dereferences": re.compile(
        r"\*\([A-Za-z_][\w ]*\*\)\([A-Za-z_]\w*\s+\+\s+0x[0-9A-Fa-f]+\)"
    ),
    "decompiler_style_locals": re.compile(
        r"\b(?:local_[0-9A-Fa-f]+|[uifdb]Var[0-9]+|p[uifdb]?Var[0-9]+)\b"
    ),
    "inline_asm": re.compile(r"\b__asm\b|\basm\s*(?:volatile\s*)?\("),
}


class RecoveryError(Exception):
    """Expected, fail-closed workflow error."""


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _repo_path(value: str, suffix: str | None = None, allow_noisy: bool = False) -> Path:
    path = Path(value).expanduser()
    if not path.is_absolute():
        path = ROOT / path
    try:
        resolved = path.resolve(strict=True)
        resolved.relative_to(ROOT)
    except (OSError, ValueError):
        raise RecoveryError("path is not an existing path inside the repository: %s" % value)
    if suffix and resolved.suffix != suffix:
        raise RecoveryError("source must be a %s path: %s" % (suffix, value))
    if not allow_noisy and any(part in NOISY_PARTS for part in resolved.relative_to(ROOT).parts):
        raise RecoveryError("generated/noisy path is not allowed: %s" % value)
    return resolved


def _output_path(value: str, suffix: str) -> Path:
    path = Path(value).expanduser()
    if not path.is_absolute():
        path = ROOT / path
    try:
        resolved = path.resolve(strict=False)
        resolved.parent.relative_to(ROOT)
    except (OSError, ValueError):
        raise RecoveryError("output path is not inside the repository: %s" % value)
    if resolved.suffix != suffix:
        raise RecoveryError("output must be a %s path: %s" % (suffix, value))
    if any(part in NOISY_PARTS for part in resolved.relative_to(ROOT).parts):
        raise RecoveryError("generated/noisy path is not allowed: %s" % value)
    return resolved


def _relative(path: Path) -> str:
    return path.relative_to(ROOT).as_posix()


def _git_head() -> str:
    try:
        result = subprocess.run(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, check=True,
            capture_output=True, text=True,
        )
    except (OSError, subprocess.CalledProcessError) as exc:
        raise RecoveryError("unable to determine git HEAD: %s" % exc)
    return result.stdout.strip()


def _inventory(source: Path) -> dict[str, list[dict[str, Any]]]:
    try:
        lines = source.read_text(encoding="utf-8", errors="replace").splitlines()
    except OSError as exc:
        raise RecoveryError("unable to read source: %s" % exc)
    inventory: dict[str, list[dict[str, Any]]] = {name: [] for name in PATTERNS}
    for line_number, line in enumerate(lines, 1):
        for category, pattern in PATTERNS.items():
            for occurrence, match in enumerate(pattern.finditer(line), 1):
                item_id = "%s:%d:%d" % (category, line_number, occurrence)
                inventory[category].append({
                    "id": item_id,
                    "line": line_number,
                    "column": match.start() + 1,
                    "text": line.strip(),
                    "status": "pending",
                })
    return inventory


def _items(inventory: dict[str, list[dict[str, Any]]]) -> list[dict[str, Any]]:
    return [item for category in sorted(inventory) for item in inventory[category]]


def _plan(source_arg: str) -> dict[str, Any]:
    source = _repo_path(source_arg, ".c")
    inventory = _inventory(source)
    return {
        "schema": SCHEMA,
        "kind": "source-recovery-manifest",
        "created_tool_version": TOOL_VERSION,
        "repo_relative_source": _relative(source),
        "git_head": _git_head(),
        "source_sha256": _sha256(source),
        "inventory": inventory,
        "items": _items(inventory),
        "baseline": {"captured": False},
        "checks": [],
    }


def _validate_manifest(manifest: Any) -> dict[str, Any]:
    if not isinstance(manifest, dict) or manifest.get("schema") != SCHEMA or manifest.get("kind") != "source-recovery-manifest":
        raise RecoveryError("malformed manifest schema")
    required = ("created_tool_version", "repo_relative_source", "git_head", "source_sha256", "inventory", "items", "baseline", "checks")
    if any(key not in manifest for key in required):
        raise RecoveryError("malformed manifest: missing required field")
    source = _repo_path(manifest["repo_relative_source"], ".c")
    if not isinstance(manifest["git_head"], str) or not manifest["git_head"]:
        raise RecoveryError("malformed manifest: git_head")
    if not isinstance(manifest["source_sha256"], str) or not re.fullmatch(r"[0-9a-f]{64}", manifest["source_sha256"]):
        raise RecoveryError("malformed manifest: source_sha256")
    inventory = manifest["inventory"]
    items = manifest["items"]
    if not isinstance(inventory, dict) or not isinstance(items, list):
        raise RecoveryError("malformed manifest: inventory")
    seen: set[str] = set()
    inventory_items = []
    for category, category_items in inventory.items():
        if not isinstance(category, str) or not isinstance(category_items, list):
            raise RecoveryError("malformed manifest: invalid inventory category")
        inventory_items.extend(category_items)
    if len(inventory_items) != len(items):
        raise RecoveryError("malformed manifest: inventory/items mismatch")
    for item in items:
        if not isinstance(item, dict) or not isinstance(item.get("id"), str) or item["id"] in seen:
            raise RecoveryError("malformed manifest: duplicate or invalid item id")
        seen.add(item["id"])
        if item.get("status") not in ("pending", "applied", "parked"):
            raise RecoveryError("malformed manifest: invalid item status")
        if not isinstance(item.get("line"), int) or item["line"] < 1:
            raise RecoveryError("malformed manifest: invalid item line")
    inventory_by_id = {item.get("id"): item for item in inventory_items}
    if set(inventory_by_id) != seen:
        raise RecoveryError("malformed manifest: inventory/items ids differ")
    items_by_id = {item["id"]: item for item in items}
    if any(inventory_by_id[item_id].get("status") != items_by_id[item_id].get("status")
           for item_id in seen):
        raise RecoveryError("malformed manifest: inventory/items statuses differ")
    if not isinstance(manifest["baseline"], dict) or not isinstance(manifest["checks"], list):
        raise RecoveryError("malformed manifest: baseline/checks")
    return manifest


def _load(path_arg: str) -> tuple[Path, dict[str, Any]]:
    path = _repo_path(path_arg, ".json")
    try:
        with path.open(encoding="utf-8") as stream:
            manifest = json.load(stream)
    except (OSError, json.JSONDecodeError) as exc:
        raise RecoveryError("unable to read manifest: %s" % exc)
    return path, _validate_manifest(manifest)


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


def _ensure_current(manifest: dict[str, Any]) -> Path:
    source = _repo_path(manifest["repo_relative_source"], ".c")
    if _sha256(source) != manifest["source_sha256"]:
        raise RecoveryError("source hash no longer matches plan")
    if _git_head() != manifest["git_head"]:
        raise RecoveryError("git revision no longer matches plan")
    return source


def _capture(path: Path, manifest: dict[str, Any], object_arg: str) -> None:
    source = _ensure_current(manifest)
    obj = _repo_path(object_arg, allow_noisy=True)
    from tools.recovery import assert_metadata_guard, coff_candidate_guard

    try:
        coff_snapshot = coff_candidate_guard.capture_object(str(obj))
        assertion_snapshot = assert_metadata_guard.capture_sources([str(source)])
    except Exception as exc:
        raise RecoveryError("baseline capture failed: %s" % exc)
    manifest["baseline"] = {
        "captured": True,
        "object_relative_path": _relative(obj),
        "object_sha256": _sha256(obj),
        "coff_snapshot": coff_snapshot,
        "assertion_snapshot": assertion_snapshot,
    }
    manifest["checks"] = []


def _run_vc71(source: Path) -> int:
    from tools.verify import vc71_regression
    args = argparse.Namespace(source=[str(source)], threshold=0.0, quiet=True, strict=True)
    return vc71_regression.cmd_check(args)


def _check(path: Path, manifest: dict[str, Any], object_arg: str, skip_vc71: bool,
           mode: str = "neutral") -> dict[str, Any]:
    if mode not in ("neutral", "corrective"):
        raise RecoveryError("invalid check mode: %s" % mode)
    result: dict[str, Any] = {
        "ok": False,
        "mode": mode,
        "source": manifest["repo_relative_source"],
        "current_source_sha256": None,
        "current_git_head": None,
        "baseline_object_sha256": None,
        "current_object_sha256": None,
        "failures": [],
        "skipped": [],
        "observations": {"changes": []},
    }
    try:
        source = _repo_path(manifest["repo_relative_source"], ".c")
        result["current_source_sha256"] = _sha256(source)
        result["current_git_head"] = _git_head()
        baseline = manifest["baseline"]
        if baseline.get("captured") is not True:
            raise RecoveryError("no captured baseline")
        obj = _repo_path(object_arg, allow_noisy=True)
        result["baseline_object_sha256"] = baseline.get("object_sha256")
        result["current_object_sha256"] = _sha256(obj)
        from tools.recovery import assert_metadata_guard, coff_candidate_guard
        try:
            coff = coff_candidate_guard.compare_snapshots(
                baseline["coff_snapshot"], coff_candidate_guard.capture_object(str(obj)))
        except Exception as exc:
            result["failures"].append("COFF baseline comparison failed: %s" % exc)
        else:
            if not coff["ok"]:
                changes = ["COFF: " + error for error in coff["errors"]]
                if mode == "corrective":
                    result["observations"]["changes"].extend(changes)
                else:
                    result["failures"].extend(changes)
        try:
            assertion = assert_metadata_guard.compare_snapshots(
                baseline["assertion_snapshot"], assert_metadata_guard.capture_sources([str(source)]))
        except Exception as exc:
            result["failures"].append("assertion baseline comparison failed: %s" % exc)
        else:
            if not assertion["ok"]:
                result["failures"].extend("assertion: " + error for error in assertion["errors"])
        if skip_vc71:
            result["skipped"].append("vc71_regression.py check --strict")
        else:
            result["vc71_passed"] = _run_vc71(source) == 0
            if not result["vc71_passed"]:
                result["failures"].append("strict VC71 regression check failed")
    except (OSError, ValueError, RecoveryError, KeyError) as exc:
        result["failures"].append(str(exc))
    if skip_vc71:
        result["vc71_passed"] = False
    result["ok"] = not result["failures"] and not result["skipped"]
    manifest["checks"].append(result)
    _write_atomic(path, manifest)
    return result


def _report(manifest: dict[str, Any]) -> str:
    items = manifest["items"]
    counts = {status: sum(item["status"] == status for item in items) for status in ("pending", "applied", "parked")}
    baseline = manifest["baseline"]
    failures = [failure for check in manifest["checks"] for failure in check.get("failures", [])]
    skipped = [gate for check in manifest["checks"] for gate in check.get("skipped", [])]
    last_check = "passed" if manifest["checks"] and manifest["checks"][-1].get("ok") else "not passed"
    if manifest["checks"] and not manifest["checks"][-1].get("ok") and skipped and not failures:
        last_check += " (skipped gates)"
    lines = ["# Source Recovery Report", "", "- Source: `%s`" % manifest["repo_relative_source"], "- Plan: `%s` at `%s`" % (manifest["created_tool_version"], manifest["git_head"]),
             "- Debt: %d items (%d pending, %d applied, %d parked)" % (len(items), counts["pending"], counts["applied"], counts["parked"]),
             "- Baseline: %s" % ("captured" if baseline.get("captured") else "not captured"),
             "- Last check: %s" % last_check]
    if failures:
        lines.append("- Failures: %s" % "; ".join(failures))
    else:
        lines.append("- Failures: none recorded")
    if skipped:
        lines.append("- Skipped gates: %s" % "; ".join(skipped))
    else:
        lines.append("- Skipped gates: none recorded")
    lines.append("")
    lines.append("## Debt By Category")
    for category in sorted(manifest["inventory"]):
        lines.append("- `%s`: %d" % (category, len(manifest["inventory"][category])))
    return "\n".join(lines) + "\n"


def _set_status(path: Path, manifest: dict[str, Any], item_id: str, status: str, reason: str | None) -> None:
    if status == "parked" and not (reason and reason.strip()):
        raise RecoveryError("parked items require --reason")
    item_matches = [item for item in manifest["items"] if item["id"] == item_id]
    inventory_matches = [item for category in manifest["inventory"].values()
                         for item in category if item["id"] == item_id]
    if len(item_matches) != 1 or len(inventory_matches) != 1:
        raise RecoveryError("item not found: %s" % item_id)
    updates = item_matches + inventory_matches
    for item in updates:
        item["status"] = status
        if reason:
            item["reason"] = reason
        elif status != "parked":
            item.pop("reason", None)
    _write_atomic(path, manifest)


def _self_test() -> int:
    from tools.recovery import assert_metadata_guard, coff_candidate_guard

    checks = [
        (bool(PATTERNS["fun_calls"].search("FUN_00123456();")), "FUN call detection"),
        (not bool(PATTERNS["absolute_address_dereferences"].search("return 42;")), "numeric literal is not address debt"),
        (TOOL_VERSION.startswith("source-recovery/"), "versioned tool"),
        (assert_metadata_guard is not None and coff_candidate_guard is not None,
         "recovery guard imports"),
    ]
    for passed, name in checks:
        print("  %s %s" % ("ok  " if passed else "FAIL", name))
    return 0 if all(passed for passed, _name in checks) else 1


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    sub = parser.add_subparsers(dest="command")
    plan = sub.add_parser("plan")
    plan.add_argument("--source", required=True)
    plan.add_argument("-o", "--output", required=True)
    capture = sub.add_parser("capture")
    capture.add_argument("manifest")
    capture.add_argument("--object", required=True)
    check = sub.add_parser("check")
    check.add_argument("manifest")
    check.add_argument("--object", required=True)
    check.add_argument("--mode", choices=("neutral", "corrective"), default="neutral")
    check.add_argument("--skip-vc71", action="store_true")
    report = sub.add_parser("report")
    report.add_argument("manifest")
    status = sub.add_parser("set-status")
    status.add_argument("manifest")
    status.add_argument("item_id")
    status.add_argument("status", choices=("pending", "applied", "parked"))
    status.add_argument("--reason")
    args = parser.parse_args(argv)
    try:
        if args.self_test:
            return _self_test()
        if args.command == "plan":
            _write_atomic(_output_path(args.output, ".json"), _plan(args.source))
            return 0
        path, manifest = _load(args.manifest)
        if args.command == "capture":
            _capture(path, manifest, args.object)
            _write_atomic(path, manifest)
            return 0
        if args.command == "check":
            result = _check(path, manifest, args.object, args.skip_vc71, args.mode)
            print(json.dumps(result, indent=2, sort_keys=True))
            return 0 if result["ok"] else 1
        if args.command == "report":
            print(_report(manifest), end="")
            return 0
        if args.command == "set-status":
            _set_status(path, manifest, args.item_id, args.status, args.reason)
            return 0
        parser.error("a subcommand is required")
    except (OSError, ValueError, RecoveryError) as exc:
        print(json.dumps({"ok": False, "failures": [str(exc)]}, sort_keys=True))
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
