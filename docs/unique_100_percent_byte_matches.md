# Unique 100% Byte-Matched Functions: `nickarcade/halo` vs `stianeklund/halo`

> [!NOTE]
> **Comparison Baseline**:
> - **Local Repository**: [`nickarcade/halo`](https://github.com/nickarcade/halo) on branch `mainrevised2` (Commit: `f1920e16`, Live Dashboard: [`nickarcade.github.io/halo`](https://nickarcade.github.io/halo/))
> - **Upstream Repository**: [`stianeklund/halo`](https://github.com/stianeklund/halo) (Commit: `d4a3cf2266`, Live Dashboard: [`stianeklund.github.io/halo`](https://stianeklund.github.io/halo/))
> - **Binary Target**: Halo CE Xbox debug build 2276 (`halo-patched/cachebeta.xbe`, MD5: `c7869590a1c64ad034e49a5ee0c02465`)

---

## Executive Summary

A comprehensive cross-audit of our structural byte audit (`artifacts/raw_xbe_structural/byte_match_report.csv` and `docs/report.json`) against upstream's live report (`stianeklund.github.io/halo/report.json`) reveals:

1. **13 Verified 100% Byte-Matched Functions Unique to Our Repository**:
   - Exactly **13 functions** have achieved a verified **100% byte match** (`structural exact` or `100.0% aligned byte accuracy`) against the original Xbox debug binary in our repository.
   - **Upstream status for all 13 functions**: Completely **unported** (`ported: false`) or entirely absent from upstream's database. Upstream has **0%** byte match on these functions.
2. **4 Functions with 100.0% VC71 Mnemonic Match Unique to Us**:
   - In addition to byte matching, our repo has 4 functions achieving 100.0% VC71 mnemonic equivalence that are unported in upstream.
3. **Broad Porting Advantage (+439 Functions)**:
   - **Our Repository**: **7,192** / 7,574 game functions ported (**7,218** total ported across all units).
   - **Upstream Repository**: **6,753** / 7,023 game functions ported (**6,779** total ported across all units).
   - **Net Lead**: **+439 more functions ported** in our codebase. Among these uniquely ported functions, **117** have partial byte matches (`structural differ`, with byte accuracy up to 92.7%) and **55** achieve $\ge 90\%$ VC71 match.

---

## The 13 Verified 100% Byte-Matched Functions

The following table lists every function where our codebase has achieved an exact 100% byte match that `stianeklund/halo` does not have.

| Address | Function Symbol | Translation Unit (TU) | Size (Bytes) | Local Verdict / Byte Accuracy | Upstream Status | Local Source File & Line |
| :--- | :--- | :--- | :---: | :---: | :---: | :--- |
| `0x000f0a90` | [`widget_function_null`](file:///storage/1F34-EBBE/halo/src/halo/interface/ui_widget_game_data_input_functions.c#L3777) | `ui_widget_game_data_input_functions` | 1 B | **100.0%** (`structural exact`) | `Unported` (`ported: false`) | [ui_widget_game_data_input_functions.c:3777](file:///storage/1F34-EBBE/halo/src/halo/interface/ui_widget_game_data_input_functions.c#L3777) |
| `0x001454b0` | [`placeholder_initialize`](file:///storage/1F34-EBBE/halo/src/halo/physics/breakable_surfaces.c#L20) | `breakable_surfaces` | 1 B | **100.0%** (`structural exact`) | `Unported` (`ported: false`) | [breakable_surfaces.c:20](file:///storage/1F34-EBBE/halo/src/halo/physics/breakable_surfaces.c#L20) |
| `0x001a0780` | [`bipeds_initialize`](file:///storage/1F34-EBBE/halo/src/halo/units/bipeds.c#L632) | `bipeds` | 1 B | **100.0%** (`structural exact`) | `Unported` (`ported: false`) | [bipeds.c:632](file:///storage/1F34-EBBE/halo/src/halo/units/bipeds.c#L632) |
| `0x001a07b0` | [`bipeds_dispose`](file:///storage/1F34-EBBE/halo/src/halo/units/bipeds.c#L658) | `bipeds` | 1 B | **100.0%** (`structural exact`) | `Absent` (not in database) | [bipeds.c:658](file:///storage/1F34-EBBE/halo/src/halo/units/bipeds.c#L658) |
| `0x001a07f0` | [`biped_delete`](file:///storage/1F34-EBBE/halo/src/halo/units/bipeds.c#L684) | `bipeds` | 1 B | **100.0%** (`structural exact`) | `Unported` (`ported: false`) | [bipeds.c:684](file:///storage/1F34-EBBE/halo/src/halo/units/bipeds.c#L684) |
| `0x001a1b90` | [`biped_approximate_surface_index`](file:///storage/1F34-EBBE/halo/src/halo/units/bipeds.c#L2015) | `bipeds` | 34 B | **100.0%** (`structural exact`, 30/30 B) | `Unported` (`ported: false`) | [bipeds.c:2015](file:///storage/1F34-EBBE/halo/src/halo/units/bipeds.c#L2015) |
| `0x001a7f70` | [`units_dispose`](file:///storage/1F34-EBBE/halo/src/halo/units/units.c#L2094) | `units` | 1 B | **100.0%** (`structural exact`) | `Absent` (not in database) | [units.c:2094](file:///storage/1F34-EBBE/halo/src/halo/units/units.c#L2094) |
| `0x001b5540` | [`vehicles_initialize`](file:///storage/1F34-EBBE/halo/src/halo/units/vehicles.c#L71) | `vehicles` | 1 B | **100.0%** (`structural exact`) | `Unported` (`ported: false`) | [vehicles.c:71](file:///storage/1F34-EBBE/halo/src/halo/units/vehicles.c#L71) |
| `0x001b5570` | [`vehicles_dispose`](file:///storage/1F34-EBBE/halo/src/halo/units/vehicles.c#L97) | `vehicles` | 1 B | **100.0%** (`structural exact`) | `Absent` (not in database) | [vehicles.c:97](file:///storage/1F34-EBBE/halo/src/halo/units/vehicles.c#L97) |
| `0x001b55b0` | [`vehicle_delete`](file:///storage/1F34-EBBE/halo/src/halo/units/vehicles.c#L111) | `vehicles` | 1 B | **100.0%** (`structural exact`) | `Unported` (`ported: false`) | [vehicles.c:111](file:///storage/1F34-EBBE/halo/src/halo/units/vehicles.c#L111) |
| `0x001b98d0` | [`tag_files_close`](file:///storage/1F34-EBBE/halo/src/halo/cache/cache_files.c#L36) | `cache_files` | 5 B | **100.0%** (`structural exact`, VC71: 100%) | `Unported` (`ported: false`) | [cache_files.c:36](file:///storage/1F34-EBBE/halo/src/halo/cache/cache_files.c#L36) |
| `0x001c58f0` | [`FUN_001c58f0`](file:///storage/1F34-EBBE/halo/src/halo/saved%20games/saved_game_files.c#L3497) | `saved_game_files` | 5 B | **100.0%** (`structural exact`) | `Unported` (`ported: false`) | [saved_game_files.c:3497](file:///storage/1F34-EBBE/halo/src/halo/saved%20games/saved_game_files.c#L3497) |
| `0x001c9be0` | [`dsound_virtual_update`](file:///storage/1F34-EBBE/halo/src/halo/sound/sound_dsound_xbox.c#L625) | `sound_dsound_xbox` | 1 B | **100.0%** (`structural exact`) | `Unported` (`ported: false`) | [sound_dsound_xbox.c:625](file:///storage/1F34-EBBE/halo/src/halo/sound/sound_dsound_xbox.c#L625) |

---

## Detailed Function Profiles

### 1. `biped_approximate_surface_index` (`0x001a1b90`)
- **Unit**: [`src/halo/units/bipeds.c`](file:///storage/1F34-EBBE/halo/src/halo/units/bipeds.c#L2015-L2020)
- **Size**: 34 bytes (30 compared instruction bytes)
- **Signature**: `int biped_approximate_surface_index(int unit_handle, float *out_point)`
- **Disassembly & Reconstructed C**:
  ```c
  int biped_approximate_surface_index(int unit_handle, float *out_point)
  {
    return biped_find_ground_surface(2.0f, out_point, (void *)0,
                                     *(float **)0x31fc50, unit_handle);
  }
  ```
- **Binary Evidence**:
  - Confirmed ABI: keystone direction in `@<eax> = *(float**)0x31fc50`; `unit_handle` passed in `@<edi> = param_1`.
  - Pushes scale `2.0f` (`0x40000000`), `out_point`, and `out_vec=NULL`, followed by `ADD ESP, 0xc`.
  - VC71 match: 83.3% mnemonic match, **100.0% raw XBE byte accuracy** (30/30 bytes exactly aligned).
  - Upstream Status: `ported: false`.

---

### 2. `tag_files_close` (`0x001b98d0`)
- **Unit**: [`src/halo/cache/cache_files.c`](file:///storage/1F34-EBBE/halo/src/halo/cache/cache_files.c#L36-L39)
- **Size**: 5 bytes (`CALL cache_files_dispose; RET`)
- **Signature**: `void tag_files_close(void)`
- **Disassembly & Reconstructed C**:
  ```c
  void tag_files_close(void)
  {
    cache_files_dispose();
  }
  ```
- **Binary Evidence**:
  - Compiles to a direct tail-call/return to `cache_files_dispose`.
  - **100.0% VC71 match** and **100.0% raw XBE byte accuracy** (5/5 bytes).
  - Upstream Status: `ported: false`.

---

### 3. `FUN_001c58f0` (`0x001c58f0`)
- **Unit**: [`src/halo/saved games/saved_game_files.c`](file:///storage/1F34-EBBE/halo/src/halo/saved%20games/saved_game_files.c#L3497-L3500)
- **Size**: 5 bytes (`CALL FUN_001c5010; RET`)
- **Signature**: `void FUN_001c58f0(void)`
- **Disassembly & Reconstructed C**:
  ```c
  void FUN_001c58f0(void)
  {
    FUN_001c5010();
  }
  ```
- **Binary Evidence**:
  - Thin thunk wrapping memory unit enumeration test `FUN_001c5010`.
  - **100.0% raw XBE byte accuracy** (`structural exact`).
  - Upstream Status: `ported: false`.

---

### 4. Lifecycle & Subsystem Stubs (1-Byte `RET` Matches)
The following 10 functions represent deliberate lifecycle hooks, null callbacks, or empty subsystem initializers/disposers in the original Xbox retail/debug build:

- `widget_function_null` (`0x000f0a90`): Default no-op UI input callback in [`ui_widget_game_data_input_functions.c`](file:///storage/1F34-EBBE/halo/src/halo/interface/ui_widget_game_data_input_functions.c#L3777).
- `placeholder_initialize` (`0x001454b0`): Breakable surface placeholder init stub in [`breakable_surfaces.c`](file:///storage/1F34-EBBE/halo/src/halo/physics/breakable_surfaces.c#L20).
- `bipeds_initialize` (`0x001a0780`): Biped subsystem init stub in [`bipeds.c`](file:///storage/1F34-EBBE/halo/src/halo/units/bipeds.c#L632).
- `bipeds_dispose` (`0x001a07b0`): Biped subsystem teardown stub in [`bipeds.c`](file:///storage/1F34-EBBE/halo/src/halo/units/bipeds.c#L658).
- `biped_delete` (`0x001a07f0`): Biped instance destructor stub in [`bipeds.c`](file:///storage/1F34-EBBE/halo/src/halo/units/bipeds.c#L684).
- `units_dispose` (`0x001a7f70`): Unit subsystem dispose stub in [`units.c`](file:///storage/1F34-EBBE/halo/src/halo/units/units.c#L2094).
- `vehicles_initialize` (`0x001b5540`): Vehicle subsystem init stub in [`vehicles.c`](file:///storage/1F34-EBBE/halo/src/halo/units/vehicles.c#L71).
- `vehicles_dispose` (`0x001b5570`): Vehicle subsystem dispose stub in [`vehicles.c`](file:///storage/1F34-EBBE/halo/src/halo/units/vehicles.c#L97).
- `vehicle_delete` (`0x001b55b0`): Vehicle instance destructor stub in [`vehicles.c`](file:///storage/1F34-EBBE/halo/src/halo/units/vehicles.c#L111).
- `dsound_virtual_update` (`0x001c9be0`): DirectSound virtual audio buffer pump stub in [`sound_dsound_xbox.c`](file:///storage/1F34-EBBE/halo/src/halo/sound/sound_dsound_xbox.c#L625).

In the original executable, each compiles to `C3` (`RET`). Our repository has them properly ported, linked into `kb.json`, hooked in redirects, and verified at **100.0% byte match**. Upstream does not have them ported.

---

## 4 Functions with 100.0% VC71 Match Unique to Us

Beyond raw byte comparisons, our repository contains 4 functions that achieve a perfect **100.0% VC71 mnemonic match** that upstream has completely unported or absent:

| Address | Function Symbol | Translation Unit | Size | VC71 Match | Local Source File & Line | Upstream Status |
| :--- | :--- | :--- | :---: | :---: | :--- | :--- |
| `0x001b98d0` | [`tag_files_close`](file:///storage/1F34-EBBE/halo/src/halo/cache/cache_files.c#L36) | `cache_files` | 5 B | **100.0%** | [cache_files.c:36](file:///storage/1F34-EBBE/halo/src/halo/cache/cache_files.c#L36) | `Unported` (`ported: false`) |
| `0x00138f70` | [`FUN_00138f70`](file:///storage/1F34-EBBE/halo/src/halo/objects/damage.c#L2703) | `damage` | 85 B | **100.0%** | [damage.c:2703](file:///storage/1F34-EBBE/halo/src/halo/objects/damage.c#L2703) | `Unported` (`ported: false`) |
| `0x000dc7f0` | [`first_person_weapons_dispose_from_old_map`](file:///storage/1F34-EBBE/halo/src/halo/interface/first_person_weapons.c#L46) | `first_person_weapons` | 208 B | **100.0%** | [first_person_weapons.c:46](file:///storage/1F34-EBBE/halo/src/halo/interface/first_person_weapons.c#L46) | `Absent` (not in database) |
| `0x001cb880` | [`sound_is_active`](file:///storage/1F34-EBBE/halo/src/halo/sound/sound_manager.c#L1049) | `sound_manager` | 32 B | **100.0%** | [sound_manager.c:1049](file:///storage/1F34-EBBE/halo/src/halo/sound/sound_manager.c#L1049) | `Absent` (not in database) |

---

## Top Uniquely Ported Functions with High Byte Accuracy

Our repository contains **117 partially byte-matched functions** (`structural differ`) that upstream has marked `unported: false`. Below are the top 10 by size and byte accuracy:

| Address | Function Symbol | Translation Unit | Size | Byte Accuracy | VC71 Match | Local Source Path |
| :--- | :--- | :--- | :---: | :---: | :---: | :--- |
| `0x001808f0` | `FUN_001808f0` | `rasterizer_text` | 536 B | **92.7%** | 94.7% | [`src/halo/rasterizer/rasterizer_text.c`](file:///storage/1F34-EBBE/halo/src/halo/rasterizer/rasterizer_text.c) |
| `0x001bb5a0` | `acquire_read_request` | `cache_files_windows` | 160 B | **85.3%** | 91.9% | [`src/halo/cache/cache_files_windows.c`](file:///storage/1F34-EBBE/halo/src/halo/cache/cache_files_windows.c) |
| `0x00147ae0` | `collision_surface_find_closest_point2d` | `collision_bsp` | 555 B | **79.3%** | 93.2% | [`src/halo/physics/collision_bsp.c`](file:///storage/1F34-EBBE/halo/src/halo/physics/collision_bsp.c) |
| `0x00149570` | `bsp2d_test_pill_recursive` | `collision_bsp` | 267 B | **77.5%** | 97.8% | [`src/halo/physics/collision_bsp.c`](file:///storage/1F34-EBBE/halo/src/halo/physics/collision_bsp.c) |
| `0x00181a90` | `FUN_00181a90` | `rasterizer_text` | 385 B | **77.4%** | 94.8% | [`src/halo/rasterizer/rasterizer_text.c`](file:///storage/1F34-EBBE/halo/src/halo/rasterizer/rasterizer_text.c) |
| `0x00184300` | `rasterizer_transparent_geometry_begin` | `rasterizer_text` | 47 B | **67.4%** | 88.9% | [`src/halo/rasterizer/rasterizer_text.c`](file:///storage/1F34-EBBE/halo/src/halo/rasterizer/rasterizer_text.c) |
| `0x000b45c0` | `FUN_000b45c0` | `game` | 226 B | **61.7%** | 90.3% | [`src/halo/game/game.c`](file:///storage/1F34-EBBE/halo/src/halo/game/game.c) |
| `0x000b5210` | `slayer_engine_display_score` | `game` | 386 B | **58.7%** | 96.7% | [`src/halo/game/game.c`](file:///storage/1F34-EBBE/halo/src/halo/game/game.c) |
| `0x001c31f0` | `FUN_001c31f0` | `saved_game_files` | 86 B | **58.1%** | 84.8% | [`src/halo/saved games/saved_game_files.c`](file:///storage/1F34-EBBE/halo/src/halo/saved%20games/saved_game_files.c) |
| `0x001c5560` | `create_enumerated_saved_game_file` | `saved_game_files` | 897 B | **53.9%** | 96.3% | [`src/halo/saved games/saved_game_files.c`](file:///storage/1F34-EBBE/halo/src/halo/saved%20games/saved_game_files.c) |
