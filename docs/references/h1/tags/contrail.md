# Halo 1 Tag: contrail

## Summary

Contrails describe the trail effects which commonly follow projectiles, or emit from model markers like the Banshee's wingtips. They reference a bitmap to be rendered at repeated intervals and can be affected by wind and gravity using point_ physics.

## Tag Information

Tag group: `contrail`.

HaloScript entries:

- `render_contrails [boolean]`

Defined fields and enum values mentioned:

- anchor
- anchor with primary
- anchor with screen space
- anchor zsprite
- animation rate
- bitmap
- first sequence index
- flags
- flags edge effect fades slowly
- flags first point unfaded
- flags last point unfaded
- flags points always pinned to ground
- flags points always pinned to media
- flags points start pinned to ground
- flags points start pinned to media
- framebuffer blend function
- framebuffer blend function add
- framebuffer blend function alpha blend
- framebuffer blend function alpha multiply add
- framebuffer blend function component max
- framebuffer blend function component min
- framebuffer blend function double multiply
- framebuffer blend function multiply
- framebuffer blend function subtract
- framebuffer fade mode
- framebuffer fade mode fade when parallel
- framebuffer fade mode fade when perpendicular
- framebuffer fade mode none
- inherited velocity fraction
- map flags
- map flags unfiltered
- point generation rate
- point states
- point states color lower bound
- point states color upper bound
- point states duration
- point states physics
- point states scale flags
- point states scale flags color
- point states scale flags duration
- point states scale flags duration delta
- point states scale flags transition duration
- point states scale flags transition duration delta
- point states scale flags width
- point states transition duration
- point states width
- point velocity
- point velocity cone angle
- point velocity max
- point velocity min
- render type
- render type double marker linked
- render type ground mapped
- render type horizontal orientation
- render type media mapped
- render type vertical orientation
- render type viewer facing
- rotation animation center
- rotation animation center x
- rotation animation center y
- rotation animation function
- rotation animation period
- rotation animation phase
- rotation animation scale
- rotation animation source
- scale flags
- scale flags inherited velocity fraction
- scale flags point generation rate
- scale flags point velocity
- scale flags point velocity cone angle
- scale flags point velocity delta
- scale flags sequence animation rate
- scale flags texture animation u
- scale flags texture animation v
- scale flags texture scale u
- scale flags texture scale v
- secondary bitmap
- secondary map flags
- sequence count
- shader flags

## Details

Contrails describe the trail effects which commonly follow projectiles, or emit from model markers like the Banshee's wingtips. They reference a bitmap to be rendered at repeated intervals and can be affected by wind and gravity using point_ physics.

### Limits

Contrails are represented in-engine as series of connected points. There is a limit of 1024 such points existing at any given time, meaning contrails may stop generating if there are already many in the scene. Lower the point generation rate if this becomes an issue.

Setting the _point generation rate_ to 15 per second or higher can cause visual artifacts in the contrail since point generation is framerate dependent and can conflict with the game's tick rate. Modern client mods will modify the effect in-engine to prevent this.

|  | Function/global | Type |
| --- | --- | --- |
|  | `(render_contrails [boolean])` Toggles the display of all contrails. | Global |

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| flags | `bitfield` |  |
Flag values:
- first point unfaded (`0x1`)
- last point unfaded (`0x2`)
- points start pinned to media (`0x4`)
- points start pinned to ground (`0x8`)
- points always pinned to media (`0x10`)
- points always pinned to ground (`0x20`)
- edge effect fades slowly (`0x40`)
| scale flags | `bitfield` |  |
Flag values:
- point generation rate (`0x1`)
- point velocity (`0x2`)
- point velocity delta (`0x4`)
- point velocity cone angle (`0x8`)
- inherited velocity fraction (`0x10`)
- sequence animation rate (`0x20`)
- texture scale u (`0x40`)
- texture scale v (`0x80`)
- texture animation u (`0x100`)
- texture animation v (`0x200`)
| point generation rate | `float` | * Unit: points per second For compatibility with unmodified clients or future mods, try not to exceed 15 points per second because it can cause visual artifacts. |
| point velocity | `Bounds` | * Unit: world units per second |
Field values:
- min (`float`)
- max (`float`)
| point velocity cone angle | `float` |  |
| inherited velocity fraction | `float` |  |
| render type | `enum` |  |
Option values:
- vertical orientation (`0x0`)
- horizontal orientation (`0x1`)
- media mapped (`0x2`)
- ground mapped (`0x3`)
- viewer facing (`0x4`)
- double marker linked (`0x5`)
| texture repeats u | `float` | * Unit: repeats |
| texture repeats v | `float` | * Unit: repeats |
| texture animation u | `float` | * Unit: repeats per second |
| texture animation v | `float` | * Unit: repeats per second |
| animation rate | `float` | * Unit: frames per second |
| bitmap | `TagDependency`: bitmap |  |
| first sequence index | `uint16` |  |
| sequence count | `uint16` |  |
| unknown int | `uint32` | * Cache only |
| shader flags | `bitfield` |  |
Flag values:
- sort bias (`0x1`)
- nonlinear tint (`0x2`)
- don't overdraw fp weapon (`0x4`)
| framebuffer blend function | `enum` |  |
Option values:
- alpha blend (`0x0`)
- multiply (`0x1`)
- double multiply (`0x2`)
- add (`0x3`)
- subtract (`0x4`)
- component min (`0x5`)
- component max (`0x6`)
- alpha multiply add (`0x7`)
| framebuffer fade mode | `enum` |  |
Option values:
- none (`0x0`): The faces do not fade by viewing angle.
- fade when perpendicular (`0x1`): The faces fade out when viewed edge-on.
- fade when parallel (`0x2`): The faces fade out when viewed flat to the camera.
| map flags | `bitfield` |  |
Flag values:
- unfiltered (`0x1`): Texture sampling is unfiltered, resulting in aliasing at a distance and a blocky pixelated look up close.
| secondary bitmap | `TagDependency`: bitmap |  |
| anchor | `enum` |  |
Option values:
- with primary (`0x0`): Positioned relative to each particle.
- with screen space (`0x1`): Positioned relative to the screen.
- zsprite (`0x2`)
| secondary map flags | `bitfield`? |  |
| u animation source | `enum` |  |
Option values:
- none (`0x0`)
- a out (`0x1`)
- b out (`0x2`)
- c out (`0x3`)
- d out (`0x4`)
| u animation function | `enum` |  |
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
| u animation period | `float` | * Unit: seconds |
| u animation phase | `float` |  |
| u animation scale | `float` | * Unit: repeats |
| v animation source | `enum`? |  |
| v animation function | `enum`? |  |
| v animation period | `float` | * Unit: seconds |
| v animation phase | `float` |  |
| v animation scale | `float` | * Unit: repeats |
| rotation animation source | `enum`? |  |
| rotation animation function | `enum`? |  |
| rotation animation period | `float` |  |
| rotation animation phase | `float` |  |
| rotation animation scale | `float` |  |
| rotation animation center | `Point2D` |  |
Field values:
- x (`float`)
- y (`float`)
| zsprite radius scale | `float` |  |
| point states | `Block` | * HEK max count: 16 |
Flag values:
- duration (`Bounds`?): * Unit: seconds
- transition duration (`Bounds`?): * Unit: seconds
- physics (`TagDependency`: point_physics)
- width (`float`): * Unit: world units
- color lower bound (`ColorARGB`)
- color upper bound (`ColorARGB`)
- scale flags (`bitfield`)
- duration (`0x1`)
- duration delta (`0x2`)
- transition duration (`0x4`)
- transition duration delta (`0x8`)
- width (`0x10`)
- color (`0x20`)

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `contrail`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
