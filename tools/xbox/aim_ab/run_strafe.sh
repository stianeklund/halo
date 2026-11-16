#!/usr/bin/env bash
# run_phase.sh NAME CAMO BIT
set -e
mkdir -p /tmp/aimtrace
cd /mnt/g/dev/halo; export HALO_WINDOWS_REEXEC=1; H=10.0.0.21
camo(){ python3 -I -c "import sys;sys.argv=['camo.py','$H',*sys.argv[1:]];exec(open('/mnt/g/dev/halo/tools/xbox/aim_ab/camo.py').read())" "$@"; }
camo set e2a80039 "$2" "$3" | tail -1
timeout 60 python3 tools/xbox/aim_assist_debug.py --host $H --script /mnt/g/dev/halo/tools/xbox/aim_ab/strafe.input.txt --end-tick 122 \
  --pose 1,9.983,-1.199,-3.258,131 --pose 0,7.359,1.822,-2.86,-49.2,-6 | tail -1
sleep 6
timeout 30 python3 tools/xbox/aim_assist_debug.py --host $H --script-status | tail -1
timeout 120 python3 tools/xbox/aim_assist_debug.py --host $H --dump /tmp/aimtrace/camo_$1.csv | tail -1
camo show | tail -2
