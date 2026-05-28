# Transparent Shader Animation Glitch (Periodic Function Table Corruption)

**Status:** FIXED
**Severity:** Moderate (visual — all animated transparent shaders affected)
**Regression:** Yes — original cachebeta.xbe does not exhibit the behavior
**Affected maps:** All maps with shader_transparent_generic or animated shaders
**Discovered:** 2026-05-27
**Fixed:** 2026-05-28
**Build:** 6816cfc4

---

## Symptom

Animated shader effects (teleporter plasma wall, power-up orbs, active camo
shield, transparent generic shaders) exhibit discontinuous animation: the
texture scrolling runs too fast, reverses direction, freezes for 1-2 seconds,
or snaps to random phases. Geometry remains stable; only the animated
texture/material pattern is affected.

## Root Cause

Three bugs in the ported `periodic_table_fill` / `periodic_functions_initialize`
(`random_math.obj`) corrupted the waveform lookup tables at startup:

1. **Hardcoded random seed address.** `periodic_functions_initialize` used
   `*(int *)0x46e3f4 = 0x20f3f660` instead of
   `*get_global_random_seed_address() = 0x20f3f660`. This corrupted the
   gaussian scratch generator's seed, breaking all variable-period waveforms.

2. **FPREM1 vs FPREM.** Clang with `-target i386-pc-win32 -mno-sse` compiled
   `fmod()` to the x87 `FPREM1` instruction (IEEE remainder — quotient rounded
   to nearest) instead of `FPREM` (C fmod — quotient truncated toward zero).
   For phase values > 0.5, `FPREM1(0.52, 1.0)` returns -0.48 (clamped to 0)
   instead of the correct 0.52. This zeroed out entire regions of the triangle,
   slide, and square waveform tables.

3. **Library cos/sin vs FCOS/FSIN.** The original binary uses inline x87
   `FCOS`/`FSIN` instructions. Our C code called library `cos()`/`sin()` which
   use different argument reduction and polynomial approximation, producing
   subtly different values for large phase arguments.

## Fix

Added inline x87 assembly helpers in `src/halo/math/random_math.c`:
- `x87_fcos_mul` / `x87_fsin_mul`: fused multiply + FCOS/FSIN
- `x87_fsin_msub`: fused multiply-subtract + FSIN
- `x87_fmod`: FPREM loop (replaces FPREM1-generating fmod)
- Seed address fix: `*get_global_random_seed_address() = 0x20f3f660`

All 18 waveform tables now match the original cachebeta.xbe byte-for-byte
(within ±1 at float-to-int rounding boundaries).

## Regression Test

`tools/verify/test_periodic_tables.py` reads all 18 tables from a running Xbox
via XBDM and compares against golden reference data in
`tests/golden/periodic_tables/`. Run after any change to `random_math.c`.

## Functions Involved

| Address | Name | Role | Status |
|---------|------|------|--------|
| `0x10ad10` | `periodic_functions_initialize` | Allocates and fills all tables | Ported |
| `0x10aa60` | `periodic_table_fill` | Fills one periodic waveform table | Ported |
| `0x10a930` | `transition_table_fill` | Fills one transition table | Ported |
| `0x10a570` | `periodic_functions_dispose` | Frees all tables | Ported |
| `0x10a5e0` | `periodic_function_evaluate` | Reads the tables (not ported) | Original |

## Bisection Log

1. Deactivated render.obj (42 functions) — still broken
2. Added objects.obj (127 functions) — still broken
3. Added game_time.obj (20 functions) — still broken
4. Deactivated ALL (2261 functions) — crashed (too many deactivation JMPs)
5. Targeted: deactivated only `periodic_functions_initialize` +
   `periodic_table_fill` + `periodic_functions_dispose` — **FIXED**
6. Narrowed: deactivated only `periodic_table_fill` — **FIXED**
