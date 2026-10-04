#!/usr/bin/env python3
"""
Legacy VC71 score fields describe mnemonic similarity, NOT raw byte accuracy.


Generate decomp.dev-compatible progress report from existing project data.

This tool bridges our existing analysis infrastructure with decomp.dev format,
enabling external progress dashboards without requiring full decomp.dev deployment.
"""

import sys
import os
import json
import argparse
import hashlib
from datetime import datetime
from collections import defaultdict
from pathlib import Path

# Ensure tools/ is on path
_root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '../..'))
sys.path.insert(0, _root_dir)
sys.path.insert(0, os.path.join(_root_dir, 'tools'))

from analysis.knowledge import KnowledgeBase, Function, Data
from analysis.kb_meta import MetadataStore
from report.atomic_write import write_json_atomic, write_text_atomic


def load_function_sizes(cache_path: str) -> dict:
    """Load function size data from Ghidra export."""
    if not os.path.exists(cache_path):
        return {}
    with open(cache_path) as f:
        return json.load(f)


def _fill_sizes_from_bounds(function_cache: dict, bounds_path: str) -> None:
    """Add sizes the Ghidra cache lacks from the committed bounds table.

    build/function_sizes.json is gitignored, so CI has none; without it sizes
    fall back to the gap to the next kb address in the unit, which spans other
    units' code.  The bounds table is tied to the same XBE (end - start).
    """
    try:
        with open(bounds_path) as f:
            table = json.load(f)
    except (OSError, json.JSONDecodeError):
        return
    functions = function_cache.setdefault('functions', {})
    known = {int(k, 16) for k in functions}
    for key, entry in table.items():
        if key == '_meta' or not isinstance(entry, dict):
            continue
        try:
            start = int(key, 16)
            size = int(entry['end'], 16) - start
        except (KeyError, TypeError, ValueError):
            continue
        if size > 0 and start not in known:
            functions[f'0x{start:x}'] = {'size': size}


def _file_hash(path: str, algorithm: str) -> str | None:
    """Return a content hash without loading a report input into memory at once."""
    digest = hashlib.new(algorithm)
    try:
        with open(path, 'rb') as f:
            while True:
                chunk = f.read(1024 * 1024)
                if not chunk:
                    break
                digest.update(chunk)
    except OSError:
        return None
    return digest.hexdigest()


def _input_file_identity(path: str, root_dir: str, include_md5: bool = False) -> dict:
    """Describe a report input with repo-relative path and content hash."""
    exists = os.path.isfile(path)
    identity = {
        'path': os.path.relpath(path, root_dir),
        'exists': exists,
        'size_bytes': None,
        'sha256': None,
    }
    if include_md5:
        identity['md5'] = None
    if not exists:
        return identity
    try:
        identity['size_bytes'] = os.path.getsize(path)
    except OSError:
        return identity
    identity['sha256'] = _file_hash(path, 'sha256')
    if include_md5:
        identity['md5'] = _file_hash(path, 'md5')
    return identity


def _score_input_provenance(path: str, root_dir: str, document: dict) -> dict:
    """Add score count and writer provenance to a score-file identity."""
    identity = _input_file_identity(path, root_dir)
    scores = document.get('scores') if isinstance(document, dict) else None
    provenance = document.get('provenance') if isinstance(document, dict) else None
    identity['score_count'] = len(scores) if isinstance(scores, dict) else 0
    identity['provenance'] = provenance if isinstance(provenance, dict) else None
    return identity


def _reference_validity_summary(document: dict) -> dict:
    """Summarize the VC71 attention queue without exposing every row twice."""
    flagged = document.get('flagged') if isinstance(document, dict) else None
    flagged = flagged if isinstance(flagged, list) else []
    by_state = {}
    for entry in flagged:
        state = entry.get('state') if isinstance(entry, dict) else None
        state = state if isinstance(state, str) and state else 'unknown'
        by_state[state] = by_state.get(state, 0) + 1
    return {
        'flagged_count': len(flagged),
        'by_state': dict(sorted(by_state.items())),
    }


def _equivalence_evidence_summary(verdicts: dict) -> dict:
    """Count latest equivalence verdicts and credible divergence findings."""
    records = list({id(record): record for record in verdicts.values()
                    if isinstance(record, dict)}.values()) if isinstance(verdicts, dict) else []
    return {
        'evidence_count': len(records),
        'divergence_count': sum(
            1 for record in records
            if record.get('status') == 'fail' and record.get('reason') == 'divergence'
        ),
    }


def build_report_provenance(root_dir: str, raw_xbe_path: str,
                            bounds_path: str, bounds_doc: dict,
                            floor_path: str, floor_doc: dict,
                            current_path: str, current_doc: dict,
                            validity_path: str, validity_doc: dict,
                            equiv_verdicts: dict) -> dict:
    """Build stable report metadata for VC71 mnemonic-score evidence inputs."""
    bounds = _input_file_identity(bounds_path, root_dir)
    bounds_meta = bounds_doc.get('_meta') if isinstance(bounds_doc, dict) else None
    bounds['recorded_xbe_md5'] = (
        bounds_meta.get('xbe_md5') if isinstance(bounds_meta, dict) else None
    )
    bounds['record_count'] = sum(
        1 for key in bounds_doc
        if key != '_meta'
    ) if isinstance(bounds_doc, dict) else 0

    validity = _input_file_identity(validity_path, root_dir)
    validity.update(_reference_validity_summary(validity_doc))

    return {
        'schema_version': 1,
        'inputs': {
            'raw_xbe': _input_file_identity(raw_xbe_path, root_dir,
                                             include_md5=True),
            'function_bounds': bounds,
            'vc71_floor': _score_input_provenance(floor_path, root_dir,
                                                   floor_doc),
            'vc71_current': _score_input_provenance(current_path, root_dir,
                                                     current_doc),
            'reference_validity': validity,
            'equivalence': _equivalence_evidence_summary(equiv_verdicts),
            'raw_byte_snapshot': _raw_snapshot_provenance(root_dir),
        },
    }


def _raw_snapshot_provenance(root_dir):
    path = Path(root_dir) / 'artifacts/byte_measurement/snapshot.json'
    result = _input_file_identity(str(path), root_dir)
    try:
        document = json.loads(path.read_text())
        result.update({key: document.get(key) for key in ('commit', 'dirty', 'run_id', 'metric')})
        result['function_count'] = len(document.get('records', []))
    except (OSError, ValueError, TypeError):
        result['function_count'] = 0
    return result


def _measurement_manifest_valid(audit, root_dir, cache):
    """Check complete measurement inputs once per manifest, including headers."""
    manifest = audit.get('measurement_manifest')
    if not manifest:
        # Older exploratory audits retain their existing, limited provenance.
        # The regression gate never consumes these records.
        audit['freshness_scope'] = 'source, XBE and function bounds only; shared inputs unvalidated'
        return True
    if manifest not in cache:
        try:
            snapshot = json.loads(Path(manifest).read_text())
            inputs = snapshot.get('inputs', {})
            cache[manifest] = (snapshot.get('metric') == 'raw_xbe_aligned_byte_lower_bound'
                               and bool(inputs) and all(
                                   _file_hash(str(Path(root_dir) / name), 'sha256') == expected
                                   for name, expected in inputs.items()))
            if cache[manifest]:
                from tools.verify import byte_regression, raw_xbe_structural
                cache[manifest] = snapshot.get('environment') == byte_regression.environment_manifest(raw_xbe_structural)
        except (OSError, ValueError, TypeError, ImportError):
            cache[manifest] = False
    audit['freshness_scope'] = 'all recorded compile and measurement inputs'
    return cache[manifest]


def _estimate_missing_sizes(funcs: list) -> dict[int, int]:
    """Estimate function sizes from adjacent kb addresses when size cache is absent."""
    estimates = {}
    ordered = sorted((f['addr'] for f in funcs if f.get('addr')), key=int)
    for i, addr in enumerate(ordered[:-1]):
        next_addr = ordered[i + 1]
        if next_addr > addr:
            estimates[addr] = next_addr - addr
    return estimates


_SCORE_ADDR_INDEX: dict = {}


def _score_addr_index(scores_data: dict) -> dict:
    """{addr_int: entry} built from each entry's own `addr` provenance field.

    Cached per scores dict (identity-keyed) because it is consulted once per
    ported function and the score file holds thousands of entries.
    """
    cached = _SCORE_ADDR_INDEX.get(id(scores_data))
    if cached is not None:
        return cached
    index: dict = {}
    for entry in scores_data.values():
        a = entry.get('addr') if isinstance(entry, dict) else None
        if not isinstance(a, str):
            continue
        try:
            index.setdefault(int(a, 16), entry)
        except ValueError:
            continue
    _SCORE_ADDR_INDEX.clear()          # only ever one live scores dict
    _SCORE_ADDR_INDEX[id(scores_data)] = index
    return index


def _lookup_score(scores_data: dict, name: str, addr: int, source_path: str | None) -> dict | None:
    """Find a VC71 score by plain name, FUN alias, namespace-qualified ref name,
    or — last — the address recorded in the entry's own provenance.

    The address fallback matters after duplicate pruning: the baseline keeps
    exactly ONE key per address (whichever the scorer emits today) instead of
    carrying both the kb.json name and a stale FUN_<addr> alias, so a function
    whose surviving key matches neither spelling used here would otherwise
    render as unscored despite having a perfectly good measurement.
    """
    keys = (
        name,
        f'FUN_{addr:08x}',
        f'FUN_{addr:08X}',
        f'thunk_FUN_{addr:08x}',
    )
    for key in keys:
        score_entry = scores_data.get(key)
        if score_entry is not None:
            return score_entry
    for key, score_entry in scores_data.items():
        if key.rsplit('::', 1)[-1] == name:
            if not source_path or score_entry.get('source') == source_path:
                return score_entry
    return _score_addr_index(scores_data).get(addr)


def _is_synthetic_unit(obj_name: str, source: str | None) -> bool:
    """Return true for SDK/platform buckets that are not game source TUs."""
    platform_prefixes = ('D3D8:', 'LIBCMT:', 'XAPILIB:', 'XNET:')
    return obj_name == '<xdk_stubs>' or obj_name.startswith(platform_prefixes)


def _source_path(source: str | None) -> str | None:
    if source in (None, '?'):
        return None
    return f'src/halo/{source}'


def _load_scoreable_addrs(root_dir: str) -> set:
    """Addresses VC71 can build a reference for, from the committed bounds table.

    A VC71 reference is derived, not exported: the pristine XBE's bytes for the
    function, bounded by tools/verify/function_bounds.json
    (tools/verify/xbe_reference.py).  So "can this receive a VC71 mnemonic score?" is exactly
    "does the table bound this address?" — objdiff.json and the on-disk delinked
    objects no longer decide it.  An unreadable table returns an empty set, which
    the caller treats as "unknown", not as "nothing is scoreable".
    """
    bounds_path = os.path.join(root_dir, 'tools', 'verify', 'function_bounds.json')
    try:
        with open(bounds_path) as f:
            table = json.load(f)
    except (OSError, json.JSONDecodeError):
        return set()
    addrs = set()
    for key, entry in table.items():
        if key == '_meta' or not isinstance(entry, dict):
            continue
        try:
            start = int(key, 16)
            if int(entry['end'], 16) > start:
                addrs.add(start)
        except (KeyError, TypeError, ValueError):
            continue
    return addrs


def _valid_equiv_counts(data: dict) -> bool:
    """Accept only non-negative, disjoint result counts within the seed total."""
    values = {}
    for field in ('passed', 'failed', 'errors', 'seeds'):
        value = data.get(field)
        if value is None:
            values[field] = None
        elif isinstance(value, bool) or not isinstance(value, int) or value < 0:
            return False
        else:
            values[field] = value
    if values['seeds'] is not None:
        total = sum(values[field] or 0 for field in ('passed', 'failed', 'errors'))
        if total > values['seeds']:
            return False
    return True


def _verified_result_address(root_dir: str, result: dict, bounds: dict,
                             source_hashes: dict) -> str | None:
    """Trust an address alias only when its source and XBE bound still match."""
    provenance = result.get('_report_provenance')
    if not isinstance(provenance, dict) or provenance.get('schema') != 1:
        return None
    try:
        addr = f"0x{int(provenance['address'], 16):x}"
        end = f"0x{int(provenance['reference_end'], 16):x}"
        source = (Path(root_dir) / provenance['source_path']).resolve()
        source.relative_to((Path(root_dir) / 'src' / 'halo').resolve())
        bound = bounds[addr]
        if (result.get('address') != addr
                or provenance.get('xbe_md5') != bounds['_meta']['xbe_md5']
                or f"0x{int(bound['end'], 16):x}" != end):
            return None
    except (KeyError, TypeError, ValueError, OSError):
        return None
    source_key = str(source)
    if source_key not in source_hashes:
        source_hashes[source_key] = _file_hash(source_key, 'sha256')
    if source_hashes[source_key] != provenance.get('source_sha256'):
        return None
    return addr


def _load_equiv_verdicts(root_dir: str) -> dict:
    """Load equivalence pass/fail VERDICTS from batch_verify result JSONs.

    Returns {name: {status, z3_proven, reason}} keyed by function name (the
    target field). status is 'pass'/'fail'/'error'/'not_applicable'. This is
    the correctness verdict — distinct from leaf_cache confidence/coverage,
    which only measure how thoroughly the function was exercised (a divergent
    function can have confidence=high). Scans recursively; latest run wins.
    """
    import glob as _glob
    base = os.environ.get(
        'HALO_REPORT_BATCH_VERIFY_DIR',
        os.path.join(root_dir, 'artifacts', 'batch_verify'),
    )
    if not os.path.isdir(base):
        return {}
    try:
        with open(os.path.join(root_dir, 'tools', 'verify', 'function_bounds.json')) as f:
            bounds = json.load(f)
    except (OSError, json.JSONDecodeError):
        bounds = {}
    source_hashes = {}
    verdicts = {}
    # Sort by mtime ascending so later (newer) results overwrite earlier ones.
    paths = []
    for p in _glob.glob(os.path.join(base, '**', '*.json'), recursive=True):
        try:
            paths.append((os.path.getmtime(p), p))
        except OSError:
            continue
    paths.sort()
    records = []
    for _, p in paths:
        if os.path.basename(p) in ('summary.json', 'results.csv'):
            continue
        try:
            with open(p) as f:
                data = json.load(f)
        except (OSError, json.JSONDecodeError):
            continue
        if not isinstance(data, dict) or not _valid_equiv_counts(data):
            continue
        records.append((p, data))

    # Imported bundles reference their supplemental result files. Those files
    # may live under this scan root, but are evidence for the bundle rather than
    # standalone verdicts and must never overwrite its main result by mtime.
    referenced = set()
    for _p, data in records:
        for case in data.get('targeted_cases', []) or []:
            if not isinstance(case, dict):
                continue
            artifact = case.get('artifact')
            if artifact:
                referenced.add(os.path.normcase(os.path.abspath(
                    artifact if os.path.isabs(artifact) else os.path.join(root_dir, artifact))))

    for p, data in records:
        if os.path.normcase(os.path.abspath(p)) in referenced:
            continue
        target = data.get('target')
        status = data.get('status')
        if (not isinstance(target, str) or not target.strip()
                or status not in ('pass', 'fail', 'error', 'inconclusive',
                                  'not_applicable')):
            continue
        targeted_cases = data.get('targeted_cases', [])
        if not isinstance(targeted_cases, list):
            targeted_cases = []
        normalized_cases = []
        for case in targeted_cases:
            if not isinstance(case, dict):
                continue
            normalized = dict(case)
            result = case.get('result')
            if isinstance(result, dict):
                for field in ('target', 'status', 'passed', 'failed', 'errors',
                              'seeds', 'coverage_pct', 'confidence', 'reason'):
                    normalized.setdefault(field, result.get(field))
            normalized_cases.append(normalized)
        targeted_cases = normalized_cases
        verdict = {
            'status': status,
            'z3_proven': bool(data.get('z3_proven')),
            'reason': data.get('reason'),
            'confidence': data.get('confidence'),
            'coverage_pct': data.get('coverage_pct'),
            'divergence_summary': data.get('divergence_summary'),
            'log_path': data.get('log_path'),
            'passed': data.get('passed'),
            'failed': data.get('failed'),
            'errors': data.get('errors'),
            'seeds': data.get('seeds'),
            'artifact': os.path.relpath(p, root_dir).replace(os.sep, '/'),
            'targeted_cases': targeted_cases,
            'targeted_case_count': len(targeted_cases),
            'targeted_case_passed': sum(
                1 for case in targeted_cases if case.get('status') == 'pass'),
        }
        verdicts[target] = verdict
        addr = _verified_result_address(root_dir, data, bounds, source_hashes)
        if addr:
            verdicts[addr] = verdict
    return verdicts


def _load_snapshot_data(results_path: str = None) -> dict:
    """Load snapshot verification results from game_state_verify.py output.
    Returns a dict keyed by function name: {passed, coverage, confidence, object}.
    """
    if results_path is None:
        root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '../..'))
        results_path = os.path.join(root_dir, 'artifacts', 'equivalence', 'ci_results.json')
    if not os.path.exists(results_path):
        return {}
    with open(results_path, encoding='utf-8') as f:
        data = json.load(f)
    out = {}
    for r in data.get('results', []):
        name = r.get('func', '')
        if not name:
            continue
        total_seeds = r.get('total_seeds', 0)
        passed = r.get('passed', 0)
        errors = r.get('errors', 0)
        status = r.get('status')
        applicable = r.get('applicable')
        if status == 'not_applicable' or applicable is False:
            # Harness/reference-data gap (missing delinked reference, truncated
            # oracle, unicorn unavailable, ...) -- not a tested verdict. Keep it
            # out of both the passed and failed buckets (None == "not run"),
            # same treatment as an entry with no snapshot data at all, rather
            # than summing it into "failed" for the dashboard.
            snapshot_passed = None
        else:
            snapshot_passed = passed == total_seeds and errors == 0 and total_seeds > 0
        out[name] = {
            'snapshot_passed': snapshot_passed,
            'snapshot_status': status,
            'snapshot_coverage': r.get('coverage', 0.0),
            'snapshot_confidence': r.get('confidence', 'unknown'),
            'snapshot_object': r.get('object', '?'),
        }
    return out


def _load_raw_byte_summary(root_dir: str) -> dict:
    summary_path = Path(root_dir) / 'artifacts' / 'raw_byte_audit' / 'summary.json'
    if not summary_path.is_file():
        return {}
    try:
        with summary_path.open(encoding='utf-8') as f:
            summary = json.load(f)
        if summary.get('xbe_sha256') != _file_hash(str(Path(root_dir) / 'halo-patched' / 'cachebeta.xbe'), 'sha256'):
            return {}
        if summary.get('bounds_sha256') != _file_hash(str(Path(root_dir) / 'tools' / 'verify' / 'function_bounds.json'), 'sha256'):
            return {}
        decl_path = Path(root_dir) / 'build' / 'generated' / 'decl.h'
        if summary.get('decl_sha256') != _file_hash(str(decl_path), 'sha256'):
            return {}
        return summary
    except (OSError, json.JSONDecodeError, TypeError):
        return {}


def _load_raw_byte_audits(root_dir: str) -> dict:
    """Load fresh literal raw-byte audit records keyed by XBE address.

    An audit is displayable only when its source, pristine XBE, and committed
    bounds entry still match the recorded provenance. This prevents a historical
    `raw-byte exact` result from surviving a source, binary, or bound change.
    """
    audit_dir = Path(root_dir) / 'artifacts' / 'raw_byte_audit'
    bounds_path = Path(root_dir) / 'tools' / 'verify' / 'function_bounds.json'
    xbe_path = Path(root_dir) / 'halo-patched' / 'cachebeta.xbe'
    if not audit_dir.is_dir() or not bounds_path.is_file() or not xbe_path.is_file():
        return {}
    try:
        with bounds_path.open(encoding='utf-8') as f:
            bounds = json.load(f)
    except (OSError, json.JSONDecodeError):
        return {}
    xbe_sha256 = _file_hash(str(xbe_path), 'sha256')
    latest = {}
    for audit_path in audit_dir.glob('*.json'):
        try:
            with audit_path.open(encoding='utf-8') as f:
                audit = json.load(f)
            address = audit.get('address')
            reference = audit.get('reference', {})
            candidate = audit.get('candidate', {})
            source = audit.get('source', {})
            verdict = audit.get('verdict')
            try:
                address_key = f'0x{int(address, 16):x}'
                reference_start = f'0x{int(reference.get("start"), 16):x}'
                reference_end = f'0x{int(reference.get("end"), 16):x}'
            except (TypeError, ValueError):
                continue
            bound = bounds.get(address_key)
            if (verdict not in ('raw-byte exact', 'bytes differ', 'not comparable')
                    or not isinstance(bound, dict)
                    or reference.get('sha256') != xbe_sha256
                    or reference_start != address_key
                    or reference_end != bound.get('end')):
                continue
            source_path = source.get('path')
            if not source_path or source.get('sha256') != _file_hash(source_path, 'sha256'):
                continue
            if not candidate.get('sha256') or not reference.get('sha256_span'):
                continue
        except (OSError, json.JSONDecodeError, TypeError):
            continue
        current = latest.get(address)
        timestamp = audit.get('generated_at', '')
        if current is None or current.get('generated_at', '') < timestamp:
            audit['artifact'] = os.path.relpath(audit_path, root_dir).replace(os.sep, '/')
            latest[address_key] = audit
    return latest


def _load_raw_xbe_structural_audits(root_dir: str) -> dict:
    """Load fresh relocation-masked structural records keyed by XBE address.

    This is a separate evidence lane from the strict literal raw-byte audit.
    Records must carry the stable batch schema and match the current source,
    pristine XBE, and committed function bound before they are displayable.
    The bound is checked per function (start/end), not by hashing the whole
    bounds file: a rename or new entry elsewhere must not hide every record.
    """
    audit_dir = Path(root_dir) / 'artifacts' / 'raw_xbe_structural'
    bounds_path = Path(root_dir) / 'tools' / 'verify' / 'function_bounds.json'
    xbe_path = Path(root_dir) / 'halo-patched' / 'cachebeta.xbe'
    if not audit_dir.is_dir() or not bounds_path.is_file() or not xbe_path.is_file():
        return {}
    try:
        with bounds_path.open(encoding='utf-8') as f:
            bounds = json.load(f)
    except (OSError, json.JSONDecodeError):
        return {}

    xbe_sha256 = _file_hash(str(xbe_path), 'sha256')
    latest = {}
    manifest_cache = {}
    valid_verdicts = ('structural exact', 'structural differ', 'not comparable')
    for audit_path in audit_dir.glob('*.json'):
        if audit_path.name == 'summary.json':
            continue
        try:
            with audit_path.open(encoding='utf-8') as f:
                audit = json.load(f)
            if (audit.get('schema_version') != 2
                    or audit.get('lane') != 'raw_xbe_structural'):
                continue
            if not _measurement_manifest_valid(audit, root_dir, manifest_cache):
                continue
            address = audit.get('address')
            reference = audit.get('reference')
            source = audit.get('source')
            record_bounds = audit.get('bounds')
            verdict = audit.get('verdict')
            error_record = verdict == 'not comparable' and record_bounds is None
            if (not isinstance(reference, dict) or verdict not in valid_verdicts
                    or (not error_record and
                        (not isinstance(source, dict) or not isinstance(record_bounds, dict)))):
                continue
            address_key = f'0x{int(address, 16):x}'
            if error_record:
                verification = audit.get('verification')
                if (reference.get('sha256') != xbe_sha256
                        or not audit.get('generated_at')
                        or not isinstance(verification, dict)
                        or any(key not in verification for key in (
                            'boundaries', 'compiler', 'behavior',
                            'instruction_operands', 'literal_bytes'))):
                    continue
                current = latest.get(address_key)
                timestamp = audit.get('generated_at', '')
                if current is None or current.get('generated_at', '') < timestamp:
                    audit['artifact'] = os.path.relpath(audit_path, root_dir).replace(os.sep, '/')
                    latest[address_key] = audit
                continue
            start = f'0x{int(record_bounds.get("start"), 16):x}'
            end = f'0x{int(record_bounds.get("end"), 16):x}'
            reference_start = f'0x{int(reference.get("start"), 16):x}'
            reference_end = f'0x{int(reference.get("end"), 16):x}'
            bound = bounds.get(address_key)
            source_path = source.get('path')
            if source_path and not Path(source_path).is_absolute():
                source_path = str(Path(root_dir) / source_path)
            byte_counts = audit.get('byte_counts')
            matching_bytes = audit.get('matching_non_relocation_bytes')
            non_relocation_bytes = (byte_counts.get('non_relocation')
                                    if isinstance(byte_counts, dict) else None)
            byte_accuracy = audit.get('byte_accuracy')
            comparable = verdict in ('structural exact', 'structural differ')
            byte_fields_valid = (
                isinstance(matching_bytes, int) and not isinstance(matching_bytes, bool)
                and isinstance(non_relocation_bytes, int) and not isinstance(non_relocation_bytes, bool)
                and non_relocation_bytes > 0
                and 0 <= matching_bytes <= non_relocation_bytes
                and isinstance(byte_accuracy, (int, float)) and not isinstance(byte_accuracy, bool)
                and 0.0 <= float(byte_accuracy) <= 1.0
                and abs(float(byte_accuracy) - matching_bytes / non_relocation_bytes) < 1e-9
            )
            if (not isinstance(bound, dict)
                    or start != address_key
                    or end != bound.get('end')
                    or reference_start != address_key
                    or reference_end != bound.get('end')
                    or reference.get('sha256') != xbe_sha256
                    or not source_path
                    or source.get('sha256') != _file_hash(source_path, 'sha256')
                    or not audit.get('generated_at')
                    or not audit.get('candidate', {}).get('sha256')
                    or (comparable and not byte_fields_valid)):
                continue
        except (OSError, json.JSONDecodeError, TypeError, ValueError):
            continue
        current = latest.get(address_key)
        timestamp = audit.get('generated_at', '')
        if current is None or current.get('generated_at', '') < timestamp:
            audit['artifact'] = os.path.relpath(audit_path, root_dir).replace(os.sep, '/')
            latest[address_key] = audit
    return latest


def _raw_xbe_structural_totals(audits: dict) -> dict:
    """Aggregate accepted structural records without trusting a batch summary."""
    totals = {}
    for audit in audits.values():
        verdict = audit.get('verdict')
        if verdict not in ('structural exact', 'structural differ', 'not comparable'):
            continue
        length = audit.get('reference', {}).get('length', 0)
        entry = totals.setdefault(verdict, {'functions': 0, 'original_bytes': 0})
        entry['functions'] += 1
        entry['original_bytes'] += length if isinstance(length, int) else 0
    comparable = totals.get('structural exact', {'functions': 0})['functions'] + \
        totals.get('structural differ', {'functions': 0})['functions']
    exact = totals.get('structural exact', {'functions': 0})['functions']
    matching_bytes = sum(
        audit.get('matching_non_relocation_bytes', 0)
        for audit in audits.values()
        if audit.get('verdict') in ('structural exact', 'structural differ')
    )
    non_relocation_bytes = sum(
        audit.get('byte_counts', {}).get('non_relocation', 0)
        for audit in audits.values()
        if audit.get('verdict') in ('structural exact', 'structural differ')
    )
    aligned = [audit.get('aligned_byte_match', {}) for audit in audits.values()]
    aligned = [item for item in aligned if item.get('status') == 'scored']
    aligned_matching = sum(item.get('matching_bytes', 0) or 0 for item in aligned)
    aligned_compared = sum(item.get('compared_bytes', 0) or 0 for item in aligned)
    aligned_uncertain = sum(item.get('uncertain_relocation_bytes', 0) or 0
                            for item in aligned)
    aligned_mismatched = sum(item.get('mismatched_relocations', 0) or 0
                             for item in aligned)
    aligned_mismatched_bytes = sum(item.get('mismatched_relocation_bytes', 0) or 0
                                   for item in aligned)
    return {
        'schema_version': 2,
        'lane': 'raw_xbe_structural',
        'totals': totals,
        'audited_functions': sum(v['functions'] for v in totals.values()),
        'function_exact_rate': exact / comparable if comparable else None,
        'matching_non_relocation_bytes': matching_bytes,
        'non_relocation_bytes': non_relocation_bytes,
        'byte_accuracy': (matching_bytes / non_relocation_bytes
                          if non_relocation_bytes else None),
        'aligned_scored_functions': len(aligned),
        'aligned_matching_bytes': aligned_matching,
        'aligned_compared_bytes': aligned_compared,
        'aligned_uncertain_bytes': aligned_uncertain,
        'aligned_mismatched_relocations': aligned_mismatched,
        'aligned_mismatched_relocation_bytes': aligned_mismatched_bytes,
        'aligned_byte_accuracy_lower': (aligned_matching / aligned_compared
                                        if aligned_compared else None),
        'aligned_byte_accuracy_upper': ((aligned_matching + aligned_uncertain) /
                                        aligned_compared if aligned_compared else None),
        'aligned_provisional_functions': sum(
            1 for item in aligned if item.get('accuracy_is_provisional')),
    }


def _raw_xbe_structural_dashboard_totals(units: list[dict]) -> dict:
    """Scope structural byte totals to implemented game functions.

    Raw audit artifacts can outlive a port or belong to synthetic platform
    units. The dashboard population is therefore derived from the current
    function records: ported functions in non-synthetic units only. A function
    with no audit is unchecked; an attempted audit without scored aligned bytes
    is counted as unable to compare.
    """
    functions = [
        function
        for unit in units
        if not unit.get('synthetic')
        for function in unit.get('functions', [])
        if function.get('ported')
    ]
    totals = {}
    compared = []
    cannot_compare = 0
    unchecked = 0
    for function in functions:
        verdict = function.get('raw_xbe_structural_verdict')
        if verdict:
            entry = totals.setdefault(verdict, {'functions': 0, 'original_bytes': 0})
            entry['functions'] += 1
            entry['original_bytes'] += function.get('raw_xbe_structural_reference_length') or 0
        aligned_status = function.get('raw_xbe_aligned_status')
        compared_bytes = function.get('raw_xbe_aligned_compared_bytes') or 0
        if aligned_status == 'scored' and compared_bytes > 0:
            compared.append(function)
        elif verdict:
            cannot_compare += 1
        else:
            unchecked += 1

    matching = sum(function.get('raw_xbe_aligned_matching_bytes') or 0
                   for function in compared)
    compared_bytes = sum(function.get('raw_xbe_aligned_compared_bytes') or 0
                         for function in compared)
    uncertain = sum(function.get('raw_xbe_aligned_uncertain_bytes') or 0
                    for function in compared)
    exact = totals.get('structural exact', {'functions': 0})['functions']
    structural_comparable = (exact +
                             totals.get('structural differ', {'functions': 0})['functions'])
    non_relocation_matching = sum(
        function.get('raw_xbe_structural_matching_non_relocation_bytes') or 0
        for function in functions if function.get('raw_xbe_structural_verdict') in
        ('structural exact', 'structural differ'))
    non_relocation_bytes = sum(
        function.get('raw_xbe_structural_non_relocation_bytes') or 0
        for function in functions if function.get('raw_xbe_structural_verdict') in
        ('structural exact', 'structural differ'))
    return {
        'schema_version': 2,
        'lane': 'raw_xbe_structural',
        'population_scope': 'implemented game functions',
        'implemented_functions': len(functions),
        'compared_functions': len(compared),
        'cannot_compare_functions': cannot_compare,
        'unchecked_functions': unchecked,
        'totals': totals,
        'audited_functions': sum(entry['functions'] for entry in totals.values()),
        'function_exact_rate': (exact / structural_comparable
                                if structural_comparable else None),
        'matching_non_relocation_bytes': non_relocation_matching,
        'non_relocation_bytes': non_relocation_bytes,
        'byte_accuracy': (non_relocation_matching / non_relocation_bytes
                          if non_relocation_bytes else None),
        'aligned_scored_functions': len(compared),
        'aligned_matching_bytes': matching,
        'aligned_compared_bytes': compared_bytes,
        'aligned_uncertain_bytes': uncertain,
        'aligned_mismatched_relocations': sum(
            function.get('raw_xbe_aligned_mismatched_relocations') or 0
            for function in compared),
        'aligned_mismatched_relocation_bytes': sum(
            function.get('raw_xbe_aligned_mismatched_relocation_bytes') or 0
            for function in compared),
        'aligned_byte_accuracy_lower': (matching / compared_bytes
                                        if compared_bytes else None),
        'aligned_byte_accuracy_upper': ((matching + uncertain) / compared_bytes
                                       if compared_bytes else None),
        'aligned_provisional_functions': sum(
            1 for function in compared if function.get('raw_xbe_aligned_provisional')),
    }


def _load_runtime_oracle_data(results_root: str = None) -> dict:
    """Load latest runtime-oracle result per target function."""
    if results_root is None:
        root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '../..'))
        results_root = os.path.join(root_dir, 'artifacts', 'runtime_oracle')
    root = Path(results_root)
    if not root.exists():
        return {}

    latest = {}
    for summary_path in root.glob('*/summary.json'):
        try:
            with summary_path.open(encoding='utf-8') as f:
                data = json.load(f)
        except (OSError, json.JSONDecodeError):
            continue

        target = data.get('target', '')
        if not target:
            continue

        finished = data.get('finished_utc') or data.get('started_utc') or ''
        current = latest.get(target)
        if current and current.get('finished_utc', '') >= finished:
            continue

        latest[target] = {
            'runtime_oracle_tested': True,
            'runtime_oracle_passed': bool(data.get('ok')),
            'runtime_oracle_run_id': data.get('run_id', ''),
            'runtime_oracle_finished_utc': data.get('finished_utc'),
            'runtime_oracle_artifact': data.get('artifact_dir', str(summary_path.parent)),
            'runtime_oracle_summary': str(summary_path),
        }

    return latest


def compute_unit_stats(kb: KnowledgeBase, store: MetadataStore,
                       function_cache: dict,
                       vc71_scores: dict = None,
                       leaf_cache: dict = None,
                       snapshot_data: dict = None,
                       runtime_oracle_data: dict = None,
                       scoreable_addrs: set = None,
                       equiv_verdicts: dict = None,
                       validity_data: dict = None,
                       raw_byte_audits: dict = None,
                       raw_xbe_structural_audits: dict = None) -> list[dict]:
    """Compute per-unit statistics in decomp.dev format.

    scoreable_addrs: addresses the committed VC71 bounds table can derive a
    reference for. Drives the dashboard's distinction between "scoreable, not yet
    run" and "not scoreable (VC71 impossible)". A unit counts as scoreable when
    at least one of its functions is bounded.

    validity_data: the VC71 attention queue (reference_validity.json). Functions
    listed there were not scored; each carries a state (compile_failed |
    no_reference) surfaced as ``vc71_flagged`` so the dashboard shows a distinct
    badge rather than a blank "—".
    """

    if vc71_scores is None:
        vc71_scores = {}
    scores_data = _mnemonic_score_entries(vc71_scores)

    # Build a flagged-function lookup keyed by every alias we might join on:
    # the vc71-emitted function string, and FUN_<addr>/0x<addr> when an address
    # is recorded (compile_failed entries carry one).  no_reference wins over
    # compile_failed only if both somehow appear (compile_failed is set first).
    flagged_map: dict = {}
    if validity_data:
        for e in validity_data.get('flagged', []):
            st = e.get('state') or 'no_reference'
            fn = e.get('function')
            if fn:
                flagged_map.setdefault(fn, st)
            a = e.get('addr')
            if a:
                try:
                    ai = int(a, 16)
                    flagged_map.setdefault(f'FUN_{ai:08x}', st)
                    flagged_map.setdefault(f'0x{ai:x}', st)
                except (ValueError, TypeError):
                    pass

    if scoreable_addrs is None:
        scoreable_addrs = set()

    if equiv_verdicts is None:
        equiv_verdicts = {}
    
    if leaf_cache is None:
        leaf_cache = {}
    
    if snapshot_data is None:
        snapshot_data = {}

    if runtime_oracle_data is None:
        runtime_oracle_data = {}

    if raw_byte_audits is None:
        raw_byte_audits = {}
    if raw_xbe_structural_audits is None:
        raw_xbe_structural_audits = {}

    units = []
    drift = {
        'kb_ported_missing_meta': 0,
        'meta_ported_missing_kb': 0,
    }
    functions_data = function_cache.get('functions', {})
    
    tracked_match_sum = 0.0
    tracked_match_weighted_sum = 0.0
    tracked_scored_count = 0
    tracked_scored_weighted = 0
    
    tracked_equiv_tested = 0
    tracked_equiv_high_conf = 0
    tracked_equiv_avg_cov_sum = 0.0
    tracked_equiv_cov_count = 0
    
    tracked_snap_tested = 0
    tracked_snap_passed = 0
    tracked_snap_high_conf = 0
    tracked_snap_cov_sum = 0.0
    tracked_snap_cov_count = 0

    tracked_runtime_tested = 0
    tracked_runtime_passed = 0
    
    # Group functions and data by object file, split by Symbol subclass
    obj_to_funcs = defaultdict(list)
    obj_to_data = defaultdict(list)
    
    for addr_str, symbol in kb.addr_to_symbol.items():
        addr = int(addr_str)
        obj = kb.symbol_to_object.get(symbol)
        if not obj or not symbol.name:
            continue
        entry = {
            'addr': addr,
            'name': symbol.name,
            'symbol': symbol
        }
        if isinstance(symbol, Function):
            obj_to_funcs[obj].append(entry)
        elif isinstance(symbol, Data):
            obj_to_data[obj].append(entry)
    
    for obj_name in sorted(set(list(obj_to_funcs.keys()) + list(obj_to_data.keys()))):
        source = kb.object_to_source.get(obj_name, '?')
        funcs = obj_to_funcs.get(obj_name, [])
        data_syms = obj_to_data.get(obj_name, [])
        synthetic = _is_synthetic_unit(obj_name, source)
        
        unit_funcs = []
        total_bytes = 0
        ported_bytes = 0
        ported_count = 0
        match_scores = []
        match_weighted_sum = 0.0
        match_scored_bytes = 0
        source_path_for_scores = _source_path(source)
        estimated_sizes = {} if synthetic else _estimate_missing_sizes(funcs)
        
        for func_info in funcs:
            addr = func_info['addr']
            name = func_info['name']
            addr_hex = f'0x{addr:x}'
            
            # Get size from cache
            size = 0
            if not synthetic and addr_hex in functions_data:
                size = functions_data[addr_hex].get('size', 0)
            if not size:
                size = estimated_sizes.get(addr, 0)
            
            # Get status from kb_meta
            meta = store.symbols.get(f'{addr:#x}')
            status = meta.status if meta else 'unknown'
            is_ported = bool(func_info['symbol'].ported)
            meta_is_ported = status in ('ported', 'verified')
            if is_ported and not meta_is_ported:
                drift['kb_ported_missing_meta'] += 1
            elif meta_is_ported and not is_ported:
                drift['meta_ported_missing_kb'] += 1
            
            # Look up VC71 mnemonic score. Prefer the real kb.json name, but fall
            # back to the address-keyed FUN_<addr> name: vc71_verify records many
            # functions under their *delinked reference* symbol (still FUN_<addr>)
            # rather than the renamed kb.json symbol. Joining on name alone drops
            # ~1100 valid scores; the address is the stable key.
            match_pct = None
            # Advisory operand-normalized score (mnemonic + operand shape with
            # canonicalized registers).  Absent for entries scored before the
            # feature existed; display-only, gates nothing.
            opnd_pct = None
            score_entry = _lookup_score(scores_data, name, addr, source_path_for_scores)
            if score_entry is not None:
                match_pct = score_entry.get('score')
                opnd_pct = score_entry.get('opnd_percent')

            # If unscored, see whether the attention queue explains why (a whole-TU
            # VC71 compile failure, or a broken/absent delinked reference).  Only
            # surface for ported functions with no score — a scored function is fine.
            vc71_flagged = None
            if is_ported and match_pct is None:
                vc71_flagged = (flagged_map.get(name)
                                or flagged_map.get(f'FUN_{addr:08x}')
                                or flagged_map.get(f'FUN_{addr:08X}')
                                or flagged_map.get(addr_hex))

            # Look up equivalence COVERAGE data from leaf_cache. NOTE: confidence
            # and coverage describe how thoroughly the function was tested — they
            # are NOT a pass/fail verdict. A divergent (buggy) function can have
            # confidence=high. The actual verdict comes from equiv_verdicts below.
            eq = leaf_cache.get(addr_hex, {})
            equiv_class = eq.get('class')
            equiv_coverage = eq.get('coverage_pct')
            equiv_confidence = eq.get('confidence')

            # Look up equivalence VERDICT (pass/fail/error) from batch_verify
            # results, joined by name then address-keyed FUN_<addr> alias.
            ev = (equiv_verdicts.get(name)
                  or equiv_verdicts.get(f'FUN_{addr:08x}')
                  or equiv_verdicts.get(f'FUN_{addr:08X}')
                  or equiv_verdicts.get(f'thunk_FUN_{addr:08x}')
                  or equiv_verdicts.get(addr_hex))
            equiv_status = ev.get('status') if ev else None
            equiv_proven = bool(ev.get('z3_proven')) if ev else False
            equiv_reason = ev.get('reason') if ev else None
            equiv_divergence = ev.get('divergence_summary') if ev else None
            equiv_log_path = ev.get('log_path') if ev else None
            equiv_passed = ev.get('passed') if ev else None
            equiv_failed = ev.get('failed') if ev else None
            equiv_errors = ev.get('errors') if ev else None
            equiv_seeds = ev.get('seeds') if ev else None
            equiv_artifact = ev.get('artifact') if ev else None
            equiv_targeted_cases = ev.get('targeted_cases', []) if ev else []
            equiv_targeted_case_count = ev.get('targeted_case_count', 0) if ev else 0
            equiv_targeted_case_passed = ev.get('targeted_case_passed', 0) if ev else 0
            # When a verdict exists, its confidence/coverage are paired with the
            # status (same run) and are more accurate than the leaf_cache snapshot,
            # so prefer them for the verified-gating decision and the display.
            if ev:
                if ev.get('confidence') is not None:
                    equiv_confidence = ev.get('confidence')
                if ev.get('coverage_pct') is not None:
                    equiv_coverage = ev.get('coverage_pct')

            # Look up snapshot verification data
            snap = snapshot_data.get(name, {})
            snapshot_passed = snap.get('snapshot_passed', None)
            snapshot_coverage = snap.get('snapshot_coverage', None)
            snapshot_confidence = snap.get('snapshot_confidence', None)

            # Look up runtime-oracle data (latest run wins)
            runtime = (runtime_oracle_data.get(name) or
                       runtime_oracle_data.get(addr_hex) or
                       runtime_oracle_data.get(addr_hex.lower()) or
                       runtime_oracle_data.get(f'0x{addr:08x}') or
                       runtime_oracle_data.get(f'FUN_{addr:08X}'))
            runtime_tested = runtime.get('runtime_oracle_tested') if runtime else None
            runtime_passed = runtime.get('runtime_oracle_passed') if runtime else None
            runtime_run_id = runtime.get('runtime_oracle_run_id') if runtime else None
            runtime_finished_utc = runtime.get('runtime_oracle_finished_utc') if runtime else None
            runtime_artifact = runtime.get('runtime_oracle_artifact') if runtime else None
            runtime_summary = runtime.get('runtime_oracle_summary') if runtime else None

            raw_audit = raw_byte_audits.get(f'0x{addr:x}')
            raw_byte_verdict = raw_audit.get('verdict') if raw_audit else None
            raw_byte_reason = raw_audit.get('reason') if raw_audit else None
            raw_byte_audited_at = raw_audit.get('generated_at') if raw_audit else None
            raw_byte_artifact = raw_audit.get('artifact') if raw_audit else None
            raw_byte_first_difference = raw_audit.get('first_difference') if raw_audit else None
            raw_byte_candidate_length = (raw_audit.get('candidate', {}).get('length')
                                         if raw_audit else None)
            raw_byte_reference_length = (raw_audit.get('reference', {}).get('length')
                                         if raw_audit else None)

            structural_audit = raw_xbe_structural_audits.get(f'0x{addr:x}')
            structural_verdict = structural_audit.get('verdict') if structural_audit else None
            structural_confidence = structural_audit.get('confidence') if structural_audit else None
            structural_reason = structural_audit.get('reason') if structural_audit else None
            structural_artifact = structural_audit.get('artifact') if structural_audit else None
            structural_audited_at = structural_audit.get('generated_at') if structural_audit else None
            structural_first_difference = (structural_audit.get('first_difference')
                                           if structural_audit else None)
            structural_reference_length = (structural_audit.get('reference', {}).get('length')
                                           if structural_audit else None)
            structural_matching_bytes = (structural_audit.get('matching_non_relocation_bytes')
                                         if structural_audit else None)
            structural_non_relocation_bytes = (structural_audit.get('byte_counts', {}).get('non_relocation')
                                               if structural_audit else None)
            structural_byte_accuracy = (structural_audit.get('byte_accuracy')
                                        if structural_audit else None)
            structural_masked_bytes = (structural_audit.get('byte_counts', {}).get('relocation_operand')
                                       if structural_audit else None)
            structural_lower_bound = (bool(structural_audit.get('accuracy_is_lower_bound'))
                                      if structural_audit else None)
            structural_reg_arg = (bool(structural_audit.get('register_argument'))
                                  if structural_audit else None)
            structural_unmasked = (structural_audit.get('unmasked_relocations')
                                   if structural_audit else None)
            aligned_audit = (structural_audit.get('aligned_byte_match', {})
                             if structural_audit else {})
            structural_verification = (structural_audit.get('verification', {})
                                       if structural_audit else {})

            func_entry = {
                'address': addr_hex,
                'name': name,
                'size': size,
                'status': status,
                'ported': is_ported,
                'match_percent': match_pct,
                'match_metric': 'mnemonic_similarity_not_raw_bytes',
                'opnd_percent': opnd_pct,
                'vc71_flagged': vc71_flagged,
                'equiv_class': equiv_class,
                'equiv_coverage': equiv_coverage,
                'equiv_confidence': equiv_confidence,
                'equiv_status': equiv_status,
                'equiv_proven': equiv_proven,
                'equiv_reason': equiv_reason,
                'equiv_divergence': equiv_divergence,
                'equiv_log_path': equiv_log_path,
                'equiv_passed': equiv_passed,
                'equiv_failed': equiv_failed,
                'equiv_errors': equiv_errors,
                'equiv_seeds': equiv_seeds,
                'equiv_artifact': equiv_artifact,
                'equiv_targeted_cases': equiv_targeted_cases,
                'equiv_targeted_case_count': equiv_targeted_case_count,
                'equiv_targeted_case_passed': equiv_targeted_case_passed,
                'snapshot_passed': snapshot_passed,
                'snapshot_coverage': snapshot_coverage,
                'snapshot_confidence': snapshot_confidence,
                'runtime_oracle_tested': runtime_tested,
                'runtime_oracle_passed': runtime_passed,
                'runtime_oracle_run_id': runtime_run_id,
                'runtime_oracle_finished_utc': runtime_finished_utc,
                'runtime_oracle_artifact': runtime_artifact,
                'runtime_oracle_summary': runtime_summary,
                'raw_byte_verdict': raw_byte_verdict,
                'raw_byte_reason': raw_byte_reason,
                'raw_byte_audited_at': raw_byte_audited_at,
                'raw_byte_artifact': raw_byte_artifact,
                'raw_byte_first_difference': raw_byte_first_difference,
                'raw_byte_candidate_length': raw_byte_candidate_length,
                'raw_byte_reference_length': raw_byte_reference_length,
                'raw_xbe_structural_verdict': structural_verdict,
                'raw_xbe_structural_confidence': structural_confidence,
                'raw_xbe_structural_reason': structural_reason,
                'raw_xbe_structural_artifact': structural_artifact,
                'raw_xbe_structural_audited_at': structural_audited_at,
                'raw_xbe_structural_first_difference': structural_first_difference,
                'raw_xbe_structural_reference_length': structural_reference_length,
                'raw_xbe_structural_matching_non_relocation_bytes': structural_matching_bytes,
                'raw_xbe_structural_non_relocation_bytes': structural_non_relocation_bytes,
                'raw_xbe_structural_byte_accuracy': structural_byte_accuracy,
                'raw_xbe_structural_masked_bytes': structural_masked_bytes,
                'raw_xbe_structural_lower_bound': structural_lower_bound,
                'raw_xbe_structural_register_argument': structural_reg_arg,
                'raw_xbe_structural_unmasked_relocations': structural_unmasked,
                'raw_xbe_verification': structural_verification,
                'raw_xbe_verification_boundaries': structural_verification.get('boundaries'),
                'raw_xbe_verification_compiler': structural_verification.get('compiler'),
                'raw_xbe_verification_behavior': structural_verification.get('behavior'),
                'raw_xbe_verification_instruction_operands': structural_verification.get('instruction_operands'),
                'raw_xbe_verification_literal_bytes': structural_verification.get('literal_bytes'),
                'raw_xbe_aligned_status': aligned_audit.get('status'),
                'raw_xbe_aligned_method': aligned_audit.get('method'),
                'raw_xbe_aligned_lower': aligned_audit.get('byte_accuracy'),
                'raw_xbe_aligned_upper': aligned_audit.get('byte_accuracy_upper_bound'),
                'raw_xbe_aligned_matching_bytes': aligned_audit.get('matching_bytes'),
                'raw_xbe_aligned_compared_bytes': aligned_audit.get('compared_bytes'),
                'raw_xbe_aligned_stable_bytes': aligned_audit.get('stable_compared_bytes'),
                'raw_xbe_aligned_uncertain_bytes': aligned_audit.get('uncertain_relocation_bytes'),
                'raw_xbe_aligned_uncertain_relocations': aligned_audit.get('uncertain_relocations'),
                'raw_xbe_aligned_mismatched_relocations': aligned_audit.get('mismatched_relocations'),
                'raw_xbe_aligned_mismatched_relocation_bytes': aligned_audit.get('mismatched_relocation_bytes'),
                'raw_xbe_aligned_exact_instructions': aligned_audit.get('normalized_exact_instructions'),
                'raw_xbe_aligned_instruction_pairs': aligned_audit.get('aligned_instruction_pairs'),
                'raw_xbe_aligned_candidate_only': aligned_audit.get('candidate_only_instructions'),
                'raw_xbe_aligned_reference_only': aligned_audit.get('reference_only_instructions'),
                'raw_xbe_aligned_unresolved_relocations': aligned_audit.get('unresolved_relocations'),
                'raw_xbe_aligned_unpaired_relocations': aligned_audit.get('unpaired_relocations'),
                'raw_xbe_aligned_ambiguous_steps': aligned_audit.get('alignment_ambiguous_steps'),
                'raw_xbe_aligned_provisional': aligned_audit.get('accuracy_is_provisional'),
            }
            unit_funcs.append(func_entry)
            
            total_bytes += size
            if is_ported:
                ported_bytes += size
                ported_count += 1
                if match_pct is not None:
                    match_scores.append(match_pct)
                    match_weighted_sum += match_pct * size
                    match_scored_bytes += size
        
        # Build per-unit data symbol array
        unit_data = []
        for data_info in data_syms:
            addr = data_info['addr']
            addr_hex = f'0x{addr:x}'
            unit_data.append({
                'address': addr_hex,
                'name': data_info['name'],
                'decl': data_info['symbol'].decl,
            })
        data_count = len(unit_data)
        
        # Skip empty units
        if not unit_funcs and not data_syms:
            continue
        
        match_avg = round(sum(match_scores) / len(match_scores), 1) if match_scores else None
        match_weighted = round(match_weighted_sum / max(match_scored_bytes, 1), 1) if match_scored_bytes > 0 else None
        
        if match_scores:
            tracked_match_sum += sum(match_scores)
            tracked_scored_count += len(match_scores)
            tracked_match_weighted_sum += match_weighted_sum
            tracked_scored_weighted += match_scored_bytes
        
        # Per-unit equivalence summary
        equiv_tested = sum(1 for f in unit_funcs if f['equiv_coverage'] is not None)
        equiv_high_conf = sum(1 for f in unit_funcs if f['equiv_confidence'] == 'high')
        equiv_cov_sum = sum(f['equiv_coverage'] for f in unit_funcs if f['equiv_coverage'] is not None)
        equiv_avg_cov = round(equiv_cov_sum / equiv_tested, 1) if equiv_tested > 0 else None
        equiv_classes = {}
        for f in unit_funcs:
            c = f['equiv_class'] or 'uncached'
            equiv_classes[c] = equiv_classes.get(c, 0) + 1
        
        tracked_equiv_tested += equiv_tested
        tracked_equiv_high_conf += equiv_high_conf
        if equiv_tested > 0:
            tracked_equiv_avg_cov_sum += equiv_cov_sum
            tracked_equiv_cov_count += equiv_tested
        
        # Per-unit snapshot verification summary
        snap_tested = sum(1 for f in unit_funcs if f['snapshot_passed'] is not None)
        snap_passed = sum(1 for f in unit_funcs if f['snapshot_passed'] is True)
        snap_cov_sum = sum(f['snapshot_coverage'] for f in unit_funcs if f['snapshot_coverage'] is not None)
        snap_avg_cov = round(snap_cov_sum / snap_tested, 1) if snap_tested > 0 else None
        snap_high_conf = sum(1 for f in unit_funcs if f['snapshot_confidence'] == 'high')
        
        tracked_snap_tested += snap_tested
        tracked_snap_passed += snap_passed
        tracked_snap_high_conf += snap_high_conf
        if snap_tested > 0:
            tracked_snap_cov_sum += snap_cov_sum
            tracked_snap_cov_count += snap_tested

        runtime_tested = sum(1 for f in unit_funcs if f['runtime_oracle_tested'])
        runtime_passed = sum(1 for f in unit_funcs if f['runtime_oracle_passed'] is True)

        tracked_runtime_tested += runtime_tested
        tracked_runtime_passed += runtime_passed

        structural_totals = {}
        for f in unit_funcs:
            verdict = f['raw_xbe_structural_verdict']
            if verdict:
                item = structural_totals.setdefault(verdict, {'functions': 0, 'original_bytes': 0})
                item['functions'] += 1
                item['original_bytes'] += f['raw_xbe_structural_reference_length'] or 0
        structural_comparable = (structural_totals.get('structural exact', {'functions': 0})['functions'] +
                                 structural_totals.get('structural differ', {'functions': 0})['functions'])
        structural_exact = structural_totals.get('structural exact', {'functions': 0})['functions']
        structural_matching_bytes = sum(f['raw_xbe_structural_matching_non_relocation_bytes'] or 0
                                        for f in unit_funcs)
        structural_non_relocation_bytes = sum(f['raw_xbe_structural_non_relocation_bytes'] or 0
                                              for f in unit_funcs)
        aligned_functions = [f for f in unit_funcs if f['raw_xbe_aligned_status'] == 'scored']
        aligned_matching = sum(f['raw_xbe_aligned_matching_bytes'] or 0 for f in aligned_functions)
        aligned_compared = sum(f['raw_xbe_aligned_compared_bytes'] or 0 for f in aligned_functions)
        aligned_uncertain = sum(f['raw_xbe_aligned_uncertain_bytes'] or 0 for f in aligned_functions)
        aligned_mismatched = sum(f['raw_xbe_aligned_mismatched_relocations'] or 0
                                 for f in aligned_functions)
        aligned_mismatched_bytes = sum(f['raw_xbe_aligned_mismatched_relocation_bytes'] or 0
                                       for f in aligned_functions)
        structural_summary = {
            'totals': structural_totals,
            'audited_functions': sum(v['functions'] for v in structural_totals.values()),
            'function_exact_rate': (structural_exact / structural_comparable
                                    if structural_comparable else None),
            'matching_non_relocation_bytes': structural_matching_bytes,
            'non_relocation_bytes': structural_non_relocation_bytes,
            'byte_accuracy': (structural_matching_bytes / structural_non_relocation_bytes
                              if structural_non_relocation_bytes else None),
            'aligned_scored_functions': len(aligned_functions),
            'aligned_matching_bytes': aligned_matching,
            'aligned_compared_bytes': aligned_compared,
            'aligned_uncertain_bytes': aligned_uncertain,
            'aligned_mismatched_relocations': aligned_mismatched,
            'aligned_mismatched_relocation_bytes': aligned_mismatched_bytes,
            'aligned_byte_accuracy_lower': (aligned_matching / aligned_compared
                                            if aligned_compared else None),
            'aligned_byte_accuracy_upper': ((aligned_matching + aligned_uncertain) /
                                            aligned_compared if aligned_compared else None),
            'aligned_provisional_functions': sum(
                1 for f in aligned_functions if f['raw_xbe_aligned_provisional']),
        }

        unit_source_path = _source_path(source)
        # Scoreable = the bounds table bounds at least one of this unit's
        # functions.  An empty table (unreadable/absent) must not badge every
        # unit as unscoreable, so fall back to "assume scoreable" in that case.
        unit_scoreable = (not scoreable_addrs) or any(
            f['addr'] in scoreable_addrs for f in funcs)

        unit = {
            'name': obj_name.replace('.obj', ''),
            'synthetic': synthetic,
            'source_path': unit_source_path,
            'functions': sorted(unit_funcs, key=lambda x: x['address']),
            'data': sorted(unit_data, key=lambda x: x['address']),
            'summary': {
                'total': len(unit_funcs),
                'ported': ported_count,
                'percent': round(ported_count / len(unit_funcs) * 100, 2) if unit_funcs else 0,
                'bytes_total': total_bytes,
                'bytes_ported': ported_bytes,
                'bytes_percent': round(ported_bytes / total_bytes * 100, 2) if total_bytes else 0,
                'match_avg': match_avg,
                'match_weighted': match_weighted,
                # Match percentage is only meaningful for the functions that have
                # a score. Keep its coverage explicit so consumers do not mistake
                # a partial VC71 snapshot for whole-TU mnemonic-score coverage.
                'match_scored_count': len(match_scores),
                'match_scored_bytes': match_scored_bytes,
                'match_coverage_percent': round(match_scored_bytes / ported_bytes * 100, 2) if ported_bytes else None,
                # Canonical name; `has_delinked_ref` is the legacy alias kept for
                # existing consumers (tools/report/progress_server.py) and now
                # carries the same bounds-derived meaning.
                'vc71_scoreable': unit_scoreable,
                'has_delinked_ref': unit_scoreable,
            },
            'data_summary': {
                'total': data_count,
            },
            'equivalence': {
                'tested': equiv_tested,
                'high_confidence': equiv_high_conf,
                'avg_coverage': equiv_avg_cov,
                'classes': equiv_classes,
            },
            'snapshot': {
                'tested': snap_tested,
                'passed': snap_passed,
                'avg_coverage': snap_avg_cov,
                'high_confidence': snap_high_conf,
            },
            'raw_xbe_structural': structural_summary,
            'runtime_oracle': {
                'tested': runtime_tested,
                'passed': runtime_passed,
            }
        }
        units.append(unit)
    
    # Compute overall match stats
    overall_match = None
    if tracked_scored_count > 0:
        overall_match = {
            'average': round(tracked_match_sum / tracked_scored_count, 1),
            'weighted': round(tracked_match_weighted_sum / max(tracked_scored_weighted, 1), 1),
            'scored_count': tracked_scored_count,
        }
    
    # Compute overall equivalence summary
    overall_equiv = None
    if tracked_equiv_tested > 0:
        overall_equiv = {
            'tested': tracked_equiv_tested,
            'high_confidence': tracked_equiv_high_conf,
            'avg_coverage': round(tracked_equiv_avg_cov_sum / tracked_equiv_cov_count, 1) if tracked_equiv_cov_count > 0 else None,
        }
    
    # Compute overall snapshot summary
    overall_snapshot = None
    if tracked_snap_tested > 0:
        overall_snapshot = {
            'tested': tracked_snap_tested,
            'passed': tracked_snap_passed,
            'high_confidence': tracked_snap_high_conf,
            'avg_coverage': round(tracked_snap_cov_sum / tracked_snap_cov_count, 1) if tracked_snap_cov_count > 0 else None,
        }
    
    # Sort by ported percentage (most complete first)
    units.sort(key=lambda x: x['summary']['percent'], reverse=True)
    overall_runtime_oracle = None
    if tracked_runtime_tested > 0:
        overall_runtime_oracle = {
            'tested': tracked_runtime_tested,
            'passed': tracked_runtime_passed,
        }

    return units, drift, overall_match, overall_equiv, overall_snapshot, overall_runtime_oracle


def _load_vc71_doc(path: str) -> dict:
    """Load a VC71 score document, or {} if absent/unreadable."""
    if not os.path.exists(path):
        return {}
    try:
        with open(path) as f:
            return json.load(f)
    except (OSError, ValueError):
        return {}


def _mnemonic_score_entries(document: dict) -> dict:
    """Accept legacy mnemonic documents, but never import byte measurements."""
    metric = 'mnemonic_similarity_not_raw_bytes'
    if document.get('metric') not in (None, metric) or 'records' in document:
        raise ValueError('Mnemonic score input contains a different metric; raw bytes belong in raw_xbe_structural')
    entries = document.get('scores') or {}
    if any(entry.get('metric') not in (None, metric) for entry in entries.values()):
        raise ValueError('Mnemonic score entry contains a different metric; raw bytes cannot populate match_percent')
    return entries


def merge_vc71_scores(floor_doc: dict, current_doc: dict,
                      current_exists: bool = True) -> dict:
    """Layer the honest current snapshot over the committed floor, PER FUNCTION.

    The floor (vc71_scores.json) is raise-only and tracked in git, so it always
    has an entry for every function ever measured.  The current snapshot
    (vc71_current.json) is present truth but only ever covers what its last
    `populate` pass measured -- one TU for a dashboard button, one shard for a
    sharded re-baseline.

    Picking one file wholesale, as this used to, means a partial current
    snapshot blanks every function outside it.  Merging keeps current where it
    measured and the floor everywhere else, which is the only combination that
    is both present-truth and complete.
    """
    floor = _mnemonic_score_entries(floor_doc)
    current = _mnemonic_score_entries(current_doc)
    merged = dict(floor)
    merged.update(current)
    if current_exists and not current and merged:
        print(
            'WARNING: vc71_current.json has no scores; every match% comes from '
            f'the committed floor vc71_scores.json ({len(merged)} scores)',
            file=sys.stderr,
        )
    elif current and len(current) < len(merged):
        # Normal after a scoped refresh; say so, so nobody reads a mixed
        # dashboard as one coherent measurement.
        print(f'VC71 mnemonic scores: {len(current)} current + '
              f'{len(merged) - len(current)} from the committed floor',
              file=sys.stderr)
    return {'version': current_doc.get('version') or floor_doc.get('version'),
            'metric': 'mnemonic_similarity_not_raw_bytes',
            'scores': merged}


def generate_report(output_path: str) -> dict:
    """Generate full decomp.dev-compatible report."""
    
    root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '../..'))
    cache_path = os.path.join(root_dir, 'build', 'function_sizes.json')
    # VC71 mnemonic scores come from two files, merged PER FUNCTION: the committed floor
    # (vc71_scores.json, a raise-only high-water tripwire, always present in git)
    # underneath, and the honest current snapshot (vc71_current.json, gated on
    # reference validity, gitignored) layered on top.
    #
    # It used to be a whole-file pick: prefer current, fall back to floor only
    # when current was entirely empty.  That makes a PARTIAL current snapshot
    # shadow the entire floor -- any `populate --source X` (one TU) or a run that
    # died after its first shard leaves a current file holding a handful of
    # functions, and every other function on the dashboard renders a bare "—"
    # despite a perfectly good floor score.  The empty-file case was already
    # known (gh-pages 2026-07-09: all 3474 functions blank while the tracked
    # floor held 3689 real scores); the partial case is the same bug one step
    # short of total.  Merging per function removes the whole class: current wins
    # where it measured, floor covers everything it did not.
    vc71_current = os.path.join(root_dir, 'tools', 'verify', 'vc71_current.json')
    vc71_floor = os.path.join(root_dir, 'tools', 'verify', 'vc71_scores.json')
    bounds_path = os.path.join(root_dir, 'tools', 'verify', 'function_bounds.json')
    leaf_cache_path = os.path.join(root_dir, 'tools', 'equivalence', 'leaf_cache.json')
    validity_path = os.path.join(root_dir, 'artifacts', 'audit', 'reference_validity.json')
    raw_xbe_path = os.path.join(root_dir, 'halo-patched', 'cachebeta.xbe')
    
    # Load knowledge base
    kb = KnowledgeBase.deserialize()
    store = MetadataStore(kb)
    store.load()
    
    # Load function sizes
    function_cache = load_function_sizes(cache_path)
    _fill_sizes_from_bounds(function_cache, bounds_path)

    # Load VC71 match scores: floor first, current layered over it (see above).
    floor_doc = _load_vc71_doc(vc71_floor)
    current_doc = _load_vc71_doc(vc71_current)
    vc71_scores = merge_vc71_scores(
        floor_doc, current_doc, current_exists=os.path.exists(vc71_current))

    # Load equivalence leaf cache
    leaf_cache = {}
    if os.path.exists(leaf_cache_path):
        with open(leaf_cache_path) as f:
            leaf_cache = json.load(f)
    
    # Load snapshot verification results
    snapshot_data = _load_snapshot_data()

    # Load runtime-oracle verification results
    runtime_oracle_data = _load_runtime_oracle_data()

    # Strict literal raw-byte results are separate evidence from VC71 mnemonic
    # similarity and behavioral verification. Only fresh provenance-valid audits
    # are loaded, so the dashboard never presents stale exactness as current.
    raw_byte_audits = _load_raw_byte_audits(root_dir)
    raw_byte_summary = _load_raw_byte_summary(root_dir)
    raw_xbe_structural_audits = _load_raw_xbe_structural_audits(root_dir)
    raw_xbe_structural_summary = _raw_xbe_structural_totals(raw_xbe_structural_audits)

    # Addresses the committed VC71 bounds table can derive a reference for. This
    # is what makes VC71 mnemonic scoring possible; the dashboard uses it to distinguish
    # "scoreable, not yet run" from "not scoreable (VC71 impossible)".
    scoreable_addrs = _load_scoreable_addrs(root_dir)

    # Load equivalence pass/fail verdicts (correctness, distinct from coverage)
    equiv_verdicts = _load_equiv_verdicts(root_dir)

    # Load the VC71 attention queue: functions not scored, with a state
    # (compile_failed | no_reference).  Lets the dashboard show a distinct badge
    # instead of a blank "—" that is indistinguishable from "not yet run".
    validity_data = {}
    if os.path.exists(validity_path):
        try:
            with open(validity_path) as f:
                validity_data = json.load(f)
        except (OSError, ValueError):
            validity_data = {}

    # Compute unit stats
    units, drift, overall_match, overall_equiv, overall_snapshot, overall_runtime_oracle = compute_unit_stats(
        kb, store, function_cache, vc71_scores, leaf_cache, snapshot_data, runtime_oracle_data,
        scoreable_addrs=scoreable_addrs,
        equiv_verdicts=equiv_verdicts,
        validity_data=validity_data,
        raw_byte_audits=raw_byte_audits,
        raw_xbe_structural_audits=raw_xbe_structural_audits,
    )
    # Re-scope raw structural totals to current implemented game functions.
    # Artifact discovery alone can include stale, non-ported, or platform-unit
    # records, which must not inflate the dashboard's byte-comparison population.
    raw_xbe_structural_summary = _raw_xbe_structural_dashboard_totals(units)
    
    # Compute overall stats
    progress_units = [u for u in units if not u.get('synthetic')]
    platform_units = [u for u in units if u.get('synthetic')]
    total_funcs = sum(u['summary']['total'] for u in progress_units)
    ported_funcs = sum(u['summary']['ported'] for u in progress_units)
    total_bytes = sum(u['summary']['bytes_total'] for u in progress_units)
    ported_bytes = sum(u['summary']['bytes_ported'] for u in progress_units)
    total_data_syms = sum(u['data_summary']['total'] for u in progress_units)
    platform_funcs = sum(u['summary']['total'] for u in platform_units)
    platform_ported = sum(u['summary']['ported'] for u in platform_units)
    
    # Get git info
    commit = 'unknown'
    commit_sha = 'unknown'
    branch = 'unknown'
    try:
        import subprocess
        result = subprocess.run(['git', 'rev-parse', '--short', 'HEAD'], 
                              capture_output=True, text=True, cwd=root_dir)
        if result.returncode == 0:
            commit = result.stdout.strip()
        result = subprocess.run(['git', 'rev-parse', 'HEAD'],
                              capture_output=True, text=True, cwd=root_dir)
        if result.returncode == 0:
            commit_sha = result.stdout.strip()
        result = subprocess.run(['git', 'rev-parse', '--abbrev-ref', 'HEAD'],
                              capture_output=True, text=True, cwd=root_dir)
        if result.returncode == 0:
            branch = result.stdout.strip()
    except:
        pass

    report_provenance = build_report_provenance(
        root_dir,
        raw_xbe_path,
        bounds_path,
        _load_vc71_doc(bounds_path),
        vc71_floor,
        floor_doc,
        vc71_current,
        current_doc,
        validity_path,
        validity_data,
        equiv_verdicts,
    )
    
    report = {
        'project': {
            'name': 'halo-ce-xbox',
            'display_name': 'Halo: Combat Evolved (Xbox)',
            'version': '01.10.12.2276',
            'total_units': len(units),
            'total_functions': total_funcs,
            'total_data_symbols': total_data_syms,
        },
        'summary': {
            'data_symbols': {
                'total': total_data_syms,
            },
            'functions': {
                'total': total_funcs,
                'ported': ported_funcs,
                'percent': round(ported_funcs / total_funcs * 100, 2) if total_funcs else 0
            },
            'bytes': {
                'total': total_bytes,
                'ported': ported_bytes,
                'percent': round(ported_bytes / total_bytes * 100, 2) if total_bytes else 0
            },
            'match': overall_match,
            'equivalence': overall_equiv,
            'snapshot': overall_snapshot,
            'runtime_oracle': overall_runtime_oracle,
            'raw_byte_audit': raw_byte_summary,
            'raw_xbe_structural': raw_xbe_structural_summary,
            'platform': {
                'units': len(platform_units),
                'functions': platform_funcs,
                'ported': platform_ported,
            },
        },
        'meta': {
            'timestamp': datetime.now().astimezone().isoformat(),
            'commit': commit,
            'commit_sha': commit_sha,
            'branch': branch,
            'tool_version': '1.0.0'
        },
        'provenance': report_provenance,
        'consistency': {
            'ported_source_of_truth': 'kb.json',
            'kb_ported_missing_meta': drift['kb_ported_missing_meta'],
            'meta_ported_missing_kb': drift['meta_ported_missing_kb']
        }
    }
    
    # Include per-unit breakdown with per-function data
    report['units'] = units
    
    # Write output
    os.makedirs(os.path.dirname(output_path) if os.path.dirname(output_path) else '.', exist_ok=True)
    write_json_atomic(output_path, report, indent=2)
    
    return report


def generate_html(report: dict, output_path: str, history_path: str = None):
    """Generate a client-rendered HTML dashboard with VC71 mnemonic scores and SSE live updates."""

    import json

    report_json = json.dumps(report)

    history_data = None
    if history_path and os.path.exists(history_path):
        try:
            with open(history_path) as f:
                history_data = json.load(f)
        except Exception:
            pass
    history_json = json.dumps(history_data) if history_data else 'null'

    # Load CI summary (written by generate_ci_status.py before this runs)
    ci_summary_data = None
    ci_summary_path = os.path.join(
        os.path.dirname(output_path) if os.path.dirname(output_path) else '.', 'ci_summary.json')
    if os.path.exists(ci_summary_path):
        try:
            with open(ci_summary_path) as f:
                ci_summary_data = json.load(f)
        except Exception:
            pass
    ci_summary_json = json.dumps(ci_summary_data) if ci_summary_data else 'null'

    TEMPLATE = '''<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Halo CE Xbox - Decompilation Progress</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.1/dist/chart.umd.min.js">
    </script>
    <style>
        :root {
            --bg-primary: #0d1117;
            --bg-secondary: #161b22;
            --bg-tertiary: #21262d;
            --border: #30363d;
            --text-primary: #c9d1d9;
            --text-secondary: #8b949e;
            --accent-blue: #58a6ff;
            --accent-green: #238636;
            --accent-yellow: #d29922;
            --accent-orange: #d4760a;
            --accent-red: #da3633;
            --live-pulse: #3fb950;
        }
        * { box-sizing: border-box; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            margin: 0; padding: 0;
            background: var(--bg-primary);
            color: var(--text-primary);
            line-height: 1.6;
        }
        .container {
            max-width: 1400px; margin: 0 auto; padding: 20px;
        }
        header {
            text-align: center; padding: 36px 0 20px;
            border-bottom: 1px solid var(--border); margin-bottom: 24px;
        }
        .header-row {
            display: flex; align-items: center; justify-content: center; gap: 16px;
            flex-wrap: wrap;
        }
        h1 {
            color: var(--accent-blue); margin: 0;
            font-size: 2.2em; font-weight: 700;
        }
        .subtitle {
            color: var(--text-secondary); font-size: 1.05em; margin-top: 6px;
        }
        .live-badge {
            display: inline-flex; align-items: center; gap: 6px;
            padding: 4px 14px; border-radius: 20px;
            font-size: 0.78em; font-weight: 700; text-transform: uppercase;
            letter-spacing: 0.5px; border: 1px solid var(--border);
        }
        .live-badge.online {
            background: rgba(63, 185, 80, 0.1); color: var(--live-pulse);
            border-color: rgba(63, 185, 80, 0.3);
        }
        .live-badge.offline {
            background: rgba(139, 148, 158, 0.1); color: var(--text-secondary);
            border-color: var(--border);
        }
        .live-badge .dot {
            width: 8px; height: 8px; border-radius: 50%;
            background: currentColor;
        }
        .live-badge.online .dot {
            animation: pulse 2s ease-in-out infinite;
        }
        @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.3; }
        }
        h2 {
            color: var(--accent-blue); margin-top: 36px; margin-bottom: 16px;
            padding-bottom: 8px; border-bottom: 1px solid var(--border);
            font-size: 1.3em;
        }
        .summary {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(260px, 1fr));
            gap: 16px; margin: 20px 0;
        }
        .card {
            background: var(--bg-secondary); border: 1px solid var(--border);
            border-radius: 12px; padding: 22px;
            transition: transform 0.15s, box-shadow 0.15s;
        }
        .card:hover {
            transform: translateY(-1px);
            box-shadow: 0 4px 16px rgba(0,0,0,0.25);
        }
        .stat-value {
            font-size: 2.2em; font-weight: 700;
            color: var(--accent-blue); margin: 8px 0 4px;
        }
        .stat-label {
            color: var(--text-secondary); font-size: 0.9em;
            text-transform: uppercase; letter-spacing: 0.5px; font-weight: 600;
        }
        .stat-sub {
            color: var(--accent-yellow); font-size: 0.85em; margin-top: 8px;
        }
        .progress-bar {
            width: 100%; height: 22px; background: var(--bg-tertiary);
            border-radius: 11px; overflow: hidden; margin-top: 12px;
            position: relative;
        }
        .progress-fill {
            height: 100%;
            background: linear-gradient(90deg, var(--accent-green), #2ea043);
            border-radius: 11px; transition: width 0.6s ease;
            display: flex; align-items: center; justify-content: flex-end;
            padding-right: 8px; min-width: 40px;
        }
        .progress-text {
            color: #fff; font-weight: 700; font-size: 0.8em;
        }
        .charts-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(420px, 1fr));
            gap: 16px; margin: 16px 0;
        }
        .chart-container {
            background: var(--bg-secondary); border: 1px solid var(--border);
            border-radius: 12px; padding: 20px; height: 360px;
            position: relative;
        }
        .chart-container.chart-full {
            grid-column: 1 / -1;
        }
        .chart-title {
            color: var(--text-secondary); font-size: 0.85em;
            text-transform: uppercase; letter-spacing: 0.5px;
            margin-bottom: 12px; font-weight: 600;
        }
        .table-controls {
            display: flex; align-items: center; gap: 12px;
            margin-bottom: 16px; flex-wrap: wrap;
        }
        .search-wrapper {
            position: relative; flex: 1; min-width: 200px; max-width: 400px;
        }
        .search-wrapper input {
            width: 100%; padding: 10px 14px 10px 36px;
            background: var(--bg-secondary); border: 1px solid var(--border);
            border-radius: 8px; color: var(--text-primary);
            font-size: 0.9em; outline: none; transition: border-color 0.2s;
        }
        .search-wrapper input:focus {
            border-color: var(--accent-blue);
        }
        .search-wrapper input::placeholder {
            color: var(--text-secondary); opacity: 0.7;
        }
        .search-icon {
            position: absolute; left: 12px; top: 50%;
            transform: translateY(-50%);
            color: var(--text-secondary); font-size: 0.9em;
            pointer-events: none;
        }
        .search-count {
            color: var(--text-secondary); font-size: 0.85em;
            white-space: nowrap;
        }
        .table-wrap {
            overflow-x: auto;
            border: 1px solid var(--border);
            border-radius: 12px;
        }
        table {
            width: 100%; border-collapse: collapse;
            background: var(--bg-secondary);
        }
        th {
            background: var(--bg-tertiary); color: var(--text-secondary);
            font-weight: 600; font-size: 0.82em;
            text-transform: uppercase; letter-spacing: 0.5px;
            padding: 12px 14px; text-align: left;
            cursor: pointer; user-select: none;
            position: sticky; top: 0; z-index: 1;
            white-space: nowrap;
        }
        th:hover { background: #30363d; }
        th .sort-arrow { color: var(--text-secondary); font-size: 0.8em; margin-left: 4px; }
        td {
            padding: 10px 14px; border-bottom: 1px solid var(--border);
            font-size: 0.9em;
        }
        tr:last-child td { border-bottom: none; }
        tr:hover { background: rgba(88, 166, 255, 0.04); }
        .unit-name {
            font-family: 'SF Mono', 'Cascadia Code', monospace;
            color: var(--accent-blue); font-weight: 500; font-size: 0.88em;
        }
        .source-path {
            color: var(--text-secondary); font-size: 0.82em; max-width: 280px;
            overflow: hidden; text-overflow: ellipsis; white-space: nowrap;
        }
        .num { text-align: right; font-variant-numeric: tabular-nums; }
        .progress-cell {
            display: flex; align-items: center; gap: 8px; min-width: 120px;
        }
        .mini-bar {
            flex: 1; height: 7px; background: var(--bg-tertiary);
            border-radius: 4px; overflow: hidden; max-width: 80px;
        }
        .mini-fill {
            height: 100%; background: var(--accent-blue);
            border-radius: 4px; transition: width 0.4s ease;
        }
        .pct-complete { color: #3fb950; font-weight: 700; }
        .pct-partial { color: var(--accent-blue); font-weight: 600; }
        .pct-none { color: var(--text-secondary); }
        /* Advisory operand-normalized score shown beside the VC71 mnemonic match %. */
        .opnd-pct { color: var(--text-secondary); font-size: 0.85em; opacity: 0.8; }
        .match-indicator {
            display: inline-flex; align-items: center; gap: 6px;
        }
        .match-dot {
            width: 8px; height: 8px; border-radius: 50%; flex-shrink: 0;
        }
        /* Mnemonic-match color is a structural heat scale, NOT a raw-byte or
           correctness verdict — VC71 has ceilings (SEH ~55%, fastcall, x87
           ~15pp). The behavioral verdict is the Verified column. */
        .match-dot.neutral { background: var(--text-secondary); }
        .match-dot.high { background: var(--accent-green); }
        .match-dot.ok { background: var(--accent-blue); }
        .match-dot.warn { background: var(--accent-yellow); }
        .match-dot.low { background: var(--accent-red); }
        .meta {
            color: var(--text-secondary); font-size: 0.82em;
            margin-top: 32px; padding-top: 16px;
            border-top: 1px solid var(--border); text-align: center;
        }
        @media (max-width: 768px) {
            h1 { font-size: 1.6em; }
            .summary { grid-template-columns: 1fr; }
            .charts-grid { grid-template-columns: 1fr; }
            .chart-container { height: 280px; }
            .search-wrapper { max-width: none; }
            th, td { padding: 8px 10px; font-size: 0.82em; }
            .source-path { max-width: 120px; }
        }
        /* ===== VIEW ROUTING ===== */
        .view { display: none; }
        .view.active { display: block; }
        .back-btn {
            display: inline-flex; align-items: center; gap: 6px;
            color: var(--accent-blue); cursor: pointer; font-size: 0.9em;
            padding: 6px 12px; border-radius: 6px;
            border: 1px solid var(--border); background: var(--bg-secondary);
            text-decoration: none; margin-bottom: 16px;
        }
        .back-btn:hover {
            background: var(--bg-tertiary);
        }
        .unit-title {
            font-family: 'SF Mono', 'Cascadia Code', monospace;
            font-size: 1.4em; color: var(--accent-blue); font-weight: 700;
        }
        .unit-meta-row {
            display: flex; gap: 24px; flex-wrap: wrap;
            margin: 12px 0 20px; font-size: 0.9em;
        }
        .unit-meta-item {
            color: var(--text-secondary);
        }
        .unit-meta-item strong {
            color: var(--text-primary);
        }
        .func-status {
            display: inline-block; padding: 2px 8px; border-radius: 4px;
            font-size: 0.78em; font-weight: 600; text-transform: uppercase;
            letter-spacing: 0.3px;
        }
        .func-status.ported {
            background: rgba(35, 134, 54, 0.15); color: #3fb950;
        }
        .func-status.unported {
            background: rgba(210, 153, 34, 0.15); color: var(--accent-yellow);
        }
        .func-name {
            font-family: 'SF Mono', 'Cascadia Code', monospace;
            font-size: 0.88em;
        }
        .func-address {
            font-family: 'SF Mono', 'Cascadia Code', monospace;
            font-size: 0.82em; color: var(--text-secondary);
        }
        td.num .match-dot {
            display: inline-block;
            width: 7px; height: 7px; border-radius: 50%; margin-right: 5px;
            vertical-align: middle;
        }
        .unit-name-link {
            cursor: pointer; color: var(--accent-blue); text-decoration: none;
        }
        .unit-name-link:hover {
            text-decoration: underline;
        }
        .detail-chart-container {
            background: var(--bg-secondary); border: 1px solid var(--border);
            border-radius: 12px; padding: 20px; height: 260px;
            margin-bottom: 20px; position: relative;
        }
        /* ===== EQUIVALENCE ===== */
        .equiv-badge {
            display: inline-block; padding: 2px 7px; border-radius: 4px;
            font-size: 0.72em; font-weight: 600; text-transform: uppercase;
            letter-spacing: 0.3px;
        }
        .equiv-badge.leaf { background: rgba(88, 166, 255, 0.15); color: var(--accent-blue); }
        .equiv-badge.non_leaf { background: rgba(139, 148, 158, 0.15); color: var(--text-secondary); }
        .equiv-badge.stubbable { background: rgba(210, 153, 34, 0.15); color: var(--accent-yellow); }
        .equiv-badge.data_only { background: rgba(212, 118, 10, 0.15); color: var(--accent-orange); }
        .equiv-badge.uncached { background: transparent; color: var(--text-secondary); opacity: 0.5; }
        .equiv-confidence {
            display: inline-block; padding: 2px 7px; border-radius: 4px;
            font-size: 0.72em; font-weight: 600; text-transform: uppercase;
            letter-spacing: 0.3px;
        }
        .equiv-confidence.high { background: rgba(35, 134, 54, 0.15); color: #3fb950; }
        .equiv-confidence.moderate { background: rgba(210, 153, 34, 0.15); color: var(--accent-yellow); }
        .equiv-confidence.weak { background: rgba(218, 54, 51, 0.15); color: var(--accent-red); }
        .equiv-confidence.runtime { background: rgba(31, 111, 235, 0.18); color: #79c0ff; }
        .equiv-cov-bar {
            display: inline-flex; align-items: center; gap: 5px;
            width: 80px;
        }
        .equiv-cov-track {
            flex: 1; height: 5px; background: var(--bg-tertiary);
            border-radius: 3px; overflow: hidden;
        }
        .equiv-cov-fill {
            height: 100%; border-radius: 3px;
            transition: width 0.3s ease;
        }
        .equiv-cov-fill.high { background: #3fb950; }
        .equiv-cov-fill.ok { background: var(--accent-blue); }
        .equiv-cov-fill.warn { background: var(--accent-yellow); }
        .equiv-cov-fill.low { background: var(--accent-red); }
        .equiv-cov-text {
            font-size: 0.78em; font-variant-numeric: tabular-nums; min-width: 32px;
        }
        /* ===== VERIFICATION COVERAGE SECTION ===== */
        .verif-two-col {
            display: grid; grid-template-columns: 1fr 1fr; gap: 16px; margin-bottom: 16px;
        }
        @media (max-width: 860px) { .verif-two-col { grid-template-columns: 1fr; } }
        .funnel-row {
            display: flex; align-items: center; gap: 10px; margin-bottom: 9px;
        }
        .funnel-label {
            color: var(--text-secondary); font-size: 0.78em; min-width: 110px;
            text-align: right; font-weight: 600; text-transform: uppercase; letter-spacing: 0.3px;
        }
        .funnel-bar-track {
            flex: 1; height: 18px; background: var(--bg-tertiary);
            border-radius: 4px; overflow: hidden; position: relative;
        }
        .funnel-bar-fill {
            height: 100%; border-radius: 4px; display: flex; align-items: center;
            padding-left: 7px; font-size: 0.72em; font-weight: 700; color: rgba(255,255,255,0.9);
            min-width: 3px; transition: width 0.6s ease;
        }
        .funnel-count {
            color: var(--text-primary); font-size: 0.78em; min-width: 140px;
            font-variant-numeric: tabular-nums;
        }
        .funnel-sub { color: var(--text-secondary); }
        .tu-heatmap-grid {
            display: flex; flex-wrap: wrap; gap: 4px; margin-top: 4px;
        }
        .tu-bar-tile {
            width: 34px; height: 16px; border-radius: 3px; cursor: pointer;
            background: #21262d; position: relative; overflow: hidden;
            transition: transform 0.1s; flex-shrink: 0;
            border: 1px solid rgba(255, 255, 255, 0.05);
        }
        .tu-bar-tile:hover { transform: scale(1.6); z-index: 10; }
        .tu-bar-fill {
            position: absolute; left: 0; top: 0; bottom: 0; border-radius: 2px;
            min-width: 0;
        }
        .tu-metric-toggle { display: inline-flex; gap: 0; margin-left: 8px; vertical-align: middle; }
        .tu-metric-toggle button {
            font-size: 0.72em; padding: 2px 8px; cursor: pointer; text-transform: none; letter-spacing: 0;
            background: transparent; color: var(--text-secondary);
            border: 1px solid var(--border, #30363d);
        }
        .tu-metric-toggle button:first-child { border-radius: 4px 0 0 4px; }
        .tu-metric-toggle button:last-child { border-radius: 0 4px 4px 0; border-left: none; }
        .tu-metric-toggle button.active { background: var(--accent-blue); color: #fff; border-color: var(--accent-blue); }
        .tu-bar-tile.divergent { outline: 1px solid #f85149; outline-offset: -1px; }
        .tu-bar-tile.divergent .tu-bar-fill::after {
            content: ''; position: absolute; inset: 0;
            background-image: repeating-linear-gradient(
                -45deg,
                rgba(0, 0, 0, 0.35),
                rgba(0, 0, 0, 0.35) 3px,
                transparent 3px,
                transparent 6px
            );
            border-radius: 2px;
        }
        .tu-legend {
            display: flex; flex-wrap: wrap; gap: 10px; margin-top: 10px;
        }
        .tu-legend-item {
            display: flex; align-items: center; gap: 5px;
            font-size: 0.72em; color: var(--text-secondary);
        }
        .tu-legend-swatch { width: 16px; height: 5px; border-radius: 2px; flex-shrink: 0; }
        .tu-map-note {
            flex-basis: 100%; color: var(--text-secondary); font-size: 0.72em;
            line-height: 1.4; margin-top: 1px;
        }
        .addr-strip-wrap {
            height: 56px; background: var(--bg-tertiary); border-radius: 6px;
            overflow: hidden; position: relative; margin-top: 4px;
        }
        #addrStripCanvas { display: block; width: 100%; height: 100%; }
        .addr-legend {
            display: flex; flex-wrap: wrap; gap: 12px; margin-top: 8px;
        }
        .addr-legend-item {
            display: flex; align-items: center; gap: 5px;
            font-size: 0.72em; color: var(--text-secondary);
        }
        .addr-legend-dot { width: 10px; height: 10px; border-radius: 2px; flex-shrink: 0; }
        .priority-rank { color: var(--text-secondary); font-weight: 700; }
        .priority-reason { display: flex; flex-wrap: wrap; gap: 4px; align-items: center; }
        .priority-reason-badge {
            display: inline-block; padding: 1px 6px; border-radius: 4px;
            font-size: 0.72em; font-weight: 600; text-transform: uppercase; letter-spacing: 0.3px;
        }
        .priority-reason-badge.vc71-great { background: rgba(35,134,54,0.15); color: #3fb950; }
        .priority-reason-badge.vc71-good { background: rgba(88,166,255,0.15); color: var(--accent-blue); }
        .priority-reason-badge.leaf { background: rgba(88,166,255,0.15); color: var(--accent-blue); }
        .priority-reason-badge.large { background: rgba(210,153,34,0.15); color: var(--accent-yellow); }
        .priority-reason-badge.size { background: rgba(139,148,158,0.15); color: var(--text-secondary); }
        .copy-cmd-btn {
            padding: 2px 8px; border-radius: 4px; border: 1px solid var(--border);
            background: var(--bg-tertiary); color: var(--text-secondary); cursor: pointer;
            font-size: 0.72em; font-weight: 600; white-space: nowrap;
            transition: background 0.15s, color 0.15s;
        }
        .copy-cmd-btn:hover { background: var(--accent-blue); color: #fff; border-color: var(--accent-blue); }
        .copy-cmd-btn.copied { background: var(--accent-green); color: #fff; border-color: var(--accent-green); }
        .score-btn {
            padding: 2px 8px; border-radius: 4px; border: 1px solid var(--border);
            background: var(--bg-tertiary); color: var(--text-secondary); cursor: pointer;
            font-size: 0.72em; font-weight: 600; white-space: nowrap;
            transition: background 0.15s, color 0.15s;
        }
        .score-btn:hover:not(:disabled) { background: var(--accent-blue); color: #fff; border-color: var(--accent-blue); }
        .score-btn:disabled { opacity: 0.5; cursor: default; }
        .score-btn.error { background: #da363322; color: #f85149; border-color: #f85149; }
        /* ===== TREEMAP ===== */
        .treemap-grid {
            display: grid; grid-template-columns: 1fr 220px; gap: 16px; margin-bottom: 16px;
        }
        @media (max-width: 860px) { .treemap-grid { grid-template-columns: 1fr; } }
        .treemap-wrap {
            position: relative; height: 420px; cursor: pointer;
        }
        #treemapCanvas { display: block; width: 100%; height: 100%; }
        .treemap-tooltip {
            position: fixed; padding: 8px 12px; background: #161b22;
            border: 1px solid var(--border); border-radius: 8px;
            color: var(--text-primary); font-size: 0.82em;
            pointer-events: none; z-index: 100; display: none;
            max-width: 280px; line-height: 1.5;
        }
        .donut-center {
            display: flex; flex-direction: column; align-items: center;
            justify-content: center;
        }
        .donut-pct {
            font-size: 1.8em; font-weight: 700; color: var(--accent-blue);
            margin-top: 8px;
        }
        .donut-label {
            color: var(--text-secondary); font-size: 0.82em; margin-top: 4px;
        }
        @media (prefers-reduced-motion: reduce) {
            .progress-fill, .live-badge.online .dot { animation: none; transition: none; }
        }
    </style>
</head>
<body>
    <div class="container">

        <!-- ===== OVERVIEW ===== -->
        <div id="view-overview" class="view active">
            <header>
                <div class="header-row">
                    <h1>Halo: Combat Evolved (Xbox)</h1>
                    <span class="live-badge offline" id="live-badge">
                        <span class="dot"></span>
                        <span id="live-text">Static</span>
                    </span>
                </div>
                <div class="subtitle">Decompilation Progress Dashboard</div>
            </header>

            <div class="summary" id="summary-cards"></div>

            <div class="charts-grid" id="charts-grid" style="margin-top:16px;margin-bottom:24px">
                <div class="chart-container" id="charts-progress-container" style="height:240px">
                    <div class="chart-title">Functions Ported Over Time</div>
                    <canvas id="progressChart"></canvas>
                </div>
                <div class="chart-container" id="charts-accuracy-container" style="height:240px">
                    <div class="chart-title" id="accuracyChartTitle">Match Over Time</div>
                    <canvas id="accuracyChart"></canvas>
                </div>
            </div>

            <h2>Behavioral Evidence</h2>
            <div class="verif-two-col">
                <div class="card">
                    <div class="chart-title">Behavioral Checks</div>
                    <div id="verif-funnel"></div>
                </div>
                <div class="card">
                    <div class="chart-title">Unit Evidence Map &mdash; <span style="font-weight:400;text-transform:none;letter-spacing:0">bar length = implemented, color = selected metric; click a tile to open unit</span><span class="tu-metric-toggle" id="tu-metric-toggle"></span></div>
                    <div class="tu-heatmap-grid" id="tu-heatmap"></div>
                    <div class="tu-legend" id="tu-legend"></div>
                </div>
            </div>
            <h2>Translation Units</h2>
            <div class="treemap-grid">
                <div class="card">
                    <div class="chart-title">Unit Size Map &mdash; <span style="font-weight:400;text-transform:none;letter-spacing:0">sized by bytes, colored by selected metric &mdash; click to drill down</span><span class="tu-metric-toggle" id="treemap-metric-toggle"></span></div>
                    <div class="treemap-wrap"><canvas id="treemapCanvas"></canvas></div>
                </div>
                <div class="card donut-center">
                    <canvas id="overviewDonut" width="160" height="160" style="max-width:160px;max-height:160px"></canvas>
                    <div class="donut-pct" id="donut-pct"></div>
                    <div class="donut-label" id="donut-label"></div>
                </div>
            </div>
            <div id="treemap-tooltip" class="treemap-tooltip"></div>



            <h2>Per-Unit Breakdown</h2>
            <div class="table-controls">
                <div class="search-wrapper">
                    <span class="search-icon">&#x1F50D;</span>
                    <input type="text" id="unit-search" placeholder="Filter units by name or path..." autocomplete="off">
                </div>
                <span class="search-count" id="search-count"></span>
            </div>
            <div class="table-wrap">
                <table>
                    <thead>
                        <tr>
                            <th data-col="0" title="Object or source group. Select it to see its functions.">Unit Name <span class="sort-arrow"></span></th>
                            <th data-col="1" title="Source file for this unit.">Source Path <span class="sort-arrow"></span></th>
                            <th data-col="2" class="num" title="Total functions in this unit.">Functions <span class="sort-arrow"></span></th>
                            <th data-col="3" class="num" title="Functions implemented in our code.">Ported <span class="sort-arrow"></span></th>
                            <th data-col="4" class="num" title="Share of functions implemented.">Progress <span class="sort-arrow"></span></th>
                            <th data-col="5" class="num" title="Implemented bytes / total bytes.">Bytes <span class="sort-arrow"></span></th>
                            <th data-col="6" class="num" title="How closely the instruction names and order match the original. Exact values are ignored.">Mnemonic % <span class="sort-arrow"></span></th>
                        </tr>
                    </thead>
                    <tbody id="table-body"></tbody>
                </table>
            </div>

            <div class="card" style="margin-bottom:16px">
                <div class="chart-title">Binary Address Space &mdash; <span style="font-weight:400;text-transform:none;letter-spacing:0">every function plotted at its real address</span></div>
                <div class="addr-strip-wrap"><canvas id="addrStripCanvas"></canvas></div>
                <div class="addr-legend" id="addr-legend"></div>
            </div>

            <div class="meta" id="meta"></div>
        </div>

        <!-- ===== UNIT DETAIL ===== -->
        <div id="view-detail" class="view">
            <header>
                <div class="header-row">
                    <h1>Halo: Combat Evolved (Xbox)</h1>
                    <span class="live-badge offline" id="live-badge-detail">
                        <span class="dot"></span>
                        <span id="live-text-detail">Static</span>
                    </span>
                </div>
                <div class="subtitle">Translation Unit Detail</div>
            </header>

            <div id="detail-content">
                <a class="back-btn" href="#" id="detail-back">&#x2190; Back to Overview</a>
                <div class="unit-title" id="detail-unit-name"></div>
                <div class="unit-meta-row" id="detail-meta"></div>

                <div class="charts-grid" id="detail-charts-grid" style="margin:16px 0">
                    <div class="chart-container" id="unitHistoryContainer" style="height:240px">
                        <div class="chart-title" id="unitHistoryTitle">Unit Progress &amp; Mnemonic-Match History</div>
                        <canvas id="unitHistoryChart"></canvas>
                    </div>
                    <div class="chart-container" id="detailChartContainer" style="height:240px">
                        <div class="chart-title">Mnemonic-Match Distribution</div>
                        <canvas id="detailChart"></canvas>
                    </div>
                </div>

                    <h2>Functions</h2>
                    <div class="table-controls">
                        <div class="search-wrapper">
                            <span class="search-icon">&#x1F50D;</span>
                            <input type="text" id="func-search" placeholder="Filter functions..." autocomplete="off">
                        </div>
                        <span class="search-count" id="func-count"></span>
                    </div>
                    <div class="table-wrap">
                    <table>
                        <thead>
                            <tr>
                                <th data-fcol="0" title="Address in the original Xbox binary.">Address <span class="sort-arrow"></span></th>
                                <th data-fcol="1" title="Recovered function name.">Function Name <span class="sort-arrow"></span></th>
                                <th data-fcol="2" class="num" title="Size in the original binary.">Size <span class="sort-arrow"></span></th>
                                <th data-fcol="3" class="num" title="Whether we have implemented this function.">Status <span class="sort-arrow"></span></th>
                                <th data-fcol="4" class="num" title="How closely the instruction names and order match the original. Exact values are ignored.">Mnemonic % <span class="sort-arrow"></span></th>
                                <th data-fcol="5" class="num" title="Bytes that match after lining up instructions. A range means some address bytes are unknown. This does not prove the behavior is correct.">Aligned bytes <span class="sort-arrow"></span></th>
                                <th data-fcol="6" class="num" title="Matched the original in at least one behavioral check. Byte and mnemonic scores do not count.">Behavioral match <span class="sort-arrow"></span></th>
                            </tr>
                        </thead>
                        <tbody id="func-table-body"></tbody>
                    </table>
                    </div>
                </div>

                <!-- Data Symbols -->
                <div id="data-section" style="margin-top:24px;display:none;">
                    <h2>Data Symbols <span id="data-count" style="color:var(--text-secondary);font-weight:400;"></span></h2>
                    <div class="table-wrap">
                    <table>
                        <thead>
                            <tr>
                                <th>Address</th>
                                <th>Name</th>
                                <th>Declaration</th>
                            </tr>
                        </thead>
                        <tbody id="data-table-body"></tbody>
                    </table>
                    </div>
                </div>

                <div style="margin-top: 20px;">
                    <a class="back-btn" href="#" id="detail-back-bottom">&#x2190; Back to Overview</a>
                </div>

            <div class="meta" id="meta-detail"></div>
        </div>

    </div>

    <script>
        /* ===== DATA ===== */
        var REPORT = __REPORT_JSON__;
        var HISTORY = __HISTORY_JSON__;
        var CI_SUMMARY = __CI_SUMMARY_JSON__;

        /* ===== STATE ===== */
        var chartInstances = {};
        var sortCol = 4;
        var sortAsc = false;
        var filterText = '';
        var funcSortCol = -1;
        var funcSortAsc = true;
        var funcFilterText = '';
        var currentUnitName = null;
        var currentUnitHasRef = false;

        /* ===== ROUTER ===== */
        function router() {
            var hash = window.location.hash || '#';
            if (hash.indexOf('#unit/') === 0) {
                var name = decodeURIComponent(hash.slice(6));
                showDetail(name);
            } else {
                showOverview();
            }
        }

        function goToUnit(name) {
            window.location.hash = '#unit/' + encodeURIComponent(name);
        }

        function goHome() {
            window.location.hash = '#';
        }

        function showOverview() {
            document.getElementById('view-overview').classList.add('active');
            document.getElementById('view-detail').classList.remove('active');
            render();
            updateLiveBadge('overview');
        }

        function showDetail(name) {
            document.getElementById('view-overview').classList.remove('active');
            document.getElementById('view-detail').classList.add('active');
            currentUnitName = name;
            funcFilterText = '';
            funcSortCol = -1;
            funcSortAsc = true;
            var fs = document.getElementById('func-search');
            if (fs) fs.value = '';
            renderUnitDetail(name);
            updateLiveBadge('detail');
        }

        function updateLiveBadge(view) {
            var liveEl = document.getElementById(view === 'detail' ? 'live-badge-detail' : 'live-badge');
            var textEl = document.getElementById(view === 'detail' ? 'live-text-detail' : 'live-text');
            // Sync detail badge state from overview badge
            var srcBadge = document.getElementById('live-badge');
            if (view === 'detail') {
                liveEl.className = srcBadge.className;
                textEl.textContent = srcBadge.querySelector('#live-text').textContent;
            }
        }

        function fetchJsonFresh(path) {
            var sep = path.indexOf('?') === -1 ? '?' : '&';
            return fetch(path + sep + '_ts=' + Date.now(), {cache: 'no-store'})
                .then(function(r) {
                    if (!r.ok) throw new Error('HTTP ' + r.status);
                    return r.json();
                });
        }

        function hydrateLiveSnapshot() {
            return Promise.allSettled([
                fetchJsonFresh('report.json').then(function(data) { REPORT = data; }),
                fetchJsonFresh('history.json').then(function(data) { HISTORY = data; })
            ]);
        }

        /* ===== SCORE / EQUIVALENCE BUTTONS ===== */
        function rerunEquivalence(btn) {
            var functionName = btn.getAttribute('data-function');
            var address = btn.getAttribute('data-address');
            if (!functionName || !address) return;
            btn.disabled = true;
            btn.classList.remove('error');
            btn.textContent = '⏳ Equiv…';
            fetch('/api/equivalence', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({function: functionName, address: address})
            }).then(function(r) { return r.json(); }).then(function(d) {
                if (d.ok) {
                    hydrateLiveSnapshot().finally(function() { router(); });
                } else {
                    btn.disabled = false;
                    btn.classList.add('error');
                    btn.textContent = '⚠ Equiv error';
                    btn.title = d.error || 'Equivalence run failed';
                }
            }).catch(function() {
                btn.disabled = false;
                btn.classList.add('error');
                btn.textContent = '⚠ Server offline';
                btn.title = 'progress_server.py is not running';
            });
        }

        function rawAuditUnit(btn) {
            var unit = btn.getAttribute('data-unit');
            if (!unit) return;
            btn.disabled = true;
            btn.classList.remove('error');
            btn.textContent = '⏳ Auditing…';
            fetch('/api/raw-audit', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({unit: unit})
            }).then(function(r) { return r.json(); }).then(function(d) {
                if (d.ok) {
                    hydrateLiveSnapshot().finally(function() { router(); });
                } else {
                    btn.disabled = false;
                    btn.classList.add('error');
                    btn.textContent = '⚠ Audit error';
                    btn.title = d.error || 'Raw-XBE audit failed';
                }
            }).catch(function() {
                btn.disabled = false;
                btn.classList.add('error');
                btn.textContent = '⚠ Server offline';
                btn.title = 'progress_server.py is not running';
            });
        }

        /* ===== OVERVIEW RENDER ===== */
        function render() {
            renderSummary();
            renderCharts();
            renderVerifSection();
            renderTable();
            renderMeta();
        }

        // A CREDIBLE divergence = equivalence FAIL with real coverage (high/moderate
        // confidence). It is never "verified" — it is a bug candidate to investigate.
        // Low-coverage/weak fails are inconclusive (often harness artifacts), so they
        // are treated as "unknown", not flagged — symmetric with how a pass needs
        // coverage to count. NOTE: even credible fails need triage (e.g. a fail at
        // ~100% mnemonic match is almost certainly a harness artifact, not a real bug).
        function isDivergent(f) {
            // Only an actual behavioral divergence counts — emulation_error / timeout /
            // *_extract_failed are harness failures, not lift bugs, so they stay "unknown".
            return f.ported && f.equiv_status === 'fail' && f.equiv_reason === 'divergence' &&
                   (f.equiv_confidence === 'high' || f.equiv_confidence === 'moderate');
        }

        // Equivalence counts as verification only on a PASS with real coverage
        // (high/moderate confidence) or a Z3 proof. Coverage/confidence ALONE is
        // not a verdict — a divergent function can have confidence=high.
        function equivVerified(f) {
            if (f.equiv_proven && f.equiv_status === 'pass') return true;
            if (f.equiv_status === 'pass' &&
                (f.equiv_confidence === 'high' || f.equiv_confidence === 'moderate')) return true;
            return false;
        }

        function countVerified() {
            var count = 0;
            var divergent = 0;
            var byMethod = { equiv: 0, snap: 0, oracle: 0 };
            for (var ui = 0; ui < REPORT.units.length; ui++) {
                if (REPORT.units[ui].synthetic) continue;
                var funcs = REPORT.units[ui].functions || [];
                for (var fi = 0; fi < funcs.length; fi++) {
                    var f = funcs[fi];
                    if (!f.ported) continue;
                    if (isDivergent(f)) { divergent++; continue; }   // bug candidate, not verified
                    var verified = false;
                    if (equivVerified(f)) { byMethod.equiv++; verified = true; }
                    if (f.snapshot_passed === true) { byMethod.snap++; verified = true; }
                    if (f.runtime_oracle_passed === true) { byMethod.oracle++; verified = true; }
                    if (verified) count++;
                }
            }
            return { total: count, divergent: divergent, byMethod: byMethod };
        }

        function isVerified(f) {
            if (!f.ported) return false;
            if (isDivergent(f)) return false;            // divergence overrides everything
            if (equivVerified(f)) return true;
            if (f.snapshot_passed === true) return true;
            if (f.runtime_oracle_passed === true) return true;
            return false;
        }

        function renderSummary() {
            var s = REPORT.summary;
            var u = REPORT.units;
            var gameUnits = u.filter(function(unit) { return !unit.synthetic; });
            var totalUnits = gameUnits.length;
            var platform = s.platform || { units: 0, functions: 0, ported: 0 };
            var platformPct = platform.functions > 0 ? (platform.ported / platform.functions * 100) : 0;
            var platformPctStr = platformPct.toFixed(1) + '%';
            var totalAllFuncs = s.functions.total + platform.functions;
            var totalAllPorted = s.functions.ported + platform.ported;
            var totalAllPct = totalAllFuncs > 0 ? (totalAllPorted / totalAllFuncs * 100) : 0;
            var totalAllPctStr = totalAllPct.toFixed(1) + '%';
            var vData = countVerified();

            var completedUnits = gameUnits.filter(function(unit) { return unit.summary.percent === 100 && unit.summary.total > 0; }).length;
            var completedPct = Math.round(completedUnits / totalUnits * 100);

            var verifiedPct = s.functions.ported > 0 ? (vData.total / s.functions.ported * 100).toFixed(1) : '0';
            var verifiedTip = 'Functions that matched the original in at least one behavioral check.\\n';
            verifiedTip += 'Equivalence or proof: ' + vData.byMethod.equiv + '\\n';
            verifiedTip += 'Runtime comparison: ' + vData.byMethod.oracle + '\\n';
            verifiedTip += 'Snapshot comparison: ' + vData.byMethod.snap;
            if (vData.divergent > 0) verifiedTip += '\\nNeeds investigation: ' + vData.divergent;

            // Match-coverage buckets: a ported function is "scored" if it has a
            // VC71 mnemonic match, "scoreable" if the committed bounds table can derive a
            // reference for its unit (could be scored), or "not scoreable" if VC71
            // is impossible (needs behavioral verification instead). Honest scope
            // for the VC71 mnemonic-match headline.
            var mScored = 0, mScoreable = 0, mNoRef = 0;
            for (var mui = 0; mui < u.length; mui++) {
                if (u[mui].synthetic) continue;
                var su = u[mui].summary || {};
                var hasRef = !!(su.vc71_scoreable !== undefined ? su.vc71_scoreable : su.has_delinked_ref);
                var mfuncs = u[mui].functions || [];
                for (var mfi = 0; mfi < mfuncs.length; mfi++) {
                    var mf = mfuncs[mfi];
                    if (!mf.ported) continue;
                    if (mf.match_percent !== null && mf.match_percent !== undefined) mScored++;
                    else if (hasRef) mScoreable++;
                    else mNoRef++;
                }
            }
            var matchTip = 'How closely instruction names and order match the original. Exact values are ignored.\\n';
            matchTip += 'Scored: ' + mScored + ' / ' + s.functions.ported + ' ported\\n';
            matchTip += 'Not yet scored: ' + mScoreable + '\\n';
            matchTip += 'No reference available: ' + mNoRef;

            var rawAudit = s.raw_byte_audit || {};
            var rawTotals = rawAudit.totals || {};
            var rawExact = rawTotals['raw-byte exact'] || {functions: 0, original_bytes: 0};
            var rawDiffers = rawTotals['bytes differ'] || {functions: 0, original_bytes: 0};
            var rawNc = rawTotals['not comparable'] || {functions: 0, original_bytes: 0};
            // The strict literal audit no longer gets its own card: it answers the
            // same question as the byte-match card and its Exact count is folded
            // in there, so the summary shows one byte-accuracy number, not two.
            var rawCard = '';
            var structuralAudit = s.raw_xbe_structural || {};
            var alignedLower = structuralAudit.aligned_byte_accuracy_lower;
            var alignedMatching = structuralAudit.aligned_matching_bytes || 0;
            var alignedCompared = structuralAudit.aligned_compared_bytes || 0;
            var alignedUncertain = structuralAudit.aligned_uncertain_bytes || 0;
            var alignedDifferent = Math.max(alignedCompared - alignedMatching - alignedUncertain, 0);
            var alignedImplemented = structuralAudit.implemented_functions;
            if (alignedImplemented == null) alignedImplemented = structuralAudit.audited_functions || 0;
            var alignedComparedFunctions = structuralAudit.compared_functions;
            if (alignedComparedFunctions == null) alignedComparedFunctions = structuralAudit.aligned_scored_functions || 0;
            var alignedCannotCompare = structuralAudit.cannot_compare_functions || 0;
            var alignedUnchecked = structuralAudit.unchecked_functions;
            if (alignedUnchecked == null) {
                alignedUnchecked = Math.max(alignedImplemented - alignedComparedFunctions - alignedCannotCompare, 0);
            }
            var alignedScope = fmtNum(alignedComparedFunctions) + ' of ' +
                fmtNum(alignedImplemented) + ' implemented functions compared';
            var alignedCoverage = fmtNum(alignedCannotCompare) + ' cannot compare &middot; ' +
                fmtNum(alignedUnchecked) + ' unchecked';
            var structuralTip = 'Instructions are lined up before bytes are compared.\\n' +
                'Uncertain bytes have unresolved address targets.\\n' +
                'Behavior is checked separately.';
            var structuralHeadline = alignedCompared > 0 && alignedLower != null ?
                (alignedLower * 100).toFixed(1) + '% byte accuracy' : 'No comparison yet';
            var structuralBar = '';
            if (alignedCompared > 0) {
                structuralBar = '<div class="progress-bar" style="display:flex" aria-label="Byte comparison breakdown">' +
                    '<div style="width:' + (alignedMatching / alignedCompared * 100) + '%;background:#3fb950" title="Matching bytes"></div>' +
                    '<div style="width:' + (alignedDifferent / alignedCompared * 100) + '%;background:#d29922" title="Different bytes"></div>' +
                    '<div style="width:' + (alignedUncertain / alignedCompared * 100) + '%;background:#8b949e" title="Uncertain bytes"></div>' +
                '</div>';
            }
            var structuralCard = (structuralAudit.implemented_functions != null || structuralAudit.audited_functions) ?
                '<div class="card" title="' + escHtml(structuralTip) + '">' +
                    '<div class="stat-label">Byte comparison</div>' +
                    '<div class="stat-value" style="color:#58a6ff">' + structuralHeadline + '</div>' +
                    '<div class="stat-label">' + alignedScope + '</div>' +
                    '<div class="stat-label">' + alignedCoverage + '</div>' +
                    '<div class="stat-label">' + fmtNum(alignedMatching) + ' matching &middot; ' +
                    fmtNum(alignedDifferent) + ' different &middot; ' + fmtNum(alignedUncertain) + ' uncertain</div>' +
                    structuralBar +
                '</div>' : '';

            document.getElementById('summary-cards').innerHTML =
                '<div class="card" title="Implemented game functions / total game functions.">' +
                    '<div class="stat-label">Game Code Progress</div>' +
                    '<div class="stat-value">' + s.functions.percent.toFixed(1) + '%</div>' +
                    '<div class="stat-label">' + fmtNum(s.functions.ported) + ' / ' + fmtNum(s.functions.total) + ' functions</div>' +
                    '<div class="progress-bar"><div class="progress-fill" style="width:' + Math.max(s.functions.percent, 2) + '%"><span class="progress-text">' + s.functions.percent.toFixed(1) + '%</span></div></div>' +
                '</div>' +
                '<div class="card" title="Implemented platform and library functions. These are separate from game code.">' +
                    '<div class="stat-label">Platform / SDK</div>' +
                    '<div class="stat-value" style="color:#79c0ff">' + platformPctStr + '</div>' +
                    '<div class="stat-label">' + fmtNum(platform.ported) + ' / ' + fmtNum(platform.functions) + ' functions &middot; ' + fmtNum(platform.units) + ' buckets (excluded)</div>' +
                    '<div class="progress-bar"><div class="progress-fill" style="width:' + Math.max(platformPct, platform.ported > 0 ? 2 : 0) + '%;background:linear-gradient(90deg,#1f6feb,#58a6ff)"><span class="progress-text">' + platformPctStr + '</span></div></div>' +
                '</div>' +
                '<div class="card" title="Implemented functions across game code and platform libraries.">' +
                    '<div class="stat-label">Total Progress</div>' +
                    '<div class="stat-value" style="color:#d2a8ff">' + totalAllPctStr + '</div>' +
                    '<div class="stat-label">' + fmtNum(totalAllPorted) + ' / ' + fmtNum(totalAllFuncs) + ' functions total</div>' +
                    '<div class="progress-bar"><div class="progress-fill" style="width:' + Math.max(totalAllPct, 2) + '%;background:linear-gradient(90deg,#8957e5,#d2a8ff)"><span class="progress-text">' + totalAllPctStr + '</span></div></div>' +
                '</div>' +
                '<div class="card" title="Source files with every function implemented.">' +
                    '<div class="stat-label">Files Complete</div>' +
                    '<div class="stat-value">' + completedUnits + '</div>' +
                    '<div class="stat-label">' + completedUnits + ' / ' + totalUnits + ' source files fully ported</div>' +
                    '<div class="progress-bar"><div class="progress-fill" style="width:' + Math.max(completedPct, 2) + '%"><span class="progress-text">' + completedPct + '%</span></div></div>' +
                '</div>' +
                (s.match ?
                '<div class="card" title="' + escHtml(matchTip) + '">' +
                    '<div class="stat-label">VC71 Mnemonic Match <span style="opacity:.6;font-weight:400">(structural diagnostic)</span></div>' +
                    '<div class="stat-value" style="color:' + matchColor(s.match.weighted) + '">' + s.match.weighted.toFixed(1) + '%</div>' +
                    '<div class="stat-label">Size-weighted &middot; ' + fmtNum(s.match.scored_count) + ' of ' + fmtNum(s.functions.ported) + ' scored &middot; has structural ceilings</div>' +
                    '<div class="progress-bar"><div class="progress-fill" style="width:' + Math.max(s.match.weighted, 2) + '%;background:linear-gradient(90deg,var(--accent-green),#2ea043)"><span class="progress-text">' + s.match.weighted.toFixed(1) + '%</span></div></div>' +
                '</div>' : '') +
                '<div class="card" title="' + escHtml(verifiedTip) + '">' +
                    '<div class="stat-label">Behavioral matches</div>' +
                    '<div class="stat-value" style="color:#3fb950">' + fmtNum(vData.total) + '</div>' +
                    '<div class="stat-label">' + verifiedPct + '% of ported &middot; hover for breakdown</div>' +
                    (s.functions.ported > 0 ? '<div class="progress-bar"><div class="progress-fill" style="width:' + Math.max(vData.total / s.functions.ported * 100, 0.3) + '%;background:linear-gradient(90deg,#238636,#3fb950)"><span class="progress-text">' + verifiedPct + '%</span></div></div>' : '') +
                '</div>' +
                rawCard +
                structuralCard +
                renderCICards();
        }

        function renderCICards() {
            if (!CI_SUMMARY) return '';
            var ci = CI_SUMMARY;
            var out = '';

            // Card 1: CI Runs (GitHub Actions)
            var ciRuns = ci.ci_runs || {};
            var wfs = ciRuns.workflows || [];
            var anyFail = wfs.some(function(w) { return w.conclusion === 'failure'; });
            var allPass = wfs.length > 0 && wfs.every(function(w) { return w.conclusion === 'success'; });
            var ciColor = allPass ? '#3fb950' : (anyFail ? '#da3633' : '#d29922');
            var ciLabel = allPass ? 'All Pass' : (anyFail ? 'Failures' : (wfs.length ? 'Mixed' : 'No data'));
            var wfRows = wfs.slice(0, 5).map(function(w) {
                var dot = w.conclusion === 'success' ? '&#9679;' : (w.conclusion === 'failure' ? '&#9679;' : '&#9675;');
                var dotColor = w.conclusion === 'success' ? '#3fb950' : (w.conclusion === 'failure' ? '#da3633' : '#8b949e');
                var shortName = (w.name || '').replace('.github/workflows/', '');
                return '<div style="display:flex;align-items:center;gap:6px;margin-top:3px">' +
                       '<span style="color:' + dotColor + ';font-size:0.7em">' + dot + '</span>' +
                       '<span style="font-size:0.8em;color:var(--text-secondary)">' + escHtml(shortName) + '</span></div>';
            }).join('');
            var sha = ciRuns.last_commit ? ' &middot; <span style="font-family:monospace;font-size:0.85em">' + escHtml(ciRuns.last_commit) + '</span>' : '';
            out += '<a href="ci.html" class="card" style="text-decoration:none;display:block;cursor:pointer" title="Open build and test status.">' +
                    '<div class="stat-label">CI Status</div>' +
                    '<div class="stat-value" style="color:' + ciColor + '">' + ciLabel + '</div>' +
                    '<div class="stat-label">' + fmtNum(wfs.length) + ' workflow' + (wfs.length === 1 ? '' : 's') + sha + '</div>' +
                    wfRows +
                    '<div style="color:var(--accent-blue);font-size:0.78em;margin-top:8px">View full CI status &#x2192;</div>' +
                '</a>';

            // Card 2: measured coverage in the committed leaf cache. This is
            // a narrower population than behavioral verdicts in batch artifacts.
            var eq = ci.equivalence || {};
            var equivColor = (eq.avg_coverage >= 60) ? '#3fb950' : (eq.avg_coverage >= 30 ? '#58a6ff' : '#d29922');
            var highPct = eq.tested > 0 ? Math.round(eq.high_confidence / eq.tested * 100) : 0;
            out += '<a href="ci.html" class="card" style="text-decoration:none;display:block;cursor:pointer" title="Open behavioral test coverage.">' +
                    '<div class="stat-label">Equivalence Coverage</div>' +
                    '<div class="stat-value" style="color:' + equivColor + '">' + (eq.avg_coverage !== null && eq.avg_coverage !== undefined ? eq.avg_coverage.toFixed(1) + '%' : '—') + '</div>' +
                    '<div class="stat-label">average among ' + fmtNum(eq.tested) + ' measured functions</div>' +
                    '<div class="stat-sub">' + fmtNum(eq.classified || 0) + ' functions classified in the cache</div>' +
                    '<div class="stat-sub" style="margin-top:6px">' + fmtNum(eq.high_confidence) + ' high-confidence (' + highPct + '%) &middot; ' + fmtNum(eq.weak_coverage) + ' weak</div>' +
                    '<div style="color:var(--accent-blue);font-size:0.78em;margin-top:8px">View coverage details &#x2192;</div>' +
                '</a>';

            return out;
        }

        // Color scale encodes mnemonic-sequence similarity — a useful at-a-glance
        // heat scale. It is NOT a raw-byte or correctness verdict: VC71 has
        // structural ceilings (SEH ~55%, fastcall preamble, x87 ~15pp), so a low
        // mnemonic match can still accompany a perfect lift. Correctness lives in the Verified
        // column (behavioral evidence), which is the separate colored verdict.
        function matchColor(pct) {
            return scaleTextColor(pct);
        }

        function matchBadge(pct) {
            if (pct === null || pct === undefined) return 'none';
            if (pct >= 95) return 'high';
            if (pct >= 85) return 'ok';
            if (pct >= 70) return 'warn';
            return 'low';
        }

        function statusBadge(ported) {
            return ported ? '<span class="func-status ported">Ported</span>' : '<span class="func-status unported">Unported</span>';
        }

        function renderCharts() {
            var grid = document.getElementById('charts-grid');
            if (grid) grid.style.display = '';

            var hasHistory = HISTORY && HISTORY.snapshots && HISTORY.snapshots.length >= 2;
            var progContainer = document.getElementById('charts-progress-container');
            var accContainer = document.getElementById('charts-accuracy-container');

            if (!hasHistory) {
                if (grid) grid.style.display = 'none';
                destroyChart('progressChart');
                destroyChart('accuracyChart');
                return;
            }

            var snaps = HISTORY.snapshots;
            var labels = snaps.map(function(s) { return s.timestamp.slice(0, 10); });
            var funcs = snaps.map(function(s) { return s.summary.functions.ported; });

            // 1. Functions Ported chart
            destroyChart('progressChart');
            var ctx1 = document.getElementById('progressChart').getContext('2d');
            chartInstances.progressChart = new Chart(ctx1, {
                type: 'line',
                data: {
                    labels: labels,
                    datasets: [{
                        label: 'Functions Ported',
                        data: funcs,
                        borderColor: '#58a6ff',
                        backgroundColor: 'rgba(88, 166, 255, 0.08)',
                        borderWidth: 2, tension: 0.35, fill: true,
                        pointRadius: 2, pointHoverRadius: 5
                    }]
                },
                options: chartOpts()
            });

            // 2. VC71 mnemonic-match chart
            destroyChart('accuracyChart');
            var matchSnaps = [];
            for (var i = 0; i < snaps.length; i++) {
                var s = snaps[i];
                if (s.summary && s.summary.match && s.summary.match.weighted !== undefined && s.summary.match.weighted !== null) {
                    // Aligned byte accuracy covers only functions with a current
                    // raw-XBE audit; coverage is plotted beside it because a
                    // shrinking audited set moves the accuracy line on its own.
                    var rx = s.summary.raw_xbe_structural || {};
                    var alignedLower = rx.aligned_byte_accuracy_lower;
                    var auditedFns = rx.aligned_scored_functions || 0;
                    var portedFns = rx.implemented_functions ||
                        (s.summary.functions ? s.summary.functions.ported : 0) || 0;
                    matchSnaps.push({
                        date: s.timestamp.slice(0, 10),
                        weighted: s.summary.match.weighted,
                        average: s.summary.match.average || s.summary.match.weighted,
                        scored: s.summary.match.scored_count || 0,
                        aligned: (alignedLower !== undefined && alignedLower !== null && auditedFns > 0) ?
                            alignedLower * 100 : null,
                        coverage: (auditedFns > 0 && portedFns > 0) ? auditedFns / portedFns * 100 : null,
                        audited: auditedFns,
                        ported: portedFns
                    });
                }
            }

            var accCanvas = document.getElementById('accuracyChart');
            if (matchSnaps.length >= 2 && accCanvas) {
                if (accContainer) accContainer.style.display = '';
                var accLabels = matchSnaps.map(function(m) { return m.date; });
                var weightedData = matchSnaps.map(function(m) { return m.weighted; });
                var alignedData = matchSnaps.map(function(m) { return m.aligned; });

                var latest = matchSnaps[matchSnaps.length - 1];
                var earliest = matchSnaps[0];
                var delta = latest.weighted - earliest.weighted;
                var deltaClass = delta >= 0 ? 'var(--accent-green)' : 'var(--accent-red)';
                var deltaSign = delta >= 0 ? '+' : '';
                var titleEl = document.getElementById('accuracyChartTitle');
                if (titleEl) {
                    titleEl.innerHTML = 'Match Over Time &mdash; ' +
                        '<span style="font-weight:400;font-size:0.85em;color:var(--text-secondary)">' +
                        'Mnemonic: <strong style="color:' + matchColor(latest.weighted) + '">' + latest.weighted.toFixed(1) + '%</strong> ' +
                        '<span style="color:' + deltaClass + ';font-weight:600">(' + deltaSign + delta.toFixed(1) + '% since ' + earliest.date + ')</span>' +
                        (latest.aligned !== null ?
                            ' &middot; Aligned bytes: <strong>' + latest.aligned.toFixed(1) + '%</strong> ' +
                            '(' + fmtNum(latest.audited) + ' / ' + fmtNum(latest.ported) + ' audited)' : '') +
                        '</span>';
                }

                var ctx2 = accCanvas.getContext('2d');
                chartInstances.accuracyChart = new Chart(ctx2, {
                    type: 'line',
                    data: {
                        labels: accLabels,
                        datasets: [
                            Object.assign({
                                label: 'Mnemonic Match, byte-weighted (%)',
                                data: weightedData,
                                backgroundColor: 'rgba(17, 99, 41, 0.08)',
                                borderWidth: 2, tension: 0.35, fill: true,
                                pointRadius: 2, pointHoverRadius: 5
                            }, scaleSeriesStyle()),
                            Object.assign({
                                label: 'Aligned Byte Accuracy (%)',
                                data: alignedData,
                                borderWidth: 2, borderDash: [6, 3], tension: 0.35, fill: false,
                                pointStyle: 'rectRot', pointRadius: 3, pointHoverRadius: 6
                            }, scaleSeriesStyle())
                        ]
                    },
                    options: chartOpts({
                        legend: true,
                        tooltipCallback: function(tooltipItem) {
                            var idx = tooltipItem.dataIndex;
                            var item = matchSnaps[idx];
                            if (tooltipItem.datasetIndex === 0) {
                                return 'Weighted mnemonic match: ' + item.weighted.toFixed(2) + '% (' + item.scored + ' functions)';
                            } else if (item.aligned === null) {
                                return null;  // pre-audit snapshot
                            }
                            return 'Aligned byte accuracy: ' + item.aligned.toFixed(2) + '% (' +
                                item.audited + ' / ' + item.ported + ' ported functions audited)';
                        }
                    })
                });
            } else if (accContainer) {
                accContainer.style.display = 'none';
            }
        }

        function destroyChart(id) {
            if (chartInstances[id]) {
                chartInstances[id].destroy();
                delete chartInstances[id];
            }
        }

        function chartOpts(opts) {
            opts = opts || {};
            var showLegend = !!opts.legend;
            var tooltipCb = opts.tooltipCallback;

            return {
                responsive: true, maintainAspectRatio: false,
                interaction: { intersect: false, mode: 'index' },
                plugins: {
                    legend: {
                        display: showLegend,
                        labels: { color: '#8b949e', boxWidth: 12, padding: 12 }
                    },
                    tooltip: {
                        backgroundColor: '#161b22', borderColor: '#30363d', borderWidth: 1,
                        titleColor: '#c9d1d9', bodyColor: '#c9d1d9',
                        padding: 12, cornerRadius: 8,
                        callbacks: tooltipCb ? { label: tooltipCb } : {}
                    }
                },
                scales: {
                    x: {
                        grid: { color: '#21262d' },
                        ticks: { color: '#8b949e', maxRotation: 45 }
                    },
                    y: {
                        grid: { color: '#21262d' },
                        ticks: { color: '#8b949e' }
                    }
                }
            };
        }

        /* ===== VERIFICATION COVERAGE RENDERERS ===== */
        function renderVerifSection() {
            renderVerifFunnel();
            renderTuHeatmap();
            renderAddrStrip();
            renderTreemap();
            renderOverviewDonut();
        }

        function renderVerifFunnel() {
            var s = REPORT.summary;
            var total = s.functions.total;
            var ported = s.functions.ported;
            var vc71 = s.match ? (s.match.scored_count || 0) : 0;
            var vData = countVerified();

            var steps = [
                { label: 'All functions',   count: total,       color: '#3d444d', pct: 100 },
                { label: 'Ported',          count: ported,      pct: total > 0 ? ported / total * 100 : 0 },
                { label: 'VC71 scored',     count: vc71,        pct: total > 0 ? vc71 / total * 100 : 0 },
                { label: 'Behavioral match', count: vData.total, pct: total > 0 ? vData.total / total * 100 : 0 }
            ];
            // Bars take the shared scale color of their share of all functions.
            for (var k = 1; k < steps.length; k++) steps[k].color = scaleColor(steps[k].pct, false);

            var html = '';
            for (var i = 0; i < steps.length; i++) {
                var st = steps[i];
                var sub = '';
                if (i === 1) sub = ' <span class="funnel-sub">(' + st.count + '/' + total + ')</span>';
                else if (i > 1) sub = ' <span class="funnel-sub">(' + (ported > 0 ? (st.count / ported * 100).toFixed(1) : '0') + '% of ported)</span>';
                var showLabel = st.pct > 12;
                html += '<div class="funnel-row">' +
                    '<div class="funnel-label">' + st.label + '</div>' +
                    '<div class="funnel-bar-track">' +
                        '<div class="funnel-bar-fill" style="width:' + Math.max(st.pct, 0.4) + '%;background:' + st.color + '">' +
                            (showLabel ? fmtNum(st.count) : '') +
                        '</div>' +
                    '</div>' +
                    '<div class="funnel-count">' + fmtNum(st.count) + sub + '</div>' +
                '</div>';
            }
            var el = document.getElementById('verif-funnel');
            if (el) el.innerHTML = html;
        }

        // The map answers the two headline questions per unit at a glance, with
        // a single visual element: bar length = implemented surface (ported
        // bytes / unit bytes) and bar color = mnemonic similarity (byte-weighted VC71
        // match over scored bytes). Everything else — function counts, the
        // scored-byte denominator, verification-lane evidence — lives in the
        // hover tooltip. A TU can be partly ported, partly scored, and partly
        // behaviorally verified at the same time, so no tile claims whole-TU
        // correctness.
        // Evidence-map colors: green is reserved for a complete 100%, so the
        // >=95% band uses purple and the two cannot be mistaken for each other.
        // One shared scale for completion, raw byte accuracy and mnemonic match:
        // 100 / >=75 / >=50 / >=25 / >0 / 0, plus gray for "no data".
        var SCALE_COMPLETE = '#116329';
        var SCALE_NO_DATA = '#8b949e';
        // complete === false marks a 100% figure over partial coverage (unaudited
        // or unscored remainder): it gets the next band down instead of the
        // solid "complete" green.
        function scaleColor(pct, complete) {
            if (pct === null || pct === undefined) return SCALE_NO_DATA;
            if (pct >= 100) return complete === false ? '#2ea043' : SCALE_COMPLETE;
            if (pct >= 75) return '#2ea043';
            if (pct >= 50) return '#d29922';
            if (pct >= 25) return '#d4760a';
            if (pct > 0) return '#da3633';
            return '#21262d';
        }
        // Text variant: the 0% band would be invisible on the dark background.
        function scaleTextColor(pct) {
            return pct !== null && pct !== undefined && pct <= 0 ? '#da3633' : scaleColor(pct);
        }
        // Chart.js styling for an accuracy series: points and line segments take
        // the scale color of their value; the dataset color (legend swatch) stays
        // the top band.
        function scaleSeriesStyle() {
            return {
                borderColor: SCALE_COMPLETE,
                pointBackgroundColor: function(c) { return scaleTextColor(c.parsed ? c.parsed.y : null); },
                pointBorderColor: function(c) { return scaleTextColor(c.parsed ? c.parsed.y : null); },
                segment: { borderColor: function(c) { return scaleTextColor(c.p1.parsed.y); } }
            };
        }
        // Which metric colors the evidence map: 'mnemonic' or 'aligned'.
        var tuMapMetric = 'aligned';
        try {
            if (localStorage.getItem('tuMapMetric') === 'aligned') tuMapMetric = 'aligned';
        } catch (e) {}
        function setTuMapMetric(metric) {
            tuMapMetric = metric;
            try { localStorage.setItem('tuMapMetric', metric); } catch (e) {}
            renderTuHeatmap();
        }
        // Aligned byte accuracy over the unit's audited functions, or null.
        function unitAlignedPct(unit, st) {
            var rx = unit.raw_xbe_structural || {};
            if (st.ported === 0 || !rx.aligned_scored_functions) return null;
            var lower = rx.aligned_byte_accuracy_lower;
            return lower === null || lower === undefined ? null : lower * 100;
        }
        // Byte-exact needs every ported function audited, like the mnemonic rule.
        function unitHasCompleteAlignedMatch(unit, st) {
            var rx = unit.raw_xbe_structural || {};
            var pct = unitAlignedPct(unit, st);
            return pct !== null && pct >= 100 && rx.aligned_scored_functions === st.ported;
        }

        // A unit earns the solid-green "complete mnemonic match" distinction only when
        // every ported function has been scored AND the size-weighted match is
        // 100%. A 100% mnemonic match over a partial score coverage is just light green —
        // the unscored remainder is unproven.
        function unitHasCompleteMnemonicMatch(unit, st) {
            if (st.ported === 0 || st.scored !== st.ported) return false;
            var match = (unit.summary || {}).match_weighted;
            return match !== null && match !== undefined && match >= 100;
        }
        function unitEvidenceStats(unit) {
            var funcs = unit.functions || [];
            var total = funcs.length;
            var totalBytes = 0;
            var ported = 0;
            var portedBytes = 0;
            var scored = 0;
            var scoredBytes = 0;
            var verified = 0;
            var verifiedBytes = 0;
            var divergent = 0;
            var methods = { equiv: 0, snapshot: 0, runtime: 0, mnemonic: 0 };

            for (var i = 0; i < funcs.length; i++) {
                var f = funcs[i];
                var size = Math.max(Number(f.size) || 0, 0);
                totalBytes += size;
                if (!f.ported) continue;
                ported++;
                portedBytes += size;
                if (typeof f.match_percent === 'number') {
                    scored++;
                    scoredBytes += size;
                }
                if (isDivergent(f)) divergent++;
                if (equivVerified(f)) methods.equiv++;
                if (f.snapshot_passed === true) methods.snapshot++;
                if (f.runtime_oracle_passed === true) methods.runtime++;
                if (f.match_percent !== null && f.match_percent !== undefined && f.match_percent >= 90) methods.mnemonic++;
                if (isVerified(f)) {
                    verified++;
                    verifiedBytes += size;
                }
            }

            var byteCoverage = portedBytes > 0 ? scoredBytes / portedBytes * 100 : null;
            var state = 'No ported functions';
            if (ported > 0) {
                if (divergent > 0) state = 'Divergence candidate needs triage';
                else if (ported === total && verified === ported) state = 'Fully ported; all functions have a behavioral match';
                else if (ported === total && scored === ported) state = 'Fully ported; every function has VC71 evidence';
                else if (ported === total) state = 'Fully ported; evidence is incomplete';
                else state = 'Partially ported';
            }
            return {
                total: total,
                totalBytes: totalBytes,
                ported: ported,
                portedBytes: portedBytes,
                scored: scored,
                scoredBytes: scoredBytes,
                byteCoverage: byteCoverage,
                verified: verified,
                verifiedBytes: verifiedBytes,
                divergent: divergent,
                methods: methods,
                state: state
            };
        }

        function pctText(value) {
            return value === null || value === undefined ? 'n/a' : value.toFixed(1) + '%';
        }

        function unitMeterPct(st, bytes, funcs) {
            var denominator = st.totalBytes > 0 ? st.totalBytes : st.total;
            var numerator = st.totalBytes > 0 ? bytes : funcs;
            return denominator > 0 ? numerator / denominator * 100 : 0;
        }

        function unitEvidenceTooltip(unit, st) {
            var s = unit.summary || {};
            var match = s.match_weighted !== null && s.match_weighted !== undefined
                ? s.match_weighted.toFixed(1) + '%'
                : 'not scored';
            var lines = [
                unit.name,
                st.state,
                'Ported: ' + st.ported + ' / ' + st.total + ' functions · ' +
                    pctText(unitMeterPct(st, st.portedBytes, st.ported)) + ' of bytes',
                'Mnemonic match: ' + match + ' · ' + st.scored + ' / ' + st.ported + ' ported functions scored',
                'Aligned bytes: ' + (unitAlignedPct(unit, st) !== null ?
                    unitAlignedPct(unit, st).toFixed(1) + '%' : 'not audited') + ' · ' +
                    ((unit.raw_xbe_structural || {}).aligned_scored_functions || 0) + ' / ' +
                    st.ported + ' ported functions audited',
                'Behavioral matches: ' + st.verified + ' / ' + st.ported + ' ported functions'
            ];
            var methodText = [];
            if (st.methods.equiv) methodText.push(st.methods.equiv + ' equivalence');
            if (st.methods.snapshot) methodText.push(st.methods.snapshot + ' snapshot');
            if (st.methods.runtime) methodText.push(st.methods.runtime + ' runtime');
            lines.push('Checks passed: ' + (methodText.length ? methodText.join(', ') : 'none'));
            if (st.divergent) lines.push('Needs investigation: ' + st.divergent);
            return lines.filter(function(l) { return l !== null; }).join('\\n');
        }

        function renderTuHeatmap() {
            var units = REPORT.units;
            var html = '';
            for (var i = 0; i < units.length; i++) {
                var u = units[i];
                var st = unitEvidenceStats(u);
                var s = u.summary || {};
                var match = st.ported > 0 ? s.match_weighted : null;
                var color = tuMapMetric === 'aligned' ?
                    scaleColor(unitAlignedPct(u, st), unitHasCompleteAlignedMatch(u, st)) :
                    scaleColor(match, unitHasCompleteMnemonicMatch(u, st));
                var portedPct = unitMeterPct(st, st.portedBytes, st.ported);
                var tip = unitEvidenceTooltip(u, st);
                var label = u.name + ': ' + st.state;
                html += '<div class="tu-bar-tile' + (st.divergent ? ' divergent' : '') + '" title="' + escHtml(tip) + '" aria-label="' + escHtml(label) + '" onclick="goToUnit(\\'' + jsEsc(u.name) + '\\')">' +
                    '<div class="tu-bar-fill" style="width:' + portedPct + '%;background:' + color + '"></div>' +
                    '</div>';
            }
            var el = document.getElementById('tu-heatmap');
            if (el) el.innerHTML = html;

            var toggleEl = document.getElementById('tu-metric-toggle');
            if (toggleEl) {
                toggleEl.innerHTML =
                    '<button class="' + (tuMapMetric === 'mnemonic' ? 'active' : '') + '" onclick="setTuMapMetric(\\'mnemonic\\')">Mnemonic</button>' +
                    '<button class="' + (tuMapMetric === 'aligned' ? 'active' : '') + '" onclick="setTuMapMetric(\\'aligned\\')">Byte accuracy</button>';
            }
            var metricName = tuMapMetric === 'aligned' ? 'aligned byte accuracy' : 'mnemonic match';

            var legEl = document.getElementById('tu-legend');
            if (legEl) {
                var legHtml =
                    '<div class="tu-legend-item"><div class="tu-legend-swatch" style="background:' + SCALE_COMPLETE + '"></div>100% ' + metricName + ' (every ported function ' + (tuMapMetric === 'aligned' ? 'audited' : 'scored') + ')</div>' +
                    '<div class="tu-legend-item"><div class="tu-legend-swatch" style="background:#2ea043"></div>75&ndash;100%</div>' +
                    '<div class="tu-legend-item"><div class="tu-legend-swatch" style="background:#d29922"></div>50&ndash;75%</div>' +
                    '<div class="tu-legend-item"><div class="tu-legend-swatch" style="background:#d4760a"></div>25&ndash;50%</div>' +
                    '<div class="tu-legend-item"><div class="tu-legend-swatch" style="background:#da3633"></div>0&ndash;25%</div>' +
                    '<div class="tu-legend-item"><div class="tu-legend-swatch" style="background:#21262d;outline:1px solid #30363d;outline-offset:-1px"></div>0%</div>' +
                    '<div class="tu-legend-item"><div class="tu-legend-swatch" style="background:' + SCALE_NO_DATA + '"></div>' + (tuMapMetric === 'aligned' ? 'not audited' : 'unscored') + '</div>' +
                    '<div class="tu-legend-item"><div class="tu-legend-swatch" style="background-color:#2ea043;background-image:repeating-linear-gradient(-45deg,rgba(0,0,0,0.35),rgba(0,0,0,0.35) 3px,transparent 3px,transparent 6px)"></div>divergence candidate (striped)</div>' +
                    '<div class="tu-legend-item"><div class="tu-legend-swatch" style="background:transparent;outline:1px solid #f85149;outline-offset:-1px"></div>divergence candidate outline</div>' +
                    '<div class="tu-map-note">Bar length = implemented (ported bytes / unit bytes). Color = ' +
                    (tuMapMetric === 'aligned' ?
                        'raw-XBE aligned byte accuracy over audited functions.' :
                        'byte-weighted VC71 mnemonic match over scored bytes.') +
                    ' Hover a tile for both figures.</div>';
                legEl.innerHTML = legHtml;
            }
        }

        function renderAddrStrip() {
            var canvas = document.getElementById('addrStripCanvas');
            if (!canvas) return;
            var wrap = canvas.parentNode;
            var W = wrap.clientWidth || 1200;
            var H = 56;
            var dpr = window.devicePixelRatio || 1;
            canvas.width = Math.round(W * dpr);
            canvas.height = Math.round(H * dpr);
            canvas.style.width = W + 'px';
            canvas.style.height = H + 'px';
            var ctx = canvas.getContext('2d');
            ctx.scale(dpr, dpr);

            var fns = [];
            var minA = Infinity, maxA = 0;
            for (var ui = 0; ui < REPORT.units.length; ui++) {
                var funcs = REPORT.units[ui].functions || [];
                for (var fi = 0; fi < funcs.length; fi++) {
                    var f = funcs[fi];
                    var addr = parseInt(f.address, 16);
                    if (!addr || addr < 0x1000) continue;
                    if (addr < minA) minA = addr;
                    if (addr > maxA) maxA = addr;
                    fns.push(f);
                }
            }
            if (!fns.length || minA >= maxA) return;

            var range = maxA - minA;
            ctx.fillStyle = '#161b22';
            ctx.fillRect(0, 0, W, H);

            for (var i = 0; i < fns.length; i++) {
                var fn = fns[i];
                var addr = parseInt(fn.address, 16);
                var x = Math.floor((addr - minA) / range * (W - 1));
                var barW = Math.max(1, Math.round((fn.size || 1) / range * W));

                var col;
                if (!fn.ported) { col = '#2d333b'; }
                else if (isVerified(fn)) { col = '#3fb950'; }
                else if (fn.match_percent !== null && fn.match_percent !== undefined) { col = '#388bfd'; }
                else { col = '#444c56'; }

                ctx.fillStyle = col;
                ctx.fillRect(x, 0, barW, H);
            }

            var legendItems = [
                ['#2d333b', 'Unported'], ['#444c56', 'Ported'],
                ['#388bfd', 'VC71 scored'], ['#3fb950', 'Behaviorally verified']
            ];
            var legEl = document.getElementById('addr-legend');
            if (legEl) {
                var lh = '';
                for (var li = 0; li < legendItems.length; li++) {
                    lh += '<div class="addr-legend-item"><div class="addr-legend-dot" style="background:' + legendItems[li][0] + '"></div>' + legendItems[li][1] + '</div>';
                }
                legEl.innerHTML = lh;
            }
        }

        /* ===== TREEMAP ===== */
        var treemapRects = [];
        // Treemap color metric: 'completion' (ported share) or 'aligned' (raw byte accuracy).
        var treemapMetric = 'completion';
        try {
            if (localStorage.getItem('treemapMetric') === 'aligned') treemapMetric = 'aligned';
        } catch (e) {}
        function setTreemapMetric(metric) {
            treemapMetric = metric;
            try { localStorage.setItem('treemapMetric', metric); } catch (e) {}
            renderTreemap();
        }

        // [fill color, label text] for a unit under the active treemap metric.
        function treemapUnitStyle(u) {
            if (treemapMetric !== 'aligned') {
                var pct = u.summary.percent;
                return [completionColor(pct), pct.toFixed(0) + '%'];
            }
            var st = unitEvidenceStats(u);
            var aligned = unitAlignedPct(u, st);
            var color = scaleColor(aligned, unitHasCompleteAlignedMatch(u, st));
            return [color, aligned === null ? 'n/a' : aligned.toFixed(0) + '%'];
        }

        function completionColor(pct) {
            return scaleColor(pct);
        }

        function squarify(items, x, y, w, h, out) {
            if (!items.length || w <= 0 || h <= 0) return;
            if (items.length === 1) {
                out.push({ x: x, y: y, w: w, h: h, item: items[0] });
                return;
            }
            var totalVal = 0;
            for (var i = 0; i < items.length; i++) totalVal += items[i].value;
            if (totalVal <= 0) return;

            var vertical = h > w;
            var mainDim = vertical ? h : w;
            var otherDim = vertical ? w : h;

            var row = [];
            var rowVal = 0;
            var bestAspect = Infinity;
            var splitAt = 0;

            for (var i = 0; i < items.length; i++) {
                row.push(items[i]);
                rowVal += items[i].value;
                var rowFrac = rowVal / totalVal;
                var rowDim = mainDim * rowFrac;

                var worstAspect = 0;
                var subVal = 0;
                for (var j = 0; j < row.length; j++) {
                    subVal += row[j].value;
                    var cellDim = rowVal > 0 ? otherDim * (row[j].value / rowVal) : 0;
                    if (cellDim <= 0 || rowDim <= 0) continue;
                    var aspect = cellDim > rowDim ? cellDim / rowDim : rowDim / cellDim;
                    if (aspect > worstAspect) worstAspect = aspect;
                }
                if (worstAspect <= bestAspect) {
                    bestAspect = worstAspect;
                    splitAt = i + 1;
                } else {
                    break;
                }
            }

            var headItems = items.slice(0, splitAt);
            var tailItems = items.slice(splitAt);
            var headVal = 0;
            for (var i = 0; i < headItems.length; i++) headVal += headItems[i].value;
            var headFrac = totalVal > 0 ? headVal / totalVal : 0;

            if (vertical) {
                var headH = h * headFrac;
                var cx = x, accVal = 0;
                for (var i = 0; i < headItems.length; i++) {
                    var frac = headVal > 0 ? headItems[i].value / headVal : 0;
                    var cw = w * frac;
                    out.push({ x: cx, y: y, w: cw, h: headH, item: headItems[i] });
                    cx += cw;
                }
                if (tailItems.length) squarify(tailItems, x, y + headH, w, h - headH, out);
            } else {
                var headW = w * headFrac;
                var cy = y, accVal = 0;
                for (var i = 0; i < headItems.length; i++) {
                    var frac = headVal > 0 ? headItems[i].value / headVal : 0;
                    var ch = h * frac;
                    out.push({ x: x, y: cy, w: headW, h: ch, item: headItems[i] });
                    cy += ch;
                }
                if (tailItems.length) squarify(tailItems, x + headW, y, w - headW, h, out);
            }
        }

        function renderTreemap() {
            var canvas = document.getElementById('treemapCanvas');
            if (!canvas) return;
            var wrap = canvas.parentNode;
            var W = wrap.clientWidth || 900;
            var H = wrap.clientHeight || 420;
            var dpr = window.devicePixelRatio || 1;
            canvas.width = Math.round(W * dpr);
            canvas.height = Math.round(H * dpr);
            canvas.style.width = W + 'px';
            canvas.style.height = H + 'px';
            var ctx = canvas.getContext('2d');
            ctx.scale(dpr, dpr);

            var items = [];
            for (var i = 0; i < REPORT.units.length; i++) {
                var u = REPORT.units[i];
                if (u.synthetic) continue;
                var bytes = u.summary.bytes_total || 1;
                items.push({ value: bytes, unit: u });
            }
            items.sort(function(a, b) { return b.value - a.value; });

            var toggleEl = document.getElementById('treemap-metric-toggle');
            if (toggleEl) {
                toggleEl.innerHTML =
                    '<button class="' + (treemapMetric === 'completion' ? 'active' : '') + '" onclick="setTreemapMetric(\\'completion\\')">Completion</button>' +
                    '<button class="' + (treemapMetric === 'aligned' ? 'active' : '') + '" onclick="setTreemapMetric(\\'aligned\\')">Byte accuracy</button>';
            }

            treemapRects = [];
            squarify(items, 1, 1, W - 2, H - 2, treemapRects);

            ctx.fillStyle = '#0d1117';
            ctx.fillRect(0, 0, W, H);

            for (var i = 0; i < treemapRects.length; i++) {
                var r = treemapRects[i];
                var u = r.item.unit;
                var style = treemapUnitStyle(u);
                ctx.fillStyle = style[0];
                ctx.fillRect(r.x + 1, r.y + 1, r.w - 2, r.h - 2);

                ctx.strokeStyle = '#0d1117';
                ctx.lineWidth = 1.5;
                ctx.strokeRect(r.x, r.y, r.w, r.h);

                if (r.w > 50 && r.h > 28) {
                    ctx.fillStyle = '#fff';
                    var fontSize = Math.min(11, Math.max(8, Math.min(r.w / 8, r.h / 3)));
                    ctx.font = '600 ' + fontSize + 'px -apple-system, sans-serif';
                    ctx.textBaseline = 'top';
                    var label = u.name.replace(/\\.obj$/, '');
                    if (ctx.measureText(label).width > r.w - 8) {
                        label = label.substring(0, Math.floor((r.w - 16) / (fontSize * 0.6))) + '\\u2026';
                    }
                    ctx.fillText(label, r.x + 4, r.y + 4);
                    if (r.h > 42) {
                        ctx.font = '700 ' + Math.max(9, fontSize) + 'px -apple-system, sans-serif';
                        ctx.fillText(style[1], r.x + 4, r.y + 4 + fontSize + 3);
                    }
                }
            }

            var tooltip = document.getElementById('treemap-tooltip');
            canvas.onmousemove = function(e) {
                var rect = canvas.getBoundingClientRect();
                var mx = (e.clientX - rect.left);
                var my = (e.clientY - rect.top);
                var hit = null;
                for (var i = 0; i < treemapRects.length; i++) {
                    var r = treemapRects[i];
                    if (mx >= r.x && mx < r.x + r.w && my >= r.y && my < r.y + r.h) { hit = r; break; }
                }
                if (hit) {
                    var u = hit.item.unit;
                    var s = u.summary;
                    var hitSt = unitEvidenceStats(u);
                    var hitAligned = unitAlignedPct(u, hitSt);
                    var hitAudited = (u.raw_xbe_structural || {}).aligned_scored_functions || 0;
                    tooltip.innerHTML = '<strong>' + escHtml(u.name) + '</strong><br>' +
                        s.ported + ' / ' + s.total + ' functions (' + s.percent.toFixed(1) + '%)<br>' +
                        fmtNum(s.bytes_ported) + ' / ' + fmtNum(s.bytes_total) + ' bytes' +
                        (s.match_weighted != null ? '<br>Mnemonic match: ' + s.match_weighted.toFixed(1) + '%' : '') +
                        '<br>Raw byte accuracy: ' + (hitAligned !== null ? hitAligned.toFixed(1) + '%' : 'not audited') +
                        ' (' + hitAudited + ' / ' + hitSt.ported + ' ported functions audited)';
                    tooltip.style.display = 'block';
                    tooltip.style.left = (e.clientX + 12) + 'px';
                    tooltip.style.top = (e.clientY - 10) + 'px';
                    canvas.style.cursor = 'pointer';
                } else {
                    tooltip.style.display = 'none';
                    canvas.style.cursor = 'default';
                }
            };
            canvas.onmouseleave = function() { tooltip.style.display = 'none'; };
            canvas.onclick = function(e) {
                var rect = canvas.getBoundingClientRect();
                var mx = (e.clientX - rect.left);
                var my = (e.clientY - rect.top);
                for (var i = 0; i < treemapRects.length; i++) {
                    var r = treemapRects[i];
                    if (mx >= r.x && mx < r.x + r.w && my >= r.y && my < r.y + r.h) {
                        tooltip.style.display = 'none';
                        goToUnit(r.item.unit.name);
                        return;
                    }
                }
            };
        }

        function renderOverviewDonut() {
            var ctx = document.getElementById('overviewDonut');
            if (!ctx) return;
            destroyChart('overviewDonut');
            var s = REPORT.summary;
            var ported = s.bytes.ported || 0;
            var remaining = (s.bytes.total || 1) - ported;
            var pctEl = document.getElementById('donut-pct');
            var lblEl = document.getElementById('donut-label');
            if (pctEl) pctEl.textContent = s.bytes.percent.toFixed(1) + '%';
            if (lblEl) lblEl.textContent = fmtNum(ported) + ' / ' + fmtNum(s.bytes.total) + ' bytes';

            chartInstances.overviewDonut = new Chart(ctx.getContext('2d'), {
                type: 'doughnut',
                data: {
                    labels: ['Ported', 'Remaining'],
                    datasets: [{
                        data: [ported, remaining],
                        backgroundColor: ['#238636', '#21262d'],
                        borderColor: ['#2ea043', '#30363d'],
                        borderWidth: 1
                    }]
                },
                options: {
                    responsive: false,
                    cutout: '65%',
                    plugins: {
                        legend: { display: false },
                        tooltip: {
                            enabled: false,
                            external: function(ctx) {
                                var tooltipEl = document.getElementById('chartjs-donut-tooltip');
                                if (!tooltipEl) {
                                    tooltipEl = document.createElement('div');
                                    tooltipEl.id = 'chartjs-donut-tooltip';
                                    tooltipEl.style.background = '#161b22';
                                    tooltipEl.style.border = '1px solid #30363d';
                                    tooltipEl.style.borderRadius = '6px';
                                    tooltipEl.style.padding = '8px 12px';
                                    tooltipEl.style.color = '#c9d1d9';
                                    tooltipEl.style.fontSize = '0.82em';
                                    tooltipEl.style.pointerEvents = 'none';
                                    tooltipEl.style.position = 'fixed';
                                    tooltipEl.style.zIndex = 1000;
                                    tooltipEl.style.boxShadow = '0 4px 12px rgba(0,0,0,0.3)';
                                    tooltipEl.style.whiteSpace = 'nowrap';
                                    document.body.appendChild(tooltipEl);
                                }
                                var chart = ctx.chart;
                                var tooltip = ctx.tooltip;
                                if (tooltip.opacity === 0) {
                                    tooltipEl.style.display = 'none';
                                    return;
                                }
                                var dp = tooltip.dataPoints[0];
                                tooltipEl.textContent = dp.label + ': ' + fmtNum(dp.raw) + ' bytes';
                                tooltipEl.style.display = 'block';
                                var canvasRect = chart.canvas.getBoundingClientRect();
                                var x = canvasRect.left + tooltip.caretX + 16;
                                var y = canvasRect.top + tooltip.caretY;
                                var trect = tooltipEl.getBoundingClientRect();
                                if (x + trect.width > window.innerWidth) {
                                    x = canvasRect.left + tooltip.caretX - trect.width - 16;
                                }
                                if (y + trect.height > window.innerHeight) {
                                    y = window.innerHeight - trect.height - 8;
                                }
                                tooltipEl.style.left = x + 'px';
                                tooltipEl.style.top = y + 'px';
                            }
                        }
                    }
                }
            });
        }

        function syncSortArrows() {
            var headers = document.querySelectorAll('th[data-col]');
            for (var i = 0; i < headers.length; i++) {
                var arrow = headers[i].querySelector('.sort-arrow');
                var c = parseInt(headers[i].getAttribute('data-col'));
                if (arrow) arrow.textContent = c === sortCol ? (sortAsc ? ' \\u25B2' : ' \\u25BC') : '';
            }
        }

        function renderTable() {
            syncSortArrows();
            var query = filterText.toLowerCase();
            var units = REPORT.units.filter(function(u) {
                return u.name.toLowerCase().indexOf(query) !== -1 ||
                       (u.source_path || '').toLowerCase().indexOf(query) !== -1;
            });

            if (sortCol >= 0) {
                units.sort(function(a, b) {
                    var va = getSortVal(a, sortCol);
                    var vb = getSortVal(b, sortCol);
                    if (typeof va === 'number') return sortAsc ? va - vb : vb - va;
                    return sortAsc ? String(va).localeCompare(String(vb)) : String(vb).localeCompare(String(va));
                });
            }

            document.getElementById('search-count').textContent = units.length + ' / ' + REPORT.units.length + ' units';

            var html = '';
            for (var i = 0; i < units.length; i++) {
                var u = units[i];
                var s = u.summary;
                var name = u.name;
                var source = u.synthetic ? 'synthetic bucket' : (u.source_path || '?');
                var fp = s.percent;

                var pctClass = fp >= 100 ? 'pct-complete' : (fp > 0 ? 'pct-partial' : 'pct-none');

                var matchHtml = '';
                var sc = s.match_avg;
                if (sc !== null && sc !== undefined) {
                    var sw = s.match_weighted !== null && s.match_weighted !== undefined ? s.match_weighted : sc;
                    var mClass = matchBadge(sw);
                    var tip = 'Instruction names and order compared with the original. Exact values are ignored.\\n';
                    tip += 'Average: ' + sc.toFixed(1) + '% · weighted: ' + sw.toFixed(1) + '%';
                    matchHtml = '<span class="match-indicator" title="' + tip + '"><span class="match-dot ' + mClass + '"></span>' + sw.toFixed(1) + '%</span>';
                } else {
                    matchHtml = '<span class="pct-none">\u2014</span>';
                }
                html += '<tr>' +
                    '<td class="unit-name"><a class="unit-name-link" onclick="goToUnit(\\'' + jsEsc(name) + '\\')">' + escHtml(name) + '</a></td>' +
                    '<td class="source-path" title="' + escHtml(source) + '">' + escHtml(source) + '</td>' +
                    '<td class="num">' + s.total + '</td>' +
                    '<td class="num">' + s.ported + '</td>' +
                    '<td class="num"><div class="progress-cell"><div class="mini-bar"><div class="mini-fill" style="width:' + Math.max(fp, 1) + '%"></div></div><span class="' + pctClass + '">' + fp.toFixed(1) + '%</span></div></td>' +
                    '<td class="num">' + fmtNum(s.bytes_ported) + ' / ' + fmtNum(s.bytes_total) + '</td>' +
                    '<td class="num">' + matchHtml + '</td>' +
                '</tr>';
            }
            document.getElementById('table-body').innerHTML = html;
        }

        function getSortVal(unit, col) {
            var s = unit.summary;
            switch (col) {
                case 0: return unit.name;
                case 1: return unit.source_path || '';
                case 2: return s.total;
                case 3: return s.ported;
                case 4: return s.percent;
                case 5: return s.bytes_total > 0 ? s.bytes_ported / s.bytes_total : 0;
                case 6: {
                    return s.match_weighted !== null && s.match_weighted !== undefined ? s.match_weighted : -1;
                }
                default: return '';
            }
        }

        function renderMeta() {
            var m = REPORT.meta;
            document.getElementById('meta').innerHTML =
                'Generated: ' + m.timestamp + ' &middot; ' +
                'Commit: ' + m.commit + ' (' + m.branch + ') &middot; ' +
                'Tool: generate_decomp_report.py ' + m.tool_version;
            document.getElementById('meta-detail').innerHTML =
                'Generated: ' + m.timestamp + ' &middot; ' +
                'Commit: ' + m.commit + ' (' + m.branch + ') &middot; ' +
                'Tool: generate_decomp_report.py ' + m.tool_version;
        }

        /* ===== UNIT DETAIL RENDER ===== */
        function renderUnitDetail(name) {
            // Find the unit
            var unit = null;
            for (var i = 0; i < REPORT.units.length; i++) {
                if (REPORT.units[i].name === name) {
                    unit = REPORT.units[i];
                    break;
                }
            }
            if (!unit) {
                document.getElementById('detail-content').innerHTML = '<div class="pct-none" style="text-align:center;padding:60px;">Unit not found: ' + escHtml(name) + '</div>';
                return;
            }

            var funcs = unit.functions || [];
            var s = unit.summary;
            currentUnitHasRef = !!(s.vc71_scoreable !== undefined ? s.vc71_scoreable : s.has_delinked_ref) && !unit.synthetic;
            // Ported functions with no current scored aligned-byte audit.
            var unauditedCount = funcs.filter(function(f) {
                return f.ported && f.raw_xbe_aligned_status !== 'scored';
            }).length;

            // Header
            document.getElementById('detail-unit-name').textContent = unit.name;
            var eq = unit.equivalence || {};
            var snap = unit.snapshot || {};
            var golden = unit.runtime_oracle || {};
            var ds = unit.data_summary || { total: 0 };
            var sourceLabel = unit.synthetic ? 'synthetic bucket' : (unit.source_path || '?');
            document.getElementById('detail-meta').innerHTML =
                '<span class="unit-meta-item">Source: <strong>' + escHtml(sourceLabel) + '</strong></span>' +
                '<span class="unit-meta-item">Functions: <strong>' + s.total + '</strong></span>' +
                '<span class="unit-meta-item">Ported: <strong>' + s.ported + '</strong> (' + s.percent.toFixed(1) + '%)</span>' +
                (ds.total > 0 ? '<span class="unit-meta-item">Data Symbols: <strong>' + ds.total + '</strong></span>' : '') +
                (!unit.synthetic ? '<span class="unit-meta-item">Bytes: <strong>' + fmtNum(s.bytes_ported) + ' / ' + fmtNum(s.bytes_total) + '</strong></span>' : '') +
                (s.match_weighted !== null && s.match_weighted !== undefined ?
                    '<span class="unit-meta-item">Match: <strong style="color:' + matchColor(s.match_weighted) + '">' + s.match_weighted.toFixed(1) + '%</strong></span>' : '') +
                (s.match_avg !== null && s.match_avg !== undefined ?
                    '<span class="unit-meta-item">Avg Match: <strong>' + s.match_avg.toFixed(1) + '%</strong></span>' : '') +
                (eq.tested > 0 ?
                    '<span class="unit-meta-item">Equiv: <strong>' + eq.tested + '</strong> tested &middot; <strong>' + (eq.avg_coverage !== null ? eq.avg_coverage.toFixed(1) + '%' : '?') + '</strong> avg cov &middot; <strong>' + eq.high_confidence + '</strong> high conf.</span>' : '') +
                (golden.tested > 0 ?
                    '<span class="unit-meta-item">Runtime Oracle: <strong style="color:#79c0ff">' + golden.passed + '</strong> / <strong>' + golden.tested + '</strong> passed</span>' : '') +
                (snap.tested > 0 ?
                    '<span class="unit-meta-item">Snapshot: <strong style="color:#a855f7">' + snap.tested + '</strong> tested &middot; <strong>' + snap.passed + '</strong> passed &middot; <strong>' + (snap.avg_coverage !== null ? snap.avg_coverage.toFixed(1) + '%' : '?') + '</strong> avg cov</span>' : '') +
                (unit.synthetic ?
                    '<span class="unit-meta-item pct-none" title="Platform or shared code grouped for reporting. Not counted as game source progress.">Synthetic bucket &middot; excluded from source progress</span>' : '') +
                // The raw-XBE structural audit is a whole-TU compile; it is
                // what fills in aligned bytes. Audits go stale on any source edit.
                (!unit.synthetic && s.ported > 0 ?
                    '<span class="unit-meta-item"><button class="score-btn" data-unit="' + jsEsc(unit.name) + '" onclick="rawAuditUnit(this)" title="Compile this unit and compare every ported function against the original XBE bytes. Fills in aligned bytes.">' +
                    (unauditedCount > 0 ?
                        '&#x25B6; Audit ' + unauditedCount + ' function' + (unauditedCount === 1 ? '' : 's') + ' (aligned bytes)' :
                        '&#x21BB; Re-audit unit (aligned bytes)') +
                    '</button></span>' : '');

            // Unit history chart & Match distribution chart
            renderUnitHistoryChart(unit.name);
            renderDetailChart(funcs);

            // Function table
            renderFuncTable(funcs);

            // Data symbol table
            renderDataTable(unit);
        }

        function renderUnitHistoryChart(unitName) {
            var container = document.getElementById('unitHistoryContainer');
            var canvas = document.getElementById('unitHistoryChart');
            destroyChart('unitHistoryChart');

            if (!container || !canvas || !HISTORY || !HISTORY.snapshots || HISTORY.snapshots.length < 2) {
                if (container) container.style.display = 'none';
                return;
            }

            var points = [];
            var snapshots = HISTORY.snapshots;
            for (var i = 0; i < snapshots.length; i++) {
                var s = snapshots[i];
                var units = s.units || [];
                for (var j = 0; j < units.length; j++) {
                    var u = units[j];
                    if (u.name === unitName) {
                        var funcPct = u.percent !== undefined ? u.percent : (u.total > 0 ? (u.ported / u.total * 100) : 0);
                        var bytesPct = u.bytes_total > 0 ? (u.bytes_ported / u.bytes_total * 100) : funcPct;
                        points.push({
                            date: s.timestamp.slice(0, 10),
                            funcPct: funcPct,
                            bytesPct: bytesPct,
                            matchWeighted: u.match_weighted !== undefined ? u.match_weighted : null,
                            matchAvg: u.match_avg !== undefined ? u.match_avg : null,
                            aligned: (u.aligned_accuracy !== undefined && u.aligned_accuracy !== null &&
                                      u.aligned_audited > 0) ? u.aligned_accuracy : null,
                            audited: u.aligned_audited || 0,
                            ported: u.ported || 0
                        });
                        break;
                    }
                }
            }

            if (points.length < 2) {
                container.style.display = 'none';
                return;
            }

            container.style.display = '';
            var labels = points.map(function(p) { return p.date; });
            var funcPctData = points.map(function(p) { return p.funcPct; });
            var bytesPctData = points.map(function(p) { return p.bytesPct; });
            var matchWeightedData = points.map(function(p) { return p.matchWeighted; });

            var hasMatchData = matchWeightedData.some(function(v) { return v !== null && v !== undefined; });
            var alignedData = points.map(function(p) { return p.aligned; });
            var hasAlignedData = alignedData.some(function(v) { return v !== null; });

            var datasets = [
                {
                    label: 'Ported Functions (%)',
                    data: funcPctData,
                    borderColor: '#58a6ff',
                    backgroundColor: 'rgba(88, 166, 255, 0.08)',
                    borderWidth: 2, tension: 0.35, fill: false,
                    pointRadius: 2, pointHoverRadius: 4
                },
                {
                    label: 'Ported Bytes (%)',
                    data: bytesPctData,
                    borderColor: '#a371f7',
                    backgroundColor: 'rgba(163, 113, 247, 0.05)',
                    borderWidth: 1.5, borderDash: [3, 3], tension: 0.35, fill: false,
                    pointRadius: 1.5, pointHoverRadius: 4
                }
            ];

            if (hasMatchData) {
                datasets.push(Object.assign({
                    label: 'Unit VC71 Mnemonic Match (%)',
                    data: matchWeightedData,
                    borderWidth: 2, tension: 0.35, fill: false,
                    spanGaps: true,
                    pointRadius: 3, pointHoverRadius: 6
                }, scaleSeriesStyle()));
            }

            // Aligned bytes cover only audited functions, so the tooltip carries
            // the audited count: a move with a coverage change is not accuracy.
            var alignedIndex = -1;
            if (hasAlignedData) {
                alignedIndex = datasets.length;
                datasets.push(Object.assign({
                    label: 'Unit Aligned Byte Accuracy (%)',
                    data: alignedData,
                    borderWidth: 2, borderDash: [6, 3], tension: 0.35, fill: false,
                    pointStyle: 'rectRot', pointRadius: 3, pointHoverRadius: 6
                }, scaleSeriesStyle()));
            }

            var ctx = canvas.getContext('2d');
            chartInstances.unitHistoryChart = new Chart(ctx, {
                type: 'line',
                data: { labels: labels, datasets: datasets },
                options: chartOpts({
                    legend: true,
                    tooltipCallback: function(tooltipItem) {
                        var p = points[tooltipItem.dataIndex];
                        var value = tooltipItem.parsed.y;
                        if (value === null || value === undefined) return null;
                        if (tooltipItem.datasetIndex === alignedIndex) {
                            return tooltipItem.dataset.label + ': ' + value.toFixed(1) +
                                '% (' + p.audited + ' / ' + p.ported + ' audited)';
                        }
                        return tooltipItem.dataset.label + ': ' + value.toFixed(1) + '%';
                    }
                })
            });
        }

        function renderDetailChart(funcs) {
            var ctx = document.getElementById('detailChart').getContext('2d');
            destroyChart('detailChart');

            var scored = funcs.filter(function(f) { return f.match_percent !== null && f.match_percent !== undefined && f.ported; });
            var parent = ctx.canvas.parentNode;
            if (scored.length < 2) {
                parent.style.display = 'none';
                return;
            }
            parent.style.display = '';

            // Bin match scores into ranges
            // Bins follow the shared color scale bands.
            var bins = { '0-25%': 0, '25-50%': 0, '50-75%': 0, '75-<100%': 0, '100%': 0 };
            var colors = [scaleColor(10), scaleColor(40), scaleColor(60), scaleColor(90), scaleColor(100)];
            var labels = ['0-25%', '25-50%', '50-75%', '75-<100%', '100%'];

            for (var i = 0; i < scored.length; i++) {
                var p = scored[i].match_percent;
                if (p < 25) bins['0-25%']++;
                else if (p < 50) bins['25-50%']++;
                else if (p < 75) bins['50-75%']++;
                else if (p < 100) bins['75-<100%']++;
                else bins['100%']++;
            }

            var data = labels.map(function(l) { return bins[l]; });

            chartInstances.detailChart = new Chart(ctx, {
                type: 'bar',
                data: {
                    labels: labels,
                    datasets: [{
                        label: 'Functions',
                        data: data,
                        backgroundColor: colors.map(function(c) { return c + '99'; }),
                        borderColor: colors,
                        borderWidth: 1,
                        borderRadius: 4
                    }]
                },
                options: {
                    responsive: true, maintainAspectRatio: false,
                    plugins: {
                        legend: { display: false },
                        tooltip: {
                            backgroundColor: '#161b22', borderColor: '#30363d', borderWidth: 1,
                            titleColor: '#c9d1d9', bodyColor: '#c9d1d9',
                            padding: 12, cornerRadius: 8,
                            callbacks: {
                                label: function(ctx) {
                                    var total = scored.length;
                                    return ctx.raw + ' functions (' + (ctx.raw / total * 100).toFixed(1) + '%)';
                                }
                            }
                        }
                    },
                    scales: {
                        x: {
                            grid: { color: '#21262d' },
                            ticks: { color: '#8b949e' }
                        },
                        y: {
                            grid: { color: '#21262d' },
                            ticks: { color: '#8b949e', precision: 0 },
                            beginAtZero: true
                        }
                    }
                }
            });
        }

        function renderFuncTable(funcs) {
            var query = funcFilterText.toLowerCase();
            var filtered = funcs.filter(function(f) {
                return f.name.toLowerCase().indexOf(query) !== -1;
            });

            if (funcSortCol >= 0) {
                filtered.sort(function(a, b) {
                    var va = getFuncSortVal(a, funcSortCol);
                    var vb = getFuncSortVal(b, funcSortCol);
                    if (typeof va === 'number') return funcSortAsc ? va - vb : vb - va;
                    return funcSortAsc ? String(va).localeCompare(String(vb)) : String(vb).localeCompare(String(va));
                });
            }

            document.getElementById('func-count').textContent = filtered.length + ' / ' + funcs.length + ' funcs';

            var html = '';
            for (var i = 0; i < filtered.length; i++) {
                var f = filtered[i];
                var mClass = matchBadge(f.match_percent);
                // Scoring is a unit-level action (see the header button), so no
                // per-function Score button here. A ported function with no score
                // is either "scoreable but not yet run" (the bounds table covers the
                // unit) or "n/a" (no bounds entry \u2014 VC71 impossible, needs equivalence).
                var matchDisplay;
                if (f.match_percent !== null && f.match_percent !== undefined) {
                    // Advisory operand-normalized score, shown as a muted
                    // secondary value. The primary number and the >=90
                    // display stays separate from behavioral verification.
                    var opndSuffix = '';
                    if (f.opnd_percent !== null && f.opnd_percent !== undefined) {
                        opndSuffix = ' <span class="opnd-pct" title="Instruction and operand shapes compared with the original. Exact constants and offsets are ignored. This is an extra diagnostic, not a correctness check.">· op ' + f.opnd_percent.toFixed(1) + '%</span>';
                    }
                    matchDisplay = '<span class="num"><span class="match-dot ' + mClass + '"></span>' + f.match_percent.toFixed(1) + '%' + opndSuffix + '</span>';
                } else if (f.vc71_flagged === 'compile_failed') {
                    matchDisplay = '<span class="func-status" style="background:#da363322;color:#f85149;border-color:#f85149" title="MSVC 7.1 could not compile this source file, so no mnemonic score is available.">compile fail</span>';
                } else if (f.vc71_flagged === 'no_reference') {
                    matchDisplay = '<span class="func-status" style="background:#d2992222;color:#d29922;border-color:#d29922" title="The original function boundaries are missing or empty, so there is nothing to compare.">no ref</span>';
                } else if (f.ported && !currentUnitHasRef) {
                    matchDisplay = '<span class="pct-none" title="The original function boundaries are unknown, so mnemonic scoring is unavailable.">n/a</span>';
                } else {
                    matchDisplay = '<span class="pct-none">\u2014</span>';
                }

                // Show only the instruction-aligned measurement here.  The old
                // positional result remains in artifacts for compatibility.
                var byteDisplay = '<span class="pct-none">\u2014</span>';
                var strictVerdict = f.raw_byte_verdict;
                var structuralVerdict = f.raw_xbe_structural_verdict;
                var byteTip = '';
                if (strictVerdict === 'raw-byte exact') {
                    byteTip = 'All bytes match the original. Nothing was ignored.\\n' +
                        'Size: ' + f.raw_byte_candidate_length + ' bytes.';
                    byteDisplay = '<span class="func-status ported" title="' + escHtml(byteTip) + '">100%</span>';
                } else if (f.raw_xbe_aligned_status === 'scored') {
                    var lower = f.raw_xbe_aligned_lower;
                    var upper = f.raw_xbe_aligned_upper == null ? lower : f.raw_xbe_aligned_upper;
                    var rangeText = (lower * 100).toFixed(1) +
                        (upper !== lower ? '\u2013' + (upper * 100).toFixed(1) : '') + '%';
                    byteTip = 'Aligned byte match: ' + rangeText + '\\n' +
                         f.raw_xbe_aligned_matching_bytes + ' / ' + f.raw_xbe_aligned_compared_bytes +
                         ' bytes match.\\n' +
                         f.raw_xbe_aligned_exact_instructions + ' / ' +
                         f.raw_xbe_aligned_instruction_pairs + ' aligned instructions match exactly.\\n' +
                         'Extra instructions, ours / original: ' + (f.raw_xbe_aligned_candidate_only || 0) +
                         ' / ' + (f.raw_xbe_aligned_reference_only || 0) + '.';
                    var verification = f.raw_xbe_verification || {};
                    var boundaries = verification.boundaries || {};
                    var compiler = verification.compiler || {};
                    var operands = verification.instruction_operands || {};
                    var literalBytes = verification.literal_bytes || {};
                    byteTip += '\\nBoundaries: ' + (boundaries.status || 'unverified') +
                        '; compiler: ' + (compiler.status || 'unverified') +
                        '; encoding: ' + (operands.status || 'unverified') +
                        '; literal bytes: ' + (literalBytes.status || 'unverified') + '.';
                    byteTip += '\\nKnown wrong relocation targets: ' +
                        (f.raw_xbe_aligned_mismatched_relocations || 0) +
                        ' (' + (f.raw_xbe_aligned_mismatched_relocation_bytes || 0) + ' bytes).';
                    if (f.raw_xbe_aligned_uncertain_bytes) {
                        byteTip += '\\nUnknown relocation targets: ' +
                            (f.raw_xbe_aligned_uncertain_relocations || 0) +
                            ' (' + f.raw_xbe_aligned_uncertain_bytes + ' bytes); these remain uncertain.';
                    }
                    if (f.raw_xbe_aligned_provisional) {
                        byteTip += '\\n? means some address bytes could not be matched safely.';
                    }
                    byteTip += '\\nThis compares compiled code. It does not prove correct behavior.';
                    var suffix = f.raw_xbe_aligned_provisional ? '?' : '';
                    var exactClass = lower === 1 && upper === 1 ? ' ported' : '';
                    byteDisplay = '<span class="func-status' + exactClass + '" style="background:#d2992222;color:#d29922;border-color:#d29922" title="' +
                        escHtml(byteTip) + '">' + rangeText + suffix + '</span>';
                } else if (strictVerdict === 'bytes differ') {
                    byteTip = 'The compiled bytes differ from the original.\\n' +
                        'Size, ours / original: ' + f.raw_byte_candidate_length + ' / ' + f.raw_byte_reference_length + ' bytes.';
                    if (f.raw_byte_first_difference !== null && f.raw_byte_first_difference !== undefined) {
                        byteTip += '\\nFirst difference at +0x' + Number(f.raw_byte_first_difference).toString(16) + '.';
                    }
                    byteTip += '\\nThis alone does not mean the behavior is wrong.';
                    byteDisplay = '<span class="func-status" style="background:#d2992222;color:#d29922;border-color:#d29922" title="' +
                        escHtml(byteTip) + '">Differs</span>';
                } else if (strictVerdict === 'not comparable' || structuralVerdict === 'not comparable') {
                    byteTip = 'No reliable byte comparison is available for this function.';
                    byteDisplay = '<span class="pct-none" title="' + escHtml(byteTip) + '">N/C</span>';
                }

                var vStatus = '<span class="pct-none">—</span>';
                if (isDivergent(f)) {
                    // Equivalence FAIL — behaviorally differs from the original. A bug
                    // CANDIDATE (some are harness artifacts), not a confirmed bug.
                    var dtip = 'Behavior differed during an equivalence test. Investigate before treating this function as correct.\\n';
                    var why = f.equiv_divergence;
                    if (why && why.message) dtip += why.message + '\\n';
                    if (f.equiv_log_path) dtip += 'Details: ' + f.equiv_log_path;
                    var whyText = why && why.message ? escHtml(why.message) : 'See test details';
                    vStatus = '<span class="func-status" style="background:#da363322;color:#f85149;border-color:#f85149" title="' + escHtml(dtip) + '">✗ Divergent</span> ' +
                        '<span class="pct-none" title="' + escHtml(dtip) + '">Why: ' + whyText + '</span> ' +
                        '<button class="score-btn" data-function="' + jsEsc(f.name) + '" data-address="' + jsEsc(f.address) + '" onclick="rerunEquivalence(this)" title="Run the behavioral comparison again.">↻ Re-run equiv</button>';
                } else if (isVerified(f)) {
                    var reasons = [];
                    var reasonTips = [];
                    if (f.equiv_proven && f.equiv_status === 'pass') {
                        reasons.push('Z3-proven');
                        reasonTips.push('Proved equivalent');
                    } else if (f.equiv_status === 'pass') {
                        reasons.push('Equiv');
                        reasonTips.push('Passed equivalence testing');
                    }
                    if (f.runtime_oracle_passed === true) {
                        reasons.push('Oracle');
                        reasonTips.push('Passed runtime comparison');
                    }
                    if (f.snapshot_passed === true) {
                        reasons.push('Snap');
                        reasonTips.push('Passed snapshot comparison');
                    }
                    var equivCounts = '';
                    if (f.equiv_status === 'pass' && f.equiv_passed !== null &&
                        f.equiv_passed !== undefined && f.equiv_seeds !== null &&
                        f.equiv_seeds !== undefined) {
                        equivCounts = ' ' + f.equiv_passed + '/' + f.equiv_seeds + ' passed';
                    }
                    var targetedCases = f.equiv_targeted_case_passed || 0;
                    var targetedLabel = targetedCases > 0 ?
                        ' · ' + targetedCases + ' targeted cases agree' : '';
                    vStatus = '<span class="func-status ported" title="' +
                        escHtml(reasonTips.join('. ')) + '">✓ ' + reasons[0] +
                        equivCounts + targetedLabel + '</span>';
                } else if (f.equiv_status === 'inconclusive') {
                    var inconclusiveReason = f.equiv_reason ||
                        'The behavioral run did not establish a verdict.';
                    var inconclusiveCases = f.equiv_targeted_case_passed || 0;
                    var inconclusiveLabel = inconclusiveCases > 0 ?
                        ' · ' + inconclusiveCases + ' targeted cases agree' : '';
                    vStatus = '<span class="func-status unported" title="' +
                        escHtml(inconclusiveReason) + '">⚠ Inconclusive' +
                        inconclusiveLabel + '</span>';
                } else if (f.ported) {
                    vStatus = '<span class="func-status unported" title="No behavioral check has passed yet.">pending</span>';
                }

                html += '<tr>' +
                    '<td class="func-address">' + f.address + '</td>' +
                    '<td class="func-name">' + escHtml(f.name) + '</td>' +
                    '<td class="num">' + fmtNum(f.size) + '</td>' +
                    '<td class="num">' + statusBadge(f.ported) + '</td>' +
                    '<td class="num">' + matchDisplay + '</td>' +
                    '<td class="num">' + byteDisplay + '</td>' +
                    '<td class="num">' + vStatus + '</td>' +
                '</tr>';
            }
            document.getElementById('func-table-body').innerHTML = html;
        }

        function getFuncSortVal(func, col) {
            switch (col) {
                case 0: return parseInt(func.address, 16) || 0;
                case 1: return func.name;
                case 2: return func.size;
                case 3: return func.ported ? 1 : 0;
                case 4: return func.match_percent !== null && func.match_percent !== undefined ? func.match_percent : -1;
                case 5:
                    // Conservative ordering: ranges sort by their lower bound.
                    if (func.raw_byte_verdict === 'raw-byte exact') return 100;
                    if (func.raw_xbe_aligned_lower !== null &&
                        func.raw_xbe_aligned_lower !== undefined) {
                        return func.raw_xbe_aligned_lower * 100;
                    }
                    if (func.raw_byte_verdict === 'bytes differ') return -1;
                    if (func.raw_byte_verdict === 'not comparable' ||
                        func.raw_xbe_structural_verdict === 'not comparable') return -2;
                    return -3;
                case 6: return isVerified(func) ? 1 : (func.ported ? 0 : -1);
                default: return '';
            }
        }

        /* ===== DATA SYMBOLS ===== */
        function renderDataTable(unit) {
            var dataSyms = unit.data || [];
            var section = document.getElementById('data-section');
            var tbody = document.getElementById('data-table-body');
            var countEl = document.getElementById('data-count');
            if (dataSyms.length === 0) {
                section.style.display = 'none';
                return;
            }
            section.style.display = '';
            countEl.textContent = '\u2014 ' + dataSyms.length + ' symbol' + (dataSyms.length !== 1 ? 's' : '');
            var html = '';
            for (var i = 0; i < dataSyms.length; i++) {
                var d = dataSyms[i];
                html += '<tr>' +
                    '<td class="func-address">' + escHtml(d.address) + '</td>' +
                    '<td class="func-name">' + escHtml(d.name) + '</td>' +
                    '<td><code style="font-size:0.82em;color:var(--text-secondary);">' + escHtml(d.decl) + '</code></td>' +
                '</tr>';
            }
            tbody.innerHTML = html;
        }

        /* ===== HELPERS ===== */
        function copyPqCmd(btn) {
            var cmd = btn.getAttribute('data-cmd');
            navigator.clipboard.writeText(cmd).then(function() {
                btn.textContent = 'Copied!';
                btn.classList.add('copied');
                setTimeout(function() { btn.textContent = 'Copy cmd'; btn.classList.remove('copied'); }, 1800);
            }).catch(function() {
                btn.textContent = cmd;
                setTimeout(function() { btn.textContent = 'Copy cmd'; }, 3000);
            });
        }

        function fmtNum(n) {
            return n.toString().replace(/\\B(?=(\\d{3})+(?!\\d))/g, ',');
        }

        function escHtml(s) {
            if (!s) return '';
            return s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
        }

        function jsEsc(s) {
            if (!s) return '';
            return s.replace(/\\\\/g, '\\\\\\\\').replace(/'/g, "\\\\'").replace(/"/g, '&quot;');
        }

        /* ===== SSE / LIVE UPDATES ===== */
        var pollTimer = null;
        var lastReportRaw = null;
        var lastHistoryRaw = null;

        function stopPolling() {
            if (pollTimer) {
                clearInterval(pollTimer);
                pollTimer = null;
            }
        }

        function startPolling() {
            if (pollTimer) return;
            pollTimer = setInterval(function() {
                fetchJsonFresh('report.json').then(function(data) {
                    var raw = JSON.stringify(data);
                    if (raw !== lastReportRaw) {
                        lastReportRaw = raw;
                        REPORT = data;
                        router();
                    }
                }).catch(function() {});
            }, 10000);
        }

        function setLiveStatus(online, label) {
            var badge = document.getElementById('live-badge');
            var text = document.getElementById('live-text');
            var badgeDetail = document.getElementById('live-badge-detail');
            var textDetail = document.getElementById('live-text-detail');
            var cls = online ? 'live-badge online' : 'live-badge offline';
            if (badge) badge.className = cls;
            if (text) text.textContent = label;
            if (badgeDetail) badgeDetail.className = cls;
            if (textDetail) textDetail.textContent = label;
        }

        function isStaticDeployment() {
            if (!window.EventSource) return true;
            if (location.protocol.indexOf('http') !== 0) return true;
            var host = (location.hostname || '').toLowerCase();
            if (host.endsWith('github.io') || host === 'blam.info') return true;
            return false;
        }

        function connectSSE() {
            if (isStaticDeployment()) {
                setLiveStatus(false, 'Static');
                return;
            }

            var es;
            try {
                es = new EventSource('/events');
            } catch(e) {
                setLiveStatus(false, 'Static');
                return;
            }

            es.onopen = function() {
                stopPolling();
                setLiveStatus(true, 'Live');
            };

            es.addEventListener('report', function(e) {
                stopPolling();
                setLiveStatus(true, 'Live');
                if (e.data !== lastReportRaw) {
                    lastReportRaw = e.data;
                    try {
                        REPORT = JSON.parse(e.data);
                        router();
                    } catch(err) {}
                }
            });

            es.addEventListener('history', function(e) {
                if (e.data !== lastHistoryRaw) {
                    lastHistoryRaw = e.data;
                    try {
                        HISTORY = JSON.parse(e.data);
                        router();
                    } catch(err) {}
                }
            });

            es.onerror = function() {
                // If the SSE endpoint is unavailable or errors (e.g. static hosting or 404),
                // close immediately to prevent the browser from reconnecting every 3 seconds.
                try {
                    es.close();
                } catch(e) {}
                setLiveStatus(false, 'Static');
            };
        }

        /* ===== EVENT BINDING ===== */
        document.addEventListener('DOMContentLoaded', function() {
            // Overview search
            var searchInput = document.getElementById('unit-search');
            if (searchInput) {
                searchInput.addEventListener('input', function() {
                    filterText = this.value;
                    renderTable();
                });
            }

            // Function search (detail view)
            var funcSearch = document.getElementById('func-search');
            if (funcSearch) {
                funcSearch.addEventListener('input', function() {
                    funcFilterText = this.value;
                    var unit = findUnit(currentUnitName);
                    if (unit) renderFuncTable(unit.functions || []);
                });
            }

            // Overview sort on header click
            var headers = document.querySelectorAll('th[data-col]');
            for (var i = 0; i < headers.length; i++) {
                headers[i].addEventListener('click', function() {
                    var col = parseInt(this.getAttribute('data-col'));
                    if (sortCol === col) {
                        sortAsc = !sortAsc;
                    } else {
                        sortCol = col;
                        sortAsc = true;
                    }
                    renderTable();
                });
            }

            // Function sort on header click (detail view)
            var fHeaders = document.querySelectorAll('th[data-fcol]');
            for (var i = 0; i < fHeaders.length; i++) {
                fHeaders[i].addEventListener('click', function() {
                    var col = parseInt(this.getAttribute('data-fcol'));
                    if (funcSortCol === col) {
                        funcSortAsc = !funcSortAsc;
                    } else {
                        funcSortCol = col;
                        funcSortAsc = true;
                    }
                    for (var j = 0; j < fHeaders.length; j++) {
                        var arrow = fHeaders[j].querySelector('.sort-arrow');
                        var c = parseInt(fHeaders[j].getAttribute('data-fcol'));
                        if (c === col) {
                            arrow.textContent = funcSortAsc ? ' \\u25B2' : ' \\u25BC';
                        } else {
                            arrow.textContent = '';
                        }
                    }
                    var unit = findUnit(currentUnitName);
                    if (unit) renderFuncTable(unit.functions || []);
                });
            }

            // Back buttons
            var backBtns = document.querySelectorAll('#detail-back, #detail-back-bottom');
            for (var i = 0; i < backBtns.length; i++) {
                backBtns[i].addEventListener('click', function(e) {
                    e.preventDefault();
                    goHome();
                });
            }

            // Hash change
            window.addEventListener('hashchange', router);

            // Redraw address strip on resize
            var resizeTimer = null;
            window.addEventListener('resize', function() {
                clearTimeout(resizeTimer);
                resizeTimer = setTimeout(function() { renderAddrStrip(); renderTreemap(); }, 200);
            });
        });

        function findUnit(name) {
            for (var i = 0; i < REPORT.units.length; i++) {
                if (REPORT.units[i].name === name) return REPORT.units[i];
            }
            return null;
        }

        /* ===== INIT ===== */
        hydrateLiveSnapshot().finally(function() {
            router();
            connectSSE();
        });
    </script>
</body>
</html>'''

    html = TEMPLATE.replace('__REPORT_JSON__', report_json)\
                   .replace('__HISTORY_JSON__', history_json)\
                   .replace('__CI_SUMMARY_JSON__', ci_summary_json)

    write_text_atomic(output_path, html)


def update_readme_progress(report: dict, readme_path: str = 'README.md') -> bool:
    """Update README.md Game Code Progress section with data from current report."""
    root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '../..'))
    target_path = os.path.join(root_dir, readme_path) if not os.path.isabs(readme_path) else readme_path
    
    if not os.path.exists(target_path):
        return False

    summary = report.get('summary', {})
    funcs = summary.get('functions', {})
    bytes_data = summary.get('bytes', {})
    match_data = summary.get('match', {})
    equiv_data = summary.get('equivalence', {})
    platform_data = summary.get('platform', {})

    func_ported = funcs.get('ported', 0)
    func_total = funcs.get('total', 0)
    func_pct = funcs.get('percent', 0.0)

    bytes_ported = bytes_data.get('ported', 0)
    bytes_total = bytes_data.get('total', 0)
    bytes_pct = bytes_data.get('percent', 0.0)

    match_avg = match_data.get('average', 0.0) if match_data else 0.0
    match_weighted = match_data.get('weighted', 0.0) if match_data else 0.0
    scored_cnt = match_data.get('scored_count', 0) if match_data else 0

    equiv_tested = equiv_data.get('tested', 0) if equiv_data else 0
    equiv_hc = equiv_data.get('high_confidence', 0) if equiv_data else 0

    units = report.get('units', [])
    progress_units_cnt = len([u for u in units if not u.get('synthetic')])
    platform_units_cnt = platform_data.get('units', 0) if platform_data else 0

    def make_bar(pct, width=40):
        filled = int(round((max(0.0, min(100.0, pct)) / 100.0) * width))
        return '█' * filled + '░' * (width - filled)

    def color_badge(pct):
        if pct >= 90: return 'brightgreen'
        if pct >= 75: return 'green'
        if pct >= 50: return 'yellowgreen'
        if pct >= 25: return 'yellow'
        if pct >= 10: return 'orange'
        return 'red'

    badge_color = color_badge(func_pct)
    func_bar = make_bar(func_pct)
    bytes_bar = make_bar(bytes_pct)

    progress_content = (
        "<!-- GAME_CODE_PROGRESS_START -->\n"
        f"[![Decompilation Progress](https://img.shields.io/badge/decompilation-{func_pct:.2f}%25-{badge_color}.svg)](https://stianeklund.github.io/halo/)\n"
        f"[![Ported Functions](https://img.shields.io/badge/functions-{func_ported:,}%2F{func_total:,}-blue.svg)](https://stianeklund.github.io/halo/)\n\n"
        "Progress breakdown from the [Decompilation Progress Dashboard](https://stianeklund.github.io/halo/):\n\n"
        f"* **Ported Functions:** `{func_ported:,} / {func_total:,}` (`{func_pct:.2f}%`)\n"
        f"  `[{func_bar}] {func_pct:.2f}%`\n"
        f"* **Ported Code Bytes:** `{bytes_ported:,} / {bytes_total:,}` (`{bytes_pct:.2f}%`)\n"
        f"  `[{bytes_bar}] {bytes_pct:.2f}%`\n"
        f"* **Average VC71 Mnemonic Match:** `{match_avg:.2f}%` (`{scored_cnt:,}` scored functions, size-weighted: `{match_weighted:.2f}%`; structural signal, not raw-byte accuracy)\n"
        f"* **Equivalence Tests:** `{equiv_tested:,}` functions tested (`{equiv_hc:,}` high confidence)\n"
        f"* **Translation Units:** `{progress_units_cnt}` source units (`{platform_units_cnt}` platform/SDK buckets tracked separately)\n\n"
        "> Explore the interactive call graph and unit breakdown: **[Decompilation Progress Dashboard](https://stianeklund.github.io/halo/)** (or locally at [`artifacts/progress/index.html`](artifacts/progress/index.html))\n"
        "<!-- GAME_CODE_PROGRESS_END -->"
    )

    with open(target_path, 'r', encoding='utf-8') as f:
        content = f.read()

    start_marker = "<!-- GAME_CODE_PROGRESS_START -->"
    end_marker = "<!-- GAME_CODE_PROGRESS_END -->"

    if start_marker in content and end_marker in content:
        before = content.split(start_marker)[0]
        after = content.split(end_marker)[1]
        new_content = before + progress_content + after
    else:
        return False

    with open(target_path, 'w', encoding='utf-8') as f:
        f.write(new_content)
    return True


def main():
    ap = argparse.ArgumentParser(description='Generate decomp.dev-style progress report')
    ap.add_argument('-o', '--output', default='artifacts/progress/report.json',
                    help='Output JSON file path')
    ap.add_argument('--html', metavar='PATH', 
                    help='Also generate HTML dashboard at specified path')
    ap.add_argument('--pretty', action='store_true',
                    help='Pretty print JSON output')
    ap.add_argument('--history', default='artifacts/progress/history.json',
                    help='Path to history.json for historical charts')
    ap.add_argument('--readme', metavar='PATH', default='README.md',
                    help='Path to README.md to update with progress (default: README.md)')
    ap.add_argument('--no-readme', action='store_true',
                    help='Skip updating README.md')
    args = ap.parse_args()
    
    print('Generating decomp.dev-compatible report...')
    
    report = generate_report(args.output)
    
    print(f'\n✓ Report written to: {args.output}')
    print(f'\nSummary:')
    print(f'  Functions: {report["summary"]["functions"]["ported"]:,} / {report["summary"]["functions"]["total"]:,} ({report["summary"]["functions"]["percent"]:.2f}%)')
    print(f'  Bytes: {report["summary"]["bytes"]["ported"]:,} / {report["summary"]["bytes"]["total"]:,} ({report["summary"]["bytes"]["percent"]:.2f}%)')
    print(f'  Units: {len(report["units"])}')
    
    if args.html:
        generate_html(report, args.html, args.history)
        print(f'\n✓ HTML dashboard written to: {args.html}')

    if not args.no_readme:
        if update_readme_progress(report, args.readme):
            print(f'✓ Updated progress in {args.readme}')
    
    if args.pretty:
        print('\nJSON output:')
        print(json.dumps(report, indent=2)[:2000] + '...' if len(json.dumps(report)) > 2000 else '')


if __name__ == '__main__':
    main()
