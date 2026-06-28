# Halo 1 Tag: particle

## Summary

The particle tag defines the characteristics of point-like entities used extensively throughout Halo's effects to simulate sparks, smoke, plasma, blood, and more. Particles can interact with wind, collision geometry and the BSP via their point_ physics.

## Tag Information

Tag group: `particle`.

HaloScript entries:

- `debug_sprites [boolean]`
- `profile_display [boolean]`
- `render_particles [boolean]`

Defined fields and enum values mentioned:

- anchor
- anchor with primary
- anchor with screen space
- anchor zsprite
- animation rate
- bitmap
- bitmap1
- collision effect
- contact deterioration
- death effect
- fade end size
- fade in time
- fade out time
- fade start size
- final sequence count
- first sequence index
- flags
- flags animate once per frame
- flags animation starts on random frame
- flags animation stops at rest
- flags can animate backwards
- flags dies at rest
- flags dies on contact with air
- flags dies on contact with structure
- flags dies on contact with water
- flags random horizontal mirroring
- flags random vertical mirroring
- flags self illuminated
- flags tint from diffuse texture
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
- initial sequence count
- lifespan
- lifespan max
- lifespan min
- looping sequence count
- make it actually work
- map flags
- map flags unfiltered
- map flags1
- minimum size
- orientation
- orientation parallel to direction
- orientation perpendicular to direction
- orientation screen facing
- physics
- radius animation
- rotation animation center
- rotation animation center x
- rotation animation center y
- rotation animation function
- rotation animation period
- rotation animation phase
- rotation animation scale
- rotation animation source
- shader flags
- shader flags dont overdraw fp weapon
- shader flags nonlinear tint
- shader flags sort bias
- sir marty exchanged his children for thine
- sprite size
- u animation function
- u animation function cosine
- u animation function cosine variable period
- u animation function diagonal wave
- u animation function diagonal wave variable period
- u animation function jitter
- u animation function noise
- u animation function one

## Details

The particle tag defines the characteristics of point-like entities used extensively throughout Halo's effects to simulate sparks, smoke, plasma, blood, and more. Particles can interact with wind, collision geometry and the BSP via their point_ physics.

|  | Function/global | Type |
| --- | --- | --- |
|  | `(debug_sprites [boolean])` Renders 2D sprite effects like particles and weather_ particle_ system with white triangle outlines. This also displays some sprite statistics at the top of the screen (coverage and big sprites count). | Global |
|  | `(profile_display [boolean])` Displays profiling and budget information in the upper-left of the screen, including object, effects, particles, AI encounters, collision tests, and more. You may find it useful to open the console while using this feature in order to stop the game simulation. | Global |
|  | `(render_particles [boolean])` Toggles the display of all particles. | Global |

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| flags | `bitfield` |  |
Flag values:
- can animate backwards (`0x1`)
- animation stops at rest (`0x2`)
- animation starts on random frame (`0x4`)
- animate once per frame (`0x8`)
- dies at rest (`0x10`)
- dies on contact with structure (`0x20`)
- tint from diffuse texture (`0x40`)
- dies on contact with water (`0x80`)
- dies on contact with air (`0x100`)
- self illuminated (`0x200`)
- random horizontal mirroring (`0x400`)
- random vertical mirroring (`0x800`)
| bitmap | `TagDependency`: bitmap | * Non-null |
| physics | `TagDependency`: point_physics |  |
| sir marty exchanged his children for thine | `TagDependency`: material_effects |  |
| lifespan | `Bounds` | * Unit: seconds |
Field values:
- min (`float`)
- max (`float`)
| fade in time | `float` |  |
| fade out time | `float` |  |
| collision effect | `TagDependency` * sound * effect |  |
| death effect | `TagDependency` * sound * effect |  |
| minimum size | `float` | * Unit: pixels |
| radius animation | `Bounds`? | * Default: 1,1 |
| animation rate | `Bounds`? | * Unit: frames per second |
| contact deterioration | `float` | * Non-cached * Hidden this value is totally broken and locks the game up if it's non-zero; even tool.exe sets it to zero |
| fade start size | `float` | * Unit: pixels * Default: 5 |
| fade end size | `float` | * Unit: pixels * Default: 4 |
| first sequence index | `uint16` |  |
| initial sequence count | `uint16` |  |
| looping sequence count | `uint16` |  |
| final sequence count | `uint16` |  |
| sprite size | `float` | * Cache only |
| orientation | `enum` |  |
Option values:
- screen facing (`0x0`): Particles will face directly toward the viewer, ideal for effects like smoke and snow.
- parallel to direction (`0x1`): Particles will be oriented along the direction of movement, facing the camera but only rotating around the axis of movement. This type is ideal for particles with trails like rain and sparks. In the sprite texture, right is the forward direction.
- perpendicular to direction (`0x2`): Particles will be oriented perpendicular to the direction of movement, so will be seen face-on when the particle is moving toward or away from the camera.
| make it actually work | `uint32` | * Cache only |
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
| bitmap1 | `TagDependency`: bitmap |  |
| anchor | `enum` |  |
Option values:
- with primary (`0x0`): Positioned relative to each particle.
- with screen space (`0x1`): Positioned relative to the screen.
- zsprite (`0x2`)
| map flags1 | `bitfield`? |  |
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
| rotation animation period | `float` | * Unit: seconds |
| rotation animation phase | `float` |  |
| rotation animation scale | `float` | * Unit: degrees |
| rotation animation center | `Point2D` |  |
Field values:
- x (`float`)
- y (`float`)
| zsprite radius scale | `float` |  |

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `particle`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
