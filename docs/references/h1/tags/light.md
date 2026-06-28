# Halo 1 Tag: light

## Summary

...

## Tag Information

Tag group: `light`.

HaloScript entries:

- `debug_lights [boolean]`
- `rasterizer_environment_diffuse_lights [boolean]`

Defined fields and enum values mentioned:

- color
- color lower bound
- color upper bound
- cos cutoff angle
- cos falloff angle
- cutoff angle
- duration
- falloff angle
- falloff function
- falloff function cosine
- falloff function early
- falloff function late
- falloff function linear
- falloff function very early
- falloff function very late
- flags
- flags dont fade active camouflage
- flags dont light own object
- flags dynamic
- flags first person flashlight
- flags no specular
- flags supersize in first person
- intensity
- interpolation flags
- interpolation flags blend in hsv
- interpolation flags more colors
- lens flare
- lens flare only radius
- pitch function
- pitch period
- primary cube map
- radius
- radius modifer
- radius modifer max
- radius modifer min
- roll function
- roll period
- secondary cube map
- sin cutoff angle
- specular radius multiplier
- texture animation function
- texture animation function cosine
- texture animation function cosine variable period
- texture animation function diagonal wave
- texture animation function diagonal wave variable period
- texture animation function jitter
- texture animation function noise
- texture animation function one
- texture animation function slide
- texture animation function slide variable period
- texture animation function spark
- texture animation function wander
- texture animation function zero
- texture animation period
- yaw function
- yaw period

## Details

...

|  | Function/global | Type |
| --- | --- | --- |
|  | `(debug_lights [boolean])` Shows orange and white spheres with the _radius_ of each dynamic light. White seems to show when a light is not yet been activated, such as the Warthog's brake lights until their first use. Lens flare only lights are not shown since their radius is 0. | Global |
|  | `(rasterizer_environment_diffuse_lights [boolean])` Toggles the rendering of _dynamic_ light diffuse illumination on the BSP. Does not affect specular highlights. | Global |

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| flags | `bitfield` |  |
Flag values:
- dynamic (`0x1`)
- no specular (`0x2`)
- don't light own object (`0x4`)
- supersize in first person (`0x8`): Guerilla describes this flag as: "For dynamic lights, light every environment surface if this light is on the gun of the current window". What this means is that the game will render the flashlight shader on all BSP faces in view, rather than just those within the light radius. This does not affect the visual appearance of the light, just how many faces it renders on. It's unclear why this would be beneficial, but it's enabled for the cyborg flashlight. Its effect on rendered dynamic light triangles can be observed with `rasterizer_stats 2`.
- first person flashlight (`0x10`): Causes the flashlight to align with the rotation of the first person weapon's flashlight marker when seen from first person. When this light is seen from third person, such as another player's flashlight, it will be aligned with their armour's integrated flashlight. If the player is holding a weapon without a flashlight marker, like the oddball or flag, or is zoomed in, the first person flashlight will align with armour light.
- don't fade active camouflage (`0x20`)
| radius | `float` | * Default: 1 |
| radius modifer | `Bounds` | * Default: 1,1 |
Field values:
- min (`float`)
- max (`float`)
| falloff angle | `float` | * Default: 3.14159274101257 |
| cutoff angle | `float` | * Default: 3.14159274101257 |
| lens flare only radius | `float` |  |
| cos falloff angle | `float` | * Cache only |
| cos cutoff angle | `float` | * Cache only |
| specular radius multiplier | `float` | * Cache only Not setting this to 2 breaks the first person flashlight. |
| sin cutoff angle | `float` | * Cache only |
| interpolation flags | `bitfield` |  |
Flag values:
- blend in hsv (`0x1`)
- more colors (`0x2`)
| color lower bound | `ColorARGB` |  |
| color upper bound | `ColorARGB` |  |
| primary cube map | `TagDependency`: bitmap |  |
| texture animation function | `enum` |  |
Option values:
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
| texture animation period | `float` | * Unit: seconds |
| secondary cube map | `TagDependency`: bitmap |  |
| yaw function | `enum`? |  |
| yaw period | `float` | * Unit: seconds * Default: 1 |
| roll function | `enum`? |  |
| roll period | `float` | * Unit: seconds * Default: 1 |
| pitch function | `enum`? |  |
| pitch period | `float` | * Unit: seconds * Default: 1 |
| lens flare | `TagDependency`: lens_flare |  |
| intensity | `float` |  |
| color | `ColorRGB` |  |
| duration | `float` | * Unit: seconds |
| falloff function | `enum` |  |
Option values:
- linear (`0x0`)
- early (`0x1`)
- very early (`0x2`)
- late (`0x3`)
- very late (`0x4`)
- cosine (`0x5`)

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `light`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
