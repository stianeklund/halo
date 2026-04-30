#!/usr/bin/env python3
import sys, os
_tools_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)

if __name__ == "__main__":
    from audit.check_requirements import check_requirements
    check_requirements()

import argparse
import json
import logging
import os
from dataclasses import dataclass, field
from typing import Any, Optional

from internal import color
from analysis.knowledge import Data, Function, KnowledgeBase

log = logging.getLogger(__name__)

ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "../.."))
KB_META_PATH = os.path.join(ROOT_DIR, 'kb_meta.json')
VALID_CONFIDENCE = {'low', 'medium', 'high', 'confirmed'}
VALID_STATUS = {'unknown', 'named', 'typed', 'ported', 'verified'}
VALID_KIND = {'function', 'data'}
PROVENANCE_KINDS = {
    'pattern_match',
    'callsite_inference',
    'sdk_header',
    'sibling_build_diff',
    'runtime_confirmed',
    'manual',
}


def normalize_addr(addr: str) -> str:
    return f'{int(addr, 0):#x}'


def _has_function_body(source_path: str, func_name: str) -> bool:
    """Return True if source_path contains a function definition (not just a
    declaration or call reference) for func_name.

    A function is considered defined if we find: name ( ... ) { — i.e. the
    identifier followed by a parameter list and an opening brace, with no
    semicolon between the closing paren and the brace.
    """
    import re
    try:
        content = open(source_path, encoding='utf-8', errors='replace').read()
    except OSError:
        return False
    pattern = rf'\b{re.escape(func_name)}\s*\([^;]*?\)\s*\{{'
    return bool(re.search(pattern, content, re.DOTALL))


@dataclass
class SymbolMetadata:
    kind: str
    name: Optional[str] = None
    name_confidence: Optional[str] = None
    signature_confidence: Optional[str] = None
    status: str = 'unknown'
    provenance: list[dict[str, str]] = field(default_factory=list)
    comments: dict[str, str] = field(default_factory=dict)
    flags: dict[str, bool] = field(default_factory=dict)
    inferred: list[str] = field(default_factory=list)
    uncertain: list[str] = field(default_factory=list)

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> 'SymbolMetadata':
        return cls(
            kind=data.get('kind', 'function'),
            name=data.get('name'),
            name_confidence=data.get('name_confidence'),
            signature_confidence=data.get('signature_confidence'),
            status=data.get('status', 'unknown'),
            provenance=list(data.get('provenance', [])),
            comments=dict(data.get('comments', {})),
            flags=dict(data.get('flags', {})),
            inferred=list(data.get('inferred', [])),
            uncertain=list(data.get('uncertain', [])),
        )

    def to_dict(self) -> dict[str, Any]:
        out: dict[str, Any] = {
            'kind': self.kind,
            'status': self.status,
        }
        if self.name:
            out['name'] = self.name
        if self.name_confidence:
            out['name_confidence'] = self.name_confidence
        if self.signature_confidence:
            out['signature_confidence'] = self.signature_confidence
        if self.provenance:
            out['provenance'] = self.provenance
        if self.comments:
            out['comments'] = self.comments
        if self.flags:
            out['flags'] = self.flags
        if self.inferred:
            out['inferred'] = self.inferred
        if self.uncertain:
            out['uncertain'] = self.uncertain
        return out


class MetadataStore:
    def __init__(self, kb: KnowledgeBase, path: str = KB_META_PATH):
        self.kb = kb
        self.path = path
        self.md5 = kb.expected_md5
        self.symbols: dict[str, SymbolMetadata] = {}

    def load(self):
        if not os.path.exists(self.path):
            return
        with open(self.path) as f:
            payload = json.load(f)
        self.md5 = payload.get('md5', self.kb.expected_md5)
        for addr, data in payload.get('symbols', {}).items():
            self.symbols[normalize_addr(addr)] = SymbolMetadata.from_dict(data)

    def save(self):
        payload = {
            'md5': self.md5,
            'symbols': {
                addr: self.symbols[addr].to_dict()
                for addr in sorted(self.symbols, key=lambda value: int(value, 16))
            },
        }
        with open(self.path, 'w') as f:
            json.dump(payload, f, indent=2)
            f.write('\n')

    def validate(self) -> list[str]:
        errors = []
        kb_addrs = {normalize_addr(hex(symbol.addr)) for symbol in self.kb.symbols if symbol.addr is not None}
        for addr, symbol in self.symbols.items():
            if addr not in kb_addrs:
                errors.append(f'{addr}: metadata address does not exist in kb.json')
            if symbol.kind not in VALID_KIND:
                errors.append(f'{addr}: invalid kind {symbol.kind!r}')
            if symbol.name_confidence and symbol.name_confidence not in VALID_CONFIDENCE:
                errors.append(f'{addr}: invalid name_confidence {symbol.name_confidence!r}')
            if (symbol.signature_confidence and
                    symbol.signature_confidence not in VALID_CONFIDENCE):
                errors.append(f'{addr}: invalid signature_confidence {symbol.signature_confidence!r}')
            if symbol.status not in VALID_STATUS:
                errors.append(f'{addr}: invalid status {symbol.status!r}')
            for entry in symbol.provenance:
                kind = entry.get('kind')
                if kind not in PROVENANCE_KINDS:
                    errors.append(f'{addr}: invalid provenance kind {kind!r}')
            summary = symbol.comments.get('summary')
            if summary is not None and not isinstance(summary, str):
                errors.append(f'{addr}: comments.summary must be a string')
            for key, value in symbol.flags.items():
                if not isinstance(value, bool):
                    errors.append(f'{addr}: flags.{key} must be boolean')
            for i, entry in enumerate(symbol.inferred):
                if not isinstance(entry, str):
                    errors.append(f'{addr}: inferred[{i}] must be a string')
            for i, entry in enumerate(symbol.uncertain):
                if not isinstance(entry, str):
                    errors.append(f'{addr}: uncertain[{i}] must be a string')
        if self.md5 != self.kb.expected_md5:
            errors.append(
                f'md5 mismatch: kb_meta.json={self.md5} kb.json={self.kb.expected_md5}')
        return errors

    def ensure_symbol(self, addr: str) -> SymbolMetadata:
        addr = normalize_addr(addr)
        if addr not in self.symbols:
            symbol = self.kb.addr_to_symbol.get(int(addr, 16))
            if symbol is None:
                raise KeyError(addr)
            kind = 'function' if isinstance(symbol, Function) else 'data'
            self.symbols[addr] = SymbolMetadata(kind=kind)
        return self.symbols[addr]

    def kb_symbol_rows(self) -> list[dict[str, str]]:
        rows = []
        for symbol in sorted(self.kb.symbols, key=lambda item: item.addr):
            if symbol.addr is None:
                continue
            addr = normalize_addr(hex(symbol.addr))
            meta = self.symbols.get(addr)
            obj = self.kb.symbol_to_object.get(symbol)
            rows.append({
                'addr': addr,
                'decl_name': symbol.name,
                'kind': 'function' if isinstance(symbol, Function) else 'data',
                'object': obj or '?',
                'source': self.kb.object_to_source.get(obj, '?'),
                'status': meta.status if meta else 'unknown',
                'name': meta.name if meta and meta.name else symbol.name,
                'name_confidence': meta.name_confidence if meta and meta.name_confidence else '-',
            })
        return rows


def build_parser() -> argparse.ArgumentParser:
    ap = argparse.ArgumentParser(description='Manage low-risk reverse engineering metadata.')
    sub = ap.add_subparsers(dest='command', required=True)

    sub.add_parser('validate', help='Validate kb_meta.json against kb.json')
    sub.add_parser('summary', help='Show metadata coverage summary')

    list_parser = sub.add_parser('list', help='List symbols with optional filters')
    list_parser.add_argument('--status', choices=sorted(VALID_STATUS))
    list_parser.add_argument('--kind', choices=sorted(VALID_KIND))
    list_parser.add_argument('--object', default='')
    list_parser.add_argument('--has-inferred', action='store_true',
                             help='Only show symbols with non-empty inferred notes')
    list_parser.add_argument('--has-uncertain', action='store_true',
                             help='Only show symbols with non-empty uncertain notes')

    status_parser = sub.add_parser('set-status', help='Set metadata status for a symbol')
    status_parser.add_argument('--addr', required=True)
    status_parser.add_argument('--status', required=True, choices=sorted(VALID_STATUS))

    name_parser = sub.add_parser('annotate-name', help='Annotate a symbol name and confidence')
    name_parser.add_argument('--addr', required=True)
    name_parser.add_argument('--name', required=True)
    name_parser.add_argument('--confidence', required=True, choices=sorted(VALID_CONFIDENCE))

    summary_parser = sub.add_parser('set-summary', help='Store a short summary comment')
    summary_parser.add_argument('--addr', required=True)
    summary_parser.add_argument('--summary', required=True)

    notes_parser = sub.add_parser(
        'annotate-notes',
        help='Append, replace, or clear inferred/uncertain notes for a symbol',
    )
    notes_parser.add_argument('--addr', required=True)
    notes_parser.add_argument(
        '--kind',
        required=True,
        choices=['inferred', 'uncertain'],
        help='Which note list to modify',
    )
    notes_parser.add_argument('--append', metavar='NOTE',
                              help='Append a note string to the list')
    notes_parser.add_argument('--clear', action='store_true',
                              help='Clear all notes of this kind')

    sync_parser = sub.add_parser(
        'sync-ported',
        help='Scan source files and mark functions with real implementations as ported',
    )
    sync_parser.add_argument('--dry-run', action='store_true',
                             help='Print what would change without saving')

    return ap


def print_rows(rows: list[dict[str, str]]):
    for row in rows:
        print(
            f"{row['addr']:>10}  {row['kind']:<8}  {row['status']:<8}  "
            f"{row['object']:<28}  {row['name']}"
        )


def validate_fast() -> list[str]:
    """Validate kb_meta.json against kb.json using raw JSON (no clang)."""
    kb_path = os.path.join(ROOT_DIR, 'kb.json')
    with open(kb_path) as f:
        kb_raw = json.load(f)

    kb_addrs: set[str] = set()
    for obj in kb_raw.get('objects', []):
        for kind in ('functions', 'data'):
            for sym in obj.get(kind, []):
                addr = sym.get('addr')
                if addr is not None:
                    kb_addrs.add(normalize_addr(addr))
    kb_md5 = kb_raw.get('md5')

    if not os.path.exists(KB_META_PATH):
        return []
    with open(KB_META_PATH) as f:
        meta_raw = json.load(f)

    errors = []
    meta_md5 = meta_raw.get('md5')
    for addr_str, data in meta_raw.get('symbols', {}).items():
        addr = normalize_addr(addr_str)
        if addr not in kb_addrs:
            errors.append(f'{addr}: metadata address does not exist in kb.json')
        kind = data.get('kind', 'function')
        if kind not in VALID_KIND:
            errors.append(f'{addr}: invalid kind {kind!r}')
        nc = data.get('name_confidence')
        if nc and nc not in VALID_CONFIDENCE:
            errors.append(f'{addr}: invalid name_confidence {nc!r}')
        sc = data.get('signature_confidence')
        if sc and sc not in VALID_CONFIDENCE:
            errors.append(f'{addr}: invalid signature_confidence {sc!r}')
        status = data.get('status', 'unknown')
        if status not in VALID_STATUS:
            errors.append(f'{addr}: invalid status {status!r}')
        for entry in data.get('provenance', []):
            pk = entry.get('kind')
            if pk not in PROVENANCE_KINDS:
                errors.append(f'{addr}: invalid provenance kind {pk!r}')
        summary = data.get('comments', {}).get('summary')
        if summary is not None and not isinstance(summary, str):
            errors.append(f'{addr}: comments.summary must be a string')
        for key, value in data.get('flags', {}).items():
            if not isinstance(value, bool):
                errors.append(f'{addr}: flags.{key} must be boolean')
        for i, entry in enumerate(data.get('inferred', [])):
            if not isinstance(entry, str):
                errors.append(f'{addr}: inferred[{i}] must be a string')
        for i, entry in enumerate(data.get('uncertain', [])):
            if not isinstance(entry, str):
                errors.append(f'{addr}: uncertain[{i}] must be a string')
    if meta_md5 != kb_md5:
        errors.append(f'md5 mismatch: kb_meta.json={meta_md5} kb.json={kb_md5}')
    return errors


def main():
    logging.basicConfig(level=logging.INFO, handlers=[color.ColorLogHandler()])
    args = build_parser().parse_args()

    if args.command == 'validate':
        errors = validate_fast()
        if errors:
            for error in errors:
                print(error)
            raise SystemExit(1)
        print('kb_meta.json is valid')
        return

    kb = KnowledgeBase.deserialize()
    store = MetadataStore(kb)
    store.load()

    if args.command == 'summary':
        rows = store.kb_symbol_rows()
        known = len([row for row in rows if row['status'] != 'unknown'])
        ported = len([row for row in rows if row['status'] == 'ported'])
        verified = len([row for row in rows if row['status'] == 'verified'])
        with_inferred = len([s for s in store.symbols.values() if s.inferred])
        with_uncertain = len([s for s in store.symbols.values() if s.uncertain])
        print(f'total symbols: {len(rows)}')
        print(f'annotated symbols: {known}')
        print(f'ported symbols: {ported}')
        print(f'verified symbols: {verified}')
        print(f'symbols with inferred notes: {with_inferred}')
        print(f'symbols with uncertain notes: {with_uncertain}')
        return

    if args.command == 'list':
        rows = store.kb_symbol_rows()
        if args.status:
            rows = [row for row in rows if row['status'] == args.status]
        if args.kind:
            rows = [row for row in rows if row['kind'] == args.kind]
        if args.object:
            rows = [row for row in rows if row['object'] == args.object]
        if args.has_inferred:
            rows = [
                row for row in rows
                if store.symbols.get(row['addr']) and store.symbols[row['addr']].inferred
            ]
        if args.has_uncertain:
            rows = [
                row for row in rows
                if store.symbols.get(row['addr']) and store.symbols[row['addr']].uncertain
            ]
        print_rows(rows)
        return

    if args.command == 'set-status':
        symbol = store.ensure_symbol(args.addr)
        symbol.status = args.status
        store.save()
        print(f'{normalize_addr(args.addr)} status={args.status}')
        return

    if args.command == 'annotate-name':
        symbol = store.ensure_symbol(args.addr)
        symbol.name = args.name
        symbol.name_confidence = args.confidence
        store.save()
        print(f'{normalize_addr(args.addr)} name={args.name} confidence={args.confidence}')
        return

    if args.command == 'set-summary':
        symbol = store.ensure_symbol(args.addr)
        symbol.comments['summary'] = args.summary
        store.save()
        print(f'{normalize_addr(args.addr)} summary updated')
        return

    if args.command == 'annotate-notes':
        if not args.clear and not args.append:
            print('error: one of --append or --clear is required')
            raise SystemExit(1)
        symbol = store.ensure_symbol(args.addr)
        target_list: list[str] = getattr(symbol, args.kind)
        if args.clear:
            target_list.clear()
            store.save()
            print(f'{normalize_addr(args.addr)} {args.kind} cleared')
        if args.append:
            target_list.append(args.append)
            store.save()
            print(f'{normalize_addr(args.addr)} {args.kind} appended')
        return

    if args.command == 'sync-ported':
        import re
        updated = []
        for sym in kb.symbols:
            if not isinstance(sym, Function) or sym.addr is None:
                continue
            addr = normalize_addr(hex(sym.addr))
            meta = store.symbols.get(addr)
            if meta and meta.status in ('ported', 'verified'):
                continue
            obj_name = store.kb.symbol_to_object.get(sym)
            source = store.kb.object_to_source.get(obj_name, '') if obj_name else ''
            if not source:
                continue
            src_path = os.path.join(ROOT_DIR, 'src', 'halo', source)
            name_parts = re.split(r'[\s\*]+', (sym.decl or '').split('(')[0])
            name = name_parts[-1] if name_parts else ''
            if not name or name.startswith('FUN_'):
                continue
            if _has_function_body(src_path, name):
                updated.append((addr, name, source))
                if not args.dry_run:
                    entry = store.ensure_symbol(addr)
                    entry.status = 'ported'

        if updated:
            if not args.dry_run:
                store.save()
            verb = 'would mark' if args.dry_run else 'marked'
            print(f'{verb} {len(updated)} function(s) as ported:')
            for addr, name, source in sorted(updated):
                print(f'  {addr:>10}  {name:<45}  {source}')
        else:
            print('all implemented functions are already tracked')
        return


if __name__ == '__main__':
    main()
