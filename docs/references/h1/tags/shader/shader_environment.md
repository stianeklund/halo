# Halo 1 Reference: Detail map blending

## Source Page

- Source: `https://c20.reclaimers.net/h1/tags/shader/shader_environment/`
- Local path: `tags/shader/shader_environment/readme.md`
- Scope: Reclaimers-derived Halo 1 reference content retained for local QMD search; verify Xbox runtime behavior against the binary before using as lift evidence.

## Content

**shader_ environment** is intended for opaque and alpha-tested surfaces and is typically used for the majority of shaders in level BSPs, though it can also be used on gbxmodels. A key feature of this shader is its ability to blend between two detail maps, making it ideal for outdoor ground shaders. It also supports cube maps, bump maps, detail maps, and masked specularity.

### Detail map blending

When this shader's type is set to _blended_ or _blended base specular_, the base map's alpha channel will be used as a blending mask between the primary and secondary detail maps.

This allows you to use two detail maps varyingly across a surface and is typically used for ground shaders which need both grassy and dirt areas. In this case the base map provides the general colouring across the ground of the level, while the detail maps tile and provide fine-level detail. There are a few options for how the detail maps blend with the base map.

### Alpha testing

When the _alpha tested_ flag is checked, the bump map's alpha channel can be used as a kind of transparency mask. The shader will either be fully opaque or fully transparent depending on if the alpha value is lighter or darker than 50% gray.

You should use this feature for transparent shaders that need to maintain the appearance of sharp edges regardless of distance and where semi-transparency is not needed. Some examples of this include the 2D "billboard" trees outside _Timberland_, the floor grates in _Derelict_, or foliage textures on scenery.

Depending on the texture, you may find that at a distance small transparent or opaque details become lost, such as a chain link fence becoming totally invisible. This is the result of mipmapping in the bump map since the alpha chanel is "blurring" as it becomes smaller and details are being lost. It can be important to tune the bump map's mipmap generation to avoid this:

*   Limiting _mipmap count_ results in more aliasing but preserves the impression of fine detail.
*   Tuning the _alpha bias_ lightens or darkens the alpha channel in mipmaps to result in more or less pixels passing the 50% alpha test.

### Bump maps

_Bump maps_ are special textures that encode the "bumpiness" of the surfaces they map to. They are used to represent fine details like cracks and grooves that affect specular reflections and lighting to make the surface look more geometrically detailed than it actually is.

Artists can create them as simple grayscale height maps in TIFF format, and once compiled by Tool or invader-bitmap into a bitmap tag with "height map" usage, they are represented as standard normal maps for use in-engine. Halo CE does not use height maps and doesn't support tessellation or parallax occlusion. The alpha channel of the bump map is unused unless the shader is _alpha tested_.

The lighting effect of bump maps can be seen under both dynamic lights and static lightmaps (sometimes called _environmental bump maps_), with the latter only natively supported in H1A and H1X unless using the CEnshine shader port for H1PC/H1CE. Environmental bump mapping uses incoming light directions which have been precalculated and stored per-vertex during radiosity.

### Invalid bump maps

Artists should ensure that the bump map referenced by a shader is a valid normal map. Don't forget to set the bitmap's usage to _height map_ if you're using tool to import a greyscale height map. Also, don't simply reuse a diffuse or a multipurpose map for a bump map. Failure to use a valid normal map will result in the surface appearing extremely dark or black.

Modders who are porting older Custom Edition maps to MCC may find that existing shaders have this problem, since environmental bump mapping was unsupported in H1CE and the original mappers would not have seen this darkening.

### Shading artifacts

Left: default stock shaders in _Chiron TL-34_ and _The Silent Cartographer_. Right: The same shaders with the alternate bump mapping flag enabled.

By default, environmental bump mapping is rendered by darkening surfaces based on the dot product (angle difference) between incoming light and the bump map. However, in some locations and lighting setups this can result in strange triangular shading artifacts that _look like_ bad smoothing despite level geometry having the intended normals:

*   Where small or point-like light sources are very close to surfaces.
*   Where sharp shadows should be, but the area has either low geometric complexity and/or uses shaders with low radiosity detail levels.

The artifact could be considered a problem with the legacy lighting model; the baked lightmap already accounts for diffuse attenuation and it shouldn't be doubly applied. It is made worse by the limited resolution of both the intermediate lightmap mesh and baked lightmap texture which results in light bleeding, and how per-vertex incident radiosity vectors cannot represent quickly changing light directions across a surface.

You can set the new _alternate bump attenuation_ flag to use a different bump mapping method (similar to Halo 2's) which removes these artifacts, but comes at the cost of possibly overexposing highlights from coloured lights and generally lightening surfaces. Using the flag or not is an artistic choice.

Custom Edition mappers never encountered this artifact due to CE's broken bump mapping. If you're porting a map from CE to MCC you may find that this this artifact is now noticeable. The new flag can be a quick fix to better maintain the original map's appearance and, if applied to all shaders, will generally brighten up a map and make it look more like CE.

If you want to avoid this artifact _without_ using the flag because you prefer the classic look for a shader, here are some workarounds:

*   Use high radiosity detail level for affected shader(s) if you aren't already.
*   Tesselate surfaces where sharp shadows lie, especially where shadow umbras and penumbras would lie. This limits light bleeding by forcing the lightmapper to resolve a higher level of detail than it normally would based on quality settings, and results in more incident radiosity vectors being stored to better match the baked lighting texture, but has the cost of increasing triangle count.
*   Replace nearby point light sources with large diffuse invisible light casting surfaces that avoid sharp shadows.
*   Avoid putting scenery lights too close to surfaces or in locations that would cast sharp shadows.

### Use by gbxmodels

When a gbxmodel references this shader type it will not render correctly in H1CE due to renderer bugs. Specular masking and tinting don't work and sky fog does not render over it. Some affected scenery include the teleporter base and human barricades. It is not recommended to use this shader type for custom objects when targeting Custom Edition, but it is safe to use in H1A.

|  | Function/global | Type |
| --- | --- | --- |
|  | `(rasterizer_environment_alpha_testing [boolean])` Toggles alpha testing for BSP shader_ environment. These shaders are rendered opaquely when disabled. | Global |
|  | `(rasterizer_environment_reflection_mirrors [boolean])` Toggles the rendering of dynamic mirrors. | Global |

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| shader environment flags | `bitfield` |  |
| | Flag | Mask | Comments | | --- | --- | --- | | alpha tested | `0x1` | Causes the shader to become _alpha tested_, using the bump map's alpha channel as the transparency mask. Where the alpha channel is darker than 50% grey this shader will not be drawn, and otherwise is fully opaque. This is commonly used for foliage and grates. Note that these surfaces will still appear fully opaque to lightmaps and completely block light unless marked render-only (`!`) to cast no shadows. | | bump map is specular mask | `0x2` | | | true atmospheric fog | `0x4` | | | use alternate bump attenuation | `0x8` | Causes the shader to use an alternate shading method for bump maps which prevents certain artifacts from appearing near light sources close to surfaces and near shadow edges in geometrically sparse regions, at the cost of sometimes having over-exposed bump map highlights. | |
| shader environment type | `enum` |  |
| | Option | Value | Comments | | --- | --- | --- | | normal | `0x0` | | | blended | `0x1` | | | blended base specular | `0x2` | | |
| lens flare spacing | `float` | Determines how far apart BSP lens flares are generated, in world units, along a surface part using this shader. If set to `0`, a single lens flare will be created at the center of the surface. |
| lens flare | `TagDependency`: lens_flare | References the lens flare to be rendered at the generated lens flare markers. If empty, no lens flares will be generated. |
| diffuse flags | `bitfield` |  |
| | Flag | Mask | Comments | | --- | --- | --- | | rescale detail maps | `0x1` | | | rescale bump map | `0x2` | | |
| base map | `TagDependency`: bitmap |  |
| detail map function | `enum` | This controls how the detail map is blended with the base map. |
| | Option | Value | Comments | | --- | --- | --- | | double biased multiply | `0x0` | The detail colour is multiplied with the base colour and multipied by 2, then clamped to 0-1. Where detail is masked, this has no effect. Unlike the S-shaped response curve of Photoshop's _Overlay_ blending mode, this function continues to brighten exponentially and quickly saturates when the detail map is lighter than 50% gray. ``` detail = lerp(0.5, detail.rgb, detail_mask); result = saturate(base.rgb * detail * 2.0); ``` | | multiply | `0x1` | The detail map is simply multiplied with the base colour, generally darkening the result. Where detail is masked, this has no effect. ``` detail = lerp(1.0, detail.rgb, detail_mask); result = saturate(base.rgb * detail); ``` | | double biased add | `0x2` | Has the effect of adding or subtracting the detail map to/from the base map. Where detail is masked or 50% gray, this has no effect. ``` detail = lerp(0.5, detail.rgb, detail_mask); biased_detail = 2.0 * detail - 1.0; result = saturate(base.rgb + biased_detail); ``` | |
| primary detail map scale | `float` | * Default: 1 |
| primary detail map | `TagDependency`: bitmap |  |
| secondary detail map scale | `float` | * Default: 1 |
| secondary detail map | `TagDependency`: bitmap |  |
| micro detail map function | `enum`? |  |
| micro detail map scale | `float` | * Default: 1 |
| micro detail map | `TagDependency`: bitmap |  |
| material color | `ColorRGB` |  |
| bump map scale | `float` | * Default: 1 |
| bump map | `TagDependency`: bitmap |  |
| bump map scale xy | `Point2D` | * Cache only |
| | Field | Type | Comments | | --- | --- | --- | | x | `float` | | | y | `float` | | |
| u animation function | `enum` |  |
| | Option | Value | Comments | | --- | --- | --- | | one | `0x0` | | | zero | `0x1` | | | cosine | `0x2` | | | cosine variable period | `0x3` | | | diagonal wave | `0x4` | | | diagonal wave variable period | `0x5` | | | slide | `0x6` | | | slide variable period | `0x7` | | | noise | `0x8` | | | jitter | `0x9` | | | wander | `0xA` | | | spark | `0xB` | | |
| u animation period | `float` | * Unit: seconds * Default: 1 |
| u animation scale | `float` | * Unit: base map repeats * Default: 1 |
| v animation function | `enum`? |  |
| v animation period | `float` | * Unit: seconds * Default: 1 |
| v animation scale | `float` | * Unit: base map repeats * Default: 1 |
| self illumination flags | `bitfield` |  |
| | Flag | Mask | Comments | | --- | --- | --- | | unfiltered | `0x1` | Texture sampling is unfiltered, resulting in aliasing at a distance and a blocky pixelated look up close. | |
| primary on color | `ColorRGB` |  |
| primary off color | `ColorRGB` |  |
| primary animation function | `enum`? |  |
| primary animation period | `float` | * Unit: seconds * Default: 1 |
| primary animation phase | `float` | * Unit: seconds |
| secondary on color | `ColorRGB` |  |
| secondary off color | `ColorRGB` |  |
| secondary animation function | `enum`? |  |
| secondary animation period | `float` | * Unit: seconds * Default: 1 |
| secondary animation phase | `float` | * Unit: seconds |
| plasma on color | `ColorRGB` |  |
| plasma off color | `ColorRGB` |  |
| plasma animation function | `enum`? |  |
| plasma animation period | `float` | * Unit: seconds * Default: 1 |
| plasma animation phase | `float` | * Unit: seconds |
| map scale | `float` | * Default: 1 |
| map | `TagDependency`: bitmap |  |
| specular flags | `bitfield` |  |
| | Flag | Mask | Comments | | --- | --- | --- | | overbright | `0x1` | | | extra shiny | `0x2` | | | lightmap is specular | `0x4` | | |
| brightness | `float` | * Min: 0 * Max: 1 |
| perpendicular color | `ColorRGB` |  |
| parallel color | `ColorRGB` |  |
| reflection flags | `bitfield` |  |
| | Flag | Mask | Comments | | --- | --- | --- | | dynamic mirror | `0x1` | | |
| reflection type | `enum` |  |
| | Option | Value | Comments | | --- | --- | --- | | bumped cube map | `0x0` | | | flat cube map | `0x1` | | | bumped radiosity | `0x2` | | |
| lightmap brightness scale | `float` | * Min: 0 * Max: 1 |
| perpendicular brightness | `float` | * Min: 0 * Max: 1 |
| parallel brightness | `float` | * Min: 0 * Max: 1 |
| reflection cube map | `TagDependency`: bitmap |  |

### Acknowledgements

Thanks to the following individuals for their research or contributions to this topic:

*   Conscars _(Notes on bump mapping and alpha testing)_
*   MosesOfEgypt _(Tag structure research)_
*   SnowyMouse _(Invader tag definitions)_

## Provenance

Adapted from the Reclaimers Library Halo 1 page `https://c20.reclaimers.net/h1/tags/shader/shader_environment/`. Retrieved 2026-06-25. Text is retained locally as a cleaned reference extract for search and reverse-engineering context. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
