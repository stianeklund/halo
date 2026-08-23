"""Write version-1 .halocase.json sidecars for the viewer.

HMRC v1 remains untouched.  Artifact paths are relative to the manifest and
existing files receive SHA-256 hashes so the viewer can warn about moved or
stale recordings.
"""

import hashlib
import json
import os
from pathlib import Path

CASE_SCHEMA_VERSION = 1


def _sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as fh:
        for block in iter(lambda: fh.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _artifact(case_path, path, build, verification_status):
    path = Path(path) if path is not None else None
    if path is None:
        return None
    base = Path(case_path).parent.resolve()
    resolved = path.resolve()
    item = {
        "path": os.path.relpath(str(resolved), str(base)),
        "verification_status": verification_status,
    }
    if build:
        item["xbe_build"] = str(build)
    if resolved.is_file():
        item["hash"] = _sha256(resolved)
    return item


def write_case(path, *, level, scenario, profile, backend, ticks, quantum,
               alignment_window, minimum_sustained_run, faithful, candidate,
               behavior_report, faithful_build, candidate_build,
               candidate_verification, verdict, coverage, metrics=None,
               parent_case=None, diagnostics=(), input_hash=None, tick_start=0,
               gameplay_anchors=None):
    """Write a viewer-compatible case manifest and return its Path."""
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    data = {
        "schema_version": CASE_SCHEMA_VERSION,
        "fixture": {
            "level": level,
            "scenario": scenario,
            "input_hash": input_hash,
        },
        "capture": {
            "profile": profile,
            "backend": backend,
            "tick_start": tick_start,
            "tick_end": ticks,
            "quantum": quantum,
            "alignment_window": alignment_window,
            "minimum_sustained_run": minimum_sustained_run,
            "gameplay_anchors": gameplay_anchors or {},
        },
        "faithful": _artifact(path, faithful, faithful_build, "verified"),
        "candidate": _artifact(path, candidate, candidate_build,
                                candidate_verification),
        "behavior_report": _artifact(path, behavior_report, None,
                                      "generated" if behavior_report and
                                      Path(behavior_report).is_file() else
                                      "missing"),
        "verdict": verdict,
        "coverage": coverage,
        "metrics": metrics or {"profile": profile},
    }
    if parent_case:
        data["parent_case"] = os.path.relpath(
            str(Path(parent_case).resolve()), str(path.parent.resolve()))
    if diagnostics:
        data["diagnostics"] = list(diagnostics)
    path.write_text(json.dumps(data, indent=2) + "\n")
    return path
