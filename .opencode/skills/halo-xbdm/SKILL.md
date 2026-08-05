---
name: halo-xbdm
description: "xbdm, rdcp, real xbox, getmem, deploy, hot patch, getfile, core.bin: Standard RDCP, XBDM, build-and-deploy, and file transfer commands."
---

# Halo XBDM & Real Xbox Operations

**Use this skill for:**
- Talking to a real Xbox or xemu over XBDM / RDCP via `tools/xbox/xbdm_rdcp.py`
- Building and deploying patched files directly to a real Xbox via `tools/xbox/deploy_xbox.py`
- Pulling files (e.g. core dumps `core.bin`) off the Xbox/xemu HDD via `tools/xbox/xbdm_getfile.py`

---

## 1. Build and Deploy to Real Xbox

Preferred tools:
- Build: `cmake --build build`
- Deploy: `rtk python3 tools/xbox/deploy_xbox.py`
- Convenience wrapper: `./tools/xbox/build_deploy_run_real_hw.sh -q` (sets `XBOX_HOST=10.0.0.29` default)

### Flow:
1. Run `cmake --build build`.
2. Deploy to Xbox: `rtk python3 tools/xbox/deploy_xbox.py`.
3. `deploy_xbox.py` automatically uploads `init.txt` to `E:\GAMES\halo-patched\init.txt`. Edit `init.txt` to control map loading and game-state checkpoint restoration (see `docs/boot-init-and-checkpoints.md`).

---

## 2. File Transfers (xbdm_getfile)

Pull files (e.g., `core_save` dump `core.bin`) off the running xemu or Xbox HDD safely in ONE command without halting the CPU:

```bash
# Fetch patched-build Halo core dump:
rtk python3 tools/xbox/xbdm_getfile.py --core -o artifacts/equivalence/core_patched.bin

# Any file:
rtk python3 tools/xbox/xbdm_getfile.py 'E:\GAMES\halo-patched\core\core.bin' -o out.bin

# Discover directory paths + sizes:
rtk python3 tools/xbox/xbdm_getfile.py --list 'E:\GAMES\halo-patched\core'
```

**RDCP Wire Format Gotcha:** RDCP `getfile` binary responses are prefixed with a 4-byte LE length `N` followed by `N` bytes of data. `xbdm_getfile.py` handles this automatically; do not hand-roll raw socket reads.

---

## 3. Title Launch & Console Commands

### Title Launch (`magicboot`):
Always use `debug` flag without quotes around paths:
```
magicboot title=E:\GAMES\halo-patched\default.xbe debug
```

### Consolidated Command Modes (`xbdm_rdcp.py`):
- Direct tool use: `rtk python3 tools/xbox/xbdm_rdcp.py --json ...`
- Modes: `/xbdm raw`, `/xbdm mem`, `/xbdm context`, `/xbdm threads`, `/xbdm modules`, `/xbdm status`, `/xbdm halt`, `/xbdm continue`, `/xbdm dir`, `/xbdm fileattrs`, `/xbdm debug`.

---

## 4. Input Recording Files

The running title opens control files from the title root as `D:\...`:
- `init.txt`: startup map & checkpoint loader
- `write.xts`: records `state.data`
- `read.xts` / `loop.xts`: replays input fixtures
Upload to `E:\GAMES\halo-patched\` and store local per-level captures under `input-recordings/` (see `docs/xbox-pad.md`).
