#!/usr/bin/env python3
"""
Verification script for progress reporting tools.
Run this to ensure everything is working correctly.
"""

import os
import sys
import subprocess
import json

def check_file(path, description):
    """Check if a file exists."""
    if os.path.exists(path):
        size = os.path.getsize(path)
        print(f"✅ {description}: {path} ({size:,} bytes)")
        return True
    else:
        print(f"❌ {description}: {path} NOT FOUND")
        return False

def run_command(cmd, description):
    """Run a command and check result."""
    print(f"\n🔄 {description}...")
    try:
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
        if result.returncode == 0:
            print(f"✅ {description} - SUCCESS")
            return True
        else:
            print(f"❌ {description} - FAILED")
            print(f"   Error: {result.stderr[:200]}")
            return False
    except Exception as e:
        print(f"❌ {description} - ERROR: {e}")
        return False

def main():
    print("=" * 70)
    print("PROGRESS REPORT TOOLS - VERIFICATION")
    print("=" * 70)
    print()
    
    os.chdir('/mnt/g/dev/halo')
    
    all_passed = True
    
    # Check required files
    print("📁 Checking required files...")
    print("-" * 70)
    files_ok = all([
        check_file('kb.json', 'Knowledge base'),
        check_file('kb_meta.json', 'Metadata store'),
        check_file('tools/report/generate_decomp_report.py', 'Report generator'),
        check_file('tools/report/leaderboard.py', 'Leaderboard tool'),
        check_file('tools/report/compare_reports.py', 'Compare tool'),
        check_file('.github/workflows/progress-report.yml', 'CI workflow'),
    ])
    all_passed &= files_ok
    
    # Test report generation
    print("\n" + "=" * 70)
    print("🧪 Testing report generation...")
    print("-" * 70)
    
    gen_ok = run_command(
        'python3 tools/report/generate_decomp_report.py -o /tmp/test_report.json 2>&1',
        'Generate JSON report'
    )
    all_passed &= gen_ok
    
    if gen_ok:
        # Validate JSON output
        try:
            with open('/tmp/test_report.json') as f:
                data = json.load(f)
                print(f"✅ JSON is valid")
                print(f"   Project: {data['project']['display_name']}")
                print(f"   Functions: {data['summary']['functions']['ported']:,} / {data['summary']['functions']['total']:,}")
                print(f"   Units: {len(data['units'])}")
        except Exception as e:
            print(f"❌ JSON validation failed: {e}")
            all_passed = False
    
    # Test HTML generation
    html_ok = run_command(
        'python3 tools/report/generate_decomp_report.py --html /tmp/test_dashboard.html 2>&1',
        'Generate HTML dashboard'
    )
    all_passed &= html_ok
    
    # Test leaderboard
    print("\n" + "=" * 70)
    print("🧪 Testing leaderboard...")
    print("-" * 70)
    
    lb_ok = run_command(
        'python3 tools/report/leaderboard.py --report /tmp/test_report.json --closest 5 2>&1 | head -20',
        'Run leaderboard'
    )
    all_passed &= lb_ok
    
    # Test comparison (same file, should show no changes)
    print("\n" + "=" * 70)
    print("🧪 Testing report comparison...")
    print("-" * 70)
    
    cmp_ok = run_command(
        'python3 tools/report/compare_reports.py /tmp/test_report.json /tmp/test_report.json 2>&1 | head -10',
        'Compare identical reports'
    )
    all_passed &= cmp_ok
    
    # Summary
    print("\n" + "=" * 70)
    print("SUMMARY")
    print("=" * 70)
    
    if all_passed:
        print("✅ ALL CHECKS PASSED")
        print()
        print("Your progress reporting tools are ready to use!")
        print()
        print("Quick commands:")
        print("  python3 tools/report/generate_decomp_report.py --html artifacts/progress/index.html")
        print("  python3 tools/report/leaderboard.py")
        print("  python3 -m http.server 8080 --directory artifacts/progress")
        print()
        print("Enable GitHub Pages to deploy dashboard automatically.")
        return 0
    else:
        print("❌ SOME CHECKS FAILED")
        print()
        print("Troubleshooting:")
        print("  1. Ensure you're in the repo root directory")
        print("  2. Check that kb.json and kb_meta.json exist")
        print("  3. Install dependencies: pip3 install clang")
        return 1

if __name__ == '__main__':
    sys.exit(main())
