---
name: debug-xemu
description: >-
  xemu-specific debugging cookbook — MCP tools, raw QMP socket recipe, GDB via
  QMP gdbserver, screenshot capture, memory examination, serial monitoring, and
  live memory dump for equivalence testing. Invoke when probing xemu state,
  attaching GDB, capturing screenshots, reading memory, or building equivalence
  snapshots. Consolidates recipes from scattered memory entries into one place.
---

# Debug xemu — Probing and Inspection Cookbook

**Invoke this skill when you need to:**
- Inspect xemu register state, memory, or serial output
- Attach GDB to the running VM
- Capture screenshots
- Dump live memory for equivalence testing
- Drive xemu with scripted controller input or replay native `state.data`
- Work around xemu MCP connection issues

Argument: `$ARGUMENTS` (what you need to inspect or capture)

---

## A — xemu MCP Tools (Primary Path)

The xemu MCP daemon auto-starts via the SessionStart hook. These are the
fastest tools when the MCP is healthy:

### Visual state
```
mcp__xemu__xemu_screenshot()
```
Returns a PNG. For multi-frame capture, use the script instead (Section D).

### Register state
```
mcp__xemu__xemu_send_monitor_command("info registers")
```

### Memory examination
```
mcp__xemu__xemu_send_monitor_command("x /16xw 0x<addr>")
```
Format: `x /COUNTFORMATSIZEw ADDR`. Examples:
- `x /4xw 0x5a8d50` — 4 hex words at object table pointer
- `x /64xb 0x31fc38` — 64 hex bytes at forward vector

### Serial output (asserts, debug prints)
```
mcp__xemu__xemu_read_serial()
```
Assert messages and `debug_print()` output appear here.

### Execution control
```
mcp__xemu__xemu_pause()     # halt VM
mcp__xemu__xemu_resume()    # continue VM
mcp__xemu__xemu_reset()     # hard reset
```

### Known issue: "connection reset by peer"

After a redeploy or when the MCP daemon's QMP attachment goes stale, MCP tools
return connection errors. **This does NOT mean xemu is dead.**

Verify with a raw QMP probe (Section B). If raw QMP works, bypass the MCP:
- Screenshots: Section D (xdbm_screenshot.py)
- Memory: Section B (raw QMP memsave)
- Registers: `rtk python3 tools/xbox/xemu_qmp.py "info registers"`

---

## B — Raw QMP Socket (Bypass When MCP Fails)

### Connection

xemu QMP endpoint: `127.0.0.1:4444` (single-client — close any other QMP
connections first).

```python
import socket, json

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(('127.0.0.1', 4444))

# 1. Receive greeting
greeting = sock.recv(4096)

# 2. Negotiate capabilities
sock.sendall(json.dumps({"execute": "qmp_capabilities"}).encode() + b'\n')
resp = sock.recv(4096)  # expect {"return": {}}

# 3. Now send commands
```

### Virtual memory read (memsave)

**Use `memsave` (virtual addresses), NOT `pmemsave` (physical).** Cerbios does
not identity-map game VA, so `pmemsave` reads wrong bytes for game data.

```python
# Pause first
sock.sendall(json.dumps({"execute": "stop"}).encode() + b'\n')
sock.recv(4096)

# Read memory — DOUBLE BACKSLASHES in Windows paths (escape gotcha)
cmd = {
    "execute": "human-monitor-command",
    "arguments": {"command-line": "memsave 0x5a8d50 128 \"G:\\\\dev\\\\halo\\\\dump.bin\""}
}
sock.sendall(json.dumps(cmd).encode() + b'\n')
sock.recv(4096)

# Resume
sock.sendall(json.dumps({"execute": "cont"}).encode() + b'\n')
```

**CRITICAL path escaping:** The HMP parser inside xemu requires escaped
backslashes. In Python, that means FOUR backslashes per real backslash:
`"G:\\\\dev\\\\halo\\\\"` → value sent over wire → `G:\\dev\\halo\\` → HMP
interprets as `G:\dev\halo\`.

### Verification gate (BEFORE trusting any dump)

After capturing, verify the dump contains real game data:

| Check | Expected Value | If Wrong |
|-------|---------------|----------|
| Forward vector ptr @ VA `0x31fc38` | `≈ 0x0028xxxx` | Dump is from wrong context (menu/idle, not gameplay) |
| Object table: `*0x5a8d50` | `≈ 0x80xxxxxx` | VM not in game state |
| Object table datum magic: `*(0x5a8d50)+0x28` | `0x64407440` | Table corrupt or wrong address |
| Player datum +0x34 (unit handle) | `!= 0xffffffff` | Player not spawned (capture during active gameplay) |

**Capture ONLY during active gameplay with a spawned player.** Menu/idle pauses
do not have the game address space live.

---

## C — GDB via QMP Gdbserver

The MCP `xemu_attach_gdb` tool is **BROKEN**. Use QMP human-monitor instead.

### Start the gdbserver stub

```
mcp__xemu__xemu_send_monitor_command("gdbserver tcp::1234")
```

The guest should report "Waiting for gdb connection on device tcp::1234".

### Connect GDB

```bash
timeout -s KILL 30 gdb -batch -nx \
  -ex "set architecture i386" \
  -ex "maint set target-async off" \
  -ex "target remote 127.0.0.1:1234" \
  -ex "info registers" \
  -ex "x/10i \$eip" \
  -ex "detach" \
  -ex "quit"
```

**Key flags:**
- `maint set target-async off` MUST come BEFORE `target remote` (else
  "Cannot execute this command while the target is running")
- `timeout -s KILL 30` prevents gdb from hanging indefinitely on a blocked
  remote read (gdb ignores SIGTERM while in remote protocol)
- `-batch -nx` for non-interactive scripted use

### Software breakpoints

**Software breakpoints DO fire** through the gdb stub:

```bash
timeout -s KILL 30 gdb -batch -nx \
  -ex "set architecture i386" \
  -ex "maint set target-async off" \
  -ex "target remote 127.0.0.1:1234" \
  -ex "break *0x000XXXXX" \
  -ex "continue" \
  -ex "info registers" \
  -ex "bt" \
  -ex "delete" \
  -ex "detach" \
  -ex "quit"
```

### CRITICAL: Always clean up

**A SIGKILLed or crashed gdb session leaves its breakpoint installed** in the
guest. The game will stop every time execution hits that VA — it appears frozen
or stuck. This is NOT a game bug.

**ALWAYS run cleanup after any GDB session:**

```bash
timeout -s KILL 10 gdb -batch -nx \
  -ex "set architecture i386" \
  -ex "maint set target-async off" \
  -ex "target remote 127.0.0.1:1234" \
  -ex "delete" \
  -ex "detach" \
  -ex "quit"
```

### Gotchas

1. **Halted/crashed game:** If the game is already halted (crashed or paused at
   kernel idle), breakpoints will NEVER fire — the CPU is not executing game
   code. Verify the game is live first: `info registers` should show EIP
   roaming in game code ranges (0x10000–0x1Dxxxx or 0x642000+), not pinned at
   kernel idle (0x8001exxx).

2. **Multiple gdb connections:** QMP gdbserver is single-client. Close any
   previous gdb session before starting a new one.

3. **Lingering breakpoints after force-kill:** If the game seems stuck at a
   specific address after a gdb session, run the cleanup commands above.

---

## D — Screenshot Capture

**Always use the Python script, NOT QMP screendump:**

```bash
rtk python3 tools/xbox/xdbm_screenshot.py --host 127.0.0.1 --images 5 --png
```

This captures 5 frames as PNG. The xemu MCP screenshot can work but resets
after redeploy. The script is more reliable.

For a single quick shot via MCP (when it's healthy):
```
mcp__xemu__xemu_screenshot()
```

---

## D2 — Controller Input and Replay

For agent-driven controller input, use `tools/xbox/xbox_pad.py` rather than
`xbox_keyboard_pad.py`. `xbox_pad.py` exposes a TCP-backed ViGEmBus controller
and supports JSON `sequence` playback for Claude/OpenCode/Codex automation.

For exact replay of human gameplay, use Halo's native input recorder:
`D:\write.xts` records `D:\state.data`, `D:\read.xts` plays it once, and
`D:\loop.xts` loops it. Store reusable per-level recordings under
`input-recordings/` with `tools/xbox/input_recordings.py add`. Full workflow:
`docs/xbox-pad.md`.

---

## E — Live Memory Dump for Equivalence Testing

### Full 128MB dump

Use `dump_xemu_memory.py` for the full dump workflow:

```bash
# Via QMP (local xemu):
rtk python3 tools/equivalence/dump_xemu_memory.py dump --method qmp -o /tmp/full_dump.bin

# Via XBDM (real Xbox or xemu with XBDM):
rtk python3 tools/equivalence/dump_xemu_memory.py dump --method xbdm -o /tmp/full_dump.bin
```

**Verify the dump before proceeding** (see Section B verification gate).

### Build a compact snapshot

```bash
rtk python3 tools/equivalence/dump_xemu_memory.py snapshot \
  --dump /tmp/full_dump.bin \
  --target <func_name_or_addr> \
  --full \
  -o artifacts/snapshot_<func>.json
```

`--full` maps all non-zero pages (includes code segments callees need). Without
it, only targeted data regions are extracted — insufficient when callees aren't
stubbed.

### Pin function arguments to real handles

```bash
rtk python3 tools/equivalence/dump_xemu_memory.py snapshot \
  --dump /tmp/full_dump.bin \
  --target <func> \
  --full \
  --arg dead_handle 0xec700000 \
  --arg player_index 0 \
  -o artifacts/snapshot_<func>.json
```

### Run equivalence with the snapshot

```bash
rtk python3 tools/equivalence/unicorn_diff.py <target> \
  --seeds 50 \
  --allow-stubs \
  --mem-trace \
  --state-snapshot artifacts/snapshot_<func>.json
```

### Virtual vs Physical address gotchas

| Method | Address Space | Use When |
|--------|--------------|----------|
| QMP `memsave` | Virtual (game VA) | Default for game data — what the CPU sees |
| QMP `pmemsave` | Physical | Only for kernel/hardware pages; Cerbios does NOT identity-map game VA |
| XBDM `getmem` | Virtual | Real Xbox (works but has been unreliable — produced all-zeros on some captures) |

**Never use `pmemsave` for game data.** The virtual-to-physical mapping under
Cerbios is non-trivial; `pmemsave` will read wrong bytes at game addresses.

**Never use QEMU `savevm`/`loadvm`** for oracle tests — those restore old
loaded-XBE code pages, invalidating original-vs-candidate comparisons.

---

## F — Quick Reference

| Task | Command |
|------|---------|
| Pause VM | `mcp__xemu__xemu_pause()` |
| Resume VM | `mcp__xemu__xemu_resume()` |
| Screenshot | `rtk python3 tools/xbox/xdbm_screenshot.py --host 127.0.0.1 --images 5 --png` |
| Registers | `mcp__xemu__xemu_send_monitor_command("info registers")` |
| Memory at addr | `mcp__xemu__xemu_send_monitor_command("x /16xw 0x<addr>")` |
| Serial/asserts | `mcp__xemu__xemu_read_serial()` |
| GDB attach | `mcp__xemu__xemu_send_monitor_command("gdbserver tcp::1234")` then gdb connect |
| GDB cleanup | `gdb -ex "target remote :1234" -ex "delete" -ex "detach" -ex "quit"` |
| Full mem dump | `rtk python3 tools/equivalence/dump_xemu_memory.py dump --method qmp -o /tmp/dump.bin` |
| Build snapshot | `rtk python3 tools/equivalence/dump_xemu_memory.py snapshot --dump /tmp/dump.bin --target <func> --full -o snap.json` |
