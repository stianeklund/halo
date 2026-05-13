# Auto-Discovery Workflow

The `tools/analysis/auto_discover.py` script automatically finds undocumented functions
in the binary and adds them to `kb.json` with proper object classification.

## Quick Start

### 1. Preview what would be added (dry run - default)

```bash
python3 tools/analysis/auto_discover.py
```

This shows:
- How many undocumented functions exist
- Which objects they'd be classified into
- Confidence levels (high/medium/low)

### 2. Actually add them to kb.json

```bash
python3 tools/analysis/auto_discover.py --auto-add
```

This modifies `kb.json` in-place. The functions are added with:
- Address-based names: `FUN_00123456`
- Default declaration: `void FUN_00123456(void);`
- Object classification based on address proximity

### 3. Check the results

```bash
# See updated progress
python3 tools/analysis/progress.py

# See frontier analysis
python3 tools/analysis/frontier.py
```

## Integration Options

### Option A: Pre-commit Hook (Interactive)

Copy the provided hook:

```bash
cp tools/hooks/pre-commit-auto-discover .git/hooks/pre-commit
chmod +x .git/hooks/pre-commit
```

Now before each commit, it will:
1. Check for undocumented functions
2. Show a summary
3. Ask if you want to auto-add them
4. Stage `kb.json` if you say yes

### Option B: Manual Batch Updates

Run periodically (e.g., weekly):

```bash
# Add all discovered functions
python3 tools/analysis/auto_discover.py --auto-add

# Commit the update
git add kb.json
git commit -m "auto-discover: add N undocumented functions"
```

### Option C: CI/CD Gate

Add to your CI pipeline to ensure kb.json stays up to date:

```yaml
# .github/workflows/discover.yml
- name: Check for undocumented functions
  run: |
    python3 tools/analysis/auto_discover.py --json > discover.json
    DISCOVERED=$(python3 -c "import json; print(json.load(open('discover.json'))['discovered'])")
    if [ "$DISCOVERED" -gt 0 ]; then
      echo "Error: $DISCOVERED undocumented functions found"
      echo "Run: python3 tools/analysis/auto_discover.py --auto-add"
      exit 1
    fi
```

## Understanding the Output

### Object Classification

Functions are classified into objects using address proximity:

- **High confidence**: Within 0x100 bytes of a known function in that object
- **Medium confidence**: Within 0x1000 bytes of known functions
- **Low confidence**: Far from any known function → placed in `<common>`

Example output:
```
By object:
  vector_math.obj                      45 (high=40, medium=5)
  objects.obj                         176 (high=130, medium=46)
  <common>                           2781 (low=2781)
```

### Filtering

The tool automatically skips:
- MSVC/CRT internals (`_chkstk`, `_ftol2`, etc.)
- XDK/Xbox SDK functions (`D3D*`, `Xapi*`, etc.)
- Standard library functions (`malloc`, `memcpy`, `sprintf`, etc.)
- C++ mangled names
- Compiler-generated SEH functions

## After Auto-Discovery

Once functions are in `kb.json`:

1. **Run frontier analysis** to see what's blocking progress:
   ```bash
   python3 tools/analysis/frontier.py
   ```

2. **Classify `<common>` functions** using name patterns:
   ```bash
   python3 tools/analysis/classify_common.py
   ```

3. **Pick targets** for lifting:
   ```bash
   python3 tools/llm_auto_lift.py select
   ```

## Command Reference

```bash
# Dry run (default)
python3 tools/analysis/auto_discover.py

# Add to kb.json
python3 tools/analysis/auto_discover.py --auto-add

# JSON output for scripting
python3 tools/analysis/auto_discover.py --json

# Quiet mode (for CI)
python3 tools/analysis/auto_discover.py --auto-add --quiet

# Custom paths
python3 tools/analysis/auto_discover.py --cache build/function_sizes.json --kb kb.json
```

## Best Practices

1. **Run after updating function cache**: Whenever you export new function sizes from Ghidra
2. **Commit separately**: Keep auto-discovery commits separate from lifting commits
3. **Review `<common>`**: Periodically run `classify_common.py` to move functions out of `<common>`
4. **Don't overthink**: Adding a function as `FUN_00XXXXXX` is better than not having it documented
