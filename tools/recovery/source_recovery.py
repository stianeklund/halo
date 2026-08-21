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

TOOL_VERSION = "source-recovery/2"
SCHEMA = 1
NOISY_PARTS = {"build", "build_debug", "node_modules", ".git", "halo-patched", "__pycache__", "dist"}

# The cleanup ladder, in mandatory risk order. Item categories use this
# vocabulary; the order below is the order work must be done in.
LADDER = (
    "comments",
    "local-renames",
    "symbol-names",
    "global-names",
    "const-enum",
    "struct-define",
    "offset-to-field",
    "expr-simplify",
    "control-flow",
)
# Categories whose rewrites legitimately move codegen. Working one requires
# `allow_risky` on the manifest (set at plan time via --allow-risky).
RISKY_CATEGORIES = frozenset({"expr-simplify", "control-flow"})
UNCATEGORIZED = "uncategorized"
CATEGORIES = LADDER + (UNCATEGORIZED,)

LADDER_SKILLS = {
    "comments": "re-comment-capture",
    "local-renames": "local-var-cleanup",
    "symbol-names": "naming-confidence",
    "global-names": "naming-confidence",
    "const-enum": "const-enum-recovery",
    "struct-define": "struct-recovery+struct-assert",
    "offset-to-field": "offset-to-struct",
    "expr-simplify": "expr-simplify",
    "control-flow": "control-flow-cleanup",
    UNCATEGORIZED: "-",
}

# Default ladder category for each debt detector. A detector only proposes the
# category; `set-status --category` overrides it when the evidence says so.
DETECTOR_CATEGORY = {
    "raw_function_pointer_address_casts": "symbol-names",
    "xcall_uses": "symbol-names",
    "fun_calls": "symbol-names",
    "absolute_address_dereferences": "global-names",
    "raw_base_offset_dereferences": "offset-to-field",
    "struct_bases": "struct-define",
    "magic_fourcc": "const-enum",
    "decompiler_style_locals": "local-renames",
    "inline_asm": UNCATEGORIZED,
}

# Detectors whose match still counts as debt when it lands inside a comment.
# Only decompiler-generated NAMES qualify: `FUN_00123456` or `local_1c` in
# prose is a readability artifact the ladder is meant to remove.  A raw address
# or a byte offset mentioned in prose is documentation, not debt.
COMMENT_DEBT_DETECTORS = frozenset({"fun_calls", "decompiler_style_locals"})

# Detectors that are NOT a line regex in PATTERNS: they are derived from a
# whole-file pass because the debt is a cluster, not an occurrence.
DERIVED_DETECTORS = frozenset({"struct_bases", "magic_fourcc"})

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


# A rename is one decision per SYMBOL, not per occurrence.  `plan` used to file
# one item per regex hit, so an address used 19 times became 19 items and the
# category agent re-ran the same evidence hunt 19 times to reach the same
# verdict.  Measured on game_engine.c (2026-08-21): global-names 547 items over
# 154 distinct addresses, of which 527 parks covered 152 addresses -- 84
# addresses parked more than once, 67 of those with byte-identical reasons, 375
# redundant decisions.  offset-to-field was 529 items over 169 (base, offset)
# pairs, with `(player, 0x20)` alone appearing 46 times.
#
# Grouping is per DECISION TARGET, so it is only correct where the edit is
# inherently file-wide: you cannot rename one occurrence of a global and leave
# the other 18.  `decompiler_style_locals` is deliberately NOT grouped -- Ghidra
# reuses `local_c` for unrelated variables in different functions (5 distinct
# roles in game_engine.c), so one name is several independent decisions and
# check_category_purity requires a single injective map per file.
_ADDR_RE = re.compile(r"0x[0-9A-Fa-f]+")
_FUN_RE = re.compile(r"FUN_[0-9A-Fa-f]{4,}")
_BASE_OFFSET_RE = re.compile(
    r"\*\(\s*[A-Za-z_][\w ]*\*\)\(\s*([A-Za-z_]\w*)\s*\+\s*(0x[0-9A-Fa-f]+)\)")


def _key_address(text: str) -> str | None:
    found = _ADDR_RE.search(text)
    return found.group(0).lower() if found else None


def _key_fun(text: str) -> str | None:
    found = _FUN_RE.search(text)
    return ("FUN_" + found.group(0)[4:].lower()) if found else None


def _key_base_offset(text: str) -> str | None:
    found = _BASE_OFFSET_RE.search(text)
    return "%s+%s" % (found.group(1), found.group(2).lower()) if found else None


# detector -> key extractor over the MATCHED TEXT.  A detector absent here, or an
# extractor returning None, keeps the per-occurrence item: a decision target we
# cannot name reliably must not be silently merged with a different one.
GROUP_KEY = {
    "absolute_address_dereferences": _key_address,
    "raw_function_pointer_address_casts": _key_address,
    "fun_calls": _key_fun,
    "raw_base_offset_dereferences": _key_base_offset,
}


def _category_of(item: dict[str, Any]) -> str:
    """Ladder category of an item; pre-ladder manifests default to uncategorized."""
    value = item.get("category")
    return value if isinstance(value, str) and value else UNCATEGORIZED


def _ladder_index(category: str) -> int:
    """Ladder position; uncategorized sorts last and never gates ordering."""
    return LADDER.index(category) if category in LADDER else len(LADDER)


def _ladder_warnings(manifest: dict[str, Any]) -> list[str]:
    """Warn when a later ladder category was worked before an earlier one finished.

    Advisory only: a category may be legitimately inapplicable, so this never
    fails a check.
    """
    pending: set[str] = set()
    applied: set[str] = set()
    for item in manifest.get("items", []):
        category = _category_of(item)
        if category not in LADDER:
            continue
        if item.get("status") == "pending":
            pending.add(category)
        elif item.get("status") == "applied":
            applied.add(category)
    warnings = []
    for later in sorted(applied, key=_ladder_index):
        earlier = sorted((c for c in pending if _ladder_index(c) < _ladder_index(later)),
                         key=_ladder_index)
        if earlier:
            warnings.append(
                "ladder order: `%s` items are applied while earlier categories still have "
                "pending items (%s) — finish or park them first" % (later, ", ".join(earlier)))
    return warnings


def _risky_failures(manifest: dict[str, Any]) -> list[str]:
    """Fail applied work in a risky category unless the manifest opted in."""
    if manifest.get("allow_risky") is True:
        return []
    worked = sorted({_category_of(item) for item in manifest.get("items", [])
                     if _category_of(item) in RISKY_CATEGORIES
                     and item.get("status") == "applied"}, key=_ladder_index)
    return ["risky category `%s` has applied items but the manifest is not opted in "
            "(re-plan with --allow-risky and provide a behavioral oracle)" % category
            for category in worked]


def _ladder_counts(manifest: dict[str, Any]) -> dict[str, dict[str, int]]:
    counts = {category: {"pending": 0, "applied": 0, "parked": 0} for category in CATEGORIES}
    for item in manifest.get("items", []):
        category = _category_of(item)
        counts.setdefault(category, {"pending": 0, "applied": 0, "parked": 0})
        counts[category][item["status"]] += 1
    return counts


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


# The base of a raw `*(T *)(base + 0xNN)` deref, and the offset it reads.  A
# struct cannot be proposed from one site: the debt is the whole (base, offset
# set) cluster, which is what `struct-define` has to cover before
# `offset-to-field` can rewrite anything against it.
_BASE_OFFSET = re.compile(
    r"\*\((?P<type>[A-Za-z_][\w ]*)\*\)\((?P<base>[A-Za-z_]\w*)\s+\+\s+"
    r"(?P<offset>0x[0-9A-Fa-f]+)\)"
)


def _comment_mask(text: str) -> list[bool]:
    """One flag per character: True inside a comment or a string literal.

    Detectors are line regexes, so without this a `FUN_00123456()` written in
    an explanatory comment is filed as live call-site debt in `symbol-names`,
    where it can never be applied -- renaming it is a `comments` edit.  Strings
    are masked for the same reason: an address inside a message is not a
    dereference."""
    mask = [False] * len(text)
    index = 0
    total = len(text)
    while index < total:
        char = text[index]
        if text.startswith("//", index):
            end = text.find("\n", index)
            end = total if end < 0 else end
            for position in range(index, end):
                mask[position] = True
            index = end
            continue
        if text.startswith("/*", index):
            end = text.find("*/", index + 2)
            end = total if end < 0 else end + 2
            for position in range(index, end):
                mask[position] = True
            index = end
            continue
        if char in ('"', "'"):
            end = index + 1
            while end < total and text[end] != char:
                end += 2 if text[end] == "\\" else 1
            end = min(end + 1, total)
            for position in range(index, end):
                mask[position] = True
            index = end
            continue
        index += 1
    return mask


_FOURCC = re.compile(r"(?<![\w.])0[xX]([0-9a-fA-F]{8})(?![\w.])")


def _fourcc_text(digits: str) -> str | None:
    """Decode a 32-bit hex literal as four printable ASCII bytes, or None.

    A tag group, a signature, or a fill pattern is written in the binary as
    its characters (`0x61637472` is 'actr'), so the literal carries its own
    name evidence -- T1 under naming-confidence, with nothing to infer.  A
    Halo virtual address cannot collide: they live below 0x00600000, so the
    high byte is 0x00 and fails the printable test.
    """
    try:
        raw = bytes.fromhex(digits)
    except ValueError:
        return None
    if len(raw) != 4 or any(byte < 0x20 or byte > 0x7e for byte in raw):
        return None
    return raw.decode("ascii")


def _magic_fourcc_items(lines: list[str], masks: list[list[bool]]) -> list[dict[str, Any]]:
    """One `const-enum` item per distinct four-character-code literal.

    This is the ONLY numeric literal the ladder proposes naming, and the
    restriction is deliberate.  Matching literals against the `#define`s
    already in `src/**.h` was measured and is worthless: small integers
    collide with everything (`6` matched `_actor_action_guard` at 192 sites).
    Frequency alone is no better -- the most repeated literals in a TU are
    `0.0f`, `1.0f`, and struct offsets that belong to `offset-to-field`.  A
    fourcc is different because the evidence is inside the value.

    Clustered like `struct_bases`: one `#define` plus N substitutions is one
    edit, so it is one item.
    """
    clusters: dict[str, dict[str, Any]] = {}
    for line_number, line in enumerate(lines, 1):
        mask = masks[line_number - 1]
        for match in _FOURCC.finditer(line):
            if mask[match.start()]:
                continue
            decoded = _fourcc_text(match.group(1))
            if decoded is None:
                continue
            literal = "0x%s" % match.group(1).lower()
            cluster = clusters.setdefault(literal, {
                "line": line_number,
                "decoded": decoded,
                "sites": 0,
            })
            cluster["sites"] += 1
    items = []
    for literal in sorted(clusters):
        cluster = clusters[literal]
        items.append({
            "id": "magic_fourcc:%s" % literal,
            "line": cluster["line"],
            "column": 1,
            "text": "%s spells %r at %d site(s)"
                    % (literal, cluster["decoded"], cluster["sites"]),
            "status": "pending",
            "category": DETECTOR_CATEGORY["magic_fourcc"],
        })
    return items


def _struct_base_items(lines: list[str], masks: list[list[bool]]) -> list[dict[str, Any]]:
    """One `struct-define` item per (base identifier, observed offset set).

    Without this the ladder cannot reach `struct-define` at all: no detector
    proposed it, so its pending count was always 0, the workflow skipped it,
    and every `offset-to-field` item then parked on `no_asserted_struct` -- the
    ladder could not produce the precondition its own next rung requires."""
    clusters: dict[str, dict[str, Any]] = {}
    for line_number, line in enumerate(lines, 1):
        mask = masks[line_number - 1]
        for match in _BASE_OFFSET.finditer(line):
            if mask[match.start()]:
                continue
            cluster = clusters.setdefault(match.group("base"), {
                "line": line_number,
                "offsets": set(),
                "types": set(),
                "sites": 0,
            })
            cluster["offsets"].add(int(match.group("offset"), 16))
            cluster["types"].add(match.group("type").strip())
            cluster["sites"] += 1
    items = []
    for base in sorted(clusters):
        cluster = clusters[base]
        offsets = sorted(cluster["offsets"])
        shown = ", ".join("0x%02x" % offset for offset in offsets[:12])
        if len(offsets) > 12:
            shown += ", ..."
        items.append({
            "id": "struct_bases:%s" % base,
            "line": cluster["line"],
            "column": 1,
            "text": "%s: %d site(s), %d distinct offset(s) [%s] as %s"
                    % (base, cluster["sites"], len(offsets), shown,
                       "/".join(sorted(cluster["types"]))),
            "status": "pending",
            "category": DETECTOR_CATEGORY["struct_bases"],
        })
    return items


def _inventory(source: Path) -> tuple[dict[str, list[dict[str, Any]]], dict[str, int]]:
    try:
        text = source.read_text(encoding="utf-8", errors="replace")
    except OSError as exc:
        raise RecoveryError("unable to read source: %s" % exc)
    # One mask over the WHOLE file, then sliced per line: a block comment
    # spanning several lines is only visible to a whole-file scan.  Split on
    # "\n" rather than splitlines() so the offset arithmetic stays exact on
    # CRLF files (splitlines() also cuts on \f, \v and U+2028).
    mask = _comment_mask(text)
    raw_lines = text.split("\n")
    masks: list[list[bool]] = []
    offset = 0
    for raw in raw_lines:
        masks.append(mask[offset:offset + len(raw)])
        offset += len(raw) + 1
    lines = [raw.rstrip("\r") for raw in raw_lines]
    inventory: dict[str, list[dict[str, Any]]] = {name: [] for name in PATTERNS}
    inventory["struct_bases"] = _struct_base_items(lines, masks)
    inventory["magic_fourcc"] = _magic_fourcc_items(lines, masks)
    skipped: dict[str, int] = {}
    # (detector, ladder, key) -> item, for the grouped detectors only.
    grouped: dict[tuple[str, str, str], dict[str, Any]] = {}
    for line_number, line in enumerate(lines, 1):
        mask = masks[line_number - 1]
        for category, pattern in PATTERNS.items():
            for occurrence, match in enumerate(pattern.finditer(line), 1):
                in_comment = mask[match.start()]
                if in_comment and category not in COMMENT_DEBT_DETECTORS:
                    # Prose is not code debt.  An address or an offset written
                    # inside an explanatory comment cannot be rewritten by ANY
                    # category, and filing it anyway is what produced hundreds
                    # of hand-written parks that all say the same thing.
                    skipped[category] = skipped.get(category, 0) + 1
                    continue
                # A decompiler name surviving in prose IS debt -- removing
                # `FUN_00123456` from the source is the point of the ladder --
                # but the edit is a comment edit, not the rename its detector
                # proposes.
                ladder = ("comments" if in_comment
                          else DETECTOR_CATEGORY.get(category, UNCATEGORIZED))
                site = {
                    "line": line_number,
                    "column": match.start() + 1,
                    "text": line.strip(),
                }
                # A comment hit and a code hit on the same address are different
                # edits in different ladder categories, so `ladder` is part of
                # the grouping key and never folds them together.
                key = GROUP_KEY.get(category, lambda _text: None)(match.group(0))
                if key is not None:
                    existing = grouped.get((category, ladder, key))
                    if existing is not None:
                        existing["occurrences"].append(site)
                        existing["occurrence_count"] = len(existing["occurrences"])
                        continue
                    # The ladder is part of the id, not just the grouping key: a
                    # FUN_ named in BOTH a comment and live code is two items in
                    # two categories, and without the suffix both claimed
                    # `<detector>:<key>` and _validate_manifest rejected the
                    # manifest as having a duplicate item id.
                    item = {
                        "id": "%s:%s%s" % (category, key,
                                           "@comment" if in_comment else ""),
                        "key": key,
                        "status": "pending",
                        "category": ladder,
                        "occurrences": [site],
                        "occurrence_count": 1,
                        **site,
                    }
                    grouped[(category, ladder, key)] = item
                else:
                    item = {
                        "id": "%s:%d:%d" % (category, line_number, occurrence),
                        "status": "pending",
                        "category": ladder,
                        **site,
                    }
                if in_comment:
                    item["in_comment"] = True
                inventory[category].append(item)
    return inventory, skipped


def _items(inventory: dict[str, list[dict[str, Any]]]) -> list[dict[str, Any]]:
    return [item for category in sorted(inventory) for item in inventory[category]]


def _plan(source_arg: str, allow_risky: bool = False) -> dict[str, Any]:
    source = _repo_path(source_arg, ".c")
    inventory, prose_dropped = _inventory(source)
    return {
        "schema": SCHEMA,
        "kind": "source-recovery-manifest",
        "created_tool_version": TOOL_VERSION,
        "repo_relative_source": _relative(source),
        "git_head": _git_head(),
        "source_sha256": _sha256(source),
        "allow_risky": bool(allow_risky),
        "inventory": inventory,
        # Detector hits that landed inside a comment or a string literal and
        # are therefore not code debt.  Reported, not filed: an item nobody can
        # apply is a park waiting to happen.
        "prose_matches_dropped": prose_dropped,
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
        if "category" in item and item["category"] not in CATEGORIES:
            raise RecoveryError("malformed manifest: invalid item category")
        # Grouped items are optional: a manifest planned before decision-target
        # grouping has neither key, and must still load.
        if "occurrences" in item:
            sites = item["occurrences"]
            if not isinstance(sites, list) or not sites:
                raise RecoveryError("malformed manifest: invalid item occurrences")
            for site in sites:
                if not isinstance(site, dict) or not isinstance(site.get("line"), int) or site["line"] < 1:
                    raise RecoveryError("malformed manifest: invalid occurrence line")
            if item.get("occurrence_count") != len(sites):
                raise RecoveryError("malformed manifest: occurrence_count disagrees with occurrences")
            if not isinstance(item.get("key"), str) or not item["key"]:
                raise RecoveryError("malformed manifest: grouped item needs a key")
    inventory_by_id = {item.get("id"): item for item in inventory_items}
    if set(inventory_by_id) != seen:
        raise RecoveryError("malformed manifest: inventory/items ids differ")
    items_by_id = {item["id"]: item for item in items}
    if any(inventory_by_id[item_id].get("status") != items_by_id[item_id].get("status")
           for item_id in seen):
        raise RecoveryError("malformed manifest: inventory/items statuses differ")
    if any(_category_of(inventory_by_id[item_id]) != _category_of(items_by_id[item_id])
           for item_id in seen):
        raise RecoveryError("malformed manifest: inventory/items categories differ")
    if "allow_risky" in manifest and not isinstance(manifest["allow_risky"], bool):
        raise RecoveryError("malformed manifest: allow_risky must be a boolean")
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
           mode: str = "neutral",
           rename_map: dict[str, str] | None = None) -> dict[str, Any]:
    """Verify the candidate object against the captured baseline.

    rename_map excuses symbol-NAME differences in the COFF comparison for the
    listed symbols only -- see coff_candidate_guard.compare_snapshots. Supply it
    only for rename categories, and only from a map already verified against the
    source diff. Any map used is echoed in the result so a relaxed pass is never
    indistinguishable from a strict one.
    """
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
        "warnings": [],
        "observations": {"changes": []},
    }
    # Ladder state is derived from the manifest alone, so record it even if a
    # guard below raises: order violations are advisory, risky work is a failure.
    result["warnings"].extend(_ladder_warnings(manifest))
    result["failures"].extend(_risky_failures(manifest))
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
        result["rename_map_applied"] = sorted(rename_map or {})
        try:
            coff = coff_candidate_guard.compare_snapshots(
                baseline["coff_snapshot"], coff_candidate_guard.capture_object(str(obj)),
                rename_map)
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
    warnings = _ladder_warnings(manifest)
    last_check = "passed" if manifest["checks"] and manifest["checks"][-1].get("ok") else "not passed"
    if manifest["checks"] and not manifest["checks"][-1].get("ok") and skipped and not failures:
        last_check += " (skipped gates)"
    lines = ["# Source Recovery Report", "", "- Source: `%s`" % manifest["repo_relative_source"], "- Plan: `%s` at `%s`" % (manifest["created_tool_version"], manifest["git_head"]),
             "- Debt: %d items (%d pending, %d applied, %d parked)" % (len(items), counts["pending"], counts["applied"], counts["parked"]),
             "- Baseline: %s" % ("captured" if baseline.get("captured") else "not captured"),
             "- Risky categories: %s" % ("opted in (--allow-risky)" if manifest.get("allow_risky") is True else "not opted in"),
             "- Last check: %s" % last_check]
    if failures:
        lines.append("- Failures: %s" % "; ".join(failures))
    else:
        lines.append("- Failures: none recorded")
    if skipped:
        lines.append("- Skipped gates: %s" % "; ".join(skipped))
    else:
        lines.append("- Skipped gates: none recorded")
    if warnings:
        lines.extend("- Warning: %s" % warning for warning in warnings)
    lines.append("")
    lines.append("## Ladder (mandatory order)")
    counts = _ladder_counts(manifest)
    for position, category in enumerate(CATEGORIES):
        bucket = counts.get(category, {"pending": 0, "applied": 0, "parked": 0})
        total = sum(bucket.values())
        step = "--" if category == UNCATEGORIZED else "%2d" % (position + 1)
        risky = " [risky]" if category in RISKY_CATEGORIES else ""
        lines.append("- %s `%s` (%s)%s: %d items (%d pending, %d applied, %d parked)" % (
            step, category, LADDER_SKILLS.get(category, "-"), risky, total,
            bucket["pending"], bucket["applied"], bucket["parked"]))
    lines.append("")
    lines.append("## Debt By Detector")
    for category in sorted(manifest["inventory"]):
        lines.append("- `%s`: %d" % (category, len(manifest["inventory"][category])))
    return "\n".join(lines) + "\n"


def _ladder(manifest: dict[str, Any]) -> tuple[str, int]:
    """Read-only ladder view: counts, order warnings, and the risky opt-in verdict."""
    counts = _ladder_counts(manifest)
    warnings = _ladder_warnings(manifest)
    failures = _risky_failures(manifest)
    lines = ["ladder for %s (risky %s)" % (
        manifest["repo_relative_source"],
        "opted in" if manifest.get("allow_risky") is True else "not opted in")]
    for position, category in enumerate(CATEGORIES):
        bucket = counts.get(category, {"pending": 0, "applied": 0, "parked": 0})
        step = "--" if category == UNCATEGORIZED else "%2d" % (position + 1)
        lines.append("  %s %-16s %-28s pending=%d applied=%d parked=%d%s" % (
            step, category, LADDER_SKILLS.get(category, "-"),
            bucket["pending"], bucket["applied"], bucket["parked"],
            "  [risky]" if category in RISKY_CATEGORIES else ""))
    lines.extend("  WARN  %s" % warning for warning in warnings)
    lines.extend("  FAIL  %s" % failure for failure in failures)
    return "\n".join(lines) + "\n", 1 if failures else 0


def _set_status(path: Path, manifest: dict[str, Any], item_id: str, status: str,
                reason: str | None, category: str | None = None) -> None:
    if status == "parked" and not (reason and reason.strip()):
        raise RecoveryError("parked items require --reason")
    if category is not None and category not in CATEGORIES:
        raise RecoveryError("invalid ladder category: %s" % category)
    item_matches = [item for item in manifest["items"] if item["id"] == item_id]
    inventory_matches = [item for category in manifest["inventory"].values()
                         for item in category if item["id"] == item_id]
    if len(item_matches) != 1 or len(inventory_matches) != 1:
        raise RecoveryError("item not found: %s" % item_id)
    updates = item_matches + inventory_matches
    if status == "applied":
        target = category if category is not None else _category_of(item_matches[0])
        if target in RISKY_CATEGORIES and manifest.get("allow_risky") is not True:
            raise RecoveryError(
                "item %s is in risky category `%s`; re-plan the manifest with --allow-risky "
                "and secure a behavioral oracle before applying it" % (item_id, target))
    for item in updates:
        item["status"] = status
        if category is not None:
            item["category"] = category
        if reason:
            item["reason"] = reason
        elif status != "parked":
            item.pop("reason", None)
    _write_atomic(path, manifest)


def _selftest_comment_routing() -> bool:
    """A FUN_ name in prose routes to `comments`; a prose address is dropped."""
    source = (
        "/* mirrors FUN_00123456() and reads *(int *)0x45b1d0 */\n"
        "int f(void) { return *(int *)0x45b1d0; }\n"
    )
    with tempfile.TemporaryDirectory(dir=ROOT) as directory:
        path = Path(directory) / "sample.c"
        path.write_text(source, encoding="ascii")
        inventory, dropped = _inventory(path)
    comment_items = [item for items in inventory.values() for item in items
                     if item.get("in_comment")]
    live = [item for items in inventory.values() for item in items
            if not item.get("in_comment")]
    return (len(comment_items) == 1
            and comment_items[0]["category"] == "comments"
            and dropped.get("absolute_address_dereferences") == 1
            and [item["category"] for item in live] == ["global-names"])


def _selftest_magic_fourcc() -> bool:
    """A printable 4-byte literal is proposed; a plain magic number is not."""
    source = (
        "int f(void) {\n"
        "  tag_get(0x61637472, 0);\n"
        "  tag_get(0x61637472, 1);\n"
        "  return 0x0045b1d0 + 4096;\n"   # an address and a round number
        "}\n"
    )
    with tempfile.TemporaryDirectory(dir=ROOT) as directory:
        path = Path(directory) / "sample.c"
        path.write_text(source, encoding="ascii")
        inventory, _dropped = _inventory(path)
    items = inventory["magic_fourcc"]
    return (len(items) == 1
            and items[0]["id"] == "magic_fourcc:0x61637472"
            and items[0]["category"] == "const-enum"
            and "'actr'" in items[0]["text"]
            and "2 site(s)" in items[0]["text"])


def _selftest_struct_bases() -> bool:
    """Two offsets off one base make ONE struct-define item, not two."""
    source = (
        "int f(char *p) {\n"
        "  return *(int *)(p + 0x10) + *(int *)(p + 0x14);\n"
        "}\n"
    )
    with tempfile.TemporaryDirectory(dir=ROOT) as directory:
        path = Path(directory) / "sample.c"
        path.write_text(source, encoding="ascii")
        inventory, _dropped = _inventory(path)
    bases = inventory["struct_bases"]
    return (len(bases) == 1 and bases[0]["id"] == "struct_bases:p"
            and bases[0]["category"] == "struct-define"
            and len(inventory["raw_base_offset_dereferences"]) == 2)


def _self_test() -> int:
    from tools.recovery import assert_metadata_guard, coff_candidate_guard

    legacy = {
        "repo_relative_source": "legacy.c",
        "items": [{"id": "a", "line": 1, "status": "pending"},
                  {"id": "b", "line": 2, "status": "applied"}],
    }
    ordered = {
        "repo_relative_source": "ordered.c",
        "items": [{"id": "a", "line": 1, "status": "pending", "category": "comments"},
                  {"id": "b", "line": 2, "status": "applied", "category": "offset-to-field"}],
    }
    risky = {
        "repo_relative_source": "risky.c",
        "items": [{"id": "a", "line": 1, "status": "applied", "category": "control-flow"}],
    }
    checks = [
        (bool(PATTERNS["fun_calls"].search("FUN_00123456();")), "FUN call detection"),
        (not bool(PATTERNS["absolute_address_dereferences"].search("return 42;")), "numeric literal is not address debt"),
        (TOOL_VERSION.startswith("source-recovery/"), "versioned tool"),
        (assert_metadata_guard is not None and coff_candidate_guard is not None,
         "recovery guard imports"),
        (set(DETECTOR_CATEGORY) == set(PATTERNS) | DERIVED_DETECTORS
         and set(DETECTOR_CATEGORY.values()) <= set(CATEGORIES),
         "every detector maps to a ladder category"),
        (_selftest_comment_routing(), "comment-only FUN_ is comments work, "
                                      "prose addresses are not debt"),
        (_selftest_struct_bases(), "struct-define items cluster by base"),
        (_selftest_magic_fourcc(), "const-enum proposes fourcc literals only"),
        (RISKY_CATEGORIES <= set(LADDER) and set(LADDER_SKILLS) == set(CATEGORIES),
         "ladder vocabulary is consistent"),
        (not _ladder_warnings(legacy) and not _risky_failures(legacy),
         "pre-ladder manifest is uncategorized and never gated"),
        (len(_ladder_warnings(ordered)) == 1, "out-of-order applied work warns"),
        (not _ladder_warnings(risky) and len(_risky_failures(risky)) == 1,
         "risky category fails without allow_risky"),
        (not _risky_failures(dict(risky, allow_risky=True)),
         "risky category passes with allow_risky"),
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
    plan.add_argument("--allow-risky", action="store_true",
                      help="opt this manifest in to the risky ladder categories "
                           "(expr-simplify, control-flow); requires a behavioral oracle")
    capture = sub.add_parser("capture")
    capture.add_argument("manifest")
    capture.add_argument("--object", required=True)
    check = sub.add_parser("check")
    check.add_argument("manifest")
    check.add_argument("--object", required=True)
    check.add_argument("--mode", choices=("neutral", "corrective"), default="neutral")
    check.add_argument("--skip-vc71", action="store_true")
    check.add_argument("--rename-map", default=None,
                       help="JSON file or inline JSON {old_name: new_name}. Excuses "
                            "symbol-NAME differences in the COFF comparison for those "
                            "symbols only; code bytes, relocation offsets/types and "
                            "target addresses must still match exactly. Use for rename "
                            "categories, with a map verified against the source diff.")
    report = sub.add_parser("report")
    report.add_argument("manifest")
    ladder = sub.add_parser("ladder", help="read-only ladder ordering view")
    ladder.add_argument("manifest")
    status = sub.add_parser("set-status")
    status.add_argument("manifest")
    status.add_argument("item_id")
    status.add_argument("status", choices=("pending", "applied", "parked"))
    status.add_argument("--reason")
    status.add_argument("--category", choices=CATEGORIES,
                        help="assign the ladder category for this item")
    args = parser.parse_args(argv)
    try:
        if args.self_test:
            return _self_test()
        if args.command == "plan":
            _write_atomic(_output_path(args.output, ".json"),
                          _plan(args.source, args.allow_risky))
            return 0
        path, manifest = _load(args.manifest)
        if args.command == "capture":
            _capture(path, manifest, args.object)
            _write_atomic(path, manifest)
            return 0
        if args.command == "check":
            rename_map = None
            if args.rename_map:
                if Path(args.rename_map).is_file():
                    rename_map = json.loads(Path(args.rename_map).read_text(encoding="ascii"))
                else:
                    rename_map = json.loads(args.rename_map)
            result = _check(path, manifest, args.object, args.skip_vc71, args.mode,
                            rename_map)
            print(json.dumps(result, indent=2, sort_keys=True))
            return 0 if result["ok"] else 1
        if args.command == "report":
            print(_report(manifest), end="")
            return 0
        if args.command == "ladder":
            text, code = _ladder(manifest)
            print(text, end="")
            return code
        if args.command == "set-status":
            _set_status(path, manifest, args.item_id, args.status, args.reason, args.category)
            return 0
        parser.error("a subcommand is required")
    except (OSError, ValueError, RecoveryError) as exc:
        print(json.dumps({"ok": False, "failures": [str(exc)]}, sort_keys=True))
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
