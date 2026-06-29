# Halo 1 Tag: decal

## Summary

`decal` tags use tag group `deca`. They describe flat visual marks such as bullet holes, blood splats, explosion scorch marks, and painted signs. Decals render over scenario BSP surfaces; they do not appear on objects.

Direct tag dependencies include `decal` for chaining and `bitmap` for the decal texture. Known referrers include `decal`, `effect`, and `scenario` tags.

## Quick Answers

| Question | Answer |
| --- | --- |
| Can decal debug visuals be enabled? | Yes. Use `debug_decals 1` for decal numbers and mesh highlighting, and `debug_permanent_decals 1` for yellow bounding spheres around permanent decals. |
| Can decals be globally hidden or disabled? | `rasterizer_environment_decals` toggles display and creation of both permanent and dynamic decals. While false, effects cannot create new decals; prior decals reappear when toggled true again. |
| Can decal pools be reset? | `rasterizer_decals_flush` destroys dynamic and permanent decals. This also removes decorative environment decals, so it is not a safe map optimization strategy. |
| What controls decal surface offset? | `rasterizer_zoffset [real]`, default about `0.003906`. c20 notes the unit is not world units. |
| Do dynamic decals have a slot limit? | Up to 2048 decal slots are documented, plus a separate vertex allocation limit. Large-radius decals can exhaust vertices earlier. |
| Are permanent decals different? | Permanent decals, also called environment decals, are placed in scenarios with Sapien under game data. |

## Runtime Behavior

Dynamic decals are created by effects such as projectile impacts and explosions. Give dynamic decals a finite lifetime to avoid poor framerates and to return decal vertices to the shared pool.

The engine may log this message if decal vertex allocation fails:

```text
### WUT? decals: failed to allocate vertices (locked=37, permanent=4)
```

The `locked` count means decals that have not expired yet. Rough testing suggests the vertex pool is likely `16 * 2048` vertices, and each decal consumes however many vertices are needed to conform to BSP geometry.

Permanent decals are placed in the scenario as environment decoration. They are affected by `debug_permanent_decals`, `rasterizer_environment_decals`, and `rasterizer_decals_flush`.

Chain decals let one decal spawn another decal at the same location. This is useful when combining blend modes, such as a short-lived additive plasma glow with a longer-lived multiply scorch mark. The `geometry inherited by next decal in chain` flag allows the chained decal to reuse the same generated mesh.

## Decal Mesh Generation

Most decals are generated as simple four-sided quads on flat surfaces. If the target area is not flat, the engine can conform the decal to BSP geometry, producing more faces and vertices depending on the decal `type`.

`debug_decals 1` visualizes decal mesh generation. It displays red numbers over dynamic and permanent decals and highlights the most recently created decal mesh. White points are original decal corner vertices. Red points are vertices added during BSP conformation.

Original corner vertices stand off slightly from the background to avoid Z-fighting. This is controlled by `rasterizer_zoffset`. Additional conformation vertices are not necessarily z-offset.

Decals can wrap onto `+sky` faces. They do not wrap onto or past breakable surfaces, even after the surface is broken.

Decals only render when their origin point is in a visible BSP cluster. If a decal wraps into another cluster and its origin cluster is not visible, the decal can disappear abruptly.

## Related HaloScript

| Name | Kind | Effect |
| --- | --- | --- |
| `debug_decals` | global | Displays numbers over dynamic and permanent decals. Also highlights the most recently created decal mesh. |
| `debug_permanent_decals` | global | Toggles yellow bounding spheres around permanent decals. |
| `rasterizer_decals_flush` | function | Destroys all dynamic and permanent decals. |
| `rasterizer_environment_decals` | global | Toggles display and creation of both permanent and dynamic decals. |
| `rasterizer_filthy_decal_fog_hack` | global | Related decal rasterizer global; c20 does not describe behavior. |
| `rasterizer_zoffset [real]` | global | Controls how far new decals are generated away from surfaces. Default is about `0.003906`. |

Repo cross-reference: `docs/debug-commands-keyboard.md` contains the broader HaloScript command reference.

## Fields And Values

| Field | Type | Search notes |
| --- | --- | --- |
| `flags` | bitfield | Controls chain geometry inheritance, color interpolation, random rotation, animation loop, aspect preservation, and tool-specific behavior. |
| `type` | enum | Controls decal geometry generation behavior. |
| `layer` | enum | Controls the render stage for the decal. |
| `next decal in chain` | tag dependency: `decal` | Spawns another decal at the same location. Do not create circular chains. |
| `radius` | bounds, world units | Controls decal scale. Has special behavior for sprite bitmap maps. Values above about 16 world units are capped in-game. |
| `intensity` | bounds, 0 to 1 | Lower and upper visibility bounds. `0,0` behaves as fully visible. |
| `color lower bounds` | RGB color | Lower random tint bound, multiplied with decal color. Defaults to white. |
| `color upper bounds` | RGB color | Upper random tint bound, multiplied with decal color. Defaults to white. |
| `animation loop frame` | `uint16` | Animation loop field. |
| `animation speed` | `uint16` ticks per frame | Defaults to 1. |
| `lifetime` | bounds, seconds | Total decal lifetime. |
| `decay time` | bounds, seconds | Fade-out duration before lifetime expires. Does not extend lifetime. |
| `framebuffer blend function` | enum | Controls blend mode. Common uses include add for plasma, double multiply for bullet holes on metal, and multiply for blood or burns. |
| `map` | tag dependency: `bitmap` | Decal texture. |
| `maximum sprite extent` | float, pixels, read-only | Runtime field. The game sets this to 16 for sprite bitmap maps, causing a long-standing radius scaling bug. |

## Flag Masks

| Flag | Mask | Behavior |
| --- | --- | --- |
| `geometry inherited by next decal in chain` | `0x1` | Chained decal reuses the generated mesh. Useful for aligned composite decals. |
| `interpolate color in hsv` | `0x2` | Random color between lower and upper bounds interpolates in HSV space using the shortest hue distance. |
| `more colors` | `0x4` | With HSV interpolation, chooses the longer hue path. |
| `no random rotation` | `0x8` | Disables random decal rotation. |
| `water effect` | `0x10` | Water-related flag. |
| `sapien snap to axis` | `0x20` | Sapien placement flag. |
| `sapien incremental counter` | `0x40` | Sapien placement flag. |
| `animation loop` | `0x80` | Enables looping animation behavior. |
| `preserve aspect` | `0x100` | Preserves bitmap aspect. |
| `disabled in remastered by blood setting` | `0x200` | Remastered blood-setting behavior. |
| `sprite scale bug fix` | `0x400` | Non-standard Invader-only flag that corrects the sprite map radius scaling bug. |

## Type Values

| Value | Name | Behavior |
| --- | --- | --- |
| `0x0` | `scratch` | Conforms to underlying geometry. |
| `0x1` | `splatter` | No observed difference from `scratch` on c20. |
| `0x2` | `burn` | No observed difference from `scratch` on c20. |
| `0x3` | `painted sign` | Clips at underlying geometry edges instead of wrapping. |

## Layer Values

| Value | Name | Behavior |
| --- | --- | --- |
| `0x0` | `primary` | Draws after environment diffuse texture. |
| `0x1` | `secondary` | Draws after primary decals and can cover them. |
| `0x2` | `light` | Draws after diffuse light but before diffuse textures, allowing lightmap-like behavior. |
| `0x3` | `alpha tested` | Alpha-tested decal layer. |
| `0x4` | `water` | Water decal layer. |

## Blend Values

| Value | Name |
| --- | --- |
| `0x0` | `alpha blend` |
| `0x1` | `multiply` |
| `0x2` | `double multiply` |
| `0x3` | `add` |
| `0x4` | `subtract` |
| `0x5` | `component min` |
| `0x6` | `component max` |
| `0x7` | `alpha multiply add` |

## Reverse-Engineering Relevance

Search terms for code and symbols: `decal`, `decals`, `rasterizer_decals`, `debug_decals`, `debug_permanent_decals`, `rasterizer_environment_decals`, `rasterizer_zoffset`, `decal_surface_add`, `invalid decal type`.

Known repo context from existing notes mentions decal-related asserts and rasterizer decal functions. Treat this behavior as design/context evidence, not binary proof. For lift work, verify behavior against the Xbox binary and runtime state before making code changes.

## Open Questions

The exact meaning of the red number shown by `debug_decals` is not certain. It may be the number of vertices needed if each projected decal region is converted into quads.

`rasterizer_filthy_decal_fog_hack` is listed with decal-related script globals, but its behavior is not described on the decal page.

`splatter` and `burn` decal types have no observed difference from `scratch` in available testing. Binary/runtime verification would be needed before assuming they are equivalent internally.

## Provenance

Adapted from Reclaimers Library Halo 1 tag information for `decal`, `bitmap`, `effect`, `scenario`, and `scenario_structure_bsp`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
