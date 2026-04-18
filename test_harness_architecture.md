This is perfectly doable without adding any friction to the normal development pipeline. We can cleanly isolate the test harness at both the build-system level and the C-code level.

Here is the blueprint for how we can build this "Golden Master" testing pipeline without touching the default build:

### 1. The Build System (CMake)
We add a simple CMake option to `CMakeLists.txt` that defaults to `OFF`:
```cmake
option(HALO_TEST_HARNESS "Build with in-engine test harness enabled" OFF)

if(HALO_TEST_HARNESS)
    add_compile_definitions(TEST_HARNESS=1)
    target_sources(halo PRIVATE src/halo/test_harness.c)
endif()
```
Because it defaults to `OFF`, running standard `cmake --build build` produces the exact same playable `default.xbe` it does today. No one even knows the harness is there unless they ask for it.

### 2. Hijacking the Boot Process (C Code)
I looked through the codebase and found exactly where we can hook in. In `src/halo/shell_xbox.c`, the ported `main()` function looks like this:

```c
int main(int argc, const char **argv, const char **envp)
{
  rasterizer_preinitialize();
  physical_memory_allocate();
  if (shell_initialize()) {
    main_loop(); // <--- The infinite game loop
    shell_dispose();
  }
  return 0;
}
```

This is the perfect injection point. We just wrap `main_loop()` in a preprocessor check:

```c
  if (shell_initialize()) {
#ifndef TEST_HARNESS
    main_loop();
#else
    extern void run_tests(void);
    run_tests();
#endif
    shell_dispose();
  }
```

Because `shell_initialize()` has already run, memory, file I/O, and Xbox APIs are fully set up. `run_tests()` (which lives in our conditionally compiled `test_harness.c`) can do whatever it wants, print the results to the serial port, and then return. The XBE cleanly shuts down immediately after.

### 3. The Orchestration Script (Python)
We write a new script, `tools/run_tests.py`, that automates the dual-execution process. 

I checked `tools/xemu.sh`, and it already has a `-T` flag that enables `stdio` serial output. Our test script would do the following automatically:

1. **The "Golden" Run:** 
   * Compiles the XBE with the harness enabled, but the target function is mapped to the *original* XBE address.
   * Launches `tools/xemu.sh -T` as a subprocess.
   * The C harness runs the test, `printf`s the resulting struct/state as hex to the serial port, and halts.
   * Python captures this output to `expected.txt`.
2. **The "Candidate" Run:**
   * Compiles the XBE again, this time linking our newly ported C function.
   * Launches `tools/xemu.sh -T` again.
   * Python captures the output to `actual.txt`.
3. **The Verdict:**
   * Python diffs the two files. If they are identical, the port is behaviorally perfect!

### Summary
This approach keeps the tests entirely isolated. To test a newly ported function, a developer would just run `python tools/run_tests.py --target=update_player_health`, and the script would spin up the emulator twice, run the function in both contexts, diff the memory state, and give them a green checkmark or a hex diff of what they broke.
