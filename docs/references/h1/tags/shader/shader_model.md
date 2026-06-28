# Halo 1 Reference: Base map

## Source Page

- Source: `https://c20.reclaimers.net/h1/tags/shader/shader_model/`
- Local path: `tags/shader/shader_model/readme.md`
- Scope: Reclaimers-derived Halo 1 reference content retained for local QMD search; verify Xbox runtime behavior against the binary before using as lift evidence.

## Content

The **shader_ model** tag is used for opaque materials on object models. It supports features like map animation, detail maps, specularity, self-illumination, and color change (e.g. for armour ranks/teams).

Singleplayer units have their colors set in their actor_ variant tags. In multiplayer, players' armor colors are hard-coded.

### Base map

The Chief's base map, with alpha shown in the top right.

The _base map_ controls the diffuse color and transparency of the shader. The **RGB** diffuse color is multiplied with the color change source if used, masked by the multipurpose map's color change mask. The base map's **alpha** channel is a transparency mask.

### Multipurpose map

The _multipurpose map_ is an optional bitmap whose individual channels provide greyscale masks for color change, reflections, self illumination, and detail maps (a technique called _channel packing_). The purpose of each channel depends on the game version:

### Gearbox

The Chief's multipurpose map with alpha shown in the top right (Gearbox channel order).

When Gearbox ported Halo to PC, the channels were reordered for an unknown reason. This is also true for all Gearbox-derived ports like H1A unless the OG Xbox channel order flag is set. Because of this change in shader behaviour, multipurpose map tags differ between H1X and Gearbox.

If you're using the legacy HEK and H1CE, don't pay attention to Guerilla's channel usage description when editing this tag. It describes Xbox channel order only which is incorrect. In H1A Guerilla the channel order is corrected described.

*   **Red:** is an auxiliary mask. It can mask the detail map if the detail mask is set to _multipurpose map alpha_. Despite the option saying "alpha" in Guerilla it really means the red channel in this context.
*   **Green:** Masks self-illumination, used for lights on the model. The self-illumination is added to diffuse light and _then_ multiplied with diffuse color, rather than being added _after_. This means pure black areas of the diffuse map cannot have self-illumination.
*   **Blue:** Masks cube map specular reflections. Pure blue is highest specularity, while black is none.
*   **Alpha:** Masks color change, such as for armour ranks/teams. Color sources include the actor_ variant, multiplayer colors, and object").

It is a common misconception that multipurpose maps need to be purple due to some stock tags having an identical red and blue channel. However, it is not necessary to have any red channel information if you do not require detail map masking or another channel can serve as the detail map mask.

### Xbox

Channel order is different on the classic Xbox version of the game. Guerilla correctly describes multipurpose maps extracted from Xbox maps:

*   **Red:** Specular reflection mask (modulates reflections)
*   **Green:** Self-illumination mask (adds to diffuse light)
*   **Blue:** Primary change-color mask (recolors diffuse map)
*   **Alpha:** Auxiliary mask

### Change color

An animation showing how color change is applied (Gearbox channel order).

The _change color_ feature allows parts of the shader to be recolored at runtime for random variation and different ranks without requiring a different bitmap for each color.

The color from the _change color source_ gets multiplied against the base map, using the color change mask from the multipurpose map. The channel of the mask depends on the channel order (Gearbox vs Xbox).

Change color sources originate from either the object tag's _change colors_ block") or are overriden by actor_ variant colors. They can also come from hard-coded multiplayer armor colors.

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| shader model flags | `bitfield` |  |
| | Flag | Mask | Comments | | --- | --- | --- | | detail after reflection | `0x1` | If enabled, detail maps should be applied _over_ specular reflections. This flag does not work as intended in the Gearbox renderer but can be worked around with Chimera or CEnshine. It is fixed in H1A. | | two sided | `0x2` | | | not alpha tested | `0x4` | | | alpha blended decal | `0x8` | | | true atmospheric fog | `0x10` | | | disable two sided culling | `0x20` | | | multipurpose map uses og xbox channel order | `0x40` | * H1A only If enabled, the multipurpose map will use Xbox channel order instead of Gearbox order. | |
| translucency | `float` |  |
| change color source | `enum` |  |
| | Option | Value | Comments | | --- | --- | --- | | none | `0x0` | | | a | `0x1` | | | b | `0x2` | | | c | `0x3` | | | d | `0x4` | | |
| shader model more flags | `bitfield` |  |
| | Flag | Mask | Comments | | --- | --- | --- | | no random phase | `0x1` | | |
| color source | `enum`? |  |
| animation function | `enum` |  |
| | Option | Value | Comments | | --- | --- | --- | | one | `0x0` | | | zero | `0x1` | | | cosine | `0x2` | | | cosine variable period | `0x3` | | | diagonal wave | `0x4` | | | diagonal wave variable period | `0x5` | | | slide | `0x6` | | | slide variable period | `0x7` | | | noise | `0x8` | | | jitter | `0x9` | | | wander | `0xA` | | | spark | `0xB` | | |
| animation period | `float` | * Unit: seconds * Default: 1 |
| animation color lower bound | `ColorRGB` |  |
| animation color upper bound | `ColorRGB` |  |
| map u scale | `float` | * Default: 1 |
| map v scale | `float` | * Default: 1 |
| base map | `TagDependency`: bitmap | A bitmap which provides diffuse colour and transparency information to the shader. See details. |
| multipurpose map | `TagDependency`: bitmap | A bitmap which provides masks for reflection, self-illumination, colour change, and detail maps. See details. |
| detail function | `enum` |  |
| | Option | Value | Comments | | --- | --- | --- | | double biased multiply | `0x0` | The detail colour is multiplied with the base colour and multipied by 2, then clamped to 0-1. Where detail is masked, this has no effect. Unlike the S-shaped response curve of Photoshop's _Overlay_ blending mode, this function continues to brighten exponentially and quickly saturates when the detail map is lighter than 50% gray. ``` detail = lerp(0.5, detail.rgb, detail_mask); result = saturate(base.rgb * detail * 2.0); ``` | | multiply | `0x1` | The detail map is simply multiplied with the base colour, generally darkening the result. Where detail is masked, this has no effect. ``` detail = lerp(1.0, detail.rgb, detail_mask); result = saturate(base.rgb * detail); ``` | | double biased add | `0x2` | Has the effect of adding or subtracting the detail map to/from the base map. Where detail is masked or 50% gray, this has no effect. ``` detail = lerp(0.5, detail.rgb, detail_mask); biased_detail = 2.0 * detail - 1.0; result = saturate(base.rgb + biased_detail); ``` | |
| detail mask | `enum` | Determines the source of detail map masking. |
| | Option | Value | Comments | | --- | --- | --- | | none | `0x0` | | | reflection mask inverse | `0x1` | | | reflection mask | `0x2` | | | self illumination mask inverse | `0x3` | | | self illumination mask | `0x4` | | | change color mask inverse | `0x5` | | | change color mask | `0x6` | | | auxiliary mask inverse | `0x7` | | | auxiliary mask | `0x8` | Use the "auxiliary mask", which is the alpha channel on Xbox and the **red channel** in Gearbox-derived ports. | |
| detail map scale | `float` | * Default: 1 |
| detail map | `TagDependency`: bitmap |  |
| detail map v scale | `float` | * Default: 1 |
| u animation source | `enum` |  |
| | Option | Value | Comments | | --- | --- | --- | | none | `0x0` | | | a out | `0x1` | | | b out | `0x2` | | | c out | `0x3` | | | d out | `0x4` | | |
| u animation function | `enum`? |  |
| u animation period | `float` | * Unit: seconds * Default: 1 |
| u animation phase | `float` |  |
| u animation scale | `float` | * Unit: repeats * Default: 1 |
| v animation source | `enum`? |  |
| v animation function | `enum`? |  |
| v animation period | `float` | * Unit: seconds * Default: 1 |
| v animation phase | `float` |  |
| v animation scale | `float` | * Unit: repeats * Default: 1 |
| rotation animation source | `enum`? |  |
| rotation animation function | `enum`? |  |
| rotation animation period | `float` | * Unit: seconds * Default: 1 |
| rotation animation phase | `float` |  |
| rotation animation scale | `float` | * Unit: degrees * Default: 360 |
| rotation animation center | `Point2D` |  |
| | Field | Type | Comments | | --- | --- | --- | | x | `float` | | | y | `float` | | |
| reflection falloff distance | `float` | * Unit: world units |
| reflection cutoff distance | `float` | * Unit: world units |
| perpendicular brightness | `float` | * Min: 0 * Max: 1 |
| perpendicular tint color | `ColorRGB` |  |
| parallel brightness | `float` | * Min: 0 * Max: 1 |
| parallel tint color | `ColorRGB` |  |
| reflection cube map | `TagDependency`: bitmap |  |
| unknown | `float` | * Cache only |

### Acknowledgements

Thanks to the following individuals for their research or contributions to this topic:

*   Giraffe _(Animation for change color masking)_
*   Jakey _(Discovering that self-illumination is added to diffuse light, not diffuse color)_
*   MosesOfEgypt _(Tag structure research)_
*   SnowyMouse _(Invader tag definitions)_
*   t3h lag _(Explaining multipurpose map channels)_

## Provenance

Adapted from the Reclaimers Library Halo 1 page `https://c20.reclaimers.net/h1/tags/shader/shader_model/`. Retrieved 2026-06-25. Text is retained locally as a cleaned reference extract for search and reverse-engineering context. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
