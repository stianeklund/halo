# Halo 1 Reference: Known issues

## Source Page

- Source: `https://c20.reclaimers.net/h1/tags/fog/`
- Local path: `tags/fog/readme.md`
- Scope: Reclaimers-derived Halo 1 reference content retained for local QMD search; verify Xbox runtime behavior against the binary before using as lift evidence.

## Content

**Fog** tags describe the colour and density properties of fog which can be applied to BSP _fog planes_ using Sapien.

Non-planar atmospheric fog does _not_ use a fog tag, and is instead controlled by the sky tag.

### Known issues

Due to a renderer regression, the _screen layers_ effect of these tags does not render on H1CE or H1PC, nor does it draw over the skybox unless _atmosphere dominant_ is set. This tag renders correctly in H1X and H1A.

As of MCC season 7, there is a new issue where fog does not render at the right density over models and causes them to stand out especially in areas of high fog density.

|  | Function/global | Type |
| --- | --- | --- |
|  | `(rasterizer_environment_fog)` Toggles both environmental sky fog and fog plane colors. Does not affect _fog screen_. Use `rasterizer_fog_plane` or `rasterizer_fog_atmosphere` to individually toggle fog types. | Global |
|  | `(rasterizer_fog_plane [boolean])` Toggles the rendering of fog planes. | Global |

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| flags | `bitfield` |  |
| | Flag | Mask | Comments | | --- | --- | --- | | is water | `0x1` | Determines if the volume behind (anti-normal to) this fog plane is a "water fog volume", which causes it to be considered underwater. This affects vehicles' ability to travel over water. | | atmosphere dominant | `0x2` | As documented in Guerilla, this was originally used to fix polygon popping when atmospheric fog density reached 1.0 within the confines of the map and interacted with fog planes. It is only enabled in c10's swamp fog tag. This flag can be used as a workaround on H1PC and H1CE to fix fog not rendering over the sky, a renderer bug introduced during the PC port. Some Chimera builds automatically flip this flag at runtime. It is **not necessary** to use this flag as a visual workaround in H1A because fog rendering has been fixed. | | fog screen only | `0x4` | Unknown purpose. This flag was not set in any original Halo maps. | |
| maximum density | `float` | * Min: 0 * Max: 1 Scales how dense the fog is at its opaque distance and depth, where `1` is the fully opaque _color_ and `0` is transparent. |
| opaque distance | `float` | * Unit: world units The distance from the player where the fog reaches its _maximum density_. This value must not be negative or zero (causes a crash in pre-H1A versions). |
| opaque depth | `float` | * Unit: world units The distance from the fog plane where the fog reaches its _maximum density_. This value must not be negative or zero (causes a crash in pre-H1A versions). |
| distance to water plane | `float` | * Unit: world units |
| color | `ColorRGB` |  |
| flags 1 | `bitfield` |  |
| | Flag | Mask | Comments | | --- | --- | --- | | no environment multipass | `0x1` | | | no model multipass | `0x2` | | | no texture based falloff | `0x4` | | |
| layer count | `uint16` |  |
| distance gradient | `Bounds` | * Unit: world units |
| | Field | Type | Comments | | --- | --- | --- | | min | `float` | | | max | `float` | | |
| density gradient | `Bounds` | * Min: 0 * Max: 1 |
| | Field | Type | Comments | | --- | --- | --- | | min | `float` | | | max | `float` | | |
| start distance from fog plane | `float` | * Unit: world units |
| screen layers color | `ColorARGBInt` | Sets the colour of the screen layer volumetrics particles. Note that this effect only works in H1X and H1A. |
| RGB Color with alpha, with 8-bit color depth per channel (0-255) | Field | Type | Comments | | --- | --- | --- | | alpha | `uint8` | | | red | `uint8` | | | green | `uint8` | | | blue | `uint8` | | |
| rotation multiplier | `float` | * Min: 0 * Max: 1 |
| strafing multiplier | `float` | * Min: 0 * Max: 1 Scales the parallax movement of the fog screen layer during strafing. |
| zoom multiplier | `float` | * Min: 0 * Max: 1 |
| map scale | `float` |  |
| map | `TagDependency`: bitmap |  |
| animation period | `float` | * Unit: seconds |
| wind velocity | `Bounds`? | * Unit: world units per second |
| wind period | `Bounds`? | * Unit: seconds |
| wind acceleration weight | `float` | * Min: 0 * Max: 1 |
| wind perpendicular weight | `float` | * Min: 0 * Max: 1 |
| background sound | `TagDependency`: sound_looping | Overrides the background sound of the cluster while under the fog plane. This is used for underwater areas. |
| sound environment | `TagDependency`: sound_environment | Overrides the sound environment of the cluster while under the fog plane. This is used for underwater areas. |

### Acknowledgements

Thanks to the following individuals for their research or contributions to this topic:

*   Jakey _(Known fog issues in S7 MCC)_
*   MosesOfEgypt _(Tag structure research)_
*   SnowyMouse _(Invader tag definitions)_

## Provenance

Adapted from the Reclaimers Library Halo 1 page `https://c20.reclaimers.net/h1/tags/fog/`. Retrieved 2026-06-25. Text is retained locally as a cleaned reference extract for search and reverse-engineering context. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
