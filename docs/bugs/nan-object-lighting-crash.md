# Object Data Corruption → Game Freeze

**Status:** FULLY FIXED
**Severity:** Critical — was killing the main game thread, freezing the game
**Regression:** Confirmed — original cachebeta.xbe did not exhibit the freeze
**Affected map:** Halo (a30), possibly others
**Discovered:** 2026-05-16
**Fixed:** 2026-05-16

---

## Fix 1: Forward Thunk Generator Bug (the actual freeze)

**Root cause:** `_gen_thunk_single_reg` in `tools/analysis/knowledge.py` used
MSVC-style `asm mov <reg>, arg0` to load register arguments. Under clang
(`-target i386-pc-win32`) this asm has no clobber declaration, so the optimizer
freely reused the register for argument marshalling — overwriting the value set
by the asm before the call.

**Specific crash path:**
- `FUN_00136bc0(int current_object_handle@<edi>, ..., float *shield_damage, ...)`
- Thunk loaded `EDI = current_object_handle`, compiler then overwrote `EDI`
  with `&shield_damage` for a PUSH instruction
- FUN_00136bc0 received a stack pointer as its object handle
- `datum_get` assert fired: object index `0xd00a25c4` is unused or changed
- Main game thread killed → freeze

**Fix:** Deleted `_gen_thunk_single_reg`. All `@<reg>` thunks now use
`_gen_thunk_multi_reg` which emits naked GCC-style asm: pushes all stack args
first (in reverse order, adjusting offset by +4 per push), loads register args
last — no clobber possible.

---

## Fix 2: normalize3d NaN persistence (pre-existing, not the freeze)

After the thunk fix, `normalize3d: NaN INPUT` messages appeared in debug.txt.
These are **not a regression** — the original XBE had the same latent behavior,
it just silently propagated NaN rather than logging it.

### Full investigation trail

**Observed pattern:** `(-1.#IND00, -1.#IND00, -z_valid)` — x and y are NaN
(`-1.#IND` = x87 indefinite, produced by `0.0/0.0` or `0*INF`), z is a valid
float (e.g. -0.848528). Fires from 4 callers, all in unported original XBE code:
- `0x13ad84`, `0x13ae48` — inside `FUN_0013ab20` (BSP lighting probe interpolator)
- `0x13bff6` — inside `FUN_0013bce0` (per-object lighting update)
- `0x18b818` — inside `FUN_0018b780` (direction vector smoother)

**Original normalize3d behavior (0x13010):** Confirmed via Ghidra decompile and
memory read:
- Threshold at `*(double*)0x2533d0` ≈ `1e-4` (confirmed: bytes `00 00 00 e0 e2 36 1a 3f`)
- Scale numerator `*(float*)0x2533c8` = `1.0f` (confirmed: `00 00 80 3f`)
- Return value for below-threshold = `*(float*)0x2533c0` = `0.0f` (confirmed: `00 00 00 00`)
- **Critical:** original exits early for below-threshold mag but does NOT zero
  the vector — leaves NaN in place and returns 0.0f

**NaN source — vertex decompressor ruled out:** `FUN_0017ffc0` (vertex normal
decompressor, called by `FUN_00180570`/`FUN_00180660`) performs pure
integer-to-float arithmetic on a packed uint32. All constants confirmed valid:
- `_DAT_0029ba04` = `2^-20`, `_DAT_002afe30` = `2^-21`
- `_DAT_002afe34` ≈ `4.88e-4`, `_DAT_0028c8e0` ≈ `9.77e-4`
Disassembly of `FUN_0017ffc0` confirmed it returns `param_1` (the output buffer)
in EAX at RET — no garbage pointer. Cannot produce NaN from valid input.

**NaN source — barycentric solver partially ruled out:** `FUN_0010d830` computes
barycentric coordinates via `1.0f / area`. For a degenerate triangle (zero area
in projection), this produces NaN — but it would affect ALL three components
equally, not just x and y. Partial-NaN `(NaN, NaN, valid_z)` cannot come from
this path alone.

**NaN source — render state buffer confirmed:** The `DAT_0050652c` table is
allocated by `FUN_0018af90` as `game_state_data_new("cached object render states",
0x100, 0x100)` — 256 entries × 256 bytes. `datum_new` (0x119610) **does**
zero-init new slots via `csmemset`. However, in `FUN_0018bc60`, the copy from
freshly-computed `iVar1+0x88` into the smoothing buffer `iVar1+0x14` is
conditional on `local_5 != 0`. When `local_5 = 0` and the slot contains a
prior frame's stale NaN (propagated silently by the original normalize3d), the
smoother reads that NaN and `normalize3d` receives it — with the original code,
NaN is returned and persists indefinitely (`NaN + finite = NaN` every frame).

**Why partial NaN `(NaN, NaN, valid_z)`:** Stale memory in `iVar1+0x14` from a
previous object's occupancy. The x and y direction slots happened to hold NaN
bit patterns while z held a valid float. The original normalize3d left these
unchanged, perpetuating the pattern across all subsequent frames.

**AI deboarding regression discovered during fix:** First fix attempt zeroed ALL
below-threshold vectors (not just NaN ones). This broke AI deboarding because
some legitimate near-zero direction vectors are left unchanged by the original
normalize3d (callers use the 0.0f return value to detect the condition, but may
still read the unmodified vector). Fixed by only zeroing when NaN is detected.

### Final fix (current state)

```c
mag = sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
if (!(fabsf(mag) >= *(double *)0x2533d0)) {
    /* Only zero for NaN input — NaN mag means NaN input components.
       Near-zero non-NaN vectors are left unchanged (original behavior). */
    if (v[0] != v[0] || v[1] != v[1] || v[2] != v[2]) {
        v[0] = v[1] = v[2] = 0.0f;
    }
    return 0.0f;
}
```

- Preserves original threshold from binary (`*(double*)0x2533d0`)
- `!(fabsf(NaN) >= threshold)` = `!(false)` = true → NaN branch taken ✓
- Near-zero valid vectors: unchanged (original behavior preserved) ✓
- NaN zeroed → next frame smoother receives `(0,0,0)` → clamps toward target
  → normalize3d succeeds → lighting recovers within 1-2 frames ✓
- Equivalence: 97/100 seeds pass (3 failures are the intentional NaN→zero divergence)

---

## Secondary Issues (all fixed)

### halt_and_catch_fire crash
`halt_and_catch_fire` (0x1029a0) tries to render an error screen but when called
mid-frame the D3D device state is inconsistent, causing a null vtable crash.
**Fix:** `system_exit` now spins forever instead of calling halt_and_catch_fire,
keeping XBDM reachable and debug.txt preserved.

### stack_walk garbage frames
`stack_walk` follows EBP chain but clang `-O3` omits frame pointers.
**Fix:** `-fno-omit-frame-pointer` in CMake release flags. Also fixed register
pressure in `player_control.c` inline asm (`"r"` → `"g"` constraints).

### display_assert log ordering
Assert message was logged AFTER `stack_walk`, so a stack_walk crash could lose
the assert reason. **Fix:** Log the message before calling `stack_walk`.

### xbox_acosf / inlines.h
`xbox_acosf` used numerically unstable `1-x^2` formula. Rewrote to `(1+x)(1-x)`.
Also: `xbox_fabsf` → `__builtin_fabsf`; `xbox_log10` missing `"st(1)"` clobber.

---

## Debug Keyboard Map

| Key | Action | Safe? |
|-----|--------|-------|
| Esc | Set debug break flag | Yes |
| F2 | Select previous AI encounter | Yes |
| F3 | Select next AI encounter | Yes |
| F4 | Select next/prev actor (+Ctrl) | Yes |
| F5 | Cycle AI line-spray debug viz | Yes |
| F6 | ai_erase_all (delete all AI) | Yes |
| F9 | Reset error log | CRASHES (ported/unported mismatch) |

---

## Files Modified

| File | Change |
|------|--------|
| `tools/analysis/knowledge.py` | Deleted `_gen_thunk_single_reg`; all thunks use `_gen_thunk_multi_reg` |
| `src/halo/math/vector_math.c` | normalize3d NaN guard: zero NaN-only, preserve near-zero |
| `src/halo/math/real_math.c` | `periodic_functions_initialize` call name fix |
| `src/halo/memory/data.c` | Port `datum_initialize` (zeroes slot + stamps identifier) |
| `src/inlines.h` | xbox_acosf, xbox_fabsf, xbox_log10, xbox_pow fixes |
| `src/halo/cseries/cseries.c` | display_assert: log before stack_walk |
| `src/halo/cseries/cseries_windows.c` | system_exit: spin instead of crash |
| `src/halo/game/player_control.c` | asm constraint fix for frame pointers |
| `CMakeLists.txt` | `-fno-omit-frame-pointer` |
