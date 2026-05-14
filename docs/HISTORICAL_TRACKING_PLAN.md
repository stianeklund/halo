# Historical Tracking & Matching Percentages - Implementation Plan

## Current State Summary

### What's Already Implemented (Basic Reporting)
1. **`generate_decomp_report.py`** - Core report generator
   - Reads from `kb.json` + `kb_meta.json`
   - Outputs JSON + HTML dashboard
   - Per-unit progress tracking
   - **MISSING:** `match_percent` is always `None` (line 83 placeholder)

2. **`leaderboard.py`** - Rankings and recommendations
   - Shows closest-to-completion units
   - Best target recommendations
   - Size distribution analysis
   - **MISSING:** No historical trends or velocity metrics

3. **`compare_reports.py`** - Delta analysis between two reports
   - Compares two JSON reports
   - Shows completed/started/improved units
   - GitHub Actions integration
   - **MISSING:** No persistent historical storage

4. **`.github/workflows/progress-report.yml`** - CI/CD automation
   - Runs on push, PR, and daily schedule
   - Deploys to GitHub Pages
   - **MISSING:** No historical data persistence

### Data Sources Available
- `kb.json` (844KB) - Function mappings and metadata
- `kb_meta.json` (201KB) - Symbol status (ported/verified/unknown)
- `objdiff.json` (37KB) - Unit configurations with paths to delinked objects
- `delinked/` directory - Reference object files for comparison
- `build/` directory - Built object files to compare against
- `FunctionSizes` (217KB) - Ghidra function size cache

---

## Phase 1: Historical Tracking System

### Requirements
1. Store daily/weekly snapshots of progress
2. Generate trend data (functions ported over time)
3. Calculate velocity metrics (functions per day/week)
4. Generate graph data for visualization

### Implementation Approach

#### Option A: JSON-based Storage (Recommended for Zero-Cost)
**Storage:** GitHub Pages (static JSON files)
**Pros:**
- Zero infrastructure cost
- Already deployed via existing workflow
- Simple to implement and debug
- Works with static site generators

**Cons:**
- Append-only (no efficient querying)
- File size grows linearly with time
- ~10KB per snapshot × 365 days = ~3.6MB/year (acceptable)

**Schema:**
```json
{
  "history": [
    {
      "timestamp": "2026-05-14T00:00:00Z",
      "commit": "abc123",
      "functions": {"ported": 1492, "total": 7524, "percent": 19.83},
      "bytes": {"ported": 315611, "total": 1795009, "percent": 17.58},
      "units": [
        {"name": "actors", "ported": 45, "total": 67, "percent": 67.2}
      ]
    }
  ],
  "trends": {
    "daily": [
      {"date": "2026-05-14", "functions_gained": 5, "velocity_7d": 3.2}
    ]
  }
}
```

#### Option B: SQLite Database (More Features)
**Storage:** Committed to repo or external
**Pros:**
- Efficient querying
- Can store per-function history
- Easy aggregation

**Cons:**
- Binary file in git (merge conflicts)
- More complex CI/CD

**Not recommended** for this project given the zero-cost requirement.

### Components to Build

1. **`tools/report/history.py`** - Historical data management
   - Append new snapshot to history.json
   - Generate trend data (7d/30d/90d velocity)
   - Calculate ETA to completion
   - Deduplicate (don't add if same commit)

2. **`tools/report/graph_data.py`** - Graph data generation
   - Generate data for line charts (progress over time)
   - Generate data for burn-down charts
   - Output format optimized for Chart.js/D3.js

3. **Enhanced HTML Dashboard**
   - Add historical graphs using Chart.js (CDN)
   - Show velocity metrics
   - Show estimated completion date
   - Per-unit historical tracking

---

## Phase 2: Matching Percentages Integration

### Requirements
1. Integrate actual matching percentages from objdiff
2. Track per-function matching status
3. Distinguish between "ported" (exists in source) vs "matching" (byte-accurate)

### Implementation Approach

#### Step 1: Parse objdiff.json for Unit Mappings
Already have this - `objdiff.json` contains unit configurations with:
- `target_path`: Built object file path
- `base_path`: Delinked reference object path
- `metadata.source_path`: Source file location

#### Step 2: Integrate objdiff CLI
**Tool:** `tools/objdiff-cli-linux-x86_64` (already in repo)

**Command pattern:**
```bash
./tools/objdiff-cli-linux-x86_64 diff \
  --base delinked/actors.obj \
  --target build/CMakeFiles/halo.dir/src/halo/ai/actors.c.obj \
  --format json
```

**Expected output:**
```json
{
  "overall_match": 87.5,
  "sections": [...],
  "functions": [
    {"name": "actor_update", "match": 100.0},
    {"name": "actor_init", "match": 75.3}
  ]
}
```

#### Step 3: Enhance Report Schema
Update `generate_decomp_report.py` to include:
```json
{
  "functions": [
    {
      "address": "0x36c00",
      "name": "actor_update",
      "status": "ported",
      "match_percent": 100.0,  // From objdiff
      "size": 256
    }
  ],
  "summary": {
    "matching": {
      "total": 1492,
      "fully_matching": 890,
      "partially_matching": 602,
      "percent": 59.7
    }
  }
}
```

### Components to Build

1. **`tools/report/matching.py`** - objdiff integration wrapper
   - Run objdiff CLI on each ported unit
   - Parse JSON output
   - Cache results to avoid re-running
   - Handle failures gracefully (report as null)

2. **Enhanced Report Generator**
   - Add `--with-matching` flag
   - Include matching percentages in output
   - Separate "ported" from "matching" metrics

3. **Performance Considerations**
   - objdiff can be slow - need caching
   - Only run on changed units in CI
   - Use `--units` flag to limit scope
   - Parallel execution where possible

---

## Phase 3: Enhanced Dashboard

### New Features

1. **Historical Graphs**
   - Line chart: Functions ported over time
   - Burn-down chart: Remaining functions
   - Velocity chart: Functions per week
   - ETA projection

2. **Matching Status Visualization**
   - Color coding: Green (100%), Yellow (partial), Red (ported but not matching)
   - Per-unit matching breakdown
   - "Most promising to fix" list

3. **Contributor Attribution** (Future)
   - Parse git blame for each function
   - Show top contributors per unit
   - Requires additional tooling

### Technical Implementation

**Chart Library:** Chart.js (loaded from CDN)
**Benefits:**
- No build step required
- Works in static HTML
- Well documented
- Small bundle size

**Dashboard Structure:**
```html
<!DOCTYPE html>
<html>
<head>
  <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
</head>
<body>
  <section id="overview">
    <!-- Current summary cards -->
  </section>
  
  <section id="historical">
    <canvas id="progress-chart"></canvas>
    <canvas id="velocity-chart"></canvas>
  </section>
  
  <section id="matching">
    <table id="matching-table">
      <!-- Per-unit matching percentages -->
    </table>
  </section>
</body>
</html>
```

---

## Implementation Roadmap

### Week 1: Historical Tracking Foundation
- [ ] Create `tools/report/history.py`
- [ ] Design `artifacts/progress/history.json` schema
- [ ] Update workflow to append to history
- [ ] Add velocity calculation
- [ ] Test with 7 days of synthetic data

### Week 2: Dashboard Graphs
- [ ] Create `tools/report/graph_data.py`
- [ ] Generate Chart.js-compatible datasets
- [ ] Update HTML dashboard with Chart.js
- [ ] Add progress line chart
- [ ] Add velocity visualization
- [ ] Add ETA projection

### Week 3: Matching Percentages
- [ ] Create `tools/report/matching.py`
- [ ] Integrate objdiff CLI
- [ ] Implement caching layer
- [ ] Update report schema with matching data
- [ ] Add `--with-matching` flag

### Week 4: Polish & Integration
- [ ] Optimize CI performance (caching, parallelization)
- [ ] Add per-function matching status
- [ ] Enhanced unit breakdown with matching data
- [ ] Documentation updates
- [ ] Stress test with full codebase

---

## Technical Considerations

### Performance
- **History file growth:** ~3.6MB/year - acceptable for GitHub Pages
- **objdiff runtime:** Can be slow for large units
  - Solution: Cache results in `artifacts/progress/matching_cache.json`
  - Only re-run on changed units
  - Use checksums to detect changes

### Storage Limits
- GitHub Pages: 1GB per site (more than enough)
- GitHub repo: No practical limit for text files

### CI/CD Complexity
- Current workflow: ~2 minutes
- With matching: +5-10 minutes (depends on caching)
- Mitigation: 
  - Run full matching only on main branch
  - PRs get summary-only reports
  - Use workflow artifacts for caching

### Backwards Compatibility
- Current report schema is extensible
- New fields are optional additions
- Existing tools will continue to work

---

## Open Questions

1. **Historical retention:** Keep all data forever or prune after N years?
2. **Granularity:** Daily snapshots or only on commits to main?
3. **Matching scope:** All ported functions or only top-level units?
4. **Velocity window:** 7-day average? 30-day? Both?
5. **ETA calculation:** Linear projection or moving average?

---

## Success Metrics

- [ ] Dashboard shows progress line chart with 30+ days of history
- [ ] Matching percentages displayed for all ported units
- [ ] Velocity metric updates daily
- [ ] ETA to completion visible on dashboard
- [ ] CI runtime < 5 minutes with full matching
- [ ] Zero additional infrastructure costs
