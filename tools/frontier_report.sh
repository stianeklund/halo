#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
limit="${1:-20}"

echo "== Frontier =="
python3 "$repo_root/tools/frontier.py" --limit "$limit"

echo
echo "== Progress (by object) =="
python3 "$repo_root/tools/progress.py" --by-object
