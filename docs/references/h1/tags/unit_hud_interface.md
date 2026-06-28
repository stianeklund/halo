# Halo 1 Tag: unit_hud_interface

## Summary

This tag page is primarily field documentation. See the tag information below for defined fields, enum values, units, and runtime notes.

## Tag Information

Tag group: `unit_hud_interface`.

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
- auxiliary overlay anchor
- health panel background anchor offset
- health panel background default color
- health panel background disabled color
- health panel background flash delay
- health panel background flash flags
- health panel background flash length
- health panel background flash period
- health panel background flashing color
- health panel background height scale
- health panel background interface bitmap
- health panel background multitex overlay
- health panel background number of flashes
- health panel background scaling flags
- health panel background sequence index
- health panel background width scale
- health panel meter alpha bias
- health panel meter alpha multiplier
- health panel meter anchor offset
- health panel meter color at meter maximum
- health panel meter color at meter minimum
- health panel meter disabled color
- health panel meter empty color
- health panel meter flags
- health panel meter flash color
- health panel meter height scale
- health panel meter max color health fraction cutoff
- health panel meter medium health left color
- health panel meter meter bitmap
- health panel meter min color health fraction cutoff
- health panel meter minimum meter value
- health panel meter opacity
- health panel meter scaling flags
- health panel meter sequence index
- health panel meter translucency
- health panel meter value scale
- health panel meter width scale
- hud background anchor offset
- hud background anchor offset x
- hud background anchor offset y
- hud background default color
- hud background default color alpha
- hud background default color blue
- hud background default color green
- hud background default color red
- hud background disabled color
- hud background flash delay
- hud background flash flags
- hud background flash flags reverse default flashing colors
- hud background flash length
- hud background flash period
- hud background flashing color
- hud background height scale
- hud background interface bitmap
- hud background multitex overlay
- hud background multitex overlay effectors
- hud background multitex overlay effectors destination
- hud background multitex overlay effectors destination geometry offset
- hud background multitex overlay effectors destination primary map
- hud background multitex overlay effectors destination secondary map
- hud background multitex overlay effectors destination tertiary map
- hud background multitex overlay effectors destination type
- hud background multitex overlay effectors destination type fade 0 1
- hud background multitex overlay effectors destination type horizontal offset
- hud background multitex overlay effectors destination type tint 0 1
- hud background multitex overlay effectors destination type vertical offset
- hud background multitex overlay effectors function period
- hud background multitex overlay effectors function phase
- hud background multitex overlay effectors in bounds
- hud background multitex overlay effectors in bounds max

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
hud background anchor offset`Point2DInt`
| Field | Type | Comments |
| --- | --- | --- |
| x | `int16` |  |
| y | `int16` |  |
hud background width scale`float`
hud background height scale`float`
hud background scaling flags`bitfield`
| Flag | Mask | Comments |
| --- | --- | --- |
| don't scale offset | `0x1` |  |
| don't scale size | `0x2` |  |
| use high res scale | `0x4` |  |
hud background interface bitmap`TagDependency`: bitmap
hud background default color`ColorARGBInt`
RGB Color with alpha, with 8-bit color depth per channel (0-255)

| Field | Type | Comments |
| --- | --- | --- |
| alpha | `uint8` |  |
| red | `uint8` |  |
| green | `uint8` |  |
| blue | `uint8` |  |
hud background flashing color`ColorARGBInt`?
hud background flash period`float`
hud background flash delay`float`
hud background number of flashes`uint16`
hud background flash flags`bitfield`
| Flag | Mask | Comments |
| --- | --- | --- |
| reverse default flashing colors | `0x1` |  |
hud background flash length`float`
hud background disabled color`ColorARGBInt`?
hud background sequence index`uint16`
hud background multitex overlay`Block`
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
shield panel background anchor offset`Point2DInt`?
shield panel background width scale`float`
shield panel background height scale`float`
shield panel background scaling flags`bitfield`?
shield panel background interface bitmap`TagDependency`: bitmap
shield panel background default color`ColorARGBInt`?
shield panel background flashing color`ColorARGBInt`?
shield panel background flash period`float`
shield panel background flash delay`float`
shield panel background number of flashes`uint16`
shield panel background flash flags`bitfield`?
shield panel background flash length`float`
shield panel background disabled color`ColorARGBInt`?
shield panel background sequence index`uint16`
shield panel background multitex overlay`Block`?
shield panel meter anchor offset`Point2DInt`?
shield panel meter width scale`float`
shield panel meter height scale`float`
shield panel meter scaling flags`bitfield`?
shield panel meter meter bitmap`TagDependency`: bitmap
shield panel meter color at meter minimum`ColorARGBInt`?
shield panel meter color at meter maximum`ColorARGBInt`?
shield panel meter flash color`ColorARGBInt`?
shield panel meter empty color`ColorARGBInt`?
shield panel meter flags`bitfield`
| Flag | Mask | Comments |
| --- | --- | --- |
| use min max for state changes | `0x1` |  |
| interpolate between min max flash colors as state changes | `0x2` |  |
| interpolate color along hsv space | `0x4` |  |
| more colors for hsv interpolation | `0x8` |  |
| invert interpolation | `0x10` |  |
shield panel meter minimum meter value`int8`
shield panel meter sequence index`uint16`
shield panel meter alpha multiplier`int8`
shield panel meter alpha bias`int8`
shield panel meter value scale`int16`
shield panel meter opacity`float`
shield panel meter translucency`float`
shield panel meter disabled color`ColorARGBInt`?
shield panel meter overcharge minimum color`ColorARGBInt`?
shield panel meter overcharge maximum color`ColorARGBInt`?
shield panel meter overcharge flash color`ColorARGBInt`?
shield panel meter overcharge empty color`ColorARGBInt`?
health panel background anchor offset`Point2DInt`?
health panel background width scale`float`
health panel background height scale`float`
health panel background scaling flags`bitfield`?
health panel background interface bitmap`TagDependency`: bitmap
health panel background default color`ColorARGBInt`?
health panel background flashing color`ColorARGBInt`?
health panel background flash period`float`
health panel background flash delay`float`
health panel background number of flashes`uint16`
health panel background flash flags`bitfield`?
health panel background flash length`float`
health panel background disabled color`ColorARGBInt`?
health panel background sequence index`uint16`
health panel background multitex overlay`Block`?
health panel meter anchor offset`Point2DInt`?
health panel meter width scale`float`
health panel meter height scale`float`
health panel meter scaling flags`bitfield`?
health panel meter meter bitmap`TagDependency`: bitmap
health panel meter color at meter minimum`ColorARGBInt`?
health panel meter color at meter maximum`ColorARGBInt`?
health panel meter flash color`ColorARGBInt`?
health panel meter empty color`ColorARGBInt`?
health panel meter flags`bitfield`?
health panel meter minimum meter value`int8`
health panel meter sequence index`uint16`
health panel meter alpha multiplier`int8`
health panel meter alpha bias`int8`
health panel meter value scale`int16`
health panel meter opacity`float`
health panel meter translucency`float`
health panel meter disabled color`ColorARGBInt`?
health panel meter medium health left color`ColorARGBInt`?
health panel meter max color health fraction cutoff`float`
health panel meter min color health fraction cutoff`float`
motion sensor background anchor offset`Point2DInt`?
motion sensor background width scale`float`
motion sensor background height scale`float`
motion sensor background scaling flags`bitfield`?
motion sensor background interface bitmap`TagDependency`: bitmap
motion sensor background default color`ColorARGBInt`?
motion sensor background flashing color`ColorARGBInt`?
motion sensor background flash period`float`
motion sensor background flash delay`float`
motion sensor background number of flashes`uint16`
motion sensor background flash flags`bitfield`?
motion sensor background flash length`float`
motion sensor background disabled color`ColorARGBInt`?
motion sensor background sequence index`uint16`
motion sensor background multitex overlays`Block`?
motion sensor foreground anchor offset`Point2DInt`?
motion sensor foreground width scale`float`
motion sensor foreground height scale`float`
motion sensor foreground scaling flags`bitfield`?
motion sensor foreground interface bitmap`TagDependency`: bitmap
motion sensor foreground default color`ColorARGBInt`?
motion sensor foreground flashing color`ColorARGBInt`?
motion sensor foreground flash period`float`
motion sensor foreground flash delay`float`
motion sensor foreground number of flashes`uint16`
motion sensor foreground flash flags`bitfield`?
motion sensor foreground flash length`float`
motion sensor foreground disabled color`ColorARGBInt`?
motion sensor foreground sequence index`uint16`
motion sensor foreground multitex overlays`Block`?
motion sensor center anchor offset`Point2DInt`?
motion sensor center width scale`float`
motion sensor center height scale`float`
motion sensor center scaling flags`bitfield`?
auxiliary overlay anchor`enum`?
overlays`Block`
| Field | Type | Comments |
| --- | --- | --- |
| anchor offset | `Point2DInt`? |  |
| width scale | `float` | * Default: 1 |
| height scale | `float` | * Default: 1 |
| scaling flags | `bitfield`? |  |
| interface bitmap | `TagDependency`: bitmap |  |
| default color | `ColorARGBInt`? |  |
| flashing color | `ColorARGBInt`? |  |
| flash period | `float` |  |
| flash delay | `float` |  |
| number of flashes | `uint16` |  |
| flash flags | `bitfield`? |  |
| flash length | `float` |  |
| disabled color | `ColorARGBInt`? |  |
| sequence index | `uint16` |  |
| multitex overlay | `Block`? |  |
| type | `enum` |  |
Option values:
- integrated light (`0x0`)
| flags | `bitfield` |  |
Flag values:
- use team color (`0x1`)
sounds`Block`
| Field | Type | Comments |
| --- | --- | --- |
| sound | `TagDependency` * sound * sound_looping |  |
| latched to | `bitfield` |  |
Flag values:
- shield recharging (`0x1`)
- shield damaged (`0x2`)
- shield low (`0x4`)
- shield empty (`0x8`)
- health low (`0x10`)
- health empty (`0x20`)
- health minor damage (`0x40`)
- health major damage (`0x80`)
| scale | `float` |  |
meters`Block`
| Field | Type | Comments |
| --- | --- | --- |
| type | `enum`? |  |
| background anchor offset | `Point2DInt`? |  |
| background width scale | `float` | * Default: 1 |
| background height scale | `float` | * Default: 1 |
| background scaling flags | `bitfield`? |  |
| background interface bitmap | `TagDependency`: bitmap |  |
| background default color | `ColorARGBInt`? |  |
| background flashing color | `ColorARGBInt`? |  |
| background flash period | `float` |  |
| background flash delay | `float` |  |
| background number of flashes | `uint16` |  |
| background flash flags | `bitfield`? |  |
| background flash length | `float` |  |
| background disabled color | `ColorARGBInt`? |  |
| background sequence index | `uint16` |  |
| background multitex overlay | `Block`? |  |
| meter anchor offset | `Point2DInt`? |  |
| meter width scale | `float` | * Default: 1 |
| meter height scale | `float` | * Default: 1 |
| meter scaling flags | `bitfield`? |  |
| meter meter bitmap | `TagDependency`: bitmap |  |
| meter color at meter minimum | `ColorARGBInt`? |  |
| meter color at meter maximum | `ColorARGBInt`? |  |
| meter flash color | `ColorARGBInt`? |  |
| meter empty color | `ColorARGBInt`? |  |
| meter flags | `bitfield`? |  |
| meter minimum meter value | `int8` |  |
| meter sequence index | `uint16` |  |
| meter alpha multiplier | `int8` |  |
| meter alpha bias | `int8` |  |
| meter value scale | `int16` |  |
| meter opacity | `float` |  |
| meter translucency | `float` |  |
| meter disabled color | `ColorARGBInt`? |  |
| meter minimum fraction cutoff | `float` |  |
| meter more flags | `bitfield` |  |
Flag values:
- show only when active (`0x1`)
- flash once if activated while disabled (`0x2`)

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `unit_hud_interface`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
