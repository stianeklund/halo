# Halo 1 Tag: light_volume

## Summary

The **light volume** tag creates dense lines of glow particles, commonly used as a projectile widget for plasma bolts and rocket exhaust. It is also used for the marine's flashlight.

## Tag Information

Tag group: `light_volume`.

Defined fields and enum values mentioned:

- attachment marker
- brightness scale source
- brightness scale source a out
- brightness scale source b out
- brightness scale source c out
- brightness scale source d out
- brightness scale source none
- count
- far fade distance
- flags
- flags interpolate color in hsv
- flags more colors
- frame animation source
- frames
- frames brightness exponent
- frames length
- frames offset exponent
- frames offset from marker
- frames radius exponent
- frames radius hither
- frames radius yon
- frames tint color exponent
- frames tint color hither
- frames tint color yon
- map
- near fade distance
- parallel brightness scale
- perpendicular brightness scale
- sequence index

## Details

The **light volume** tag creates dense lines of glow particles, commonly used as a projectile widget for plasma bolts and rocket exhaust. It is also used for the marine's flashlight.

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| attachment marker | `TagString` |  |
| flags | `bitfield` |  |
Flag values:
- interpolate color in hsv (`0x1`)
- more colors (`0x2`)
| near fade distance | `float` | * Unit: world units |
| far fade distance | `float` | * Unit: world units |
| perpendicular brightness scale | `float` | * Min: 0 * Max: 1 * Default: 1 |
| parallel brightness scale | `float` | * Min: 0 * Max: 1 * Default: 1 |
| brightness scale source | `enum` |  |
Option values:
- none (`0x0`)
- a out (`0x1`)
- b out (`0x2`)
- c out (`0x3`)
- d out (`0x4`)
| map | `TagDependency`: bitmap |  |
| sequence index | `uint16` |  |
| count | `uint16` |  |
| frame animation source | `enum`? |  |
| frames | `Block` | * HEK max count: 2 |
Field values:
- offset from marker (`float`): * Unit: world units
- offset exponent (`float`): * Default: 1
- length (`float`): * Unit: world units
- radius hither (`float`): * Unit: world units
- radius yon (`float`): * Unit: world units
- radius exponent (`float`): * Default: 1
- tint color hither (`ColorARGB`)
- tint color yon (`ColorARGB`)
- tint color exponent (`float`): * Default: 1
- brightness exponent (`float`): * Default: 1

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `light_volume`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
