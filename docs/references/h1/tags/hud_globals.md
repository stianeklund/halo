# Halo 1 Tag: hud_globals

## Summary

1.   Structure and fields     1.   button icons     2.   waypoint arrows

## Tag Information

Tag group: `hud_globals`.

Defined fields and enum values mentioned:

- alternate icon text
- anchor
- anchor bottom center
- anchor bottom left
- anchor bottom right
- anchor center
- anchor left center
- anchor offset
- anchor offset x
- anchor offset y
- anchor right center
- anchor top center
- anchor top left
- anchor top right
- arrow bitmap
- bottom offset
- button icons
- button icons flags
- button icons flags override default color
- button icons flags use text from string list instead
- button icons flags width offset is absolute icon width
- button icons frame rate
- button icons offset from reference corner
- button icons override icon color
- button icons override icon color alpha
- button icons override icon color blue
- button icons override icon color green
- button icons override icon color red
- button icons sequence index
- button icons text index
- button icons width offset
- carnage report bitmap
- checkpoint begin text
- checkpoint end text
- checkpoint sound
- default chapter title bounds
- default chapter title bounds bottom
- default chapter title bounds left
- default chapter title bounds right
- default chapter title bounds top
- default weapon hud
- fade time
- height scale
- hud damage bottom offset
- hud damage color
- hud damage indicator bitmap
- hud damage left offset
- hud damage multiplayer sequence index
- hud damage right offset
- hud damage sequence index
- hud damage top offset
- hud help default color
- hud help disabled color
- hud help flash delay
- hud help flash flags
- hud help flash flags reverse default flashing colors
- hud help flash length
- hud help flash period
- hud help flashing color
- hud help number of flashes
- hud messages
- hud scale in multiplayer
- icon bitmap
- icon color
- item message text
- left offset
- loading begin text
- loading end text
- motion sensor range
- motion sensor scale
- motion sensor velocity sensitivity
- multi player font
- not much time left default color
- not much time left disabled color
- not much time left flash delay
- not much time left flash flags
- not much time left flash length
- not much time left flash period
- not much time left flashing color
- not much time left number of flashes

## Details

1. button icons
 2. waypoint arrows

...

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
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
| anchor offset | `Point2DInt` |  |
Field values:
- x (`int16`)
- y (`int16`)
| width scale | `float` |  |
| height scale | `float` |  |
| scaling flags | `bitfield` |  |
Flag values:
- don't scale offset (`0x1`)
- don't scale size (`0x2`)
- use high res scale (`0x4`)
| single player font | `TagDependency`: font |  |
| multi player font | `TagDependency`: font |  |
| up time | `float` |  |
| fade time | `float` |  |
| icon color | `ColorARGB` |  |
| text color | `ColorARGB` |  |
| text spacing | `float` |  |
| item message text | `TagDependency`: unicode_string_list |  |
| icon bitmap | `TagDependency`: bitmap |  |
| alternate icon text | `TagDependency`: unicode_string_list |  |
| button icons | `Block` | * HEK max count: 18 |
Flag values:
- sequence index (`uint16`)
- width offset (`int16`)
- offset from reference corner (`Point2DInt`?)
- override icon color (`ColorARGBInt`)
- RGB Color with alpha, with 8-bit color depth per channel (0-255) (Field): Type
- Comments (): ---
- alpha (`uint8`)
- red (`uint8`)
- green (`uint8`)
- blue (`uint8`)
- frame rate (`int8`)
- flags (`bitfield`)
- use text from string list instead (`0x1`)
- override default color (`0x2`)
- width offset is absolute icon width (`0x4`)
- text index (`uint16`)
| hud help default color | `ColorARGBInt`? |  |
| hud help flashing color | `ColorARGBInt`? |  |
| hud help flash period | `float` |  |
| hud help flash delay | `float` |  |
| hud help number of flashes | `uint16` |  |
| hud help flash flags | `bitfield` |  |
Flag values:
- reverse default flashing colors (`0x1`)
| hud help flash length | `float` |  |
| hud help disabled color | `ColorARGBInt`? |  |
| hud messages | `TagDependency`: hud_message_text |  |
| objective default color | `ColorARGBInt`? |  |
| objective flashing color | `ColorARGBInt`? |  |
| objective flash period | `float` |  |
| objective flash delay | `float` |  |
| objective number of flashes | `uint16` |  |
| objective flash flags | `bitfield`? |  |
| objective flash length | `float` |  |
| objective disabled color | `ColorARGBInt`? |  |
| objective uptime ticks | `uint16` |  |
| objective fade ticks | `uint16` |  |
| top offset | `float` |  |
| bottom offset | `float` |  |
| left offset | `float` |  |
| right offset | `float` |  |
| arrow bitmap | `TagDependency`: bitmap |  |
| waypoint arrows | `Block` | * HEK max count: 16 |
Flag values:
- name (`TagString`)
- color (`ColorARGBInt`?)
- opacity (`float`)
- translucency (`float`)
- on screen sequence index (`uint16`)
- off screen sequence index (`uint16`)
- occluded sequence index (`uint16`)
- flags (`bitfield`)
- dont rotate when pointing offscreen (`0x1`)
| hud scale in multiplayer | `float` |  |
| default weapon hud | `TagDependency`: weapon_hud_interface |  |
| motion sensor range | `float` |  |
| motion sensor velocity sensitivity | `float` |  |
| motion sensor scale | `float` |  |
| default chapter title bounds | `Rectangle2D` |  |
Field values:
- top (`int16`)
- left (`int16`)
- bottom (`int16`)
- right (`int16`)
| hud damage top offset | `int16` |  |
| hud damage bottom offset | `int16` |  |
| hud damage left offset | `int16` |  |
| hud damage right offset | `int16` |  |
| hud damage indicator bitmap | `TagDependency`: bitmap |  |
| hud damage sequence index | `uint16` |  |
| hud damage multiplayer sequence index | `uint16` |  |
| hud damage color | `ColorARGBInt`? |  |
| not much time left default color | `ColorARGBInt`? |  |
| not much time left flashing color | `ColorARGBInt`? |  |
| not much time left flash period | `float` |  |
| not much time left flash delay | `float` |  |
| not much time left number of flashes | `uint16` |  |
| not much time left flash flags | `bitfield`? |  |
| not much time left flash length | `float` |  |
| not much time left disabled color | `ColorARGBInt`? |  |
| time out flash default color | `ColorARGBInt`? |  |
| time out flash flashing color | `ColorARGBInt`? |  |
| time out flash flash period | `float` |  |
| time out flash flash delay | `float` |  |
| time out flash number of flashes | `uint16` |  |
| time out flash flash flags | `bitfield`? |  |
| time out flash flash length | `float` |  |
| time out flash disabled color | `ColorARGBInt`? |  |
| carnage report bitmap | `TagDependency`: bitmap |  |
| loading begin text | `uint16` |  |
| loading end text | `uint16` |  |
| checkpoint begin text | `uint16` |  |
| checkpoint end text | `uint16` |  |
| checkpoint sound | `TagDependency`: sound |  |

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `hud_globals`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
