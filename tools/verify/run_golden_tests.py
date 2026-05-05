#!/usr/bin/env python3
import subprocess
import os
import sys
import time

def run_build(harness_enabled=True):
    print(f"Building with HALO_TEST_HARNESS={'ON' if harness_enabled else 'OFF'}...")
    cmake_cmd = [
        "cmake", 
        f"-DHALO_TEST_HARNESS={'ON' if harness_enabled else 'OFF'}",
        "-B", "build",
        "."
    ]
    subprocess.run(cmake_cmd, check=True)

    build_cmd = [
        "cmake", "--build", "build", 
        "--", "-j"
    ]
    subprocess.run(build_cmd, check=True)

def run_xbox_and_capture(log_file):
    print(f"Deploying to Xbox and capturing to {log_file}...")
    
    # Deploy and run
    deploy_cmd = [sys.executable, "tools/xbox/deploy_xbox.py", "--xbe-only"]
    subprocess.run(deploy_cmd, check=True)
    
    # Poll debug.txt via XBDM
    fetch_cmd = [sys.executable, "tools/xbox/xbdm_debug_txt.py"]
    
    print("Waiting for test harness to complete...")
    max_attempts = 20
    for attempt in range(max_attempts):
        time.sleep(2.0)
        result = subprocess.run(fetch_cmd, capture_output=True, text=True)
        if "--- TEST_HARNESS_END" in result.stdout:
            # Extract only the test harness portion
            lines = result.stdout.splitlines()
            capture = []
            recording = False
            for line in lines:
                if "--- TEST_HARNESS_START ---" in line:
                    recording = True
                    continue
                if "--- TEST_HARNESS_END" in line:
                    break
                if recording:
                    capture.append(line + "\n")
            
            with open(log_file, "w") as f:
                f.writelines(capture)
            print(f"Captured {len(capture)} lines to {log_file}")
            return
            
    print(f"Timeout waiting for test completion. Last debug.txt output:\n{result.stdout}")
    sys.exit(1)

def main():
    print("=== Phase 1: Capturing Golden Master ===")
    print("Ensure your C function is UNMAPPED in kb.json so the original ASM runs.")
    input("Press Enter to build and capture Golden...")
    run_build(harness_enabled=True)
    run_xbox_and_capture("golden.txt")
    
    print("\n=== Phase 2: Capturing Candidate ===")
    print("Now MAP your C function in kb.json so your new code runs.")
    input("Press Enter to build and capture Candidate...")
    run_build(harness_enabled=True)
    run_xbox_and_capture("candidate.txt")
    
    print("\n=== Phase 3: Results ===")
    with open("golden.txt") as g, open("candidate.txt") as c:
        golden_data = g.read()
        candidate_data = c.read()
        
    if golden_data == candidate_data:
        print("✅ PASS: Candidate output matches Golden exactly.")
    else:
        print("❌ FAIL: Outputs differ!")
        import difflib
        g_lines = golden_data.splitlines(keepends=True)
        c_lines = candidate_data.splitlines(keepends=True)
        diff = difflib.unified_diff(g_lines, c_lines, fromfile='golden.txt', tofile='candidate.txt')
        sys.stdout.writelines(diff)

if __name__ == "__main__":
    main()