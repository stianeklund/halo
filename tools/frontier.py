#!/usr/bin/env python3
if __name__ == "__main__":
    from check_requirements import check_requirements
    check_requirements()

import argparse
import logging
import os
from collections import defaultdict

import clang.cindex as clang

import color
from kb_meta import MetadataStore
from knowledge import Data, Function, KnowledgeBase

ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
SRC_ROOT = os.path.join(ROOT_DIR, 'src', 'halo')


def iter_source_files() -> list[str]:
    out = []
    for root, _, files in os.walk(SRC_ROOT):
        for filename in files:
            if filename.endswith('.c'):
                out.append(os.path.join(root, filename))
    return sorted(out)


def parse_called_functions(path: str, known_names: set[str]) -> set[str]:
    with open(path) as f:
        text = f.read()
    index = clang.Index.create()
    tu = index.parse(path, unsaved_files=[(path, text)], options=0)
    called = set()

    def visit(cursor: clang.Cursor):
        if cursor.kind == clang.CursorKind.CALL_EXPR:
            name = cursor.spelling
            if name in known_names:
                called.add(name)
        for child in cursor.get_children():
            visit(child)

    visit(tu.cursor)
    return called


def implemented_names(kb: KnowledgeBase) -> set[str]:
    names = set()
    index = clang.Index.create()
    for path in iter_source_files():
        with open(path) as f:
            text = f.read()
        tu = index.parse(path, unsaved_files=[(path, text)], options=0)
        for cursor in tu.cursor.get_children():
            if (cursor.kind == clang.CursorKind.FUNCTION_DECL and
                    cursor.is_definition() and cursor.spelling in kb.name_to_addr):
                names.add(cursor.spelling)
    return names


def score_object(frontier_refs: int, unresolved_count: int,
                 ported_count: int, confirmed_names: int) -> int:
    return (unresolved_count * 5 +
            frontier_refs * 2 +
            (3 if ported_count > 0 else 0) +
            confirmed_names -
            ported_count)


def function_symbols_for_object(kb: KnowledgeBase, object_name: str) -> list[Function]:
    return [
        symbol for symbol in kb.object_to_symbols.get(object_name, [])
        if isinstance(symbol, Function)
    ]


def object_ported_and_remaining(kb: KnowledgeBase, store: MetadataStore,
                                object_name: str) -> tuple[int, int]:
    ported = 0
    remaining = 0
    for symbol in function_symbols_for_object(kb, object_name):
        meta = store.symbols.get(f'{symbol.addr:#x}')
        status = meta.status if meta else 'unknown'
        if status in {'ported', 'verified'}:
            ported += 1
        else:
            remaining += 1
    return ported, remaining


def print_section(title: str):
    print(title)
    print('-' * len(title))


def print_object_table(rows: list[dict[str, object]]):
    for row in rows:
        obj = row['object'] or '(unknown)'
        print(
            f"{obj:<30}  {row['ported']:>3}/{row['total']:<3}  "
            f"remaining={row['remaining']:<3}  frontier={row['frontier_refs']:<3}  "
            f"score={row['score']:<3}  src={row['source']}"
        )


def sort_object_name(row: dict[str, object]) -> str:
    name = row['object']
    if isinstance(name, str):
        return name
    return '~unknown~'


def main():
    logging.basicConfig(level=logging.INFO, handlers=[color.ColorLogHandler()])
    ap = argparse.ArgumentParser(description='Rank frontier objects from current ported code.')
    ap.add_argument('--limit', type=int, default=20)
    args = ap.parse_args()

    kb = KnowledgeBase.deserialize()
    store = MetadataStore(kb)
    store.load()

    known_names = set(kb.name_to_addr)
    ported_names = implemented_names(kb)
    object_refs = defaultdict(int)
    object_unresolved = defaultdict(set)
    object_ported = defaultdict(set)
    object_confirmed_names = defaultdict(set)

    for name in ported_names:
        addr = kb.name_to_addr[name]
        symbol = kb.addr_to_symbol[addr]
        obj = kb.symbol_to_object.get(symbol)
        if obj:
            object_ported[obj].add(name)

    index = clang.Index.create()
    for path in iter_source_files():
        with open(path) as f:
            text = f.read()
        tu = index.parse(path, unsaved_files=[(path, text)], options=0)
        called = parse_called_functions(path, known_names)
        for callee in called:
            addr = kb.name_to_addr[callee]
            symbol = kb.addr_to_symbol[addr]
            obj = kb.symbol_to_object.get(symbol)
            if obj is None:
                continue
            object_refs[obj] += 1
            meta = store.symbols.get(f'{addr:#x}')
            status = meta.status if meta else 'unknown'
            if status != 'ported' and callee not in ported_names:
                object_unresolved[obj].add(callee)
            if meta and meta.name_confidence == 'confirmed':
                object_confirmed_names[obj].add(callee)

    ranked = []
    total_functions = 0
    total_ported = 0
    quick_wins = []
    active_tus = []

    for obj in kb.object_to_symbols:
        total = len(function_symbols_for_object(kb, obj))
        if total == 0:
            continue
        total_functions += total
        ported_total, remaining_total = object_ported_and_remaining(kb, store, obj)
        total_ported += ported_total

        unresolved = object_unresolved[obj]
        refs = object_refs[obj]
        ported = object_ported[obj]
        confirmed = object_confirmed_names[obj]
        row = {
            'object': obj,
            'source': kb.object_to_source.get(obj, '?'),
            'score': score_object(refs, len(unresolved), len(ported), len(confirmed)),
            'frontier_refs': refs,
            'unresolved': sorted(unresolved, key=lambda name: kb.name_to_addr[name]),
            'ported_count': len(ported),
            'confirmed_names': len(confirmed),
            'ported': ported_total,
            'remaining': remaining_total,
            'total': total,
        }
        if 0 < ported_total < total:
            active_tus.append(row)
        if ported_total == 0 and 1 <= total <= 3:
            quick_wins.append(row)
        if refs > 0 or unresolved:
            ranked.append(row)

    ranked.sort(key=lambda row: (-row['score'], sort_object_name(row)))
    active_tus.sort(key=lambda row: (row['remaining'], -row['ported'], sort_object_name(row)))
    quick_wins.sort(key=lambda row: (row['remaining'], sort_object_name(row)))

    print_section('Coverage')
    percent = (total_ported / total_functions * 100.0) if total_functions else 0.0
    print(f'Functions: {total_ported}/{total_functions} ({percent:.1f}%)')
    print()

    print_section('Active TUs')
    print_object_table(active_tus[:args.limit])
    print()

    print_section('Quick Wins')
    print_object_table(quick_wins[:args.limit])
    print()

    print_section('Recommended Next Targets')
    print_object_table(ranked[:args.limit])

    for row in ranked[:args.limit]:
        print(f"  candidates for {row['object']}:")
        for name in row['unresolved'][:5]:
            print(f"  - {kb.name_to_addr[name]:#x} {name}")


if __name__ == '__main__':
    main()
