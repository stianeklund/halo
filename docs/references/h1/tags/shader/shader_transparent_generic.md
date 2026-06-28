# Halo 1 Reference: Replication using a Chicago shader

## Source Page

- Source: `https://c20.reclaimers.net/h1/tags/shader/shader_transparent_generic/`
- Local path: `tags/shader/shader_transparent_generic/readme.md`
- Scope: Reclaimers-derived Halo 1 reference content retained for local QMD search; verify Xbox runtime behavior against the binary before using as lift evidence.

## Content

The **shader_ transparent_ generic** tag is a powerful transparent shader used for jackal shields, teleporters, control panels, Cortana, and more.

Instances of this tag were replaced with shader_ transparent_ chicago and shader_ transparent_ chicago_ extended when the game was ported to PC, but these shader types cannot fully replicate original appearances. Only the original Xbox version of the game and H1A in MCC support this shader; it is invisible in the Gearbox PC port.

### Replication using a Chicago shader

For H1CE and H1PC, most of these shaders can be accurately recreated using extra layers and/or 4x4 "tint" bitmaps for recoloring the output of the shader.

Some other shaders will need small 256x4 gradient tint bitmaps that are animated to slide back and forth to tint the shaders output a blend of two different colors.

The most complex of the shaders can only be recreated using very large animated bitmaps that have had all their output pre-computed for each frame of the animation, however these can't be timed properly and are inefficient.

The Refined project attempts to recreate classic Xbox visuals by improving transparent shaders using these techniques.

|  | Function/global | Type |
| --- | --- | --- |
|  | `(rasterizer_active_camouflage_multipass [boolean])` Toggles whether or not transparent shaders are shown through active camouflage. If disabled, shaders like glass or lights will not be visible through a camouflaged unit. | Global |
|  | `(rasterizer_model_transparents [boolean])` Toggles the rendering of transparent shaders in models. For example, the Warthog's windshield. | Global |

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| numeric counter limit | `uint8` |  |
| shader transparent generic flags | `bitfield` |  |
| | Flag | Mask | Comments | | --- | --- | --- | | alpha tested | `0x1` | | | decal | `0x2` | Makes the shader render with a slight depth bias toward the camera to prevent Z-fighting. Use this flag for light planes that will be placed at very small offsets from the surface behind them. Avoid using shaders with this flag where they're placed behind greates or embedded within geometric details, as it will sometimes sort incorrectly against them when viewed at a distance. | | two sided | `0x4` | | | first map is in screenspace | `0x8` | | | draw before water | `0x10` | | | ignore effect | `0x20` | | | scale first map with distance | `0x40` | | | numeric | `0x80` | | |
| first map type | `enum` |  |
| | Option | Value | Comments | | --- | --- | --- | | 2d map | `0x0` | | | first map is reflection cube map | `0x1` | | | first map is object centered cube map | `0x2` | | | first map is viewer centered cube map | `0x3` | | |
| framebuffer blend function | `enum` |  |
| | Option | Value | Comments | | --- | --- | --- | | alpha blend | `0x0` | | | multiply | `0x1` | | | double multiply | `0x2` | | | add | `0x3` | | | subtract | `0x4` | | | component min | `0x5` | | | component max | `0x6` | | | alpha multiply add | `0x7` | | |
| framebuffer fade mode | `enum` |  |
| | Option | Value | Comments | | --- | --- | --- | | none | `0x0` | The faces do not fade by viewing angle. | | fade when perpendicular | `0x1` | The faces fade out when viewed edge-on. | | fade when parallel | `0x2` | The faces fade out when viewed flat to the camera. | |
| framebuffer fade source | `enum` |  |
| | Option | Value | Comments | | --- | --- | --- | | none | `0x0` | | | a out | `0x1` | | | b out | `0x2` | | | c out | `0x3` | | | d out | `0x4` | | |
| lens flare spacing | `float` | * Unit: world units |
| lens flare | `TagDependency`: lens_flare |  |
| extra layers | `Block` | * HEK max count: 4 |
| | Field | Type | Comments | | --- | --- | --- | | shader | `TagDependency`: shader | | |
| maps | `Block` | * HEK max count: 4 * Max: 4 |
| | Field | Type | Comments | | --- | --- | --- | | flags | `bitfield` | | | | Flag | Mask | Comments | | --- | --- | --- | | unfiltered | `0x1` | | | u clamped | `0x2` | | | v clamped | `0x4` | | | | map u scale | `float` | | | map v scale | `float` | | | map u offset | `float` | | | map v offset | `float` | | | map rotation | `float` | * Unit: degrees | | mapmap bias | `float` | * Min: 0 * Max: 1 | | map | `TagDependency`: bitmap | | | u animation source | `enum`? | | | u animation function | `enum` | | | | Option | Value | Comments | | --- | --- | --- | | one | `0x0` | | | zero | `0x1` | | | cosine | `0x2` | | | cosine variable period | `0x3` | | | diagonal wave | `0x4` | | | diagonal wave variable period | `0x5` | | | slide | `0x6` | | | slide variable period | `0x7` | | | noise | `0x8` | | | jitter | `0x9` | | | wander | `0xA` | | | spark | `0xB` | | | | u animation period | `float` | * Unit: seconds * Default: 1 | | u animation phase | `float` | | | u animation scale | `float` | * Unit: repeats * Default: 1 | | v animation source | `enum`? | | | v animation function | `enum`? | | | v animation period | `float` | * Unit: seconds * Default: 1 | | v animation phase | `float` | | | v animation scale | `float` | * Unit: repeats * Default: 1 | | rotation animation source | `enum`? | | | rotation animation function | `enum`? | | | rotation animation period | `float` | * Unit: seconds * Default: 1 | | rotation animation phase | `float` | | | rotation animation scale | `float` | * Unit: degrees * Default: 360 | | rotation animation center | `Point2D` | | | | Field | Type | Comments | | --- | --- | --- | | x | `float` | | | y | `float` | | | |
| stages | `Block` | * HEK max count: 7 * Max: 7 |
| | Field | Type | Comments | | --- | --- | --- | | flags | `bitfield` | | | | Flag | Mask | Comments | | --- | --- | --- | | color mux | `0x1` | | | alpha mux | `0x2` | | | a out controls color0 animation | `0x4` | | | | color0 source | `enum` | | | | Option | Value | Comments | | --- | --- | --- | | none | `0x0` | | | a | `0x1` | | | b | `0x2` | | | c | `0x3` | | | d | `0x4` | | | | color0 animation function | `enum`? | | | color0 animation period | `float` | * Unit: seconds * Default: 1 | | color0 animation lower bound | `ColorARGB` | | | color0 animation upper bound | `ColorARGB` | | | color1 | `ColorARGB` | | | input a | `enum` | | | | Option | Value | Comments | | --- | --- | --- | | zero | `0x0` | | | one | `0x1` | | | one half | `0x2` | | | negative one | `0x3` | | | negative one half | `0x4` | | | map color 0 | `0x5` | | | map color 1 | `0x6` | | | map color 2 | `0x7` | | | map color 3 | `0x8` | | | vertex color 0 diffuse light | `0x9` | | | vertex color 1 fade perpendicular | `0xA` | | | scratch color 0 | `0xB` | | | scratch color 1 | `0xC` | | | constant color 0 | `0xD` | | | constant color 1 | `0xE` | | | map alpha 0 | `0xF` | | | map alpha 1 | `0x10` | | | map alpha 2 | `0x11` | | | map alpha 3 | `0x12` | | | vertex alpha 0 fade none | `0x13` | | | vertex alpha 1 fade perpendicular | `0x14` | | | scratch alpha 0 | `0x15` | | | scratch alpha 1 | `0x16` | | | constant alpha 0 | `0x17` | | | constant alpha 1 | `0x18` | | | | input a mapping | `enum` | | | | Option | Value | Comments | | --- | --- | --- | | clamp x | `0x0` | | | 1 clamp x | `0x1` | | | 2 | `0x2` | | | 1 2 | `0x3` | | | clamp x 1 2 | `0x4` | | | 1 2 clamp x | `0x5` | | | x | `0x6` | | | x 1 | `0x7` | | | | input b | `enum`? | | | input b mapping | `enum`? | | | input c | `enum`? | | | input c mapping | `enum`? | | | input d | `enum`? | | | input d mapping | `enum`? | | | output ab | `enum` | | | | Option | Value | Comments | | --- | --- | --- | | alpha discard | `0x0` | | | alpha scratch alpha 0 final alpha | `0x1` | | | alpha scratch alpha 1 | `0x2` | | | alpha vertex alpha 0 fog | `0x3` | | | alpha vertex alpha 1 | `0x4` | | | alpha map alpha 0 | `0x5` | | | alpha map alpha 1 | `0x6` | | | alpha map alpha 2 | `0x7` | | | alpha map alpha 3 | `0x8` | | | | output ab function | `enum` | | | | Option | Value | Comments | | --- | --- | --- | | multiply | `0x0` | | | dot product | `0x1` | | | | output bc | `enum`? | | | output cd function | `enum`? | | | output ab cd mux sum | `enum`? | | | output mapping color | `enum` | | | | Option | Value | Comments | | --- | --- | --- | | color identity | `0x0` | | | color scale by 1 2 | `0x1` | | | color scale by 2 | `0x2` | | | color scale by 4 | `0x3` | | | color bias by 1 2 | `0x4` | | | color expand normal | `0x5` | | | | input a alpha | `enum` | | | | Option | Value | Comments | | --- | --- | --- | | zero | `0x0` | | | one | `0x1` | | | one half | `0x2` | | | negative one | `0x3` | | | negative one half | `0x4` | | | map alpha 0 | `0x5` | | | map alpha 1 | `0x6` | | | map alpha 2 | `0x7` | | | map alpha 3 | `0x8` | | | vertex alpha 0 fade none | `0x9` | | | vertex alpha 1 fade perpendicular | `0xA` | | | scratch alpha 0 | `0xB` | | | scratch alpha 1 | `0xC` | | | constant alpha 0 | `0xD` | | | constant alpha 1 | `0xE` | | | map blue 0 | `0xF` | | | map blue 1 | `0x10` | | | map blue 2 | `0x11` | | | map blue 3 | `0x12` | | | vertex blue 0 blue light | `0x13` | | | vertex blue 1 fade parallel | `0x14` | | | scratch blue 0 | `0x15` | | | scratch blue 1 | `0x16` | | | constant blue 0 | `0x17` | | | constant blue 1 | `0x18` | | | | input a mapping alpha | `enum`? | | | input b alpha | `enum`? | | | input b mapping alpha | `enum`? | | | input c alpha | `enum`? | | | input c mapping alpha | `enum`? | | | input d alpha | `enum`? | | | input d mapping alpha | `enum`? | | | output ab alpha | `enum`? | | | output cd alpha | `enum`? | | | output ab cd mux sum alpha | `enum`? | | | output mapping alpha | `enum`? | | |

### Acknowledgements

Thanks to the following individuals for their research or contributions to this topic:

*   MosesOfEgypt _(Tag structure research)_
*   SnowyMouse _(Invader tag definitions)_

## Provenance

Adapted from the Reclaimers Library Halo 1 page `https://c20.reclaimers.net/h1/tags/shader/shader_transparent_generic/`. Retrieved 2026-06-25. Text is retained locally as a cleaned reference extract for search and reverse-engineering context. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
