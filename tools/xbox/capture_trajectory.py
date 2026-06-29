#!/usr/bin/env python3
"""Capture a game-state trajectory from live xemu to a `.halorec`, tick-quantized.

The state-observation half of A/B trajectory testing (docs/ab-trajectory-testing.md).
Run this WHILE a deterministic input fixture is replaying (capture_scenario.py
replay): it waits for gameplay to start, anchors on that engine event, then
captures the full pool set every K game ticks via atomic QMP stop/memsave/cont.

Quantizing on the RELATIVE game tick (tick - anchor) — not wall-clock — means two
runs of the same deterministic fixture capture the SAME relative ticks, so their
frames align exactly regardless of host timing. Output loads in halo-memory-viewer
and feeds trajectory_diff.py / halorec_to_snapshot.py.

    python3 tools/xbox/capture_trajectory.py -o run.halorec --ticks 300 --quantum 4

Typical use (A/A or A/B): start the replay, then run this; see trajectory_diff.py
and the aa_check orchestrator.
"""
import argparse
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "tools" / "equivalence"))

import qmp_capture as qc       # noqa: E402
import hmrc                    # noqa: E402


def wait_for_gameplay(cap, require_spawn=True, timeout=90.0, poll=0.25):
    """Poll until the objtable magic resolves (and a player is spawned).

    Returns the anchor tick at the first ready poll, or None on timeout.
    """
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            if qc.magic_ok(cap) and (not require_spawn or qc.player_spawned(cap)):
                return cap.tick()
        except qc.QMPError:
            pass
        time.sleep(poll)
    return None


def capture_trajectory(cap, name, ticks, quantum, require_spawn=True,
                       stall_timeout=4.0, max_frames=4000, log=print):
    """Capture one frame per relative-tick bucket (rel // quantum) up to `ticks`.

    Buckets are a FIXED grid {0, K, 2K, ...} relative to the gameplay-ready
    anchor — independent of when each capture actually lands — so two runs of the
    same fixture target the same ticks and their frames align. We capture the
    first observed tick in each not-yet-seen bucket and record its ACTUAL tick.

    Returns (frames, anchor, last_rel). frames = [(t_elapsed, [(addr,bytes),...])].
    Stops at the tick span, a playback stall, or max_frames.
    """
    anchor = wait_for_gameplay(cap, require_spawn=require_spawn)
    if anchor is None:
        raise qc.QMPError("gameplay never became ready (magic/player-spawn timeout)")
    log(f"  [traj] gameplay ready: anchor tick={anchor}")

    t0 = time.monotonic()
    frames = []
    seen_buckets = set()
    last_tick = anchor
    last_advance = time.monotonic()
    last_rel = 0

    while len(frames) < max_frames:
        try:
            tick = cap.tick()
        except qc.QMPError:
            tick = None
        now = time.monotonic()
        if tick is not None and tick != last_tick:
            last_tick = tick
            last_advance = now

        if tick is not None:
            rel = tick - anchor
            bucket = rel // quantum
            if rel >= 0 and bucket not in seen_buckets:
                seen_buckets.add(bucket)
                regions, ftick = qc.capture_full_frame(cap)
                frel = (ftick - anchor) if ftick is not None else rel
                frames.append((now - t0, list(regions.items())))
                last_rel = frel
                if len(frames) % 10 == 0:
                    log(f"  [traj] {len(frames)} frames, rel tick={frel}")
                if frel >= ticks:
                    log(f"  [traj] reached tick span ({frel} >= {ticks})")
                    break
                continue

        if now - last_advance > stall_timeout:
            log(f"  [traj] playback stalled (no tick advance {stall_timeout}s) "
                f"at rel={last_rel}; stopping")
            break
        # poll as fast as the QMP round-trip allows (no sleep) to catch each tick

    return frames, anchor, last_rel


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-o", "--output", required=True, type=Path,
                    help="output .halorec path")
    ap.add_argument("--name", default=None, help="recording name (default: output stem)")
    ap.add_argument("--ticks", type=int, default=300,
                    help="relative game ticks to span (default 300 ~= 10s)")
    ap.add_argument("--quantum", type=int, default=4,
                    help="capture every K relative ticks (default 4)")
    ap.add_argument("--no-wait-spawn", action="store_true",
                    help="anchor on objtable magic only, do not require a spawned player")
    ap.add_argument("--stall-timeout", type=float, default=4.0,
                    help="stop if the tick does not advance for this long (s)")
    ap.add_argument("--host", default=qc.QMP_HOST)
    ap.add_argument("--port", type=int, default=qc.QMP_PORT)
    a = ap.parse_args(argv)

    name = a.name or a.output.stem
    cap = qc.QMPCapture(host=a.host, port=a.port)
    try:
        frames, anchor, last_rel = capture_trajectory(
            cap, name, a.ticks, a.quantum,
            require_spawn=not a.no_wait_spawn, stall_timeout=a.stall_timeout)
    finally:
        cap.close()

    if not frames:
        sys.exit("error: captured 0 frames")
    n = hmrc.write_halorec(a.output, name, frames)
    span = last_rel
    print(f"  [traj] wrote {n} frames (anchor={anchor}, rel span 0..{span}) "
          f"-> {a.output} ({a.output.stat().st_size} B)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
