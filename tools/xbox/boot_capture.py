#!/usr/bin/env python3
"""Zero-input A/B harness: boot a chosen build straight into a level, capture state.

The input-driven harness (capture_scenario replay + ab_check) needs a recorded
controller fixture, and its verdict is only as good as that fixture's coverage.
For signals that advance on their own -- animation being the clearest -- no input
is needed at all: `core_load_at_startup` drops the engine into a saved core on
boot, and the trajectory can be captured immediately. That makes an A/B run fully
mechanical, and an A/A control cheap enough to always run first.

    # A/A control, then the candidate:
    boot_capture.py --xbe cachebeta.xbe -o aa1.halorec --ticks 600 --quantum 2
    boot_capture.py --xbe cachebeta.xbe -o aa2.halorec --ticks 600 --quantum 2
    boot_capture.py --xbe default.xbe   -o ab.halorec  --ticks 600 --quantum 2
    halorec_anim_diff.py aa1.halorec aa2.halorec --vs ab.halorec

`--init-line` lines are written to the title's init.txt before booting (the
`core_load_at_startup` this depends on). The PREVIOUS init.txt is saved to
`--init-backup` first and restored by `--restore-init`, so a run cannot silently
leave the box configured differently than it found it.
"""
import argparse
import os
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "equivalence"))
sys.path.insert(0, str(ROOT / "tools" / "xbox"))

import qmp_capture as qc         # noqa: E402
import hmrc                      # noqa: E402
import capture_scenario as cs    # noqa: E402
from capture_trajectory import capture_trajectory   # noqa: E402

RDCP = ROOT / "tools" / "xbox" / "xbdm_rdcp.py"
GETFILE = ROOT / "tools" / "xbox" / "xbdm_getfile.py"
DEFAULT_INIT_LINES = ("game_difficulty_set impossible", "map_name a10",
                      "core_load_at_startup")
# 0x1011c holds the XBE section count: 24 (0x18) unpatched cachebeta, 30 (0x1e)
# patched default. The cheapest independent proof of which build actually booted.
SECTION_COUNT_ADDR = 0x1011C
EXPECTED_SECTIONS = {"cachebeta.xbe": 24, "default.xbe": 30}


def _rel(p):
    """Repo-relative path string.

    The helper scripts run under a sandbox that rejects absolute host paths
    ("local file not found" even for a file that plainly exists), so every path
    handed to a child process is relative and the child is run with cwd=ROOT.
    """
    return os.path.relpath(str(Path(p).resolve()), str(ROOT))


def _run(cmd, **kw):
    kw.setdefault("cwd", str(ROOT))
    return subprocess.run([sys.executable, *cmd], capture_output=True, text=True, **kw)


def get_remote(xbox_path, local: Path):
    local.parent.mkdir(parents=True, exist_ok=True)
    p = _run([_rel(GETFILE), xbox_path, "-o", _rel(local)])
    return local if p.returncode == 0 and local.exists() else None


def write_synced(path: Path, data: bytes, timeout=5.0):
    """Write and make the bytes visible to a SEPARATE process.

    The repo lives on a 9p mount (/mnt/g); a plain write_bytes followed
    immediately by subprocess.run loses the race -- the child opens the path
    before the mount publishes it and reports "local file not found".
    """
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "wb") as fh:
        fh.write(data)
        fh.flush()
        os.fsync(fh.fileno())
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            if path.stat().st_size == len(data):
                return path
        except OSError:
            pass
        time.sleep(0.02)
    raise RuntimeError(f"{path} not visible after write")


def put_remote(local: Path, xbox_path, retries=3):
    last = ""
    for _ in range(retries):
        p = _run([_rel(RDCP), "--sendfile", _rel(local), xbox_path])
        if p.returncode == 0:
            return
        last = f"{p.stdout}{p.stderr}"
        time.sleep(0.3)
    raise RuntimeError(f"sendfile failed: {last}")


def running_sections():
    """Section count of the RUNNING xbe -- independent build-identity evidence."""
    p = _run([_rel(RDCP), f"getmem addr={SECTION_COUNT_ADDR:#x} length=2"])
    txt = (p.stdout or "").strip().splitlines()
    for ln in txt:
        ln = ln.strip()
        if len(ln) == 4 and all(c in "0123456789abcdefABCDEF" for c in ln):
            return int(ln[2:4] + ln[0:2], 16)      # little-endian byte pair
    return None


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-o", "--output", type=Path, help="output .halorec")
    ap.add_argument("--xbe", default="cachebeta.xbe",
                    help="cachebeta.xbe (unpatched/faithful) or default.xbe (patched)")
    ap.add_argument("--title-root", default=cs.DEFAULT_TITLE_ROOT)
    ap.add_argument("--host", default=None)
    ap.add_argument("--ticks", type=int, default=600)
    ap.add_argument("--quantum", type=int, default=2)
    ap.add_argument("--include-object-bodies", action="store_true", default=True)
    ap.add_argument("--no-object-bodies", dest="include_object_bodies",
                    action="store_false")
    ap.add_argument("--pools", default="objects,actors")
    ap.add_argument("--object-body-size", type=lambda x: int(x, 0),
                    default=qc.DEFAULT_OBJECT_BODY_SIZE)
    ap.add_argument("--init-line", action="append", default=None,
                    help="init.txt line (repeatable); default is the a10 core boot")
    ap.add_argument("--init-backup", type=Path,
                    default=ROOT / "artifacts" / "ai_regression" / "init_backup.txt")
    ap.add_argument("--no-init", action="store_true",
                    help="do not touch init.txt (it is already configured)")
    ap.add_argument("--restore-init", action="store_true",
                    help="restore init.txt from --init-backup and exit")
    ap.add_argument("--stall-timeout", type=float, default=8.0)
    a = ap.parse_args(argv)

    init_path = a.title_root.rstrip("\\") + r"\init.txt"

    if a.restore_init:
        if not a.init_backup.exists():
            sys.exit(f"error: no backup at {a.init_backup}")
        put_remote(a.init_backup, init_path)
        check = get_remote(init_path, a.init_backup.with_suffix(".verify"))
        ok = check is not None and check.read_bytes() == a.init_backup.read_bytes()
        print(f"  [init] restored {init_path}: "
              f"{'byte-for-byte OK' if ok else 'MISMATCH'}")
        return 0 if ok else 1

    if not a.output:
        sys.exit("error: -o/--output is required unless --restore-init")

    if not a.no_init:
        if not a.init_backup.exists():
            saved = get_remote(init_path, a.init_backup)
            print(f"  [init] backed up {init_path} -> {a.init_backup} "
                  f"({saved.stat().st_size if saved else '?'} B)")
        lines = a.init_line or list(DEFAULT_INIT_LINES)
        tmp = write_synced(a.init_backup.parent / "init_boot.txt",
                           ("\n".join(lines) + "\n").encode())
        put_remote(tmp, init_path)
        print(f"  [init] wrote {len(lines)} lines -> {init_path}")

    print(f"  [boot] magicboot {a.xbe} ...")
    if not cs.boot_title(a.xbe, a.title_root, a.host):
        sys.exit(f"error: {a.xbe} did not come up as the running title")
    sec = running_sections()
    want = EXPECTED_SECTIONS.get(a.xbe)
    print(f"  [boot] running={a.xbe} sections={sec} (expect {want})")
    if want is not None and sec is not None and sec != want:
        sys.exit(f"error: build identity mismatch -- {a.xbe} should have {want} "
                 f"sections, running image has {sec}")

    cap = qc.QMPCapture()
    try:
        frames, anchor, last_rel = capture_trajectory(
            cap, a.output.stem, a.ticks, a.quantum,
            stall_timeout=a.stall_timeout,
            object_bodies=a.include_object_bodies,
            body_size=a.object_body_size,
            pools=tuple(p.strip() for p in a.pools.split(",") if p.strip()))
    finally:
        cap.close()
    if not frames:
        sys.exit("error: captured 0 frames")
    n = hmrc.write_halorec(a.output, a.output.stem, frames)
    print(f"  [traj] wrote {n} frames (anchor={anchor}, rel 0..{last_rel}) -> "
          f"{a.output} ({a.output.stat().st_size} B)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
