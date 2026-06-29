# xbox_pad.py — Virtual Controller for xemu

`tools/xbox/xbox_pad.py` creates a virtual Xbox 360 controller that xemu can use,
and exposes it over TCP so an LLM (or any script) can send gamepad input
programmatically. This is useful for automated debugging — navigating menus,
selecting options, and triggering in-game actions without human interaction.

## Architecture

```
 LLM (opencode)                 TCP :27000              Windows
 ───────────────  ──→  xbox_pad.py client  ──→  xbox_pad.py server
 (via Bash tool)       (thin CLI, no deps)      (holds vgamepad)
                                                         │
                                                         ▼
                                                    ViGEmBus
                                                         │
                                                         ▼
                                                      xemu.exe
```

- **Server** — a persistent process on Windows that holds a `vgamepad.VX360Gamepad`
  instance. It listens on `127.0.0.1:27000` for newline-delimited JSON commands.
  Requires ViGEmBus driver and `vgamepad` Python package.

- **Client** — a zero-dependency CLI that sends a single command to the server and
  prints the JSON response. Runs from WSL or Windows.

WSL2 localhost forwarding handles the networking between the Linux-based LLM and
the Windows-based server.

## Setup

### One-time

1. Install the [ViGEmBus driver](https://github.com/nefarius/ViGEmBus/releases) on Windows.
2. Install vgamepad in your **Windows** Python: `pip install vgamepad`
3. Open xemu, go to **Settings > Controllers**, and map the virtual controller.

### Per session

Start the server in a terminal (Windows Python):

```bash
python.exe tools/xbox/xbox_pad.py serve
```

The server prints a self-test confirmation and waits for connections. Leave it
running for the duration of the xemu session.

### Agent bootstrap

Claude, OpenCode, or any other agent should start with:

```bash
rtk python3 tools/xbox/xbox_pad.py ensure
```

`ensure` checks the TCP controller server and starts it if needed. From WSL it
launches Windows Python via the repo's local environment helpers, so the server
can hold the `vgamepad` device while the agent keeps using the Linux CLI.

For one-shot commands, agents can also pass `--auto-start`:

```bash
rtk python3 tools/xbox/xbox_pad.py --auto-start tap A
rtk python3 tools/xbox/xbox_pad.py --auto-start dpad down
```

If startup fails, inspect the JSON response's `log` path. Common causes are a
missing ViGEmBus driver, missing Windows `vgamepad`, or xemu not mapped to the
virtual controller.

## Commands

All commands accept `--host` and `--port` flags (defaults: `127.0.0.1`, `27000`).

### `tap` — press and release

```bash
python3 tools/xbox/xbox_pad.py tap A            # quick tap (~100ms)
python3 tools/xbox/xbox_pad.py tap Start -d 0.5 # hold for half a second
python3 tools/xbox/xbox_pad.py tap LB -d 0.3    # left bumper
```

Options:
- `-d, --duration` — hold time in seconds (default: `0.1`)

### `press` — hold a button

```bash
python3 tools/xbox/xbox_pad.py press A
```

The button stays held until `release` is called.

### `release` — release held input

```bash
python3 tools/xbox/xbox_pad.py release      # release everything (buttons + sticks + triggers)
python3 tools/xbox/xbox_pad.py release A    # release only button A
```

### `dpad` — tap a D-pad direction

```bash
python3 tools/xbox/xbox_pad.py dpad down
python3 tools/xbox/xbox_pad.py dpad up -d 0.2
```

Options:
- `-d, --duration` — hold time in seconds (default: `0.15`)
- Direction: `up`, `down`, `left`, `right`

### `stick` — set stick position

```bash
python3 tools/xbox/xbox_pad.py stick 0.0 -1.0           # left stick, full forward
python3 tools/xbox/xbox_pad.py stick 0.7 0.0 --which l  # left stick, right
python3 tools/xbox/xbox_pad.py stick 0.0 0.5 --which r  # right stick, down
```

Axes range from `-1.0` to `1.0`. Position persists until changed or `reset`.

Options:
- `--which` — `left`/`l` or `right`/`r` (default: `left`)

### `trigger` — set trigger value

```bash
python3 tools/xbox/xbox_pad.py trigger left 0.8
python3 tools/xbox/xbox_pad.py trigger rt 1.0
```

Value range: `0.0` (released) to `1.0` (fully pressed). Persists until changed or `reset`.

### `reset` — release all inputs

```bash
python3 tools/xbox/xbox_pad.py reset
```

Releases all buttons, centers both sticks, zeros both triggers.

### `status` — query current state

```bash
python3 tools/xbox/xbox_pad.py status
```

Returns the full controller state as JSON:

```json
{
  "ok": true,
  "buttons": ["A"],
  "lx": 0.0, "ly": -1.0,
  "rx": 0.0, "ry": 0.0,
  "lt": 0.0, "rt": 0.0
}
```

### `ensure` — check or start server

```bash
rtk python3 tools/xbox/xbox_pad.py ensure
rtk python3 tools/xbox/xbox_pad.py ensure --no-start
```

Returns JSON with `running`, `started`, and when autostart was attempted, a
server `log` path.

### `sequence` — play JSON input sequence

```bash
rtk python3 tools/xbox/xbox_pad.py --auto-start sequence artifacts/input/menu.json
```

Sequence files are agent-neutral JSON. They can be a list or an object with a
`steps` list:

```json
{
  "steps": [
    {"tap": "Start", "duration": 0.2},
    {"wait": 0.5},
    {"dpad": "down"},
    {"action": "tap", "button": "A", "duration": 0.1},
    {"action": "stick", "which": "left", "x": 0.0, "y": -1.0},
    {"wait": 1.0},
    {"action": "reset"}
  ]
}
```

Any raw protocol command accepted by the TCP server can be used as a sequence
step. `wait`/`sleep`, `tap`, and `dpad` have shorthand forms for readability.

## Automation Strategy

Use the lightest layer that fits the task:

| Need | Use | Notes |
|------|-----|-------|
| Agent-driven menu navigation or short in-game actions | `xbox_pad.py` commands | Human-readable, easy to adjust from screenshots. |
| Deterministic agent script playback | `xbox_pad.py sequence` | JSON input recipe; portable across Claude, OpenCode, Codex, and shell. |
| Exact replay of a human gameplay run | Native `state.data` recording | Binary per-tick controller-state packets consumed by the game engine. |
| Debug keyboard or console keys | `xemu_key.py` | QMP keyboard only; not analog controller input. |

`xbox_pad.py sequence` is the recommended script format for new automation.
It is simple JSON and can be edited by agents. The native `state.data` format is
better for replaying a recorded human run exactly, but it is binary engine state,
not a convenient authoring format.

## Native Input-State Recording and Playback

Halo's Xbox input layer has a built-in controller-state recorder. During input
initialization, `input_check_state_mode()` in `src/halo/input/input_xbox.c`
checks for sentinel files under `D:\`:

| Sentinel file | Mode | Behavior |
|---------------|------|----------|
| `D:\write.xts` | Record | Opens `D:\state.data` for writing and appends one `input_gamepad_state` packet per gamepad per tick. |
| `D:\read.xts` | Playback | Opens `D:\state.data` for reading and consumes packets once. |
| `D:\loop.xts` | Loop playback | Reads packets and seeks back to the beginning when EOF is reached. |

Each packet is `0x28` bytes. The engine processes all 4 gamepads each tick, so a
single-player recording usually advances in `4 * 0x28 = 0xa0` byte groups per
tick even when only controller 0 matters.

This is separate from `cutscene_recording` / `recorded_animations`, which drives
scripted unit/cinematic animation streams rather than player controller input.

> **Automated workflow:** `tools/xbox/capture_scenario.py` wraps the entire
> arm/record/close/download/trim/store flow below into one command and stores the
> result as a named, self-contained per-level fixture (input + checkpoint core +
> deploy recipe). See `docs/input-fixture-capture.md`. The manual steps below are
> the underlying mechanism it drives.

### Boot Directly Into a Campaign Map

The console startup path evaluates `D:\init.txt` line by line with
`hs_console_evaluate()`. To boot into a campaign level before recording, place an
`init.txt` in the deployed title root containing a map command, for example:

```text
map_name levels\a10\a10
```

`deploy_xbox.py` already uploads repo-root `init.txt` to the deployed title
directory. If the title is deployed at the default `E:\GAMES\halo-patched`, that
directory is the practical place to upload files that the running game opens as
`D:\...`.

### Record Human Gameplay

1. Ensure xemu or the Xbox is using the deployed Halo title root.
2. Put `init.txt` in the title root if you want to skip menus and boot a map.
3. Upload an empty `write.xts` sentinel to the same title root before launching.
4. Launch the title and play normally with a real controller or `xbox_keyboard_pad.py`.
5. Quit or reboot the title so the file is closed.
6. Download `state.data` from the title root and save it under `artifacts/input/`.
   Promote reusable captures into the local corpus with
   `tools/xbox/input_recordings.py add`.
7. Delete `write.xts` from the title root before the next launch.

Example XBDM commands, assuming the default deployed title root:

```bash
rtk python3 tools/xbox/xbdm_rdcp.py --sendfile init.txt "E:\GAMES\halo-patched\init.txt"
rtk python3 tools/xbox/xbdm_rdcp.py --sendfile /tmp/write.xts "E:\GAMES\halo-patched\write.xts"
rtk python3 tools/xbox/xbdm_rdcp.py --json 'getfileattributes name="E:\GAMES\halo-patched\state.data"'
```

For `getfile`, first get the remote file size `N`, then request `N` bytes with a
binary length of `N + 4`; RDCP prefixes the payload with a 4-byte little-endian
reported size:

```bash
rtk python3 tools/xbox/xbdm_rdcp.py \
  'getfile name="E:\GAMES\halo-patched\state.data" offset=0 size=N' \
  --binary-length N_PLUS_4 \
  --output artifacts/input/a10_state.data.raw
```

Strip the first 4 bytes from the `.raw` file before using it as `state.data` for
playback.

For recordings worth keeping, promote the raw capture into the per-level corpus:

```bash
rtk python3 tools/xbox/input_recordings.py add \
  --level a10 \
  --source artifacts/input/a10_state.data.raw \
  --strip-rdcp-prefix \
  --title "a10 cryo exit" \
  --purpose "Reusable route for startup regression checks" \
  --start-condition "Fresh a10 boot via init.txt; no checkpoint"
```

The local corpus lives under `input-recordings/`.

### Replay a Recording

1. Upload the stripped `state.data` file to the title root.
2. Upload an empty `read.xts` sentinel for one-shot playback, or `loop.xts` for
   continuous loop playback.
3. Remove `write.xts`; if both record and playback sentinels exist,
   `write.xts` wins because `input_check_state_mode()` checks it first.
4. Launch the title with the same map and startup conditions used while
   recording.
5. Remove `read.xts` / `loop.xts` when finished.

Example:

```bash
rtk python3 tools/xbox/xbdm_rdcp.py --sendfile input-recordings/levels/a10/<recording-id>/state.data "E:\GAMES\halo-patched\state.data"
rtk python3 tools/xbox/xbdm_rdcp.py --sendfile /tmp/read.xts "E:\GAMES\halo-patched\read.xts"
rtk python3 tools/xbox/xbdm_rdcp.py 'delete name="E:\GAMES\halo-patched\write.xts"'
```

Recordings are only deterministic when map, build, difficulty, checkpoint/state,
and frame progression match closely. For exploratory agent control, prefer
`xbox_pad.py sequence`; for replaying a known route through a level, use native
`state.data` playback.

## Debug Keyboard

For Xbox debug-key or keyboard automation, use QMP instead of the virtual
controller:

```bash
rtk python3 tools/xbox/xemu_key.py tap grave_accent
rtk python3 tools/xbox/xemu_key.py tap f1
rtk python3 tools/xbox/xemu_key.py tap ctrl+alt+f1
rtk python3 tools/xbox/xemu_key.py sequence artifacts/input/debug_keys.json
```

`xemu_key.py` uses xemu QMP `send-key`; it does not require ViGEmBus or
`vgamepad`. It is keyboard-only and cannot drive analog gamepad state.

## Button names

| Button | Aliases |
|--------|---------|
| `A` | — |
| `B` | — |
| `X` | — |
| `Y` | — |
| `Start` | — |
| `Back` | `Select` |
| `LB` | `L1`, `LShoulder` |
| `RB` | `R1`, `RShoulder` |
| `LS` | `L3`, `LThumb` (left stick click) |
| `RS` | `R3`, `RThumb` (right stick click) |
| `Up` | D-pad up |
| `Down` | D-pad down |
| `Left` | D-pad left |
| `Right` | D-pad right |

## Protocol

The server speaks newline-delimited JSON over TCP. Each request is a JSON object
with an `action` field:

```jsonc
// Client → Server
{"action": "tap", "button": "A", "duration": 0.1}

// Server → Client
{"ok": true, "tapped": "A", "duration": 0.1}
```

All responses include `"ok": true/false`. On failure, an `"error"` field is present.

## Typical LLM debugging loop

```
1. Capture screenshots: `rtk python3 tools/xbox/xdbm_screenshot.py --host 127.0.0.1 --images 5 --png`
2. Analyze the current screen state
3. Navigate: python3 tools/xbox/xbox_pad.py dpad down
4. Wait a beat, capture screenshots again with `rtk python3 tools/xbox/xdbm_screenshot.py --host 127.0.0.1 --images 5 --png`
5. Confirm: python3 tools/xbox/xbox_pad.py tap A
6. Repeat as needed
7. python3 tools/xbox/xbox_pad.py reset  # clean up when done
```

## Relationship to xbox_keyboard_pad.py

`tools/xbox/xbox_keyboard_pad.py` is the original keyboard-to-controller mapper for
human use. It hooks physical keyboard input and requires the user to press keys
while xemu is focused.

`tools/xbox/xbox_pad.py` replaces it for automated/LLM use by exposing a TCP API
instead of a keyboard hook. Both scripts use the same `vgamepad` + ViGEmBus stack
and produce identical virtual controller hardware.
