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
import json
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "tools" / "equivalence"))

import qmp_capture as qc       # noqa: E402
import hmrc                    # noqa: E402
import capture_profile as cp   # noqa: E402


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
                       stall_timeout=4.0, max_frames=4000, log=print,
                       object_bodies=False,
                       body_size=qc.DEFAULT_OBJECT_BODY_SIZE,
                       pools=None, profile="full-fidelity", tick_start=0,
                       focused=None):
    """Capture one frame per relative-tick bucket (rel // quantum) up to `ticks`.

    Buckets are a FIXED grid {0, K, 2K, ...} relative to the gameplay-ready
    anchor — independent of when each capture actually lands — so two runs of the
    same fixture target the same ticks and their frames align. We capture the
    first observed tick in each not-yet-seen bucket and record its ACTUAL tick.

    Returns (frames, anchor, last_rel). frames = [(t_elapsed, [(addr,bytes),...])].
    Stops at the tick span, a playback stall, or max_frames.
    """
    profile = cp.normalize_profile(profile)
    pools = cp.pools_for_profile(profile, pools)
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
            if rel >= tick_start and bucket not in seen_buckets:
                seen_buckets.add(bucket)
                if focused is None:
                    regions, ftick = qc.capture_full_frame(
                        cap, pools=pools, object_bodies=object_bodies,
                        body_size=body_size)
                else:
                    regions, ftick = qc.capture_focused_frame(
                        cap, pools=pools, **focused)
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
    ap.add_argument("--tick-start", type=int, default=0,
                    help="first relative tick to retain (default 0)")
    ap.add_argument("--quantum", type=int, default=4,
                    help="capture every K relative ticks (default 4)")
    ap.add_argument("--no-wait-spawn", action="store_true",
                    help="anchor on objtable magic only, do not require a spawned player")
    ap.add_argument("--include-object-bodies", action="store_true",
                    help="also follow each live object entry's +0x08 pointer and "
                         "capture the object BODY (animation state, position, ...); "
                         "without this a frame carries only 12-byte datum entries")
    ap.add_argument("--object-body-size", type=lambda x: int(x, 0),
                    default=qc.DEFAULT_OBJECT_BODY_SIZE,
                    help="bytes to capture per object body (default 0x100)")
    ap.add_argument("--profile", choices=cp.PROFILES, default="full-fidelity",
                    help="capture profile (default: full-fidelity)")
    ap.add_argument("--pools", default=None,
                    help="comma-separated pools to capture; dropping unused pools "
                         "buys tick density (each memsave costs ~13ms round-trip)")
    ap.add_argument("--focused-actor-slots", default=None,
                    help="comma-separated actor slots for focused recipe capture")
    ap.add_argument("--include-focused-perception", action="store_true")
    ap.add_argument("--include-focused-linked-bodies", action="store_true")
    ap.add_argument("--include-focused-weapon-bodies", action="store_true")
    ap.add_argument("--include-focused-object-relations", action="store_true")
    ap.add_argument("--max-relation-nodes", type=int, default=16)
    ap.add_argument("--metadata-out", type=Path, default=None,
                    help="write capture anchor/window metadata JSON")
    ap.add_argument("--stall-timeout", type=float, default=4.0,
                    help="stop if the tick does not advance for this long (s)")
    ap.add_argument("--host", default=qc.QMP_HOST)
    ap.add_argument("--port", type=int, default=qc.QMP_PORT)
    a = ap.parse_args(argv)
    if a.quantum <= 0:
        ap.error("--quantum must be positive")
    if a.tick_start < 0 or a.tick_start > a.ticks:
        ap.error("--tick-start must be between 0 and --ticks")

    focused = None
    if a.focused_actor_slots is not None:
        try:
            actor_slots = tuple(dict.fromkeys(
                int(value.strip(), 0)
                for value in a.focused_actor_slots.split(",") if value.strip()))
        except ValueError:
            ap.error("--focused-actor-slots must contain integers")
        if not actor_slots:
            ap.error("--focused-actor-slots must not be empty")
        if any(slot < 0 or slot > 0xFFFF for slot in actor_slots):
            ap.error("focused actor slots must be in range 0..65535")
        focused = {
            "actor_slots": actor_slots,
            "include_perception": a.include_focused_perception,
            "include_linked_object_body": a.include_focused_linked_bodies,
            "include_weapon_bodies": a.include_focused_weapon_bodies,
            "include_object_relations": a.include_focused_object_relations,
            "max_relation_nodes": a.max_relation_nodes,
        }

    name = a.name or a.output.stem
    cap = qc.QMPCapture(host=a.host, port=a.port)
    try:
        frames, anchor, last_rel = capture_trajectory(
            cap, name, a.ticks, a.quantum,
            require_spawn=not a.no_wait_spawn, stall_timeout=a.stall_timeout,
            object_bodies=a.include_object_bodies, body_size=a.object_body_size,
            profile=a.profile,
            tick_start=a.tick_start, focused=focused,
            pools=None if a.pools is None else tuple(
                p.strip() for p in a.pools.split(",") if p.strip()))
    finally:
        cap.close()

    if not frames:
        sys.exit("error: captured 0 frames")
    n = hmrc.write_halorec(a.output, name, frames)
    span = last_rel
    print(f"  [traj] wrote {n} frames (anchor={anchor}, rel span 0..{span}) "
          f"-> {a.output} ({a.output.stat().st_size} B)")
    if a.metadata_out:
        a.metadata_out.parent.mkdir(parents=True, exist_ok=True)
        a.metadata_out.write_text(json.dumps({
            "anchor_tick": anchor,
            "tick_start": a.tick_start,
            "tick_end": a.ticks,
            "last_relative_tick": last_rel,
            "frames": n,
            "profile": a.profile,
            "focused_actor_slots": list(focused["actor_slots"]) if focused else [],
        }, indent=2) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
