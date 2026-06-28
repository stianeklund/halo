# Halo 1 Reference: Known issues

## Source Page

- Source: `https://c20.reclaimers.net/h1/tags/shader/shader_transparent_glass/`
- Local path: `tags/shader/shader_transparent_glass/readme.md`
- Scope: Reclaimers-derived Halo 1 reference content retained for local QMD search; verify Xbox runtime behavior against the binary before using as lift evidence.

## Content

**Glass transparent shaders** are used for environmental and model glass surfaces. This shader type is characterized by its various reflectivity and tint settings which let you control how the glass appears from different angles and how light passes through it.

### Known issues

In Custom Edition, glass shaders which use bump-mapped reflections render incorrectly. Instead of using the intended reflection cube map, the renderer uses the _vector normalization_ bitmap from globals, causing reflections to be brightly multicoloured. This issue is fixed in H1A.

|  | Function/global | Type |
| --- | --- | --- |
|  | `(rasterizer_active_camouflage_multipass [boolean])` Toggles whether or not transparent shaders are shown through active camouflage. If disabled, shaders like glass or lights will not be visible through a camouflaged unit. | Global |
|  | `(rasterizer_model_transparents [boolean])` Toggles the rendering of transparent shaders in models. For example, the Warthog's windshield. | Global |

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| shader transparent glass flags | `bitfield` |  |
| | Flag | Mask | Comments | | --- | --- | --- | | alpha tested | `0x1` | | | decal | `0x2` | | | two sided | `0x4` | | | bump map is specular mask | `0x8` | | |
| background tint color | `ColorRGB` | * Default: 1,1,1 |
| background tint map scale | `float` | * Default: 1 |
| background tint map | `TagDependency`: bitmap |  |
| reflection type | `enum` |  |
| | Option | Value | Comments | | --- | --- | --- | | bumped cube map | `0x0` | | | flat cube map | `0x1` | | | dynamic mirror | `0x2` | | |
| perpendicular brightness | `float` | * Min: 0 * Max: 1 |
| perpendicular tint color | `ColorRGB` |  |
| parallel brightness | `float` | * Min: 0 * Max: 1 |
| parallel tint color | `ColorRGB` |  |
| reflection map | `TagDependency`: bitmap |  |
| bump map scale | `float` | * Default: 1 |
| bump map | `TagDependency`: bitmap |  |
| diffuse map scale | `float` | * Default: 1 |
| diffuse map | `TagDependency`: bitmap |  |
| diffuse detail map scale | `float` | * Default: 1 |
| diffuse detail map | `TagDependency`: bitmap |  |
| specular map scale | `float` | * Default: 1 |
| specular map | `TagDependency`: bitmap |  |
| specular detail map scale | `float` | * Default: 1 |
| specular detail map | `TagDependency`: bitmap |  |

### Acknowledgements

Thanks to the following individuals for their research or contributions to this topic:

*   MosesOfEgypt _(Tag structure research)_
*   SnowyMouse _(Invader tag definitions)_

## Provenance

Adapted from the Reclaimers Library Halo 1 page `https://c20.reclaimers.net/h1/tags/shader/shader_transparent_glass/`. Retrieved 2026-06-25. Text is retained locally as a cleaned reference extract for search and reverse-engineering context. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
