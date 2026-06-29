# Halo 1 Reference: Known issues

## Source Page

- Source: `https://c20.reclaimers.net/h1/tags/shader/shader_transparent_plasma/`
- Local path: `tags/shader/shader_transparent_plasma/readme.md`
- Scope: Reclaimers-derived Halo 1 reference content retained for local QMD search; verify Xbox runtime behavior against the binary before using as lift evidence.

## Content

**Plasma shaders** are used for energy shield effects. They are mostly referenced as the _modifier shader_ of an object") like a biped, though the Sentinel biped instead references it via its gbxmodel (presumably because it uses a custom shape).

### Known issues

The original PC ports of Halo (H1PC and H1CE) include a number of known renderer issues. On some PC hardware plasma shaders render incorrectly:

Notice how some areas of the plasma are cut off.

After the noise maps are sampled and blended, the resuling value is supposed to be mapped to transparent at high and low values and to opaque at midtones. Something about the shader math at this step is platform-dependent and results in half the values mapping to fully transparent instead of a blend.

The workaround is to duplicate the plasma shader and apply it in two layers. The duplicate shader should be identical except for using inverted noise bitmaps. It is **not necessary** to use this workaround in H1A because the shader has been fixed.

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| intensity source | `enum` |  |
| | Option | Value | Comments | | --- | --- | --- | | none | `0x0` | | | a out | `0x1` | | | b out | `0x2` | | | c out | `0x3` | | | d out | `0x4` | | |
| intensity exponent | `float` | * Default: 1 |
| offset source | `enum`? |  |
| offset amount | `float` | * Unit: world units |
| offset exponent | `float` | * Default: 1 |
| perpendicular brightness | `float` | * Min: 0 * Max: 1 |
| perpendicular tint color | `ColorRGB` |  |
| parallel brightness | `float` | * Min: 0 * Max: 1 |
| parallel tint color | `ColorRGB` |  |
| tint color source | `enum` |  |
| | Option | Value | Comments | | --- | --- | --- | | none | `0x0` | | | a | `0x1` | | | b | `0x2` | | | c | `0x3` | | | d | `0x4` | | |
| primary animation period | `float` | * Unit: seconds * Default: 1 |
| primary animation direction | `Vector3D` |  |
| primary noise map scale | `float` |  |
| primary noise map | `TagDependency`: bitmap |  |
| secondary animation period | `float` | * Unit: seconds * Default: 1 |
| secondary animation direction | `Vector3D` |  |
| secondary noise map scale | `float` |  |
| secondary noise map | `TagDependency`: bitmap |  |

### Acknowledgements

Thanks to the following individuals for their research or contributions to this topic:

*   crow _(Renderer issue workaround)_
*   MosesOfEgypt _(Tag structure research)_
*   SnowyMouse _(Invader tag definitions)_

## Provenance

Adapted from the Reclaimers Library Halo 1 page `https://c20.reclaimers.net/h1/tags/shader/shader_transparent_plasma/`. Retrieved 2026-06-25. Text is retained locally as a cleaned reference extract for search and reverse-engineering context. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
