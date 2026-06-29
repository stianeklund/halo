# Halo 1 Tag: particle_system

## Summary

A particle system describes the creation and evolution of particles, most commonly for explosions.

## Tag Information

Tag group: `particle_system`.

HaloScript entries:

- `debug_sprites [boolean]`
- `render_psystems [boolean]`

Defined fields and enum values mentioned:

- particle types
- particle types complex sprite render modes
- particle types complex sprite render modes rotational
- particle types complex sprite render modes simple
- particle types flags
- particle types flags animation rate scales with effect
- particle types flags creation rate scales with effect
- particle types flags disabled
- particle types flags dont draw in first person
- particle types flags dont draw in third person
- particle types flags forward backward
- particle types flags forward backward 1
- particle types flags initial count scales with effect
- particle types flags minimum count scales with effect
- particle types flags particle states loop
- particle types flags particles die in air
- particle types flags particles die in water
- particle types flags particles die on ground
- particle types flags rotation rate scales with effect
- particle types flags rotational sprites animate sideways
- particle types flags scale scales with effect
- particle types flags tint by effect color
- particle types flags type states loop
- particle types initial particle count
- particle types name
- particle types particle creation physics
- particle types particle creation physics default
- particle types particle creation physics explosion
- particle types particle creation physics jet
- particle types particle states
- particle types particle states anchor
- particle types particle states anchor with primary
- particle types particle states anchor with screen space
- particle types particle states anchor zsprite
- particle types particle states animation rate
- particle types particle states bitmaps
- particle types particle states color 1
- particle types particle states color 2
- particle types particle states duration bounds
- particle types particle states flags
- particle types particle states framebuffer blend function
- particle types particle states framebuffer blend function add
- particle types particle states framebuffer blend function alpha blend
- particle types particle states framebuffer blend function alpha multiply add
- particle types particle states framebuffer blend function component max
- particle types particle states framebuffer blend function component min
- particle types particle states framebuffer blend function double multiply
- particle types particle states framebuffer blend function multiply
- particle types particle states framebuffer blend function subtract
- particle types particle states framebuffer fade mode
- particle types particle states framebuffer fade mode fade when parallel
- particle types particle states framebuffer fade mode fade when perpendicular
- particle types particle states framebuffer fade mode none
- particle types particle states map flags
- particle types particle states map flags unfiltered
- particle types particle states name
- particle types particle states physics constants
- particle types particle states point physics
- particle types particle states radius multiplier
- particle types particle states rotation animation center
- particle types particle states rotation animation center x
- particle types particle states rotation animation center y
- particle types particle states rotation animation function
- particle types particle states rotation animation period
- particle types particle states rotation animation phase
- particle types particle states rotation animation scale
- particle types particle states rotation animation source
- particle types particle states rotation rate
- particle types particle states rotation rate max
- particle types particle states rotation rate min
- particle types particle states scale
- particle types particle states secondary map bitmap
- particle types particle states sequence index
- particle types particle states shader flags
- particle types particle states shader flags dont overdraw fp weapon
- particle types particle states shader flags nonlinear tint
- particle types particle states shader flags sort bias
- particle types particle states transition time bounds
- particle types particle states u animation function
- particle types particle states u animation function cosine

## Details

A particle system describes the creation and evolution of particles, most commonly for explosions.

This tag was given support for a "jet" particle creation physics type when Gearbox ported the game to PC, in order to support the flamethrower's fire.

|  | Function/global | Type |
| --- | --- | --- |
|  | `(debug_sprites [boolean])` Renders 2D sprite effects like particles and weather_ particle_ system with white triangle outlines. This also displays some sprite statistics at the top of the screen (coverage and big sprites count). | Global |
|  | `(render_psystems [boolean])` Toggles the rendering of particle_ systems and weather_ particle_ system. | Global |

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| point physics | `TagDependency`: point_physics |  |
| system update physics | `enum` |  |
Option values:
- default (`0x0`)
- explosion (`0x1`)
| physics flags | `bitfield` |  |
Flag values:
- unused (`0x1`)
| physics constants | `Block` | * HEK max count: 8 |
Field values:
- k (`float`)
| particle types | `Block` | * HEK max count: 4 |
Field values:
- name (`TagString`)
- flags (`bitfield`)
- type states loop (`0x1`)
- forward backward (`0x2`)
- particle states loop (`0x4`)
- forward backward 1 (`0x8`)
- particles die in water (`0x10`)
- particles die in air (`0x20`)
- particles die on ground (`0x40`)
- rotational sprites animate sideways (`0x80`)
- disabled (`0x100`)
- tint by effect color (`0x200`)
- initial count scales with effect (`0x400`)
- minimum count scales with effect (`0x800`)
- creation rate scales with effect (`0x1000`)
- scale scales with effect (`0x2000`)
- animation rate scales with effect (`0x4000`)
- rotation rate scales with effect (`0x8000`)
- don't draw in first person (`0x10000`)
- don't draw in third person (`0x20000`)
- initial particle count (`uint16`)
- complex sprite render modes (`enum`)
- simple (`0x0`)
- rotational (`0x1`)
- radius (`float`): * Unit: world units
- particle creation physics (`enum`)
- default (`0x0`)
- explosion (`0x1`)
- jet (`0x2`)
- physics flags (`bitfield`?)
- physics constants (`Block`?): * HEK max count: 16
- states (`Block`): * HEK max count: 8
- name (`TagString`)
- duration bounds (`Bounds`): * Unit: second
- min (`float`)
- max (`float`)
- transition time bounds (`Bounds`?): * Unit: second
- scale multiplier (`float`)
- animation rate multiplier (`float`)
- rotation rate multiplier (`float`)
- color multiplier (`ColorARGB`)
- radius multiplier (`float`)
- minimum particle count (`float`)
- particle creation rate (`float`): * Unit: particles per second
- particle creation physics (`enum`?)
- particle update physics (`enum`)
- default (`0x0`)
- physics constants (`Block`?): * HEK max count: 16
- particle states (`Block`): * HEK max count: 8
- * Processed during compile (Field): Type
- Comments (): ---
- name (`TagString`)
- duration bounds (`Bounds`?): * Unit: seconds
- transition time bounds (`Bounds`?): * Unit: seconds
- bitmaps (`TagDependency`: bitmap)
- sequence index (`uint16`)
- scale (`Bounds`?): * Unit: world units per pixel
- animation rate (`Bounds`?): * Unit: frames per second
- rotation rate (`Bounds`): * Unit: degrees per second
- min (`float`)
- max (`float`)
- color 1 (`ColorARGB`)
- color 2 (`ColorARGB`)
- radius multiplier (`float`)
- point physics (`TagDependency`: point_physics)
- unknown int (`uint32`): * Cache only
- shader flags (`bitfield`)
- sort bias (`0x1`)
- nonlinear tint (`0x2`)
- don't overdraw fp weapon (`0x4`)
- framebuffer blend function (`enum`)
- alpha blend (`0x0`)
- multiply (`0x1`)
- double multiply (`0x2`)
- add (`0x3`)
- subtract (`0x4`)
- component min (`0x5`)
- component max (`0x6`)
- alpha multiply add (`0x7`)
- framebuffer fade mode (`enum`)
- none (`0x0`): The faces do not fade by viewing angle.
- fade when perpendicular (`0x1`): The faces fade out when viewed edge-on.
- fade when parallel (`0x2`): The faces fade out when viewed flat to the camera.
- map flags (`bitfield`)
- unfiltered (`0x1`): Texture sampling is unfiltered, resulting in aliasing at a distance and a blocky pixelated look up close.
- secondary map bitmap (`TagDependency`: bitmap)
- anchor (`enum`)
- with primary (`0x0`): Positioned relative to each particle.
- with screen space (`0x1`): Positioned relative to the screen.
- zsprite (`0x2`)
- flags (`bitfield`?)
- u animation source (`enum`)
- none (`0x0`)
- a out (`0x1`)
- b out (`0x2`)
- c out (`0x3`)
- d out (`0x4`)
- u animation function (`enum`)
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
- u animation period (`float`): * Unit: seconds
- u animation phase (`float`)
- u animation scale (`float`): * Unit: repeats
- v animation source (`enum`?)
- v animation function (`enum`?)
- v animation period (`float`): * Unit: seconds
- v animation phase (`float`)
- v animation scale (`float`): * Unit: repeats
- rotation animation source (`enum`?)
- rotation animation function (`enum`?)
- rotation animation period (`float`): * Unit: seconds
- rotation animation phase (`float`)
- rotation animation scale (`float`): * Unit: degrees
- rotation animation center (`Point2D`)
- x (`float`)
- y (`float`)
- zsprite radius scale (`float`)
- physics constants (`Block`?): * HEK max count: 16

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `particle_system`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
