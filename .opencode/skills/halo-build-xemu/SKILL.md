---
name: halo-build-xemu
description: "build load, build-load, xbe deploy, build_deploy_run, xemu build: Standard project build, deploy, and run workflow"
---

# Halo Build, Deploy & Run

Use this skill whenever work needs to build the project and deploy to xemu or
real Xbox.

## Target Binary Contract

The target executable is the **Halo Xbox debug build 2276** ("cachebeta"):
- File: `halo-patched/cachebeta.xbe`
- MD5: `c7869590a1c64ad034e49a5ee0c02465`
- Version: `01.10.12.2276` (Oct 12, 2001 debug build with rich symbols/asserts)
- **Note:** `kb.json` addresses are absolute VAs into THIS exact file. Do not swap in a retail `default.xbe`.

## Toolchain & Environment Requirements

Required toolchain: **clang + lld (`lld-link`) + cmake + python3**.
Pinned python dependencies: `pefile~=2023.2.7`, `pyxbe~=1.0.2`, `libclang~=16.0.0`, `setuptools<81` (newer setuptools removed `pkg_resources`), `capstone`.

### Direct CMake Interface
```bash
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=toolchains/llvm.cmake
cmake --build build
```

### Windows Setup
Install via winget: `LLVM.LLVM`, `Kitware.CMake`, `Python.Python.3.12`, `Ninja-build.Ninja`.
If MSVC CRT is not installed, configure with static lib test:
```bash
cmake -B build -S . -G Ninja -DCMAKE_TOOLCHAIN_FILE=toolchains/llvm.cmake \
  -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY
cmake --build build
```

### Linux Non-Root User-Space Setup
Bootstrap clang/lld without root access using `apt-get download`:
```bash
mkdir -p /tmp/debs && cd /tmp/debs
apt-get download clang-14 lld-14 libllvm14 libclang-cpp14 \
    libclang-common-14-dev libclang1-14 llvm-14-linker-tools llvm-14
mkdir -p ~/llvm && for d in *.deb; do dpkg-deb -x "$d" ~/llvm; done
export PATH=~/llvm/usr/lib/llvm-14/bin:$HOME/.local/bin:$PATH
export LD_LIBRARY_PATH=~/llvm/usr/lib/llvm-14/lib:~/llvm/usr/lib/x86_64-linux-gnu
pip install --break-system-packages cmake pefile~=2023.2.7 pyxbe~=1.0.2 libclang~=16.0.0 setuptools<81 capstone
```

## Canonical Build & Deploy Workflow

```bash
./tools/xbox/build_deploy_run.sh -q
```

This single command handles build (`tools/build/build.py`) and XBDM deploy
(`tools/xbox/deploy_xbox.py`) in one step. No ISO creation is needed — the XBE
is hot-patched directly into the running instance.

## Variants

| Target | Command |
|--------|---------|
| xemu (local) | `./tools/xbox/build_deploy_run.sh -q` |
| Real Xbox | `./tools/xbox/build_deploy_run_real_hw.sh -q` |
| Custom host | `./tools/xbox/build_deploy_run.sh --xbox <host> -q` |

The real-hardware wrapper sets `XBOX_HOST` to `10.0.0.29` by default.

## Standalone ISO Workflow & xemu Configuration

When running standalone in xemu without XBDM hot-patching:
1. Copy the full **2276 build directory** (maps/, bink/, etc.) and replace its `default.xbe` with `halo-patched/default.xbe`.
2. Pack an ISO: `extract-xiso -c "<dir>" out.iso`.
3. In xemu settings:
   - **System Memory = 128 MiB** (CRITICAL: 128 MiB is required for the debug build!).
   - Internal resolution = 1x, properly-formatted HDD image, Complex BIOS.
4. Load ISO and select **Machine → Reset** to boot (resetting from menu is required on each load).

## xemu Control Notes

- Default xemu host is `127.0.0.1` (override via `XBOX_HOST` or `--xbox`).
- Use `xemu_*` tools directly for monitor control — they are the primary
  interface (the daemon auto-starts via SessionStart hook):
  - `xemu_xemu_status` — QMP connection and VM status
  - `xemu_xemu_pause` / `xemu_xemu_resume` — pause/resume VM
  - `rtk python3 tools/xbox/xbdm_screenshot.py --host 127.0.0.1 --images 5 --png` — capture screen over XBDM
  - `xemu_xemu_send_monitor_command("info registers")` — HMP passthrough
  - `xemu_xemu_send_monitor_command("x /16xw 0x<addr>")` — examine memory
- `tools/xbox/xemu_qmp.py` is a fallback only when the MCP daemon is unavailable.

## Controller Automation

- Use `rtk python3 tools/xbox/xbox_pad.py ensure` to start/check the
  LLM-controllable virtual controller server.
- Use `tools/xbox/xbox_pad.py sequence <json>` for agent-readable controller
  scripts.
- Use native `state.data` playback for exact route replays. Reusable per-level
  recordings live under `input-recordings/`; see `docs/xbox-pad.md`.

## Report Format

Report:

- build status
- commit hash from `git rev-parse HEAD` when the build succeeds
- deploy target (xemu / real Xbox)
- any warnings or errors

