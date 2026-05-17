# Decomp.dev-Style Progress Report Implementation

## What Was Built

### 1. Report Generator Tool (`tools/report/generate_decomp_report.py`)

A fully functional tool that generates decomp.dev-compatible progress reports:

**Features:**
- ✅ Reads from existing kb.json and kb_meta.json
- ✅ Computes per-unit statistics (functions, bytes, percentages)
- ✅ Generates JSON output compatible with decomp.dev format
- ✅ Generates standalone HTML dashboard with sorting and filtering
- ✅ Includes git metadata (commit hash, branch, timestamp)

**Usage:**
```bash
# Generate JSON report only
python3 tools/report/generate_decomp_report.py -o report.json

# Generate both JSON and HTML dashboard
python3 tools/report/generate_decomp_report.py --html dashboard.html

# Include per-function details (larger file)
python3 tools/report/generate_decomp_report.py --with-functions
```

**Output Format:**
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
  "meta": {
    "timestamp": "2026-05-14T20:10:53Z",
    "commit": "378fdd27",
    "branch": "main"
  },
  "units": [...]
}
```

### 2. GitHub Actions Workflow (`.github/workflows/progress-report.yml`)

Automated CI pipeline that:

- **On every push to main:**
  - Generates progress report
  - Deploys to GitHub Pages (`gh-pages` branch)
  - Updates progress badge

- **On pull requests:**
  - Compares progress before/after PR
  - Posts comment with progress change summary
  - Shows delta in functions and bytes

- **Daily cron job:**
  - Keeps dashboard updated even without pushes

### 3. HTML Dashboard

A standalone, self-contained HTML file with:
- Dark theme (GitHub-style)
- Progress bars for visual representation
- Sortable table (click headers to sort)
- Per-unit breakdown with function counts and byte coverage
- Responsive design

---

## Current Progress Snapshot

| Metric | Value |
|--------|-------|
| **Functions Ported** | 1,492 / 7,524 (19.83%) |
| **Bytes Ported** | 315,611 / 1,795,009 (17.58%) |
| **Translation Units** | 202 |
| **Complete Units (100%)** | ~15 |

---

## Next Steps to Full decomp.dev Integration

### Phase 1: Deploy Static Dashboard (1-2 hours)

1. **Enable GitHub Pages** on the repository:
   - Go to Settings → Pages
   - Source: Deploy from a branch
   - Branch: `gh-pages` / folder: `/progress`
   - Custom domain: `blam.info` (optional)

2. **Test the workflow:**
   ```bash
   # Trigger manually via GitHub Actions UI
   # Or push to main to trigger automatically
   ```

3. **Update README with badge:**
   ```markdown
   ![Progress](https://img.shields.io/badge/progress-19.8%25-blue)
   [View Dashboard](https://yourusername.github.io/halo/progress/)
   ```

### Phase 2: Add Matching Percentages (1-2 days)

To get per-function matching percentages (like decomp.dev shows):

1. **Batch run objdiff on all units:**
   ```python
   # New tool: tools/report/objdiff_batch.py
   for unit in objdiff_config['units']:
       run_objdiff(unit['base_path'], unit['target_path'])
       extract_matching_percentage()
   ```

2. **Store results:**
   - Option A: Extend report JSON with `match_percent` field
   - Option B: Separate `matching_scores.json` file

3. **Update HTML dashboard** to show:
   - Matching % column in table
   - Color coding (green for 100%, yellow for 80-99%, red for <80%)
   - Filter by match quality

### Phase 3: Historical Tracking (2-3 days)

To show progress graphs over time:

1. **Create database schema** (SQLite or PostgreSQL):
   ```sql
   CREATE TABLE progress_snapshots (
       id INTEGER PRIMARY KEY,
       timestamp TEXT,
       commit_hash TEXT,
       functions_total INTEGER,
       functions_ported INTEGER,
       bytes_total INTEGER,
       bytes_ported INTEGER
   );
   ```

2. **Ingest script** to save each CI run:
   ```python
   # tools/report/ingest_snapshot.py
   def save_snapshot(report_json):
       insert_into_db(report_json)
   ```

3. **API endpoint** to query history:
   ```python
   # Simple Flask/FastAPI server
   @app.get('/api/history')
   def get_history(days: int = 30):
       return query_snapshots_last_n_days(days)
   ```

4. **Frontend charts** (Chart.js or similar):
   - Line graph: Progress % over time
   - Bar chart: Functions added per week
   - Contributor activity (if tracking who ported what)

### Phase 4: Full decomp.dev Deployment (1-2 weeks)

If you want the full decomp.dev experience:

1. **Deploy decomp.dev backend:**
   - Clone https://github.com/encounter/decomp.dev
   - Set up PostgreSQL database
   - Configure GitHub App for PR comments

2. **Create project configuration:**
   ```yaml
   # config/projects/halo-ce-xbox.yml
   name: "Halo: Combat Evolved (Xbox)"
   version: "01.10.12.2276"
   repo: https://github.com/you/repo
   objdiff_config: objdiff.json
   ```

3. **Adapt report generator** to post to decomp.dev API:
   ```python
   requests.post(
       'https://decomp.dev/api/reports',
       headers={'Authorization': 'Bearer TOKEN'},
       json=report_data
   )
   ```

---

## What Makes This Approach Work

**Leverages existing infrastructure:**
- Uses kb.json (already has 99.7% function coverage)
- Uses kb_meta.json (already tracks ported status)
- Uses objdiff.json (already configured for 150+ units)
- Uses delinked/ directory (already has reference objects)

**No new data sources needed** - just new ways to visualize and expose the data we already collect.

**Incremental adoption:**
- Phase 1 gives immediate value (live dashboard)
- Each phase adds more features
- Can stop at any point based on needs

---

## Files Created

| File | Purpose |
|------|---------|
| `tools/report/generate_decomp_report.py` | Main report generator |
| `.github/workflows/progress-report.yml` | CI automation |
| `docs/decomp-dev-evaluation.md` | Requirements analysis |
| `artifacts/progress/report.json` | Sample JSON output |
| `artifacts/progress/index.html` | Sample HTML dashboard |

---

## Running Locally

```bash
# Generate report
cd /mnt/g/dev/halo
python3 tools/report/generate_decomp_report.py --html artifacts/progress/index.html

# View dashboard
python3 -m http.server 8080 --directory artifacts/progress
# Open http://localhost:8080/index.html
```

---

## Integration with blam.info

Since the project already has `blam.info` as its homepage, the easiest integration is:

1. **GitHub Pages deployment** pointed at `blam.info/progress/`
2. **Embed iframe** on blam.info homepage showing the dashboard
3. **JSON endpoint** at `blam.info/progress/report.json` for other tools to consume
