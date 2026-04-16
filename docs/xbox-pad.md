# xbox_pad.py — Virtual Controller for xemu

`tools/xbox_pad.py` creates a virtual Xbox 360 controller that xemu can use,
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
python.exe tools/xbox_pad.py serve
```

The server prints a self-test confirmation and waits for connections. Leave it
running for the duration of the xemu session.

## Commands

All commands accept `--host` and `--port` flags (defaults: `127.0.0.1`, `27000`).

### `tap` — press and release

```bash
python3 tools/xbox_pad.py tap A            # quick tap (~100ms)
python3 tools/xbox_pad.py tap Start -d 0.5 # hold for half a second
python3 tools/xbox_pad.py tap LB -d 0.3    # left bumper
```

Options:
- `-d, --duration` — hold time in seconds (default: `0.1`)

### `press` — hold a button

```bash
python3 tools/xbox_pad.py press A
```

The button stays held until `release` is called.

### `release` — release held input

```bash
python3 tools/xbox_pad.py release      # release everything (buttons + sticks + triggers)
python3 tools/xbox_pad.py release A    # release only button A
```

### `dpad` — tap a D-pad direction

```bash
python3 tools/xbox_pad.py dpad down
python3 tools/xbox_pad.py dpad up -d 0.2
```

Options:
- `-d, --duration` — hold time in seconds (default: `0.15`)
- Direction: `up`, `down`, `left`, `right`

### `stick` — set stick position

```bash
python3 tools/xbox_pad.py stick 0.0 -1.0           # left stick, full forward
python3 tools/xbox_pad.py stick 0.7 0.0 --which l  # left stick, right
python3 tools/xbox_pad.py stick 0.0 0.5 --which r  # right stick, down
```

Axes range from `-1.0` to `1.0`. Position persists until changed or `reset`.

Options:
- `--which` — `left`/`l` or `right`/`r` (default: `left`)

### `trigger` — set trigger value

```bash
python3 tools/xbox_pad.py trigger left 0.8
python3 tools/xbox_pad.py trigger rt 1.0
```

Value range: `0.0` (released) to `1.0` (fully pressed). Persists until changed or `reset`.

### `reset` — release all inputs

```bash
python3 tools/xbox_pad.py reset
```

Releases all buttons, centers both sticks, zeros both triggers.

### `status` — query current state

```bash
python3 tools/xbox_pad.py status
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
1. Screenshot xemu (xemu_xemu_screenshot)
2. Analyze the current screen state
3. Navigate: python3 tools/xbox_pad.py dpad down
4. Wait a beat, screenshot again
5. Confirm: python3 tools/xbox_pad.py tap A
6. Repeat as needed
7. python3 tools/xbox_pad.py reset  # clean up when done
```

## Relationship to xbox_keyboard_pad.py

`tools/xbox_keyboard_pad.py` is the original keyboard-to-controller mapper for
human use. It hooks physical keyboard input and requires the user to press keys
while xemu is focused.

`tools/xbox_pad.py` replaces it for automated/LLM use by exposing a TCP API
instead of a keyboard hook. Both scripts use the same `vgamepad` + ViGEmBus stack
and produce identical virtual controller hardware.
