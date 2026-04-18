# Assertion tripwire

The beta XBE (`cachebeta.xbe`) ships with **live assertions** — every
precondition/postcondition check the original authors wrote funnels through
`display_assert` at `0x8d9f0`. When the 4th argument (`severity`) is non-zero,
`display_assert` calls `stack_walk` and the game is dead.

Catching the assert at the funnel is dramatically cheaper than debugging the
downstream page fault that eventually falls out of `stack_walk` when it
follows a clang-compiled (`-fomit-frame-pointer`) EBP chain into garbage.

## How to use it

1. Start xemu with the GDB stub enabled (`-s`, usually already set in your
   xemu config). Confirm with `info chardev` in the QMP monitor — you should
   see `gdb: ... server=on` on port `1234`.
2. From the repo root:

   ```
   gdb -x tools/asserts.gdb
   ```

3. Type `continue` to run. Any `severity != 0` assertion will print:

   ```
   [ASSERT] severity=1  c:\halo\SOURCE\units\units.c:1509
            message: assert_valid_real_normal3d(&control_data->facing_vector)
   #0  0x0008d9f0 in display_assert
   #1  0x001af990 in unit_set_control
   #2  0x00645532 in stack_walk
   ...
   ```

   The message is the exact assertion string from the original binary —
   `file:line` points at the original's source, and the top of the backtrace
   names the caller whose precondition was violated (usually one of *our*
   lifts, or the last unported frame before one).

## What it catches

Anything that fires a beta assertion, which is broad:

- Wrong struct field offsets → "field has invalid range" checks
- Wrong arg order/type → callee arg-validation asserts
- Failed init ordering → `FUN_e3e10`-style "subsystem not initialized"
- Stale pointers / NaN floats / out-of-range indices → `assert_valid_real`,
  `valid_handle`, etc.

If a smoke test (boot the XBE → main menu → MP map load → SP first cinematic)
completes without the tripwire firing, every invariant the original authors
cared to check is still holding under our lifts.

## Why not just let the game crash?

Because the failure mode is **load-bearing misleading**. When `stack_walk`
tries to walk an EBP chain that passes through a `-fomit-frame-pointer`
clang frame, it eventually dereferences whatever garbage was in EBP as a
general-purpose register and page-faults. The user-visible symptom is a
kernel page fault at `FUN_92370` / `0x923f6` with `CR2 = 0xec700004` (or
similar), which tells you nothing about the original invariant that was
violated — you have to walk the trap frame backward manually to even find
out which assertion fired.

The tripwire gives you the original's error message and file/line in one
line, before the walker even runs.

## Known limits

- Only catches `severity != 0`. Severity-0 calls to `display_assert` are
  soft warnings (the original code logs and continues).
- Requires the GDB stub to be reachable — if you're running a release XBE
  image against retail hardware this isn't available.
- Relies on `display_assert` staying at `0x8d9f0` — if the assertion
  funnel ever gets re-ported to C, update the breakpoint address in
  `tools/asserts.gdb`.
