#!/usr/bin/env python3
"""Run multi-frame trajectory equivalence sweeps across unpatched .halorec recordings.

Iterates unpatched recordings in /mnt/g/dev/halo-memory-viewer/recordings/unpatched,
executes multi-frame sweeps across target functions using halorec_frame_sweep.py,
and records grand coverage unions and divergence findings into artifacts/trajectory_sweeps.
"""

import json
import os
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
UNPATCHED_DIR = Path("/mnt/g/dev/halo-memory-viewer/recordings/unpatched")

DEFAULT_TARGETS = [
    "actor_action_perform",
    "actor_action_update",
    "actor_action_handle_panic_from_damage",
    "actor_action_handle_berserking_from_damage",
    "actor_action_try_to_seek_cover",
    "actor_action_try_to_enter_vehicle",
]


def main():
    if not UNPATCHED_DIR.exists():
        print(f"[trajectory-sweeps] Unpatched directory not found at {UNPATCHED_DIR}")
        return 0

    recordings = sorted(list(UNPATCHED_DIR.glob("*.halorec")))
    if not recordings:
        print(f"[trajectory-sweeps] No .halorec recordings found in {UNPATCHED_DIR}")
        return 0

    print(f"[trajectory-sweeps] Found {len(recordings)} unpatched .halorec recordings.")

    output_dir = ROOT / "artifacts" / "trajectory_sweeps"
    output_dir.mkdir(parents=True, exist_ok=True)

    summary_results = []

    for target in DEFAULT_TARGETS:
        for rec in recordings[:3]:  # Sweep top recordings
            rec_name = rec.name
            out_json = output_dir / f"{target}_{rec.stem}.json"
            print(f"\n[sweep] Target: {target:35s} | Recording: {rec_name}")

            cmd = [
                sys.executable,
                str(HERE / "halorec_frame_sweep.py"),
                target,
                "--recording", str(rec),
                "--frames", "every:10",
                "--seeds", "10",
                "--allow-stubs",
                "--float-tolerance", "32",
                "--mem-trace",
                "--json", str(out_json),
            ]

            try:
                proc = subprocess.run(cmd, capture_output=True, text=True, cwd=str(ROOT))
                print(proc.stdout)
                if out_json.exists():
                    res_data = json.loads(out_json.read_text(encoding="utf-8"))
                    summary_results.append(res_data)
            except Exception as e:
                print(f"  [error] Sweep failed: {e}")

    summary_file = output_dir / "summary.json"
    summary_file.write_text(json.dumps(summary_results, indent=2), encoding="utf-8")
    print(f"\n[trajectory-sweeps] Multi-frame sweep complete! Saved summary to {summary_file}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
