# Decomp.dev-Style Progress Reporting

## Current Status

```
Halo: Combat Evolved (Xbox) - Decompilation Progress
====================================================
Functions: 1,492 / 7,524 (19.83%)
Bytes:     315,611 / 1,795,009 (17.58%)
Units:     202
Complete:  10 units (100%)
```

**Status:** Phase 1 implemented and verified. Phase 2/3 planned but not started.

---

## Phase 1: Implemented

### Core Tools (`tools/report/`)

| Tool | Lines | Purpose |
|------|-------|---------|
| `generate_decomp_report.py` | 477 | Main report generator (JSON + HTML) |
| `leaderboard.py` | 328 | Rankings and statistics |
| `compare_reports.py` | 416 | Compare progress between commits |
| `verify.py` | 147 | Verification/health check |
| `setup.sh` | 71 | One-time setup script |
| `README.md` | 328 | Documentation |

### CI/CD Pipeline

- `.github/workflows/progress-report.yml` — triggers on push to main, PRs, daily cron, manual dispatch
- Deploys to GitHub Pages (`gh-pages` branch)

### Commands

```bash
python3 tools/report/generate_decomp_report.py --html artifacts/progress/index.html
python3 tools/report/leaderboard.py
python3 tools/report/compare_reports.py --git --base-ref HEAD~10 --head-ref HEAD
python3 tools/report/verify.py
python3 -m http.server 8080 --directory artifacts/progress
```

### Output Format

```json
{
  "project": {
    "name": "halo-ce-xbox",
    "display_name": "Halo: Combat Evolved (Xbox)",
    "version": "01.10.12.2276",
    "total_units": 202,
    "total_functions": 7524
  },
  "summary": {
    "functions": { "total": 7524, "ported": 1492, "percent": 19.83 },
    "bytes": { "total": 1795009, "ported": 315611, "percent": 17.58 }
  },
  "meta": { "timestamp": "...", "commit": "...", "branch": "main" },
  "units": [...]
}
```

### Data Sources

- `kb.json` — function mappings and metadata
- `kb_meta.json` — symbol status (ported/verified/unknown)
- `objdiff.json` — unit configurations with paths to delinked objects
- `delinked/` — reference object files for comparison
- `build/` — built candidate object files

---

## Phase 2: Matching Percentages (Planned)

Add per-function byte-accuracy tracking via objdiff integration.

### Components

1. **`tools/report/matching.py`** — objdiff integration wrapper
   - Run objdiff CLI on each ported unit
   - Parse JSON output, cache results
   - Handle failures gracefully

2. **Enhanced Report Schema** — add `match_percent` field (currently always `None`)
3. **Dashboard updates** — color coding (green=100%, yellow=partial, red=ported-not-matching)

```bash
./tools/objdiff-cli-linux-x86_64 diff \
  --base delinked/actors.obj \
  --target build/CMakeFiles/halo.dir/src/halo/ai/actors.c.obj \
  --format json
```

### Performance

- objdiff is slow — needs caching, only re-run on changed units
- Use checksums to detect changes

---

## Phase 3: Historical Tracking (Planned)

Store daily/weekly snapshots for trend graphs and velocity metrics.

### Approach: JSON-based (zero infrastructure)

- Append-only `history.json` on GitHub Pages (~3.6MB/year)
- Velocity metrics: functions/day, functions/week, ETA to completion

### Components

1. **`tools/report/history.py`** — historical data management
2. **`tools/report/graph_data.py`** — Chart.js-compatible datasets
3. **Enhanced Dashboard** — line charts, burn-down charts, velocity charts (Chart.js from CDN)

### Open Questions

- Daily snapshots or only on main-branch commits?
- Velocity window: 7-day or 30-day?
- ETA: linear projection or moving average?

---

## Feature Comparison

| Feature | Status |
|---------|--------|
| Per-unit progress tracking | Done |
| Function-level counting | Done |
| Byte coverage metrics | Done |
| HTML dashboard | Done |
| JSON API | Done |
| GitHub Actions CI | Done |
| PR comments | Done |
| Historical tracking | Phase 3 (planned) |
| Matching percentages | Phase 2 (planned) |
| Time-series graphs | Phase 3 (planned) |
