# Object Data Corruption → Game Freeze

**Status:** FIXED — root cause identified and patched
**Severity:** Critical — was killing the main game thread, freezing the game
**Regression:** Confirmed — original cachebeta.xbe did not exhibit this behavior
**Affected map:** Halo (a30), possibly others
**Discovered:** 2026-05-16
**Fixed:** 2026-05-16

## Root Cause (IDENTIFIED AND FIXED)

The crash was caused by a **forward thunk generator bug** in `tools/analysis/knowledge.py`.

The `_gen_thunk_single_reg` method used MSVC-style `asm mov <reg>, arg0` to load
register arguments into place. Under clang (`-target i386-pc-win32`), this asm block
has no clobber declaration, so the optimizer freely reused the register for argument
marshalling — overwriting the value set by the asm before the call.

**Specific crash path:**
- `FUN_00136bc0(int current_object_handle@<edi>, ..., float *shield_damage, float *body_damage)`
- The thunk loaded `EDI = current_object_handle`, then the compiler overwrote `EDI`
  with `&shield_damage` for a push instruction
- FUN_00136bc0 received a **stack pointer** as its object handle
- `datum_get` assert fired: `data.c#79: object index #9668 (0xd00a25c4) is unused or changed`
- This killed the main game thread

**Fix (commit after `da7c69bc`):** Removed the `_gen_thunk_single_reg` special case.
All `@<reg>` thunks now use `_gen_thunk_multi_reg` which emits naked GCC-style asm
that pushes all stack args first, then loads register args last — no clobber possible.

## normalize3d NaN Messages (pre-existing, not a regression)

After the crash fix, `normalize3d: NaN INPUT` messages continue to appear
(callers: `0x13ad84`, `0x13ae48` inside FUN_0013ab20, plus `0x18b818`, `0x13bff6`).
**Investigation confirmed these are in unported code only** — the lighting probe
computation (FUN_0013ab20 / FUN_0013bce0) produces NaN when objects are in
transitional states (e.g. AI exiting Covenant Spirit dropship). The original XBE
had the same NaN but normalize3d silently propagated it; our guard zeros it safely.
This is **not a regression** — just cosmetic lighting artifacts for transitioning objects.

## Symptoms

- Game freezes 2–6 minutes into gameplay on the Halo map
- No error screen displayed — screen just stops updating
- All game threads remain alive in kernel waits, but the main game thread
  (running `main_loop` at 0x102e40) has terminated
- Multiple corruption signatures observed in different runs:
  - `normalize3d: NaN INPUT` — NaN in object position/lighting fields
  - `shield_material_type` assert — out-of-range value in damage resistance

## Root Cause (not yet identified)

A ported function is corrupting object data fields. The corruption hits
different fields at different times, producing different crash signatures.
This is consistent with a **buffer overflow, struct layout mismatch, or
MSVC stack overlap hazard** in a ported function that writes to object data.

The original cachebeta.xbe does NOT exhibit any of these crashes — they are
all regressions from our ported code.

## Crash Paths Observed

### Path 1: NaN in lighting

1. NaN appears in object data (position at object+0x50/0x54/0x58)
2. Unported `FUN_0013bce0` (object_lights.c) reads position, calls lighting
   probe `FUN_0013ab20`, accumulates results, calls `normalize3d`
3. NaN propagates into rendering → GPU fault → main thread dies

Stack: render.obj → scenario_test_pvs → scenario BSP functions → objects.obj
→ normalize3d (all unported except normalize3d)

### Path 2: shield_material_type assert

1. Corrupted `shield_material_type` field (offset 0xd2 from damage_resistance)
   has value outside [0, 32]
2. Assert fires in unported `FUN_00136bc0` (damage.c line 0x60e)
3. `display_assert` → `stack_walk` (works now with frame pointer fix)
4. `system_exit` → `halt_and_catch_fire` → crashes on null D3D vtable call
   (EIP=0x4F or 0xF7 — vtable offset through null device pointer)

### Secondary crash: halt_and_catch_fire

`halt_and_catch_fire` at 0x1029a0 tries to render an error screen, but when
called mid-frame, the D3D device state is inconsistent, causing a null vtable
crash. This kills the main thread silently. **Mitigated**: `system_exit` now
spins forever instead of calling halt_and_catch_fire, keeping XBDM reachable
and debug.txt preserved.

### Secondary crash: stack_walk (fixed)

`stack_walk` walks the EBP chain, but clang's default `-O3` omits frame
pointers. The walker follows garbage EBP values into invalid memory.
**Fixed**: Added `-fno-omit-frame-pointer` to CMake release flags. Also fixed
register pressure in player_control.c inline asm (`"r"` → `"g"` constraints).

## Current Mitigations

### normalize3d NaN guard
Zeros the vector and returns 0.0f on NaN input. Prevents NaN from propagating
into rendering. The `error()` log fires but `stack_walk()` is removed (was
crashing before the frame pointer fix).

### system_exit spin loop
Instead of calling `halt_and_catch_fire` (which crashes mid-frame),
`system_exit` now logs and spins forever. Assert messages will appear in
debug.txt and XBDM stays reachable for inspection.

### display_assert reordering
Error message is now logged BEFORE `stack_walk`, so the assert reason appears
in debug.txt even if stack_walk has issues.

### Frame pointer preservation
`-fno-omit-frame-pointer` added to CMake release flags. `stack_walk` now
produces correct traces from ported code.

### xbox_acosf rewrite
Rewrote from inline asm (numerically unstable `1-x^2`) to pure C using
`(1+x)(1-x)`. Eliminated one known NaN source, but others remain.

### Other inlines.h fixes
- `xbox_fabsf`: replaced asm with `__builtin_fabsf`
- `xbox_log10`: added missing `"st(1)"` clobber
- `xbox_pow`: documented base>0 restriction

## What to Investigate Next

1. **Find the corrupting ported function.** The corruption affects multiple
   object fields (position, shield_material_type), suggesting a struct write
   that's offset incorrectly or a buffer that overflows into adjacent fields.

2. **Prime suspects:**
   - Ported functions in objects.obj (73 ported) that write to object structs
   - Ported functions in damage.obj that modify damage_resistance fields
   - Any ported function with local buffers passed to unported callees
     (MSVC stack overlap hazard — see CLAUDE.md)
   - Physics/collision functions that update object position

3. **Approach:** Add NaN/range checks at object position and damage_resistance
   WRITE sites in ported code. When corruption is detected, log which function
   wrote the bad value and what the value was.

4. **Bisection:** Use `"ported": false` in kb.json to disable groups of ported
   functions and narrow down which one causes the corruption. Start with
   objects.obj and damage.obj ported functions.

## Debug Keyboard Map

| Key | Action | Safe? |
|-----|--------|-------|
| Esc | Set debug break flag | Yes |
| F2 | Select previous AI encounter | Yes |
| F3 | Select next AI encounter | Yes |
| F4 | Select next/prev actor (+Ctrl) | Yes |
| F5 | Cycle AI line-spray debug viz | Yes |
| F6 | ai_erase_all (delete all AI) | Yes (doesn't cause crash) |
| . | Unknown | Unknown |
| F9 | Reset error log | CRASHES (ported/unported mismatch) |

## Reproduction

1. Boot patched build on real Xbox
2. Load the Halo map (a30) from campaign menu
3. Play normally for 2–6 minutes → freeze or NaN in debug.txt
4. With current mitigations: game may hang on assert (system_exit spins)
   instead of crashing — check debug.txt for EXCEPTION messages

## Files Modified

- `src/inlines.h` — xbox_acosf, xbox_fabsf, xbox_log10, xbox_pow
- `src/halo/math/vector_math.c` — normalize3d NaN guard
- `src/halo/math/real_math.c` — rotate_vector3d_by_sincos NaN diagnostic
- `src/halo/math/random_math.c` — FUN_0010c510 NaN recovery
- `src/halo/cseries/cseries.c` — display_assert: log before stack_walk
- `src/halo/cseries/cseries_windows.c` — system_exit: spin instead of crash
- `src/halo/game/player_control.c` — asm constraint fix for frame pointers
- `CMakeLists.txt` — `-fno-omit-frame-pointer`
