# Halo 1 Reference: Base maps

## Source Page

- Source: `https://c20.reclaimers.net/h1/tags/shader/shader_transparent_water/`
- Local path: `tags/shader/shader_transparent_water/readme.md`
- Scope: Reclaimers-derived Halo 1 reference content retained for local QMD search; verify Xbox runtime behavior against the binary before using as lift evidence.

## Content

**Water shaders** are characterized by their use of layered animated _ripple maps_, tint colours, and reflectance.

They need not exclusively be used for water -- the coolant pools of _Keyes_ (d20) also use water shaders, and the Halo CE Refined project uses them for some glass to better emulate Xbox glass shaders. They are also not exclusively used within the BSP, with skies like _Damnation's_ also using this shader type.

Flowing water with rapids, like waterfalls and rivers, can instead use shader_ transparent_ chicago which allows for more varied animation and map blending.

### Base maps

Water shaders do not tile their base maps. Instead, the edges of the texture continue infinitely ("clamp" texture address mode). This can be useful for concentrating texel density near the level without dedicating texture space to surroundings which stretch to the horizon.

### Known issues

Ripple maps are not rendered correctly in H1PC and H1CE compared to classic Xbox. The highest level of detail mipmap is used for the most distant areas, but water closer to the camera uses the lowest detail mipmap. This is the opposite of how it should be, and results in distant water suffering from major aliasing. The water shader was fixed in H1A.

|  | Function/global | Type |
| --- | --- | --- |
|  | `(rasterizer_water [boolean])` Toggles the rendering of shader_ transparent_ water shaders. | Global |

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| water flags | `bitfield` |  |
| | Flag | Mask | Comments | | --- | --- | --- | | base map alpha modulates reflection | `0x1` | | | base map color modulates background | `0x2` | | | atmospheric fog | `0x4` | | | draw before fog | `0x8` | | |
| base map | `TagDependency`: bitmap |  |
| view perpendicular brightness | `float` | * Min: 0 * Max: 1 |
| view perpendicular tint color | `ColorRGB` |  |
| view parallel brightness | `float` | * Min: 0 * Max: 1 * Default: 1 |
| view parallel tint color | `ColorRGB` |  |
| reflection map | `TagDependency`: bitmap |  |
| ripple animation angle | `float` |  |
| ripple animation velocity | `float` |  |
| ripple scale | `float` | * Default: 1 |
| ripple maps | `TagDependency`: bitmap |  |
| ripple mipmap levels | `uint16` | * Default: 1 |
| ripple mipmap fade factor | `float` | * Min: 0 * Max: 1 |
| ripple mipmap detail bias | `float` |  |
| ripples | `Block` | * HEK max count: 4 * Max: 4 |
| | Field | Type | Comments | | --- | --- | --- | | contribution factor | `float` | * Min: 0 * Max: 1 | | animation angle | `float` | | | animation velocity | `float` | | | map offset | `Vector2D` | | | map repeats | `uint16` | * Default: 1 | | map index | `uint16` | | |

### Acknowledgements

Thanks to the following individuals for their research or contributions to this topic:

*   gbMichelle _(Discovering that base maps do not tile)_
*   MosesOfEgypt _(Tag structure research)_
*   SnowyMouse _(Invader tag definitions)_

## Provenance

Adapted from the Reclaimers Library Halo 1 page `https://c20.reclaimers.net/h1/tags/shader/shader_transparent_water/`. Retrieved 2026-06-25. Text is retained locally as a cleaned reference extract for search and reverse-engineering context. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
