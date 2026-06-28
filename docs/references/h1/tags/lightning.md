# Halo 1 Tag: lightning

## Summary

1.   Structure and fields     1.   markers     2.   shader

## Tag Information

Tag group: `lightning`.

Defined fields and enum values mentioned:

- bitmap
- brightness scale source
- count
- far fade distance
- jitter scale source
- jitter scale source a out
- jitter scale source b out
- jitter scale source c out
- jitter scale source d out
- jitter scale source none
- markers
- markers attachment marker
- markers flags
- markers flags not connected to next marker
- markers octaves to next marker
- markers random jitter
- markers random position bounds
- markers thickness
- markers tint
- near fade distance
- shader
- shader framebuffer blend function
- shader framebuffer blend function add
- shader framebuffer blend function alpha blend
- shader framebuffer blend function alpha multiply add
- shader framebuffer blend function component max
- shader framebuffer blend function component min
- shader framebuffer blend function double multiply
- shader framebuffer blend function multiply
- shader framebuffer blend function subtract
- shader framebuffer fade mode
- shader framebuffer fade mode fade when parallel
- shader framebuffer fade mode fade when perpendicular
- shader framebuffer fade mode none
- shader make it work
- shader map flags
- shader map flags unfiltered
- shader shader flags
- shader shader flags dont overdraw fp weapon
- shader shader flags nonlinear tint
- shader shader flags sort bias
- shader some more stuff that should be set for some reason
- thickness scale source
- tint modulation source
- tint modulation source a
- tint modulation source b
- tint modulation source c
- tint modulation source d
- tint modulation source none

## Details

...

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| count | `uint16` |  |
| near fade distance | `float` | * Unit: world units |
| far fade distance | `float` | * Unit: world units |
| jitter scale source | `enum` |  |
Option values:
- none (`0x0`)
- a out (`0x1`)
- b out (`0x2`)
- c out (`0x3`)
- d out (`0x4`)
| thickness scale source | `enum`? |  |
| tint modulation source | `enum` |  |
Option values:
- none (`0x0`)
- a (`0x1`)
- b (`0x2`)
- c (`0x3`)
- d (`0x4`)
| brightness scale source | `enum`? |  |
| bitmap | `TagDependency`: bitmap |  |
| markers | `Block` | * HEK max count: 16 |
Flag values:
- attachment marker (`TagString`)
- flags (`bitfield`)
- not connected to next marker (`0x1`)
- octaves to next marker (`int16`)
- random position bounds (`Vector3D`): * Unit: world units
- random jitter (`float`): * Unit: world units
- thickness (`float`): * Unit: world units
- tint (`ColorARGB`)
| shader | `Block` | * HEK max count: 1 |
Flag values:
- make it work (`uint32`): * Cache only
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
- some more stuff that should be set for some reason (`uint32`): * Cache only

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `lightning`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
