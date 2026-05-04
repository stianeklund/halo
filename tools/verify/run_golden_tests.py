#!/usr/bin/env python3
import subprocess
import os
import sys

def run_build(harness_enabled=True):
    print(f"Building with HALO_TEST_HARNESS={'ON' if harness_enabled else 'OFF'}...")
    # Clean configuration might be needed to toggle the CMake option effectively
    # but for speed, we assume the user has configured it, or we re-invoke cmake.
    
    # We configure it explicitly:
    cmake_cmd = [
        "cmake", 
        f"-DHALO_TEST_HARNESS={'ON' if harness_enabled else 'OFF'}",
        "-B", "build",
        "."
    ]
    subprocess.run(cmake_cmd, check=True)

    # Build
    build_cmd = [
        "cmake", "--build", "build", 
        "--", "-j"
    ]
    subprocess.run(build_cmd, check=True)

def run_xemu_and_capture(log_file):
    print(f"Running xemu, capturing to {log_file}...")
    cmd = ["bash", "tools/xbox/xemu.sh", "-T", "-l", log_file]
    
    process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    
    captured_output = []
    recording = False
    
    for line in process.stdout:
        if "--- TEST_HARNESS_START ---" in line:
            recording = True
            continue
        if "--- TEST_HARNESS_END ---" in line:
            break
            
        if recording:
            captured_output.append(line)
            
    process.terminate()
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
    
    with open(log_file, "w") as f:
        f.writelines(captured_output)

def main():
    print("=== Phase 1: Capturing Golden Master ===")
    print("Ensure your C function is UNMAPPED in kb.json so the original ASM runs.")
    input("Press Enter to build and capture Golden...")
    run_build(harness_enabled=True)
    run_xemu_and_capture("golden.txt")
    
    print("\n=== Phase 2: Capturing Candidate ===")
    print("Now MAP your C function in kb.json so your new code runs.")
    input("Press Enter to build and capture Candidate...")
    run_build(harness_enabled=True)
    run_xemu_and_capture("candidate.txt")
    
    print("\n=== Phase 3: Results ===")
    with open("golden.txt") as g, open("candidate.txt") as c:
        golden_data = g.read()
        candidate_data = c.read()
        
    if golden_data == candidate_data:
        print("✅ PASS: Candidate output matches Golden exactly.")
    else:
        print("❌ FAIL: Outputs differ!")
        
        # Simple diff print
        import difflib
        g_lines = golden_data.splitlines(keepends=True)
        c_lines = candidate_data.splitlines(keepends=True)
        diff = difflib.unified_diff(g_lines, c_lines, fromfile='golden.txt', tofile='candidate.txt')
        sys.stdout.writelines(diff)

if __name__ == "__main__":
    main()