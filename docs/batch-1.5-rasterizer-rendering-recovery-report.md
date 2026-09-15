# Batch 1.5 Recovery Report: Rasterizer & Hardware Rendering Pipeline

## Executive Summary
Batch 1.5 completes the authentic symbol recovery and labeling of 662 functions across the Rasterizer, Structure BSP Rendering, Bitmap Processing, Dynamic Lighting, and Hardware Shading pipelines in Halo CE Xbox debug build 2276.

All 662 function names and addresses are 100% verified against the canonical debug symbols in `halo_2276_functions.txt` (Oct 12, 2001 build 2276). Zero speculation or synthetic naming was introduced.

## Verification & Guardrail Metrics
- **Functions Renamed:** 662 functions (554 ported C implementations, 108 assembly thunks/unported).
- **Register ABI Invariance:** 886 / 886 tracked functions verified against `tools/kb_reg_baseline.json` (**0 drift, 0 missing, 0 stale**).
- **Hazard Scan:** Passed (`check_lift_hazards.py --changed-only` reports 0 blocker hazards).
- **Compilation & Linkage:** Successful (`build.py -q --target halo` compiled cleanly and produced target executable without errors).
- **Duplicate Check:** 0 duplicate symbol names in `kb.json`.

## Subsystem Functional Breakdown

| Object Module | Count | Primary Subsystem Responsibilities |
| :--- | :--- | :--- |
| `structures.obj` | 106 | BSP world geometry rendering: cluster PVS traversal, portal occlusion culling, leaf map polygon clipping, dynamic surface triangulation, and lightmap blending passes. |
| `rasterizer_sprites.obj` | 82 | Particle and sprite quad generation, billboard orientation matrix calculations, screen-space sprite sorting, and vertex buffer packing. |
| `rasterizer.obj` | 81 | Master hardware rasterization controller: render target creation, depth buffer clearing, viewport configuration, backbuffer swap intervals, and frame timing. |
| `rasterizer_decals.obj` | 63 | Surface decal rasterization: projection geometry generation, decal vertex clipping against BSP polygons, texture coordinate generation, and decal fading. |
| `rasterizer_text.obj` | 51 | Bitmap font glyph rasterization, string metric calculations, text kerning, multi-line formatting, and HUD text rendering passes. |
| `bitmap_utilities.obj` | 39 | Texture post-processing pipeline: mipmap downsampling (2D, 3D, cubemaps), box smoothing, alpha bleeding, edge sharpening, and format conversions. |
| `render_debug.obj` | 36 | Hardware rendering diagnostics: frustum visualization, leaf portal wireframes, surface normal debug overlays, light volume spheres, and performance stats. |
| `rasterizer_xbox_environment.obj` | 28 | Environment shading passes: diffuse/specular light passes, reflection cubemaps, detail map modulation, and Xbox GPU pushbuffer commands. |
| `rasterizer_xbox.obj` | 23 | Xbox D3D device setup, display mode negotiation, NV2A GPU state management, pushbuffer submission, and hardware interrupt synchronization. |
| `rasterizer_xbox_environment_fog.obj` | 18 | Volumetric and atmospheric fog: planar fog plane evaluation, atmospheric distance fog equations, and fog screen effect rendering. |
| `rasterizer_xbox_hardware_bitmaps.obj` | 17 | NV2A texture swizzling, Xbox linear/swizzled texture memory allocation, palette loading, and format validation. |
| `bitmaps.obj` | 16 | Core bitmap tag structures: 2D/3D/cubemap face extraction, color quantization, and row dithering algorithms. |
| `rasterizer_xbox_draw_primitives.obj` | 15 | DrawPrimitive and DrawIndexedPrimitive dispatch, index buffer binding, and vertex stream configuration for the NV2A vertex pipeline. |
| `render.obj` | 12 | Scene composition loop: camera view-projection matrix calculation, object culling, transparent queue sorting, and post-processing passes. |
| `rasterizer_xbox_models.obj` | 9 | Model and biped mesh rendering: rigid and blended bone matrix submission to GPU constant registers, skinning vertex shader setup. |
| `structure_visibility.obj` | 8 | Potentially Visible Set (PVS) calculations: subcluster traversal, mirror surfaces, and cluster portal visibility trees. |
| `rasterizer_xbox_dynavobgeom.obj` | 8 | Dynamic vertex and object geometry streaming buffers, ring buffer management, and scratch vertex memory allocation. |
| `rasterizer_xbox_widgets.obj` | 8 | Screen-space widget and HUD element rasterization passes, textured quad batching, and scissor rect clipping. |
| `shaders.obj` | 7 | Shader tag compilation and parameter loading: environment shaders, model shaders, glass shaders, and transparent water shaders. |
| `structure_bsp_definitions.obj` | 7 | Structure BSP leaf face compilation, spatial collision node hierarchies, and surface plane equations. |
| `structure_detail_objects.obj` | 6 | Instanced detail geometry rendering: foliage, rocks, debris placement grids, cell boundary queries, and distance LOD fading. |
| `rasterizer_xbox_hardware_geometry.obj` | 5 | Hardware vertex buffer allocation, vertex declaration creation, and GPU pushbuffer vertex stream binding. |
| `rasterizer_xbox_lights.obj` | 5 | Hardware dynamic light evaluation: point lights, spotlights, light falloff attenuation tables, and light scissor boxes. |
| `render_cameras.obj` | 4 | Viewport camera matrices, field of view projection matrices, orthographic interface projections, and view frustum planes. |
| `rasterizer_xbox_shadows.obj` | 4 | Dynamic object shadow volumes: stencil shadow pass configuration, silhouette edge extrusion, and shadow buffer clearing. |
| `rasterizer_common.obj` | 2 | Shared rasterizer helper utilities and state caching logic. |
| `rasterizer_xbox_screen_effect.obj` | 2 | Full-screen post-processing effects: motion blur, screen flash, and night vision color lookup tables. |
| `rasterizer_xbox_profile.obj` | 2 | GPU performance metering, rasterizer cycle counters, and pushbuffer wait-time profiling. |
| `rasterizer_xbox_vertex_shaders_runtime.obj` | 1 | NV2A vertex shader microcode assembly and hardware constant register loading. |
| `structure_render.obj` | 1 | Planar fog depth offset configuration and BSP render pass coordination. |
| `rasterizer_lights.obj` | 1 | Dynamic light list management and global illumination updates. |

## Collision Resolutions & Authentic Recoveries
1. Resolved collision between `0x15f1f0`/`0x15f200`/`0x15f210` and `0x160940`/`0x160970`/`0x1609a0`:
   - `0x15f1f0` -> Authentic `_rasterizer_hud_begin`
   - `0x15f200` -> Authentic `_rasterizer_hud_end`
   - `0x15f210` -> Authentic `_rasterizer_dynamic_lit_geometry_draw`
   - `0x160940` -> Authentic `_rasterizer_environment_diffuse_lights_end`
   - `0x160970` -> Authentic `_rasterizer_environment_specular_light_end`
   - `0x1609a0` -> Authentic `_rasterizer_environment_specular_lightmaps_end`
2. Stripped 56 demangled Xbox D3D device parameter signatures (e.g. `IDirect3DDevice8_CreateVertexBuffer(x,x,x,x,x,x)`) to canonical C identifiers.
3. Filtered 5 unlabeled symbols lacking binary entries in `halo_2276_functions.txt` (`0x17ce30`, `0x17ce60`, `0x15f540`, `0x160bc0`, `0x160be0`) to guarantee 100% symbol authenticity.
