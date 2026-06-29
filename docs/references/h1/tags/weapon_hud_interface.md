# Halo 1 Tag: weapon_hud_interface

## Summary

1.   Structure and fields     1.   static elements     2.   meter elements     3.   number elements     4.   crosshairs     5.   overlay elements     6.   screen effect

## Tag Information

Tag group: `weapon_hud_interface`.

Defined fields and enum values mentioned:

- age cutoff
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
- child hud
- crosshair types
- crosshair types aim
- crosshair types charge
- crosshair types flash battery
- crosshair types flash heat
- crosshair types flash secondary reload
- crosshair types flash secondary total ammo
- crosshair types flash total ammo
- crosshair types flash when firing and no ammo
- crosshair types flash when firing secondary trigger with no ammo
- crosshair types flash when firing with depleted battery
- crosshair types flash when throwing and no grenade
- crosshair types low ammo and none left to reload
- crosshair types low secondary ammo and none left to reload
- crosshair types primary trigger ready
- crosshair types reload overheat
- crosshair types secondary trigger ready
- crosshair types should reload
- crosshair types should reload secondary trigger
- crosshair types zoom level
- crosshairs
- crosshairs allowed view type
- crosshairs crosshair bitmap
- crosshairs crosshair overlays
- crosshairs crosshair overlays anchor offset
- crosshairs crosshair overlays default color
- crosshairs crosshair overlays disabled color
- crosshairs crosshair overlays flags
- crosshairs crosshair overlays flags dont show when zoomed
- crosshairs crosshair overlays flags flashes when active
- crosshairs crosshair overlays flags hide area outside reticle
- crosshairs crosshair overlays flags not a sprite
- crosshairs crosshair overlays flags one zoom level
- crosshairs crosshair overlays flags show only when zoomed
- crosshairs crosshair overlays flags show sniper data
- crosshairs crosshair overlays flash delay
- crosshairs crosshair overlays flash flags
- crosshairs crosshair overlays flash length
- crosshairs crosshair overlays flash period
- crosshairs crosshair overlays flashing color
- crosshairs crosshair overlays frame rate
- crosshairs crosshair overlays height scale
- crosshairs crosshair overlays number of flashes
- crosshairs crosshair overlays scaling flags
- crosshairs crosshair overlays sequence index
- crosshairs crosshair overlays width scale
- crosshairs crosshair type
- crosshairs crosshair type aim
- crosshairs crosshair type charge
- crosshairs crosshair type flash battery
- crosshairs crosshair type flash heat
- crosshairs crosshair type flash secondary reload
- crosshairs crosshair type flash secondary total ammo
- crosshairs crosshair type flash total ammo
- crosshairs crosshair type flash when firing and no ammo
- crosshairs crosshair type flash when firing secondary trigger with no ammo
- crosshairs crosshair type flash when firing with depleted battery
- crosshairs crosshair type flash when throwing and no grenade
- crosshairs crosshair type low ammo and none left to reload
- crosshairs crosshair type low secondary ammo and none left to reload
- crosshairs crosshair type primary trigger ready
- crosshairs crosshair type reload overheat
- crosshairs crosshair type secondary trigger ready
- crosshairs crosshair type should reload
- crosshairs crosshair type should reload secondary trigger
- crosshairs crosshair type zoom level
- flags
- flags use parent hud flashing parameters

## Details

1. static elements
 2. meter elements
 3. number elements
 5. overlay elements
 6. screen effect

...

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| child hud | `TagDependency`: weapon_hud_interface |  |
| flags | `bitfield` |  |
Flag values:
- use parent hud flashing parameters (`0x1`)
| total ammo cutoff | `uint16` | When the reserve ammo is below this amount, flash. |
| loaded ammo cutoff | `uint16` | When the ammo in the magazine is below this amount, flash. |
| heat cutoff | `uint16` | When the weapon heat is above this amount, flash. |
| age cutoff | `uint16` | When the weapon has aged this much (eg for plasma weapon batteries), flash. |
| anchor | `enum` |  |
Option values:
- top left (`0x0`)
- top right (`0x1`)
- bottom left (`0x2`)
- bottom right (`0x3`)
- center (`0x4`)
- top center (`0x5`): * H1A only
- bottom center (`0x6`): * H1A only
- left center (`0x7`): * H1A only
- right center (`0x8`): * H1A only
| static elements | `Block` | * HEK max count: 16 |
Option values:
- state attached to (`enum`)
- total ammo (`0x0`)
- loaded ammo (`0x1`)
- heat (`0x2`)
- age (`0x3`)
- secondary weapon total ammo (`0x4`)
- secondary weapon loaded ammo (`0x5`)
- distance to target (`0x6`)
- elevation to target (`0x7`)
- allowed view type (`enum`)
- any (`0x0`)
- solo (`0x1`)
- split screen (`0x2`)
- anchor offset (`Point2DInt`)
- x (`int16`)
- y (`int16`)
- width scale (`float`): * Default: 1
- height scale (`float`): * Default: 1
- scaling flags (`bitfield`)
- don't scale offset (`0x1`)
- don't scale size (`0x2`)
- use high res scale (`0x4`)
- interface bitmap (`TagDependency`: bitmap)
- default color (`ColorARGBInt`)
- RGB Color with alpha, with 8-bit color depth per channel (0-255) (Field): Type
- Comments (): ---
- alpha (`uint8`)
- red (`uint8`)
- green (`uint8`)
- blue (`uint8`)
- flashing color (`ColorARGBInt`?)
- flash period (`float`)
- flash delay (`float`)
- number of flashes (`uint16`)
- flash flags (`bitfield`)
- reverse default flashing colors (`0x1`)
- flash length (`float`)
- disabled color (`ColorARGBInt`?)
- sequence index (`uint16`)
- multitexture overlays (`Block`): * HEK max count: 30
- type (`int16`)
- framebuffer blend function (`enum`)
- alpha blend (`0x0`)
- multiply (`0x1`)
- double multiply (`0x2`)
- add (`0x3`)
- subtract (`0x4`)
- component min (`0x5`)
- component max (`0x6`)
- alpha multiply add (`0x7`)
- primary anchor (`enum`?)
- secondary anchor (`enum`)
- texture (`0x0`)
- screen (`0x1`)
- tertiary anchor (`enum`?)
- zero to one blend function (`enum`)
- add (`0x0`)
- subtract (`0x1`)
- multiply (`0x2`)
- multiply2x (`0x3`)
- dot (`0x4`)
- one to two blend function (`enum`?)
- primary scale (`Point2D`)
- x (`float`)
- y (`float`)
- secondary scale (`Point2D`?)
- tertiary scale (`Point2D`?)
- primary offset (`Point2D`?)
- secondary offset (`Point2D`?)
- tertiary offset (`Point2D`?)
- primary (`TagDependency`: bitmap)
- secondary (`TagDependency`: bitmap)
- tertiary (`TagDependency`: bitmap)
- primary wrap mode (`enum`)
- clamp (`0x0`)
- wrap (`0x1`)
- secondary wrap mode (`enum`?)
- tertiary wrap mode (`enum`?)
- effectors (`Block`): * HEK max count: 30
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
| meter elements | `Block` | * HEK max count: 16 |
Flag values:
- state attached to (`enum`?)
- allowed view type (`enum`?)
- anchor offset (`Point2DInt`?)
- width scale (`float`): * Default: 1
- height scale (`float`): * Default: 1
- scaling flags (`bitfield`?)
- meter bitmap (`TagDependency`: bitmap)
- color at meter minimum (`ColorARGBInt`?)
- color at meter maximum (`ColorARGBInt`?)
- flash color (`ColorARGBInt`?)
- empty color (`ColorARGBInt`?)
- flags (`bitfield`)
- use min max for state changes (`0x1`)
- interpolate between min max flash colors as state changes (`0x2`)
- interpolate color along hsv space (`0x4`)
- more colors for hsv interpolation (`0x8`)
- invert interpolation (`0x10`)
- minimum meter value (`int8`)
- sequence index (`uint16`): For HUD bitmaps which are split into multiple parts or "sequences," specifies which sequence to use. 0-indexed.
- alpha multiplier (`int8`): The difference in alpha mask values from one round in an ammo meter to the next. This value times the number of rounds in the magazine cannot exceed 255.
- alpha bias (`int8`): When nonzero, offsets the starting value of alpha mask in a meter to (255-alpha bias). Can usually be left at zero or 255 minus the alpha mask of the first round.
- value scale (`int16`)
- opacity (`float`)
- translucency (`float`)
- disabled color (`ColorARGBInt`?)
| number elements | `Block` | * HEK max count: 16 |
Flag values:
- state attached to (`enum`?)
- allowed view type (`enum`?)
- anchor offset (`Point2DInt`?)
- width scale (`float`)
- height scale (`float`)
- scaling flags (`bitfield`?)
- default color (`ColorARGBInt`?)
- flashing color (`ColorARGBInt`?)
- flash period (`float`)
- flash delay (`float`)
- number of flashes (`uint16`)
- flash flags (`bitfield`?)
- flash length (`float`)
- disabled color (`ColorARGBInt`?)
- maximum number of digits (`int8`)
- flags (`bitfield`)
- show leading zeros (`0x1`)
- only show when zoomed (`0x2`)
- draw a trailing m (`0x4`)
- number of fractional digits (`int8`)
- weapon specific flags (`bitfield`)
- divide number by clip size (`0x1`)
| crosshairs | `Block` | * HEK max count: 19 |
Flag values:
- crosshair type (`enum`)
- aim (`0x0`)
- zoom level (`0x1`)
- charge (`0x2`)
- should reload (`0x3`)
- flash heat (`0x4`)
- flash total ammo (`0x5`)
- flash battery (`0x6`)
- reload overheat (`0x7`)
- flash when firing and no ammo (`0x8`)
- flash when throwing and no grenade (`0x9`)
- low ammo and none left to reload (`0xA`)
- should reload secondary trigger (`0xB`)
- flash secondary total ammo (`0xC`)
- flash secondary reload (`0xD`)
- flash when firing secondary trigger with no ammo (`0xE`)
- low secondary ammo and none left to reload (`0xF`)
- primary trigger ready (`0x10`)
- secondary trigger ready (`0x11`)
- flash when firing with depleted battery (`0x12`)
- allowed view type (`enum`?)
- crosshair bitmap (`TagDependency`: bitmap)
- crosshair overlays (`Block`): * HEK max count: 16
- anchor offset (`Point2DInt`?)
- width scale (`float`): * Default: 1
- height scale (`float`): * Default: 1
- scaling flags (`bitfield`?)
- default color (`ColorARGBInt`?)
- flashing color (`ColorARGBInt`?)
- flash period (`float`)
- flash delay (`float`)
- number of flashes (`uint16`)
- flash flags (`bitfield`?)
- flash length (`float`)
- disabled color (`ColorARGBInt`?)
- frame rate (`uint16`)
- sequence index (`uint16`)
- flags (`bitfield`)
- flashes when active (`0x1`)
- not a sprite (`0x2`)
- show only when zoomed (`0x4`)
- show sniper data (`0x8`)
- hide area outside reticle (`0x10`)
- one zoom level (`0x20`)
- don't show when zoomed (`0x40`)
| overlay elements | `Block` | * HEK max count: 16 |
Flag values:
- state attached to (`enum`?)
- allowed view type (`enum`?)
- overlay bitmap (`TagDependency`: bitmap)
- overlays (`Block`): * HEK max count: 16
- anchor offset (`Point2DInt`?)
- width scale (`float`): * Default: 1
- height scale (`float`): * Default: 1
- scaling flags (`bitfield`?)
- default color (`ColorARGBInt`?)
- flashing color (`ColorARGBInt`?)
- flash period (`float`)
- flash delay (`float`)
- number of flashes (`uint16`)
- flash flags (`bitfield`?)
- flash length (`float`)
- disabled color (`ColorARGBInt`?)
- frame rate (`uint16`)
- sequence index (`uint16`)
- type (`bitfield`)
- show on flashing (`0x1`)
- show on empty (`0x2`)
- show on reload overheating (`0x4`)
- show on default (`0x8`)
- show always (`0x10`)
- flags (`bitfield`)
- flashes when active (`0x1`)
| crosshair types | `bitfield` | * Cache only |
Flag values:
- aim (`0x1`)
- zoom level (`0x2`)
- charge (`0x4`)
- should reload (`0x8`)
- flash heat (`0x10`)
- flash total ammo (`0x20`)
- flash battery (`0x40`)
- reload overheat (`0x80`)
- flash when firing and no ammo (`0x100`)
- flash when throwing and no grenade (`0x200`)
- low ammo and none left to reload (`0x400`)
- should reload secondary trigger (`0x800`)
- flash secondary total ammo (`0x1000`)
- flash secondary reload (`0x2000`)
- flash when firing secondary trigger with no ammo (`0x4000`)
- low secondary ammo and none left to reload (`0x8000`)
- primary trigger ready (`0x10000`)
- secondary trigger ready (`0x20000`)
- flash when firing with depleted battery (`0x40000`)
| screen effect | `Block` | * HEK max count: 1 |
Flag values:
- mask flags (`bitfield`)
- only when zoomed (`0x1`)
- mask fullscreen (`TagDependency`: bitmap)
- mask splitscreen (`TagDependency`: bitmap)
- convolution flags (`bitfield`?)
- convolution fov in bounds (`Bounds`)
- min (`float`)
- max (`float`)
- convolution radius out bounds (`Bounds`?): * Unit: pixels
- even more flags (`bitfield`)
- only when zoomed (`0x1`)
- connect to flashlight (`0x2`)
- masked (`0x4`)
- night vision script source (`int16`)
- night vision intensity (`float`): * Min: 0 * Max: 1
- desaturation flags (`bitfield`)
- only when zoomed (`0x1`)
- connect to flashlight (`0x2`)
- additive (`0x4`)
- masked (`0x8`)
- desaturation script source (`int16`)
- desaturation intensity (`float`): * Min: 0 * Max: 1
- effect tint (`ColorRGB`)
| sequence index | `uint16` |  |
| width offset | `int16` |  |
| offset from reference corner | `Point2DInt`? |  |
| override icon color | `ColorARGBInt`? |  |
| frame rate | `int8` |  |
| more flags | `bitfield` |  |
Flag values:
- use text from string list instead (`0x1`)
- override default color (`0x2`)
- width offset is absolute icon width (`0x4`)
| text index | `uint16` |  |

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `weapon_hud_interface`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
