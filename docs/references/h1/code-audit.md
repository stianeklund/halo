# Halo 1 Reference Code Audit

## Summary

This page records the initial cross-reference pass between the imported Reclaimers Library Halo 1 reference notes and this repository's reimplemented Xbox code. Reclaimers-derived facts are useful search context, but Xbox runtime behavior remains binary-first and must be verified against source, `kb.json`, disassembly, or runtime tests before guiding lifts.

## Confirmed Against Code

| Imported claim                                                                                                | Local evidence                                                                                                                                                                                                                                                                                                                |
|---------------------------------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Xbox tag metadata loads at `0x803A6000`; Xbox tag space is 22 MiB.                                            | `src/types.h` defines `TAG_CACHE_BASE_ADDRESS 0x803A6000`; `src/halo/cache/physical_memory_map.c` defines `HALO_TAG_CACHE_SIZE 0x1600000`.                                                                                                                                                                                    |
| Xbox cache headers use build string `01.10.12.2276`, 0x800-byte headers, and scenario type at header `+0x60`. | `src/halo/cache/cache_files_windows.c` validates the build string, reads `0x800` headers, uses scenario type at `+0x60`, and creates `z:\cache%03d.map`.                                                                                                                                                                      |
| Legacy HaloScript syntax node limit is 19001.                                                                 | `src/halo/hs/hs.c` initializes `data_new("script node", 0x4a39, 0x14)`, and `0x4a39 == 19001`.                                                                                                                                                                                                                                |
| HaloScript external globals and scenario globals are separate mechanisms.                                     | `src/halo/hs/hs.c` references the external global descriptor table at `0x2f3708`, count at `0x27d504`, and scenario globals from scenario block `+0x4a8`.                                                                                                                                                                     |
| HaloScript object lists are runtime datum/list structures.                                                    | `src/halo/hs/hs_runtime.c` uses `0x5aa698` as object-list header data and `0x5aa694` as object-list reference data.                                                                                                                                                                                                           |
| Several game-state datum limits match Reclaimers values.                                                      | Confirmed examples: players `16`, teams `16`, actor `0x100`, swarm `0x20`, swarm component `0x100`, prop `0x300`, decals `0x800`, effect `0x100`, effect locations `0x200`, particle `0x400`, particle systems `0x40`, particle-system particles `0x200`, object looping sounds `0x400`, cached object render states `0x100`. |
| Some engine-referenced tag paths are present in lifted source.                                                | `src/halo/interface/ui_widget.c` references `sound\music\title1\title1` and `ui\shell\main_menu\main_menu`.                                                                                                                                                                                                                   |
| Some debug sound class helpers are lifted.                                                                    | `src/halo/sound/sound_classes.c` implements `debug_sound_classes_enable`, `debug_sound_classes_set_distances`, and `debug_sound_classes_set_wet`; these are ported in `kb.json`.                                                                                                                                              |

## Mismatches or Unverified Claims

| Imported claim                                                                              | Audit result                                                                                                                                                   |
|---------------------------------------------------------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Campaign tag patches modify pistol/plasma rifle values at runtime or build time.            | Searches in `src/`, `tools/`, and `kb.json` did not find the tag paths or implementation. This is high-value missing Xbox behavior unless proven absent.       |
| Sound cache can hold 512 entries or 64 MiB.                                                 | Xbox code allocates `HALO_SOUND_CACHE_SIZE 0x400000` (4 MiB) in `src/halo/cache/physical_memory_map.c`. Treat `64 MiB` as non-Xbox or unverified until proven. |
| HaloScript thread stack is 1280 bytes.                                                      | Lifted Xbox code currently uses `[thread+0x18, thread+0x218)`, i.e. `0x200` bytes, in `src/halo/hs/hs_runtime.c`. Needs disassembly verification.              |
| HaloScript object-list limits are 48 headers and 128 references.                            | Usage is confirmed, but allocator `FUN_000ce150` is unported in `kb.json`; limits are not yet source-confirmed.                                                |
| Full engine-referenced tag path list is present in source.                                  | Only a small subset was found in lifted source. A direct XBE string audit is needed.                                                                           |
| Collision, physics gravity, renderer PVS/wireframe, sound-cache/channel globals are mapped. | Most are not yet found as lifted implementations or named globals. Treat imported names as search targets, not confirmed mappings.                             |

## PC/H1A-Only or Non-Xbox Claims to Avoid

- H1A-expanded script limits: `32767` syntax nodes, `1024` scripts, `512` globals, `512` references, `16` source files, and larger string/source data.
- H1A scenario flag `do not apply bungie campaign tag patches`.
- PC/H1CE resource-map behavior involving `bitmaps.map`, `sounds.map`, or `loc.map`; the imported maps page explicitly says H1X does not use resource maps.
- H1A map/tag addresses such as `0x40448000`, separate H1A BSP locations, and 64 MiB tag space.
- Gearbox/H1A renderer details involving DirectX 9/11, MCC fixes, Chimera behavior, and PC renderer regressions unless used only as contrast.
- MCC/H1A client-side hit detection based on weapon paths.
- H1A/EK-specific static-script parameters and extended Tool behavior.

## Follow-Up Audit Items

- Reverse/audit `FUN_000ce150` for HaloScript object-list allocator limits (`48` and `128`).
- Locate original runtime tag-patch code for pistol/plasma rifle, or prove it absent in Xbox retail.
- Verify HaloScript thread stack size against disassembly and reconcile `0x200` source evidence with the imported `1280`-byte claim.
- Build a focused engine tag-path audit from XBE strings, not only lifted source.
- Audit unported debug globals/functions for collision, physics gravity, sound cache/channel state, renderer wireframe, and PVS controls.
- Continue datum-limit mapping for unlifted tables: objects, cluster refs, lights, glows, antennas, device groups, and mounted weapon units.

## Provenance

Generated from a local repository audit against imported Reclaimers-derived docs on 2026-06-25. This file is original audit commentary for this repository; Reclaimers-derived facts referenced here remain under the attribution and license notes in `docs/references/h1/README.md`.
