#!/usr/bin/env bash
set -euo pipefail

python3 tools/build.py
python3 tools/deploy_xbox.py -x 127.0.0.1 --xbe-only
