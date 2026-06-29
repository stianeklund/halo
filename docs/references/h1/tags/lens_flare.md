# Halo 1 Tag: lens_flare

## Summary

1.   By a light tag which may be attached to an object or effect. Lens flares belonging to attached lights on objects can be scaled by a function. 2.   By a sky light, used for the cinematic looking camera lens flares on the sun. 3.   By shaders") which generate BSP lens flares into a level for static light sources.

## Tag Information

Tag group: `lens_flare`.

HaloScript entries:

- `rasterizer_lens_flares [boolean]`
- `rasterizer_lens_flares_occlusion [boolean]`
- `rasterizer_lens_flares_occlusion_debug [boolean]`
- `rasterizer_ray_of_buddha [boolean]`

Defined fields and enum values mentioned:

- bitmap
- cos cutoff angle
- cos falloff angle
- cutoff angle
- falloff angle
- far fade distance
- flags
- flags no occlusion test
- flags sun
- horizontal scale
- lights
- near fade distance
- occlusion offset direction
- occlusion offset direction marker forward
- occlusion offset direction none
- occlusion offset direction toward viewer
- occlusion radius
- rasterizer data glow
- reflections
- reflections animation function
- reflections animation function cosine
- reflections animation function cosine variable period
- reflections animation function diagonal wave
- reflections animation function diagonal wave variable period
- reflections animation function jitter
- reflections animation function noise
- reflections animation function one
- reflections animation function slide
- reflections animation function slide variable period
- reflections animation function spark
- reflections animation function wander
- reflections animation function zero
- reflections animation period
- reflections animation phase
- reflections bitmap index
- reflections brightness
- reflections brightness max
- reflections brightness min
- reflections brightness scaled by
- reflections color lower bound
- reflections color upper bound
- reflections flags
- reflections flags align rotation with screen center
- reflections flags occluded by solid objects
- reflections flags radius not scaled by distance
- reflections flags radius scaled by occlusion factor
- reflections more flags
- reflections more flags interpolate colors in hsv
- reflections more flags more colors
- reflections position
- reflections radius
- reflections radius max
- reflections radius min
- reflections radius scaled by
- reflections radius scaled by distance from center
- reflections radius scaled by none
- reflections radius scaled by rotation
- reflections radius scaled by rotation and strafing
- reflections rotation offset
- reflections tint color
- rotation function
- rotation function none
- rotation function rotation a
- rotation function rotation b
- rotation function rotation translation
- rotation function scale
- rotation function translation
- vertical scale

## Details

**Lens flares** are a visual effect used to emulate glowing light sources, glare, and camera lens flares. They can be used in 3 ways:

1. By a light tag which may be attached to an object or effect. Lens flares belonging to attached lights on objects can be scaled by a function.
2. By a sky light, used for the cinematic looking camera lens flares on the sun.
3. By shaders") which generate BSP lens flares into a level for static light sources.

Lens flares usually contain one or multiple _reflections_, which are indices of a bitmap rendered along a _flare axis_ which passes through the flare and the center of the screen.

### Ray of Buddha

_Ray of Buddha_ shining through an alpha-tested texture

The "Ray of Buddha" effect is a glowing disk of light that simulates light beams when obstructed by the BSP and objects within it (even by alpha-tested transparent textures like foliage). This effect will appear if either the _sun_ flag is set or _occlusion radius_ is set to `50`.

Halo supports multiple Rays of Buddha drawing simultaneously, seemingly for as many lens flares can render. However, it does not support distance-based fading and will simply pop in or out at the tag's _far fade distance_ if set.

The appearance of Ray of Buddha is partially hard-coded. You can change its radius using _occlusion radius_. The shape and fade of the flare are controlled by the globals _glow_ bitmap. Its yellowish tint is hard-coded in the `sun_glow_draw.psh` FX shader.

|  | Function/global | Type |
| --- | --- | --- |
|  | `(rasterizer_lens_flares [boolean])` Toggles rendering of all lens flares. | Global |
|  | `(rasterizer_lens_flares_occlusion [boolean])` Toggles lens flare occlusion. If set to `false`, lens flares will no longer be occluded and stay visible even through objects in the foreground. | Global |
|  | `(rasterizer_lens_flares_occlusion_debug [boolean])` Displays red squares over lens flares in the environment. How much the square is occluded by other geometry or the view frustrum is how much the lens flare fades out. The size of the square relates to the _occlusion radius_. | Global |
|  | `(rasterizer_ray_of_buddha [boolean])` Toggles the lens flare "god rays" effect, present on sky lights or lens flares explicitly set to _sun_. | Global |

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| falloff angle | `float` |  |
| cutoff angle | `float` |  |
| cos falloff angle | `float` | * Cache only |
| cos cutoff angle | `float` | * Cache only |
| occlusion radius | `float` | * Unit: world units Primarily controls the size of the camera-facing occlusion square. The more this square is covered, the more the lens flare fades out. A larger radius covers more screen space and gradually fades in/out while passing behind objects in the foreground, while a small radius will cause the lens flare to quickly appear or disappear. The square can be visualized with `rasterizer_lens_flares_occlusion_debug 1`. This also controls how far the test square is moved depending on the _occlusion offset direction_. This field also interacts with the _Ray of Buddha_ effect, controlling its radius when the _sun_ flag is set. However if this radius is set to exactly `50` then the _sun_ flag will be implied and _Ray of Buddha_ will render too. |
| occlusion offset direction | `enum` | Controls which direction the lens flare is offset. Ignored by the _Ray of Buddha_ effect if this tag's _sun_ flag is set, which always uses a _marker forward_ offset. |
Option values:
- toward viewer (`0x0`): The occlusion test square is moved towards the viewer by the _occlusion radius_. This avoids the test square being always half-embedded if the lens flare is placed directly on a surface. However, this offsets the test square perpendicularly toward the camera's view plane, not toward the camera location itself. This results in test squares near the edges of the screen not aligning with their lens flares, causing them to fade out even while on-screen. Avoid setting this value too high to make this less apparent.
- marker forward (`0x1`): The test square is moved in the "forward" direction of the marker by the _occlusion radius_, usually off the surface in the case of BSP lens flares.
- none (`0x2`): The test square is not offset.
| near fade distance | `float` | * Unit: world units The distance where the lens flare begins to fade out. |
| far fade distance | `float` | * Unit: world units The distance where the lens flare becomes invisible. Distance-based fading is disabled when set to `0`. |
| bitmap | `TagDependency`: bitmap |  |
| flags | `bitfield` |  |
Flag values:
- sun (`0x1`): If set, gives this lens flare a Ray of Buddha effect. This flag is already implied for any lens flare with occlusion radius of `50` (typically used by sky lights).
- no occlusion test (`0x2`): If enabled, skips checking if this lens flare is occluded. It will render regardless of any obstructions in the way like BSP. This also prevents reflections from fading out when the flare's origin goes off-screen.
| rotation function | `enum` |  |
Option values:
- none (`0x0`)
- rotation a (`0x1`)
- rotation b (`0x2`)
- rotation translation (`0x3`)
- translation (`0x4`)
| rotation function scale | `float` | * Default: 360 Yes, the default is indeed 360 radians. |
| horizontal scale | `float` | * Default: 1 Scales the lens flare's first reflection horizontally by this factor. Ignored and assumed `1` if this is set to `0`. |
| vertical scale | `float` | * Default: 1 Scales the lens flare's first reflection vertically by this factor. Ignored and assumed `1` if this is set to `0`. |
| reflections | `Block` | * HEK max count: 32 |
Option values:
- flags (`bitfield`)
- align rotation with screen center (`0x1`)
- radius not scaled by distance (`0x2`)
- radius scaled by occlusion factor (`0x4`)
- occluded by solid objects (`0x8`)
- bitmap index (`uint16`)
- position (`float`): * Unit: along flare axis
- rotation offset (`float`)
- radius (`Bounds`): * Unit: world units
- min (`float`)
- max (`float`)
- radius scaled by (`enum`)
- none (`0x0`)
- rotation (`0x1`)
- rotation and strafing (`0x2`)
- distance from center (`0x3`)
- brightness (`Bounds`): * Min: 0 * Max: 1
- min (`float`)
- max (`float`)
- brightness scaled by (`enum`?)
- tint color (`ColorARGB`)
- color lower bound (`ColorARGB`)
- color upper bound (`ColorARGB`)
- more flags (`bitfield`)
- interpolate colors in hsv (`0x1`)
- more colors (`0x2`)
- animation function (`enum`)
- one (`0x0`)
- zero (`0x1`)
- cosine (`0x2`)
- cosine variable period (`0x3`)
- diagonal wave (`0x4`)
- diagonal wave variable period (`0x5`)
- slide (`0x6`)
- slide variable period (`0x7`)
- noise (`0x8`)
- jitter (`0x9`)
- wander (`0xA`)
- spark (`0xB`)
- animation period (`float`): * Unit: world units * Default: 1
- animation phase (`float`): * Unit: world units

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `lens_flare`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
