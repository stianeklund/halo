# Batch 3.2 Lift Report: High-Xref Engine Helpers

**Project:** Halo: Combat Evolved (Xbox Retail 01.10.12.2276, `cachebeta.xbe`, MD5: `c7869590a1c64ad034e49a5ee0c02465`)  
**Branch:** `batch-3.2-high-xref-engine-helpers`  
**Decompiler Engine:** Kuna (Ghidra 11.2 / SLEIGH x86 32-bit little-endian)  
**Status:** 5 / 5 Functions Lifted, Verified, and Patched to `ported: true`  

---

## Executive Summary

Batch 3.2 targets 5 crucial high-xref engine helper functions across graphics rasterization, AI profiling/debug displays, and core 2D polygon geometry clipping. These functions represent 1,736 bytes of core engine logic and eliminate over 172 trampoline jumps from original Xbox code paths to native C reimplementations.

| VA | Size | Function Name / Symbol | Owning Source File | Object Group | Calling Convention | Xrefs | Status |
|:---:|:---:|:---|:---|:---|:---|:---:|:---:|
| `0x17ffc0` | 135 B | [`FUN_0017ffc0`](file:///storage/1F34-EBBE/halo/src/halo/rasterizer/rasterizer_text.c#L39-L51) (`uncompress_int32_to_real_vector3d`) | `src/halo/rasterizer/rasterizer_text.c` | `rasterizer_text.obj` | cdecl (`float *out, uint32_t packed`) | 31 | **ACTIVE / PORTED** |
| `0x180b10` | 508 B | [`FUN_00180b10`](file:///storage/1F34-EBBE/halo/src/halo/rasterizer/rasterizer_text.c#L601-L675) (`compress_real_vector3d_to_int32_clamp`) | `src/halo/rasterizer/rasterizer_text.c` | `rasterizer_text.obj` | cdecl (`float *in` -> `uint32_t`) | 9 | **ACTIVE / PORTED** |
| `0x167ff0` | 429 B | [`FUN_00167ff0`](file:///storage/1F34-EBBE/halo/src/halo/rasterizer/rasterizer.c#L6725-L6818) (`rasterizer_error`) | `src/halo/rasterizer/rasterizer.c` | `rasterizer_xbox_environment_fog.obj` | cdecl varargs (`int hr, const char *fmt, ...`) | 98 | **LIFTED / VERIFIED** |
| `0x053800` | 137 B | [`FUN_00053800`](file:///storage/1F34-EBBE/halo/src/halo/ai/ai_debug.c#L3874-L3898) (`ai_profile_string`) | `src/halo/ai/ai_debug.c` | `ai_debug.obj` | regparm (`char *text, int count, short *stops, void *ctx @<eax>`) | 26 | **LIFTED / VERIFIED** |
| `0x108060` | 527 B | [`FUN_00108060`](file:///storage/1F34-EBBE/halo/src/halo/math/geometry.c#L690-L779) (`convex_hull2d_intersect`) | `src/halo/math/geometry.c` | `geometry.obj` | cdecl (`int16_t, float*, int16_t, float*, int16_t, float*, float`) | 8 | **LIFTED / VERIFIED** |

---

## 1. Vector Compression / Decompression (`0x17ffc0` & `0x180b10`)

The Xbox Halo vertex pipeline packs 3D unit normal vectors into compact 32-bit bitfields for hardware vertex buffers and environmental decals:
- **Bits [10:0]:** 11-bit signed fixed point component ($X$)
- **Bits [21:11]:** 11-bit signed fixed point component ($Y$)
- **Bits [31:22]:** 10-bit signed fixed point component ($Z$)

Both routines were previously implemented in `src/halo/rasterizer/rasterizer_text.c` but parked in `tools/audit/deactivation_allowlist.json` with `"ported": false`.
In Batch 3.2:
1. Marked `"ported": true` in `kb.json`.
2. Removed both deactivations from `tools/audit/deactivation_allowlist.json`.
3. Verified zero regressions and eliminated 40 trampolines across rasterizer models, flares, and decals.

---

## 2. Direct3D Rasterizer Error Reporting (`0x167ff0`)

- **VA:** `0x167ff0` (429 bytes, 98 callers across rasterizer).
- **Symbol:** `FUN_00167ff0` (`rasterizer_error`).
- **Signature:** `void FUN_00167ff0(int hr, const char *format, ...)`.
- **Functionality:**
  - Formats user message using `vsprintf`.
  - Queries D3D error description via `FUN_00201c48` (`D3DXGetErrorStringA`), falling back to `<can't get description>`.
  - Dispatches `hr` code through dual binary jump tables mapping all known Xbox Direct3D error constants (`D3DERR_*`, `E_OUTOFMEMORY`, `E_FAIL`, `E_INVALIDARG`).
  - Calls `error(2, "%s in %s (code=%d, error=%s)", error_name, message, hr, desc)`.

---

## 3. AI Profiling / Status Text Drawer (`0x053800`)

- **VA:** `0x053800` (137 bytes, 26 callers in `ai_debug.c` and `encounters.c`).
- **Symbol:** `FUN_00053800` (`ai_profile_string`).
- **Signature:** `void FUN_00053800(char *text, int column_count, short *column_positions, void *context @<eax>)`.
- **Functionality:**
  - Sets up screen bounds rectangle with top coordinate loaded from screen-derived global `0x5aba80`.
  - Defaults context to `*(void **)0x2ee6c4` if `context == NULL`.
  - Configures font text style via `interface_draw_text(1, -1, 0, 0, 5, 0)`.
  - Sets text color via `draw_string_set_color(context)`.
  - Sets tab stops via `draw_string_set_tab_stops(column_positions, column_count)`.
  - Draws text with `rasterizer_text_draw(bounds, NULL, cursor, 0, text)`.
  - Clears tab stops with `draw_string_set_tab_stops(NULL, 0)`.
  - Decrements global `0x5aba80` by line height `(bounds[0] - cursor[1])`, scrolling AI debug lines upward from the bottom of the screen.

---

## 4. Convex Hull 2D Polygon Intersection (`0x108060`)

- **VA:** `0x108060` (527 bytes, 8 callers in `structures.c` and `structure_visibility.c`).
- **Symbol:** `FUN_00108060` (`convex_hull2d_intersect`).
- **Owning Module:** `c:\halo\SOURCE\math\geometry.c` (moved in `kb.json` from incorrect `rectangles.obj` to `geometry.obj`).
- **Signature:** `short FUN_00108060(int16_t p_count, const float *p, int16_t q_count, const float *q, int16_t maximum_count, float *result, float epsilon)`.
- **Functionality:**
  - Validates `maximum_count <= 512`, non-null pointers, non-zero vertex counts, and ensures `result` does not alias `p` or `q`.
  - Allocates two 512-point ping-pong stack buffers (`float ping_pong[2][1024]`, 8192 bytes total).
  - Iterates over each oriented edge of polygon $P$ from vertex $prev$ to $i$:
    - Derives separating line plane using `plane2d_from_points`.
    - Clips candidate polygon against line using `convex_polygon2d_clip_to_plane`.
    - Routes final edge output directly into caller's `result` buffer.
  - Returns final polygon vertex count, or -1 on error.
- **Call site corrections:** Resolved callers in `src/halo/structures/structures.c` and `src/halo/structures/structure_visibility.c` to pass authentic float epsilon values (`0.00048828125f` / `0x3a000000` and `0.0001f` / `0x38d1b717`).

---

## 5. Build and Verification Audits

1. **Knowledge Base Generation:** `build/generated/decl.h` cleanly regenerated via `tools/analysis/knowledge.py`.
2. **Register Argument Audit:** `python3 tools/audit/extract_reg_args.py --check` reports `917 OK, 0 drift, 0 missing, 0 stale`.
3. **Parameter Type Baseline:** `python3 tools/audit/check_param_types.py --check` reports `PASS: no new type mismatches`.
4. **Hazard Scan:** `python3 tools/audit/check_lift_hazards.py --changed-only` reports zero new hazards or warnings.
5. **XCALL Type Audit:** `python3 tools/audit/check_xcall_types.py` reports zero ERRORs.
6. **Binary Patching:** `python3 tools/build/build.py -q --target patched_xbe` produces `halo-patched/default.xbe` (5.3 MB) with exit code 0.
