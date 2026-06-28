#!/usr/bin/env python3
"""Generate host-only equivalence state snapshots from .halorec recordings.

COPYRIGHT BOUNDARY: `.halorec` recordings and the snapshot JSONs produced here
contain raw Halo runtime memory (copyrighted). They are gitignored and live
host-only. Only this script and host_snapshots/manifest.json (our own metadata)
are committed. CI runs this on the self-hosted runner where the recordings exist;
anywhere a recordings directory isn't found it prints a notice and exits 0, so the
snapshot-dependent regression targets SKIP rather than fail.

Usage:
    HALO_HALOREC_DIR=/path/to/recordings python3 tools/equivalence/build_host_snapshots.py
"""

import json
import os
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
SNAP_DIR = HERE / "host_snapshots"
MANIFEST = SNAP_DIR / "manifest.json"
sys.path.insert(0, str(HERE))  # for halorec_to_snapshot


def _find_recordings_dir(manifest):
    env_name = manifest.get("recordings_dir_env", "HALO_HALOREC_DIR")
    candidates = []
    if os.environ.get(env_name):
        candidates.append(os.environ[env_name])
    candidates += manifest.get("recordings_dir_defaults", [])
    for c in candidates:
        p = Path(c).expanduser()
        if p.is_dir():
            return p, env_name
    return None, env_name


def _parse_frame(sel):
    """Mirror unicorn_diff's --halorec-frame selector parsing."""
    if sel is None or sel in ("last", "first"):
        return {"frame": sel}
    if sel.startswith("t="):
        return {"t": float(sel[2:])}
    if sel.startswith("handle="):
        return {"handle": int(sel[7:], 0)}
    return {"frame": sel}


def main():
    from halorec_to_snapshot import build_snapshot

    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    rec_dir, env_name = _find_recordings_dir(manifest)
    if rec_dir is None:
        print(f"[host-snapshots] no recordings directory found "
              f"(set ${env_name}); skipping build — snapshot-dependent "
              f"regression targets will SKIP.")
        return 0

    built = skipped = 0
    for s in manifest.get("snapshots", []):
        rec = rec_dir / s["recording"]
        out = SNAP_DIR / s["out"]
        if not rec.exists():
            print(f"[host-snapshots] SKIP {s['out']}: recording not found ({rec})")
            skipped += 1
            continue
        args = {k: (int(v, 0) if isinstance(v, str) else v)
                for k, v in (s.get("args") or {}).items()}
        snap, idx, t = build_snapshot(
            str(rec), args=args, build_label=s["out"],
            **_parse_frame(s.get("frame")),
        )
        out.write_text(json.dumps(snap) + "\n", encoding="utf-8")
        print(f"[host-snapshots] built {s['out']} "
              f"(frame {idx}, t={t:.3f}s, {len(snap['regions'])} regions)")
        built += 1

    print(f"[host-snapshots] done: {built} built, {skipped} skipped")
    return 0


if __name__ == "__main__":
    sys.exit(main())
