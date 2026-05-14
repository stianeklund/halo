#!/bin/bash
# Setup script for decomp.dev-style progress reporting
# Run this after the tools have been created to initialize everything

set -e

echo "=========================================="
echo "Halo CE Progress Report Setup"
echo "=========================================="
echo ""

# Check we're in the right directory
if [ ! -f "kb.json" ]; then
    echo "Error: Must run from repo root (kb.json not found)"
    exit 1
fi

# Create output directory
echo "Creating artifacts directory..."
mkdir -p artifacts/progress

# Check dependencies
echo "Checking dependencies..."
python3 -c "import clang" 2>/dev/null || {
    echo "Installing clang Python bindings..."
    pip3 install clang
}

# Generate initial report
echo ""
echo "Generating initial progress report..."
python3 tools/report/generate_decomp_report.py \
    --output artifacts/progress/report.json \
    --html artifacts/progress/index.html

echo ""
echo "✓ Report generated successfully!"
echo ""

# Show summary
python3 -c "
import json
with open('artifacts/progress/report.json') as f:
    data = json.load(f)
    s = data['summary']
    print('Current Progress:')
    print(f\"  Functions: {s['functions']['ported']:,} / {s['functions']['total']:,} ({s['functions']['percent']:.2f}%)\")
    print(f\"  Bytes:     {s['bytes']['ported']:,} / {s['bytes']['total']:,} ({s['bytes']['percent']:.2f}%)\")
    print(f\"  Units:     {len(data['units'])}\")
"

echo ""
echo "Next steps:"
echo ""
echo "1. View the dashboard:"
echo "   python3 -m http.server 8080 --directory artifacts/progress"
echo "   # Open http://localhost:8080/index.html"
echo ""
echo "2. Run the leaderboard:"
echo "   python3 tools/report/leaderboard.py"
echo ""
echo "3. Enable GitHub Pages:"
echo "   - Go to Settings → Pages in your repo"
echo "   - Source: Deploy from a branch"
echo "   - Branch: gh-pages / folder: / (root)"
echo ""
echo "4. The workflow will run automatically on pushes to main"
echo ""

# Check if function size cache exists
if [ ! -f "build/function_sizes.json" ]; then
    echo "⚠️  Warning: Function size cache not found!"
    echo "   Some features may not work correctly."
    echo "   Generate it via Ghidra: Window → Script Manager → ExportFunctionSizes.java"
    echo ""
fi

echo "Setup complete!"
