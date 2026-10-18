#!/usr/bin/env python3
import sys, os
_tools_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)

"""Ratchet check: fail if raw function-pointer casts to hex addresses increase.

Raw casts like ((void (*)(int))0x231220)(...) bypass kb.json and the thunk
system, hiding calling-convention mismatches (stdcall vs cdecl).  This check
counts them across src/ and fails the build if the count exceeds the recorded
baseline, preventing new ones from slipping in.

Usage:
    python3 tools/audit/check_raw_casts.py [--update]

    --update   Rewrite the baseline to the current count (use after cleaning
               up raw casts to lower the bar).
"""
import os
import re
import sys

ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), '../..'))
SRC_DIR = os.path.join(ROOT_DIR, 'src')
BASELINE_FILE = os.path.join(ROOT_DIR, 'tools', 'raw_cast_baseline.txt')

PATTERN = re.compile(r'\(\(.*\(\*\).*\)0x[0-9a-fA-F]')


def count_raw_casts():
    total = 0
    for dirpath, _, filenames in os.walk(SRC_DIR):
        for fname in filenames:
            # Dotfiles are tool scratch (e.g. leaked .permuter_fast_* TU copies),
            # never build sources.
            if not fname.endswith('.c') or fname.startswith('.'):
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


def truncated_tracked_sources():
    """Tracked src/*.c files that are missing or empty on disk.

    A campaign worktree once held a 0-byte actor_perception.c mid-run; the
    auto-ratchet below lowered the baseline 195->182 during that window, and
    every later lift failed the gate once the file came back (campaign 09-27
    run 3). Never ratchet down while the tree is in that state.
    """
    import subprocess
    try:
        out = subprocess.run(['git', '-C', ROOT_DIR, 'ls-files', '-s', '--', 'src/*.c'],
                             capture_output=True, text=True, check=True).stdout
    except (OSError, subprocess.CalledProcessError):
        return []
    empty_blob = 'e69de29bb2d1d6434b8b29ae775ad8c2e48c5391'
    bad = []
    for line in out.splitlines():
        meta, rel = line.split('\t', 1)
        if meta.split()[1] == empty_blob:
            continue  # empty in the index too: an intentional placeholder
        path = os.path.join(ROOT_DIR, rel)
        if not os.path.exists(path) or os.path.getsize(path) == 0:
            bad.append(rel)
    return bad


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
        truncated = truncated_tracked_sources()
        if truncated:
            print(
                f'WARNING: raw-cast count {current} < baseline {baseline}, but '
                f'tracked sources are missing or empty: {", ".join(truncated)}. '
                f'Baseline NOT lowered.',
                file=sys.stderr,
            )
            return 0
        write_baseline(current)
        print(f'raw-cast count decreased {baseline} -> {current}, baseline updated')
    else:
        print(f'raw-cast count: {current} (baseline: {baseline})')

    return 0


if __name__ == '__main__':
    sys.exit(main())
