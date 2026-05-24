# Game-State Snapshot Verification

Snapshot verification tests ported C functions against real xemu-captured game
memory. Unlike seed-based equivalence testing (which uses zero-filled globals),
snapshot verification exercises functions with actual in-game memory state.

## Pipeline Overview

```
xemu (QMP + GDB stub)   →   capture snapshot   →   verify with unicorn_diff   →   report to dashboard
```

### Architecture

| Tool | Purpose |
|------|---------|
| `tools/equivalence/game_state_snapshot.py` | Capture memory snapshots from xemu via GDB RSP |
| `tools/equivalence/game_state_verify.py` | Run unicorn_diff on ported functions with snapshot data |
| `tools/report/generate_decomp_report.py` | Embed snapshot results in progress report |
| `tools/report/progress_server.py` | Serve blam.info/progress dashboard |
| `.github/workflows/snapshot-tests.yml` | CI pipeline: build → verify → report |

## Capture

### Prerequisites

xemu must be running with GDB stub (`-g 1234`) and QMP (`-q 4444`).
A checkpoint must be loaded (e.g., saved at a known game state).

```bash
python3 tools/equivalence/game_state_snapshot.py capture \
    --output artifacts/snapshots/derelict.json \
    --checkpoint "main_menu"
```

The capture:
1. Auto-pauses xemu via QMP `stop`
2. Reads memory regions via GDB RSP chunked reads (4096 bytes/packet)
3. Auto-resumes xemu via QMP `cont`

### Captured Regions

| Region | Address | Size |
|--------|---------|------|
| Globals | 0x4EA990 | configurable |
| Xbox globals | XBE-relative | configurable |
| CPU pool | 0x80061000 | ~3.1 MB |
| GPU pool | 0x00345000 | 256 KB |
| Callback table | 0x32EAA0 | 64 B |
| Scenario index | 0x326A08 | 8 B |
| Map type | 0x31FA94 | 8 B |

### Minimal CI Snapshot

The CI uses a minimal snapshot (`ci/snapshot.json`, 493 bytes) with only
globals + callback table — sufficient for functions that don't depend on
pool memory. Functions referencing pool data are handled via `_safe_write`
auto-mapping in unicorn_diff.

## Verification

### Running verification

```bash
# Verify all functions in an object file
python3 tools/equivalence/game_state_verify.py \
    --snapshot artifacts/snapshots/derelict.json \
    --objects game_state.obj --seeds 100 \
    --output artifacts/equivalence/ci_results.json

# Verify specific functions
python3 tools/equivalence/game_state_verify.py \
    --snapshot artifacts/snapshots/derelict.json \
    --funcs game_state_dispose,game_state_save

# Test ALL ported functions (2,275 functions — slow)
python3 tools/equivalence/game_state_verify.py \
    --snapshot artifacts/snapshots/derelict.json \
    --all --seeds 100
```

### Results format (`ci_results.json`)

```json
{
  "results": [
    {
      "func": "game_state_dispose",
      "object": "game_state.obj",
      "addr": "0x4e9a90",
      "passed": 100,
      "failed": 0,
      "errors": 0,
      "coverage": 100.0,
      "confidence": "high",
      "total_seeds": 100,
      "elapsed_s": 1.2
    }
  ],
  "summary": {
    "total": 12,
    "passed": 7,
    "avg_coverage_pct": 50.0
  }
}
```

## Progress Report Integration

Snapshot results flow into the [blam.info/progress](https://blam.info/progress/) dashboard.

### Data flow

```
game_state_verify.py               generate_decomp_report.py          progress_server.py
    ↓                                       ↓                               ↓
ci_results.json  ─────────────→  _load_snapshot_data()  ─────→  report.json  ─────→  blam.info/progress
```

The report generator reads `artifacts/equivalence/ci_results.json` and embeds
per-function snapshot pass/fail, coverage, and confidence into the report JSON.

### What the dashboard shows

| Dashboard element | Snapshot data |
|-------------------|--------------|
| **Summary card** | "Snapshot Verified" — passed/total count, % of ported |
| **Verification funnel** | "Snap verified" and "Snap passed" tiers |
| **TU heatmap** | Purple shades: snap_some (#7c3aed), snap_pass (#a855f7) |
| **Unit detail** | Per-unit: tested, passed, avg coverage |
| **Function table** | Per-function: pass/fail badge, coverage bar |

Evidence levels (in priority order):
1. `snap_pass` — snapshot verified & passes all seeds
2. `snap_some` — snapshot tested but not all pass
3. `eq_high` — equivalence high-confidence
4. `eq_some` — equivalence tested
5. `vc71` — VC71 byte-match scored
6. `unverified` — ported but not verified
7. `no_ported` — no ported functions

## CI Integration

The `.github/workflows/snapshot-tests.yml` workflow runs on push/PR to
src/, tools/equivalence/, kb.json, and delinked/:

1. Build halo target
2. Run `game_state_verify.py` on `game_state.obj` with minimal snapshot
3. Copy results to `artifacts/progress/snapshot_results.json`
4. Generate progress report HTML

### Trigger paths

```yaml
on:
  push:
    paths:
      - "src/**"
      - "tools/equivalence/**"
      - "kb.json"
      - "delinked/game_state.obj"
```

## Status (2026-05-24)

| Object | Functions | Passed | Failing |
|--------|-----------|--------|---------|
| game_state.obj | 12 | 7 | 5 |

**Passing**: dispose, dispose_from_old_map, initialize_for_new_map, save,
save_core, malloc, gpu_alloc

**Failing** (known blockers):
- `call_after_load_procs` — loops through 13 callback pointers
- `revert` — callback at 0x32EAA4 triggers fetch
- `data_new`, `memory_pool_new` — pre-existing stack corruption (eip=0x1ffffc)
- `load_core` — XAPI file I/O

## Key Implementation Details

### GDB RSP Protocol
- Chunked reads at 4096 bytes/packet (`m<addr>,<len>`)
- `$` prefix stripped from responses
- Notification `%` packets skipped
- Chunked writes for large memory ranges

### Unicorn Memory Mapping
- `_safe_write`/`_safe_read` in stubs auto-map pages before access
- Multi-page snapshot mapping (not just first 64 KB of pool)
- Globals-slot seeding from snapshot data via prefix matching (DAT_, PTR_,
  PTR_FUN_, PTR_DAT_, s_)

### Indirect Call Pre-map
- Scans lifted code for `mov reg, IMM; call reg` patterns
- Pre-maps stub targets with `xor eax,eax; ret` stubs
- Callback pre-map: scans globals_seeds for 4-byte pointers in XBE code range

### fetch_hook Direct-EIP
- Unknown code fetches handled by setting EAX=0, popping return address,
  and setting EIP directly (no RET execution)
- Fallback for already-mapped pages: mem_protect + stub
- Stub page extended to 0x20000 bytes (2-page safety margin for 0x40010000)

## Limitations

- Requires xemu running with GDB stub + QMP for capture
- Functions with complex callback chains need downstream delinked objects
  or expanded callback pre-map coverage
- XAPI file I/O functions (e.g., `load_core`) can't be tested without
  emulated file system
- Stack corruption regression in `data_new`/`memory_pool_new` is pre-existing
  and unrelated to snapshot changes
