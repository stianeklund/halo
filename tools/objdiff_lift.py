#!/usr/bin/env python3
# One-shot CLI wrapper for objdiff comparisons.
# For interactive/GUI diffing, open the repo root in objdiff — it reads objdiff.json
# which maps every delinked/<name>.obj (target) against the build's compiled .obj (base).
# Populate delinked/ using tools/export_delinked_object.py.
import argparse
import json
import os
import shutil
import subprocess
import sys


def parse_args() -> argparse.Namespace:
    ap = argparse.ArgumentParser(
        description='Run objdiff between a delinked reference object and a compiled candidate object.'
    )
    ap.add_argument('--target', default='',
                    help='Human-readable target name for reports')
    ap.add_argument('--reference', required=True,
                    help='Path to delinked reference object exported from Ghidra')
    ap.add_argument('--candidate', required=True,
                    help='Path to compiled candidate object from the Halo build')
    ap.add_argument('--output-json', default='',
                    help='Optional path to save a small JSON summary')
    ap.add_argument('--output-text', default='',
                    help='Optional path to save stdout from objdiff')
    ap.add_argument('--tool', default='objdiff',
                    help='objdiff executable name or path')
    ap.add_argument('objdiff_args', nargs='*',
                    help='Extra arguments forwarded to objdiff')
    return ap.parse_args()


def normalize_path(path: str) -> str:
    return os.path.abspath(path)


def ensure_exists(path: str, label: str):
    if not os.path.exists(path):
        print(f'{label} does not exist: {path}', file=sys.stderr)
        raise SystemExit(2)


def main():
    args = parse_args()
    reference = normalize_path(args.reference)
    candidate = normalize_path(args.candidate)

    ensure_exists(reference, 'Reference object')
    ensure_exists(candidate, 'Candidate object')

    tool = shutil.which(args.tool) if os.path.sep not in args.tool else args.tool
    if not tool:
        print(f'objdiff tool not found: {args.tool}', file=sys.stderr)
        raise SystemExit(2)

    target = args.target or os.path.splitext(os.path.basename(candidate))[0]
    command = [tool, reference, candidate] + args.objdiff_args
    result = subprocess.run(command, capture_output=True, text=True)

    header = [
        f'target: {target}',
        f'reference: {reference}',
        f'candidate: {candidate}',
        f'command: {" ".join(command)}',
        f'exit_code: {result.returncode}',
        '',
    ]
    text_report = '\n'.join(header) + result.stdout
    if result.stderr:
        text_report += '\n[stderr]\n' + result.stderr

    print(text_report)

    if args.output_text:
        with open(args.output_text, 'w') as f:
            f.write(text_report)

    if args.output_json:
        payload = {
            'target': target,
            'reference': reference,
            'candidate': candidate,
            'command': command,
            'exit_code': result.returncode,
            'stdout': result.stdout,
            'stderr': result.stderr,
        }
        with open(args.output_json, 'w') as f:
            json.dump(payload, f, indent=2)
            f.write('\n')

    raise SystemExit(result.returncode)


if __name__ == '__main__':
    main()
