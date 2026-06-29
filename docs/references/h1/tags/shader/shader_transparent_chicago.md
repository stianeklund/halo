# Halo 1 Reference: Structure and fields

## Source Page

- Source: `https://c20.reclaimers.net/h1/tags/shader/shader_transparent_chicago/`
- Local path: `tags/shader/shader_transparent_chicago/readme.md`
- Scope: Reclaimers-derived Halo 1 reference content retained for local QMD search; verify Xbox runtime behavior against the binary before using as lift evidence.

## Content

The **transparent chicago shader** is applied to surfaces that require transparent elements. This shader can contain up to 4 "stages" of bitmaps, where each stage and its alpha can animate and blend to form final accumulated diffuse and alpha channels. The shader can be given a final framebuffer blending function too, like _add_ for holograms.

Alpha-tested BSP surfaces like 2D billboard trees and ladders can use the simpler shader_ environment instead.

If referenced as an extra shader layer, this tag will inherit some fields from its parent regardless of its own flags (e.g. _first map type_).

|  | Function/global | Type |
| --- | --- | --- |
|  | `(rasterizer_active_camouflage_multipass [boolean])` Toggles whether or not transparent shaders are shown through active camouflage. If disabled, shaders like glass or lights will not be visible through a camouflaged unit. | Global |
|  | `(rasterizer_model_transparents [boolean])` Toggles the rendering of transparent shaders in models. For example, the Warthog's windshield. | Global |
|  | `(<void> reload_shader_transparent_chicago)` Reloads all shader_ transparent_ chicago tags. Use this in Sapien or Standalone to see changes to their bitmaps or parameters without having to reload the scenario. | Function |

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| numeric counter limit | `uint8` |  |
| shader transparent chicago flags | `bitfield` |  |
| | Flag | Mask | Comments | | --- | --- | --- | | alpha tested | `0x1` | | | decal | `0x2` | Makes the shader render with a slight depth bias toward the camera to prevent Z-fighting. Use this flag for light planes that will be placed at very small offsets from the surface behind them. Avoid using shaders with this flag where they're placed behind greates or embedded within geometric details, as it will sometimes sort incorrectly against them when viewed at a distance. | | two sided | `0x4` | | | first map is in screenspace | `0x8` | | | draw before water | `0x10` | | | ignore effect | `0x20` | | | scale first map with distance | `0x40` | | | numeric | `0x80` | | |
| first map type | `enum` |  |
| | Option | Value | Comments | | --- | --- | --- | | 2d map | `0x0` | | | first map is reflection cube map | `0x1` | | | first map is object centered cube map | `0x2` | | | first map is viewer centered cube map | `0x3` | | |
| framebuffer blend function | `enum` |  |
| | Option | Value | Comments | | --- | --- | --- | | alpha blend | `0x0` | | | multiply | `0x1` | | | double multiply | `0x2` | | | add | `0x3` | | | subtract | `0x4` | | | component min | `0x5` | | | component max | `0x6` | | | alpha multiply add | `0x7` | | |
| framebuffer fade mode | `enum` |  |
| | Option | Value | Comments | | --- | --- | --- | | none | `0x0` | The faces do not fade by viewing angle. | | fade when perpendicular | `0x1` | The faces fade out when viewed edge-on. | | fade when parallel | `0x2` | The faces fade out when viewed flat to the camera. | |
| framebuffer fade source | `enum` |  |
| | Option | Value | Comments | | --- | --- | --- | | none | `0x0` | | | a out | `0x1` | | | b out | `0x2` | | | c out | `0x3` | | | d out | `0x4` | | |
| lens flare spacing | `float` |  |
| lens flare | `TagDependency`: lens_flare |  |
| extra layers | `Block` | * HEK max count: 4 Specifies additional shaders to render _before_ this parent shader. The same geometry is essentially rendered multiple times with different shaders. Each extra layer shader can have an independent _framebuffer blend function_, but some settings like _first map is reflection cube map_ from the parent will also override the setting in extra layers. Although any shader type can be referenced here, only the **chicago transparent shaders** are properly supported; the other types will either not render or render incorrectly. **Warning**: Using a shader with extra layers on an object") with _transparent self occlusion_ enabled may cause Sapien and Standalone to crash if there are too many overlapping faces from the viewing angle. |
| | Field | Type | Comments | | --- | --- | --- | | shader | `TagDependency`: shader | | |
| maps | `Block` | * HEK max count: 4 * Max: 4 Warning: HEK Sapien will crash if this block is empty. The shader is not rendered in H1A Sapien or in-game in this case. |
| | Field | Type | Comments | | --- | --- | --- | | flags | `bitfield` | | | | Flag | Mask | Comments | | --- | --- | --- | | unfiltered | `0x1` | | | alpha replicate | `0x2` | | | u clamped | `0x4` | Texture coordinates outside the U 0-1 range will be clamped to the edges. See _u animation function_ for caveats. | | v clamped | `0x8` | Texture coordinates outside the V 0-1 range will be clamped to the edges. See _u animation function_ for caveats. | | | color function | `enum` | | | | Option | Value | Comments | | --- | --- | --- | | current | `0x0` | | | next map | `0x1` | | | multiply | `0x2` | | | double multiply | `0x3` | | | add | `0x4` | | | add signed current | `0x5` | | | add signed next map | `0x6` | | | subtract current | `0x7` | | | subtract next map | `0x8` | | | blend current alpha | `0x9` | | | blend current alpha inverse | `0xA` | | | blend next map alpha | `0xB` | | | blend next map alpha inverse | `0xC` | | | | alpha function | `enum`? | | | map u scale | `float` | | | map v scale | `float` | | | map u offset | `float` | | | map v offset | `float` | | | map rotation | `float` | | | mipmap bias | `float` | * Min: 0 * Max: 1 | | map | `TagDependency`: bitmap | | | u animation source | `enum`? | | | u animation function | `enum` | * Default: one Horizontal animation function. The u and v animation functions default to 1, which results in effectively an additional _map u offset_ and _map v offset_ of 1. This causes some counterintuitive side effects. The _u clamped_ and _v clamped_ flags will not work properly, stretching the corner pixel across the entire canvas The rotation animation functions will behave unexpectedly as well, with 1 added to the _rotation animation center_ coordinates. It is recommended to set the function to 0 if these features are being used. | | | Option | Value | Comments | | --- | --- | --- | | one | `0x0` | | | zero | `0x1` | | | cosine | `0x2` | | | cosine variable period | `0x3` | | | diagonal wave | `0x4` | | | diagonal wave variable period | `0x5` | | | slide | `0x6` | | | slide variable period | `0x7` | | | noise | `0x8` | | | jitter | `0x9` | | | wander | `0xA` | | | spark | `0xB` | | | | u animation period | `float` | * Unit: seconds * Default: 1 | | u animation phase | `float` | | | u animation scale | `float` | * Unit: world units * Default: 1 | | v animation source | `enum`? | | | v animation function | `enum`? | * Default: one Vertical animation function. See _u animation function_ for caveats. | | v animation period | `float` | * Unit: seconds * Default: 1 | | v animation phase | `float` | | | v animation scale | `float` | * Unit: world units * Default: 1 | | rotation animation source | `enum`? | | | rotation animation function | `enum`? | | | rotation animation period | `float` | * Unit: seconds * Default: 1 | | rotation animation phase | `float` | | | rotation animation scale | `float` | * Unit: degrees * Default: 360 | | rotation animation center | `Point2D` | Coordinates for the center of map rotation. Note that these coordinates may be offset unintuitively if _u animation function_ and _v animation function_ are not _zero_. | | | Field | Type | Comments | | --- | --- | --- | | x | `float` | | | y | `float` | | | |
| extra flags | `bitfield` |  |
| | Flag | Mask | Comments | | --- | --- | --- | | don't fade active camouflage | `0x1` | | | numeric countdown timer | `0x2` | | | custom edition blending | `0x4` | Custom edition originally had an incorrect implementation of the _multiply_ and _double multiply_ framebuffer blend functions for transparent shaders which made fully dark pixels transparent. MCC is fixed but this flag allows modders to use custom edition's blending, which can help when porting legacy tags to the updated engine. | |

### Acknowledgements

Thanks to the following individuals for their research or contributions to this topic:

*   Galap _(Rotation coordinate offset writeup)_
*   MosesOfEgypt _(Research on tag structure research, extra layers crash, rotation coordinate offset)_
*   SnowyMouse _(Invader tag definitions)_

## Provenance

Adapted from the Reclaimers Library Halo 1 page `https://c20.reclaimers.net/h1/tags/shader/shader_transparent_chicago/`. Retrieved 2026-06-25. Text is retained locally as a cleaned reference extract for search and reverse-engineering context. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
