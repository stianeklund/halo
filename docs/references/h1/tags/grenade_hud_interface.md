# Halo 1 Tag: grenade_hud_interface

## Summary

This tag page is primarily field documentation. See the tag information below for defined fields, enum values, units, and runtime notes.

## Tag Information

Tag group: `grenade_hud_interface`.

Defined fields and enum values mentioned:

- anchor
- anchor bottom center
- anchor bottom left
- anchor bottom right
- anchor center
- anchor left center
- anchor right center
- anchor top center
- anchor top left
- anchor top right
- background anchor offset
- background anchor offset x
- background anchor offset y
- background default color
- background default color alpha
- background default color blue
- background default color green
- background default color red
- background disabled color
- background flash delay
- background flash flags
- background flash flags reverse default flashing colors
- background flash length
- background flash period
- background flashing color
- background height scale
- background interface bitmap
- background multitexture overlays
- background multitexture overlays effectors
- background multitexture overlays effectors destination
- background multitexture overlays effectors destination geometry offset
- background multitexture overlays effectors destination primary map
- background multitexture overlays effectors destination secondary map
- background multitexture overlays effectors destination tertiary map
- background multitexture overlays effectors destination type
- background multitexture overlays effectors destination type fade 0 1
- background multitexture overlays effectors destination type horizontal offset
- background multitexture overlays effectors destination type tint 0 1
- background multitexture overlays effectors destination type vertical offset
- background multitexture overlays effectors function period
- background multitexture overlays effectors function phase
- background multitexture overlays effectors in bounds
- background multitexture overlays effectors in bounds max
- background multitexture overlays effectors in bounds min
- background multitexture overlays effectors out bounds
- background multitexture overlays effectors periodic function
- background multitexture overlays effectors periodic function cosine
- background multitexture overlays effectors periodic function cosine variable period
- background multitexture overlays effectors periodic function diagonal wave
- background multitexture overlays effectors periodic function diagonal wave variable period
- background multitexture overlays effectors periodic function jitter
- background multitexture overlays effectors periodic function noise
- background multitexture overlays effectors periodic function one
- background multitexture overlays effectors periodic function slide
- background multitexture overlays effectors periodic function slide variable period
- background multitexture overlays effectors periodic function spark
- background multitexture overlays effectors periodic function wander
- background multitexture overlays effectors periodic function zero
- background multitexture overlays effectors source
- background multitexture overlays effectors source explicit uses low bound
- background multitexture overlays effectors source player pitch
- background multitexture overlays effectors source player pitch tangent
- background multitexture overlays effectors source player yaw
- background multitexture overlays effectors source weapon ammo loaded
- background multitexture overlays effectors source weapon ammo total
- background multitexture overlays effectors source weapon heat
- background multitexture overlays effectors source weapon zoom level
- background multitexture overlays effectors tint color lower bound
- background multitexture overlays effectors tint color upper bound
- background multitexture overlays framebuffer blend function
- background multitexture overlays framebuffer blend function add
- background multitexture overlays framebuffer blend function alpha blend
- background multitexture overlays framebuffer blend function alpha multiply add
- background multitexture overlays framebuffer blend function component max
- background multitexture overlays framebuffer blend function component min
- background multitexture overlays framebuffer blend function double multiply
- background multitexture overlays framebuffer blend function multiply
- background multitexture overlays framebuffer blend function subtract
- background multitexture overlays one to two blend function
- background multitexture overlays primary

## Details

anchor`enum`
| Option | Value | Comments |
| --- | --- | --- |
| top left | `0x0` |  |
| top right | `0x1` |  |
| bottom left | `0x2` |  |
| bottom right | `0x3` |  |
| center | `0x4` |  |
| top center | `0x5` | * H1A only |
| bottom center | `0x6` | * H1A only |
| left center | `0x7` | * H1A only |
| right center | `0x8` | * H1A only |
background anchor offset`Point2DInt`
| Field | Type | Comments |
| --- | --- | --- |
| x | `int16` |  |
| y | `int16` |  |
background width scale`float`
background height scale`float`
background scaling flags`bitfield`
| Flag | Mask | Comments |
| --- | --- | --- |
| don't scale offset | `0x1` |  |
| don't scale size | `0x2` |  |
| use high res scale | `0x4` |  |
background interface bitmap`TagDependency`: bitmap
background default color`ColorARGBInt`
RGB Color with alpha, with 8-bit color depth per channel (0-255)

| Field | Type | Comments |
| --- | --- | --- |
| alpha | `uint8` |  |
| red | `uint8` |  |
| green | `uint8` |  |
| blue | `uint8` |  |
background flashing color`ColorARGBInt`?
background flash period`float`
background flash delay`float`
background number of flashes`uint16`
background flash flags`bitfield`
| Flag | Mask | Comments |
| --- | --- | --- |
| reverse default flashing colors | `0x1` |  |
background flash length`float`
background disabled color`ColorARGBInt`?
background sequence index`uint16`
background multitexture overlays`Block`
| Field | Type | Comments |
| --- | --- | --- |
| type | `int16` |  |
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
| primary anchor | `enum`? |  |
| secondary anchor | `enum` |  |
Option values:
- texture (`0x0`)
- screen (`0x1`)
| tertiary anchor | `enum`? |  |
| zero to one blend function | `enum` |  |
Option values:
- add (`0x0`)
- subtract (`0x1`)
- multiply (`0x2`)
- multiply2x (`0x3`)
- dot (`0x4`)
| one to two blend function | `enum`? |  |
| primary scale | `Point2D` |  |
Field values:
- x (`float`)
- y (`float`)
| secondary scale | `Point2D`? |  |
| tertiary scale | `Point2D`? |  |
| primary offset | `Point2D`? |  |
| secondary offset | `Point2D`? |  |
| tertiary offset | `Point2D`? |  |
| primary | `TagDependency`: bitmap |  |
| secondary | `TagDependency`: bitmap |  |
| tertiary | `TagDependency`: bitmap |  |
| primary wrap mode | `enum` |  |
Option values:
- clamp (`0x0`)
- wrap (`0x1`)
| secondary wrap mode | `enum`? |  |
| tertiary wrap mode | `enum`? |  |
| effectors | `Block` | * HEK max count: 30 |
Option values:
- destination type (`enum`)
- tint 0 1 (`0x0`)
- horizontal offset (`0x1`)
- vertical offset (`0x2`)
- fade 0 1 (`0x3`)
- destination (`enum`)
- geometry offset (`0x0`)
- primary map (`0x1`)
- secondary map (`0x2`)
- tertiary map (`0x3`)
- source (`enum`)
- player pitch (`0x0`)
- player pitch tangent (`0x1`)
- player yaw (`0x2`)
- weapon ammo total (`0x3`)
- weapon ammo loaded (`0x4`)
- weapon heat (`0x5`)
- explicit uses low bound (`0x6`)
- weapon zoom level (`0x7`)
- in bounds (`Bounds`): * Unit: source units
- min (`float`)
- max (`float`)
- out bounds (`Bounds`?): * Unit: pixels
- tint color lower bound (`ColorRGB`)
- tint color upper bound (`ColorRGB`)
- periodic function (`enum`)
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
- function period (`float`): * Unit: seconds
- function phase (`float`): * Unit: seconds
total grenades background anchor offset`Point2DInt`?
total grenades background width scale`float`
total grenades background height scale`float`
total grenades background scaling flags`bitfield`?
total grenades background interface bitmap`TagDependency`: bitmap
total grenades background default color`ColorARGBInt`?
total grenades background flashing color`ColorARGBInt`?
total grenades background flash period`float`
total grenades background flash delay`float`
total grenades background number of flashes`uint16`
total grenades background flash flags`bitfield`?
total grenades background flash length`float`
total grenades background disabled color`ColorARGBInt`?
total grenades background sequence index`uint16`
total grenades background multitexture overlays`Block`?
total grenades numbers anchor offset`Point2DInt`?
total grenades numbers width scale`float`
total grenades numbers height scale`float`
total grenades numbers scaling flags`bitfield`?
total grenades numbers default color`ColorARGBInt`?
total grenades numbers flashing color`ColorARGBInt`?
total grenades numbers flash period`float`
total grenades numbers flash delay`float`
total grenades numbers number of flashes`uint16`
total grenades numbers flash flags`bitfield`?
total grenades numbers flash length`float`
total grenades numbers disabled color`ColorARGBInt`?
total grenades numbers maximum number of digits`int8`
total grenades numbers flags`bitfield`
| Flag | Mask | Comments |
| --- | --- | --- |
| show leading zeros | `0x1` |  |
| only show when zoomed | `0x2` |  |
| draw a trailing m | `0x4` |  |
total grenades numbers number of fractional digits`int8`
flash cutoff`uint16`
total grenades overlay bitmap`TagDependency`: bitmap
total grenades overlays`Block`
| Field | Type | Comments |
| --- | --- | --- |
| anchor offset | `Point2DInt`? |  |
| width scale | `float` | * Default: 1 |
| height scale | `float` | * Default: 1 |
| scaling flags | `bitfield`? |  |
| default color | `ColorARGBInt`? |  |
| flashing color | `ColorARGBInt`? |  |
| flash period | `float` |  |
| flash delay | `float` |  |
| number of flashes | `uint16` |  |
| flash flags | `bitfield`? |  |
| flash length | `float` |  |
| disabled color | `ColorARGBInt`? |  |
| frame rate | `float` |  |
| sequence index | `uint16` |  |
| type | `bitfield` |  |
Flag values:
- show on flashing (`0x1`)
- show on empty (`0x2`)
- show on default (`0x4`)
- show always (`0x8`)
| flags | `bitfield` |  |
Flag values:
- flashes when active (`0x1`)
total grenades warning sounds`Block`
| Field | Type | Comments |
| --- | --- | --- |
| sound | `TagDependency` * sound * sound_looping |  |
| latched to | `bitfield` |  |
Flag values:
- low grenade count (`0x1`)
- no grenades left (`0x2`)
- throw on no grenades (`0x4`)
| scale | `float` |  |
messaging information sequence index`uint16`
messaging information width offset`int16`
messaging information offset from reference corner`Point2DInt`?
messaging information override icon color`ColorARGBInt`?
messaging information frame rate`int8`
messaging information flags`bitfield`
| Flag | Mask | Comments |
| --- | --- | --- |
| use text from string list instead | `0x1` |  |
| override default color | `0x2` |  |
| width offset is absolute icon width | `0x4` |  |
messaging information text index`uint16`

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `grenade_hud_interface`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
