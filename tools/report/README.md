# Decompilation Progress Reporting Tools

A suite of tools for tracking and visualizing Halo CE Xbox decompilation progress in a decomp.dev-compatible format.

## Quick Start

```bash
# Generate a progress report with historical charts
python3 tools/report/generate_decomp_report.py --html artifacts/progress/index.html

# View the leaderboard
python3 tools/report/leaderboard.py

# Manage historical data
python3 tools/report/history.py --add-snapshot artifacts/progress/report.json
python3 tools/report/history.py --show-summary --show-velocity --show-eta

# Check binary matching (requires built objects)
python3 tools/report/matching.py --unit actors
python3 tools/report/matching.py --all-ported artifacts/progress/report.json

# Compare two commits
python3 tools/report/compare_reports.py --git --base-ref HEAD~10 --head-ref HEAD
```

---

## Tools Overview

### 1. `generate_decomp_report.py` - Main Report Generator

Generates decomp.dev-compatible JSON and an enhanced HTML dashboard with historical charts.

**Usage:**
```bash
# Generate JSON report
python3 tools/report/generate_decomp_report.py -o report.json

# Generate both JSON and HTML with historical charts
python3 tools/report/generate_decomp_report.py --html dashboard.html

# Include per-function details (larger file)
python3 tools/report/generate_decomp_report.py --with-functions

# Specify custom history file for charts
python3 tools/report/generate_decomp_report.py --html dashboard.html --history path/to/history.json
```

**Output:**
- `report.json` - Machine-readable progress data
- `index.html` - Standalone interactive dashboard with Chart.js graphs

**JSON Format:**
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
  "matching_summary": {
    "total_units_checked": 45,
    "avg_match_percent": 87.5,
    "fully_matching": 23
  },
  "units": [...]
}
```

**HTML Dashboard Features:**
- 📊 **Historical Progress Chart** - Line graph showing functions ported over time (requires history.json)
- 📈 **Velocity Chart** - Bar chart showing daily functions completed
- 🎯 **ETA Calculation** - Estimated completion date based on recent velocity
- 📋 **Sortable Unit Table** - Click column headers to sort
- 🎨 **Dark Theme** - Easy on the eyes
- 📱 **Responsive Design** - Works on mobile devices

---

### 2. `history.py` - Historical Progress Tracking

Manages a JSON-based history database for tracking progress over time, calculating velocity, and estimating completion dates.

**Usage:**
```bash
# Add current report to history
python3 tools/report/history.py --add-snapshot artifacts/progress/report.json

# Show historical summary
python3 tools/report/history.py --show-summary

# Show velocity metrics
python3 tools/report/history.py --show-velocity

# Show ETA to completion
python3 tools/report/history.py --show-eta

# Output graph data for Chart.js
python3 tools/report/history.py --show-graph-data

# Prune old snapshots (keep last 90 days)
python3 tools/report/history.py --prune 90
```

**Features:**
- **Automatic deduplication** - Won't add duplicate snapshots for same commit within 1 hour
- **Velocity calculation** - Functions per day over 7-day and 30-day windows
- **ETA projection** - Estimated completion date with confidence level
- **Graph data export** - Ready-to-use data for Chart.js visualization

**Example Output:**
```
=== Historical Progress Summary ===
Total snapshots: 45
First snapshot: 2026-04-01T00:00:00Z
Latest snapshot: 2026-05-14T00:00:00Z

Progress since start:
  Functions: +342
  Bytes: +72,341

=== Velocity Metrics ===
Last 7 days: 4.50 funcs/day (7 samples)
Last 30 days: 3.20 funcs/day (30 samples)

=== ETA to Completion ===
Estimated completion: 2027-08-15
Days remaining: 458
Confidence: medium
Velocity: 3.20 funcs/day
Remaining: 6,032 functions
```

**History File Format:**
```json
{
  "version": "1.0.0",
  "snapshots": [
    {
      "timestamp": "2026-05-14T00:00:00Z",
      "commit": "abc123",
      "summary": { "functions": {...}, "bytes": {...} },
      "units": [...]
    }
  ],
  "trends": {
    "daily": [...],
    "weekly": [...]
  },
  "metadata": {
    "created": "2026-04-01T00:00:00Z",
    "last_updated": "2026-05-14T00:00:00Z"
  }
}
```

---

### 3. `matching.py` - Binary Matching Tracker

Integrates with objdiff CLI to track actual binary matching percentages, distinguishing between "ported" (exists in source) and "matching" (byte-accurate).

**Prerequisites:**
- Built object files in `build/` directory
- Delinked reference objects in `delinked/` directory
- objdiff CLI at `tools/objdiff-cli-linux-x86_64`

**Usage:**
```bash
# Check specific unit
python3 tools/report/matching.py --unit actors

# Check all ported units from report
python3 tools/report/matching.py --all-ported artifacts/progress/report.json

# Force re-check (ignore cache)
python3 tools/report/matching.py --unit actors --force

# Enhance existing report with matching data
python3 tools/report/matching.py --enhance-report artifacts/progress/report.json

# Show matching summary
python3 tools/report/matching.py --show-summary

# Clear the cache
python3 tools/report/matching.py --clear-cache
```

**Features:**
- **Caching** - Results cached for 24 hours to avoid slow re-checks
- **Incremental updates** - Only checks units that have changed
- **Per-function tracking** - Tracks individual function match percentages
- **Graceful degradation** - Reports null if objdiff fails

**Example Output:**
```
=== Matching Summary ===
Units checked: 45
Average match: 87.5%
Fully matching (100%): 23
Partially matching: 18
Not matching: 4

Checking 45 ported units...
[1/45] Checking actors... 92.3%
[2/45] Checking ai... 88.7%
...

✓ Checked 45 units (0 failed)
```

**Cache Format:**
```json
{
  "version": "1.0.0",
  "last_updated": "2026-05-14T00:00:00Z",
  "units": {
    "actors": {
      "name": "actors",
      "match_percent": 92.3,
      "timestamp": "2026-05-14T00:00:00Z",
      "functions": [...]
    }
  },
  "functions": {
    "actors::actor_update": {
      "unit": "actors",
      "name": "actor_update",
      "match_percent": 100.0
    }
  }
}
```

---

### 4. `leaderboard.py` - Progress Leaderboard

Shows various rankings and statistics about translation units.

**Usage:**
```bash
# Show all sections
python3 tools/report/leaderboard.py

# Show specific sections
python3 tools/report/leaderboard.py --closest 20      # Closest to completion
python3 tools/report/leaderboard.py --targets 20      # Best targets
python3 tools/report/leaderboard.py --largest 10      # Largest remaining
python3 tools/report/leaderboard.py --quick-wins 15   # Small units to complete
python3 tools/report/leaderboard.py --category        # Breakdown by category
python3 tools/report/leaderboard.py --sizes           # Size distribution
```

**Example Output:**
```
🏆 HALO CE XBOX - DECOMPILATION LEADERBOARD

================================================================================
TOP 10 UNITS CLOSEST TO COMPLETION
================================================================================
Unit Name                                   Funcs   Ported   Remain        %
--------------------------------------------------------------------------------
console                                        12       11        1    91.7%
game                                           36       30        6    83.3%
player_rumble                                  12        9        3    75.0%
...
```

---

### 5. `compare_reports.py` - Progress Comparison

Compares two progress reports to show changes/delta.

**Usage:**
```bash
# Compare two JSON files
python3 tools/report/compare_reports.py old_report.json new_report.json

# Compare two git commits
python3 tools/report/compare_reports.py --git --base-ref HEAD~10 --head-ref HEAD

# Output as JSON
python3 tools/report/compare_reports.py old.json new.json --json

# Write GitHub Actions summary
python3 tools/report/compare_reports.py old.json new.json --github-summary
```

**Example Output:**
```
============================================================
PROGRESS COMPARISON REPORT
============================================================

From: abc123 (2026-05-14T00:00:00Z)
To:   def456 (2026-05-15T00:00:00Z)

------------------------------------------------------------
OVERALL PROGRESS
------------------------------------------------------------
  Functions: 1,450 → 1,492 (+42 / +0.56%)
  Bytes:     298,000 → 315,611 (+17,611 / +0.98%)

------------------------------------------------------------
TRANSLATION UNIT CHANGES
------------------------------------------------------------

✅ Completed (3 units):
   • game_time                                 (20 functions)
   • debug_keys                                (5 functions)
   • console                                   (12 functions)

📈 Improved (15 units):
   • actors                                    45→47 (+2) [43.2%→45.7%]
   ...
```

---

### 6. `verify.py` - Health Check

Validates that all report tools are properly configured.

**Usage:**
```bash
python3 tools/report/verify.py
```

Checks:
- All required files exist (kb.json, kb_meta.json)
- Required tools are present
- objdiff CLI availability
- History file integrity
- Git repository status

---

## CI/CD Integration

### GitHub Actions

The included workflow (`.github/workflows/progress-report.yml`) automatically:

1. **On every push to main:**
   - Generates progress report
   - Updates historical data
   - Generates historical charts
   - Calculates velocity and ETA
   - Deploys to GitHub Pages
   - Updates badge data

2. **On pull requests:**
   - Compares progress before/after PR
   - Posts comment with delta summary
   - Shows velocity information
   - Shows detailed changes

3. **Daily (cron):**
   - Keeps dashboard fresh
   - Maintains historical continuity

### Setting up GitHub Pages

1. Go to **Settings → Pages** in your repository
2. Source: **Deploy from a branch**
3. Branch: `gh-pages` / folder: `/progress`
4. Custom domain: `blam.info` (optional)

Your dashboard will be available at:
- `https://yourusername.github.io/halo/progress/`
- Or your custom domain: `https://blam.info/progress/`

### Manual Trigger

```bash
# Run workflow manually via GitHub CLI
gh workflow run progress-report.yml
```

---

## Data Sources

These tools leverage existing project infrastructure:

| Source | Data | Purpose |
|--------|------|---------|
| `kb.json` | Function addresses, names, declarations | Function discovery |
| `kb_meta.json` | Ported status per function | Progress tracking |
| `build/function_sizes.json` | Function byte sizes | Size calculations |
| `objdiff.json` | Unit configurations | Matching percentages |
| `delinked/*.obj` | Reference objects | Binary comparison |
| `build/*.obj` | Built objects | Binary comparison |

---

## Current Progress Snapshot

**As of May 2026:**

| Metric | Value |
|--------|-------|
| Functions Ported | 1,492 / 7,524 (19.83%) |
| Bytes Ported | 315,611 / 1,795,009 (17.58%) |
| Translation Units | 202 |
| Units Complete (100%) | 10 |
| Units In Progress | ~40 |

### Top 5 Closest to Completion

1. `console` - 91.7% (11/12 functions)
2. `game` - 83.3% (30/36 functions)
3. `player_rumble` - 75.0% (9/12 functions)
4. `physical_memory_map` - 75.0% (3/4 functions)
5. `particles` - 73.7% (14/19 functions)

### Top 5 by Remaining Size (Impact Potential)

1. `<common>` - 839.6 KB remaining
2. `units` - 26.8 KB remaining
3. `objects` - 20.0 KB remaining
4. `encounters` - 17.0 KB remaining
5. `actor_perception` - 16.8 KB remaining

---

## Roadmap to decomp.dev Parity

### ✅ Phase 1: Basic Reporting (COMPLETE)
- [x] JSON report generation
- [x] HTML dashboard
- [x] Per-unit breakdown
- [x] GitHub Actions automation

### ✅ Phase 2: Historical Tracking (COMPLETE)
- [x] Snapshot database (history.json)
- [x] Velocity calculation (functions/day)
- [x] ETA projection with confidence
- [x] Chart.js integration for graphs
- [x] Progress over time visualization

### ✅ Phase 3: Matching Integration (COMPLETE)
- [x] objdiff CLI integration
- [x] Per-unit match percentages
- [x] Caching layer for performance
- [x] Graceful error handling

### 🔄 Phase 4: Enhanced Dashboard
- [ ] Per-function matching display
- [ ] Contributor attribution
- [ ] Issue tracking integration
- [ ] Achievement badges

### 📊 Phase 5: Advanced Analytics
- [ ] Burndown charts
- [ ] Trend analysis
- [ ] Seasonality detection
- [ ] Monte Carlo ETA simulation

### 🌐 Phase 6: Full decomp.dev Integration
- [ ] decomp.dev API compatibility
- [ ] Webhook integration
- [ ] Real-time updates
- [ ] Mobile app support

---

## Troubleshooting

### Report generation fails

```bash
# Ensure kb.json and kb_meta.json exist
ls -la kb.json kb_meta.json

# Ensure function size cache exists
ls -la build/function_sizes.json

# If missing, generate from Ghidra:
# Window → Script Manager → ExportFunctionSizes.java
```

### History tracking issues

```bash
# Check history file integrity
python3 -c "import json; json.load(open('artifacts/progress/history.json'))"

# View history summary
python3 tools/report/history.py --show-summary

# Reset history (WARNING: loses all history)
rm artifacts/progress/history.json
```

### Matching checks fail

```bash
# Verify objdiff CLI exists
ls -la tools/objdiff-cli-linux-x86_64

# Check that objects are built
ls -la build/CMakeFiles/halo.dir/src/halo/

# Check that delinked references exist
ls -la delinked/*.obj | head -5

# Clear matching cache and retry
python3 tools/report/matching.py --clear-cache
```

### Import errors

```bash
# Ensure running from repo root
cd /path/to/halo/repo

# Check Python path
python3 -c "import sys; print(sys.path)"

# Install dependencies
pip install clang
```

### Empty or wrong data

- Check that `kb_meta.json` is up to date with ported functions
- Verify `kb.json` has function declarations
- Ensure git repo has commits for metadata extraction
- For matching: ensure both reference and built objects exist

---

## File Structure

```
tools/report/
├── generate_decomp_report.py   # Main report generator
├── history.py                  # Historical tracking
├── matching.py                 # Binary matching tracker
├── compare_reports.py          # Compare two reports
├── leaderboard.py              # Rankings and stats
├── verify.py                   # Health check
└── README.md                   # This file

artifacts/progress/             # Generated output
├── report.json                 # Machine-readable data
├── history.json                # Historical snapshots
├── matching_cache.json         # Matching percentages
├── index.html                  # Dashboard with charts
└── badge.json                  # Badge data

.github/workflows/
└── progress-report.yml         # CI automation with history
```

---

## Performance Considerations

### History File Size
- ~10KB per snapshot
- 365 days ≈ 3.6MB
- GitHub Pages limit: 1GB (more than enough)

### Matching Check Performance
- objdiff can be slow (~1-5 seconds per unit)
- Use caching to avoid re-checks
- Only run full matching on main branch
- PRs get summary-only reports

### Dashboard Load Time
- Chart.js loaded from CDN (~100KB)
- History data: ~3.6MB max (cached by browser)
- First load: ~4 seconds
- Subsequent loads: <1 second (cached)

---

## Contributing

To extend these tools:

1. Add new metrics to `generate_decomp_report.py`
2. Create new analysis in `history.py` or `matching.py`
3. Add visualizations to the HTML template
4. Extend comparison logic in `compare_reports.py`
5. Update this README with new features

---

## License

These tools are part of the Halo CE Xbox reimplementation project and follow the same license terms.
