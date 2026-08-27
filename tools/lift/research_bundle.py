#!/usr/bin/env python3
"""Fingerprint-validated mechanical evidence bundles for automated lifts.

The shared store contains immutable binary/tool-backed artifacts.  It never
stores an agent brief, inferred argument meanings, source-presence claims, or
retrieval neighbor bodies.  Source presence and retrieval are evaluated live.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Callable, Optional

REPO_ROOT = Path(__file__).resolve().parents[2]
TOOLS_DIR = REPO_ROOT / "tools"
LIFT_DIR = TOOLS_DIR / "lift"
for _path in (REPO_ROOT, TOOLS_DIR, LIFT_DIR):
    if str(_path) not in sys.path:
        sys.path.insert(0, str(_path))

import llm_auto_lift as auto_lift  # noqa: E402
from park import ledger_root  # noqa: E402

SCHEMA = 1
EXTRACTOR_VERSION = "research-bundle-v1"
GHIDRA_ANALYSIS_VERSION = "cachebeta-2276-analysis-v1"
SCORE_OPTIONS_VERSION = "vc71-msvc71-o2-project-options-v1"
NEIGHBOR_BODY_BUDGET = 4000


def _canonical(value) -> bytes:
    return json.dumps(value, sort_keys=True, separators=(",", ":"),
                      ensure_ascii=True).encode("utf-8")


def _sha_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _fingerprint(value) -> str:
    return _sha_bytes(_canonical(value))


def _file_sha(path: Path) -> str:
    try:
        return _sha_bytes(path.read_bytes())
    except OSError:
        return "missing"


def _md5(path: Path) -> str:
    try:
        return hashlib.md5(path.read_bytes(), usedforsecurity=False).hexdigest()
    except (OSError, TypeError):
        try:
            return hashlib.md5(path.read_bytes()).hexdigest()
        except OSError:
            return "missing"


def _norm_addr(value: str) -> str:
    try:
        return f"0x{int(str(value), 16):x}"
    except (TypeError, ValueError):
        return ""


def research_cache_root() -> Path:
    override = os.environ.get("HALO_RESEARCH_CACHE_DIR")
    return Path(override) if override else ledger_root().parent / "research_cache"


def context_fingerprint(payload: dict) -> str:
    """Stable context fingerprint over explicitly supplied mechanical inputs."""
    return _fingerprint({"kind": "context", "schema": SCHEMA, **payload})


def score_fingerprint(payload: dict) -> str:
    """Stable score-attempt fingerprint over explicitly supplied inputs."""
    return _fingerprint({"kind": "score", "schema": SCHEMA, **payload})


def retrieval_cohort(addr: str) -> str:
    """Deterministic address assignment for the retrieval A/B experiment."""
    return "retrieval" if int(addr, 16) % 2 == 0 else "control"


@dataclass(frozen=True)
class ResolvedTarget:
    addr: str
    name: str
    object_name: str
    source_path: str
    decl: str
    kb_entry: dict
    has_reg_args: bool

    def as_lift_target(self):
        return auto_lift.LiftTarget(
            addr=self.addr, name=self.name, decl=self.decl,
            object_name=self.object_name, source_path=self.source_path,
            has_reg_args=self.has_reg_args,
            register_args=auto_lift._parse_register_args(self.decl),
        )


def _resolve_target(query: str, kb: dict) -> ResolvedTarget:
    wanted_addr = _norm_addr(query)
    for obj in kb.get("objects", []):
        source = obj.get("source") or ""
        source_path = source if source.startswith("src/") else f"src/halo/{source}"
        for entry in obj.get("functions", []):
            addr = _norm_addr(entry.get("addr", ""))
            decl = entry.get("decl", "")
            name = auto_lift._parse_name_from_decl(decl)
            if (wanted_addr and addr == wanted_addr) or query == name:
                return ResolvedTarget(
                    addr=addr, name=name, object_name=obj.get("name", ""),
                    source_path=source_path, decl=decl, kb_entry=entry,
                    has_reg_args=bool(auto_lift._parse_register_args(decl)),
                )
    raise KeyError(f"target not found in kb.json: {query}")


def _kb_decl_index(kb: dict) -> dict[str, str]:
    out = {}
    for obj in kb.get("objects", []):
        for entry in obj.get("functions", []):
            decl = entry.get("decl", "")
            name = auto_lift._parse_name_from_decl(decl)
            if name:
                out[name] = decl
            addr = _norm_addr(entry.get("addr", ""))
            if addr:
                out[addr] = decl
    return out


def _bounds_entry(root: Path, addr: str) -> dict:
    path = root / "tools" / "verify" / "function_bounds.json"
    try:
        table = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {}
    normalized = _norm_addr(addr)
    for key, value in table.items():
        if _norm_addr(key) == normalized and isinstance(value, dict):
            return value
    return {}


def _function_bytes(addr: str, bounds: dict) -> bytes:
    try:
        start = int(addr, 16)
        end = int(bounds["end"], 16)
    except (KeyError, TypeError, ValueError):
        return b""
    return auto_lift._read_va(start, max(0, end - start)) or b""


def _context_payload(root: Path, target: ResolvedTarget, kb: dict,
                     callee_names: list[str]) -> dict:
    bounds = _bounds_entry(root, target.addr)
    body = _function_bytes(target.addr, bounds)
    decls = _kb_decl_index(kb)
    callee_decls = sorted((name, decls.get(name, "missing"))
                          for name in sorted(set(callee_names)))
    return {
        "xbe_md5": _md5(root / "halo-patched" / "cachebeta.xbe"),
        "target": {
            "addr": target.addr,
            "bounds": bounds,
            "function_bytes_sha256": _sha_bytes(body),
        },
        "kb_entry": target.kb_entry,
        "callee_declarations_sha256": _fingerprint(callee_decls),
        "extractor_version": EXTRACTOR_VERSION,
        "ghidra_analysis_version": GHIDRA_ANALYSIS_VERSION,
    }


def _score_payload(root: Path, target: ResolvedTarget, source_sha: str) -> dict:
    bounds = _bounds_entry(root, target.addr)
    body = _function_bytes(target.addr, bounds)
    verifier_files = [
        root / "tools" / "verify" / "vc71_verify.py",
        root / "tools" / "verify" / "xbe_reference.py",
    ]
    return {
        "target_addr": target.addr,
        "candidate_source_sha256": source_sha,
        "kb_declaration_sha256": _sha_bytes(target.decl.encode("utf-8")),
        "compiler_options": SCORE_OPTIONS_VERSION,
        "verifier_version_sha256": _fingerprint([_file_sha(p) for p in verifier_files]),
        "reference_sha256": _fingerprint({
            "bytes": _sha_bytes(body), "bounds": bounds,
        }),
    }


def _source_sha(root: Path, target: ResolvedTarget) -> str:
    return _file_sha(root / target.source_path)


def _artifact_id(data) -> str:
    return "sha256:" + _fingerprint(data)


class ResearchCache:
    def __init__(self, root: Path, repo_root: Path = REPO_ROOT):
        self.root = root
        self.repo_root = repo_root

    def _pointer_path(self, target: ResolvedTarget) -> Path:
        return self.root / "targets" / f"{target.addr[2:]}.json"

    def _bundle_path(self, target: ResolvedTarget, fingerprint: str) -> Path:
        return self.root / "bundles" / target.addr[2:] / f"{fingerprint}.json"

    def _artifact_path(self, kind: str, artifact_id: str) -> Path:
        return self.root / "objects" / kind / f"{artifact_id.split(':', 1)[1]}.json"

    @staticmethod
    def _load(path: Path) -> Optional[dict]:
        try:
            value = json.loads(path.read_text(encoding="utf-8"))
            return value if isinstance(value, dict) else None
        except (OSError, json.JSONDecodeError):
            return None

    @staticmethod
    def _write_immutable(path: Path, value: dict) -> None:
        encoded = json.dumps(value, indent=2, sort_keys=True) + "\n"
        if path.exists():
            if path.read_text(encoding="utf-8") != encoded:
                raise RuntimeError(f"immutable artifact collision: {path}")
            return
        path.parent.mkdir(parents=True, exist_ok=True)
        tmp = path.with_name(f".{path.name}.{os.getpid()}.tmp")
        tmp.write_text(encoded, encoding="utf-8")
        try:
            os.link(tmp, path)
        except FileExistsError:
            if path.read_text(encoding="utf-8") != encoded:
                raise RuntimeError(f"immutable artifact collision: {path}")
        finally:
            tmp.unlink(missing_ok=True)

    @staticmethod
    def _write_pointer(path: Path, value: dict) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        tmp = path.with_name(f".{path.name}.{os.getpid()}.tmp")
        tmp.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n",
                       encoding="utf-8")
        os.replace(tmp, path)

    def _matching_score(self, target: ResolvedTarget) -> tuple[str, Optional[Path]]:
        source_sha = _source_sha(self.repo_root, target)
        attempt_fp = score_fingerprint(_score_payload(self.repo_root, target, source_sha))
        path = self.root / "scores" / target.addr[2:] / f"{attempt_fp}.json"
        return attempt_fp, path if path.exists() else None

    def publish_local_score(self, target: ResolvedTarget) -> tuple[str, Optional[Path]]:
        """Publish a current-attempt score pack; legacy packs are misses."""
        source_sha = _source_sha(self.repo_root, target)
        candidates = [target.name]
        try:
            candidates.append(f"FUN_{int(target.addr, 16):08x}")
        except ValueError:
            pass
        pack = None
        for name in candidates:
            candidate = self.repo_root / "artifacts" / "score_context" / f"{name}.json"
            value = self._load(candidate)
            if value and value.get("candidate_source_sha256") == source_sha:
                pack = value
                break
        attempt_fp = score_fingerprint(_score_payload(self.repo_root, target, source_sha))
        if pack is None:
            return attempt_fp, None
        out = self.root / "scores" / target.addr[2:] / f"{attempt_fp}.json"
        self._write_immutable(out, pack)
        return attempt_fp, out

    def _record_cache_event(self, hit: bool, ghidra_builds: int) -> None:
        event = {
            "hit": bool(hit), "ghidra_builds": int(ghidra_builds),
            "ts": datetime.now(timezone.utc).isoformat(timespec="microseconds"),
            "pid": os.getpid(),
        }
        path = self.root / "events" / "cache" / f"{_fingerprint(event)}.json"
        self._write_immutable(path, event)

    def inspect_target(self, target: ResolvedTarget, kb: dict) -> dict:
        pointer = self._load(self._pointer_path(target)) or {}
        callees = pointer.get("callee_names", []) if pointer.get("schema") == SCHEMA else []
        fingerprint = context_fingerprint(
            _context_payload(self.repo_root, target, kb, callees))
        bundle_path = self._bundle_path(target, fingerprint)
        bundle = self._load(bundle_path)
        ghidra_id = ((bundle or {}).get("artifacts") or {}).get("ghidra", "")
        artifact_path = self._artifact_path("ghidra", ghidra_id) if ghidra_id else None
        hit = bool(bundle and artifact_path and artifact_path.exists())
        attempt_fp, score_path = self._matching_score(target)
        return {
            "target": target.addr, "fingerprint": fingerprint,
            "context_cache": "hit" if hit else ("stale" if pointer else "miss"),
            "bundle_path": str(bundle_path) if hit else "",
            "ghidra_path": str(artifact_path) if hit else "",
            "attempt_fingerprint": attempt_fp,
            "score_context_path": str(score_path) if score_path else "",
        }

    def prepare_target(self, target: ResolvedTarget, kb: dict, *,
                       force: bool = False, allow_reg_args: bool = False,
                       current_attempt: bool = False,
                       build_ghidra: Optional[Callable[[ResolvedTarget], dict]] = None,
                       source_checker: Optional[Callable[[str, str], Optional[str]]] = None,
                       retrieval_loader: Optional[Callable[[str, str], list[dict]]] = None) -> dict:
        checker = source_checker or auto_lift._is_already_in_source
        source_hit = checker(target.addr, target.name)
        cohort = retrieval_cohort(target.addr)
        if source_hit and not current_attempt:
            return {
                "schema": SCHEMA, "addr": target.addr, "name": target.name,
                "obj": target.object_name, "source_path": target.source_path,
                "fingerprint": "", "attempt_fingerprint": "",
                "artifacts": {"ghidra": "", "score_context": ""},
                "artifact_paths": {"ghidra": "", "score_context": ""},
                "verdicts": [{
                    "rule_id": "live_source_presence", "verdict": "skip",
                    "confidence": "high", "evidence": [],
                }],
                "pre_screen": "skip_already_in_source",
                "skip_reason": f"already implemented: {source_hit}",
                "cache": {"context": "not_needed", "ghidra_builds": 0},
                "retrieval": {"cohort": cohort, "neighbor_ids": []},
            }

        pointer = self._load(self._pointer_path(target)) or {}
        callee_names = pointer.get("callee_names", []) if pointer.get("schema") == SCHEMA else []
        fingerprint = context_fingerprint(
            _context_payload(self.repo_root, target, kb, callee_names))
        bundle_path = self._bundle_path(target, fingerprint)
        bundle = self._load(bundle_path)
        ghidra_id = ((bundle or {}).get("artifacts") or {}).get("ghidra", "")
        ghidra_path = self._artifact_path("ghidra", ghidra_id) if ghidra_id else None
        hit = bool(not force and bundle and ghidra_path and ghidra_path.exists())
        ghidra_builds = 0

        if hit:
            ghidra = self._load(ghidra_path) or {}
        else:
            if build_ghidra is None:
                build_ghidra = _build_ghidra_context
            ghidra = build_ghidra(target)
            ghidra_builds = 1
            callee_names = sorted(set(ghidra.get("callees", [])))
            fingerprint = context_fingerprint(
                _context_payload(self.repo_root, target, kb, callee_names))
            bundle_path = self._bundle_path(target, fingerprint)
            ghidra_id = _artifact_id(ghidra)
            ghidra_path = self._artifact_path("ghidra", ghidra_id)
            self._write_immutable(ghidra_path, ghidra)

        attempt_fp, score_path = self.publish_local_score(target)
        score_id = ""
        if score_path:
            score_pack = self._load(score_path) or {}
            score_id = _artifact_id(score_pack)

        verdicts = _mechanical_verdicts(target, ghidra, score_path,
                                         allow_reg_args=allow_reg_args)
        context_verdicts = _mechanical_verdicts(
            target, ghidra, None, allow_reg_args=False)
        persisted = {
            "schema": SCHEMA,
            "target": {"addr": target.addr, "name": target.name,
                       "object": target.object_name},
            "fingerprint": fingerprint,
            # Context bundles are immutable. Score contexts are separate
            # attempt-fingerprinted artifacts and are joined only in the live
            # prepare/inspect view below.
            "artifacts": {"ghidra": ghidra_id, "score_context": ""},
            "verdicts": context_verdicts,
        }
        self._write_immutable(bundle_path, persisted)
        self._write_pointer(self._pointer_path(target), {
            "schema": SCHEMA, "fingerprint": fingerprint,
            "bundle": str(bundle_path), "callee_names": callee_names,
        })
        self._record_cache_event(hit, ghidra_builds)

        retrieval = {"cohort": cohort, "neighbor_ids": []}
        if cohort == "retrieval" and ghidra.get("decompile_c"):
            loader = retrieval_loader or _load_retrieval_neighbors
            neighbors = loader(ghidra["decompile_c"], target.object_name)
            retrieval["neighbor_ids"] = [n.get("addr", "") for n in neighbors]
            if neighbors:
                first = neighbors[0]
                retrieval["neighbor"] = {
                    "addr": first.get("addr", ""), "name": first.get("name", ""),
                    "decl": first.get("decl", ""),
                    "c_source": str(first.get("c_source") or "")[:NEIGHBOR_BODY_BUDGET],
                }

        skip = next((v for v in verdicts if v["verdict"] == "skip"), None)
        return {
            "schema": SCHEMA, "addr": target.addr, "name": target.name,
            "obj": target.object_name, "source_path": target.source_path,
            "fingerprint": fingerprint, "attempt_fingerprint": attempt_fp,
            "artifacts": {"ghidra": ghidra_id, "score_context": score_id},
            "artifact_paths": {
                "ghidra": str(ghidra_path),
                "score_context": str(score_path) if score_path else "",
            },
            "verdicts": verdicts,
            "pre_screen": "skip_mechanical" if skip else "ok",
            "skip_reason": skip["rule_id"] if skip else "",
            "cache": {"context": "hit" if hit else "miss",
                      "ghidra_builds": ghidra_builds},
            "retrieval": retrieval,
        }

    def record_outcome(self, *, target: str, cohort: str, outcome: str,
                       tokens: int, fingerprint: str = "") -> Path:
        record = {
            "schema": SCHEMA, "target": _norm_addr(target), "cohort": cohort,
            "outcome": outcome, "tokens": max(0, int(tokens)),
            "fingerprint": fingerprint,
            "ts": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        }
        event_id = _fingerprint(record)
        path = self.root / "outcomes" / f"{event_id}.json"
        self._write_immutable(path, record)
        return path

    def stats(self) -> dict:
        cache = {"hits": 0, "misses": 0, "ghidra_builds": 0}
        cache_events = self.root / "events" / "cache"
        for path in cache_events.glob("*.json") if cache_events.exists() else []:
            event = self._load(path) or {}
            cache["hits" if event.get("hit") else "misses"] += 1
            cache["ghidra_builds"] += int(event.get("ghidra_builds") or 0)
        cohorts = {}
        for path in (self.root / "outcomes").glob("*.json") if (self.root / "outcomes").exists() else []:
            rec = self._load(path) or {}
            cohort = rec.get("cohort", "unknown")
            row = cohorts.setdefault(cohort, {
                "targets": 0, "tokens": 0, "accepted": 0,
                "reviewer_rejections": 0, "runtime_failures": 0,
            })
            row["targets"] += 1
            row["tokens"] += int(rec.get("tokens") or 0)
            outcome = rec.get("outcome")
            row["accepted"] += int(outcome in {"committed", "would_commit"})
            row["reviewer_rejections"] += int(outcome == "reviewer_rejected")
            row["runtime_failures"] += int(outcome == "runtime_failed")
        for row in cohorts.values():
            row["accepted_per_100k_tokens"] = (
                100000.0 * row["accepted"] / row["tokens"] if row["tokens"] else 0.0)
        eligible = all(cohorts.get(c, {}).get("targets", 0) >= 50
                       for c in ("retrieval", "control"))
        retrieval = cohorts.get("retrieval", {})
        control = cohorts.get("control", {})
        gate_passes = bool(
            eligible
            and retrieval.get("accepted_per_100k_tokens", 0)
                > control.get("accepted_per_100k_tokens", 0)
            and retrieval.get("reviewer_rejections", 0)
                <= control.get("reviewer_rejections", 0)
            and retrieval.get("runtime_failures", 0)
                <= control.get("runtime_failures", 0)
        )
        return {"schema": SCHEMA, "cache": cache, "cohorts": cohorts,
                "retrieval_decision_eligible": eligible,
                "recommend_unconditional_retrieval": gate_passes}


def _build_ghidra_context(target: ResolvedTarget) -> dict:
    check = subprocess.run(
        [sys.executable, str(REPO_ROOT / "tools" / "audit" / "check_ghidra_mcp.py")],
        cwd=REPO_ROOT, capture_output=True, text=True,
    )
    if check.returncode != 0:
        raise RuntimeError("Ghidra MCP preflight failed: " + check.stderr.strip())
    lift_target = target.as_lift_target()
    builder = auto_lift.ContextPackBuilder(ghidra_live=True)
    ghidra = builder._fetch_ghidra_context(lift_target)
    ghidra = builder._enrich_ghidra_context(lift_target, ghidra)
    try:
        response = auto_lift.GhidraMCPClient().call_tool(
            "get_xrefs_to", {"address": target.addr})
        ghidra["xrefs_to"] = response.get("content", [{}])[0].get("text", "")
    except Exception:
        ghidra["xrefs_to"] = ""
    return ghidra


def _mechanical_verdicts(target: ResolvedTarget, ghidra: dict,
                         score_path: Optional[Path], *, allow_reg_args: bool) -> list[dict]:
    verdicts = []

    def add(rule_id: str, verdict: str, confidence: str, evidence: list[str]):
        verdicts.append({"rule_id": rule_id, "verdict": verdict,
                         "confidence": confidence, "evidence": evidence})

    ghidra_evidence = [_artifact_id(ghidra)]
    combined = (ghidra.get("decompile_c", "") + "\n" +
                ghidra.get("disassembly", ""))
    if target.has_reg_args and not allow_reg_args:
        add("register_arguments_disabled", "skip", "high", [])
    if re.search(r"__SEH_(?:prolog|epilog)|0x1dd5c8|0x1dd601", combined, re.I):
        add("seh_wrapper", "skip", "high", ghidra_evidence)
    if re.search(r"\b(?:Nt|Ob|Ke|Rtl|Ex|Ps|Io|Hal)[A-Z]\w+", combined):
        add("xbox_kernel_import", "skip", "high", ghidra_evidence)
    if ghidra.get("xrefs_to"):
        from tools.analysis.classify_liftability import (  # pylint: disable=import-outside-toplevel
            _xref_lines, is_fragment,
        )
        if is_fragment(_xref_lines(ghidra["xrefs_to"])):
            add("switch_case_fragment", "skip", "high", ghidra_evidence)

    if score_path:
        from tools.analysis.classify_cap import (  # pylint: disable=import-outside-toplevel
            float_arg_lowering_verdict, fucompp_assert_verdict,
        )
        cap = (float_arg_lowering_verdict(str(score_path)) or
               fucompp_assert_verdict(str(score_path)))
        if cap:
            reason = cap.get("cap_reason", "mechanical_cap")
            add(reason.split(":", 1)[0], "skip", "high",
                [_artifact_id(ResearchCache._load(score_path) or {})])
    if not verdicts:
        add("mechanical_context_complete", "continue", "high", ghidra_evidence)
    return verdicts


def _load_retrieval_neighbors(decompile: str, obj_name: str) -> list[dict]:
    try:
        from tools.retrieval.query import query_neighbors  # pylint: disable=import-outside-toplevel
        found = query_neighbors(decompile, top_k=1, min_similarity=0.35,
                                prefer_obj_name=obj_name)
        return [{"addr": n.addr, "name": n.name, "decl": n.decl,
                 "c_source": n.c_source} for n in found[:1]]
    except Exception:
        return []


def _json_print(value: dict, as_json: bool) -> None:
    if as_json:
        print(json.dumps(value, sort_keys=True))
    else:
        print(json.dumps(value, indent=2, sort_keys=True))


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    for name in ("prepare", "inspect"):
        cmd = sub.add_parser(name)
        cmd.add_argument("--target", required=True)
        cmd.add_argument("--json", action="store_true")
        if name == "prepare":
            cmd.add_argument("--force", action="store_true",
                             help="Explicitly rebuild after fingerprint validation")
            cmd.add_argument("--allow-reg-args", action="store_true")
            cmd.add_argument("--current-attempt", action="store_true",
                             help="Publish score evidence for the active lift")
    stats = sub.add_parser("stats")
    stats.add_argument("--json", action="store_true")
    outcome = sub.add_parser("record-outcome")
    outcome.add_argument("--target", required=True)
    outcome.add_argument("--cohort", choices=("retrieval", "control"), required=True)
    outcome.add_argument("--outcome", required=True)
    outcome.add_argument("--tokens", type=int, default=0)
    outcome.add_argument("--fingerprint", default="")
    outcome.add_argument("--json", action="store_true")
    return parser


def main(argv=None) -> int:
    args = build_parser().parse_args(argv)
    cache = ResearchCache(research_cache_root())
    if args.command == "stats":
        _json_print(cache.stats(), args.json)
        return 0
    if args.command == "record-outcome":
        path = cache.record_outcome(target=args.target, cohort=args.cohort,
                                    outcome=args.outcome, tokens=args.tokens,
                                    fingerprint=args.fingerprint)
        _json_print({"recorded": str(path)}, args.json)
        return 0

    kb = json.loads((REPO_ROOT / "kb.json").read_text(encoding="utf-8"))
    try:
        target = _resolve_target(args.target, kb)
    except KeyError as exc:
        print(str(exc), file=sys.stderr)
        return 2
    if args.command == "inspect":
        _json_print(cache.inspect_target(target, kb), args.json)
    else:
        result = cache.prepare_target(
            target, kb, force=args.force, allow_reg_args=args.allow_reg_args,
            current_attempt=args.current_attempt)
        _json_print(result, args.json)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
