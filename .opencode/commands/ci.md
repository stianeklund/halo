---
description: Check GitHub Actions CI status, diagnose failures, and run checks locally
---

Diagnose GitHub Actions failures and run CI checks locally.

Argument: $ARGUMENTS (optional run ID, workflow name, or "local" to run checks without GitHub)

## Workflows

| Workflow | Runner | Triggers |
|---|---|---|
| `main.yml` | ubuntu-latest / macos-latest | every push/PR |
| `equivalence.yml` | self-hosted (`hermes-wsl2`) | push to main, nightly, manual |
| `progress-report.yml` | ubuntu-latest | push to main, nightly |

## Step 1 — Quick status

```bash
gh run list --limit 10 --json databaseId,conclusion,workflowName,displayTitle,updatedAt \
  | python3 -c "
import json,sys
for r in json.load(sys.stdin):
    icon = '✅' if r['conclusion']=='success' else '❌' if r['conclusion']=='failure' else '⏳'
    print(f\"{icon} [{r['databaseId']}] {r['workflowName']}: {r['displayTitle'][:60]}\")
"
```

## Step 2 — Which jobs failed

```bash
gh run view <RUN_ID> --json jobs \
  | python3 -c "
import json,sys
for j in json.load(sys.stdin)['jobs']:
    if j['conclusion'] not in ('success','skipped'):
        failed = [s['name'] for s in j['steps'] if s.get('conclusion') not in ('success','skipped',None)]
        print(j['name'], j['conclusion'], failed)
"
```

## Step 3 — Get the actual error

```bash
# Download and extract error lines from logs
gh api "repos/$(gh repo view --json nameWithOwner -q .nameWithOwner)/actions/runs/<RUN_ID>/logs" \
  > /tmp/run_logs.zip \
  && unzip -p /tmp/run_logs.zip "*.txt" \
  | grep -E "##\[error\]|error:|Error:|FAILED|ModuleNotFoundError|make\[\d\].*Error" \
  | head -40
```

For self-hosted equivalence.yml failures, also check local runner logs:
```bash
ls -lt ~/actions-runner/_work/_diag/ | head -3
# then: grep -i "error\|fail" ~/actions-runner/_work/_diag/Runner_<timestamp>.log | tail -30
```

## Run checks locally (no push needed)

```bash
# AgentPolicy — AGENTS.md/CLAUDE.md sync + line count
python3 tools/audit/check_agent_docs.py

# Regression tests — quick (5 seeds, ~40s)
.venv/bin/python3 tools/equivalence/regression_test.py --quick

# Regression tests — single target
.venv/bin/python3 tools/equivalence/regression_test.py --target <name_or_addr>

# Regression tests — list targets without running
.venv/bin/python3 tools/equivalence/regression_test.py --dry-run

# Progress regression gate — compare HEAD vs HEAD~1
python3 tools/report/compare_reports.py --git --base-ref HEAD~1 --head-ref HEAD

# Validate all workflow YAML files
python3 -c "
import yaml
for f in ['.github/workflows/main.yml','.github/workflows/progress-report.yml','.github/workflows/equivalence.yml']:
    yaml.safe_load(open(f))
    print('OK:', f)
"

# Requirements check (pkg_resources replacement)
python3 tools/audit/check_requirements.py
```

## Common failures and fixes

| Symptom | Cause | Fix |
|---|---|---|
| `AGENTS.md and CLAUDE.md are out of sync` | Files diverged | `cp CLAUDE.md AGENTS.md` |
| `has N lines (limit: 250)` | File too long | Trim or raise `MAX_LINES` in `check_agent_docs.py` |
| `ModuleNotFoundError: No module named 'pkg_resources'` | Old `check_requirements.py` on checkout | `check_requirements.py` now uses `importlib.metadata`; rebuild/reinstall |
| `'FUN_XXXXXXXX' not found in kb.json` | Function renamed since batch_verify ran | `populate_regression_targets.py` bug fixed; re-run `--from-batch --apply` |
| YAML parse error in workflow | Python/heredoc content at col 0 terminates block scalar | Ensure all run-block content is indented to match the block level |
| `jobs: []` from API | Workflow YAML parse error — no jobs ever started | Validate with `python3 -c "import yaml; yaml.safe_load(open('file.yml'))"` |

## Expand regression coverage

```bash
# See what batch_verify results could be added
.venv/bin/python3 tools/equivalence/populate_regression_targets.py --from-batch

# Apply (writes regression_targets.json)
.venv/bin/python3 tools/equivalence/populate_regression_targets.py --from-batch --apply

# Verify no new errors before committing
.venv/bin/python3 tools/equivalence/regression_test.py --quick
```

`populate_regression_targets.py` sources:
- `--from-batch`: functions that passed 20+ seeds in `artifacts/batch_verify/`
- default (no flag): functions with `high`/`moderate` confidence in `leaf_cache.json` (≥50% coverage)

## Self-hosted runner (`hermes-wsl2`)

```bash
# Check runner service status
systemctl is-active actions.runner.stianeklund-halo.hermes-wsl2.service

# Runner config
cat ~/actions-runner/.runner

# Restart if needed
sudo systemctl restart actions.runner.stianeklund-halo.hermes-wsl2.service
```

The runner uses `.venv/bin/python3` and symlinks `delinked/` and `build/` from the local machine at:
- `VENV_PYTHON`: `/mnt/g/dev/halo/.venv/bin/python3`
- `DELINKED_DIR`: `/mnt/g/dev/halo/delinked`
- `BUILD_DIR`: `/mnt/g/dev/halo/build`
