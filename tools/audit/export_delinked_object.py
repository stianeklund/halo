#!/usr/bin/env python3

import sys, os
_tools_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)

import argparse
import os
import shlex
import subprocess
import sys


DEFAULT_GHIDRA_ROOT = '/mnt/c/Users/stian/AppData/Roaming/ghidra/ghidra_12.0.3_PUBLIC'
DEFAULT_PROJECT_DIR = '/mnt/c/Users/stian/AppData/Roaming/ghidra/ghidra_12.0.3_PUBLIC/projects'
DEFAULT_SCRIPT_DIR = '/mnt/c/Users/stian/ghidra_scripts'
DEFAULT_SCRIPT_NAME = 'DelinkProgram.java'
DEFAULT_PROGRAM = 'cachebeta.xbe'


def parse_args() -> argparse.Namespace:
    ap = argparse.ArgumentParser(
        description='Export a delinked reference object using Ghidra headless and DelinkProgram.java.'
    )
    ap.add_argument('--project-dir', default=DEFAULT_PROJECT_DIR,
                    help='Ghidra project directory')
    ap.add_argument('--project-name', default='cachebeta',
                    help='Ghidra project name')
    ap.add_argument('--program', default=DEFAULT_PROGRAM,
                    help='Program name inside the Ghidra project')
    ap.add_argument('--ghidra-root', default=DEFAULT_GHIDRA_ROOT,
                    help='Ghidra user root that contains support/analyzeHeadless')
    ap.add_argument('--script-dir', default=DEFAULT_SCRIPT_DIR,
                    help='Directory containing DelinkProgram.java')
    ap.add_argument('--script-name', default=DEFAULT_SCRIPT_NAME,
                    help='Headless delinker script name')
    ap.add_argument('--exporter', required=True,
                    help='Exporter name, e.g. Relocatable Object File (COFF)')
    ap.add_argument('--output', required=True,
                    help='Output object file path')

    target = ap.add_mutually_exclusive_group(required=True)
    target.add_argument('--symbol', help='Symbol name to include')
    target.add_argument('--range', dest='address_range',
                        help='Address range start-end, e.g. 000a8220-000a833e')

    ap.add_argument('--analyze', action='store_true',
                    help='Run analysis during import if the program must be opened')
    ap.add_argument('--verbose', action='store_true', help='Print headless command before running')
    return ap.parse_args()


def ensure_exists(path: str, label: str):
    if not os.path.exists(path):
        print(f'{label} does not exist: {path}', file=sys.stderr)
        raise SystemExit(2)


def main():
    args = parse_args()

    analyze_headless = os.path.join(args.ghidra_root, 'support', 'analyzeHeadless')
    analyze_headless_bat = os.path.join(args.ghidra_root, 'support', 'analyzeHeadless.bat')
    if os.name == 'nt' and os.path.exists(analyze_headless_bat):
        launcher = analyze_headless_bat
    elif os.path.exists(analyze_headless):
        launcher = analyze_headless
    elif os.path.exists(analyze_headless_bat):
        launcher = analyze_headless_bat
    else:
        print('analyzeHeadless does not exist in support/', file=sys.stderr)
        raise SystemExit(2)

    ensure_exists(args.project_dir, 'project directory')
    ensure_exists(args.script_dir, 'script directory')
    ensure_exists(os.path.join(args.script_dir, args.script_name), 'delinker script')

    output_path = os.path.abspath(args.output)
    os.makedirs(os.path.dirname(output_path), exist_ok=True)

    script_args = ['/exporter', args.exporter]
    if args.symbol:
        script_args.extend(['/include', args.symbol])
    else:
        script_args.extend(['/include-range', args.address_range])
    script_args.extend(['/export', output_path])

    command = [
        launcher,
        args.project_dir,
        args.project_name,
        '-process',
        args.program,
        '-scriptPath',
        args.script_dir,
        '-postScript',
        args.script_name,
        *script_args,
    ]

    if not args.analyze:
        command.append('-noanalysis')

    if args.verbose:
        print('command:')
        print(' '.join(command))

    if os.name == 'nt' and launcher.lower().endswith('.bat'):
        command_text = subprocess.list2cmdline(command)
        result = subprocess.run(['cmd.exe', '/c', command_text], capture_output=True, text=True)
    else:
        result = subprocess.run(command, capture_output=True, text=True)
    if result.stdout:
        print(result.stdout, end='')
    if result.stderr:
        print(result.stderr, end='', file=sys.stderr)

    if result.returncode != 0:
        raise SystemExit(result.returncode)

    if not os.path.exists(output_path):
        print(f'export did not produce output: {output_path}', file=sys.stderr)
        raise SystemExit(1)

    print(f'exported: {output_path}')


if __name__ == '__main__':
    main()
