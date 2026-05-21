# Self-Hosted GitHub Actions Runner Setup

Step-by-step instructions for configuring a GitHub Actions self-hosted runner on this machine to run Unicorn/Z3 equivalence tests. This document is designed to be followed by a human or an LLM agent with shell access.

## Machine Context

| Property | Value |
|----------|-------|
| Hostname | Hermes |
| OS | Ubuntu 24.04.4 LTS on WSL2 (kernel 6.6.87.2-microsoft-standard-WSL2) |
| Arch | x86_64 (amd64) |
| User | `stian` |
| Home | `/home/stian` |
| Shell | `/bin/bash` (fish available) |
| systemd | 255 (available, needed for the runner service) |
| Repo root | `/mnt/g/dev/halo` |
| Repo remote | `https://github.com/stianeklund/halo.git` (fork of `halo-re/halo`) |
| Python venv | `/mnt/g/dev/halo/.venv/bin/python3` (Python 3.12.3) |

## Prerequisites Already Installed

These are already present in the project venv. Do NOT reinstall them.

| Package | Version | Purpose |
|---------|---------|---------|
| unicorn | 2.1.4 | x86 emulation for differential testing |
| capstone | 5.0.7 | Disassembly for concolic/branch analysis |
| z3-solver | 4.16.0 | Formal equivalence proofs + seed generation |
| lief | 0.17.6 | COFF/PE object parsing |
| pefile | 2023.2.7 | XBE/PE header parsing |
| pyxbe | 1.0.3 | Xbox XBE format |
| libclang | 16.0.6 | C parsing for build tooling |
| PyYAML | 6.0.3 | Config file parsing |

System tools required (already installed):
- `llvm-objdump` at `/usr/bin/llvm-objdump`
- `cmake`, `clang` (for building candidate objects)

## Step 1: Create a GitHub Personal Access Token (or use Fine-Grained Token)

The runner registration requires a token. Two options:

### Option A: Ephemeral registration token (recommended)

Go to the repo settings page in a browser:
```
https://github.com/stianeklund/halo/settings/actions/runners/new?arch=x64&os=linux
```
GitHub will show a registration token on that page. Copy it — it expires in 1 hour.

### Option B: Personal access token

Create a fine-grained PAT at https://github.com/settings/tokens with:
- Repository: `stianeklund/halo`
- Permissions: `Administration: Read and write` (needed for runner registration)

## Step 2: Download and Install the Runner

```bash
# Create runner directory
mkdir -p ~/actions-runner && cd ~/actions-runner

# Download latest runner (check https://github.com/actions/runner/releases for newest)
curl -o actions-runner-linux-x64.tar.gz -L \
  https://github.com/actions/runner/releases/download/v2.323.0/actions-runner-linux-x64.tar.gz

# Extract
tar xzf actions-runner-linux-x64.tar.gz

# Install dependencies (the runner needs these system libs)
sudo ./bin/installdependencies.sh
```

## Step 3: Configure the Runner

```bash
cd ~/actions-runner

# Configure with the registration token from Step 1
./config.sh \
  --url https://github.com/stianeklund/halo \
  --token <REGISTRATION_TOKEN> \
  --name hermes-wsl2 \
  --labels self-hosted,linux,x64,equivalence \
  --work /home/stian/actions-runner/_work \
  --replace
```

When prompted:
- **Runner group**: press Enter (default)
- **Runner name**: `hermes-wsl2` (already set via flag)
- **Additional labels**: press Enter (already set via flag)
- **Work folder**: press Enter (already set via flag)

## Step 4: Configure Environment for the Runner

The runner needs access to the project venv and system tools. Create an environment file:

```bash
cat > ~/actions-runner/.env << 'EOF'
# Ensure llvm-objdump, cmake, clang are on PATH
PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/lib/llvm-18/bin
# The delinked/ directory is a symlink in the repo pointing to /mnt/g/dev/halo/delinked
# The checkout action will clone fresh, so we need delinked refs accessible.
# The workflow uses the repo's .venv — Step 5 ensures it exists in the work dir.
EOF
```

## Step 5: Ensure the Venv Exists for Checkouts

The GHA workflow references `.venv/bin/python3` relative to the checkout directory. The runner checks out a fresh copy, so the venv won't exist there. Two solutions:

### Option A: Pre-job script that symlinks the venv (recommended)

Create a pre-job hook that symlinks the existing venv into the workspace:

```bash
mkdir -p ~/actions-runner/hooks

cat > ~/actions-runner/hooks/pre-job.sh << 'HOOK'
#!/bin/bash
# Symlink the project venv and delinked refs into the runner workspace.
# The runner sets GITHUB_WORKSPACE before calling this hook.
if [ -n "$GITHUB_WORKSPACE" ] && [ -d "$GITHUB_WORKSPACE" ]; then
    # Venv
    if [ ! -e "$GITHUB_WORKSPACE/.venv" ]; then
        ln -sf /mnt/g/dev/halo/.venv "$GITHUB_WORKSPACE/.venv"
    fi
    # Delinked oracle objects
    if [ ! -e "$GITHUB_WORKSPACE/delinked" ]; then
        ln -sf /mnt/g/dev/halo/delinked "$GITHUB_WORKSPACE/delinked"
    fi
fi
HOOK

chmod +x ~/actions-runner/hooks/pre-job.sh
```

Then tell the runner to use this hook:

```bash
# Add to ~/actions-runner/.env:
echo 'ACTIONS_RUNNER_HOOK_JOB_STARTED=/home/stian/actions-runner/hooks/pre-job.sh' >> ~/actions-runner/.env
```

### Option B: Modify the workflow to use absolute paths

Change `VENV_PYTHON` in `.github/workflows/equivalence.yml` to `/mnt/g/dev/halo/.venv/bin/python3`. Simpler but less portable.

## Step 6: Install as a systemd Service

```bash
cd ~/actions-runner

# Install the service (runs as your user, not root)
sudo ./svc.sh install stian

# Start the service
sudo ./svc.sh start

# Verify it's running
sudo ./svc.sh status
```

The service name will be `actions.runner.stianeklund-halo.hermes-wsl2.service`.

### WSL2 systemd note

WSL2 with systemd enabled (Ubuntu 24.04 default) supports this natively. If systemd is not enabled, add to `/etc/wsl.conf`:

```ini
[boot]
systemd=true
```

Then restart WSL: `wsl --shutdown` from PowerShell, then reopen.

### Auto-start on Windows boot (optional)

WSL2 doesn't start automatically. To ensure the runner is always available:

1. Create a Windows Task Scheduler task that runs at login:
   - Action: `wsl -d Ubuntu -u stian -- bash -c "sudo systemctl start actions.runner.stianeklund-halo.hermes-wsl2.service"`
   - Trigger: At log on

2. Or add to PowerShell profile (`$PROFILE`):
   ```powershell
   wsl -d Ubuntu -- sudo systemctl start actions.runner.stianeklund-halo.hermes-wsl2.service 2>$null
   ```

## Step 7: Verify the Runner

### Check GitHub sees the runner

```bash
# Requires gh CLI authenticated
gh api repos/stianeklund/halo/actions/runners --jq '.runners[] | {name, status, labels: [.labels[].name]}'
```

Expected output:
```json
{
  "name": "hermes-wsl2",
  "status": "online",
  "labels": ["self-hosted", "linux", "x64", "equivalence"]
}
```

### Test with a manual workflow dispatch

```bash
gh workflow run equivalence.yml --field mode=regression
```

Then watch:
```bash
gh run list --workflow=equivalence.yml --limit 1
# Wait for it, then:
gh run view <run-id> --log
```

### Run the regression test locally to confirm baseline

```bash
cd /mnt/g/dev/halo
.venv/bin/python3 tools/equivalence/regression_test.py --quick
```

Expected: 3 targets, all PASS, ~10 seconds.

## Step 8: Seed the Baseline

Before the nightly job can detect regressions, create a baseline:

```bash
cd /mnt/g/dev/halo

# Run a full batch verify with discovery mode
.venv/bin/python3 tools/equivalence/batch_verify.py \
  --discover --csv --seeds 50 --timeout 120 \
  --output-dir artifacts/batch_verify

# Inspect results
cat artifacts/batch_verify/summary.json | python3 -m json.tool | head -20
```

This `summary.json` becomes the `--baseline` for future nightly runs. The workflow automatically picks it up if present in `artifacts/batch_verify/`.

## Step 9: Grow the Regression Suite

After seeding the baseline, auto-populate the fast regression targets:

```bash
cd /mnt/g/dev/halo

# Dry run — see what would be added
.venv/bin/python3 tools/equivalence/populate_regression_targets.py --from-batch

# Apply
.venv/bin/python3 tools/equivalence/populate_regression_targets.py --from-batch --apply
```

This adds all high/moderate-confidence functions from the leaf cache and batch results to `regression_targets.json`, which is used by the pre-commit hook and the quick CI regression job.

## Troubleshooting

### Runner shows "offline" in GitHub

```bash
# Check service status
sudo systemctl status actions.runner.stianeklund-halo.hermes-wsl2.service

# Check logs
journalctl -u actions.runner.stianeklund-halo.hermes-wsl2.service -n 50

# Restart
sudo ./svc.sh stop && sudo ./svc.sh start
```

### "No module named unicorn" in CI

The pre-job hook didn't symlink the venv. Check:
```bash
ls -la ~/actions-runner/_work/halo/halo/.venv
```
Should be a symlink to `/mnt/g/dev/halo/.venv`. If missing, verify the hook path in `~/actions-runner/.env`.

### Delinked refs not found

The `delinked/` directory must be accessible in the checkout. The pre-job hook symlinks it. Verify:
```bash
ls ~/actions-runner/_work/halo/halo/delinked/ | head -5
```

### Build fails (missing clang/cmake)

```bash
which clang cmake llvm-objdump
# If missing:
sudo apt install clang cmake llvm
```

### Nightly job doesn't trigger

GitHub disables scheduled workflows after 60 days of repo inactivity. Push any commit to keep it active. Alternatively, use `workflow_dispatch` to trigger manually.

## File Reference

| File | Purpose |
|------|---------|
| `.github/workflows/equivalence.yml` | GHA workflow (regression on PR, batch nightly) |
| `tools/equivalence/regression_test.py` | Fast pre-commit/CI regression runner (3+ targets, ~10s) |
| `tools/equivalence/regression_targets.json` | Curated high-risk targets for regression testing |
| `tools/equivalence/batch_verify.py` | Full batch equivalence verifier (`--discover` for all ported) |
| `tools/equivalence/populate_regression_targets.py` | Auto-populate regression targets from results |
| `tools/equivalence/unicorn_diff.py` | Core Unicorn differential tester |
| `tools/equivalence/leaf_cache.json` | Cache of tested functions with confidence tiers |
| `artifacts/batch_verify/summary.json` | Batch run results (used as regression baseline) |
