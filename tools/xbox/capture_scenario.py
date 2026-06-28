#!/usr/bin/env python3
"""capture_scenario.py — record/replay per-level input fixtures for Halo CE Xbox.

Automates the proven arm -> record -> close -> download -> trim -> store ->
co-locate flow into named, self-contained per-level scenario fixtures under
``input-recordings/levels/<level>/<scenario>/``.

Each fixture is self-contained and HOST-ONLY (the whole input-recordings/ tree is
gitignored; ``core.bin`` is a copyrighted game-state heap snapshot — never commit):

    state.data      trimmed controller input (4 pads * 0x28 bytes per tick)
    core.bin        the checkpoint the scenario starts from (start=core only)
    init.txt        boot recipe (map_name + core_load_at_startup, or empty for menu)
    read.xts        empty playback sentinel
    recording.json  metadata (written by input_recordings.py)
    REPLAY.md       one-shot deploy recipe

Start modes (only the two empirically proven on this dev box are offered):
    core   (default)  verify d:\\core\\core.bin exists, boot map then overlay it
    menu              empty init.txt; record menu navigation + gameplay (script-faithful AI)

Subcommands:
    record    interactive: arm -> "play, press Enter" -> finalize -> validate
    arm       set up the box and reboot into record mode (load-bearing half)
    finalize  close + download + trim + store + co-locate (load-bearing half)
    replay    redeploy an existing fixture and boot it (optionally confirm known_good)
    selftest  validate analyze()/trim() against raws already on disk (no hardware)

The arm/finalize split is the crash-recovery seam: box state (sentinels + core)
plus the args fully determine finalize, so a hiccup during a multi-minute
playthrough never loses the run.

Two hard-won invariants are baked in:
  * ALWAYS pass repo-relative paths to xbdm_rdcp.py (absolute /tmp got remapped to
    G:\\tmp by the windows-python path layer); cwd is always the repo root.
  * ALWAYS delete write.xts before any reboot in finalize — a boot with write.xts
    present reopens state.data with CREATE_ALWAYS and TRUNCATES the recording.
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
RDCP = "tools/xbox/xbdm_rdcp.py"                 # repo-relative (invoked via python3)
RECORDINGS = "tools/xbox/input_recordings.py"
STAGE = "artifacts/capture"                       # gitignored scratch
DEFAULT_TITLE_ROOT = r"E:\GAMES\halo-patched"
DEFAULT_XBE = "cachebeta.xbe"                      # unpatched -> faithful capture

TICK = 0xA0      # bytes per tick (4 gamepads * 0x28)
PKT = 0x28       # bytes per gamepad packet
PADS = 4

# --------------------------------------------------------------------------
# Pure functions (no I/O) — unit-tested by `selftest`.
# --------------------------------------------------------------------------

def strip_rdcp_prefix(raw: bytes) -> bytes:
    """getfile prepends a 4-byte little-endian payload size; drop it."""
    return raw[4:]


def analyze(data: bytes) -> dict:
    """Per-tick / per-slot non-zero analysis of a clean (prefix-stripped) stream."""
    n = len(data) // TICK
    slot_nz = [0, 0, 0, 0]
    input_ticks = []
    for t in range(n):
        base = t * TICK
        tnz = 0
        for s in range(PADS):
            p = data[base + s * PKT: base + s * PKT + PKT]
            nz = sum(1 for b in p if b)
            slot_nz[s] += nz
            tnz += nz
        if tnz:
            input_ticks.append(t)
    return {
        "ticks": n,
        "slot_nz": slot_nz,
        "total_nz": sum(slot_nz),
        "input_ticks": input_ticks,
        "first_input_tick": input_ticks[0] if input_ticks else None,
        "last_input_tick": input_ticks[-1] if input_ticks else None,
    }


def trim(data: bytes, tail_pad: int = 36) -> tuple[bytes, dict]:
    """Trim the idle tail, keeping the head intact (replay reloads the level at pace).

    Returns (trimmed_bytes, info). If no input is found, returns the data unchanged
    and info['trimmed'] is False — never trims to empty.
    """
    info = analyze(data)
    last = info["last_input_tick"]
    if last is None:
        info["trimmed"] = False
        info["kept_ticks"] = info["ticks"]
        return data, info
    keep_ticks = min(info["ticks"], last + 1 + max(0, tail_pad))
    info["trimmed"] = keep_ticks < info["ticks"]
    info["kept_ticks"] = keep_ticks
    return data[: keep_ticks * TICK], info


# --------------------------------------------------------------------------
# XBDM transport (subprocess over xbdm_rdcp.py — replicate the proven invocation).
# --------------------------------------------------------------------------

def _rdcp(args: list[str], host: str | None) -> subprocess.CompletedProcess:
    cmd = ["python3", RDCP, *args]
    if host:
        cmd += ["--host", host]
    return subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)


def rdcp_cmd(command: str, host: str | None) -> subprocess.CompletedProcess:
    return _rdcp([command], host)


def sendfile(local_rel: str, xbox_path: str, host: str | None) -> None:
    cp = _rdcp(["--sendfile", local_rel, xbox_path], host)
    if cp.returncode != 0:
        raise RuntimeError(f"sendfile {local_rel} -> {xbox_path} failed:\n{cp.stderr or cp.stdout}")


def xbox_join(title_root: str, *parts: str) -> str:
    return title_root.rstrip("\\") + "".join("\\" + p for p in parts)


_DIR_RE = re.compile(r'name="([^"]+)"\s+sizehi=0x([0-9a-fA-F]+)\s+sizelo=0x([0-9a-fA-F]+)')


def dirlist_sizes(title_root: str, host: str | None) -> dict[str, int] | None:
    cp = rdcp_cmd(f'dirlist name="{title_root}"', host)
    if cp.returncode != 0:
        return None
    out: dict[str, int] = {}
    for m in _DIR_RE.finditer(cp.stdout):
        out[m.group(1).lower()] = (int(m.group(2), 16) << 32) | int(m.group(3), 16)
    return out


def remote_size(title_root: str, name: str, host: str | None) -> int | None:
    """Size of a file at title_root\\name (name may contain a subpath like core\\core.bin)."""
    parent = title_root
    leaf = name
    if "\\" in name:
        parent = xbox_join(title_root, *name.split("\\")[:-1])
        leaf = name.split("\\")[-1]
    cp = rdcp_cmd(f'dirlist name="{parent}"', host)
    if cp.returncode != 0:
        return None
    for m in _DIR_RE.finditer(cp.stdout):
        if m.group(1).lower() == leaf.lower():
            return (int(m.group(2), 16) << 32) | int(m.group(3), 16)
    return None


def getfile(xbox_path: str, size: int, out_rel: str, host: str | None) -> bytes:
    """Download `size` bytes, strip the 4-byte RDCP prefix, return clean bytes.

    The on-disk file at out_rel is rewritten to the CLEAN (stripped) bytes — callers
    that re-read it (e.g. core co-location) must never see the prefix, which would
    shift every byte by 4 and corrupt the payload (a heap core, a packet stream, …).
    """
    (ROOT / out_rel).parent.mkdir(parents=True, exist_ok=True)
    cp = _rdcp(
        [f'getfile name="{xbox_path}" offset=0 size={size}',
         "--binary-length", str(size + 4), "--output", out_rel],
        host,
    )
    if cp.returncode != 0:
        raise RuntimeError(f"getfile {xbox_path} failed:\n{cp.stderr or cp.stdout}")
    clean = strip_rdcp_prefix((ROOT / out_rel).read_bytes())
    (ROOT / out_rel).write_bytes(clean)   # overwrite raw -> clean on disk
    return clean


def magicboot(xbe: str, title_root: str, host: str | None) -> None:
    # Best-effort: xemu returns "200- OK"; a real Xbox resets the TCP link (treated as success).
    rdcp_cmd(f"magicboot title={xbox_join(title_root, xbe)} debug", host)


def delete_remote(xbox_path: str, host: str | None) -> None:
    rdcp_cmd(f'delete name="{xbox_path}"', host)


def handle_released(title_root: str, host: str | None) -> bool:
    """True iff state.data is not held open by the running title.

    state.data is opened share-mode 0 (exclusive) during record/playback; a 4-byte
    getfile only succeeds once the handle is closed. Three states are distinguished
    so callers can poll safely across a reboot:
      * XBDM unreachable (mid-reboot) -> False (not ready; keep waiting)
      * state.data absent             -> True  (nothing can hold it)
      * present                       -> probe; True iff it opens

    The local output dir MUST exist first, or the host-side `getfile --output`
    fails with "No such file or directory" and we would falsely report "locked"
    forever — the bug that made the release loop spin and look wedged.
    """
    (ROOT / STAGE).mkdir(parents=True, exist_ok=True)
    sizes = dirlist_sizes(title_root, host)
    if sizes is None:
        return False
    if "state.data" not in sizes:
        return True
    cp = _rdcp(
        [f'getfile name="{xbox_join(title_root, "state.data")}" offset=0 size=4',
         "--binary-length", "8", "--output", f"{STAGE}/.probe.bin"],
        host,
    )
    return cp.returncode == 0


def _poll_until(pred, timeout: float, interval: float = 3.0) -> bool:
    deadline = time.time() + timeout
    while time.time() < deadline:
        if pred():
            return True
        time.sleep(interval)
    return pred()


def qmp_reset(port: int = 4444) -> bool:
    """Hard-reset the emulated machine via xemu QMP (best-effort; xemu only).

    This is the hammer for a genuinely halted box (a bad core header can leave the
    CPU in a halt loop where magicboot/HalReturnToFirmware never executes). No-op
    that returns False on a real Xbox or if QMP is unreachable.
    """
    import socket
    try:
        s = socket.create_connection(("127.0.0.1", port), timeout=5)
        s.settimeout(5)
        s.recv(4096)                                      # QMP greeting
        s.sendall(b'{"execute":"qmp_capabilities"}\n'); s.recv(4096)
        s.sendall(b'{"execute":"system_reset"}\n'); s.recv(4096)
        s.close()
        return True
    except OSError:
        return False


def ensure_state_data_free(args, soft_timeout: float = 60, hard_timeout: float = 150) -> None:
    """Guarantee state.data is overwritable, escalating until it is.

    A sendfile over an open state.data returns "413 file cannot be created", so a
    prior playback/record must be torn down first. Escalation:
      1. already free            -> return
      2. soft: disarm + magicboot, poll for release
      3. hard: QMP system_reset (or XBDM cold reboot), re-magicboot, poll
      4. give up with actionable guidance
    """
    tr, host = args.title_root, args.host
    if handle_released(tr, host):
        return
    print("  state.data is held open — soft release (disarm + magicboot)...")
    for s in ("read.xts", "write.xts", "loop.xts"):
        delete_remote(xbox_join(tr, s), host)
    magicboot(args.xbe, tr, host)
    if _poll_until(lambda: handle_released(tr, host), soft_timeout):
        print("  released (soft).")
        return
    print("  soft release failed — hard reset...")
    if not qmp_reset():
        rdcp_cmd("reboot", host)                          # real-Xbox fallback
    # After a hard reset the box may auto-boot a different title; once XBDM + the
    # file are reachable and free, put the requested xbe back up.
    if _poll_until(lambda: handle_released(tr, host), hard_timeout):
        magicboot(args.xbe, tr, host)
        _poll_until(lambda: handle_released(tr, host), soft_timeout)
        print("  released (hard reset).")
        return
    sys.exit("error: could not free state.data even after a hard reset. The box may "
             "be at a halt/assert screen — inspect it manually before retrying.")


# --------------------------------------------------------------------------
# Staging helpers
# --------------------------------------------------------------------------

def _stage_dir() -> Path:
    d = ROOT / STAGE
    d.mkdir(parents=True, exist_ok=True)
    return d


def _empty_xts() -> str:
    p = _stage_dir() / "empty.xts"
    p.write_bytes(b"")
    return f"{STAGE}/empty.xts"


def _init_txt_content(start: str, mapname: str) -> str:
    if start == "core":
        return f"map_name {mapname}\ncore_load_at_startup\n"
    if start == "menu":
        return ""  # empty -> boot to main menu
    raise ValueError(f"unknown start mode {start!r}")


def fixture_dir(level: str, scenario: str) -> Path:
    return ROOT / "input-recordings" / "levels" / level / scenario


# --------------------------------------------------------------------------
# Phases
# --------------------------------------------------------------------------

def preflight(args) -> None:
    sizes = dirlist_sizes(args.title_root, args.host)
    if sizes is None:
        sys.exit(f"error: XBDM unreachable at title root {args.title_root} "
                 f"(host={args.host or 'default'}). Is xemu/Xbox up?")
    if args.start == "core":
        csize = remote_size(args.title_root, "core\\core.bin", args.host)
        if not csize:
            sys.exit("error: start=core but d:\\core\\core.bin not found. "
                     "Play to the spot and run `core_save` in the console first "
                     "(or use --start menu).")
        print(f"  preflight: core.bin present ({csize} bytes)")
    print(f"  preflight: XBDM ok, title root has {len(sizes)} entries")


def cmd_arm(args) -> None:
    preflight(args)
    stage = _stage_dir()
    init_rel = f"{STAGE}/{args.level}__{args.scenario}.init.txt"
    (stage / Path(init_rel).name).write_text(_init_txt_content(args.start, args.map), encoding="ascii")

    print("  arming recorder...")
    sendfile(init_rel, xbox_join(args.title_root, "init.txt"), args.host)
    sendfile(_empty_xts(), xbox_join(args.title_root, "write.xts"), args.host)
    delete_remote(xbox_join(args.title_root, "read.xts"), args.host)
    delete_remote(xbox_join(args.title_root, "loop.xts"), args.host)

    sizes = dirlist_sizes(args.title_root, args.host) or {}
    if "write.xts" not in sizes:
        sys.exit("error: write.xts did not stick — aborting before reboot.")
    if "read.xts" in sizes or "loop.xts" in sizes:
        sys.exit("error: a playback sentinel survived — would shadow record mode.")
    print(f"  armed (start={args.start}); rebooting into {args.xbe}...")
    magicboot(args.xbe, args.title_root, args.host)
    print("  >> PLAY your scenario now. When done, run `finalize` "
          "(or press Enter in record mode).")


def _wait_handle_closed(args, timeout=150) -> int:
    """Poll until state.data size is stable AND the exclusive handle is released."""
    prev = None
    deadline = time.time() + timeout
    while time.time() < deadline:
        size = remote_size(args.title_root, "state.data", args.host)
        if size is not None and size == prev and handle_released(args.title_root, args.host):
            return size
        prev = size
        time.sleep(3)
    raise TimeoutError("state.data never stabilized / handle never released after reboot")


def cmd_finalize(args) -> None:
    fdir = fixture_dir(args.level, args.scenario)
    existing = fdir / "recording.json"
    if existing.exists() and not args.force:
        meta = json.loads(existing.read_text())
        if meta.get("known_good"):
            sys.exit(f"refusing to overwrite known_good fixture {fdir} without --force")

    # 1. Disarm BEFORE any reboot (truncation guard), then reboot to close the handle.
    print("  disarming write.xts (truncation guard)...")
    delete_remote(xbox_join(args.title_root, "write.xts"), args.host)
    sizes = dirlist_sizes(args.title_root, args.host) or {}
    for s in ("write.xts", "read.xts", "loop.xts"):
        if s in sizes:
            sys.exit(f"error: {s} still present — refusing to reboot (would truncate/replay).")
    print("  rebooting to close the recording handle...")
    magicboot(args.xbe, args.title_root, args.host)
    size = _wait_handle_closed(args)
    print(f"  handle closed; state.data = {size} bytes ({size // TICK} ticks)")

    # 2. Download + analyze + trim.
    raw_rel = f"{STAGE}/{args.level}__{args.scenario}.raw"
    data = getfile(xbox_join(args.title_root, "state.data"), size, raw_rel, args.host)
    trimmed, info = trim(data, args.tail_pad)
    if info["last_input_tick"] is None:
        print("  WARNING: no controller input detected in slot 0 — storing untrimmed, "
              "known_good will stay false. Re-check controller binding / write.xts timing.")
    else:
        print(f"  input slot0={info['slot_nz'][0]}B, ticks {info['first_input_tick']}.."
              f"{info['last_input_tick']}; trimmed {info['ticks']}->{info['kept_ticks']} ticks")
    clean_rel = f"{STAGE}/{args.level}__{args.scenario}.state.data"
    (ROOT / clean_rel).write_bytes(trimmed)

    # 3. Download the core (start=core) before we store, so the fixture is complete.
    core_size = None
    if args.start == "core":
        core_size = remote_size(args.title_root, "core\\core.bin", args.host)
        if not core_size:
            sys.exit("error: core.bin vanished from the box before finalize.")
        getfile(xbox_join(args.title_root, "core", "core.bin"), core_size,
                f"{STAGE}/{args.level}__{args.scenario}.core.bin", args.host)

    # 4. Promote into the corpus via input_recordings.py (manifest + sha + ticks).
    start_cond = (f"magicboot {args.xbe}; init.txt={_init_txt_content(args.start, args.map)!r}; "
                  + ("core.bin at d:\\core\\core.bin; " if args.start == "core" else "")
                  + "read.xts to play")
    notes = (f"{'CORE-ANCHORED' if args.start == 'core' else 'MENU-START'}, host-only. "
             f"Self-contained fixture; see REPLAY.md. Captured on {args.xbe}.")
    add_cmd = [
        "python3", RECORDINGS, "add",
        "--level", args.level, "--id", args.scenario,
        "--source", clean_rel, "--allow-overwrite",
        "--title", args.title or f"{args.level} {args.scenario}",
        "--purpose", args.purpose or f"Deterministic replay fixture for {args.level} ({args.scenario})",
        "--build", args.build, "--difficulty", args.difficulty,
        "--start-condition", start_cond, "--notes", notes,
    ]
    cp = subprocess.run(add_cmd, cwd=ROOT, capture_output=True, text=True)
    if cp.returncode != 0:
        sys.exit(f"input_recordings add failed:\n{cp.stderr or cp.stdout}")

    # 5. Co-locate the rest of the self-contained fixture.
    fdir.mkdir(parents=True, exist_ok=True)
    if args.start == "core":
        (fdir / "core.bin").write_bytes((ROOT / f"{STAGE}/{args.level}__{args.scenario}.core.bin").read_bytes())
    (fdir / "init.txt").write_text(_init_txt_content(args.start, args.map), encoding="ascii")
    (fdir / "read.xts").write_bytes(b"")
    _write_replay_md(fdir, args, info, core_size)
    print(f"  stored fixture -> {fdir.relative_to(ROOT)} (known_good=false, pending replay confirm)")

    if not args.no_validate:
        print("  validating by replay...")
        cmd_replay(args, validate=True)


def cmd_replay(args, validate: bool = False) -> None:
    fdir = fixture_dir(args.level, args.scenario)
    if not (fdir / "state.data").exists():
        sys.exit(f"error: no fixture at {fdir}")
    tr = args.title_root
    ensure_state_data_free(args)   # bulletproof: soft -> hard-reset escalation until overwritable
    core = fdir / "core.bin"
    if core.exists():
        if core.stat().st_size % 0x1000 != 0:
            sys.exit(f"error: {core} size {core.stat().st_size} is not page-aligned — "
                     "core is RDCP-prefix-corrupted (strip 4 leading bytes or re-capture).")
        sendfile(str(core.relative_to(ROOT)), xbox_join(tr, "core", "core.bin"), args.host)
    sendfile(str((fdir / "state.data").relative_to(ROOT)), xbox_join(tr, "state.data"), args.host)
    sendfile(str((fdir / "init.txt").relative_to(ROOT)), xbox_join(tr, "init.txt"), args.host)
    sendfile(str((fdir / "read.xts").relative_to(ROOT)), xbox_join(tr, "read.xts"), args.host)
    delete_remote(xbox_join(tr, "write.xts"), args.host)
    print(f"  deployed {args.level}/{args.scenario}; booting {args.xbe} in playback...")
    magicboot(args.xbe, tr, args.host)
    if validate:
        ans = input("  did it reproduce the run on screen? [y/N] ").strip().lower()
        if ans == "y":
            _set_known_good(args.level, args.scenario, True)
            print("  known_good = true")
        else:
            print("  left known_good = false")


def cmd_record(args) -> None:
    cmd_arm(args)
    input("\n  >> Press Enter when you have finished playing the scenario... ")
    cmd_finalize(args)


def cmd_selftest(args) -> None:
    """Validate analyze()/trim() against raws already on disk (no hardware)."""
    raw = ROOT / "artifacts/input/a10_core_tight.raw"
    if not raw.exists():
        sys.exit(f"selftest needs {raw} (a prior getfile .raw) — not found")
    data = strip_rdcp_prefix(raw.read_bytes())
    info = analyze(data)
    assert info["slot_nz"][0] == 464, info["slot_nz"]
    assert info["slot_nz"][1:] == [0, 0, 0], info["slot_nz"]
    assert info["last_input_tick"] == 364, info["last_input_tick"]
    assert info["first_input_tick"] == 219, info["first_input_tick"]
    trimmed, tinfo = trim(data, tail_pad=36)
    assert tinfo["kept_ticks"] == 401, tinfo["kept_ticks"]      # 364 + 1 + 36
    assert len(trimmed) == 401 * TICK, len(trimmed)
    assert tinfo["trimmed"] is True
    print(f"selftest OK: slot0=464B, input ticks 219..364, trim {info['ticks']}->{tinfo['kept_ticks']}")


# --------------------------------------------------------------------------
# REPLAY.md + known_good
# --------------------------------------------------------------------------

def _write_replay_md(fdir: Path, args, info: dict, core_size: int | None) -> None:
    import hashlib
    sd = (fdir / "state.data").read_bytes()
    sha = hashlib.sha256(sd).hexdigest()
    head_s = (info["first_input_tick"] or 0) / 30.0
    core_line = ("| `core.bin` | checkpoint heap snapshot |\n" if args.start == "core" else "")
    core_push = ('python3 $X --sendfile "$D/core.bin"   \'%s\'\n'
                 % xbox_join(args.title_root, "core", "core.bin")) if args.start == "core" else ""
    md = f"""# {args.level} — {args.scenario}

Deterministic replay fixture. **Host-only** — the `input-recordings/` tree is
gitignored and `core.bin` is copyrighted game memory; never commit these bytes.

| File | |
|------|--|
| `state.data` | trimmed input — {len(sd)} B / {info['kept_ticks']} ticks (slot 0, ticks {info['first_input_tick']}..{info['last_input_tick']}) |
{core_line}| `init.txt` | boot recipe ({'map_name + core_load_at_startup' if args.start == 'core' else 'empty -> menu'}) |
| `read.xts` | playback sentinel |

state.data sha256: `{sha}`

## Determinism note

Input playback is **open-loop**: a fixed, tick-locked stream replayed regardless
of game state. That is exactly why it is a test oracle — a behavior divergence
mid-replay is the *signal*. The one confound: the meaningful presses land at fixed
*tick numbers*, after a ~{head_s:.1f}s "time-to-control" head. If a build change
shifts how many ticks it takes to reach player control, those presses land at a
different game moment (desync that looks like a bug but isn't). For faithful lifts
the head is identical; still, the **first** time you reuse this against a patched
`default.xbe`, confirm the action lands in the same place.

## Replay (from repo root)

```bash
D="input-recordings/levels/{args.level}/{args.scenario}"
X='tools/xbox/xbdm_rdcp.py'
{core_push}python3 $X --sendfile "$D/state.data" '{xbox_join(args.title_root, "state.data")}'
python3 $X --sendfile "$D/init.txt"   '{xbox_join(args.title_root, "init.txt")}'
python3 $X --sendfile "$D/read.xts"   '{xbox_join(args.title_root, "read.xts")}'
python3 $X 'delete name="{xbox_join(args.title_root, "write.xts")}"'
python3 $X 'magicboot title={xbox_join(args.title_root, args.xbe)} debug'
```

Or: `python3 tools/xbox/capture_scenario.py replay --level {args.level} --scenario {args.scenario}`

To test a **patched** build, pass `--xbe default.xbe` (same heap layout; the core loads identically).
"""
    (fdir / "REPLAY.md").write_text(md, encoding="utf-8")


def _set_known_good(level: str, scenario: str, value: bool) -> None:
    fdir = fixture_dir(level, scenario)
    rj = fdir / "recording.json"
    meta = json.loads(rj.read_text())
    meta["known_good"] = value
    rj.write_text(json.dumps(meta, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    man = ROOT / "input-recordings" / "manifest.json"
    if man.exists():
        mj = json.loads(man.read_text())
        for rec in mj.get("recordings", []):
            if rec.get("id") == scenario and rec.get("level") == level:
                rec["known_good"] = value
        man.write_text(json.dumps(mj, indent=2, sort_keys=True) + "\n", encoding="utf-8")


# --------------------------------------------------------------------------
# CLI
# --------------------------------------------------------------------------

def _add_common(p) -> None:
    p.add_argument("--level", required=True, help="Level key, e.g. a10")
    p.add_argument("--scenario", required=True, help="Scenario id / folder name, e.g. cryo-bay-exit")
    p.add_argument("--start", choices=["core", "menu"], default="core",
                   help="core (default, needs a saved core) or menu (script-faithful)")
    p.add_argument("--map", default="", help="map_name arg (default = level)")
    p.add_argument("--xbe", default=DEFAULT_XBE, help="title XBE to boot (default cachebeta.xbe = unpatched)")
    p.add_argument("--title-root", default=DEFAULT_TITLE_ROOT)
    p.add_argument("--host", default="", help="XBDM host (default: xbdm_rdcp.py default)")
    p.add_argument("--tail-pad", type=int, default=36, help="ticks kept past last input")
    p.add_argument("--title", default="")
    p.add_argument("--purpose", default="")
    p.add_argument("--build", default="cachebeta-unpatched")
    p.add_argument("--difficulty", default="")
    p.add_argument("--force", action="store_true", help="overwrite a known_good fixture")
    p.add_argument("--no-validate", action="store_true", help="skip the post-store replay validation")


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="Per-level input fixture capture/replay")
    sub = ap.add_subparsers(dest="command", required=True)
    for name in ("record", "arm", "finalize", "replay"):
        _add_common(sub.add_parser(name))
    sub.add_parser("selftest")

    args = ap.parse_args(argv)
    if getattr(args, "host", ""):
        args.host = args.host
    else:
        args.host = None
    if getattr(args, "map", "") == "" and hasattr(args, "level"):
        args.map = args.level

    dispatch = {
        "record": cmd_record, "arm": cmd_arm, "finalize": cmd_finalize,
        "replay": cmd_replay, "selftest": cmd_selftest,
    }
    dispatch[args.command](args)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
