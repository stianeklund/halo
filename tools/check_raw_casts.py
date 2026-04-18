#!/usr/bin/env python3
"""Ratchet check: fail if raw function-pointer casts to hex addresses increase.

Raw casts like ((void (*)(int))0x231220)(...) bypass kb.json and the thunk
system, hiding calling-convention mismatches (stdcall vs cdecl).  This check
counts them across src/ and fails the build if the count exceeds the recorded
baseline, preventing new ones from slipping in.

Usage:
    python3 tools/check_raw_casts.py [--update]

    --update   Rewrite the baseline to the current count (use after cleaning
               up raw casts to lower the bar).
"""
import os
import re
import sys

ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
SRC_DIR = os.path.join(ROOT_DIR, 'src')
BASELINE_FILE = os.path.join(ROOT_DIR, 'tools', 'raw_cast_baseline.txt')

PATTERN = re.compile(r'\(\(.*\(\*\).*\)0x[0-9a-fA-F]')


def count_raw_casts():
    total = 0
    for dirpath, _, filenames in os.walk(SRC_DIR):
        for fname in filenames:
            if not fname.endswith('.c'):
                continue
            fpath = os.path.join(dirpath, fname)
            with open(fpath, 'r', errors='replace') as f:
                for line in f:
                    total += len(PATTERN.findall(line))
    return total


def read_baseline():
    if not os.path.exists(BASELINE_FILE):
        return None
    with open(BASELINE_FILE) as f:
        return int(f.read().strip())


def write_baseline(count):
    with open(BASELINE_FILE, 'w') as f:
        f.write(f'{count}\n')


def main():
    update = '--update' in sys.argv

    current = count_raw_casts()

    if update:
        write_baseline(current)
        print(f'raw-cast baseline updated to {current}')
        return 0

    baseline = read_baseline()
    if baseline is None:
        write_baseline(current)
        print(f'raw-cast baseline initialized at {current}')
        return 0

    if current > baseline:
        print(
            f'ERROR: raw function-pointer casts increased from {baseline} to '
            f'{current} (+{current - baseline}). Add callees to kb.json '
            f'instead of using raw casts. Run with --update after fixing.',
            file=sys.stderr,
        )
        return 1

    if current < baseline:
        write_baseline(current)
        print(f'raw-cast count decreased {baseline} -> {current}, baseline updated')
    else:
        print(f'raw-cast count: {current} (baseline: {baseline})')

    return 0


if __name__ == '__main__':
    sys.exit(main())
