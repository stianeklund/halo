#!/usr/bin/env python3
"""Fresh base-versus-candidate raw-XBE byte regression gate.

Uses the aligned byte lower bound, not mnemonic scores. Both revisions are
compiled in this invocation with identical evaluator code and compiler assets.
Historical dashboard records and mnemonic floors are never comparison inputs.
"""

import argparse
import concurrent.futures
import gzip
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
import uuid
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
EXPECTED_MD5 = "c7869590a1c64ad034e49a5ee0c02465"
EVALUATOR_FILES = (
    "tools/verify/byte_regression.py", "tools/verify/raw_xbe_structural.py",
    "tools/verify/raw_byte_audit.py", "tools/verify/vc71_verify.py",
    "tools/verify/xbe_reference.py", "tools/audit/check_delinked_bounds.py",
    "tools/equivalence/xbe_image.py",
)
# One worker took ~16 min per side; eight took ~1 min on the same inputs with
# identical counts. HALO_BYTE_GATE_WORKERS overrides when the box is busy.
DEFAULT_WORKERS = int(os.environ.get("HALO_BYTE_GATE_WORKERS") or
                      min(8, max(1, (os.cpu_count() or 2) // 2)))
INPUT_PATHS = ("src", "kb.json", "third_party/xbox", "toolchains", "CMakeLists.txt",
               "tools/analysis", "tools/verify/function_bounds.json",
               "tools/verify/vc71_scores.json", "tools/verify/vc71_current.json")


def git(root, *args):
    return subprocess.check_output(["git", "-C", str(root), *args], text=True).strip()


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(".tmp")
    temporary.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n")
    os.replace(temporary, path)


def input_manifest(root):
    """Hash all tracked compile/discovery inputs, plus the actual generated header."""
    paths = git(root, "ls-files", "--cached", "--others", "--exclude-standard", "--", *INPUT_PATHS).splitlines()
    paths += list(EVALUATOR_FILES) + ["build/generated/decl.h"]
    return {name: digest(root / name) for name in sorted(set(paths))}


def stageable_inputs(root):
    """INPUT_PATHS minus gitignored paths: `git add` rejects an ignored pathspec it
    is given by name, and vc71_current.json is a local, ignored measurement file."""
    return [path for path in INPUT_PATHS
            if subprocess.run(["git", "-C", str(root), "check-ignore", "-q", "--", path]).returncode != 0]


def environment_manifest(raw):
    compiler = Path(raw.vc71.VC71_CL if os.name == "nt" else raw.vc71.VC71_CL_WSL)
    include = Path(raw.vc71.RXDK_INC)
    if os.name != "nt":
        include = Path("/mnt/" + str(include)[0].lower() + str(include)[2:].replace("\\", "/"))
    if not compiler.is_file() or not include.is_dir():
        raise ValueError("VC71 compiler or RXDK headers are unavailable")
    files = {}
    for label, directory in (("compiler", compiler.parent), ("include", include)):
        for path in sorted(directory.rglob("*")):
            if path.is_file():
                files[label + "/" + path.relative_to(directory).as_posix()] = digest(path)
    import capstone
    files["capstone_version"] = capstone.__version__
    files["python_version"] = sys.version
    return files


# Byte cache. A TU's records are a pure function of the inputs keyed below, so a
# revision measured before (usually the base: it was the previous candidate) is
# not recompiled. Any source, header, kb, bounds, evaluator, compiler or XBE
# change is a miss. `check` without --cache-dir (CI) always measures fresh.
CACHE_TOOL_FILES = EVALUATOR_FILES + ("tools/verify/function_bounds.py", "tools/verify/compare_obj.py")
CACHE_MAX_AGE = 3 * 24 * 3600


def cache_common_key(root, inputs, environment, xbe_sha256):
    """Digest of what every TU shares: inputs other than TU sources, tools, compiler, XBE."""
    payload = {
        "schema": 1,
        "inputs": {name: value for name, value in inputs.items()
                   if not (name.startswith("src/") and name.endswith(".c"))},
        "tools": {name: digest(root / name) for name in CACHE_TOOL_FILES},
        "environment": environment, "xbe_sha256": xbe_sha256,
        # __FILE__ embeds the checkout path in the object; only reuse at equal length.
        "root_length": len(str(root)),
    }
    return hashlib.sha256(json.dumps(payload, sort_keys=True).encode()).hexdigest()


def tu_cache_key(root, common, source, functions, inputs):
    relative = source.relative_to(root).as_posix()
    if relative not in inputs:
        return None
    payload = [common, relative, inputs[relative],
               sorted([item["function"], item["address"]] for item in functions)]
    return hashlib.sha256(json.dumps(payload).encode()).hexdigest()


def load_cached_tu(cache_dir, key, functions):
    path = cache_dir / (key + ".json.gz")
    try:
        records = json.loads(gzip.decompress(path.read_bytes()))
        addresses = sorted(int(record["address"], 0) for record in records)
        os.utime(path)  # keep entries in use from being pruned
    except (OSError, EOFError, ValueError, KeyError, TypeError):
        return None
    if addresses != sorted(item["address"] for item in functions):
        return None
    return records


def store_cached_tu(cache_dir, key, records):
    cache_dir.mkdir(parents=True, exist_ok=True)
    path = cache_dir / (key + ".json.gz")
    temporary = path.with_name("%s.%d.tmp" % (path.name, os.getpid()))
    temporary.write_bytes(gzip.compress(json.dumps(records).encode(), 1))
    os.replace(temporary, path)


def prune_cache(cache_dir):
    cutoff = time.time() - CACHE_MAX_AGE
    for path in cache_dir.glob("*.gz*"):
        try:
            if path.stat().st_mtime < cutoff:
                path.unlink()
        except OSError:
            pass


# Each local gate run leaves a `local-XXXXXXXX` output dir (~230 MB of
# measurements) that nothing reads again; keep only the newest few for
# inspection. A recent mtime guard protects a run another gate is still writing.
RUN_KEEP = 3
RUN_MIN_AGE = 3600
RUN_DIR = re.compile(r"local-[a-z0-9_]{8}")


def prune_runs(parent, keep=RUN_KEEP, min_age=RUN_MIN_AGE):
    """Remove old per-run output dirs; never the reusable checkouts or the cache."""
    runs = []
    for path in parent.iterdir():
        # `local-reusable` has the same length as mkdtemp's names: exclude by name.
        if path.name == REUSABLE_TREES or not RUN_DIR.fullmatch(path.name):
            continue
        # Real directories only, and never a checkout (a worktree has a .git).
        if path.is_symlink() or not path.is_dir() or os.path.lexists(path / ".git"):
            continue
        try:
            runs.append((path.stat().st_mtime, path))
        except OSError:
            pass
    cutoff = time.time() - min_age
    for mtime, path in sorted(runs, reverse=True)[keep:]:
        if mtime < cutoff:
            shutil.rmtree(path, ignore_errors=True)


def ensure_revision(root, commit):
    if git(root, "rev-parse", "HEAD") != commit:
        raise ValueError("checkout does not match requested commit: " + commit)
    changed = set(git(root, "diff", "--name-only", commit, "--", *INPUT_PATHS).splitlines())
    if changed:
        raise ValueError("checkout inputs differ from commit: " + ", ".join(sorted(changed)))
    # A header absent from Git could still be picked up by /I or relative includes.
    extra = git(root, "ls-files", "--others", "--exclude-standard", "--", *INPUT_PATHS)
    if extra:
        raise ValueError("untracked compile inputs: " + extra)


def measure(args):
    import raw_xbe_structural as raw
    if args.allow_dirty:
        if git(ROOT, "rev-parse", "HEAD") != args.commit:
            raise ValueError("sample commit does not match checkout HEAD")
    else:
        ensure_revision(ROOT, args.commit)
    xbe = ROOT / "halo-patched/cachebeta.xbe"
    if hashlib.md5(xbe.read_bytes()).hexdigest() != EXPECTED_MD5:
        raise ValueError("reference is not pristine debug 2276")
    bounds = json.loads((ROOT / "tools/verify/function_bounds.json").read_text())
    if bounds.get("_meta", {}).get("xbe_md5") != EXPECTED_MD5:
        raise ValueError("bounds table does not identify pristine debug 2276")
    if not raw.vc71.regen_decl_header(quiet=True):
        raise ValueError("declaration generation failed")
    before = input_manifest(ROOT)
    environment = environment_manifest(raw)
    plan = json.loads(args.plan.read_text())
    items = raw._eligible_functions(plan["sources"])
    if not items and not args.allow_empty:
        raise ValueError("scope contains no ported functions")
    groups = {}
    records = []
    for item in items:
        if item["source"] is None:
            raise ValueError("missing source for 0x%x: %s" %
                             (item["address"], item.get("selection_reason")))
        groups.setdefault(item["source"], []).append(item)
    # Once per source, not per function: resolve() lstats every path component
    # on the 9p mount, and 6600 of them took over a minute per side.
    if not all(source.resolve().is_relative_to(ROOT) for source in groups):
        raise ValueError("source lies outside checkout")
    keys = {}
    if args.cache_dir:
        common = cache_common_key(ROOT, before, environment, digest(xbe))
        for source, functions in groups.items():
            key = tu_cache_key(ROOT, common, source, functions, before)
            cached = load_cached_tu(args.cache_dir, key, functions) if key else None
            if cached is None:
                keys[source] = key
            else:
                records.extend(cached)
        print("Reused %d/%d TUs from the byte cache" % (len(groups) - len(keys), len(groups)),
              flush=True)
    else:
        keys = dict.fromkeys(groups)
    objects = args.output.parent / (args.output.stem + "-objects")
    objects.mkdir(parents=True, exist_ok=False)  # never consume old candidate objects
    # TU decoding retains large compiler/disassembly caches. Recycle workers so
    # a whole-repository sweep does not accumulate every TU's caches in each
    # process. Each worker peaks well under 1 GB, so the default stays at 8.
    with concurrent.futures.ProcessPoolExecutor(max_workers=args.workers,
                                               max_tasks_per_child=1) as pool:
        futures = {pool.submit(raw._audit_tu, source, groups[source], objects, raw._decl_hash()): source
                   for source in sorted(keys)}
        for index, future in enumerate(concurrent.futures.as_completed(futures), 1):
            measured = []
            for item, record in future.result():
                record["source"]["path"] = item["source"].relative_to(ROOT).as_posix()
                if record.get("reason", "").startswith("VC71 compilation failed"):
                    raise ValueError(record["reason"] + ": " + record["source"]["path"])
                measured.append(record)
            records.extend(measured)
            if keys[futures[future]]:
                store_cached_tu(args.cache_dir, keys[futures[future]], measured)
            print("Measured TU %d/%d" % (index, len(keys)), flush=True)
    if input_manifest(ROOT) != before or environment_manifest(raw) != environment:
        raise ValueError("measurement inputs changed during compilation")
    snapshot = {
        "schema_version": 1, "commit": args.commit, "run_id": args.run_id,
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "inputs": before, "environment": environment, "plan": plan,
        "xbe_sha256": digest(xbe), "records": records,
        "metric": "raw_xbe_aligned_byte_lower_bound",
        "dirty": bool(git(ROOT, "diff", "--name-only", args.commit, "--", *INPUT_PATHS)),
    }
    write_json(args.output, snapshot)
    if args.publish:
        for record in records:
            record["measurement_manifest"] = str(args.output)
            write_json(ROOT / "artifacts/raw_xbe_structural" /
                       (record["address"] + "-ci.json"), record)
        raw._write_summary(records, sys.argv[1:], len(groups), 0, eligible=items)


def indexed(snapshot, commit, run_id, root, plan, allow_empty=False):
    """Reject stale snapshots before interpreting any metric."""
    if (snapshot.get("schema_version") != 1 or snapshot.get("commit") != commit or
            snapshot.get("run_id") != run_id or snapshot.get("plan") != plan):
        raise ValueError("stale or foreign measurement provenance")
    if snapshot.get("inputs") != input_manifest(root):
        raise ValueError("measurement input hashes no longer match checkout")
    if snapshot.get("xbe_sha256") != digest(root / "halo-patched/cachebeta.xbe"):
        raise ValueError("measurement XBE hash no longer matches reference")
    result = {}
    for record in snapshot["records"]:
        addr = int(record["address"], 0)
        if addr in result:
            raise ValueError("duplicate measured address: " + record["address"])
        source = record.get("source", {})
        if source.get("sha256") != snapshot["inputs"].get(source.get("path")):
            raise ValueError("stale function source: " + record["address"])
        tool = record.get("tool", {})
        if (tool.get("decl_sha256") != snapshot["inputs"]["build/generated/decl.h"] or
                tool.get("bounds_sha256") != snapshot["inputs"]["tools/verify/function_bounds.json"] or
                record["reference"].get("sha256") != snapshot["xbe_sha256"]):
            raise ValueError("stale function reference or declarations: " + record["address"])
        result[addr] = record
    if not result and not allow_empty:
        raise ValueError("empty measurement")
    return result


def counts(record, proven=False):
    """``(matching, compared)`` aligned bytes of one measured function.

    ``proven`` also counts the 4-byte operand of every relocation whose target
    the resolver verified against the reference. The lower-bound metric masks
    those fields, so swapping a literal address for a named symbol would
    otherwise lower the matching count with no loss. Uncertain and mismatched
    relocations are already inside ``compared`` and never count as matching.
    """
    aligned = record.get("aligned_byte_match", {})
    ref = record.get("reference", {})
    if (aligned.get("status") != "scored" or ref.get("bound_provenance") != "table" or
            ref.get("bound_kind") == "no_terminator"):
        return None
    matched, compared = aligned.get("matching_bytes"), aligned.get("compared_bytes")
    if (type(matched) is not int or type(compared) is not int or
            not 0 <= matched <= compared or compared <= 0):
        raise ValueError("invalid measured byte counts")
    if proven:
        resolved = aligned.get("masked_relocation_bytes") or 0
        if type(resolved) is not int or resolved < 0:
            raise ValueError("invalid resolved relocation byte count")
        matched, compared = matched + resolved, compared + resolved
    return matched, compared


def load_byte_scores(root=ROOT):
    """Current byte scores only; never fall back to mnemonic or legacy audits."""
    sys.path.insert(0, str(ROOT))
    from tools.report.generate_decomp_report import _load_raw_xbe_structural_audits
    scores = {}
    for record in _load_raw_xbe_structural_audits(str(root)).values():
        if not record.get("measurement_manifest"):
            continue
        measured = counts(record)
        if measured is None:
            continue
        matched, compared = measured
        aligned = record["aligned_byte_match"]
        scores[record["function"]] = {
            "score": 100 * matched / compared, "metric": "raw_xbe_aligned_byte_lower_bound",
            "addr": record["address"], "source": record["source"]["path"],
            "matching_bytes": matched, "compared_bytes": compared,
            "n_r": aligned.get("aligned_instruction_pairs", 0) + aligned.get("reference_only_instructions", 0),
        }
    return scores


def compare(base, candidate, header_noise=None):
    """Per-address ratchet; improvements elsewhere cannot conceal a loss.

    ``header_noise(before, after)`` may move a byte or counter regression to
    ``header_noise``: it must return True only when nothing the function uses
    changed (see ``header_noise_filter``).
    """
    errors, gaps, noise, checked, added = [], [], [], 0, 0
    for addr in sorted(base.keys() | candidate.keys()):
        before, after = base.get(addr), candidate.get(addr)
        label = "0x%08x %s" % (addr, (after or before)["function"])
        if after is None:
            errors.append(label + ": removed/deactivated measured function")
            continue
        new = counts(after)
        old = counts(before) if before else None
        if before is None:
            added += 1
            if new is None:
                errors.append(label + ": new port has no valid byte measurement")
            continue
        reference_keys = ("sha256", "start", "end", "sha256_span", "bound_kind", "bound_provenance")
        spanless = before["reference"].get("start") is None
        # An unmeasured base whose bound was explicitly unverified (`no_terminator`) holds no
        # byte result either, so verifying that bound is not a reference change.
        unverified = old is None and before["reference"].get("bound_kind") == "no_terminator"
        if spanless or unverified:
            # The base never resolved a span (compile/COFF/lookup failure), so it holds no
            # byte result to compare; only the XBE identity must still agree.
            reference_keys = ("sha256",)
        if any(before["reference"].get(key) != after["reference"].get(key) for key in reference_keys):
            errors.append(label + ": reference span changed; byte results are not comparable")
            continue
        # A spanless base compiled nothing for this function, so its options (which
        # belong to whichever TU it was wrongly attributed to) are not a baseline.
        if not spanless and before.get("tool", {}).get("opt") != after.get("tool", {}).get("opt"):
            errors.append(label + ": compiler options changed")
            continue
        if old is None:
            if new is None and before.get("reason") != after.get("reason"):
                errors.append(label + ": unmeasured result changed")
            elif new is None:
                gaps.append(label + ": " + before.get("reason", "byte comparison unavailable"))
            continue
        checked += 1
        if new is None:
            errors.append(label + ": lost byte measurement")
            continue
        losses = []
        old, new = counts(before, proven=True), counts(after, proven=True)
        if new[0] < old[0] or new[0] * old[1] < old[0] * new[1]:
            losses.append(label + ": bytes %d/%d -> %d/%d" % (*old, *new))
        for key in ("uncertain_relocation_bytes", "mismatched_relocations", "unpaired_relocations",
                    "alignment_ambiguous_steps"):
            # A measured function can still carry null counters; treat them as 0.
            if (after["aligned_byte_match"].get(key) or 0) > (before["aligned_byte_match"].get(key) or 0):
                losses.append(label + ": increased " + key)
        if before.get("literal_byte_match") == "exact" and after.get("literal_byte_match") != "exact":
            losses.append(label + ": lost literal byte identity")
        if losses and header_noise is not None and header_noise(before, after):
            noise += losses
        else:
            errors += losses
    if not checked and not any(counts(record) for record in candidate.values()):
        errors.append("No comparable functions measured; cannot establish a byte regression result")
    return {"errors": errors, "existing_gaps": gaps, "header_noise": noise, "checked_functions": checked,
            "new_functions": added, "base_functions": len(base), "candidate_functions": len(candidate)}


# Not preceded by a word character: the `x34` inside `0x34` is no identifier.
_IDENTIFIER_RE = re.compile(r"(?<!\w)[A-Za-z_]\w*")


def changed_kb_names(root, base, head):
    """Every identifier in a kb.json entry that differs between two revisions.

    Over-collects on purpose (types and parameter names too): a larger set only
    makes ``header_noise_filter`` classify fewer losses as noise. Returns None
    when a change cannot be pinned to individual entries.
    """
    def load(commit):
        return json.loads(subprocess.check_output(
            ["git", "-C", str(root), "show", commit + ":kb.json"]))

    def entries(kb):
        objects = kb.get("objects") or []
        shape = [{key: value for key, value in obj.items() if not isinstance(value, list)}
                 for obj in objects]
        found = {key: value for key, value in kb.items() if key not in ("objects", "md5")}
        for index, obj in enumerate(objects):
            for key, value in obj.items():
                if isinstance(value, list):
                    for item in value:
                        addr = item.get("addr") if isinstance(item, dict) else None
                        found[(index, key, addr if addr is not None else json.dumps(item))] = item
        return kb.get("md5"), shape, found

    before, after = entries(load(base)), entries(load(head))
    if before[:2] != after[:2]:
        return None
    names = set()
    for key in before[2].keys() | after[2].keys():
        old, new = before[2].get(key), after[2].get(key)
        if old != new:
            names.update(_IDENTIFIER_RE.findall(json.dumps([old, new])))
    return names


# Spelled-out in changed header lines but never the subject of an edit.
_HEADER_COMMON_NAMES = frozenset((
    "char", "short", "int", "long", "unsigned", "signed", "float", "double", "void",
    "struct", "union", "enum", "typedef", "const", "static", "define", "bool", "real",
    "int8_t", "int16_t", "int32_t", "uint8_t", "uint16_t", "uint32_t", "co", "cs"))


def changed_header_names(root, base, head):
    """Identifiers on every changed src/types.h line, plus each hunk's enclosing declaration.

    Over-collects on purpose, like ``changed_kb_names``: a larger set only makes
    ``header_noise_filter`` classify fewer losses as noise.
    """
    names = set()
    diff = git(root, "diff", "-U0", base, head, "--", "src/types.h")
    for line in diff.splitlines():
        if line.startswith("@@"):
            names.update(_IDENTIFIER_RE.findall(line.split("@@")[-1]))
        elif line[:1] in "+-" and not line.startswith(("+++", "---")):
            names.update(_IDENTIFIER_RE.findall(line[1:]))
    return names - _HEADER_COMMON_NAMES


def function_text(text, name):
    """Signature and body of the column-0 definition of ``name``, else None."""
    for match in re.finditer(r"(?m)^[A-Za-z_][^\n=;]*\b%s\s*\(" % re.escape(name), text):
        brace = text.find("{", match.end())
        semi = text.find(";", match.end())
        if brace < 0 or 0 <= semi < brace:
            continue
        depth = 0
        for index in range(brace, len(text)):
            depth += (text[index] == "{") - (text[index] == "}")
            if depth == 0:
                return text[match.start():index + 1]
    return None


def header_noise_filter(root, base, head):
    """Classifier for losses that only unrelated declarations can explain.

    VC71 numbers its internal labels across the whole TU, so adding parameters
    to unrelated prototypes in the generated decl.h, or reshaping an unrelated
    struct in src/types.h, can reshuffle registers and scheduling in functions
    that use none of them. A loss counts as that noise only when the
    base-to-head input change is kb.json, src/types.h and .c files, the
    function's own source is unchanged, and neither the function nor any symbol
    its candidate object relocates against changed in kb.json. When src/types.h
    changed, the function's own source must also mention no identifier on a
    changed header line. Anything that cannot be proven stays a regression.
    Returns None when classification is not possible for this revision pair.
    """
    changed = git(root, "diff", "--name-only", base, head, "--", *INPUT_PATHS).splitlines()
    if any(path not in ("kb.json", "src/types.h") and
           not (path.startswith("src/") and path.endswith(".c"))
           for path in changed):
        return None
    names = changed_kb_names(root, base, head) if "kb.json" in changed else set()
    if names is None:
        return None
    header_names = changed_header_names(root, base, head) if "src/types.h" in changed else set()
    import raw_xbe_structural as raw
    sources = {}

    def uses_changed_header_name(after):
        if not header_names:
            return False
        path = after["source"]["path"]
        if path not in sources:
            sources[path] = (root / path).read_text(errors="replace")
        body = function_text(sources[path], after["function"])
        return body is None or bool(set(_IDENTIFIER_RE.findall(body)) & header_names)

    def is_noise(before, after):
        if before.get("source") != after.get("source") or after["function"] in names:
            return False
        if uses_changed_header_name(after):
            return False
        candidate = after.get("candidate") or {}
        try:
            _code, relocs, _provenance = raw._parse_coff(candidate["path"], after["function"])
        except Exception:
            return False
        used = {raw._c_name(r["symbol"]["name"]) for r in relocs if r.get("symbol")}
        return not used & names

    return is_noise


def scope(root, base, head):
    changed = git(root, "diff", "--name-only", base, head).splitlines()
    # KB declarations can change callers in other TUs: conservatively check all.
    relevant = [p for p in changed if p.startswith(("src/", "toolchains/", "third_party/xbox/", "tools/"))
                or p in ("kb.json", "CMakeLists.txt", ".github/workflows/vc71-regression.yml")]
    full = any(not (p.startswith("src/") and p.endswith(".c")) for p in relevant)
    return {"sources": None if full else sorted(set(relevant)), "changed_paths": changed}


def check(args):
    base_root = args.base_root.resolve()
    if base_root == ROOT or not (base_root / ".git").exists():
        raise ValueError("provide a separate base checkout")
    ensure_revision(ROOT, args.head_commit)
    ensure_revision(base_root, args.base_commit)
    plan = scope(ROOT, args.base_commit, args.head_commit)
    args.output.mkdir(parents=True, exist_ok=True)
    if plan["sources"] == []:
        result = {"errors": [], "scope": plan, "skipped": "No compile or measurement inputs changed"}
    else:
        # Overlay only evaluator code. Source, KB, bounds, discovery metadata and
        # declaration generator remain those of their respective revisions.
        for name in EVALUATOR_FILES:
            destination = base_root / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / name, destination)
        xbe = base_root / "halo-patched/cachebeta.xbe"
        xbe.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT / "halo-patched/cachebeta.xbe", xbe)
        run_id = uuid.uuid4().hex
        plan_path = args.output / "plan.json"
        write_json(plan_path, plan)
        snapshots = []
        for label, root, commit in (("base", base_root, args.base_commit),
                                    ("candidate", ROOT, args.head_commit)):
            output = args.output / (label + ".json")
            command = [sys.executable, str(root / EVALUATOR_FILES[0]), "measure",
                       "--commit", commit, "--run-id", run_id, "--plan", str(plan_path),
                       "--output", str(output), "--workers", str(args.workers)]
            if label == "base":
                command.append("--allow-empty")
            if args.cache_dir:
                command += ["--cache-dir", str(args.cache_dir)]
            subprocess.run(command, cwd=root, check=True)
            snapshots.append(json.loads(output.read_text()))
        before, after = snapshots
        if (before["environment"] != after["environment"] or
                any(before["inputs"][name] != after["inputs"][name] for name in EVALUATOR_FILES)):
            raise ValueError("base and candidate used different measurement tools")
        base = indexed(before, args.base_commit, run_id, base_root, plan, allow_empty=True)
        candidate = indexed(after, args.head_commit, run_id, ROOT, plan)
        result = compare(base, candidate,
                         header_noise_filter(ROOT, args.base_commit, args.head_commit))
        result.update({"run_id": run_id, "base_commit": args.base_commit,
                       "head_commit": args.head_commit, "scope": plan})
    write_json(args.output / "result.json", result)
    verdict = "FAIL" if result["errors"] else "PASS"
    lines = ["## Raw-XBE Byte Regression: " + verdict, "",
             "Metric: aligned byte lower bound; relocation uncertainty counts against accuracy.",
             "Fresh base `%s` → candidate `%s`." % (args.base_commit, args.head_commit), "",
             "Compared: %d functions. New: %d. Existing gaps: %d." %
             (result.get("checked_functions", 0), result.get("new_functions", 0), len(result.get("existing_gaps", [])))]
    if result.get("skipped"):
        lines.append(result["skipped"])
    lines += ["", *["- " + error for error in result["errors"][:100]]]
    if result.get("header_noise"):
        lines += ["", "### Header-length noise (not counted)", "",
                  "These functions and everything they reference are unchanged; only the",
                  "longer generated decl.h or an unrelated src/types.h edit moved their",
                  "VC71 code.", "",
                  *["- " + item for item in result["header_noise"][:100]]]
    report = "\n".join(lines) + "\n"
    print(report)
    if os.environ.get("GITHUB_STEP_SUMMARY"):
        with open(os.environ["GITHUB_STEP_SUMMARY"], "a") as stream:
            stream.write(report)
    return bool(result["errors"])


# Persistent measurement checkouts: creating and removing two worktrees took
# 40-110s per run on the 9p mount; moving existing ones rewrites only changed
# files. The name has the length of mkdtemp's "local-XXXXXXXX", so the byte
# cache keys (which include the checkout path length) match either way.
REUSABLE_TREES = "local-reusable"


def lock_reusable_trees(parent):
    """Lock the persistent checkouts; (None, None) while another gate holds them."""
    try:
        import fcntl
    except ImportError:  # Windows: always use temporary checkouts
        return None, None
    home = parent / REUSABLE_TREES
    home.mkdir(exist_ok=True)
    lock = open(home / ".lock", "w")
    try:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except OSError:
        lock.close()
        return None, None
    return lock, home


def reset_tree(tree_root, commit, env):
    """Put a persistent checkout at commit with no untracked, ignored or overlaid files."""
    quiet = ["git", "-c", "core.hooksPath=/dev/null"]
    if (tree_root / ".git").is_file():
        try:
            subprocess.run(quiet + ["-C", str(tree_root), "checkout", "-q", "--detach", "--force", commit],
                           env=env, check=True)
            subprocess.run(quiet + ["-C", str(tree_root), "clean", "-q", "-ffdx"], env=env, check=True)
            return
        except subprocess.CalledProcessError:
            # Stale registration (repository moved, re-cloned or pruned): rebuild.
            subprocess.run(quiet + ["worktree", "remove", "--force", str(tree_root)], cwd=ROOT, env=env)
            shutil.rmtree(tree_root, ignore_errors=True)
    subprocess.run(quiet + ["worktree", "prune"], cwd=ROOT, env=env, check=True)
    subprocess.run(quiet + ["worktree", "add", "--detach", str(tree_root), commit], cwd=ROOT,
                   env=env, check=True)


def local(args):
    """Gate exact Git snapshots without touching the user's index or sources."""
    parent = ROOT / "artifacts/byte_regression"
    parent.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="local-", dir=parent))
    prune_runs(parent)
    base = git(ROOT, "rev-parse", args.base_ref + "^{commit}")
    if args.head_ref:
        head = git(ROOT, "rev-parse", args.head_ref + "^{commit}")
    else:
        env = os.environ.copy()
        if args.working_tree:
            env["GIT_INDEX_FILE"] = str(output / "snapshot.index")
            subprocess.run(["git", "read-tree", "HEAD"], cwd=ROOT, env=env, check=True)
            subprocess.run(["git", "add", "-A", "--", *stageable_inputs(ROOT)], cwd=ROOT, env=env, check=True)
        tree = subprocess.check_output(["git", "write-tree"], cwd=ROOT, env=env, text=True).strip()
        # An unreachable commit object only: no branch/ref moves and no hooks run.
        head = subprocess.check_output(
            ["git", "-c", "user.name=Byte measurement", "-c", "user.email=byte-measurement@localhost",
             "commit-tree", tree, "-p", "HEAD", "-m", "Temporary byte measurement snapshot"],
            cwd=ROOT, env=env, text=True).strip()
    # A pre-commit hook inherits GIT_INDEX_FILE=.git/index (relative); inside
    # a new worktree .git is a file, so worktree add/remove must not see it.
    # A hook run from a linked worktree can also export GIT_DIR/GIT_WORK_TREE,
    # which would make `git -C <new worktree>` resolve to this checkout.
    worktree_env = {k: v for k, v in os.environ.items()
                    if k not in ("GIT_INDEX_FILE", "GIT_DIR", "GIT_WORK_TREE")}
    # Equal-length names: __FILE__ embeds the checkout path in assert
    # strings, and a longer candidate path alone shifted measured bytes.
    labels = (("base-tree", base), ("head-tree", head))
    worktrees, reusable, lock = [], False, None
    try:
        if os.environ.get("HALO_BYTE_GATE_REUSE_TREES", "1") != "0":
            lock, home = lock_reusable_trees(parent)
        if lock is not None:
            try:
                for label, commit in labels:
                    reset_tree(home / label, commit, worktree_env)
                worktrees, reusable = [home / label for label, _commit in labels], True
            except subprocess.CalledProcessError:
                print("Reusable byte-gate checkouts unusable; using temporary ones", flush=True)
        if not reusable:
            for label, commit in labels:
                tree_root = output / label
                subprocess.run(["git", "worktree", "add", "--detach", str(tree_root), commit], cwd=ROOT,
                               env=worktree_env, check=True)
                worktrees.append(tree_root)
        candidate = worktrees[1]
        for name in EVALUATOR_FILES:
            target = candidate / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / name, target)
        target = candidate / "halo-patched/cachebeta.xbe"
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT / "halo-patched/cachebeta.xbe", target)
        print("Byte regression artifacts: " + str(output), flush=True)
        command = [sys.executable, str(candidate / EVALUATOR_FILES[0]), "check",
                   "--base-root", str(worktrees[0]), "--base-commit", base,
                   "--head-commit", head, "--output", str(output / "measurements"),
                   "--workers", str(args.workers)]
        if os.environ.get("HALO_BYTE_GATE_CACHE", "1") != "0":
            cache_dir = parent / "cache"
            prune_cache(cache_dir)
            command += ["--cache-dir", str(cache_dir)]
        return subprocess.run(command, cwd=candidate, env=worktree_env).returncode
    finally:
        if not reusable:
            for tree_root in reversed(worktrees):
                subprocess.run(["git", "worktree", "remove", "--force", str(tree_root)], cwd=ROOT,
                               env=worktree_env, check=True)
        if lock is not None:
            lock.close()
        (output / "snapshot.index").unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    run = sub.add_parser("check")
    run.add_argument("--base-root", type=Path, required=True)
    run.add_argument("--base-commit", required=True)
    run.add_argument("--head-commit", required=True)
    run.add_argument("--output", type=Path, required=True)
    worker = sub.add_parser("measure")
    worker.add_argument("--commit", required=True)
    worker.add_argument("--run-id", required=True)
    worker.add_argument("--plan", type=Path, required=True)
    worker.add_argument("--output", type=Path, required=True)
    worker.add_argument("--allow-empty", action="store_true")
    worker.add_argument("--allow-dirty", action="store_true", help="Exploratory sample only; record dirty input hashes")
    worker.add_argument("--publish", action="store_true", help="Publish fresh records for the dashboard")
    snapshot = sub.add_parser("local", help="Fresh HEAD-versus-index byte gate; preserves dirty work")
    snapshot.add_argument("--base-ref", default="HEAD")
    candidate = snapshot.add_mutually_exclusive_group()
    candidate.add_argument("--head-ref", help="Gate this committed revision instead of the index")
    candidate.add_argument("--working-tree", action="store_true", help="Snapshot working compile inputs via a temporary index")
    for command in (run, worker, snapshot):
        command.add_argument("--workers", type=int, default=DEFAULT_WORKERS)
    for command in (run, worker):
        command.add_argument("--cache-dir", type=Path,
                             help="Reuse/store per-TU records keyed by every measurement input")
    args = parser.parse_args()
    if hasattr(args, "output"):
        args.output = args.output.resolve()
    if args.workers < 1:
        parser.error("--workers must be positive")
    try:
        if args.command == "local":
            return local(args)
        return check(args) if args.command == "check" else measure(args) or 0
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        print("Byte regression evidence invalid: " + str(exc), file=sys.stderr)
        if os.environ.get("GITHUB_STEP_SUMMARY"):
            with open(os.environ["GITHUB_STEP_SUMMARY"], "a") as stream:
                stream.write("## Raw-XBE Byte Regression: INVALID EVIDENCE\n\n" + str(exc) + "\n")
        return 2


if __name__ == "__main__":
    sys.exit(main())
